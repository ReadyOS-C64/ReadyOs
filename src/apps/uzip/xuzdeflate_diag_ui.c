/* Full-ReadyOS UI shell for the physical fixed-Deflate creator diagnostic. */

#if defined(UZIP_XUZZIP8_DIAGNOSTIC)
#include "xuzzip8_diag.h"
#elif defined(UZIP_XUZMULTI_DIAGNOSTIC)
#include "xuzmulti_diag.h"
#else
#include "xuzdeflate_diag.h"
#endif
#include "xuzdeflate_diag_internal.h"

#include "xuzdeflate_config.h"
#include "uz_job.h"
#include "uz_pack.h"
#include "uz_package.h"

#include "../../lib/reu_control_bank.h"
#include "../../lib/reu_mgr.h"
#include "../../lib/reu_owned_alloc.h"
#include "../../lib/tui.h"

#include <string.h>

#define XUZD_NONE 0xFFu
#define XUZD_SCRATCH_SLOT 2u
#define XUZD_CATALOG_SLOT 3u
#define XUZD_CASE_COUNT 4u
#ifdef UZIP_XUZMULTI_DIAGNOSTIC
#define XUZD_ENTRY_COUNT 7u
#endif

#if defined(UZIP_XUZZIP8_DIAGNOSTIC) || defined(UZIP_XUZMULTI_DIAGNOSTIC)
#define XUZD_ARCHIVE_DIAGNOSTIC 1
#endif

#if defined(UZIP_XUZZIP8_DIAGNOSTIC)
#define XUZD_DIAG_RUN xuzzip8_diag_run
#elif defined(UZIP_XUZMULTI_DIAGNOSTIC)
#define XUZD_DIAG_RUN xuzmulti_diag_run
#else
#define XUZD_DIAG_RUN xuzdeflate_diag_run
#endif

#pragma code-name(push, "UI_CODE")
#pragma rodata-name(push, "UI_RODATA")

static const char *input_name(unsigned char index) {
    if (index == 0u) return "empty.bin";
    if (index == 1u) return "repeat.bin";
    if (index == 2u) return "random.bin";
    return "cross.bin";
}

static const char *output_name(unsigned char index) {
#ifdef UZIP_XUZMULTI_DIAGNOSTIC
    (void)index;
    return "multi.zip";
#else
#ifdef XUZD_ARCHIVE_DIAGNOSTIC
    if (index == 0u) return "empty.zip";
    if (index == 1u) return "repeat.zip";
    if (index == 2u) return "random.zip";
    return "cross.zip";
#else
    if (index == 0u) return "empty.raw";
    if (index == 1u) return "repeat.raw";
    if (index == 2u) return "random.raw";
    return "cross.raw";
#endif
#endif
}

#ifdef UZIP_XUZMULTI_DIAGNOSTIC
static const char *multi_member_name(unsigned char index) {
    if (index == 0u) return "root/";
    if (index == 1u) return "root/empty.bin";
    if (index == 2u) return "root/repeat.bin";
    if (index == 3u) return "root/sub/";
    if (index == 4u) return "root/sub/random.bin";
    if (index == 5u) return "root/repeat.sto";
    return "cross.bin";
}

static const char *multi_input_name(unsigned char index) {
    if (index == 1u) return "empty.bin";
    if (index == 2u || index == 5u) return "repeat.bin";
    if (index == 4u) return "random.bin";
    return "cross.bin";
}

static unsigned char multi_case_index(unsigned char index) {
    if (index == 1u) return 0u;
    if (index == 2u) return 1u;
    if (index == 4u) return 2u;
    if (index == 6u) return 3u;
    return XUZD_NONE;
}

static unsigned char multi_directory(unsigned char index) {
    return (unsigned char)(index == 0u || index == 3u);
}

static unsigned char multi_method(unsigned char index) {
    return (unsigned char)((index == 0u || index == 3u || index == 5u) ?
                           0u : 8u);
}
#endif

