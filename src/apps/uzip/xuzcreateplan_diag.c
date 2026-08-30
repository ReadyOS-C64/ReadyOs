/* Physical recursive source-plan probe. It reads only the exact owner-marked
 * fixture tree, stores its breadth-first plan in one owned REU bank, and never
 * opens an output file. */

#include "xuzcreateplan_diag.h"

#include "xuzcreateplan_config.h"
#include "uz_browser.h"
#include "uz_catalog.h"
#include "uz_create_plan.h"
#include "uz_dos.h"

#include "../../lib/reu_control_bank.h"
#include "../../lib/reu_mgr.h"
#include "../../lib/reu_owned_alloc.h"
#include "../../lib/tui.h"

#include <string.h>

#define XUZC_RESULT ((volatile unsigned char *)0x033Cu)
#define XUZC_NONE 0xFFu
#define XUZC_CATALOG_SLOT 3u
#define XUZC_COMMAND_CAP 300u
#define XUZC_DATA_CAP 160u
#define XUZC_STATUS_CAP 256u

#define XUZC_STAGE_READY  1u
#define XUZC_STAGE_BANKS  2u
#define XUZC_STAGE_OWNER  3u
#define XUZC_STAGE_PLAN   4u
#define XUZC_STAGE_VERIFY 5u
#define XUZC_STAGE_DONE   6u

#pragma code-name(push, "UI_CODE")
#pragma rodata-name(push, "UI_RODATA")
#pragma bss-name(push, "UI_BSS")

static UzDos dos;
static unsigned char command[XUZC_COMMAND_CAP];
static unsigned char data[XUZC_DATA_CAP];
static unsigned char status[XUZC_STATUS_CAP];
static UzCatalog catalog;
static UzCreatePlan plan;
static UzBrowserPage page;
static UzZipRecord record;
static UzZipRecord scratch;
static char absolute_path[UZ_BROWSER_PATH_CAP];
static unsigned char active_catalog_bank;
static unsigned int list_calls;
static unsigned char max_page;

static unsigned char screen_code(unsigned char value) {
    if (value >= 0x41u && value <= 0x5Au)
        return (unsigned char)(value - 0x40u);
    if (value >= 0xC1u && value <= 0xDAu)
        return (unsigned char)(value - 0xC0u);
    return value;
}

static void screen_text(unsigned char row, const char *text) {
    volatile unsigned char *screen;
    unsigned char column;

    screen = (volatile unsigned char *)(0x0400u + (unsigned int)row * 40u);
    for (column = 0u; column < 40u; ++column)
        screen[column] = *text != 0 ?
            screen_code((unsigned char)*text++) : 0x20u;
}

static void result_u16(unsigned char offset, unsigned int value) {
    XUZC_RESULT[offset] = (unsigned char)value;
    XUZC_RESULT[(unsigned char)(offset + 1u)] =
        (unsigned char)(value >> 8u);
}

/* Keep the live result useful when a physical run is deliberately sampled
 * before completion.  The runner may reboot during cleanup, so every major
 * boundary is committed before starting the next bounded operation. */
static void checkpoint(unsigned char stage, unsigned char detail) {
    XUZC_RESULT[6] = stage;
    XUZC_RESULT[30] = detail;
}

static unsigned char bank_type(unsigned char bank) {
    if (bank == XUZC_NONE) return REU_UNAVAIL;
    return readyos_bank_read_byte((unsigned int)(REUCB_BANK_TYPE_OFF + bank));
}

static unsigned char catalog_write(void *context, unsigned int offset,
                                   const void *source,
                                   unsigned int length) {
    (void)context;
    if (active_catalog_bank == XUZC_NONE ||
        offset > 0xFFFFu - length) return 0u;
    reu_dma_stash((unsigned int)source, active_catalog_bank, offset, length);
    return 1u;
}

static unsigned char catalog_read(void *context, unsigned int offset,
                                  void *destination,
                                  unsigned int length) {
    (void)context;
    if (active_catalog_bank == XUZC_NONE ||
        offset > 0xFFFFu - length) return 0u;
    reu_dma_fetch((unsigned int)destination, active_catalog_bank,
                  offset, length);
    return 1u;
}

