/*
 * uzip.c - ReadyOS Ultimate DOS ZIP utility
 *
 * This first integrated scaffold establishes the final product identity,
 * ReadyOS-owned REU allocation, custom memory contract, and idle navigation.
 * ZIP mutations remain unavailable until their shared standalone C64 probes
 * have passed on physical Ultimate hardware.
 */

#include "../../lib/reu_owned_alloc.h"
#include "../../lib/reu_mgr.h"
#include "../../lib/reu_control_bank.h"
#include "../../lib/tui.h"
#include "uz_pack.h"
#include "uz_package.h"
#ifndef UZIP_PHYSICAL_DIAGNOSTIC
#include "uz_browser.h"
#include "uz_catalog.h"
#include "uz_create_package.h"
#include "uz_create_plan_overlay.h"
#include "uz_workflow.h"
#include "uz_zip_write.h"
#endif
#ifdef UZIP_XUZREU_DIAGNOSTIC
#include "xuzreu_diag.h"
#endif
#ifdef UZIP_XUZDEFLATE_DIAGNOSTIC
#include "xuzdeflate_diag.h"
#endif
#ifdef UZIP_XUZZIP8_DIAGNOSTIC
#include "xuzzip8_diag.h"
#endif
#ifdef UZIP_XUZMULTI_DIAGNOSTIC
#include "xuzmulti_diag.h"
#endif
#ifdef UZIP_XUZREAD_DIAGNOSTIC
#include "xuzread_diag.h"
#endif
#ifdef UZIP_XUZEXTRACT_DIAGNOSTIC
#include "xuzextract_diag.h"
#endif
#ifdef UZIP_XUZCREATEPLAN_DIAGNOSTIC
#include "xuzcreateplan_diag.h"
#endif

#include <string.h>

#define SHIM_CURRENT_BANK (*(volatile unsigned char *)0xC834)
#define UZIP_BANK_NONE 0xFFu
#define UZIP_PACKAGE_SLOT 1u
#define UZIP_WORK_SLOT 2u
#define UZIP_CATALOG_SLOT 3u
#define UZIP_BROWSER_FILE 0u
#define UZIP_BROWSER_FOLDER 1u
#define UZIP_BROWSER_SOURCE 2u
#define UZIP_CPU_PORT (*(volatile unsigned char *)0x0001u)
#define UZIP_LORAM 0x01u

static unsigned char package_bank = UZIP_BANK_NONE;
#ifndef UZIP_PHYSICAL_DIAGNOSTIC
static unsigned char running = 1u;
#endif

#pragma code-name(push, "UI_CODE")
#pragma rodata-name(push, "UI_RODATA")
#pragma bss-name(push, "UI_BSS")

static char status_text[40];
#ifdef UZIP_SELF_SEED_PACKAGE
static unsigned char seed_header[UZ_PACKAGE_HEADER_SIZE];
#endif

#ifndef UZIP_PHYSICAL_DIAGNOSTIC
#ifdef UZIP_COLD_UI
/* The ReadyOS shim restores the full app snapshot and then jumps to $1000.
 * This marker lets the resident cold bridge enter the already-restored UI
 * instead of inflating and initializing it again. */
unsigned char uzip_ui_resume_marker[4];
#endif
static UzDos ui_dos;
static unsigned char ui_command[300];
static unsigned char ui_data[160];
static unsigned char ui_status[256];
static UzBrowserPage browser_page;
static char browser_path[UZ_BROWSER_PATH_CAP];
static char source_path[UZ_BROWSER_PATH_CAP];
static char target_path[UZ_BROWSER_PATH_CAP];
static char path_scratch[UZ_BROWSER_PATH_CAP];
static char display_text[40];
static char archive_name[32];
static unsigned char status_error;
static unsigned char status_detail;
static unsigned char home_selected;
static unsigned int seed_count;
static unsigned char create_method;
static UzZipRecord seed_record;
static UzCreatePlanOverlayRequest plan_request;

static const char *home_items[] = {
    "CREATE ZIP",
    "EXTRACT ZIP",
    "ABOUT / HELP"
};
#endif

static unsigned int resource_record_offset(unsigned char record_index) {
    return (unsigned int)(REUCB_RSRC_REC_OFF +
        ((unsigned int)record_index * REUCB_RSRC_REC_SIZE));
}

static unsigned char find_current_resource_bank(unsigned char kind,
                                                unsigned char slot_id) {
    unsigned char app_id;
    unsigned char index;
    unsigned char logical_bank;
    unsigned int offset;

    logical_bank = SHIM_CURRENT_BANK;
    if (logical_bank == 0u) return UZIP_BANK_NONE;
    app_id = readyos_bank_read_byte(
        (unsigned int)(REUCB_TOKEN_APP_OFF + logical_bank));
    if (app_id >= REUCB_APP_REG_COUNT) return UZIP_BANK_NONE;

    for (index = 0u; index < REUCB_RSRC_REC_COUNT; ++index) {
        offset = resource_record_offset(index);
        if (readyos_bank_read_byte(offset) == app_id &&
            readyos_bank_read_byte((unsigned int)(offset + 2u)) == kind &&
            readyos_bank_read_byte((unsigned int)(offset + 10u)) == slot_id) {
            logical_bank = readyos_bank_read_byte((unsigned int)(offset + 3u));
            if (logical_bank != 0u) return logical_bank;
        }
    }
    return UZIP_BANK_NONE;
}

static void set_status(const char *text) {
    strncpy(status_text, text, sizeof(status_text) - 1u);
    status_text[sizeof(status_text) - 1u] = 0;
}

#ifdef UZIP_SELF_SEED_PACKAGE
static void seed_put16(unsigned char at, unsigned int value) {
    seed_header[at] = (unsigned char)value;
    seed_header[(unsigned char)(at + 1u)] = (unsigned char)(value >> 8u);
}

static void seed_descriptor(unsigned char phase, unsigned int offset,
                            unsigned int size, unsigned int run,
                            unsigned int bss_size) {
    unsigned char at;

    at = (unsigned char)(UZ_PACKAGE_DESC_BASE +
                         phase * UZ_PACKAGE_DESC_SIZE);
    seed_put16(at + UZ_PACKAGE_FIELD_OFFSET, offset);
    seed_put16(at + UZ_PACKAGE_FIELD_SIZE, size);
    seed_put16(at + UZ_PACKAGE_FIELD_RUN, run);
    seed_put16(at + UZ_PACKAGE_FIELD_BSS_SIZE, bss_size);
}

