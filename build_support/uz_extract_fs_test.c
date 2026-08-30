#include <assert.h>
#include <string.h>

#include "uz_extract_fs.h"

static char calls[512];
static unsigned int calls_len;
static unsigned char change_results[16];
static unsigned char change_count;
static unsigned char change_at;
static unsigned char open_failures;
static unsigned char fail_seek;
static unsigned char fail_close;
static unsigned char fail_rename;
static unsigned char store_result;
static unsigned char inflate_result;
static unsigned char inflate_error;

static void log_call(const char *kind, const char *name) {
    unsigned int length;

    length = strlen(kind);
    assert(calls_len + length + 2u < sizeof(calls));
    memcpy(calls + calls_len, kind, length);
    calls_len = (unsigned int)(calls_len + length);
    if (name != 0) {
        calls[calls_len++] = ':';
        length = strlen(name);
        memcpy(calls + calls_len, name, length);
        calls_len = (unsigned int)(calls_len + length);
    }
    calls[calls_len++] = '|';
    calls[calls_len] = 0;
}

unsigned char uz_dos_change_absolute(UzDos *dos, const char *path) {
    (void)dos;
    log_call("root", path);
    return 1u;
}

unsigned char uz_dos_change_path(UzDos *dos, const char *path) {
    (void)dos;
    log_call("cd", path);
    assert(change_at < change_count);
    return change_results[change_at++];
}

unsigned char uz_dos_create_dir(UzDos *dos, const char *name) {
    (void)dos;
    log_call("md", name);
    return 1u;
}

unsigned char uz_dos_open(UzDos *dos, const char *name, unsigned char flags) {
    assert(flags == UZ_DOS_OPEN_WRITE_NEW);
    log_call("open", name);
    if (open_failures != 0u) {
        --open_failures;
        return 0u;
    }
    dos->file_open = 1u;
    return 1u;
}

unsigned char uz_dos_close(UzDos *dos) {
    log_call("close", 0);
    dos->file_open = 0u;
    return (unsigned char)!fail_close;
}

unsigned char uz_dos_delete(UzDos *dos, const char *name) {
    (void)dos;
    log_call("del", name);
    return 1u;
}

unsigned char uz_dos_rename(UzDos *dos, const char *old_name,
                            const char *new_name) {
    (void)dos;
    log_call("ren", old_name);
    log_call("to", new_name);
    return (unsigned char)!fail_rename;
}

unsigned char uz_dos_seek(UzDos *dos, const UzU32 *offset) {
    (void)dos;
    assert(offset->lo == 0x3456u && offset->hi == 0x0012u);
    log_call("seek", 0);
    return (unsigned char)!fail_seek;
}

unsigned char uz_job_run_store(unsigned char package_bank,
                               unsigned char work_bank,
                               UzJobStoreEntry entry,
                               const UzStoreJobRequest *request) {
    assert(package_bank == 7u && work_bank == 8u);
    assert(entry == uz_store_job_entry);
    assert(request->input_target == UZ_DOS_TARGET_READ &&
           request->output_target == UZ_DOS_TARGET_WRITE);
    assert(request->size.lo == 99u);
    log_call("store", 0);
    return store_result;
}

unsigned char uz_job_run_inflate(unsigned char package_bank,
                                 unsigned char work_bank,
                                 UzJobInflateEntry entry,
                                 const UzInflateJobRequest *request) {
    assert(package_bank == 7u && work_bank == 8u);
    assert(entry == uz_inflate_job_entry);
    assert(request->compressed_size.lo == 44u && request->output_size.lo == 99u);
    log_call("inflate", 0);
    return inflate_result;
}

unsigned char uz_store_job_entry(const UzStoreJobRequest *request) {
    (void)request;
    return 0u;
}

unsigned char uz_inflate_job_entry(const UzInflateJobRequest *request) {
    (void)request;
    return 0u;
}

