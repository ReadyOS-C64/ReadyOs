/* Physical callback-parser/catalog probe. No destination is ever created. */

#include "xuzread_diag.h"

#include "xuzread_config.h"
#include "uz_dos.h"
#include "uz_pack.h"
#include "uz_package.h"
#include "uz_zip_read.h"

#include "../../lib/reu_control_bank.h"
#include "../../lib/reu_mgr.h"
#include "../../lib/reu_owned_alloc.h"
#include "../../lib/tui.h"

#include <string.h>

#define XUZR_RESULT ((volatile unsigned char *)0x033Cu)
#define XUZR_DOS ((UzDos *)0x0400u)
#define XUZR_COMMAND ((unsigned char *)0x0460u)
#define XUZR_DATA ((unsigned char *)0x0500u)
#define XUZR_STATUS ((unsigned char *)0x0540u)
#define XUZR_READER ((UzZipReader *)0x0580u)
#define XUZR_RECORD ((UzZipRecord *)0x05C0u)
#define XUZR_SCRATCH ((unsigned char *)0x0700u)

#define XUZR_DOS_CAP 160u
#define XUZR_DATA_CAP 64u
#define XUZR_STATUS_CAP 64u
#define XUZR_SCRATCH_CAP 512u
#define XUZR_NONE 0xFFu
#define XUZR_WORK_SLOT 2u
#define XUZR_CATALOG_SLOT 3u
#define XUZR_REU_READ_OFFSET 0x9000u
#define XUZR_CPU_PORT (*(volatile unsigned char *)0x0001u)
#define XUZR_LORAM 0x01u

#define XUZR_STAGE_READY 1u
#define XUZR_STAGE_BANKS 2u
#define XUZR_STAGE_OWNER 3u
#define XUZR_STAGE_OPEN 4u
#define XUZR_STAGE_PARSE 5u
#define XUZR_STAGE_CATALOG 6u
#define XUZR_STAGE_DONE 7u

#pragma code-name(push, "UI_CODE")
#pragma rodata-name(push, "UI_RODATA")
#pragma bss-name(push, "UI_BSS")

