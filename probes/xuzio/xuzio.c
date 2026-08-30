#include <conio.h>
#include <string.h>

#include "xuzio_config.h"
#include "uz_crc32.h"
#include "uz_dos.h"

#define RESULT ((volatile unsigned char *)0xC000u)
#define CASE_COUNT 9u
#define IO_CHUNK 512u
#define WRITE_CHUNK UZ_DOS_WRITE_MAX

#define STAGE_IDENTIFY  1u
#define STAGE_OWNER     2u
#define STAGE_PATHS     3u
#define STAGE_WRITE     4u
#define STAGE_READ      5u
#define STAGE_MUTATE    6u
#define STAGE_DIRECTORY 7u
#define STAGE_DONE      8u

typedef struct {
    const char *name;
    UzU32 size;
    UzCrc32 crc;
} IoCase;

static unsigned char command1[UZ_DOS_QUEUE_MAX];
static unsigned char command2[UZ_DOS_QUEUE_MAX];
static unsigned char data1[IO_CHUNK];
static unsigned char data2[IO_CHUNK];
static unsigned char status1[64];
static unsigned char status2[64];
static unsigned char io_buffer[IO_CHUNK];
static UzDos reader;
static UzDos writer;
static char source_path[UZ_DOS_PATH_CAP];
static char output_path[UZ_DOS_PATH_CAP];
static unsigned char stage;
static unsigned char case_index;
static unsigned char failure_code;
static unsigned int directory_count;
static char failure_message[40];
static unsigned char path_step;
static unsigned char root_count;
static char root_summary[40];
static unsigned char usb_count;
static unsigned char usb_match_attr;
static char usb_summary[36];
static unsigned char io_step;
static UzU32 reported_size;
static int last_got;
static unsigned char actual_byte;
static unsigned char expected_byte;

static IoCase cases[CASE_COUNT] = {
    {"s00000.bin", {0x0000u, 0x0000u}, {{0u, 0u, 0u, 0u}}},
    {"s00001.bin", {0x0001u, 0x0000u}, {{0u, 0u, 0u, 0u}}},
    {"s00511.bin", {0x01FFu, 0x0000u}, {{0u, 0u, 0u, 0u}}},
    {"s00512.bin", {0x0200u, 0x0000u}, {{0u, 0u, 0u, 0u}}},
    {"s00513.bin", {0x0201u, 0x0000u}, {{0u, 0u, 0u, 0u}}},
    {"s04095.bin", {0x0FFFu, 0x0000u}, {{0u, 0u, 0u, 0u}}},
    {"s04096.bin", {0x1000u, 0x0000u}, {{0u, 0u, 0u, 0u}}},
    {"s65535.bin", {0xFFFFu, 0x0000u}, {{0u, 0u, 0u, 0u}}},
    {"s65536.bin", {0x0000u, 0x0001u}, {{0u, 0u, 0u, 0u}}}
};

/* Exercise the exact 512-byte boundary first so queue regressions fail fast. */
static const unsigned char case_order[CASE_COUNT] = {
    3u, 4u, 6u, 2u, 7u, 8u, 0u, 1u, 5u
};

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

static void write_result(unsigned char done) {
    unsigned char index;

    RESULT[0] = 0x58u; /* XUZ1 */
    RESULT[1] = 0x55u;
    RESULT[2] = 0x5Au;
    RESULT[3] = 0x31u;
    RESULT[4] = 1u;
    RESULT[5] = done;
    RESULT[6] = stage;
    RESULT[7] = failure_code;
    RESULT[8] = case_index;
    RESULT[9] = CASE_COUNT;
    result_u16(10u, directory_count);
    result_u16(12u, uz_uci_base());
    RESULT[14] = reader.transfer.flags;
    RESULT[15] = reader.transfer.last_status;
    RESULT[16] = writer.transfer.flags;
    RESULT[17] = writer.transfer.last_status;
    RESULT[18] = path_step;
    RESULT[19] = root_count;
    RESULT[20] = usb_count;
    RESULT[21] = usb_match_attr;
    RESULT[22] = io_step;
    uz_u32_to_le((unsigned char *)(RESULT + 24u), &reported_size);
    result_u16(28u, (unsigned int)last_got);
    RESULT[30] = actual_byte;
    RESULT[31] = expected_byte;
    for (index = 0u; index < CASE_COUNT; ++index) {
        memcpy((void *)(RESULT + 32u + (unsigned int)index * 4u),
               cases[index].crc.byte, 4u);
    }
    if (stage == STAGE_OWNER && failure_code != 0u) {
        memcpy((void *)(RESULT + 32u), usb_summary, sizeof(usb_summary));
    }
    memcpy((void *)(RESULT + 72u), root_summary, sizeof(root_summary));
}

