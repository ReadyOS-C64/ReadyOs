/*
 * xuzreu_diag.c - full-ReadyOS physical direct file/REU diagnostic
 *
 * This source is linked only when UZIP_XUZREU_DIAGNOSTIC is enabled. It runs
 * in the idle/UI window while the production job phase occupies $B000-$C3FF.
 * The scratch address passed to Ultimate DOS is always a physical bank
 * returned by reu_alloc_owned_bank; a standalone arbitrary REU address is
 * never used.
 */

#include "xuzreu_diag.h"

#include "xuzreu_config.h"
#include "uz_crc32.h"
#include "uz_dos.h"
#include "uz_pack.h"
#include "uz_package.h"

#include "../../lib/reu_control_bank.h"
#include "../../lib/reu_mgr.h"
#include "../../lib/reu_owned_alloc.h"
#include "../../lib/tui.h"

#include <string.h>

#define RESULT ((volatile unsigned char *)0xC000u)
#define SHIM_CURRENT_BANK (*(volatile unsigned char *)0xC834u)

#define XUZREU_NONE 0xFFu
#define XUZREU_SCRATCH_SLOT 2u
#define XUZREU_STAGE_READY 1u
#define XUZREU_STAGE_BANKS 2u
#define XUZREU_STAGE_IDENTIFY 3u
#define XUZREU_STAGE_LOAD 4u
#define XUZREU_STAGE_SAVE 5u
#define XUZREU_STAGE_VERIFY 6u
#define XUZREU_STAGE_FREE 7u
#define XUZREU_STAGE_CORE_DONE 8u
#define XUZREU_STAGE_RESUME 9u

#define XUZREU_LOAD1_OFFSET 0x9400u
#define XUZREU_LOAD2_OFFSET 0xA400u
#define XUZREU_LOAD_REQUEST 4096u
#define XUZREU_SHORT_LENGTH 777u
#define XUZREU_IO_CHUNK 512u

#pragma code-name(push, "UI_CODE")
#pragma rodata-name(push, "UI_RODATA")
#pragma bss-name(push, "UI_BSS")

static unsigned char command1[UZ_DOS_QUEUE_MAX];
static unsigned char command2[UZ_DOS_QUEUE_MAX];
static unsigned char data1[XUZREU_IO_CHUNK];
static unsigned char data2[XUZREU_IO_CHUNK];
static unsigned char status1[64];
static unsigned char status2[64];
static unsigned char io_buffer[XUZREU_IO_CHUNK];
static unsigned char compare_buffer[XUZREU_IO_CHUNK];
static UzDos input;
static UzDos output;
static UzCrc32 load1_crc;
static UzCrc32 load2_crc;
static UzCrc32 output_crc;
static UzCrc32 package_before_crc;
static UzCrc32 package_after_crc;
static UzU32 seek_offset;
static UzU32 file_size;
static char source_path[UZ_DOS_PATH_CAP];
static char output_path[UZ_DOS_PATH_CAP];
static char failure_message[40];
static unsigned char scratch_bank;
static unsigned char scratch_physical;
static unsigned char stage;
static unsigned char failure_code;
static unsigned int load1_got;
static unsigned int load2_got;
static unsigned int save1_got;
static unsigned int save2_got;
static unsigned char diag_package_bank;
static unsigned char failure_data_len;
static unsigned char failure_stat_len;
static unsigned char resume_check_bits;
static unsigned char resume_expected_bank;
static unsigned char resume_expected_free;
static unsigned char resume_expected_crc[4];

static unsigned char bank_type(unsigned char bank) {
    if (bank == XUZREU_NONE) return REU_UNAVAIL;
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
    for (column = 0u; column < 40u; ++column) {
        screen[column] = (*text != 0) ? screen_code((unsigned char)*text++) : 0x20u;
    }
}

static void result_u16(unsigned char offset, unsigned int value) {
    RESULT[offset] = (unsigned char)value;
    RESULT[(unsigned char)(offset + 1u)] = (unsigned char)(value >> 8u);
}