static unsigned char seed_package(void) {
    unsigned int job_size;
    unsigned int inflate_size;
    unsigned int inflate_bss_size;
    unsigned int match_size;
    unsigned int emit_size;
    unsigned int coord_size;
    unsigned int coord_bss_size;
    unsigned int zip_read_size;
    unsigned int job_offset;
    unsigned int inflate_offset;
    unsigned int match_offset;
    unsigned int emit_offset;
    unsigned int coord_offset;
    unsigned int zip_read_offset;
    unsigned int package_size;

    job_size = uz_pack_job_size();
    inflate_size = uz_pack_inflate_size();
    inflate_bss_size = uz_pack_inflate_bss_size();
    match_size = uz_pack_deflate_match_size();
    emit_size = uz_pack_deflate_emit_size();
    coord_size = uz_pack_deflate_coord_size();
    coord_bss_size = uz_pack_deflate_coord_bss_size();
    zip_read_size = uz_pack_zip_read_size();
    if (job_size == 0u || job_size > 0x1100u ||
        match_size == 0u || match_size > 0x1000u ||
        emit_size == 0u || emit_size > 0x0D00u ||
        coord_size == 0u || coord_size > 0x0B00u ||
        zip_read_size == 0u || zip_read_size > 0x1500u ||
        inflate_size == 0u || inflate_size > 0x1000u ||
        uz_pack_job_run() != 0xB000u ||
        uz_pack_inflate_run() != 0xB000u ||
        uz_pack_inflate_bss_run() < 0xB000u ||
        uz_pack_inflate_bss_run() + inflate_bss_size > 0xC400u ||
        uz_pack_deflate_match_run() != 0xB000u ||
        uz_pack_deflate_emit_run() != 0xB000u ||
        uz_pack_zip_read_run() != 0xB000u ||
        uz_pack_deflate_coord_run() != 0xA000u ||
        uz_pack_deflate_coord_bss_run() < 0xA000u + coord_size ||
        uz_pack_deflate_coord_bss_run() + coord_bss_size > 0xB000u) {
        return 0u;
    }

    job_offset = UZ_PACKAGE_HEADER_SIZE;
    inflate_offset = (unsigned int)(job_offset + job_size);
    match_offset = (unsigned int)(inflate_offset + inflate_size);
    emit_offset = (unsigned int)(match_offset + match_size);
    coord_offset = (unsigned int)(emit_offset + emit_size);
    zip_read_offset = (unsigned int)(coord_offset + coord_size);
    package_size = (unsigned int)(zip_read_offset + zip_read_size);
    if (package_size > UZ_PACKAGE_MAX_SIZE) return 0u;

    memset(seed_header, 0, sizeof(seed_header));
    seed_header[0] = 0x55u; /* ASCII UZPK without target charset literals. */
    seed_header[1] = 0x5Au;
    seed_header[2] = 0x50u;
    seed_header[3] = 0x4Bu;
    seed_header[4] = UZ_PACKAGE_VERSION;
    seed_header[5] = UZ_PACKAGE_PHASE_COUNT;
    seed_put16(6u, UZ_PACKAGE_HEADER_SIZE);
    seed_put16(8u, package_size);
    seed_descriptor(UZ_PACKAGE_PHASE_JOB, job_offset, job_size, 0xB000u, 0u);
    seed_descriptor(UZ_PACKAGE_PHASE_INFLATE, inflate_offset, inflate_size,
                    0xB000u, inflate_bss_size);
    seed_descriptor(UZ_PACKAGE_PHASE_MATCH, match_offset, match_size,
                    0xB000u, 0u);
    seed_descriptor(UZ_PACKAGE_PHASE_EMIT, emit_offset, emit_size,
                    0xB000u, 0u);
    seed_descriptor(UZ_PACKAGE_PHASE_COORD, coord_offset, coord_size,
                    0xA000u, coord_bss_size);
    seed_descriptor(UZ_PACKAGE_PHASE_READER, zip_read_offset, zip_read_size,
                    0xB000u, 0u);
    reu_dma_stash((unsigned int)seed_header, package_bank, 0u,
                  sizeof(seed_header));
    reu_dma_stash(uz_pack_job_load(), package_bank,
                  job_offset, job_size);
    reu_dma_stash(uz_pack_inflate_load(), package_bank,
                  inflate_offset, inflate_size);
    reu_dma_stash(uz_pack_deflate_match_load(), package_bank,
                  match_offset, match_size);
    reu_dma_stash(uz_pack_deflate_emit_load(), package_bank,
                  emit_offset, emit_size);
    reu_dma_stash(uz_pack_deflate_coord_load(), package_bank,
                  coord_offset, coord_size);
    reu_dma_stash(uz_pack_zip_read_load(), package_bank,
                  zip_read_offset, zip_read_size);
    return uz_package_open(package_bank);
}
#endif

static unsigned char validate_package(void) {
    if (!uz_package_open(package_bank)) return 0u;
    return (unsigned char)(
        uz_package_phase_size(UZ_PACKAGE_PHASE_JOB) == uz_pack_job_size() &&
        uz_package_phase_run(UZ_PACKAGE_PHASE_JOB) == 0xB000u &&
        uz_package_phase_size(UZ_PACKAGE_PHASE_INFLATE) ==
            uz_pack_inflate_size() &&
        uz_package_phase_run(UZ_PACKAGE_PHASE_INFLATE) == 0xB000u &&
        uz_package_phase_bss_size(UZ_PACKAGE_PHASE_INFLATE) ==
            uz_pack_inflate_bss_size() &&
        uz_package_phase_size(UZ_PACKAGE_PHASE_MATCH) ==
            uz_pack_deflate_match_size() &&
        uz_package_phase_run(UZ_PACKAGE_PHASE_MATCH) == 0xB000u &&
        uz_package_phase_size(UZ_PACKAGE_PHASE_EMIT) ==
            uz_pack_deflate_emit_size() &&
        uz_package_phase_run(UZ_PACKAGE_PHASE_EMIT) == 0xB000u &&
        uz_package_phase_size(UZ_PACKAGE_PHASE_COORD) ==
            uz_pack_deflate_coord_size() &&
        uz_package_phase_run(UZ_PACKAGE_PHASE_COORD) == 0xA000u &&
        uz_package_phase_bss_size(UZ_PACKAGE_PHASE_COORD) ==
            uz_pack_deflate_coord_bss_size() &&
        uz_package_phase_size(UZ_PACKAGE_PHASE_READER) ==
            uz_pack_zip_read_size() &&
        uz_package_phase_run(UZ_PACKAGE_PHASE_READER) == 0xB000u);
}