static unsigned int expected_size(unsigned char index) {
    if (index == 0u) return XUZD_EMPTY_SIZE;
    if (index == 1u) return XUZD_REPEAT_SIZE;
    if (index == 2u) return XUZD_RANDOM_SIZE;
    return XUZD_CROSS_SIZE;
}

static unsigned char crc_matches(unsigned char index, const UzCrc32 *crc) {
    if (index == 0u)
        return (unsigned char)(crc->byte[0] == XUZD_EMPTY_CRC0 &&
            crc->byte[1] == XUZD_EMPTY_CRC1 &&
            crc->byte[2] == XUZD_EMPTY_CRC2 &&
            crc->byte[3] == XUZD_EMPTY_CRC3);
    if (index == 1u)
        return (unsigned char)(crc->byte[0] == XUZD_REPEAT_CRC0 &&
            crc->byte[1] == XUZD_REPEAT_CRC1 &&
            crc->byte[2] == XUZD_REPEAT_CRC2 &&
            crc->byte[3] == XUZD_REPEAT_CRC3);
    if (index == 2u)
        return (unsigned char)(crc->byte[0] == XUZD_RANDOM_CRC0 &&
            crc->byte[1] == XUZD_RANDOM_CRC1 &&
            crc->byte[2] == XUZD_RANDOM_CRC2 &&
            crc->byte[3] == XUZD_RANDOM_CRC3);
    return (unsigned char)(crc->byte[0] == XUZD_CROSS_CRC0 &&
        crc->byte[1] == XUZD_CROSS_CRC1 &&
        crc->byte[2] == XUZD_CROSS_CRC2 &&
        crc->byte[3] == XUZD_CROSS_CRC3);
}

static unsigned char bank_type(unsigned char bank) {
    if (bank == XUZD_NONE) return REU_UNAVAIL;
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
        screen[column] = (*text != 0) ?
            screen_code((unsigned char)*text++) : 0x20u;
}

static void stamp(volatile unsigned char *destination) {
    destination[0] = *(volatile unsigned char *)0x00A0u;
    destination[1] = *(volatile unsigned char *)0x00A1u;
    destination[2] = *(volatile unsigned char *)0x00A2u;
}

static void result_u16(unsigned char offset, unsigned int value) {
    XUZD_RESULT[offset] = (unsigned char)value;
    XUZD_RESULT[(unsigned char)(offset + 1u)] = (unsigned char)(value >> 8u);
}

static void result_u32(unsigned char offset, const UzU32 *value) {
    XUZD_RESULT[offset] = (unsigned char)value->lo;
    XUZD_RESULT[(unsigned char)(offset + 1u)] = (unsigned char)(value->lo >> 8u);
    XUZD_RESULT[(unsigned char)(offset + 2u)] = (unsigned char)value->hi;
    XUZD_RESULT[(unsigned char)(offset + 3u)] = (unsigned char)(value->hi >> 8u);
}

