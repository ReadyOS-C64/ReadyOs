#include <conio.h>
#include <string.h>

#include "xuzstore_config.h"
#include "uz_dos.h"
#include "uz_zip_read.h"
#include "uz_zip_write.h"

#define RESULT ((volatile unsigned char *)0xC000u)
#define ENTRY_COUNT 5u
#define HOST_ENTRY_COUNT 4u
#define IO_CHUNK 512u

#define STAGE_IDENTIFY 1u
#define STAGE_OWNER    2u
#define STAGE_PATHS    3u
#define STAGE_ARCHIVE  4u
#define STAGE_CLOSE    5u
#define STAGE_VERIFY   6u
#define STAGE_HOST_ZIP 7u
#define STAGE_REJECT   8u
#define STAGE_DONE     9u

#define REJECT_BEGIN 0u
#define REJECT_NEXT  1u
#define REJECT_CRC   2u

static unsigned char command1[UZ_DOS_QUEUE_MAX];
static unsigned char command2[UZ_DOS_QUEUE_MAX];
static unsigned char data1[IO_CHUNK];
static unsigned char data2[IO_CHUNK];
static unsigned char status1[64];
static unsigned char status2[64];
static unsigned char io_buffer[IO_CHUNK];
static UzDos input;
static UzDos output;
static UzZipWriter zip;
static UzZipReader archive_reader;
static UzZipRecord records[ENTRY_COUNT];
static char output_path[UZ_DOS_PATH_CAP];
static char extract_path[UZ_DOS_PATH_CAP];
static char extract_directory_path[UZ_DOS_PATH_CAP];
static char extract_directory_name[UZ_ZIP_NAME_CAP];
static const char *extract_slash;
static const char *extract_file_name;
static unsigned int extract_prefix_len;
static unsigned int extract_name_len;
static char failure_message[40];
static unsigned char stage;
static unsigned char entry_index;
static unsigned char failure_code;
static unsigned char host_entry_index;
static unsigned char corrupt_index;
static UzZipRecord reject_record;
static const char *host_names[HOST_ENTRY_COUNT] = {
    "hostempty/", "host.txt", "deep/", "deep/file.bin"
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

    RESULT[0] = 0x58u; /* XZS1 */
    RESULT[1] = 0x5Au;
    RESULT[2] = 0x53u;
    RESULT[3] = 0x31u;
    RESULT[4] = 1u;
    RESULT[5] = done;
    RESULT[6] = stage;
    RESULT[7] = failure_code;
    RESULT[8] = entry_index;
    RESULT[9] = ENTRY_COUNT;
    result_u16(10u, uz_uci_base());
    RESULT[12] = input.transfer.flags;
    RESULT[13] = input.transfer.last_status;
    RESULT[14] = output.transfer.flags;
    RESULT[15] = output.transfer.last_status;
    RESULT[16] = zip.error;
    RESULT[17] = archive_reader.error;
    RESULT[18] = host_entry_index;
    RESULT[19] = corrupt_index;
    uz_u32_to_le((unsigned char *)(RESULT + 20u), &zip.offset);
    for (index = 0u; index < ENTRY_COUNT; ++index) {
        memcpy((void *)(RESULT + 32u + (unsigned int)index * 4u),
               records[index].crc.byte, 4u);
        uz_u32_to_le((unsigned char *)(RESULT + 52u + (unsigned int)index * 4u),
                     &records[index].size);
    }
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
    write_result(1u);
    (void)uz_dos_close(&input);
    (void)uz_dos_close(&output);
    textcolor(COLOR_RED);
    gotoxy(0u, 7u);
    cputs("XUZSTORE FINISHED FAIL");
    screen_text(7u, "XUZSTORE FINISHED FAIL");
    if (failure_message[0] != 0) screen_text(9u, failure_message);
    for (;;) { }
}

static unsigned char verify_owner(void) {
    unsigned int expected;
    int got;

    expected = strlen(XUZSTORE_OWNER_TEXT);
    if (!uz_dos_change_path(&input, XUZSTORE_OWNED_ROOT) ||
        !uz_dos_open(&input, ".readyos-uzip-owner", UZ_DOS_OPEN_READ)) return 0u;
    got = uz_dos_read(&input, io_buffer, sizeof(io_buffer));
    if (!uz_dos_close(&input) || got < 0 || (unsigned int)got != expected ||
        memcmp(io_buffer, XUZSTORE_OWNER_TEXT, expected) != 0) return 0u;
    return 1u;
}