static void show_stage(const char *text) {
    gotoxy(0u, 4u);
    cclear(40u);
    gotoxy(0u, 4u);
    cputs(text);
    screen_text(4u, text);
    write_result(0u);
}

static void stop_failed(unsigned char code, const UzDos *dos) {
    failure_code = code;
    if (dos != 0) {
        strncpy(failure_message, uz_dos_message(dos), sizeof(failure_message) - 1u);
        failure_message[sizeof(failure_message) - 1u] = 0;
    } else {
        failure_message[0] = 0;
    }
    /* Preserve the command that failed before CLOSE can replace its status. */
    write_result(1u);
    (void)uz_dos_close(&reader);
    (void)uz_dos_close(&writer);
    textcolor(COLOR_RED);
    gotoxy(0u, 7u);
    cputs("XUZIO FINISHED FAIL STAGE ");
    cputc((char)('0' + stage));
    cputs(" CODE ");
    cputhex8(code);
    screen_text(7u, "XUZIO FINISHED FAIL");
    if (failure_message[0] != 0) {
        gotoxy(0u, 9u);
        cputs(failure_message);
    }
    if (root_summary[0] != 0) screen_text(11u, root_summary);
    if (usb_summary[0] != 0) screen_text(12u, usb_summary);
    for (;;) { }
}

static unsigned char ascii_upper(unsigned char value) {
    if (value >= 0x61u && value <= 0x7Au) return (unsigned char)(value - 0x20u);
    if (value == 0xA4u) return 0x5Fu;
    return value;
}

static unsigned char data_ends_with(const UzDos *dos, const char *suffix) {
    unsigned int suffix_len;
    unsigned int data_len;
    unsigned int start;
    unsigned int index;

    suffix_len = strlen(suffix);
    data_len = dos->transfer.data_len;
    while (data_len != 0u &&
           (dos->data[data_len - 1u] == 0x2Fu || dos->data[data_len - 1u] == 0x5Cu)) {
        --data_len;
    }
    if (data_len < suffix_len) return 0u;
    start = (unsigned int)(data_len - suffix_len);
    for (index = 0u; index < suffix_len; ++index) {
        if (ascii_upper(dos->data[start + index]) !=
            ascii_upper((unsigned char)suffix[index])) return 0u;
    }
    return 1u;
}

static void collect_usb_name(const unsigned char *data,
                             unsigned int data_len,
                             const unsigned char *status,
                             unsigned int status_len);

static unsigned char verify_owner(void) {
    int got;
    unsigned int owner_len;

    owner_len = strlen(XUZIO_OWNER_TEXT);
    path_step = 1u;
    if (!uz_dos_change_path(&reader, "/")) return 0u;
    path_step = 2u;
    if (!uz_dos_change_path(&reader, "/usb1")) return 0u;
    usb_count = 0u;
    usb_match_attr = 0u;
    usb_summary[0] = 0;
    if (!uz_dos_open_dir(&reader) ||
        !uz_dos_read_dir(&reader, collect_usb_name)) return 0u;
    path_step = 3u;
    if (!uz_dos_change_path(&reader, "/usb1/readyos\x5fuzip\x5ftest")) return 0u;
    path_step = 4u;
    if (!uz_dos_change_path(&reader, XUZIO_OWNED_ROOT)) return 0u;
    path_step = 5u;
    if (!uz_dos_open(&reader, ".readyos-uzip-owner", UZ_DOS_OPEN_READ)) return 0u;
    got = uz_dos_read(&reader, io_buffer, sizeof(io_buffer));
    if (!uz_dos_close(&reader) || got < 0 || (unsigned int)got != owner_len ||
        memcmp(io_buffer, XUZIO_OWNER_TEXT, owner_len) != 0) return 0u;
    return 1u;
}