static void write_result(unsigned char done, unsigned char stage,
                         unsigned char failure, unsigned char completed,
                         unsigned char active, unsigned char package_bank,
                         unsigned char work_bank) {
    XUZD_RESULT[0] = 0x58u; /* XZD1 */
    XUZD_RESULT[1] = 0x5Au;
    XUZD_RESULT[2] = 0x44u;
    XUZD_RESULT[3] = 0x31u;
#ifdef XUZD_ARCHIVE_DIAGNOSTIC
#ifdef UZIP_XUZMULTI_DIAGNOSTIC
    XUZD_RESULT[4] = 3u; /* v3 is one seven-entry streamed archive. */
#else
    XUZD_RESULT[4] = 2u; /* v2 adds distinct uzct allocation/release proof. */
#endif
#else
    XUZD_RESULT[4] = 1u;
#endif
    XUZD_RESULT[5] = done;
    XUZD_RESULT[6] = stage;
    XUZD_RESULT[7] = failure;
    XUZD_RESULT[8] = completed;
    XUZD_RESULT[9] = active;
    result_u16(10u, uz_uci_base());
    XUZD_RESULT[12] = package_bank;
    XUZD_RESULT[13] = work_bank;
    XUZD_RESULT[14] = XUZD_INPUT_DOS->transfer.flags;
    XUZD_RESULT[15] = XUZD_INPUT_DOS->transfer.last_status;
    XUZD_RESULT[16] = XUZD_OUTPUT_DOS->transfer.flags;
    XUZD_RESULT[17] = XUZD_OUTPUT_DOS->transfer.last_status;
    XUZD_RESULT[18] = XUZD_DEFLATE->error;
    XUZD_RESULT[20] = XUZD_CASE_COUNT;
    XUZD_RESULT[21] = XUZD_WORKSPACE_GUARD;
    result_u16(112u, XUZD_CONTEXT->stack_initial);
    result_u16(114u, XUZD_CONTEXT->stack_low);
    XUZD_RESULT[116] = XUZD_CONTEXT->package_version;
    XUZD_RESULT[117] = XUZD_CONTEXT->package_phase_count;
    XUZD_RESULT[118] = XUZDEFLATE_COOKIE0;
    XUZD_RESULT[119] = XUZDEFLATE_COOKIE1;
    XUZD_RESULT[120] = XUZDEFLATE_COOKIE2;
    XUZD_RESULT[121] = XUZDEFLATE_COOKIE3;
    XUZD_RESULT[122] = XUZD_CONTEXT->phase_match_count;
    XUZD_RESULT[123] = XUZD_CONTEXT->phase_emit_count;
}

static void load_store(unsigned char package_bank) {
    reu_dma_fetch(uz_pack_job_run(), package_bank,
                  uz_package_phase_offset(UZ_PACKAGE_PHASE_JOB),
                  uz_pack_job_size());
}

static unsigned char verify_owner(void) {
    unsigned int index;
    int got;

    if (!uz_dos_change_absolute(XUZD_INPUT_DOS, XUZDEFLATE_OWNED_ROOT) ||
        !uz_dos_open(XUZD_INPUT_DOS, ".readyos-uzip-owner", UZ_DOS_OPEN_READ))
        return 0u;
    got = uz_dos_read(XUZD_INPUT_DOS, XUZD_INPUT_DATA,
                      XUZDEFLATE_OWNER_LENGTH);
    if (!uz_dos_close(XUZD_INPUT_DOS) || got < 0 ||
        (unsigned int)got != XUZDEFLATE_OWNER_LENGTH) return 0u;
    for (index = 0u; index < XUZDEFLATE_OWNER_LENGTH; ++index) {
        if (XUZD_INPUT_DATA[index] !=
            (unsigned char)XUZDEFLATE_OWNER_TEXT[index]) return 0u;
    }
    return 1u;
}

static unsigned char size_is(const UzU32 *size, unsigned int expected) {
    return (unsigned char)(size->hi == 0u && size->lo == expected);
}

static void record_case(unsigned char index) {
    unsigned char offset;

    offset = (unsigned char)(48u + 4u * index);
    result_u32(offset, &XUZD_DEFLATE->output_size);
    memcpy((void *)(XUZD_RESULT + 64u + 4u * index),
           XUZD_DEFLATE->crc.byte, 4u);
    result_u16((unsigned char)(80u + 2u * index),
               XUZD_DEFLATE->fixed_blocks);
    result_u16((unsigned char)(88u + 2u * index),
               XUZD_DEFLATE->stored_blocks);
    result_u32((unsigned char)(96u + 4u * index),
               &XUZD_DEFLATE->input_size);
}

