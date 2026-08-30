/* Physical UltimateDOS extraction transaction probe. It preflights both ZIPs
 * before creating any destination and then exercises directory, Store,
 * Deflate, existing-final, and bad-CRC cleanup paths through production code. */

#include "xuzextract_diag.h"

#include "xuzextract_config.h"
#include "uz_dos.h"
#include "uz_extract_fs.h"
#include "uz_pack.h"
#include "uz_package.h"
#include "uz_zip_read.h"

#include "../../lib/reu_control_bank.h"
#include "../../lib/reu_mgr.h"
#include "../../lib/reu_owned_alloc.h"
#include "../../lib/tui.h"

#include <string.h>

#define XUZE_RESULT ((volatile unsigned char *)0x033Cu)
#define XUZE_P_DOS ((UzDos *)0x0400u)
#define XUZE_P_COMMAND ((unsigned char *)0x0460u)
#define XUZE_P_DATA ((unsigned char *)0x0500u)
#define XUZE_P_STATUS ((unsigned char *)0x0540u)
#define XUZE_READER ((UzZipReader *)0x0580u)
#define XUZE_RECORD ((UzZipRecord *)0x05C0u)
#define XUZE_SCRATCH ((unsigned char *)0x0700u)
#define XUZE_ENTRY ((XuzeCatalogEntry *)0x0900u)

#define XUZE_COMMAND_CAP 160u
#define XUZE_DATA_CAP 64u
#define XUZE_STATUS_CAP 64u
#define XUZE_SCRATCH_CAP 512u
#define XUZE_NONE 0xFFu
#define XUZE_WORK_SLOT 2u
#define XUZE_CATALOG_SLOT 3u
#define XUZE_REU_READ_OFFSET 0xA000u
#define XUZE_BAD_CATALOG_OFFSET 0x1000u
#define XUZE_UI_START 0x3000u
#define XUZE_UI_LENGTH 0x9400u
#define XUZE_CPU_PORT (*(volatile unsigned char *)0x0001u)
#define XUZE_LORAM 0x01u

#define XUZE_STAGE_READY 1u
#define XUZE_STAGE_BANKS 2u
#define XUZE_STAGE_OWNER 3u
#define XUZE_STAGE_GOOD_PREFLIGHT 4u
#define XUZE_STAGE_BAD_PREFLIGHT 5u
#define XUZE_STAGE_EXTRACT 6u
#define XUZE_STAGE_CONFLICT 7u
#define XUZE_STAGE_BAD_CRC 8u
#define XUZE_STAGE_DONE 9u

typedef struct {
    UzZipRecord record;
    UzU32 data_offset;
} XuzeCatalogEntry;

#pragma code-name(push, "UI_CODE")
#pragma rodata-name(push, "UI_RODATA")
#pragma bss-name(push, "UI_BSS")