static const unsigned int expected_size_lo[XUZREAD_ENTRY_COUNT] = {
    XUZREAD_SIZE_LO_0, XUZREAD_SIZE_LO_1, XUZREAD_SIZE_LO_2,
    XUZREAD_SIZE_LO_3, XUZREAD_SIZE_LO_4, XUZREAD_SIZE_LO_5,
    XUZREAD_SIZE_LO_6
};
static const unsigned int expected_size_hi[XUZREAD_ENTRY_COUNT] = {
    XUZREAD_SIZE_HI_0, XUZREAD_SIZE_HI_1, XUZREAD_SIZE_HI_2,
    XUZREAD_SIZE_HI_3, XUZREAD_SIZE_HI_4, XUZREAD_SIZE_HI_5,
    XUZREAD_SIZE_HI_6
};
static const unsigned int expected_compressed_lo[XUZREAD_ENTRY_COUNT] = {
    XUZREAD_COMPRESSED_LO_0, XUZREAD_COMPRESSED_LO_1,
    XUZREAD_COMPRESSED_LO_2, XUZREAD_COMPRESSED_LO_3,
    XUZREAD_COMPRESSED_LO_4, XUZREAD_COMPRESSED_LO_5,
    XUZREAD_COMPRESSED_LO_6
};
static const unsigned int expected_compressed_hi[XUZREAD_ENTRY_COUNT] = {
    XUZREAD_COMPRESSED_HI_0, XUZREAD_COMPRESSED_HI_1,
    XUZREAD_COMPRESSED_HI_2, XUZREAD_COMPRESSED_HI_3,
    XUZREAD_COMPRESSED_HI_4, XUZREAD_COMPRESSED_HI_5,
    XUZREAD_COMPRESSED_HI_6
};
static const unsigned int expected_offset_lo[XUZREAD_ENTRY_COUNT] = {
    XUZREAD_OFFSET_LO_0, XUZREAD_OFFSET_LO_1, XUZREAD_OFFSET_LO_2,
    XUZREAD_OFFSET_LO_3, XUZREAD_OFFSET_LO_4, XUZREAD_OFFSET_LO_5,
    XUZREAD_OFFSET_LO_6
};
static const unsigned int expected_offset_hi[XUZREAD_ENTRY_COUNT] = {
    XUZREAD_OFFSET_HI_0, XUZREAD_OFFSET_HI_1, XUZREAD_OFFSET_HI_2,
    XUZREAD_OFFSET_HI_3, XUZREAD_OFFSET_HI_4, XUZREAD_OFFSET_HI_5,
    XUZREAD_OFFSET_HI_6
};
static const unsigned char expected_method[XUZREAD_ENTRY_COUNT] = {
    XUZREAD_METHOD_0, XUZREAD_METHOD_1, XUZREAD_METHOD_2,
    XUZREAD_METHOD_3, XUZREAD_METHOD_4, XUZREAD_METHOD_5,
    XUZREAD_METHOD_6
};
static const unsigned char expected_directory[XUZREAD_ENTRY_COUNT] = {
    XUZREAD_DIRECTORY_0, XUZREAD_DIRECTORY_1, XUZREAD_DIRECTORY_2,
    XUZREAD_DIRECTORY_3, XUZREAD_DIRECTORY_4, XUZREAD_DIRECTORY_5,
    XUZREAD_DIRECTORY_6
};
static const unsigned char expected_crc[XUZREAD_ENTRY_COUNT][4] = {
    {XUZREAD_CRC_0_0, XUZREAD_CRC_0_1, XUZREAD_CRC_0_2, XUZREAD_CRC_0_3},
    {XUZREAD_CRC_1_0, XUZREAD_CRC_1_1, XUZREAD_CRC_1_2, XUZREAD_CRC_1_3},
    {XUZREAD_CRC_2_0, XUZREAD_CRC_2_1, XUZREAD_CRC_2_2, XUZREAD_CRC_2_3},
    {XUZREAD_CRC_3_0, XUZREAD_CRC_3_1, XUZREAD_CRC_3_2, XUZREAD_CRC_3_3},
    {XUZREAD_CRC_4_0, XUZREAD_CRC_4_1, XUZREAD_CRC_4_2, XUZREAD_CRC_4_3},
    {XUZREAD_CRC_5_0, XUZREAD_CRC_5_1, XUZREAD_CRC_5_2, XUZREAD_CRC_5_3},
    {XUZREAD_CRC_6_0, XUZREAD_CRC_6_1, XUZREAD_CRC_6_2, XUZREAD_CRC_6_3}
};

static const char *expected_name(unsigned char index) {
    switch (index) {
        case 0u: return XUZREAD_NAME_0;
        case 1u: return XUZREAD_NAME_1;
        case 2u: return XUZREAD_NAME_2;
        case 3u: return XUZREAD_NAME_3;
        case 4u: return XUZREAD_NAME_4;
        case 5u: return XUZREAD_NAME_5;
        default: return XUZREAD_NAME_6;
    }
}

static unsigned char bank_type(unsigned char bank) {
    if (bank == XUZR_NONE) return REU_UNAVAIL;
    return readyos_bank_read_byte((unsigned int)(REUCB_BANK_TYPE_OFF + bank));
}

static unsigned char screen_code(unsigned char value) {
    if (value >= 0x41u && value <= 0x5Au) return (unsigned char)(value - 0x40u);
    if (value >= 0xC1u && value <= 0xDAu) return (unsigned char)(value - 0xC0u);
    return value;
}

static void screen_text(unsigned char row, const char *text) {
    volatile unsigned char *screen;
    unsigned char column;

    screen = (volatile unsigned char *)(0x0400u + (unsigned int)row * 40u);
    for (column = 0u; column < 40u; ++column)
        screen[column] = (*text != 0) ? screen_code((unsigned char)*text++) : 0x20u;
}

static void result_u16(unsigned char offset, unsigned int value) {
    XUZR_RESULT[offset] = (unsigned char)value;
    XUZR_RESULT[(unsigned char)(offset + 1u)] = (unsigned char)(value >> 8u);
}

static void result_u32(unsigned char offset, const UzU32 *value) {
    result_u16(offset, value->lo);
    result_u16((unsigned char)(offset + 2u), value->hi);
}