static void collect_root_name(const unsigned char *data,
                              unsigned int data_len,
                              const unsigned char *status,
                              unsigned int status_len) {
    unsigned int used;
    unsigned int index;

    (void)status;
    (void)status_len;
    ++root_count;
    if (data_len <= 1u) return;
    used = strlen(root_summary);
    if (used != 0u && used + 1u < sizeof(root_summary)) root_summary[used++] = ' ';
    for (index = 1u; index < data_len && used + 1u < sizeof(root_summary); ++index) {
        root_summary[used++] = (char)data[index];
    }
    root_summary[used] = 0;
}

static void collect_usb_name(const unsigned char *data,
                             unsigned int data_len,
                             const unsigned char *status,
                             unsigned int status_len) {
    unsigned int used;
    unsigned int index;

    (void)status;
    (void)status_len;
    ++usb_count;
    if (data_len < 13u || ascii_upper(data[1]) != 0x52u ||
        ascii_upper(data[2]) != 0x45u || ascii_upper(data[3]) != 0x41u ||
        ascii_upper(data[4]) != 0x44u || ascii_upper(data[5]) != 0x59u ||
        ascii_upper(data[6]) != 0x4Fu || ascii_upper(data[7]) != 0x53u ||
        data[8] != 0x5Fu || ascii_upper(data[9]) != 0x55u ||
        ascii_upper(data[10]) != 0x5Au || ascii_upper(data[11]) != 0x49u ||
        ascii_upper(data[12]) != 0x50u) return;
    usb_match_attr = data[0];
    used = strlen(usb_summary);
    if (used != 0u && used + 1u < sizeof(usb_summary)) usb_summary[used++] = ' ';
    for (index = 1u; index < data_len && used + 1u < sizeof(usb_summary); ++index) {
        usb_summary[used++] = (char)data[index];
    }
    usb_summary[used] = 0;
}

static unsigned char value_is_zero(const UzU32 *value) {
    return (unsigned char)(value->lo == 0u && value->hi == 0u);
}

static void value_sub_u16(UzU32 *value, unsigned int amount) {
    unsigned int before;

    before = value->lo;
    value->lo = (unsigned int)(value->lo - amount);
    if (before < amount) --value->hi;
}

static void value_half(UzU32 *destination, const UzU32 *source) {
    destination->lo = (unsigned int)((source->lo >> 1u) |
                          ((source->hi & 1u) != 0u ? 0x8000u : 0u));
    destination->hi = (unsigned int)(source->hi >> 1u);
}

static unsigned char pattern_byte(unsigned char index, const UzU32 *position) {
    return (unsigned char)((unsigned char)position->lo ^
                           (unsigned char)(position->lo >> 8u) ^
                           (unsigned char)position->hi ^
                           (unsigned char)(index * 29u) ^ 0xA5u);
}

static void fill_pattern(unsigned char index, UzU32 *position,
                         unsigned int length) {
    unsigned int offset;

    for (offset = 0u; offset < length; ++offset) {
        io_buffer[offset] = pattern_byte(index, position);
        uz_u32_add_u16(position, 1u);
    }
}

static unsigned int next_chunk(const UzU32 *remaining, unsigned int maximum) {
    if (remaining->hi != 0u || remaining->lo > maximum) return maximum;
    return remaining->lo;
}

static unsigned char write_case(unsigned char index) {
    UzU32 remaining;
    UzU32 position;
    unsigned int chunk;

    remaining = cases[index].size;
    uz_u32_zero(&position);
    uz_crc32_init(&cases[index].crc);
    uz_u32_zero(&reported_size);
    io_step = 1u;
    if (!uz_dos_open(&writer, cases[index].name, UZ_DOS_OPEN_WRITE_NEW)) return 0u;
    while (!value_is_zero(&remaining)) {
        chunk = next_chunk(&remaining, WRITE_CHUNK);
        fill_pattern(index, &position, chunk);
        uz_crc32_update(&cases[index].crc, io_buffer, chunk);
        io_step = 2u;
        if (!uz_dos_write(&writer, io_buffer, chunk)) return 0u;
        value_sub_u16(&remaining, chunk);
    }
    uz_crc32_finish(&cases[index].crc);
    /* Ultimate DOS does not publish the new length through FILE_INFO until
     * CLOSE. The following read phase reopens and verifies persisted size. */
    io_step = 3u;
    if (!uz_dos_close(&writer)) return 0u;
    return 1u;
}