static void result_crc(unsigned char offset, const UzCrc32 *crc) {
    memcpy((void *)(RESULT + offset), crc->byte, 4u);
}

static void write_result(unsigned char done, unsigned char package_bank) {
    RESULT[0] = 0x58u; /* ASCII XZR1, kept numeric for cc65 charset safety. */
    RESULT[1] = 0x5Au;
    RESULT[2] = 0x52u;
    RESULT[3] = 0x31u;
    RESULT[4] = 1u;
    RESULT[5] = done;
    RESULT[6] = stage;
    RESULT[7] = failure_code;
    RESULT[8] = package_bank;
    RESULT[9] = scratch_physical;
    result_u16(10u, uz_uci_base());
    RESULT[12] = input.transfer.flags;
    RESULT[13] = input.transfer.last_status;
    RESULT[14] = output.transfer.flags;
    RESULT[15] = output.transfer.last_status;
    result_u16(16u, XUZREU_LOAD_REQUEST);
    result_u16(18u, load1_got);
    result_u16(20u, XUZREU_LOAD_REQUEST);
    result_u16(22u, load2_got);
    result_u16(24u, save1_got);
    result_u16(26u, save2_got);
    result_crc(28u, &load1_crc);
    result_crc(32u, &load2_crc);
    result_crc(36u, &output_crc);
    result_crc(40u, &package_before_crc);
    result_crc(44u, &package_after_crc);
    RESULT[48] = (unsigned char)(stage == XUZREU_STAGE_RESUME);
    RESULT[49] = (scratch_bank == XUZREU_NONE) ? 1u : 0u;
    RESULT[50] = (package_bank != XUZREU_NONE &&
                  bank_type(package_bank) == REU_APP_ALLOC) ? 1u : 0u;
    RESULT[51] = SHIM_CURRENT_BANK;
    result_u16(52u, XUZREU_OUTPUT_LENGTH);
    RESULT[54] = XUZREU_CONFIG_COOKIE0;
    RESULT[55] = XUZREU_CONFIG_COOKIE1;
    RESULT[56] = XUZREU_CONFIG_COOKIE2;
    RESULT[57] = XUZREU_CONFIG_COOKIE3;
    RESULT[58] = failure_data_len;
    RESULT[59] = failure_stat_len;
    RESULT[60] = resume_check_bits;
}

static void draw_probe(const char *line2, const char *line4,
                       unsigned char color) {
    tui_clear(TUI_COLOR_BLUE);
    screen_text(0u, "XUZREU READYOS OWNED BANK PROBE");
    screen_text(1u, line2);
    screen_text(3u, line4);
    tui_puts(0u, 6u, "PKG BANK", TUI_COLOR_GRAY3);
    tui_print_uint(9u, 6u, RESULT[8], TUI_COLOR_WHITE);
    tui_puts(16u, 6u, "WORK BANK", TUI_COLOR_GRAY3);
    tui_print_uint(26u, 6u, RESULT[9], TUI_COLOR_WHITE);
    tui_puts(0u, 8u, "ULTIMATE DOS DIRECT FILE/REU", color);
    tui_puts(0u, 22u, "CTRL-B RETURNS TO LAUNCHER", TUI_COLOR_CYAN);
}

static void show_stage(const char *text, unsigned char package_bank) {
    screen_text(3u, text);
    write_result(0u, package_bank);
}

static void cleanup(void) {
    (void)uz_dos_close(&input);
    (void)uz_dos_close(&output);
    if (scratch_bank != XUZREU_NONE) {
        reu_free_owned_bank(scratch_bank);
        scratch_bank = XUZREU_NONE;
    }
}