static const unsigned int good_size_lo[XUZE_GOOD_COUNT] = {
    XUZE_GOOD_SIZE_LO_0, XUZE_GOOD_SIZE_LO_1, XUZE_GOOD_SIZE_LO_2,
    XUZE_GOOD_SIZE_LO_3, XUZE_GOOD_SIZE_LO_4
};
static const unsigned int good_size_hi[XUZE_GOOD_COUNT] = {
    XUZE_GOOD_SIZE_HI_0, XUZE_GOOD_SIZE_HI_1, XUZE_GOOD_SIZE_HI_2,
    XUZE_GOOD_SIZE_HI_3, XUZE_GOOD_SIZE_HI_4
};
static const unsigned int good_compressed_lo[XUZE_GOOD_COUNT] = {
    XUZE_GOOD_COMPRESSED_LO_0, XUZE_GOOD_COMPRESSED_LO_1,
    XUZE_GOOD_COMPRESSED_LO_2, XUZE_GOOD_COMPRESSED_LO_3,
    XUZE_GOOD_COMPRESSED_LO_4
};
static const unsigned int good_compressed_hi[XUZE_GOOD_COUNT] = {
    XUZE_GOOD_COMPRESSED_HI_0, XUZE_GOOD_COMPRESSED_HI_1,
    XUZE_GOOD_COMPRESSED_HI_2, XUZE_GOOD_COMPRESSED_HI_3,
    XUZE_GOOD_COMPRESSED_HI_4
};
static const unsigned int good_offset_lo[XUZE_GOOD_COUNT] = {
    XUZE_GOOD_OFFSET_LO_0, XUZE_GOOD_OFFSET_LO_1, XUZE_GOOD_OFFSET_LO_2,
    XUZE_GOOD_OFFSET_LO_3, XUZE_GOOD_OFFSET_LO_4
};
static const unsigned int good_offset_hi[XUZE_GOOD_COUNT] = {
    XUZE_GOOD_OFFSET_HI_0, XUZE_GOOD_OFFSET_HI_1, XUZE_GOOD_OFFSET_HI_2,
    XUZE_GOOD_OFFSET_HI_3, XUZE_GOOD_OFFSET_HI_4
};
static const unsigned char good_method[XUZE_GOOD_COUNT] = {
    XUZE_GOOD_METHOD_0, XUZE_GOOD_METHOD_1, XUZE_GOOD_METHOD_2,
    XUZE_GOOD_METHOD_3, XUZE_GOOD_METHOD_4
};
static const unsigned char good_directory[XUZE_GOOD_COUNT] = {
    XUZE_GOOD_DIRECTORY_0, XUZE_GOOD_DIRECTORY_1, XUZE_GOOD_DIRECTORY_2,
    XUZE_GOOD_DIRECTORY_3, XUZE_GOOD_DIRECTORY_4
};
static const unsigned char good_crc[XUZE_GOOD_COUNT][4] = {
    {XUZE_GOOD_CRC_0_0, XUZE_GOOD_CRC_0_1, XUZE_GOOD_CRC_0_2, XUZE_GOOD_CRC_0_3},
    {XUZE_GOOD_CRC_1_0, XUZE_GOOD_CRC_1_1, XUZE_GOOD_CRC_1_2, XUZE_GOOD_CRC_1_3},
    {XUZE_GOOD_CRC_2_0, XUZE_GOOD_CRC_2_1, XUZE_GOOD_CRC_2_2, XUZE_GOOD_CRC_2_3},
    {XUZE_GOOD_CRC_3_0, XUZE_GOOD_CRC_3_1, XUZE_GOOD_CRC_3_2, XUZE_GOOD_CRC_3_3},
    {XUZE_GOOD_CRC_4_0, XUZE_GOOD_CRC_4_1, XUZE_GOOD_CRC_4_2, XUZE_GOOD_CRC_4_3}
};
static const unsigned char good_name_0[] = { XUZE_GOOD_NAME_BYTES_0 };
static const unsigned char good_name_1[] = { XUZE_GOOD_NAME_BYTES_1 };
static const unsigned char good_name_2[] = { XUZE_GOOD_NAME_BYTES_2 };
static const unsigned char good_name_3[] = { XUZE_GOOD_NAME_BYTES_3 };
static const unsigned char good_name_4[] = { XUZE_GOOD_NAME_BYTES_4 };
static const unsigned char bad_name_0[] = { XUZE_BAD_NAME_BYTES_0 };

/* Numeric bytes are intentional. cc65 translates characters in C string
 * literals to the target execution set; an ownership marker is host-created
 * ASCII and must be compared byte-for-byte without that translation. */
static const unsigned char owner_bytes[XUZE_OWNER_LENGTH] = {
    XUZE_OWNER_BYTES
};

/* These objects deliberately live in the UI snapshot window. The parser uses
 * fixed low-memory state while its $B000 overlay is active; extraction then
 * initializes these objects after the parser/UI snapshot has been restored. */
static UzDos archive_dos;
static UzDos output_dos;
static UzExtractFs extract_state;
static unsigned char archive_command[XUZE_COMMAND_CAP];
static unsigned char archive_data[XUZE_DATA_CAP];
static unsigned char archive_status[XUZE_STATUS_CAP];
static unsigned char output_command[XUZE_COMMAND_CAP];
static unsigned char output_data[XUZE_DATA_CAP];
static unsigned char output_status[XUZE_STATUS_CAP];