static void finish_screen(const char *line, unsigned char color) {
    tui_clear(TUI_COLOR_BLUE);
#ifdef XUZD_ARCHIVE_DIAGNOSTIC
#ifdef UZIP_XUZMULTI_DIAGNOSTIC
    screen_text(0u, "XUZMULTI C64U");
#else
    screen_text(0u, "XUZZIP8 C64U");
#endif
#else
    screen_text(0u, "XUZDEFLATE C64U");
#endif
    screen_text(2u, line);
#ifdef XUZD_ARCHIVE_DIAGNOSTIC
#ifdef UZIP_XUZMULTI_DIAGNOSTIC
    tui_puts(0u, 5u, "MIXED NESTED ZIP", color);
#else
    tui_puts(0u, 5u, "STREAMED METHOD-8 ZIPS", color);
#endif
#else
    tui_puts(0u, 5u, "RAW RFC1951 STREAMS", color);
#endif
    tui_puts(0u, 22u, "RUN/STOP: LAUNCHER", TUI_COLOR_CYAN);
}

void XUZD_DIAG_RUN(unsigned char package_bank) {
    unsigned char work_bank;
#ifdef XUZD_ARCHIVE_DIAGNOSTIC
    unsigned char catalog_bank;
#endif
    unsigned char index;
    unsigned char failure;
#ifdef UZIP_XUZMULTI_DIAGNOSTIC
    unsigned char completed;
    unsigned char case_index;
#endif
    UzU32 actual_size;

    work_bank = XUZD_NONE;
#ifdef XUZD_ARCHIVE_DIAGNOSTIC
    catalog_bank = XUZD_NONE;
#endif
    index = 0u;
    failure = 0u;
#ifdef UZIP_XUZMULTI_DIAGNOSTIC
    completed = 0u;
#endif
    memset((void *)XUZD_RESULT, 0, 128u);
    memset((void *)0x0400u, 0x20, 1000u);
#ifdef XUZD_ARCHIVE_DIAGNOSTIC
#ifdef UZIP_XUZMULTI_DIAGNOSTIC
    screen_text(0u, "XUZMULTI C64U");
    screen_text(1u, "7 ENTRIES / STORE + DEFLATE");
#else
    screen_text(0u, "XUZZIP8 C64U");
    screen_text(1u, "STORE + RAW DEFLATE + STORE");
#endif
#else
    screen_text(0u, "XUZDEFLATE C64U");
    screen_text(1u, "OWNED REU + PACKED PHASES");
#endif
    screen_text(3u, "PRESS SPACE TO START");
    write_result(0u, XUZD_STAGE_READY, 0u, 0u, 0u,
                 package_bank, work_bank);
    while (tui_getkey() != ' ') { }

    memset((void *)0x0400u, 0, 1000u);
    work_bank = reu_alloc_owned_bank(XUZD_SCRATCH_SLOT, "uzwk");
#ifdef XUZD_ARCHIVE_DIAGNOSTIC
    catalog_bank = reu_alloc_owned_bank(XUZD_CATALOG_SLOT, "uzct");
#endif
    if (package_bank == XUZD_NONE || work_bank == XUZD_NONE ||
        package_bank == work_bank || bank_type(package_bank) != REU_APP_ALLOC ||
        bank_type(work_bank) != REU_APP_ALLOC
#ifdef XUZD_ARCHIVE_DIAGNOSTIC
        || catalog_bank == XUZD_NONE || catalog_bank == package_bank ||
        catalog_bank == work_bank || bank_type(catalog_bank) != REU_APP_ALLOC
#endif
        ) {
        failure = 0x21u;
        goto finished;
    }
#ifdef XUZD_ARCHIVE_DIAGNOSTIC
    XUZD_CONTEXT->catalog_bank = catalog_bank;
#endif

    load_store(package_bank);
    uz_dos_init(XUZD_INPUT_DOS, UZ_DOS_TARGET_READ,
                XUZD_INPUT_COMMAND, XUZD_DOS_COMMAND_CAP,
                XUZD_INPUT_DATA, XUZD_DOS_DATA_CAP,
                XUZD_SHARED_STATUS, XUZD_DOS_STATUS_CAP);
    uz_dos_init(XUZD_OUTPUT_DOS, UZ_DOS_TARGET_WRITE,
                XUZD_OUTPUT_COMMAND, XUZD_OUTPUT_COMMAND_CAP,
                XUZD_INPUT_DATA, XUZD_DOS_DATA_CAP,
                XUZD_SHARED_STATUS, XUZD_DOS_STATUS_CAP);
    write_result(0u, XUZD_STAGE_IDENTIFY, 0u, 0u, 0u,
                 package_bank, work_bank);
    if (!uz_dos_identify(XUZD_INPUT_DOS) ||
        !uz_dos_identify(XUZD_OUTPUT_DOS)) {
        failure = 0x31u;
        goto finished;
    }
    write_result(0u, XUZD_STAGE_OWNER, 0u, 0u, 0u,
                 package_bank, work_bank);
    if (!verify_owner()) {
        failure = 0x41u;
        goto finished;
    }
    write_result(0u, XUZD_STAGE_PATHS, 0u, 0u, 0u,
                 package_bank, work_bank);
    if (!uz_dos_change_absolute(XUZD_INPUT_DOS, XUZDEFLATE_OWNED_ROOT) ||
        !uz_dos_change_path(XUZD_INPUT_DOS, "source") ||
        !uz_dos_change_absolute(XUZD_OUTPUT_DOS, XUZDEFLATE_OWNED_ROOT) ||
        !uz_dos_create_dir(XUZD_OUTPUT_DOS, "output") ||
        !uz_dos_change_path(XUZD_OUTPUT_DOS, "output")) {
        failure = 0x51u;
        goto finished;
    }

#ifdef UZIP_XUZMULTI_DIAGNOSTIC
    if (!uz_dos_open(XUZD_OUTPUT_DOS, output_name(0u),
                     UZ_DOS_OPEN_WRITE_NEW)) {
        failure = 0x58u;
        goto finished;
    }
    for (index = 0u; index < XUZD_ENTRY_COUNT; ++index) {
        case_index = multi_case_index(index);
        write_result(0u, XUZD_STAGE_COMPRESS, 0u, completed, index,
                     package_bank, work_bank);
        if (!multi_directory(index)) {
            if (!uz_dos_open(XUZD_INPUT_DOS, multi_input_name(index),
                             UZ_DOS_OPEN_READ) ||
                !uz_dos_file_info(XUZD_INPUT_DOS, &actual_size) ||
                !size_is(&actual_size,
                         expected_size(case_index == XUZD_NONE ? 1u :
                                       case_index))) {
                failure = (unsigned char)(0x60u + index);
                goto finished;
            }
            XUZD_CONTEXT->input_size = actual_size;
        } else {
            uz_u32_zero(&XUZD_CONTEXT->input_size);
        }
        strcpy((char *)XUZD_INPUT, multi_member_name(index));
        XUZD_CONTEXT->entry_index = index;
        XUZD_CONTEXT->entry_count = XUZD_ENTRY_COUNT;
        XUZD_CONTEXT->method = multi_method(index);
        XUZD_CONTEXT->directory = multi_directory(index);
        XUZD_CONTEXT->first_entry = (unsigned char)(index == 0u);
        XUZD_CONTEXT->last_entry =
            (unsigned char)(index == XUZD_ENTRY_COUNT - 1u);
        XUZD_WORKSPACE_GUARD = 0xA5u;
        if (case_index != XUZD_NONE)
            stamp(XUZD_RESULT + 24u + 3u * case_index);
        if (!uz_job_run_deflate(package_bank, work_bank,
                                xuzdeflate_coord_entry)) {
            failure = (unsigned char)(0x70u + index);
            goto finished;
        }
        if (XUZD_WORKSPACE_GUARD != 0xA5u) {
            failure = (unsigned char)(0x80u + index);
            goto finished;
        }
        if (case_index != XUZD_NONE) {
            stamp(XUZD_RESULT + 36u + 3u * case_index);
            if (!size_is(&XUZD_DEFLATE->input_size,
                         expected_size(case_index)) ||
                !crc_matches(case_index, &XUZD_DEFLATE->crc) ||
                XUZD_DEFLATE->error != UZ_DEFLATE_OK) {
                failure = (unsigned char)(0x88u + index);
                goto finished;
            }
            record_case(case_index);
            ++completed;
        }
    }
#else
    for (index = 0u; index < XUZD_CASE_COUNT; ++index) {
        write_result(0u, XUZD_STAGE_COMPRESS, 0u, index, index,
                     package_bank, work_bank);
        if (!uz_dos_open(XUZD_INPUT_DOS, input_name(index),
                         UZ_DOS_OPEN_READ) ||
            !uz_dos_file_info(XUZD_INPUT_DOS, &actual_size) ||
            !size_is(&actual_size, expected_size(index)) ||
            !uz_dos_open(XUZD_OUTPUT_DOS, output_name(index),
                         UZ_DOS_OPEN_WRITE_NEW)) {
            failure = (unsigned char)(0x60u + index);
            goto finished;
        }
        XUZD_CONTEXT->input_size = actual_size;
#ifdef XUZD_ARCHIVE_DIAGNOSTIC
        strcpy(XUZD_CONTEXT->member_name, input_name(index));
#endif
        XUZD_WORKSPACE_GUARD = 0xA5u;
        stamp(XUZD_RESULT + 24u + 3u * index);
        if (!uz_job_run_deflate(package_bank, work_bank,
                                xuzdeflate_coord_entry)) {
            failure = (unsigned char)(0x70u + index);
            goto finished;
        }
        stamp(XUZD_RESULT + 36u + 3u * index);
        if (!size_is(&XUZD_DEFLATE->input_size, expected_size(index)) ||
            !crc_matches(index, &XUZD_DEFLATE->crc) ||
            XUZD_DEFLATE->error != UZ_DEFLATE_OK ||
            XUZD_WORKSPACE_GUARD != 0xA5u) {
            failure = (unsigned char)(0x80u + index);
            goto finished;
        }
        record_case(index);
    }
#endif

finished:
    if (package_bank != XUZD_NONE) load_store(package_bank);
    (void)uz_dos_close(XUZD_INPUT_DOS);
    (void)uz_dos_close(XUZD_OUTPUT_DOS);
    if (work_bank != XUZD_NONE) {
        reu_free_owned_bank(work_bank);
        XUZD_RESULT[19] = (unsigned char)(bank_type(work_bank) == REU_FREE);
    }
#ifdef XUZD_ARCHIVE_DIAGNOSTIC
    if (catalog_bank != XUZD_NONE) {
        reu_free_owned_bank(catalog_bank);
        XUZD_RESULT[23] =
            (unsigned char)(bank_type(catalog_bank) == REU_FREE);
    }
#endif
    write_result(1u, failure == 0u ? XUZD_STAGE_DONE : XUZD_STAGE_COMPRESS,
                 failure,
#ifdef UZIP_XUZMULTI_DIAGNOSTIC
                 completed,
#else
                 failure == 0u ? XUZD_CASE_COUNT : index,
#endif
                 index, package_bank, work_bank);
#ifdef XUZD_ARCHIVE_DIAGNOSTIC
#ifdef UZIP_XUZMULTI_DIAGNOSTIC
    if (failure == 0u) finish_screen("XUZMULTI FINISHED PASS",
                                    TUI_COLOR_LIGHTGREEN);
    else finish_screen("XUZMULTI FINISHED FAIL", TUI_COLOR_LIGHTRED);
#else
    if (failure == 0u) finish_screen("XUZZIP8 FINISHED PASS",
                                    TUI_COLOR_LIGHTGREEN);
    else finish_screen("XUZZIP8 FINISHED FAIL", TUI_COLOR_LIGHTRED);
#endif
#else
    if (failure == 0u) finish_screen("XUZDEFLATE FINISHED PASS",
                                    TUI_COLOR_LIGHTGREEN);
    else finish_screen("XUZDEFLATE FINISHED FAIL", TUI_COLOR_LIGHTRED);
#endif
    for (;;) {
        if (tui_getkey() == TUI_KEY_RUNSTOP) tui_return_to_launcher();
    }
}

#pragma rodata-name(pop)
#pragma code-name(pop)