#ifndef UZIP_PHYSICAL_DIAGNOSTIC
static void init_ui_dos(void) {
    uz_dos_init(&ui_dos, UZ_DOS_TARGET_READ,
                ui_command, sizeof(ui_command),
                ui_data, sizeof(ui_data),
                ui_status, sizeof(ui_status));
}

static void set_result_status(const char *text, unsigned char error,
                              unsigned char detail) {
    set_status(text);
    status_error = error;
    status_detail = detail;
}

#ifndef UZIP_SELF_SEED_PACKAGE
static unsigned char open_preloaded_package(void) {
    package_bank = find_current_resource_bank(
        REUCB_DEP_KIND_UZIP_PACKAGE, UZIP_PACKAGE_SLOT);
    if (package_bank == UZIP_BANK_NONE) {
        set_status("UZPACK PRELOAD RESOURCE MISSING");
        return 0u;
    }
    if (!validate_package()) {
        package_bank = UZIP_BANK_NONE;
        set_status("UZPACK PRELOAD INVALID");
        return 0u;
    }
    if (!uz_create_package_open(package_bank)) {
        package_bank = UZIP_BANK_NONE;
        set_status("UZPACK CREATE EXTENSION INVALID");
        return 0u;
    }
    return 1u;
}
#endif

static unsigned char name_safe(const char *name) {
    unsigned int length;
    unsigned char value;

    if (name == 0 || name[0] == 0) return 0u;
    length = 0u;
    while (name[length] != 0) {
        value = (unsigned char)name[length++];
        if (value < 0x20u || value > 0x7Eu || value == '/' ||
            value == '\\' || value == ':' || length >= sizeof(archive_name))
            return 0u;
    }
    return (unsigned char)(!(length == 1u && name[0] == '.') &&
        !(length == 2u && name[0] == '.' && name[1] == '.'));
}

static unsigned char join_path(char *destination, const char *path,
                               const char *leaf) {
    unsigned int path_len;
    unsigned int leaf_len;
    unsigned int at;

    path_len = strlen(path);
    leaf_len = strlen(leaf);
    if (path_len + leaf_len + (path_len == 1u ? 1u : 2u) >
        UZ_BROWSER_PATH_CAP) return 0u;
    memcpy(destination, path, path_len);
    at = path_len;
    if (path_len != 1u) destination[at++] = '/';
    memcpy(destination + at, leaf, leaf_len + 1u);
    return 1u;
}

static unsigned int seed_offset(unsigned int index) {
    return (unsigned int)(index * (unsigned int)sizeof(UzZipRecord));
}

static unsigned char seed_matches(const UzZipRecord *record,
                                  const UzBrowserEntry *entry) {
    unsigned int length;
    unsigned int index;

    length = strlen(record->name);
    if (record->directory) {
        if (length == 0u || record->name[length - 1u] != '/') return 0u;
        --length;
    }
    if (record->directory != entry->directory ||
        strlen(entry->name) != length) return 0u;
    for (index = 0u; index < length; ++index) {
        if (record->name[index] != entry->name[index]) return 0u;
    }
    return 1u;
}

static int find_seed(unsigned char catalog_bank,
                     const UzBrowserEntry *entry) {
    unsigned int index;

    for (index = 0u; index < seed_count; ++index) {
        reu_dma_fetch((unsigned int)&seed_record, catalog_bank,
                      seed_offset(index), sizeof(seed_record));
        if (seed_matches(&seed_record, entry)) return (int)index;
    }
    return -1;
}

static unsigned char toggle_seed(unsigned char catalog_bank,
                                 const UzBrowserEntry *entry) {
    int found;
    unsigned int index;
    unsigned int length;

    if (entry->unusable) return 0u;
    found = find_seed(catalog_bank, entry);
    if (found >= 0) {
        for (index = (unsigned int)found; index + 1u < seed_count; ++index) {
            reu_dma_fetch((unsigned int)&seed_record, catalog_bank,
                          seed_offset(index + 1u), sizeof(seed_record));
            reu_dma_stash((unsigned int)&seed_record, catalog_bank,
                          seed_offset(index), sizeof(seed_record));
        }
        --seed_count;
        return 1u;
    }
    if (seed_count >= UZ_CATALOG_MAX_ENTRIES) return 0u;
    memset(&seed_record, 0, sizeof(seed_record));
    length = strlen(entry->name);
    if (length == 0u || length + (entry->directory ? 2u : 1u) >
        sizeof(seed_record.name)) return 0u;
    memcpy(seed_record.name, entry->name, length);
    if (entry->directory) seed_record.name[length++] = '/';
    seed_record.name[length] = 0;
    seed_record.directory = entry->directory;
    seed_record.method = entry->directory ? 0u : 8u;
    reu_dma_stash((unsigned int)&seed_record, catalog_bank,
                  seed_offset(seed_count), sizeof(seed_record));
    ++seed_count;
    return 1u;
}

static unsigned char seed_current_folder(unsigned char catalog_bank) {
    unsigned int length;

    memset(&seed_record, 0, sizeof(seed_record));
    if (!uz_browser_split_current(browser_path, source_path,
                                  sizeof(source_path), seed_record.name,
                                  sizeof(seed_record.name))) return 0u;
    length = strlen(seed_record.name);
    if (length + 2u > sizeof(seed_record.name)) return 0u;
    seed_record.name[length++] = '/';
    seed_record.name[length] = 0;
    seed_record.directory = 1u;
    seed_record.method = 0u;
    reu_dma_stash((unsigned int)&seed_record, catalog_bank, 0u,
                  sizeof(seed_record));
    seed_count = 1u;
    return 1u;
}

static void apply_page_marks(unsigned char catalog_bank) {
    unsigned char row;

    for (row = 0u; row < browser_page.count; ++row) {
        browser_page.entries[row].attributes &= 0x7Fu;
        if (find_seed(catalog_bank, &browser_page.entries[row]) >= 0)
            browser_page.entries[row].attributes |= 0x80u;
    }
}