static unsigned char add_directory(unsigned char index, const char *name) {
    entry_index = index;
    write_result(0u);
    return (unsigned char)(uz_zip_begin_store(&zip, &records[index], name, 1u) &&
                          uz_zip_finish_store(&zip));
}

static unsigned char add_file(unsigned char index, const char *archive_name,
                              const char *source_dir, const char *source_name) {
    int got;

    entry_index = index;
    write_result(0u);
    if (!uz_dos_change_path(&input, source_dir) ||
        !uz_dos_open(&input, source_name, UZ_DOS_OPEN_READ) ||
        !uz_zip_begin_store(&zip, &records[index], archive_name, 0u)) return 0u;
    for (;;) {
        got = uz_dos_read(&input, io_buffer, sizeof(io_buffer));
        if (got < 0) return 0u;
        if (got != 0 && !uz_zip_store_data(&zip, io_buffer, (unsigned int)got)) {
            return 0u;
        }
        if ((unsigned int)got < sizeof(io_buffer)) break;
    }
    if (!uz_dos_close(&input) || !uz_zip_finish_store(&zip)) return 0u;
    return 1u;
}

static unsigned char extract_record(const UzZipRecord *record) {
    if (record->directory) {
        extract_name_len = strlen(record->name);
        if (extract_name_len < 2u ||
            record->name[extract_name_len - 1u] != '/' ||
            strchr(record->name, '/') != record->name + extract_name_len - 1u) {
            return 0u;
        }
        memcpy(extract_directory_name, record->name, extract_name_len - 1u);
        extract_directory_name[extract_name_len - 1u] = 0;
        return (unsigned char)(uz_zip_extract_store(&archive_reader, record, 0,
                                                   io_buffer, sizeof(io_buffer)) &&
                              uz_dos_change_path(&output, extract_path) &&
                              uz_dos_create_dir(&output, extract_directory_name));
    }

    extract_slash = strrchr(record->name, '/');
    extract_file_name = record->name;
    strcpy(extract_directory_path, extract_path);
    if (extract_slash != 0) {
        extract_prefix_len = (unsigned int)(extract_slash - record->name);
        if (strlen(extract_directory_path) + 1u + extract_prefix_len >=
            sizeof(extract_directory_path)) {
            return 0u;
        }
        strcat(extract_directory_path, "/");
        strncat(extract_directory_path, record->name, extract_prefix_len);
        extract_file_name = extract_slash + 1;
    }
    if (!uz_dos_change_path(&output, extract_directory_path) ||
        !uz_dos_open(&output, extract_file_name, UZ_DOS_OPEN_WRITE_NEW)) return 0u;
    if (!uz_zip_extract_store(&archive_reader, record, &output,
                              io_buffer, sizeof(io_buffer))) {
        (void)uz_dos_close(&output);
        return 0u;
    }
    return uz_dos_close(&output);
}

static unsigned char reject_archive(const char *source_path, const char *name,
                                    unsigned char mode,
                                    unsigned char expected_error) {
    unsigned char accepted;

    if (!uz_dos_change_path(&input, source_path) ||
        !uz_dos_open(&input, name, UZ_DOS_OPEN_READ)) return 0u;
    uz_zip_reader_init(&archive_reader, &input);
    accepted = uz_zip_reader_begin(&archive_reader, io_buffer, sizeof(io_buffer));
    if (mode == REJECT_BEGIN) {
        accepted = (unsigned char)(!accepted &&
            (archive_reader.error == expected_error ||
             (expected_error == UZ_ZIP_READ_IO &&
              archive_reader.error == UZ_ZIP_READ_NO_EOCD)));
    } else if (!accepted) {
        accepted = 0u;
    } else if (mode == REJECT_NEXT) {
        accepted = (unsigned char)(!uz_zip_reader_next(&archive_reader,
                                                       &reject_record) &&
                                   archive_reader.error == expected_error);
    } else {
        accepted = (unsigned char)(uz_zip_reader_next(&archive_reader,
                                                      &reject_record) &&
            !uz_zip_extract_store(&archive_reader, &reject_record, 0,
                                  io_buffer, sizeof(io_buffer)) &&
            archive_reader.error == expected_error);
    }
    if (!uz_dos_close(&input)) return 0u;
    return accepted;
}