static unsigned char list_page(void *context, const char *path,
                               unsigned char page_number,
                               UzBrowserPage *result) {
    unsigned char ok;

    (void)context;
    ++list_calls;
    result_u16(20u, list_calls);
    if (page_number > max_page) max_page = page_number;
    XUZC_RESULT[22] = max_page;
    checkpoint(XUZC_STAGE_PLAN,
               (unsigned char)(0x80u | (unsigned char)list_calls));
    /* uz_browser_list delegates each complete READ_DIR transaction to the
     * shared asynchronous UCI state machine. It owns idle synchronization,
     * one PUSH, full queue draining, DATA_ACC, recovery, and quiet idle. */
    ok = uz_browser_list(&dos, path, page_number, UZ_BROWSER_SHOW_ALL,
                         result);
    XUZC_RESULT[31] = ok;
    checkpoint(XUZC_STAGE_PLAN,
               (unsigned char)(0x90u | (unsigned char)list_calls));
    return ok;
}

static unsigned char owner_valid(void) {
    unsigned int index;
    int got;

    if (!uz_dos_change_absolute(&dos, XUZCREATEPLAN_OWNED_ROOT) ||
        !uz_dos_open(&dos, ".readyos-uzip-owner", UZ_DOS_OPEN_READ))
        return 0u;
    got = uz_dos_read(&dos, data, XUZCREATEPLAN_OWNER_LENGTH);
    if (!uz_dos_close(&dos) || got < 0 ||
        (unsigned int)got != XUZCREATEPLAN_OWNER_LENGTH) return 0u;
    for (index = 0u; index < XUZCREATEPLAN_OWNER_LENGTH; ++index)
        if (data[index] !=
            (unsigned char)XUZCREATEPLAN_OWNER_TEXT[index]) return 0u;
    return 1u;
}

static const char *expected_name(unsigned char index) {
    switch (index) {
        case 0u: return XUZCREATEPLAN_NAME_0;
        case 1u: return XUZCREATEPLAN_NAME_1;
        case 2u: return XUZCREATEPLAN_NAME_2;
        case 3u: return XUZCREATEPLAN_NAME_3;
        case 4u: return XUZCREATEPLAN_NAME_4;
        case 5u: return XUZCREATEPLAN_NAME_5;
        case 6u: return XUZCREATEPLAN_NAME_6;
        case 7u: return XUZCREATEPLAN_NAME_7;
        case 8u: return XUZCREATEPLAN_NAME_8;
        case 9u: return XUZCREATEPLAN_NAME_9;
        case 10u: return XUZCREATEPLAN_NAME_10;
        case 11u: return XUZCREATEPLAN_NAME_11;
        case 12u: return XUZCREATEPLAN_NAME_12;
        case 13u: return XUZCREATEPLAN_NAME_13;
        case 14u: return XUZCREATEPLAN_NAME_14;
        case 15u: return XUZCREATEPLAN_NAME_15;
        case 16u: return XUZCREATEPLAN_NAME_16;
        case 17u: return XUZCREATEPLAN_NAME_17;
        case 18u: return XUZCREATEPLAN_NAME_18;
        case 19u: return XUZCREATEPLAN_NAME_19;
        case 20u: return XUZCREATEPLAN_NAME_20;
        case 21u: return XUZCREATEPLAN_NAME_21;
        default: return XUZCREATEPLAN_NAME_22;
    }
}

static unsigned char expected_directory(unsigned char index) {
    switch (index) {
        case 0u: return XUZCREATEPLAN_DIRECTORY_0;
        case 1u: return XUZCREATEPLAN_DIRECTORY_1;
        case 2u: return XUZCREATEPLAN_DIRECTORY_2;
        case 3u: return XUZCREATEPLAN_DIRECTORY_3;
        case 4u: return XUZCREATEPLAN_DIRECTORY_4;
        case 5u: return XUZCREATEPLAN_DIRECTORY_5;
        case 6u: return XUZCREATEPLAN_DIRECTORY_6;
        case 7u: return XUZCREATEPLAN_DIRECTORY_7;
        case 8u: return XUZCREATEPLAN_DIRECTORY_8;
        case 9u: return XUZCREATEPLAN_DIRECTORY_9;
        case 10u: return XUZCREATEPLAN_DIRECTORY_10;
        case 11u: return XUZCREATEPLAN_DIRECTORY_11;
        case 12u: return XUZCREATEPLAN_DIRECTORY_12;
        case 13u: return XUZCREATEPLAN_DIRECTORY_13;
        case 14u: return XUZCREATEPLAN_DIRECTORY_14;
        case 15u: return XUZCREATEPLAN_DIRECTORY_15;
        case 16u: return XUZCREATEPLAN_DIRECTORY_16;
        case 17u: return XUZCREATEPLAN_DIRECTORY_17;
        case 18u: return XUZCREATEPLAN_DIRECTORY_18;
        case 19u: return XUZCREATEPLAN_DIRECTORY_19;
        case 20u: return XUZCREATEPLAN_DIRECTORY_20;
        case 21u: return XUZCREATEPLAN_DIRECTORY_21;
        default: return XUZCREATEPLAN_DIRECTORY_22;
    }
}