static void draw_browser(const char *title, unsigned char mode,
                         unsigned char selected) {
    TuiRect window = {0u, 0u, 40u, 25u};
    unsigned char row;
    unsigned char color;
    UzBrowserEntry *entry;

    tui_clear(TUI_THEME_BG);
    tui_window_title(&window, title, TUI_COLOR_LIGHTBLUE, TUI_COLOR_YELLOW);
    uz_browser_display(display_text, sizeof(display_text), browser_path);
    tui_puts_n(2u, 2u, display_text, 36u, TUI_COLOR_CYAN);
    for (row = 0u; row < browser_page.count; ++row) {
        entry = &browser_page.entries[row];
        color = row == selected ? TUI_COLOR_YELLOW :
                (entry->directory ? TUI_COLOR_LIGHTGREEN : TUI_COLOR_WHITE);
        tui_putc(2u, (unsigned char)(4u + row),
                 tui_ascii_to_screen(row == selected ? '>' :
                    (mode == UZIP_BROWSER_SOURCE &&
                     (entry->attributes & 0x80u) != 0u ? '*' : ' ')), color);
        uz_browser_display(display_text, sizeof(display_text), entry->name);
        tui_puts_n(4u, (unsigned char)(4u + row), display_text, 32u, color);
        if (entry->directory)
            tui_putc(37u, (unsigned char)(4u + row),
                     tui_ascii_to_screen('/'), color);
    }
    tui_puts(2u, 19u, mode == UZIP_BROWSER_SOURCE ?
             "SPACE MARK   RETURN OPEN FOLDER" :
             "RETURN OPEN/CHOOSE  LEFT PARENT", TUI_COLOR_GRAY3);
    if (mode == UZIP_BROWSER_FOLDER)
        tui_puts(2u, 21u, "F1 USE THIS FOLDER", TUI_COLOR_CYAN);
    else if (mode == UZIP_BROWSER_SOURCE) {
        tui_puts(2u, 21u, seed_count == 0u ?
                 "F1 CURRENT FOLDER  F5 CLEAR" :
                 "F1 USE MARKS       F5 CLEAR", TUI_COLOR_CYAN);
        tui_puts(25u, 23u, "MARKS", TUI_COLOR_GRAY3);
        tui_print_uint(32u, 23u, seed_count, TUI_COLOR_WHITE);
    }
    tui_puts(2u, 20u, "F3 TYPE FOLDER PATH", TUI_COLOR_CYAN);
    tui_puts(2u, 22u, "F7/F8 PAGE   RUN/STOP CANCEL", TUI_COLOR_GRAY3);
    tui_puts(2u, 23u, "PAGE", TUI_COLOR_GRAY3);
    tui_print_uint(7u, 23u, browser_page.page + 1u, TUI_COLOR_WHITE);
}

static unsigned char prompt_browser_path(unsigned char use_folder);

static unsigned char choose_sources(unsigned char catalog_bank) {
    unsigned char page;
    unsigned char selected;
    unsigned char key;

    strcpy(browser_path, "/usb1");
    seed_count = 0u;
    page = 0u;
    selected = 0u;
    init_ui_dos();
    if (!uz_dos_identify(&ui_dos)) return 0u;
    for (;;) {
        if (!uz_browser_list(&ui_dos, browser_path, page,
                             UZ_BROWSER_SHOW_ALL, &browser_page)) return 0u;
        browser_page.page = page;
        apply_page_marks(catalog_bank);
        if (selected >= browser_page.count) selected = 0u;
        for (;;) {
            draw_browser("CREATE: MARK SOURCES", UZIP_BROWSER_SOURCE,
                         selected);
            key = tui_getkey();
            if (key == TUI_KEY_RUNSTOP) return 0u;
            if (key == TUI_KEY_UP && selected != 0u) --selected;
            else if (key == TUI_KEY_DOWN &&
                     selected + 1u < browser_page.count) ++selected;
            else if ((key == ' ' || key == TUI_KEY_RETURN) &&
                     browser_page.count != 0u &&
                     (key == ' ' ||
                      !browser_page.entries[selected].directory)) {
                if (toggle_seed(catalog_bank,
                                &browser_page.entries[selected]))
                    apply_page_marks(catalog_bank);
            } else if (key == TUI_KEY_RETURN && seed_count == 0u &&
                       browser_page.count != 0u &&
                       browser_page.entries[selected].directory) {
                if (!uz_browser_enter(browser_path, sizeof(browser_path),
                        browser_page.entries[selected].name)) return 0u;
                page = 0u;
                selected = 0u;
                break;
            } else if (key == TUI_KEY_LEFT && seed_count == 0u) {
                uz_browser_parent(browser_path);
                page = 0u;
                selected = 0u;
                break;
            } else if (key == TUI_KEY_F3 && seed_count == 0u) {
                if (prompt_browser_path(0u)) {
                    page = 0u;
                    selected = 0u;
                    break;
                }
            } else if (key == TUI_KEY_F5) {
                seed_count = 0u;
                apply_page_marks(catalog_bank);
            } else if (key == TUI_KEY_F7 && page != 0u) {
                --page;
                selected = 0u;
                break;
            } else if (key == TUI_KEY_F8 && browser_page.more) {
                ++page;
                selected = 0u;
                break;
            } else if (key == TUI_KEY_F1) {
                if (seed_count != 0u) {
                    strcpy(source_path, browser_path);
                    return 1u;
                }
                if (seed_current_folder(catalog_bank)) return 1u;
            }
        }
    }
}