static void fail(unsigned char code, unsigned char package_bank,
                 const UzDos *dos) {
    unsigned char key;

    (void)package_bank;
    failure_code = code;
    if (dos != 0) {
        failure_data_len = (unsigned char)dos->transfer.data_len;
        failure_stat_len = (unsigned char)dos->transfer.stat_len;
        strncpy(failure_message, uz_dos_message(dos),
                sizeof(failure_message) - 1u);
        failure_message[sizeof(failure_message) - 1u] = 0;
    } else {
        failure_message[0] = 0;
    }
    cleanup();
    write_result(1u, diag_package_bank);
    draw_probe("DIRECT TRANSFER FINISHED FAIL", failure_message,
               TUI_COLOR_LIGHTRED);
    screen_text(7u, "XUZREU FINISHED FAIL");
    for (;;) {
        key = tui_getkey();
        if (key == 2u || key == TUI_KEY_RUNSTOP) tui_return_to_launcher();
    }
}

static void crc_source_range(unsigned char bank, unsigned int reu_offset,
                             unsigned int length, UzCrc32 *crc) {
    unsigned int chunk;

    uz_crc32_init(crc);
    while (length != 0u) {
        chunk = (length > sizeof(io_buffer)) ? sizeof(io_buffer) : length;
        reu_dma_fetch((unsigned int)io_buffer, bank, reu_offset, chunk);
        uz_crc32_update(crc, io_buffer, chunk);
        reu_offset = (unsigned int)(reu_offset + chunk);
        length = (unsigned int)(length - chunk);
    }
    uz_crc32_finish(crc);
}

static void crc_reu(unsigned char bank, unsigned int offset,
                    unsigned int length, UzCrc32 *crc) {
    unsigned int chunk;

    uz_crc32_init(crc);
    while (length != 0u) {
        chunk = (length > sizeof(io_buffer)) ? sizeof(io_buffer) : length;
        reu_dma_fetch((unsigned int)io_buffer, bank, offset, chunk);
        uz_crc32_update(crc, io_buffer, chunk);
        offset = (unsigned int)(offset + chunk);
        length = (unsigned int)(length - chunk);
    }
    uz_crc32_finish(crc);
}

static unsigned char verify_owner(void) {
    unsigned int index;
    int got;

    if (!uz_dos_change_path(&input, XUZREU_OWNED_ROOT) ||
        !uz_dos_open(&input, ".readyos-uzip-owner", UZ_DOS_OPEN_READ)) {
        return 0u;
    }
    got = uz_dos_read(&input, io_buffer, sizeof(io_buffer));
    if (!uz_dos_close(&input) || got < 0 ||
        (unsigned int)got != XUZREU_OWNER_LENGTH) return 0u;
    for (index = 0u; index < XUZREU_OWNER_LENGTH; ++index) {
        if (io_buffer[index] != (unsigned char)XUZREU_OWNER_TEXT[index]) {
            return 0u;
        }
    }
    return 1u;
}

static unsigned char verify_output_queue(void) {
    unsigned int total;
    unsigned int chunk;
    unsigned int expected_offset;
    unsigned int index;
    int got;

    total = 0u;
    uz_crc32_init(&output_crc);
    for (;;) {
        got = uz_dos_read(&input, io_buffer, sizeof(io_buffer));
        if (got < 0) return 0u;
        if (got == 0) break;
        chunk = (unsigned int)got;
        if (total < XUZREU_LOAD_REQUEST) {
            expected_offset = (unsigned int)(XUZREU_LOAD1_OFFSET + total);
        } else {
            expected_offset = (unsigned int)(XUZREU_LOAD2_OFFSET +
                (unsigned int)(total - XUZREU_LOAD_REQUEST));
        }
        reu_dma_fetch((unsigned int)compare_buffer, scratch_bank,
                      expected_offset, chunk);
        for (index = 0u; index < chunk; ++index) {
            if (io_buffer[index] != compare_buffer[index]) return 0u;
        }
        uz_crc32_update(&output_crc, io_buffer, chunk);
        total = (unsigned int)(total + chunk);
        if (chunk < sizeof(io_buffer)) break;
    }
    uz_crc32_finish(&output_crc);
    return (unsigned char)(total == XUZREU_OUTPUT_LENGTH);
}