static unsigned char verify_catalog(void) {
    unsigned char expected;
    unsigned int index;
    unsigned char found;
    unsigned char directory;

    for (expected = 0u; expected < XUZCREATEPLAN_ENTRY_COUNT; ++expected) {
        found = 0u;
        directory = expected_directory(expected);
        for (index = 0u; index < catalog.count; ++index) {
            if (!uz_catalog_get(&catalog, index, &record)) return 0u;
            if (strcmp(record.name, expected_name(expected)) == 0 &&
                record.directory == directory &&
                record.method == (directory ? 0u : 8u)) {
                found = 1u;
                break;
            }
        }
        if (!found) return 0u;
    }
    return 1u;
}

static void finish_screen(unsigned char passed) {
    tui_clear(TUI_COLOR_BLUE);
    screen_text(0u, "XUZCREATEPLAN C64U RECURSIVE");
    screen_text(2u, passed ? "XUZCREATEPLAN FINISHED PASS" :
                            "XUZCREATEPLAN FINISHED FAIL");
    tui_puts(0u, 5u, "READ-ONLY REU PLAN", passed ?
             TUI_COLOR_LIGHTGREEN : TUI_COLOR_LIGHTRED);
    tui_puts(0u, 22u, "RUN/STOP: LAUNCHER", TUI_COLOR_CYAN);
}