static unsigned char read_case(unsigned char index) {
    UzU32 remaining;
    UzU32 position;
    UzU32 midpoint;
    UzU32 reported;
    UzCrc32 crc;
    unsigned int chunk;
    int got;

    uz_u32_zero(&reported_size);
    last_got = -1;
    io_step = 11u;
    if (!uz_dos_open(&reader, cases[index].name, UZ_DOS_OPEN_READ)) return 0u;
    io_step = 12u;
    if (!uz_dos_file_info(&reader, &reported)) return 0u;
    reported_size = reported;
    io_step = 13u;
    if (!uz_u32_equal(&reported, &cases[index].size)) return 0u;

    if (!value_is_zero(&cases[index].size)) {
        value_half(&midpoint, &cases[index].size);
        io_step = 14u;
        if (!uz_dos_seek(&reader, &midpoint)) return 0u;
        io_step = 15u;
        got = uz_dos_read(&reader, io_buffer, 1u);
        last_got = got;
        if (got != 1) return 0u;
        io_step = 16u;
        actual_byte = io_buffer[0];
        expected_byte = pattern_byte(index, &midpoint);
        if (actual_byte != expected_byte) return 0u;
    }

    uz_u32_zero(&position);
    io_step = 17u;
    if (!uz_dos_seek(&reader, &position)) return 0u;
    remaining = cases[index].size;
    uz_crc32_init(&crc);
    while (!value_is_zero(&remaining)) {
        chunk = next_chunk(&remaining, IO_CHUNK);
        io_step = 18u;
        got = uz_dos_read(&reader, io_buffer, chunk);
        last_got = got;
        if (got < 0) return 0u;
        io_step = 19u;
        if ((unsigned int)got != chunk) return 0u;
        uz_crc32_update(&crc, io_buffer, chunk);
        uz_u32_add_u16(&position, chunk);
        value_sub_u16(&remaining, chunk);
    }
    io_step = 20u;
    got = uz_dos_read(&reader, io_buffer, 1u);
    last_got = got;
    uz_crc32_finish(&crc);
    io_step = 21u;
    if (got != 0) return 0u;
    io_step = 22u;
    if (!uz_crc32_equal(&crc, &cases[index].crc)) return 0u;
    io_step = 23u;
    if (!uz_dos_close(&reader)) return 0u;
    return 1u;
}

static unsigned char test_rename_delete(void) {
    UzU32 one = {1u, 0u};
    UzU32 reported;

    io_buffer[0] = 0x5Au;
    if (!uz_dos_open(&writer, "rename.tmp", UZ_DOS_OPEN_WRITE_NEW) ||
        !uz_dos_write(&writer, io_buffer, 1u) || !uz_dos_close(&writer) ||
        !uz_dos_rename(&writer, "rename.tmp", "renamed.ok") ||
        !uz_dos_file_stat(&writer, "renamed.ok", &reported) ||
        !uz_u32_equal(&one, &reported)) return 0u;
    if (!uz_dos_open(&writer, "delete.tmp", UZ_DOS_OPEN_WRITE_NEW) ||
        !uz_dos_write(&writer, io_buffer, 1u) || !uz_dos_close(&writer) ||
        !uz_dos_delete(&writer, "delete.tmp")) return 0u;
    return 1u;
}

static void count_directory_block(const unsigned char *data,
                                  unsigned int data_len,
                                  const unsigned char *status,
                                  unsigned int status_len) {
    (void)data;
    (void)status;
    (void)status_len;
    if (data_len != 0u) ++directory_count;
}