static unsigned char crc_matches_config(const UzCrc32 *actual,
                                        unsigned char which) {
    if (which == 1u) {
        return (unsigned char)(actual->byte[0] == XUZREU_CRC1_0 &&
            actual->byte[1] == XUZREU_CRC1_1 &&
            actual->byte[2] == XUZREU_CRC1_2 &&
            actual->byte[3] == XUZREU_CRC1_3);
    }
    if (which == 2u) {
        return (unsigned char)(actual->byte[0] == XUZREU_CRC2_0 &&
            actual->byte[1] == XUZREU_CRC2_1 &&
            actual->byte[2] == XUZREU_CRC2_2 &&
            actual->byte[3] == XUZREU_CRC2_3);
    }
    return (unsigned char)(actual->byte[0] == XUZREU_CRCO_0 &&
        actual->byte[1] == XUZREU_CRCO_1 &&
        actual->byte[2] == XUZREU_CRCO_2 &&
        actual->byte[3] == XUZREU_CRCO_3);
}

static unsigned char result_cookie_matches(void) {
    return (unsigned char)(RESULT[54] == XUZREU_CONFIG_COOKIE0 &&
                           RESULT[55] == XUZREU_CONFIG_COOKIE1 &&
                           RESULT[56] == XUZREU_CONFIG_COOKIE2 &&
                           RESULT[57] == XUZREU_CONFIG_COOKIE3);
}

unsigned char xuzreu_diag_result_present(void) {
    return (unsigned char)(RESULT[0] == 0x58u && RESULT[1] == 0x5Au &&
                           RESULT[2] == 0x52u && RESULT[3] == 0x31u &&
                           RESULT[4] == 1u && RESULT[5] == 1u &&
                           (RESULT[6] == XUZREU_STAGE_CORE_DONE ||
                            RESULT[6] == XUZREU_STAGE_RESUME) &&
                           result_cookie_matches());
}

static void load_job_phase(unsigned char package_bank) {
    reu_dma_fetch(uz_pack_job_run(), package_bank,
                  uz_package_phase_offset(UZ_PACKAGE_PHASE_JOB),
                  uz_pack_job_size());
}

static void wait_for_launcher(void) {
    unsigned char key;
    for (;;) {
        key = tui_getkey();
        if (key == 2u || key == TUI_KEY_RUNSTOP) tui_return_to_launcher();
    }
}

static void run_resume_check(unsigned char package_bank) {
    (void)package_bank;
    /* RESULT is deliberately at $C000 so hardware automation can dump it, but
     * $C000 also belongs to the $B000-$C3FF job window. Preserve the expected
     * evidence in UI state before proving that uzpk can reload the overlay. */
    resume_expected_bank = RESULT[8];
    resume_expected_free = RESULT[49];
    memcpy(resume_expected_crc, (const void *)(RESULT + 40u), 4u);
    load_job_phase(diag_package_bank);
    resume_check_bits = 0u;
    if (diag_package_bank == XUZREU_NONE) resume_check_bits |= 0x01u;
    if (diag_package_bank != resume_expected_bank) resume_check_bits |= 0x02u;
    if (bank_type(diag_package_bank) != REU_APP_ALLOC)
        resume_check_bits |= 0x04u;
    if (resume_expected_free != 1u) resume_check_bits |= 0x08u;
    if (resume_check_bits != 0u) {
        stage = XUZREU_STAGE_RESUME;
        fail(0x91u, diag_package_bank, 0);
    }
    crc_reu(diag_package_bank, 0u, uz_package_payload_size(),
            &package_after_crc);
    memcpy(package_before_crc.byte, resume_expected_crc, 4u);
    if (!uz_crc32_equal(&package_before_crc, &package_after_crc)) {
        stage = XUZREU_STAGE_RESUME;
        fail(0x92u, diag_package_bank, 0);
    }
    stage = XUZREU_STAGE_RESUME;
    failure_code = 0u;
    write_result(1u, diag_package_bank);
    draw_probe("WARM SNAPSHOT + UZPK PRESERVED",
               "XUZREU RESUME PASS", TUI_COLOR_LIGHTGREEN);
    screen_text(7u, "XUZREU FINISHED RESUME PASS");
    wait_for_launcher();
}