int main(void) {
    char nested_path[UZ_DOS_PATH_CAP];
    UzZipRecord parsed;
    UzU32 central_offset;
    unsigned char index;

    bordercolor(COLOR_BLUE);
    bgcolor(COLOR_BLUE);
    textcolor(COLOR_WHITE);
    clrscr();
    screen_text(0u, "XUZSTORE PHYSICAL C64 ULTIMATE PROBE");
    screen_text(1u, "STREAMED STORE ZIP + DESCRIPTORS");
    screen_text(2u, "PRESS SPACE TO START");
    memset((void *)RESULT, 0, 128u);
    while (cgetc() != ' ') { }

    uz_dos_init(&input, UZ_DOS_TARGET_READ,
                command1, sizeof(command1), data1, sizeof(data1),
                status1, sizeof(status1));
    uz_dos_init(&output, UZ_DOS_TARGET_WRITE,
                command2, sizeof(command2), data2, sizeof(data2),
                status2, sizeof(status2));

    stage = STAGE_IDENTIFY;
    show_stage("IDENTIFY BOTH ULTIMATE DOS TARGETS");
    if (!uz_dos_identify(&input)) stop_failed(0x11u, &input);
    if (!uz_dos_identify(&output)) stop_failed(0x12u, &output);

    stage = STAGE_OWNER;
    show_stage("VERIFY UNIQUE OWNED ROOT");
    if (!verify_owner()) stop_failed(0x21u, &input);

    stage = STAGE_PATHS;
    show_stage("CREATE ARCHIVE OUTPUT FOLDER");
    if (!uz_dos_change_path(&output, XUZSTORE_OWNED_ROOT) ||
        !uz_dos_create_dir(&output, "output")) stop_failed(0x31u, &output);
    strcpy(output_path, XUZSTORE_OWNED_ROOT);
    strcat(output_path, "/output");
    if (!uz_dos_change_path(&output, output_path) ||
        !uz_dos_open(&output, "created.zip", UZ_DOS_OPEN_WRITE_NEW)) {
        stop_failed(0x32u, &output);
    }

    stage = STAGE_ARCHIVE;
    show_stage("STREAM FIVE STORE ENTRIES");
    uz_zip_writer_init(&zip, &output);
    if (!add_directory(0u, "empty/")) stop_failed(0x40u, &output);
    if (!add_file(1u, "hello.txt", XUZSTORE_OWNED_ROOT "/source",
                  "hello.txt")) stop_failed(0x41u, &input);
    if (!add_directory(2u, "nested/")) stop_failed(0x42u, &output);
    strcpy(nested_path, XUZSTORE_OWNED_ROOT);
    strcat(nested_path, "/source/nested");
    if (!add_file(3u, "nested/boundary.bin", nested_path,
                  "boundary.bin")) stop_failed(0x43u, &input);
    if (!add_file(4u, "zero.bin", XUZSTORE_OWNED_ROOT "/source",
                  "zero.bin")) stop_failed(0x44u, &input);
    central_offset = zip.offset;
    for (index = 0u; index < ENTRY_COUNT; ++index) {
        if (!uz_zip_emit_central(&zip, &records[index]))
            stop_failed(0x45u, &output);
    }
    if (!uz_zip_finish_archive(&zip, &central_offset, ENTRY_COUNT)) {
        stop_failed(0x45u, &output);
    }

    stage = STAGE_CLOSE;
    show_stage("CLOSE ARCHIVE AT PERSISTENCE BOUNDARY");
    if (!uz_dos_close(&output)) stop_failed(0x51u, &output);

    stage = STAGE_VERIFY;
    show_stage("C64 PARSE + EXTRACT WHOLE ZIP");
    if (!uz_dos_change_path(&output, XUZSTORE_OWNED_ROOT) ||
        !uz_dos_create_dir(&output, "extracted")) stop_failed(0x52u, &output);
    strcpy(extract_path, XUZSTORE_OWNED_ROOT);
    strcat(extract_path, "/extracted");
    if (!uz_dos_change_path(&input, output_path) ||
        !uz_dos_open(&input, "created.zip", UZ_DOS_OPEN_READ)) {
        stop_failed(0x53u, &input);
    }
    uz_zip_reader_init(&archive_reader, &input);
    if (!uz_zip_reader_begin(&archive_reader, io_buffer, sizeof(io_buffer)) ||
        archive_reader.entry_count != ENTRY_COUNT) stop_failed(0x54u, &input);
    for (index = 0u; index < ENTRY_COUNT; ++index) {
        entry_index = index;
        write_result(0u);
        if (!uz_zip_reader_next(&archive_reader, &parsed) ||
            strcmp(parsed.name, records[index].name) != 0 ||
            !uz_u32_equal(&parsed.size, &records[index].size) ||
            !uz_crc32_equal(&parsed.crc, &records[index].crc) ||
            !extract_record(&parsed)) {
            stop_failed((unsigned char)(0x60u + index), &input);
        }
    }
    if (!uz_zip_reader_finished(&archive_reader) || !uz_dos_close(&input)) {
        stop_failed(0x68u, &input);
    }

    stage = STAGE_HOST_ZIP;
    show_stage("EXTRACT HOST ZIP WITHOUT DESCRIPTORS");
    if (!uz_dos_change_path(&output, XUZSTORE_OWNED_ROOT) ||
        !uz_dos_create_dir(&output, "hostextracted")) stop_failed(0x71u, &output);
    strcpy(extract_path, XUZSTORE_OWNED_ROOT);
    strcat(extract_path, "/hostextracted");
    strcpy(nested_path, XUZSTORE_OWNED_ROOT);
    strcat(nested_path, "/source");
    if (!uz_dos_change_path(&input, nested_path) ||
        !uz_dos_open(&input, "hoststore.zip", UZ_DOS_OPEN_READ)) {
        stop_failed(0x72u, &input);
    }
    uz_zip_reader_init(&archive_reader, &input);
    if (!uz_zip_reader_begin(&archive_reader, io_buffer, sizeof(io_buffer)) ||
        archive_reader.entry_count != HOST_ENTRY_COUNT) stop_failed(0x73u, &input);
    for (index = 0u; index < HOST_ENTRY_COUNT; ++index) {
        host_entry_index = index;
        write_result(0u);
        if (!uz_zip_reader_next(&archive_reader, &parsed) ||
            strcmp(parsed.name, host_names[index]) != 0 || parsed.flags != 0u ||
            !extract_record(&parsed)) {
            stop_failed((unsigned char)(0x74u + index), &input);
        }
    }
    if (!uz_zip_reader_finished(&archive_reader) || !uz_dos_close(&input)) {
        stop_failed(0x78u, &input);
    }

    stage = STAGE_REJECT;
    show_stage("REJECT CORRUPT STORE ARCHIVES");
    corrupt_index = 0u;
    if (!reject_archive(nested_path, "truncated.zip", REJECT_BEGIN,
                        UZ_ZIP_READ_IO)) stop_failed(0x81u, &input);
    corrupt_index = 1u;
    if (!reject_archive(nested_path, "multidisk.zip", REJECT_BEGIN,
                        UZ_ZIP_READ_UNSUPPORTED)) stop_failed(0x82u, &input);
    corrupt_index = 2u;
    if (!reject_archive(nested_path, "traversal.zip", REJECT_NEXT,
                        UZ_ZIP_READ_NAME)) stop_failed(0x83u, &input);
    corrupt_index = 3u;
    if (!reject_archive(nested_path, "badcrc.zip", REJECT_CRC,
                        UZ_ZIP_READ_CRC)) stop_failed(0x84u, &input);
    corrupt_index = 4u;

    stage = STAGE_DONE;
    entry_index = ENTRY_COUNT;
    host_entry_index = HOST_ENTRY_COUNT;
    write_result(1u);
    textcolor(COLOR_GREEN);
    screen_text(9u, "HOST ZIP ORACLE REQUIRED");
    gotoxy(0u, 7u);
    cputs("XUZSTORE FINISHED PASS");
    screen_text(7u, "XUZSTORE FINISHED PASS");
    for (;;) { }
    return 0;
}