static unsigned char build_create_plan(unsigned char work_bank,
                                       unsigned char catalog_bank,
                                       unsigned int *entry_count) {
    unsigned char saved_port;
    unsigned char result;

    memset(&plan_request, 0, sizeof(plan_request));
    plan_request.dos = &ui_dos;
    plan_request.source_base = source_path;
    plan_request.absolute_output = path_scratch;
    plan_request.path = browser_path;
    plan_request.path_cap = sizeof(browser_path);
    plan_request.page = &browser_page;
    plan_request.seed_count = seed_count;
    plan_request.catalog_bank = catalog_bank;
    plan_request.method = create_method;
    reu_dma_stash(UZ_CREATE_PLAN_OVERLAY_RUN, work_bank,
                  UZ_CREATE_PLAN_OVERLAY_SAVE_OFFSET,
                  UZ_CREATE_PLAN_OVERLAY_MAX_SIZE);
    reu_dma_fetch(UZ_CREATE_PLAN_OVERLAY_RUN, package_bank,
                  UZ_CREATE_PLAN_OVERLAY_CACHE_OFFSET,
                  UZ_CREATE_PLAN_OVERLAY_MAX_SIZE);
    saved_port = UZIP_CPU_PORT;
    UZIP_CPU_PORT = (unsigned char)(saved_port & (unsigned char)~UZIP_LORAM);
    result = uz_create_plan_overlay_entry(&plan_request);
    UZIP_CPU_PORT = saved_port;
    reu_dma_fetch(UZ_CREATE_PLAN_OVERLAY_RUN, work_bank,
                  UZ_CREATE_PLAN_OVERLAY_SAVE_OFFSET,
                  UZ_CREATE_PLAN_OVERLAY_MAX_SIZE);
    *entry_count = plan_request.entry_count;
    return result;
}

static unsigned char create_progress(void *context, unsigned int completed,
                                     unsigned int total, const char *name) {
    TuiRect header = {0u, 0u, 40u, 4u};
    unsigned char key;

    (void)context;
    tui_clear(TUI_THEME_BG);
    tui_window_title(&header, "CREATING ZIP", TUI_COLOR_LIGHTBLUE,
                     TUI_COLOR_YELLOW);
    tui_puts(2u, 6u, "METHOD", TUI_COLOR_GRAY3);
    tui_puts(11u, 6u, create_method == 0u ? "STORE" : "COMPRESS",
             TUI_COLOR_WHITE);
    tui_puts(2u, 9u, "ENTRY", TUI_COLOR_GRAY3);
    tui_print_uint(10u, 9u, completed + 1u, TUI_COLOR_WHITE);
    tui_puts(16u, 9u, "OF", TUI_COLOR_GRAY3);
    tui_print_uint(20u, 9u, total, TUI_COLOR_WHITE);
    uz_browser_display(display_text, sizeof(display_text), name);
    tui_puts_n(2u, 12u, display_text, 36u, TUI_COLOR_CYAN);
    tui_puts(2u, 20u, "RUN/STOP CANCELS AT NEXT ENTRY",
             TUI_COLOR_GRAY3);
    if (!tui_kbhit()) return 1u;
    key = tui_getkey();
    return (unsigned char)(key != TUI_KEY_RUNSTOP);
}

static unsigned char extract_progress(void *context, unsigned int completed,
                                      unsigned int total, const char *name) {
    TuiRect header = {0u, 0u, 40u, 4u};
    unsigned char key;

    (void)context;
    tui_clear(TUI_THEME_BG);
    tui_window_title(&header, "EXTRACTING ZIP", TUI_COLOR_LIGHTBLUE,
                     TUI_COLOR_YELLOW);
    tui_puts(2u, 7u, "ENTRY", TUI_COLOR_GRAY3);
    tui_print_uint(10u, 7u, completed + 1u, TUI_COLOR_WHITE);
    tui_puts(16u, 7u, "OF", TUI_COLOR_GRAY3);
    tui_print_uint(20u, 7u, total, TUI_COLOR_WHITE);
    uz_browser_display(display_text, sizeof(display_text), name);
    tui_puts_n(2u, 11u, display_text, 36u, TUI_COLOR_CYAN);
    tui_puts(2u, 20u, "RUN/STOP CANCELS AT NEXT ENTRY",
             TUI_COLOR_GRAY3);
    if (!tui_kbhit()) return 1u;
    key = tui_getkey();
    return (unsigned char)(key != TUI_KEY_RUNSTOP);
}

static unsigned char prompt_browser_path(unsigned char use_folder) {
    TuiInput input;
    TuiRect box = {1u, 7u, 38u, 10u};
    unsigned char key;

    strcpy(path_scratch, browser_path);
    tui_clear(TUI_THEME_BG);
    tui_window_title(&box, "ABSOLUTE FOLDER PATH", TUI_COLOR_LIGHTBLUE,
                     TUI_COLOR_YELLOW);
    tui_puts(4u, 10u, "PATH:", TUI_COLOR_WHITE);
    tui_puts(4u, 14u, use_folder ? "RETURN USE FOLDER  RUN/STOP CANCEL" :
                                  "RETURN OPEN  RUN/STOP CANCEL",
             TUI_COLOR_GRAY3);
    tui_input_init(&input, 4u, 12u, 32u,
                   UZ_BROWSER_PATH_CAP - 1u, path_scratch,
                   TUI_COLOR_CYAN);
    strcpy(path_scratch, browser_path);
    input.cursor = (unsigned char)strlen(path_scratch);
    for (;;) {
        tui_input_draw(&input);
        key = tui_getkey();
        if (key == TUI_KEY_RUNSTOP) return 0u;
        if (tui_input_key(&input, key)) {
            if (path_scratch[0] != '/' || path_scratch[1] == 0) return 0u;
            strcpy(browser_path, path_scratch);
            return 1u;
        }
    }
}