static void add_callback_bytes(unsigned int length) {
    unsigned int before;

    before = (unsigned int)(XUZR_RESULT[22] |
                            ((unsigned int)XUZR_RESULT[23] << 8u));
    before = (unsigned int)(before + length);
    result_u16(22u, before);
    if (before < length) {
        before = (unsigned int)(XUZR_RESULT[24] |
                                ((unsigned int)XUZR_RESULT[25] << 8u));
        result_u16(24u, (unsigned int)(before + 1u));
    }
}

static unsigned char read_at(void *context, const UzU32 *offset,
                             unsigned char *destination,
                             unsigned int length) {
    unsigned int transferred;
    unsigned int calls;
    unsigned char work_bank;

    (void)context;
    work_bank = XUZR_RESULT[9];
    if (length == 0u || length > XUZR_SCRATCH_CAP) return 0u;
    transferred = 0u;
    /* The shared UCI transport owns synchronization, one asynchronous PUSH,
     * complete queue draining, DATA_ACC transitions, recovery, and the final
     * quiet-idle wait. This callback adds no instruction-delay pacing. */
    if (!uz_dos_seek(XUZR_DOS, offset) ||
        !uz_dos_load_reu(XUZR_DOS, work_bank, XUZR_REU_READ_OFFSET,
                         length, &transferred) || transferred != length)
        return 0u;
    reu_dma_fetch((unsigned int)destination, work_bank,
                  XUZR_REU_READ_OFFSET, length);
    calls = (unsigned int)(XUZR_RESULT[20] |
                           ((unsigned int)XUZR_RESULT[21] << 8u));
    result_u16(20u, (unsigned int)(calls + 1u));
    add_callback_bytes(length);
    if (length > (unsigned int)(XUZR_RESULT[26] |
            ((unsigned int)XUZR_RESULT[27] << 8u))) result_u16(26u, length);
    return 1u;
}

static unsigned char record_matches(unsigned char index,
                                    const UzZipRecord *record) {
    return (unsigned char)(index < XUZREAD_ENTRY_COUNT &&
        strcmp(record->name, expected_name(index)) == 0 &&
        record->method == expected_method[index] &&
        record->directory == expected_directory[index] &&
        record->size.lo == expected_size_lo[index] &&
        record->size.hi == expected_size_hi[index] &&
        record->compressed_size.lo == expected_compressed_lo[index] &&
        record->compressed_size.hi == expected_compressed_hi[index] &&
        record->local_offset.lo == expected_offset_lo[index] &&
        record->local_offset.hi == expected_offset_hi[index] &&
        memcmp(record->crc.byte, expected_crc[index], 4u) == 0);
}

static void load_store(unsigned char package_bank) {
    reu_dma_fetch(uz_pack_job_run(), package_bank,
                  uz_package_phase_offset(UZ_PACKAGE_PHASE_JOB),
                  uz_pack_job_size());
}

static void load_reader(unsigned char package_bank) {
    reu_dma_fetch(uz_pack_zip_read_run(), package_bank,
                  uz_package_phase_offset(UZ_PACKAGE_PHASE_READER),
                  uz_pack_zip_read_size());
}

static unsigned char owner_valid(void) {
    unsigned int index;
    int got;

    if (!uz_dos_change_absolute(XUZR_DOS, XUZREAD_OWNED_ROOT) ||
        !uz_dos_open(XUZR_DOS, ".readyos-uzip-owner", UZ_DOS_OPEN_READ))
        return 0u;
    got = uz_dos_read(XUZR_DOS, XUZR_DATA, XUZREAD_OWNER_LENGTH);
    if (!uz_dos_close(XUZR_DOS) || got < 0 ||
        (unsigned int)got != XUZREAD_OWNER_LENGTH) return 0u;
    for (index = 0u; index < XUZREAD_OWNER_LENGTH; ++index)
        if (XUZR_DATA[index] != (unsigned char)XUZREAD_OWNER_TEXT[index])
            return 0u;
    return 1u;
}