void xuzreu_diag_run(unsigned char package_bank) {
    unsigned int package_length;
    unsigned int invalid_got;

    diag_package_bank = package_bank;
    if (xuzreu_diag_result_present()) {
        run_resume_check(package_bank);
        return;
    }

    memset((void *)RESULT, 0, 128u);
    memset(&load1_crc, 0, sizeof(load1_crc));
    memset(&load2_crc, 0, sizeof(load2_crc));
    memset(&output_crc, 0, sizeof(output_crc));
    memset(&package_before_crc, 0, sizeof(package_before_crc));
    memset(&package_after_crc, 0, sizeof(package_after_crc));
    scratch_bank = XUZREU_NONE;
    scratch_physical = XUZREU_NONE;
    load1_got = load2_got = save1_got = save2_got = 0u;
    failure_data_len = failure_stat_len = 0u;
    resume_check_bits = 0u;
    failure_code = 0u;
    stage = XUZREU_STAGE_READY;
    write_result(0u, package_bank);
    draw_probe("PHYSICAL C64 ULTIMATE ONLY",
               "PRESS SPACE TO START", TUI_COLOR_YELLOW);
    screen_text(7u, "PRESS SPACE TO START");
    while (tui_getkey() != ' ') { }

    stage = XUZREU_STAGE_BANKS;
    show_stage("ALLOCATE OWNED UZWK BANK", package_bank);
    scratch_bank = reu_alloc_owned_bank(XUZREU_SCRATCH_SLOT, "uzwk");
    scratch_physical = scratch_bank;
    if (package_bank == XUZREU_NONE || scratch_bank == XUZREU_NONE ||
        package_bank == scratch_bank ||
        bank_type(package_bank) != REU_APP_ALLOC ||
        bank_type(scratch_bank) != REU_APP_ALLOC) {
        fail(0x21u, package_bank, 0);
    }
    load_job_phase(package_bank);
    package_length = uz_package_payload_size();
    crc_reu(package_bank, 0u, package_length, &package_before_crc);

    uz_dos_init(&input, UZ_DOS_TARGET_READ,
                command1, sizeof(command1), data1, sizeof(data1),
                status1, sizeof(status1));
    uz_dos_init(&output, UZ_DOS_TARGET_WRITE,
                command2, sizeof(command2), data2, sizeof(data2),
                status2, sizeof(status2));

    stage = XUZREU_STAGE_IDENTIFY;
    show_stage("IDENTIFY BOTH ULTIMATE DOS TARGETS", package_bank);
    if (!uz_dos_identify(&input)) fail(0x31u, package_bank, &input);
    if (!uz_dos_identify(&output)) fail(0x32u, package_bank, &output);
    if (!verify_owner()) fail(0x33u, package_bank, &input);
    strcpy(source_path, XUZREU_SOURCE_PATH);
    if (!uz_dos_change_path(&input, source_path) ||
        !uz_dos_open(&input, "source.bin", UZ_DOS_OPEN_READ) ||
        !uz_dos_file_info(&input, &file_size) || file_size.hi != 0u ||
        file_size.lo != XUZREU_SOURCE_LENGTH) {
        fail(0x34u, package_bank, &input);
    }

    stage = XUZREU_STAGE_LOAD;
    show_stage("DIRECT LOAD AT NONZERO OFFSETS", package_bank);
    seek_offset.hi = 0u;
    seek_offset.lo = XUZREU_SOURCE_OFFSET1;
    if (!uz_dos_seek(&input, &seek_offset) ||
        !uz_dos_load_reu(&input, scratch_bank, XUZREU_LOAD1_OFFSET,
                         XUZREU_LOAD_REQUEST, &load1_got) ||
        load1_got != XUZREU_LOAD_REQUEST) {
        fail(0x41u, package_bank, &input);
    }
    crc_source_range(scratch_bank, XUZREU_LOAD1_OFFSET,
                     load1_got, &load1_crc);
    if (!crc_matches_config(&load1_crc, 1u)) fail(0x41u, package_bank, &input);
    seek_offset.lo = XUZREU_SOURCE_OFFSET2;
    if (!uz_dos_seek(&input, &seek_offset)) fail(0x42u, package_bank, &input);
    invalid_got = 0xA55Au;
    if (uz_dos_load_reu(&input, scratch_bank, 0xF800u,
                        XUZREU_LOAD_REQUEST, &invalid_got) ||
        invalid_got != 0xA55Au) {
        fail(0x43u, package_bank, &input);
    }
    if (!uz_dos_load_reu(&input, scratch_bank, XUZREU_LOAD2_OFFSET,
                         XUZREU_LOAD_REQUEST, &load2_got) ||
        load2_got != XUZREU_SHORT_LENGTH) {
        fail(0x44u, package_bank, &input);
    }
    crc_source_range(scratch_bank, XUZREU_LOAD2_OFFSET,
                     load2_got, &load2_crc);
    if (!crc_matches_config(&load2_crc, 2u) || !uz_dos_close(&input)) {
        fail(0x44u, package_bank, &input);
    }

    stage = XUZREU_STAGE_SAVE;
    show_stage("DIRECT SAVE TO NEW OUTPUT FILE", package_bank);
    if (!uz_dos_change_path(&output, XUZREU_OWNED_ROOT) ||
        !uz_dos_create_dir(&output, "output")) {
        fail(0x51u, package_bank, &output);
    }
    strcpy(output_path, XUZREU_OUTPUT_PATH);
    if (!uz_dos_change_path(&output, output_path) ||
        !uz_dos_open(&output, "result.bin", UZ_DOS_OPEN_WRITE_NEW) ||
        !uz_dos_save_reu(&output, scratch_bank, XUZREU_LOAD1_OFFSET,
                         XUZREU_LOAD_REQUEST, &save1_got) ||
        save1_got != XUZREU_LOAD_REQUEST ||
        !uz_dos_save_reu(&output, scratch_bank, XUZREU_LOAD2_OFFSET,
                         XUZREU_SHORT_LENGTH, &save2_got) ||
        save2_got != XUZREU_SHORT_LENGTH || !uz_dos_close(&output)) {
        fail(0x52u, package_bank, &output);
    }

    stage = XUZREU_STAGE_VERIFY;
    show_stage("CLOSE REOPEN QUEUE VERIFY OUTPUT", package_bank);
    if (!uz_dos_change_path(&input, output_path) ||
        !uz_dos_open(&input, "result.bin", UZ_DOS_OPEN_READ) ||
        !uz_dos_file_info(&input, &file_size) || file_size.hi != 0u ||
        file_size.lo != XUZREU_OUTPUT_LENGTH || !verify_output_queue() ||
        !crc_matches_config(&output_crc, 3u) || !uz_dos_close(&input)) {
        fail(0x61u, package_bank, &input);
    }
    crc_reu(package_bank, 0u, package_length, &package_after_crc);
    if (!uz_crc32_equal(&package_before_crc, &package_after_crc)) {
        fail(0x62u, package_bank, 0);
    }

    stage = XUZREU_STAGE_FREE;
    show_stage("FREE UZWK KEEP UZPK FOR RESUME", package_bank);
    reu_free_owned_bank(scratch_bank);
    scratch_bank = XUZREU_NONE;
    if (bank_type(scratch_physical) != REU_FREE) fail(0x71u, package_bank, 0);

    stage = XUZREU_STAGE_CORE_DONE;
    failure_code = 0u;
    write_result(1u, package_bank);
    draw_probe("DIRECT LOAD/SAVE + QUEUE VERIFY",
               "XUZREU CORE PASS", TUI_COLOR_LIGHTGREEN);
    screen_text(7u, "XUZREU FINISHED CORE PASS");
    wait_for_launcher();
}

#pragma bss-name(pop)
#pragma rodata-name(pop)
#pragma code-name(pop)