int main(void) {
    unsigned char index;

    bordercolor(COLOR_BLUE);
    bgcolor(COLOR_BLUE);
    textcolor(COLOR_WHITE);
    clrscr();
    cputs("XUZIO PHYSICAL C64 ULTIMATE PROBE\r\n");
    cputs("OWNED ROOT + QUEUE + SEEK + CRC\r\n");
    cputs("PRESS SPACE TO START\r\n");
    screen_text(0u, "XUZIO PHYSICAL C64 ULTIMATE PROBE");
    screen_text(1u, "OWNED ROOT + QUEUE + SEEK + CRC");
    screen_text(2u, "PRESS SPACE TO START");
    memset((void *)RESULT, 0, 128u);

    while (cgetc() != ' ') { }

    uz_dos_init(&reader, UZ_DOS_TARGET_READ,
                command1, sizeof(command1), data1, sizeof(data1),
                status1, sizeof(status1));
    uz_dos_init(&writer, UZ_DOS_TARGET_WRITE,
                command2, sizeof(command2), data2, sizeof(data2),
                status2, sizeof(status2));

    stage = STAGE_IDENTIFY;
    show_stage("IDENTIFY TARGETS 1 AND 2");
    if (!uz_dos_identify(&reader)) stop_failed(0x11u, &reader);
    if (!uz_dos_identify(&writer)) stop_failed(0x12u, &writer);

    stage = STAGE_OWNER;
    show_stage("VERIFY OWNERSHIP MARKER");
    root_count = 0u;
    root_summary[0] = 0;
    if (!uz_dos_change_absolute(&reader, "/") ||
        !uz_dos_open_dir(&reader) ||
        !uz_dos_read_dir(&reader, collect_root_name)) {
        stop_failed(0x20u, &reader);
    }
    if (!verify_owner()) stop_failed(0x21u, &reader);

    stage = STAGE_PATHS;
    show_stage("CREATE INDEPENDENT NESTED PATHS");
    if (!uz_dos_change_absolute(&reader, XUZIO_OWNED_ROOT) ||
        !uz_dos_create_dir(&reader, "source mix")) stop_failed(0x31u, &reader);
    strcpy(source_path, XUZIO_OWNED_ROOT);
    strcat(source_path, "/source mix");
    if (!uz_dos_change_absolute(&reader, source_path) ||
        !uz_dos_get_path(&reader) || !data_ends_with(&reader, "/source mix")) {
        stop_failed(0x32u, &reader);
    }
    if (!uz_dos_change_absolute(&writer, XUZIO_OWNED_ROOT) ||
        !uz_dos_create_dir(&writer, "output\x5fmix")) stop_failed(0x33u, &writer);
    strcpy(output_path, XUZIO_OWNED_ROOT);
    strcat(output_path, "/output\x5fmix");
    if (!uz_dos_change_absolute(&writer, output_path) ||
        !uz_dos_get_path(&writer) || !data_ends_with(&writer, "/output\x5fmix")) {
        stop_failed(0x34u, &writer);
    }

    stage = STAGE_WRITE;
    show_stage("WRITE + REOPEN + VERIFY CASES");
    if (!uz_dos_change_absolute(&reader, output_path)) stop_failed(0x50u, &reader);
    for (index = 0u; index < CASE_COUNT; ++index) {
        case_index = case_order[index];
        stage = STAGE_WRITE;
        write_result(0u);
        if (!write_case(case_index)) {
            stop_failed((unsigned char)(0x40u + case_index), &writer);
        }
        stage = STAGE_READ;
        write_result(0u);
        if (!read_case(case_index)) {
            stop_failed((unsigned char)(0x51u + case_index), &reader);
        }
    }

    stage = STAGE_MUTATE;
    show_stage("RENAME + STAT + DELETE");
    if (!test_rename_delete()) stop_failed(0x61u, &writer);

    stage = STAGE_DIRECTORY;
    show_stage("FULL DIRECTORY STREAM DRAIN");
    directory_count = 0u;
    if (!uz_dos_open_dir(&reader) ||
        !uz_dos_read_dir(&reader, count_directory_block) ||
        directory_count != CASE_COUNT + 1u) stop_failed(0x71u, &reader);

    stage = STAGE_DONE;
    case_index = CASE_COUNT;
    failure_code = 0u;
    write_result(1u);
    textcolor(COLOR_GREEN);
    gotoxy(0u, 7u);
    cputs("XUZIO FINISHED PASS");
    screen_text(7u, "XUZIO FINISHED PASS");
    gotoxy(0u, 9u);
    cputs("9 SIZES, SEEK, CRC, DIR, MUTATIONS");
    screen_text(9u, "9 SIZES, SEEK, CRC, DIR, MUTATIONS");
    for (;;) { }
    return 0;
}