static unsigned char parse_archive(unsigned char package_bank,
                                   unsigned char catalog_bank,
                                   const UzU32 *archive_size) {
    unsigned char index;
    unsigned int catalog_offset;
    UzU32 data_offset;

    load_reader(package_bank);
    uz_zip_reader_init_at(XUZR_READER, archive_size, read_at, 0);
    if (!uz_zip_reader_begin(XUZR_READER, XUZR_SCRATCH, XUZR_SCRATCH_CAP))
        return 0u;
    if (XUZR_READER->entry_count != XUZREAD_ENTRY_COUNT ||
        XUZR_READER->central_offset.lo != XUZREAD_CENTRAL_LO ||
        XUZR_READER->central_offset.hi != XUZREAD_CENTRAL_HI ||
        XUZR_READER->central_size.lo != XUZREAD_CENTRAL_SIZE_LO ||
        XUZR_READER->central_size.hi != XUZREAD_CENTRAL_SIZE_HI)
        return 0u;
    for (index = 0u; index < XUZREAD_ENTRY_COUNT; ++index) {
        if (!uz_zip_reader_next(XUZR_READER, XUZR_RECORD) ||
            !record_matches(index, XUZR_RECORD) ||
            !uz_zip_reader_local(XUZR_READER, XUZR_RECORD, &data_offset,
                                 XUZR_SCRATCH, XUZR_SCRATCH_CAP)) return 0u;
        catalog_offset = (unsigned int)(index * (unsigned int)sizeof(UzZipRecord));
        reu_dma_stash((unsigned int)XUZR_RECORD, catalog_bank,
                      catalog_offset, sizeof(UzZipRecord));
        XUZR_RESULT[(unsigned char)(48u + index)] =
            (unsigned char)XUZR_RECORD->method;
        if (XUZR_RECORD->directory) XUZR_RESULT[55] |= (unsigned char)(1u << index);
    }
    if (!uz_zip_reader_finished(XUZR_READER)) return 0u;
    result_u32(32u, &XUZR_READER->central_offset);
    result_u32(36u, &XUZR_READER->central_size);
    for (index = 0u; index < XUZREAD_ENTRY_COUNT; ++index) {
        catalog_offset = (unsigned int)(index * (unsigned int)sizeof(UzZipRecord));
        reu_dma_fetch((unsigned int)XUZR_RECORD, catalog_bank,
                      catalog_offset, sizeof(UzZipRecord));
        if (!record_matches(index, XUZR_RECORD)) return 0u;
    }
    XUZR_RESULT[40] = 1u;
    return 1u;
}

static void finish_screen(unsigned char passed) {
    tui_clear(TUI_COLOR_BLUE);
    screen_text(0u, "XUZREAD C64U RANDOM ACCESS");
    screen_text(2u, passed ? "XUZREAD FINISHED PASS" : "XUZREAD FINISHED FAIL");
    tui_puts(0u, 5u, "ZIP PARSER + REU CATALOG", passed ?
             TUI_COLOR_LIGHTGREEN : TUI_COLOR_LIGHTRED);
    tui_puts(0u, 22u, "RUN/STOP: LAUNCHER", TUI_COLOR_CYAN);
}