static unsigned char choose_path(const char *title, unsigned char mode,
                                 char *chosen) {
    unsigned char page;
    unsigned char selected;
    unsigned char key;
    unsigned char folders_only;

    strcpy(browser_path, "/usb1");
    page = 0u;
    selected = 0u;
    init_ui_dos();
    if (!uz_dos_identify(&ui_dos)) return 0u;
    folders_only = mode == UZIP_BROWSER_FOLDER ?
                   UZ_BROWSER_FOLDERS : UZ_BROWSER_SHOW_ALL;
    for (;;) {
        if (!uz_browser_list(&ui_dos, browser_path, page,
                             folders_only, &browser_page)) return 0u;
        browser_page.page = page;
        if (selected >= browser_page.count) selected = 0u;
        for (;;) {
            draw_browser(title, mode, selected);
            key = tui_getkey();
            if (key == TUI_KEY_RUNSTOP) return 0u;
            if (key == TUI_KEY_UP && selected != 0u) --selected;
            else if (key == TUI_KEY_DOWN &&
                     selected + 1u < browser_page.count) ++selected;
            else if (key == TUI_KEY_F3) {
                if (prompt_browser_path(
                        (unsigned char)(mode == UZIP_BROWSER_FOLDER))) {
                    /* A typed destination may validly be empty. Ultimate DOS
                     * can reject READ_DIR when there is no entry block, so
                     * validate the directory with CD+OPEN_DIR and use it
                     * directly instead of requiring a listing payload. */
                    if (mode == UZIP_BROWSER_FOLDER &&
                        uz_browser_open_folder(&ui_dos, browser_path)) {
                        strcpy(chosen, browser_path);
                        return 1u;
                    }
                    page = 0u;
                    selected = 0u;
                    break;
                }
            }
            else if (key == TUI_KEY_LEFT) {
                uz_browser_parent(browser_path);
                page = 0u;
                selected = 0u;
                break;
            } else if (key == TUI_KEY_F7 && page != 0u) {
                --page;
                selected = 0u;
                break;
            } else if (key == TUI_KEY_F8 && browser_page.more) {
                ++page;
                selected = 0u;
                break;
            } else if (key == TUI_KEY_F1 &&
                       mode == UZIP_BROWSER_FOLDER) {
                strcpy(chosen, browser_path);
                return 1u;
            } else if (key == TUI_KEY_RETURN && browser_page.count != 0u) {
                if (browser_page.entries[selected].unusable) continue;
                if (browser_page.entries[selected].directory) {
                    if (!uz_browser_enter(browser_path,
                                          sizeof(browser_path),
                                          browser_page.entries[selected].name))
                        return 0u;
                    page = 0u;
                    selected = 0u;
                    break;
                }
                if (mode == UZIP_BROWSER_FILE) {
                    strcpy(chosen, browser_path);
                    if (!uz_browser_enter(chosen, UZ_BROWSER_PATH_CAP,
                            browser_page.entries[selected].name)) return 0u;
                    return 1u;
                }
            }
        }
    }
}

static unsigned char prompt_archive_name(void) {
    TuiInput input;
    TuiRect box = {2u, 8u, 36u, 8u};
    unsigned char key;

    tui_clear(TUI_THEME_BG);
    tui_window_title(&box, "ARCHIVE NAME", TUI_COLOR_LIGHTBLUE,
                     TUI_COLOR_YELLOW);
    tui_puts(5u, 10u, "NAME:", TUI_COLOR_WHITE);
    tui_puts(5u, 14u, "RETURN ACCEPT  RUN/STOP CANCEL", TUI_COLOR_GRAY3);
    tui_input_init(&input, 11u, 10u, 23u,
                   sizeof(archive_name) - 1u, archive_name,
                   TUI_COLOR_CYAN);
    strcpy(archive_name, "archive.zip");
    input.cursor = (unsigned char)strlen(archive_name);
    for (;;) {
        tui_input_draw(&input);
        key = tui_getkey();
        if (key == TUI_KEY_RUNSTOP) return 0u;
        if (tui_input_key(&input, key)) return name_safe(archive_name);
    }
}

static unsigned char prompt_create_method(void) {
    TuiRect box = {3u, 6u, 34u, 13u};
    unsigned char selected;
    unsigned char key;

    selected = create_method == 0u ? 0u : 1u;
    for (;;) {
        tui_clear(TUI_THEME_BG);
        tui_window_title(&box, "ZIP METHOD", TUI_COLOR_LIGHTBLUE,
                         TUI_COLOR_YELLOW);
        tui_puts(7u, 9u, selected == 0u ? "> STORE" : "  STORE",
                 selected == 0u ? TUI_COLOR_YELLOW : TUI_COLOR_WHITE);
        tui_puts(7u, 10u, "  FAST, NO COMPRESSION", TUI_COLOR_GRAY3);
        tui_puts(7u, 13u, selected == 1u ? "> COMPRESS" : "  COMPRESS",
                 selected == 1u ? TUI_COLOR_YELLOW : TUI_COLOR_WHITE);
        tui_puts(7u, 14u, "  FIXED DEFLATE", TUI_COLOR_GRAY3);
        tui_puts(5u, 17u, "RETURN SELECT  RUN/STOP CANCEL",
                 TUI_COLOR_CYAN);
        key = tui_getkey();
        if (key == TUI_KEY_RUNSTOP) return 0u;
        if (key == TUI_KEY_UP || key == TUI_KEY_DOWN)
            selected = (unsigned char)(selected == 0u ? 1u : 0u);
        else if (key == TUI_KEY_RETURN) {
            create_method = selected == 0u ? 0u : 8u;
            return 1u;
        }
    }
}

static unsigned char confirm_operation(const char *title,
                                       const char *from,
                                       const char *to,
                                       const char *method,
                                       unsigned int entry_count) {
    unsigned char key;

    tui_clear(TUI_THEME_BG);
    tui_puts(1u, 1u, title, TUI_COLOR_YELLOW);
    tui_puts(1u, 4u, "FROM", TUI_COLOR_GRAY3);
    uz_browser_display(display_text, sizeof(display_text), from);
    tui_puts_n(1u, 5u, display_text, 38u, TUI_COLOR_WHITE);
    tui_puts(1u, 8u, "TO", TUI_COLOR_GRAY3);
    uz_browser_display(display_text, sizeof(display_text), to);
    tui_puts_n(1u, 9u, display_text, 38u, TUI_COLOR_WHITE);
    if (method != 0) {
        tui_puts(1u, 12u, "METHOD", TUI_COLOR_GRAY3);
        tui_puts(10u, 12u, method, TUI_COLOR_WHITE);
        tui_puts(1u, 14u, "ENTRIES", TUI_COLOR_GRAY3);
        tui_print_uint(10u, 14u, entry_count, TUI_COLOR_WHITE);
    }
    tui_puts(1u, 20u, "RETURN START   RUN/STOP CANCEL", TUI_COLOR_CYAN);
    do { key = tui_getkey(); } while (key != TUI_KEY_RETURN &&
                                      key != TUI_KEY_RUNSTOP);
    return (unsigned char)(key == TUI_KEY_RETURN);
}