static const char *good_name(unsigned char index) {
    switch (index) {
        case 0u: return (const char *)good_name_0;
        case 1u: return (const char *)good_name_1;
        case 2u: return (const char *)good_name_2;
        case 3u: return (const char *)good_name_3;
        default: return (const char *)good_name_4;
    }
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
    XUZE_RESULT[offset] = (unsigned char)value;
    XUZE_RESULT[(unsigned char)(offset + 1u)] = (unsigned char)(value >> 8u);
}

static void result_u32(unsigned char offset, const UzU32 *value) {
    result_u16(offset, value->lo);
    result_u16((unsigned char)(offset + 2u), value->hi);
}

static void add_callback_bytes(unsigned int length) {
    unsigned int low;
    unsigned int high;

    low = (unsigned int)(XUZE_RESULT[22] |
                         ((unsigned int)XUZE_RESULT[23] << 8u));
    high = (unsigned int)(XUZE_RESULT[24] |
                          ((unsigned int)XUZE_RESULT[25] << 8u));
    low = (unsigned int)(low + length);
    if (low < length) ++high;
    result_u16(22u, low);
    result_u16(24u, high);
}

static unsigned char bank_type(unsigned char bank) {
    if (bank == XUZE_NONE) return REU_UNAVAIL;
    return readyos_bank_read_byte((unsigned int)(REUCB_BANK_TYPE_OFF + bank));
}

static void snapshot_ui(unsigned char work_bank) {
    reu_dma_stash(XUZE_UI_START, work_bank, 0u, XUZE_UI_LENGTH);
}