void xuzcreateplan_diag_run(unsigned char package_bank) {
    unsigned char catalog_bank;
    unsigned char failure;
    unsigned char stage;
    unsigned char key;

    memset((void *)XUZC_RESULT, 0, 96u);
    XUZC_RESULT[0] = 0x58u; /* XZC1 */
    XUZC_RESULT[1] = 0x5Au;
    XUZC_RESULT[2] = 0x43u;
    XUZC_RESULT[3] = 0x31u;
    XUZC_RESULT[4] = 1u;
    XUZC_RESULT[8] = package_bank;
    XUZC_RESULT[9] = XUZC_NONE;
    XUZC_RESULT[27] = 8u;
    XUZC_RESULT[32] = XUZCREATEPLAN_COOKIE_0;
    XUZC_RESULT[33] = XUZCREATEPLAN_COOKIE_1;
    XUZC_RESULT[34] = XUZCREATEPLAN_COOKIE_2;
    XUZC_RESULT[35] = XUZCREATEPLAN_COOKIE_3;
    result_u16(46u, sizeof(UzZipRecord));
    result_u16(48u, sizeof(UzBrowserPage));
    stage = XUZC_STAGE_READY;
    checkpoint(stage, 0u);
    tui_clear(TUI_COLOR_BLUE);
    screen_text(0u, "XUZCREATEPLAN C64U RECURSIVE");
    screen_text(2u, "PRESS SPACE TO START");
    while (tui_getkey() != ' ') { }

    checkpoint(XUZC_STAGE_BANKS, 1u);
    active_catalog_bank = XUZC_NONE;
    catalog_bank = reu_alloc_owned_bank(XUZC_CATALOG_SLOT, "uzcp");
    active_catalog_bank = catalog_bank;
    XUZC_RESULT[9] = catalog_bank;
    stage = XUZC_STAGE_BANKS;
    checkpoint(stage, 2u);
    failure = 0u;
    if (package_bank == XUZC_NONE || catalog_bank == XUZC_NONE ||
        package_bank == catalog_bank ||
        bank_type(package_bank) != REU_APP_ALLOC ||
        bank_type(catalog_bank) != REU_APP_ALLOC) {
        failure = 0x21u;
        goto finished;
    }
    XUZC_RESULT[26] = bank_type(catalog_bank);

    uz_dos_init(&dos, UZ_DOS_TARGET_READ, command, sizeof(command),
                data, sizeof(data), status, sizeof(status));
    result_u16(24u, uz_uci_base());
    stage = XUZC_STAGE_OWNER;
    checkpoint(stage, 1u);
    /* All following DOS calls use the shared async UCI gateway, which owns
     * synchronization, PUSH/ABORT, complete queue draining, DATA_ACC, and the
     * final quiet-idle wait. This probe adds no timing assumptions. */
    if (!uz_dos_identify(&dos)) {
        failure = 0x31u;
        goto finished;
    }
    checkpoint(stage, 2u);
    if (!owner_valid()) {
        failure = 0x32u;
        goto finished;
    }
    checkpoint(stage, 3u);
    XUZC_RESULT[29] = 1u;

    stage = XUZC_STAGE_PLAN;
    checkpoint(stage, 1u);
    list_calls = 0u;
    max_page = 0u;
    uz_catalog_init(&catalog, catalog_write, catalog_read, 0);
    uz_create_plan_init(&plan, &catalog, list_page, 0,
                        XUZCREATEPLAN_SOURCE_BASE, absolute_path,
                        sizeof(absolute_path), &page, &record, &scratch, 8u);
    checkpoint(stage, 2u);
    if (plan.error != UZ_CREATE_PLAN_OK) {
        XUZC_RESULT[11] = plan.error;
        failure = 0x41u;
        goto finished;
    }
    if (!uz_create_plan_seed(&plan, "loose.prg", 0u)) {
        XUZC_RESULT[11] = plan.error;
        failure = 0x42u;
        goto finished;
    }
    checkpoint(stage, 3u);
    if (!uz_create_plan_seed(&plan, "top", 1u)) {
        XUZC_RESULT[11] = plan.error;
        failure = 0x43u;
        goto finished;
    }
    checkpoint(stage, 4u);
    if (!uz_create_plan_build(&plan, XUZCREATEPLAN_OUTPUT_PATH)) {
        XUZC_RESULT[11] = plan.error;
        failure = 0x44u;
        goto finished;
    }
    checkpoint(stage, 5u);
    XUZC_RESULT[11] = plan.error;
    result_u16(12u, catalog.count);
    result_u16(14u, plan.seed_count);
    result_u16(16u, plan.files);
    result_u16(18u, plan.directories);
    result_u16(20u, list_calls);
    XUZC_RESULT[22] = max_page;

    stage = XUZC_STAGE_VERIFY;
    checkpoint(stage, 1u);
    if (catalog.count != XUZCREATEPLAN_ENTRY_COUNT ||
        plan.files != XUZCREATEPLAN_FILE_COUNT ||
        plan.directories != XUZCREATEPLAN_DIRECTORY_COUNT ||
        plan.seed_count != 2u || list_calls != XUZCREATEPLAN_LIST_CALLS ||
        max_page != 1u || !verify_catalog()) {
        failure = 0x51u;
        goto finished;
    }
    XUZC_RESULT[28] = 1u;
    checkpoint(stage, 2u);

finished:
    XUZC_RESULT[44] = dos.transfer.flags;
    XUZC_RESULT[45] = dos.transfer.last_status;
    if (catalog_bank != XUZC_NONE) {
        reu_free_owned_bank(catalog_bank);
        active_catalog_bank = XUZC_NONE;
        XUZC_RESULT[10] = 1u;
    }
    stage = failure == 0u ? XUZC_STAGE_DONE : stage;
    XUZC_RESULT[5] = 1u;
    checkpoint(stage, failure == 0u ? 0u : XUZC_RESULT[30]);
    XUZC_RESULT[7] = failure;
    finish_screen((unsigned char)(failure == 0u));
    for (;;) {
        key = tui_getkey();
        if (key == TUI_KEY_RUNSTOP || key == 2u) tui_return_to_launcher();
    }
}

#pragma bss-name(pop)
#pragma rodata-name(pop)
#pragma code-name(pop)