static unsigned char allocate_operation_banks(unsigned char *work_bank,
                                              unsigned char *catalog_bank) {
    *work_bank = reu_alloc_owned_bank(UZIP_WORK_SLOT, "uzwk");
    *catalog_bank = reu_alloc_owned_bank(UZIP_CATALOG_SLOT, "uzct");
    if (*work_bank == UZIP_BANK_NONE || *catalog_bank == UZIP_BANK_NONE ||
        *work_bank == *catalog_bank || *work_bank == package_bank ||
        *catalog_bank == package_bank) {
        if (*work_bank != UZIP_BANK_NONE) reu_free_owned_bank(*work_bank);
        if (*catalog_bank != UZIP_BANK_NONE)
            reu_free_owned_bank(*catalog_bank);
        *work_bank = UZIP_BANK_NONE;
        *catalog_bank = UZIP_BANK_NONE;
        return 0u;
    }
    return 1u;
}

static unsigned char release_operation_banks(unsigned char work_bank,
                                             unsigned char catalog_bank) {
    if (catalog_bank != UZIP_BANK_NONE) reu_free_owned_bank(catalog_bank);
    if (work_bank != UZIP_BANK_NONE) reu_free_owned_bank(work_bank);

    /* A COMPLETE status is also the production hardware oracle for transient
     * REU lifecycle cleanup. Check both authoritative tables: the physical
     * bank allocation map and this app's owned-resource records. */
    if ((catalog_bank != UZIP_BANK_NONE &&
         reu_bank_type(catalog_bank) != REU_FREE) ||
        (work_bank != UZIP_BANK_NONE &&
         reu_bank_type(work_bank) != REU_FREE) ||
        find_current_resource_bank(REUCB_DEP_KIND_APP_ALLOC,
                                   UZIP_CATALOG_SLOT) != UZIP_BANK_NONE ||
        find_current_resource_bank(REUCB_DEP_KIND_APP_ALLOC,
                                   UZIP_WORK_SLOT) != UZIP_BANK_NONE) {
        return 0u;
    }
    return 1u;
}

static void run_create(void) {
    unsigned char work_bank;
    unsigned char catalog_bank;
    unsigned char ok;
    unsigned char error;
    unsigned char detail;
    unsigned char cleanup_ok;
    unsigned int entry_count;

    status_error = 0u;
    status_detail = 0u;
    create_method = 8u;
    if (!allocate_operation_banks(&work_bank, &catalog_bank)) {
        set_status("NEED TWO FREE READYOS REU BANKS");
        return;
    }
    if (!choose_sources(catalog_bank) ||
        !choose_path("CREATE: OUTPUT FOLDER", UZIP_BROWSER_FOLDER,
                     target_path) || !prompt_archive_name() ||
        !prompt_create_method() ||
        !join_path(path_scratch, target_path, archive_name)) {
        cleanup_ok = release_operation_banks(work_bank, catalog_bank);
        set_status(cleanup_ok ? "CREATE CANCELLED" : "CREATE CLEANUP FAILED");
        return;
    }
    if (!build_create_plan(work_bank, catalog_bank, &entry_count)) {
        error = plan_request.error;
        cleanup_ok = release_operation_banks(work_bank, catalog_bank);
        if (!cleanup_ok) set_result_status("CREATE CLEANUP FAILED", 0u, 0u);
        else set_result_status("CREATE PLAN FAILED", error, 0u);
        return;
    }
    if (!confirm_operation("CREATE ZIP?", source_path, path_scratch,
                           create_method == 0u ? "STORE" : "COMPRESS",
                           entry_count)) {
        cleanup_ok = release_operation_banks(work_bank, catalog_bank);
        set_status(cleanup_ok ? "CREATE CANCELLED" : "CREATE CLEANUP FAILED");
        return;
    }
    ok = uz_workflow_create(package_bank, work_bank, catalog_bank,
                            source_path, target_path, archive_name,
                            entry_count, create_progress, 0);
    error = uz_workflow_error();
    detail = uz_workflow_detail();
    cleanup_ok = release_operation_banks(work_bank, catalog_bank);
    if (!cleanup_ok) set_result_status("CREATE CLEANUP FAILED", 0u, 0u);
    else if (ok) set_result_status("CREATE COMPLETE", 0u, 0u);
    else if (error == UZ_WORKFLOW_CANCEL)
        set_result_status("CREATE CANCELLED", error, detail);
    else set_result_status("CREATE FAILED", error, detail);
}

static void run_extract(void) {
    unsigned char work_bank;
    unsigned char catalog_bank;
    unsigned char ok;
    unsigned char error;
    unsigned char detail;
    unsigned char cleanup_ok;

    status_error = 0u;
    status_detail = 0u;
    if (!choose_path("EXTRACT: CHOOSE ZIP", UZIP_BROWSER_FILE,
                     source_path) ||
        !choose_path("EXTRACT: DESTINATION", UZIP_BROWSER_FOLDER,
                     target_path) ||
        !confirm_operation("EXTRACT ZIP?", source_path, target_path,
                           0, 0u)) {
        set_status("EXTRACT CANCELLED");
        return;
    }
    if (!allocate_operation_banks(&work_bank, &catalog_bank)) {
        set_status("NEED TWO FREE READYOS REU BANKS");
        return;
    }
    ok = uz_workflow_extract(package_bank, work_bank, catalog_bank,
                             source_path, target_path,
                             extract_progress, 0);
    error = uz_workflow_error();
    detail = uz_workflow_detail();
    cleanup_ok = release_operation_banks(work_bank, catalog_bank);
    if (!cleanup_ok) set_result_status("EXTRACT CLEANUP FAILED", 0u, 0u);
    else if (ok) set_result_status("EXTRACT COMPLETE", 0u, 0u);
    else if (error == UZ_WORKFLOW_CANCEL)
        set_result_status("EXTRACT CANCELLED", error, detail);
    else set_result_status("EXTRACT FAILED", error, detail);
}