static void restore_ui(unsigned char work_bank) {
    reu_dma_fetch(XUZE_UI_START, work_bank, 0u, XUZE_UI_LENGTH);
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

static unsigned char read_at(void *context, const UzU32 *offset,
                             unsigned char *destination,
                             unsigned int length) {
    unsigned int transferred;
    unsigned int calls;
    unsigned char work_bank;

    (void)context;
    work_bank = XUZE_RESULT[9];
    if (length == 0u || length > XUZE_SCRATCH_CAP) return 0u;
    transferred = 0u;
    /* The shared UCI transport owns idle synchronization, asynchronous PUSH
     * and ABORT, complete queue drains, DATA_ACC transitions, and final quiet
     * idle. This parser callback adds no timing or instruction-delay pacing. */
    if (!uz_dos_seek(XUZE_P_DOS, offset) ||
        !uz_dos_load_reu(XUZE_P_DOS, work_bank, XUZE_REU_READ_OFFSET,
                         length, &transferred) || transferred != length)
        return 0u;
    reu_dma_fetch((unsigned int)destination, work_bank,
                  XUZE_REU_READ_OFFSET, length);
    calls = (unsigned int)(XUZE_RESULT[20] |
                           ((unsigned int)XUZE_RESULT[21] << 8u));
    result_u16(20u, (unsigned int)(calls + 1u));
    add_callback_bytes(length);
    if (length > (unsigned int)(XUZE_RESULT[26] |
            ((unsigned int)XUZE_RESULT[27] << 8u))) result_u16(26u, length);
    return 1u;
}

static unsigned char good_record_matches(unsigned char index,
                                         const UzZipRecord *record) {
    return (unsigned char)(index < XUZE_GOOD_COUNT &&
        strcmp(record->name, good_name(index)) == 0 &&
        record->method == good_method[index] &&
        record->directory == good_directory[index] &&
        record->size.lo == good_size_lo[index] &&
        record->size.hi == good_size_hi[index] &&
        record->compressed_size.lo == good_compressed_lo[index] &&
        record->compressed_size.hi == good_compressed_hi[index] &&
        record->local_offset.lo == good_offset_lo[index] &&
        record->local_offset.hi == good_offset_hi[index] &&
        memcmp(record->crc.byte, good_crc[index], 4u) == 0);
}

static unsigned char bad_record_matches(const UzZipRecord *record) {
    static const unsigned char crc[4] = {
        XUZE_BAD_CRC_0_0, XUZE_BAD_CRC_0_1,
        XUZE_BAD_CRC_0_2, XUZE_BAD_CRC_0_3
    };
    return (unsigned char)(
        strcmp(record->name, (const char *)bad_name_0) == 0 &&
        record->method == XUZE_BAD_METHOD_0 &&
        record->directory == XUZE_BAD_DIRECTORY_0 &&
        record->size.lo == XUZE_BAD_SIZE_LO_0 &&
        record->size.hi == XUZE_BAD_SIZE_HI_0 &&
        record->compressed_size.lo == XUZE_BAD_COMPRESSED_LO_0 &&
        record->compressed_size.hi == XUZE_BAD_COMPRESSED_HI_0 &&
        record->local_offset.lo == XUZE_BAD_OFFSET_LO_0 &&
        record->local_offset.hi == XUZE_BAD_OFFSET_HI_0 &&
        memcmp(record->crc.byte, crc, 4u) == 0);
}

static unsigned char owner_valid(void) {
    unsigned int index;
    int got;

    if (!uz_dos_change_absolute(XUZE_P_DOS, XUZE_OWNED_ROOT) ||
        !uz_dos_open(XUZE_P_DOS, ".readyos-uzip-owner", UZ_DOS_OPEN_READ))
        return 0u;
    got = uz_dos_read(XUZE_P_DOS, XUZE_P_DATA, XUZE_OWNER_LENGTH);
    XUZE_RESULT[91] = (unsigned char)got;
    XUZE_RESULT[95] = (unsigned char)XUZE_P_DOS->transfer.data_len;
    for (index = 0u; index < 8u; ++index)
        XUZE_RESULT[(unsigned char)(96u + index)] = XUZE_P_DATA[index];
    if (!uz_dos_close(XUZE_P_DOS) || got < 0 ||
        (unsigned int)got != XUZE_OWNER_LENGTH) return 0u;
    XUZE_RESULT[94] = 0xFFu;
    for (index = 0u; index < XUZE_OWNER_LENGTH; ++index)
        if (XUZE_P_DATA[index] != owner_bytes[index]) {
            XUZE_RESULT[94] = (unsigned char)index;
            return 0u;
        }
    return 1u;
}

static unsigned char preflight(unsigned char package_bank,
                               unsigned char work_bank,
                               unsigned char catalog_bank,
                               const char *archive_name,
                               unsigned char bad_archive) {
    UzU32 archive_size;
    unsigned int count;
    unsigned int index;
    unsigned int catalog_offset;
    unsigned int reader_offset;
    unsigned char result;
    unsigned char evidence;

    evidence = bad_archive ? 110u : 104u;
    result = 0u;
    XUZE_RESULT[evidence] = 1u;
    /* Ownership validation leaves target 1 at the marker-owned root. The
     * promoted xuzread probe already covers close-then-absolute reposition;
     * keep this transaction focused on parser/extraction semantics. */
    XUZE_RESULT[evidence] = 2u;
    if (!uz_dos_open(XUZE_P_DOS, archive_name, UZ_DOS_OPEN_READ))
        goto preflight_early_failure;
    XUZE_RESULT[evidence] = 3u;
    if (!uz_dos_file_info(XUZE_P_DOS, &archive_size))
        goto preflight_early_failure;
    XUZE_RESULT[evidence] = 4u;
    result_u32(bad_archive ? 40u : 28u, &archive_size);
    if ((!bad_archive &&
         (archive_size.lo != XUZE_GOOD_ARCHIVE_LO ||
          archive_size.hi != XUZE_GOOD_ARCHIVE_HI)) ||
        (bad_archive &&
         (archive_size.lo != XUZE_BAD_ARCHIVE_LO ||
          archive_size.hi != XUZE_BAD_ARCHIVE_HI))) {
        goto preflight_early_failure;
    }
    XUZE_RESULT[evidence] = 5u;

    /* The compact-package probe records both sides of the reader handoff.
     * Byte 13 identifies the last completed operation: package-bank peek,
     * overlay fetch, reader init, or parser begin.  This evidence survives a
     * crash because the result block is outside the UI snapshot window. */
    if (!bad_archive) {
        reader_offset = uz_package_phase_offset(UZ_PACKAGE_PHASE_READER);
        result_u16(126u, reader_offset);
        XUZE_RESULT[13] = 1u;
        reu_dma_fetch((unsigned int)XUZE_P_DATA, package_bank,
                      reader_offset, 2u);
        XUZE_RESULT[82] = XUZE_P_DATA[0];
        XUZE_RESULT[83] = XUZE_P_DATA[1];
        XUZE_RESULT[13] = 2u;
    }
    snapshot_ui(work_bank);
    load_reader(package_bank);
    if (!bad_archive) {
        XUZE_RESULT[124] = *(volatile unsigned char *)0xB000u;
        XUZE_RESULT[125] = *(volatile unsigned char *)0xB001u;
        XUZE_RESULT[13] = 3u;
    }
    uz_zip_reader_init_at(XUZE_READER, &archive_size, read_at, 0);
    if (!bad_archive) XUZE_RESULT[13] = 4u;
    result = uz_zip_reader_begin(XUZE_READER, XUZE_SCRATCH,
                                 XUZE_SCRATCH_CAP);
    if (!bad_archive) XUZE_RESULT[13] = 5u;
    count = bad_archive ? 1u : XUZE_GOOD_COUNT;
    if (result && (XUZE_READER->entry_count != count ||
        XUZE_READER->central_offset.lo !=
            (bad_archive ? XUZE_BAD_CENTRAL_LO : XUZE_GOOD_CENTRAL_LO) ||
        XUZE_READER->central_offset.hi !=
            (bad_archive ? XUZE_BAD_CENTRAL_HI : XUZE_GOOD_CENTRAL_HI) ||
        XUZE_READER->central_size.lo !=
            (bad_archive ? XUZE_BAD_CENTRAL_SIZE_LO : XUZE_GOOD_CENTRAL_SIZE_LO) ||
        XUZE_READER->central_size.hi !=
            (bad_archive ? XUZE_BAD_CENTRAL_SIZE_HI : XUZE_GOOD_CENTRAL_SIZE_HI)))
        result = 0u;
    for (index = 0u; result && index < count; ++index) {
        if (!uz_zip_reader_next(XUZE_READER, XUZE_RECORD) ||
            (bad_archive ? !bad_record_matches(XUZE_RECORD) :
                           !good_record_matches((unsigned char)index, XUZE_RECORD)) ||
            !uz_zip_reader_local(XUZE_READER, XUZE_RECORD,
                                 &XUZE_ENTRY->data_offset,
                                 XUZE_SCRATCH, XUZE_SCRATCH_CAP)) {
            result = 0u;
            break;
        }
        memcpy(&XUZE_ENTRY->record, XUZE_RECORD, sizeof(UzZipRecord));
        catalog_offset = bad_archive ? XUZE_BAD_CATALOG_OFFSET :
            (unsigned int)(index * (unsigned int)sizeof(XuzeCatalogEntry));
        reu_dma_stash((unsigned int)XUZE_ENTRY, catalog_bank,
                      catalog_offset, sizeof(XuzeCatalogEntry));
        if (!bad_archive) {
            XUZE_RESULT[(unsigned char)(84u + index)] = XUZE_RECORD->method;
            if (XUZE_RECORD->directory)
                XUZE_RESULT[89] |= (unsigned char)(1u << index);
        }
    }
    if (result && !uz_zip_reader_finished(XUZE_READER)) result = 0u;
    if (!bad_archive) {
        result_u32(32u, &XUZE_READER->central_offset);
        result_u32(36u, &XUZE_READER->central_size);
        XUZE_RESULT[60] = XUZE_READER->error;
        XUZE_RESULT[76] = (unsigned char)XUZE_READER->entry_index;
    } else {
        result_u32(44u, &XUZE_READER->central_offset);
        result_u32(48u, &XUZE_READER->central_size);
        XUZE_RESULT[61] = XUZE_READER->error;
        XUZE_RESULT[77] = (unsigned char)XUZE_READER->entry_index;
    }
    if (!uz_dos_close(XUZE_P_DOS)) result = 0u;
    restore_ui(work_bank);
    if (result) XUZE_RESULT[evidence] = 7u;
    return result;

preflight_early_failure:
    XUZE_RESULT[(unsigned char)(evidence + 1u)] =
        XUZE_P_DOS->transfer.flags;
    XUZE_RESULT[(unsigned char)(evidence + 2u)] =
        XUZE_P_DOS->transfer.last_status;
    XUZE_RESULT[(unsigned char)(evidence + 3u)] =
        (unsigned char)XUZE_P_DOS->transfer.data_len;
    XUZE_RESULT[(unsigned char)(evidence + 4u)] = XUZE_P_DOS->file_open;
    XUZE_RESULT[115] = (unsigned char)XUZE_P_DOS->transfer.stat_len;
    for (index = 0u; index < 8u; ++index) {
        XUZE_RESULT[(unsigned char)(116u + index)] =
            index < XUZE_P_DOS->transfer.stat_len ? XUZE_P_STATUS[index] : 0u;
    }
    if (XUZE_P_DOS->file_open) (void)uz_dos_close(XUZE_P_DOS);
    return 0u;
}

static unsigned char open_archive(UzDos *dos, const char *name) {
    return uz_dos_open(dos, name, UZ_DOS_OPEN_READ);
}

static unsigned char extract_good(unsigned char package_bank,
                                  unsigned char work_bank,
                                  unsigned char catalog_bank) {
    unsigned int index;
    unsigned int catalog_offset;
    unsigned char ok;

    if (!open_archive(&archive_dos, XUZE_GOOD_ARCHIVE)) return 0u;
    for (index = 0u; index < XUZE_GOOD_COUNT; ++index) {
        catalog_offset = (unsigned int)(index * (unsigned int)sizeof(XuzeCatalogEntry));
        reu_dma_fetch((unsigned int)XUZE_ENTRY, catalog_bank,
                      catalog_offset, sizeof(XuzeCatalogEntry));
        uz_extract_fs_init(&extract_state);
        ok = uz_extract_member(&extract_state, &archive_dos, &output_dos,
                               XUZE_DEST_ROOT, &XUZE_ENTRY->record,
                               &XUZE_ENTRY->data_offset,
                               package_bank, work_bank);
        if (index < 4u) {
            if (!ok || extract_state.error != UZ_EXTRACT_OK) return 0u;
            ++XUZE_RESULT[69];
            if (index < 2u) ++XUZE_RESULT[54];
            else if (index == 2u) XUZE_RESULT[55] = 1u;
            else XUZE_RESULT[56] = 1u;
        } else {
            XUZE_RESULT[70] = extract_state.error;
            XUZE_RESULT[71] = extract_state.job_error;
            XUZE_RESULT[72] = (unsigned char)(extract_state.temp_open |
                (unsigned char)(extract_state.temp_created << 1u));
            if (ok || extract_state.error != UZ_EXTRACT_COMMIT ||
                extract_state.job_error != UZ_STORE_JOB_OK ||
                XUZE_RESULT[72] != 0u || output_dos.file_open) return 0u;
            XUZE_RESULT[57] = 1u;
        }
    }
    return uz_dos_close(&archive_dos);
}

static unsigned char extract_bad_crc(unsigned char package_bank,
                                     unsigned char work_bank,
                                     unsigned char catalog_bank) {
    unsigned char ok;

    if (!open_archive(&archive_dos, XUZE_BAD_ARCHIVE)) return 0u;
    reu_dma_fetch((unsigned int)XUZE_ENTRY, catalog_bank,
                  XUZE_BAD_CATALOG_OFFSET, sizeof(XuzeCatalogEntry));
    uz_extract_fs_init(&extract_state);
    ok = uz_extract_member(&extract_state, &archive_dos, &output_dos,
                           XUZE_DEST_ROOT, &XUZE_ENTRY->record,
                           &XUZE_ENTRY->data_offset,
                           package_bank, work_bank);
    XUZE_RESULT[73] = extract_state.error;
    XUZE_RESULT[74] = extract_state.job_error;
    XUZE_RESULT[75] = (unsigned char)(extract_state.temp_open |
        (unsigned char)(extract_state.temp_created << 1u));
    if (ok || extract_state.error != UZ_EXTRACT_JOB ||
        extract_state.job_error != UZ_INFLATE_JOB_CRC ||
        XUZE_RESULT[75] != 0u || output_dos.file_open) return 0u;
    XUZE_RESULT[58] = 1u;
    return uz_dos_close(&archive_dos);
}

static void finish_screen(unsigned char passed) {
    tui_clear(TUI_COLOR_BLUE);
    screen_text(0u, "XUZEXTRACT C64U TRANSACTION");
    screen_text(2u, passed ? "XUZEXTRACT FINISHED PASS" :
                             "XUZEXTRACT FINISHED FAIL");
    tui_puts(0u, 5u, "STORE + DEFLATE + CLEANUP", passed ?
             TUI_COLOR_LIGHTGREEN : TUI_COLOR_LIGHTRED);
    tui_puts(0u, 22u, "RUN/STOP: LAUNCHER", TUI_COLOR_CYAN);
}

void xuzextract_diag_run(unsigned char package_bank) {
    unsigned char work_bank;
    unsigned char catalog_bank;
    unsigned char saved_cpu_port;
    unsigned char port_changed;
    unsigned char runtime_ready;
    unsigned char failure;
    unsigned char stage;
    unsigned char key;

    memset((void *)XUZE_RESULT, 0, 128u);
    XUZE_RESULT[0] = 0x58u; /* XZE1 */
    XUZE_RESULT[1] = 0x5Au;
    XUZE_RESULT[2] = 0x45u;
    XUZE_RESULT[3] = 0x31u;
    XUZE_RESULT[4] = 1u;
    XUZE_RESULT[8] = package_bank;
    XUZE_RESULT[9] = XUZE_NONE;
    XUZE_RESULT[10] = XUZE_NONE;
    XUZE_RESULT[15] = XUZE_GOOD_COUNT;
    XUZE_RESULT[62] = uz_package_version();
    XUZE_RESULT[63] = uz_package_phase_count();
    XUZE_RESULT[64] = XUZE_COOKIE_0;
    XUZE_RESULT[65] = XUZE_COOKIE_1;
    XUZE_RESULT[66] = XUZE_COOKIE_2;
    XUZE_RESULT[67] = XUZE_COOKIE_3;
    XUZE_RESULT[68] = 4u;
    XUZE_RESULT[78] = (unsigned char)sizeof(UzDos);
    XUZE_RESULT[79] = (unsigned char)sizeof(UzZipReader);
    XUZE_RESULT[80] = (unsigned char)sizeof(UzZipRecord);
    XUZE_RESULT[81] = (unsigned char)sizeof(UzExtractFs);
    work_bank = XUZE_NONE;
    catalog_bank = XUZE_NONE;
    failure = 0u;
    port_changed = 0u;
    runtime_ready = 0u;
    stage = XUZE_STAGE_READY;
    XUZE_RESULT[6] = stage;
    tui_clear(TUI_COLOR_BLUE);
    screen_text(0u, "XUZEXTRACT C64U TRANSACTION");
    screen_text(2u, "PRESS SPACE TO START");
    while (tui_getkey() != ' ') { }

    work_bank = reu_alloc_owned_bank(XUZE_WORK_SLOT, "uzew");
    catalog_bank = reu_alloc_owned_bank(XUZE_CATALOG_SLOT, "uzec");
    XUZE_RESULT[9] = work_bank;
    XUZE_RESULT[10] = catalog_bank;
    stage = XUZE_STAGE_BANKS;
    if (package_bank == XUZE_NONE || work_bank == XUZE_NONE ||
        catalog_bank == XUZE_NONE || package_bank == work_bank ||
        package_bank == catalog_bank || work_bank == catalog_bank ||
        /* Launcher-owned resources deliberately retain their specific type;
         * only temporary work/catalog banks use generic app allocation. */
        bank_type(package_bank) != REU_UZIP_PACKAGE ||
        bank_type(work_bank) != REU_APP_ALLOC ||
        bank_type(catalog_bank) != REU_APP_ALLOC) {
        failure = 0x21u;
        goto finished;
    }

    saved_cpu_port = XUZE_CPU_PORT;
    XUZE_CPU_PORT = (unsigned char)(saved_cpu_port & (unsigned char)~XUZE_LORAM);
    port_changed = 1u;
    /* Keep one identified target object across owner validation and both
     * archive preflights.  The physically promoted xuzread probe uses this
     * lifecycle; reinitializing the local target between CLOSE and the next
     * CHANGE_DIR caused firmware to reject the otherwise identical path. */
    load_store(package_bank);
    uz_dos_init(XUZE_P_DOS, UZ_DOS_TARGET_READ,
                XUZE_P_COMMAND, XUZE_COMMAND_CAP,
                XUZE_P_DATA, XUZE_DATA_CAP,
                XUZE_P_STATUS, XUZE_STATUS_CAP);
    result_u16(16u, uz_uci_base());
    stage = XUZE_STAGE_OWNER;
    if (!uz_dos_identify(XUZE_P_DOS) || !owner_valid()) {
        failure = 0x31u;
        goto io_finished;
    }
    /* Record the successful validation after returning to resident control.
     * A final volatile store inside owner_valid() was absent in the physical
     * result even though its true return and all byte evidence were present. */
    XUZE_RESULT[90] = 7u;
    stage = XUZE_STAGE_GOOD_PREFLIGHT;
    if (!preflight(package_bank, work_bank, catalog_bank,
                   XUZE_GOOD_ARCHIVE, 0u)) {
        failure = 0x41u;
        goto io_finished;
    }
    XUZE_RESULT[52] = 1u;
    stage = XUZE_STAGE_BAD_PREFLIGHT;
    if (!preflight(package_bank, work_bank, catalog_bank,
                   XUZE_BAD_ARCHIVE, 1u)) {
        failure = 0x51u;
        goto io_finished;
    }
    XUZE_RESULT[53] = 1u;

    /* Parser overlays have been retired and the last UI snapshot restored.
     * Initialize persistent extraction handles only now. */
    uz_dos_init(&archive_dos, UZ_DOS_TARGET_READ,
                archive_command, sizeof(archive_command),
                archive_data, sizeof(archive_data),
                archive_status, sizeof(archive_status));
    uz_dos_init(&output_dos, UZ_DOS_TARGET_WRITE,
                output_command, sizeof(output_command),
                output_data, sizeof(output_data),
                output_status, sizeof(output_status));
    uz_extract_fs_init(&extract_state);
    runtime_ready = 1u;
    if (!uz_dos_identify(&archive_dos) || !uz_dos_identify(&output_dos)) {
        failure = 0x61u;
        goto io_finished;
    }
    stage = XUZE_STAGE_EXTRACT;
    if (!extract_good(package_bank, work_bank, catalog_bank)) {
        failure = 0x71u;
        goto io_finished;
    }
    stage = XUZE_STAGE_CONFLICT;
    if (!XUZE_RESULT[57]) {
        failure = 0x75u;
        goto io_finished;
    }
    stage = XUZE_STAGE_BAD_CRC;
    if (!extract_bad_crc(package_bank, work_bank, catalog_bank)) {
        failure = 0x81u;
        goto io_finished;
    }

io_finished:
    if (runtime_ready && archive_dos.file_open &&
        !uz_dos_close(&archive_dos) && failure == 0u)
        failure = 0x91u;
    if (runtime_ready && output_dos.file_open &&
        !uz_dos_close(&output_dos) && failure == 0u)
        failure = 0x92u;
    if (runtime_ready) {
        XUZE_RESULT[18] = archive_dos.transfer.flags;
        XUZE_RESULT[19] = archive_dos.transfer.last_status;
    }
    if (port_changed) {
        XUZE_CPU_PORT = saved_cpu_port;
        XUZE_RESULT[59] = (unsigned char)(XUZE_CPU_PORT == saved_cpu_port);
    }

finished:
    if (work_bank != XUZE_NONE) {
        reu_free_owned_bank(work_bank);
        XUZE_RESULT[11] = 1u;
    }
    if (catalog_bank != XUZE_NONE) {
        reu_free_owned_bank(catalog_bank);
        XUZE_RESULT[12] = 1u;
    }
    stage = failure == 0u ? XUZE_STAGE_DONE : stage;
    XUZE_RESULT[5] = 1u;
    XUZE_RESULT[6] = stage;
    XUZE_RESULT[7] = failure;
    finish_screen((unsigned char)(failure == 0u));
    for (;;) {
        key = tui_getkey();
        if (key == TUI_KEY_RUNSTOP || key == 2u) tui_return_to_launcher();
    }
}

#pragma bss-name(pop)
#pragma rodata-name(pop)
#pragma code-name(pop)