unsigned char uz_job_inflate_error(void) {
    return inflate_error;
}

unsigned char uz_job_inflate_codec_error(void) {
    return 0u;
}

static void reset_test(UzDos *input, UzDos *output, UzExtractFs *state,
                       UzZipRecord *record, UzU32 *offset) {
    memset(input, 0, sizeof(*input));
    memset(output, 0, sizeof(*output));
    memset(record, 0, sizeof(*record));
    memset(change_results, 0, sizeof(change_results));
    calls[0] = 0;
    calls_len = change_count = change_at = 0u;
    open_failures = fail_seek = fail_close = fail_rename = 0u;
    store_result = UZ_STORE_JOB_OK;
    inflate_result = 1u;
    inflate_error = UZ_INFLATE_JOB_OK;
    input->target = UZ_DOS_TARGET_READ;
    input->file_open = 1u;
    output->target = UZ_DOS_TARGET_WRITE;
    offset->lo = 0x3456u;
    offset->hi = 0x0012u;
    record->size.lo = 99u;
    record->compressed_size.lo = 99u;
    uz_extract_fs_init(state);
}

int main(void) {
    UzDos input;
    UzDos output;
    UzExtractFs state;
    UzZipRecord record;
    UzU32 offset;

    reset_test(&input, &output, &state, &record, &offset);
    strcpy(record.name, "root/sub/file.bin");
    change_results[0] = 0u; change_results[1] = 1u;
    change_results[2] = 1u; change_count = 3u;
    open_failures = 1u;
    assert(uz_extract_member(&state, &input, &output, "/usb1/out", &record,
                             &offset, 7u, 8u));
    assert(strcmp(state.temp, ".uztmp01") == 0);
    assert(strstr(calls, "root:/usb1/out|cd:root|md:root|cd:root|cd:sub|") != 0);
    assert(strstr(calls, "open:.uztmp00|open:.uztmp01|seek|store|close|") != 0);
    assert(strstr(calls, "ren:.uztmp01|to:file.bin|") != 0);

    reset_test(&input, &output, &state, &record, &offset);
    strcpy(record.name, "empty/dir/");
    record.directory = 1u;
    change_results[0] = 1u; change_results[1] = 0u;
    change_results[2] = 1u; change_count = 3u;
    assert(uz_extract_member(&state, &input, &output, "/usb1/out", &record,
                             &offset, 7u, 8u));
    assert(strstr(calls, "cd:empty|cd:dir|md:dir|cd:dir|") != 0);
    assert(strstr(calls, "open") == 0);

    reset_test(&input, &output, &state, &record, &offset);
    strcpy(record.name, "../escape");
    assert(!uz_extract_member(&state, &input, &output, "/usb1/out", &record,
                              &offset, 7u, 8u));
    assert(state.error == UZ_EXTRACT_PATH && strstr(calls, "open") == 0);

    reset_test(&input, &output, &state, &record, &offset);
    strcpy(record.name, "file.bin");
    record.method = 8u;
    record.compressed_size.lo = 44u;
    inflate_result = 0u;
    inflate_error = UZ_INFLATE_JOB_CRC;
    assert(!uz_extract_member(&state, &input, &output, "/usb1/out", &record,
                              &offset, 7u, 8u));
    assert(state.error == UZ_EXTRACT_JOB &&
           state.job_error == UZ_INFLATE_JOB_CRC);
    assert(strstr(calls, "inflate|close|del:.uztmp00|") != 0);

    reset_test(&input, &output, &state, &record, &offset);
    strcpy(record.name, "file.bin");
    fail_rename = 1u;
    assert(!uz_extract_member(&state, &input, &output, "/usb1/out", &record,
                              &offset, 7u, 8u));
    assert(state.error == UZ_EXTRACT_COMMIT);
    assert(strstr(calls, "ren:.uztmp00|to:file.bin|del:.uztmp00|") != 0);
    return 0;
}