void xuzread_diag_run(unsigned char package_bank) {
    UzU32 archive_size;
    unsigned char work_bank;
    unsigned char catalog_bank;
    unsigned char saved_cpu_port;
    unsigned char failure;
    unsigned char stage;
    unsigned char key;

    memset((void *)XUZR_RESULT, 0, 96u);
    XUZR_RESULT[0] = 0x58u; /* XZP1 */
    XUZR_RESULT[1] = 0x5Au;
    XUZR_RESULT[2] = 0x50u;
    XUZR_RESULT[3] = 0x31u;
    XUZR_RESULT[4] = 1u;
    XUZR_RESULT[8] = package_bank;
    XUZR_RESULT[9] = XUZR_NONE;
    XUZR_RESULT[10] = XUZR_NONE;
    XUZR_RESULT[15] = XUZREAD_ENTRY_COUNT;
    XUZR_RESULT[44] = XUZREAD_COOKIE_0;
    XUZR_RESULT[45] = XUZREAD_COOKIE_1;
    XUZR_RESULT[46] = XUZREAD_COOKIE_2;
    XUZR_RESULT[47] = XUZREAD_COOKIE_3;
    stage = XUZR_STAGE_READY;
    XUZR_RESULT[6] = stage;
    tui_clear(TUI_COLOR_BLUE);
    screen_text(0u, "XUZREAD C64U RANDOM ACCESS");
    screen_text(2u, "PRESS SPACE TO START");
    while (tui_getkey() != ' ') { }

    work_bank = reu_alloc_owned_bank(XUZR_WORK_SLOT, "uzrw");
    catalog_bank = reu_alloc_owned_bank(XUZR_CATALOG_SLOT, "uzrc");
    XUZR_RESULT[9] = work_bank;
    XUZR_RESULT[10] = catalog_bank;
    XUZR_RESULT[56] = (unsigned char)sizeof(UzDos);
    XUZR_RESULT[57] = (unsigned char)sizeof(UzZipReader);
    XUZR_RESULT[58] = (unsigned char)sizeof(UzZipRecord);
    stage = XUZR_STAGE_BANKS;
    failure = 0u;
    if (package_bank == XUZR_NONE || work_bank == XUZR_NONE ||
        catalog_bank == XUZR_NONE || package_bank == work_bank ||
        package_bank == catalog_bank || work_bank == catalog_bank ||
        bank_type(package_bank) != REU_APP_ALLOC ||
        bank_type(work_bank) != REU_APP_ALLOC ||
        bank_type(catalog_bank) != REU_APP_ALLOC) {
        failure = 0x21u;
        goto finished;
    }

    saved_cpu_port = XUZR_CPU_PORT;
    XUZR_CPU_PORT = (unsigned char)(saved_cpu_port & (unsigned char)~XUZR_LORAM);
    load_store(package_bank);
    uz_dos_init(XUZR_DOS, UZ_DOS_TARGET_READ,
                XUZR_COMMAND, XUZR_DOS_CAP, XUZR_DATA, XUZR_DATA_CAP,
                XUZR_STATUS, XUZR_STATUS_CAP);
    result_u16(16u, uz_uci_base());
    stage = XUZR_STAGE_OWNER;
    if (!uz_dos_identify(XUZR_DOS) || !owner_valid()) {
        failure = 0x31u;
        goto io_finished;
    }
    stage = XUZR_STAGE_OPEN;
    if (!uz_dos_change_absolute(XUZR_DOS, XUZREAD_OWNED_ROOT) ||
        !uz_dos_open(XUZR_DOS, XUZREAD_ARCHIVE_NAME, UZ_DOS_OPEN_READ) ||
        !uz_dos_file_info(XUZR_DOS, &archive_size) ||
        archive_size.lo != XUZREAD_ARCHIVE_LO ||
        archive_size.hi != XUZREAD_ARCHIVE_HI) {
        failure = 0x41u;
        goto io_finished;
    }
    result_u32(28u, &archive_size);
    stage = XUZR_STAGE_PARSE;
    if (!parse_archive(package_bank, catalog_bank, &archive_size)) {
        XUZR_RESULT[13] = XUZR_READER->error;
        failure = 0x51u;
        goto io_finished;
    }
    stage = XUZR_STAGE_CATALOG;
    failure = 0u;

io_finished:
    load_store(package_bank);
    if (!uz_dos_close(XUZR_DOS) && failure == 0u) failure = 0x61u;
    XUZR_RESULT[18] = XUZR_DOS->transfer.flags;
    XUZR_RESULT[19] = XUZR_DOS->transfer.last_status;
    XUZR_CPU_PORT = saved_cpu_port;
    XUZR_RESULT[43] = (unsigned char)(XUZR_CPU_PORT == saved_cpu_port);

finished:
    if (work_bank != XUZR_NONE) {
        reu_free_owned_bank(work_bank);
        XUZR_RESULT[11] = 1u;
    }
    if (catalog_bank != XUZR_NONE) {
        reu_free_owned_bank(catalog_bank);
        XUZR_RESULT[12] = 1u;
    }
    stage = failure == 0u ? XUZR_STAGE_DONE : stage;
    XUZR_RESULT[5] = 1u;
    XUZR_RESULT[6] = stage;
    XUZR_RESULT[7] = failure;
    XUZR_RESULT[13] = XUZR_READER->error;
    XUZR_RESULT[14] = XUZR_READER->entry_index;
    XUZR_RESULT[41] = uz_package_version();
    XUZR_RESULT[42] = uz_package_phase_count();
    finish_screen((unsigned char)(failure == 0u));
    for (;;) {
        key = tui_getkey();
        if (key == TUI_KEY_RUNSTOP || key == 2u) tui_return_to_launcher();
    }
}

#pragma bss-name(pop)
#pragma rodata-name(pop)
#pragma code-name(pop)