static void draw_home(unsigned char selected) {
    TuiRect header = {0u, 0u, 40u, 4u};
    unsigned char index;
    unsigned char color;

    tui_clear(TUI_THEME_BG);
    tui_window_title(&header, "ULTIMATE ZIP",
                     TUI_COLOR_LIGHTBLUE, TUI_COLOR_YELLOW);
    tui_puts(2u, 1u, "ULT.DOS", TUI_COLOR_GRAY3);
    tui_puts(12u, 1u, "REU PACKAGE", TUI_COLOR_GRAY3);
    tui_puts(2u, 2u, package_bank == UZIP_BANK_NONE ?
             "CORE UNAVAILABLE" : "CREATE + EXTRACT",
             package_bank == UZIP_BANK_NONE ?
             TUI_COLOR_LIGHTRED : TUI_COLOR_LIGHTGREEN);
    if (package_bank == UZIP_BANK_NONE) {
        tui_puts(14u, 2u, "UNAVAILABLE", TUI_COLOR_LIGHTRED);
    } else {
        tui_puts(14u, 2u, "BANK", TUI_COLOR_LIGHTGREEN);
        tui_print_uint(19u, 2u, package_bank, TUI_COLOR_WHITE);
    }

    tui_puts(1u, 5u, "ULTIMATE HOST ARCHIVES", TUI_COLOR_GRAY3);
    for (index = 0u; index < 3u; ++index) {
        color = index == selected ? TUI_COLOR_YELLOW : TUI_COLOR_WHITE;
        tui_putc(4u, (unsigned char)(8u + index * 2u),
                 tui_ascii_to_screen(index == selected ? '>' : ' '), color);
        tui_puts(6u, (unsigned char)(8u + index * 2u),
                 home_items[index], color);
    }
    tui_puts(1u, 21u, "RETURN SELECT   RUN/STOP LAUNCHER",
             TUI_COLOR_CYAN);
    tui_puts(1u, 22u, "F2/F4 SWITCH WHEN IDLE", TUI_COLOR_GRAY3);
    tui_clear_line(24u, 0u, 40u, TUI_THEME_BG);
    tui_puts_n(1u, 24u, status_text, 38u,
               package_bank == UZIP_BANK_NONE
                   ? TUI_COLOR_LIGHTRED : TUI_COLOR_GRAY3);
    if (status_error != 0u) {
        tui_print_uint(32u, 24u, status_error, TUI_COLOR_LIGHTRED);
        tui_putc(35u, 24u, tui_ascii_to_screen('/'), TUI_COLOR_LIGHTRED);
        tui_print_uint(36u, 24u, status_detail, TUI_COLOR_LIGHTRED);
    }
}

static void handle_selection(unsigned char selected) {
    switch (selected) {
        case 0u:
            if (package_bank != UZIP_BANK_NONE) run_create();
            break;
        case 1u:
            if (package_bank != UZIP_BANK_NONE) run_extract();
            break;
        default:
            set_status("ULTIMATE DOS ZIP - C64U ONLY");
            break;
    }
}

static void handle_global_key(unsigned char key) {
    unsigned char target;

    target = tui_handle_global_hotkey(key, SHIM_CURRENT_BANK, 1u);
    if (target == TUI_HOTKEY_LAUNCHER) {
        tui_return_to_launcher();
    } else if (target >= TUI_APP_BANK_MIN && target <= TUI_APP_BANK_MAX) {
        tui_switch_to_app(target);
    }
}

static void run_home(void) {
    unsigned char key;

    while (running) {
        draw_home(home_selected);
        key = tui_getkey();
        if (key == TUI_KEY_RUNSTOP) {
            tui_return_to_launcher();
        } else if (key == TUI_KEY_RETURN) {
            handle_selection(home_selected);
        } else if (key == TUI_KEY_UP && home_selected != 0u) {
            --home_selected;
        } else if (key == TUI_KEY_DOWN && home_selected < 2u) {
            ++home_selected;
        } else {
            handle_global_key(key);
        }
    }
}

#ifdef UZIP_COLD_UI
int uzip_ui_warm_main(void) {
    tui_init();
    tui_keyrepeat_default();
    running = 1u;
    /* Resident package descriptors are normal BSS and are cleared when the
     * cc65 entry at $1000 runs again. Reopen them, but preserve the visible
     * operation result and home selection from snapshotted UI_BSS. */
    (void)open_preloaded_package();
    run_home();
    return 0;
}
#endif
#endif

#ifdef UZIP_COLD_UI
int uzip_ui_main(void) {
#else
int main(void) {
#endif
    tui_init();
    tui_keyrepeat_default();

#ifdef UZIP_SELF_SEED_PACKAGE
    if (package_bank == UZIP_BANK_NONE) {
        package_bank = reu_alloc_owned_bank(UZIP_PACKAGE_SLOT, "uzpk");
    }
    if (package_bank == UZIP_BANK_NONE) {
        set_status("NEED ONE FREE READYOS REU BANK");
#ifdef UZIP_XUZREU_DIAGNOSTIC
    } else if (xuzreu_diag_result_present()) {
        /* A warm diagnostic entry must validate the package bytes which
         * survived the snapshot. Do not make the check vacuous by reseeding. */
        if (!validate_package()) {
            package_bank = UZIP_BANK_NONE;
            set_status("PACKED JOB IMAGE INVALID");
        }
#endif
    } else if (!seed_package()) {
        set_status("PACKED JOB IMAGE INVALID");
    } else {
        set_status("ZIP CORE PACKED - PROBES PENDING");
    }
#else
    if (open_preloaded_package()) {
        set_status("ZIP CORE READY");
    }
#endif

#ifdef UZIP_XUZREU_DIAGNOSTIC
    xuzreu_diag_run(package_bank);
#endif
#ifdef UZIP_XUZDEFLATE_DIAGNOSTIC
    xuzdeflate_diag_run(package_bank);
#endif
#ifdef UZIP_XUZZIP8_DIAGNOSTIC
    xuzzip8_diag_run(package_bank);
#endif
#ifdef UZIP_XUZMULTI_DIAGNOSTIC
    xuzmulti_diag_run(package_bank);
#endif
#ifdef UZIP_XUZREAD_DIAGNOSTIC
    xuzread_diag_run(package_bank);
#endif
#ifdef UZIP_XUZEXTRACT_DIAGNOSTIC
    xuzextract_diag_run(package_bank);
#endif
#ifdef UZIP_XUZCREATEPLAN_DIAGNOSTIC
    xuzcreateplan_diag_run(package_bank);
#endif

#ifndef UZIP_PHYSICAL_DIAGNOSTIC
    home_selected = 0u;
#ifdef UZIP_COLD_UI
    uzip_ui_resume_marker[0] = 0x55u; /* ASCII UZW1, charset-independent. */
    uzip_ui_resume_marker[1] = 0x5Au;
    uzip_ui_resume_marker[2] = 0x57u;
    uzip_ui_resume_marker[3] = 0x31u;
#endif
    run_home();
#endif
    return 0;
}

#pragma bss-name(pop)
#pragma rodata-name(pop)
#pragma code-name(pop)
