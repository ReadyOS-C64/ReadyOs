#include "uz_extract_fs.h"

#include <string.h>

#ifdef UZIP_READYOS_APP
#pragma code-name(push, "UI_CODE")
#pragma rodata-name(push, "UI_RODATA")
#pragma bss-name(push, "UI_BSS")
#endif

static unsigned char component_safe(const char *text, unsigned int length) {
    unsigned int i;
    unsigned char value;

    if (length == 0u ||
        (length == 1u && text[0] == '.') ||
        (length == 2u && text[0] == '.' && text[1] == '.')) return 0u;
    for (i = 0u; i < length; ++i) {
        value = (unsigned char)text[i];
        if (value < 0x20u || value > 0x7Eu || value == '\\' || value == ':')
            return 0u;
    }
    return 1u;
}

static unsigned char ensure_directory(UzDos *output, const char *name) {
    if (uz_dos_change_path(output, name)) return 1u;
    if (!uz_dos_create_dir(output, name)) return 0u;
    return uz_dos_change_path(output, name);
}

static unsigned char prepare_destination(UzExtractFs *state,
                                         UzDos *output,
                                         const char *root,
                                         const UzZipRecord *record) {
    const char *cursor;
    unsigned int length;
    unsigned int total;

    if (root == 0 || root[0] != '/' || record == 0 || record->name[0] == 0 ||
        record->name[0] == '/' || record->name[0] == '\\' ||
        !uz_dos_change_absolute(output, root)) return 0u;
    cursor = record->name;
    total = 0u;
    while (*cursor != 0) {
        length = 0u;
        while (cursor[length] != 0 && cursor[length] != '/') {
            if (++total >= UZ_ZIP_NAME_CAP ||
                length + 1u >= sizeof(state->component)) return 0u;
            state->component[length] = cursor[length];
            ++length;
        }
        if (!component_safe(state->component, length)) return 0u;
        state->component[length] = 0;
        cursor += length;
        if (*cursor == '/') {
            ++cursor;
            ++total;
            if (!ensure_directory(output, state->component)) return 0u;
            if (*cursor == 0) {
                state->leaf[0] = 0;
                return record->directory;
            }
        } else {
            if (record->directory) return 0u;
            strcpy(state->leaf, state->component);
            return 1u;
        }
    }
    return 0u;
}

static unsigned char hex_char(unsigned char value) {
    value &= 0x0Fu;
    return (unsigned char)(value < 10u ? '0' + value : 'a' + value - 10u);
}

static unsigned char open_unique_temp(UzExtractFs *state, UzDos *output) {
    unsigned int attempt;

    strcpy(state->temp, ".uztmp00");
    for (attempt = 0u; attempt < 256u; ++attempt) {
        state->temp[6] = (char)hex_char((unsigned char)(attempt >> 4u));
        state->temp[7] = (char)hex_char((unsigned char)attempt);
        if (uz_dos_open(output, state->temp, UZ_DOS_OPEN_WRITE_NEW)) {
            state->temp_open = 1u;
            state->temp_created = 1u;
            return 1u;
        }
    }
    return 0u;
}

static void discard_temp(UzExtractFs *state, UzDos *output) {
    if (state->temp_open) {
        uz_dos_close(output);
        state->temp_open = 0u;
    }
    if (state->temp_created) {
        uz_dos_delete(output, state->temp);
        state->temp_created = 0u;
    }
}

static unsigned char close_temp(UzExtractFs *state, UzDos *output) {
    (void)state;
    return uz_dos_close(output);
}

void uz_extract_fs_init(UzExtractFs *state) {
    memset(state, 0, sizeof(*state));
}

unsigned char uz_extract_member(UzExtractFs *state,
                                UzDos *archive,
                                UzDos *output,
                                const char *destination_root,
                                const UzZipRecord *record,
                                const UzU32 *data_offset,
                                unsigned char package_bank,
                                unsigned char work_bank) {
    UzStoreJobRequest store_request;
    UzInflateJobRequest inflate_request;
    unsigned char job_result;

    if (state == 0 || archive == 0 || output == 0 || record == 0 ||
        data_offset == 0 || !archive->file_open || output->file_open ||
        package_bank == 0xFFu || work_bank == 0xFFu ||
        package_bank == work_bank) {
        if (state != 0) state->error = UZ_EXTRACT_STATE;
        return 0u;
    }
    state->error = UZ_EXTRACT_OK;
    state->job_error = 0u;
    state->temp_open = 0u;
    state->temp_created = 0u;
    if (!prepare_destination(state, output, destination_root, record)) {
        state->error = UZ_EXTRACT_PATH;
        return 0u;
    }
    if (record->directory) return 1u;
    if (!open_unique_temp(state, output)) {
        state->error = UZ_EXTRACT_TEMP;
        return 0u;
    }
    if (!uz_dos_seek(archive, data_offset)) {
        state->error = UZ_EXTRACT_SEEK;
        discard_temp(state, output);
        return 0u;
    }

    if (record->method == 0u) {
        memset(&store_request, 0, sizeof(store_request));
        store_request.input_target = archive->target;
        store_request.output_target = output->target;
        store_request.work_bank = work_bank;
        store_request.size = record->size;
        store_request.expected_crc = record->crc;
        job_result = uz_job_run_store(package_bank, work_bank,
                                      uz_store_job_entry, &store_request);
        state->job_error = job_result;
        job_result = (unsigned char)(job_result == UZ_STORE_JOB_OK);
    } else if (record->method == 8u) {
        memset(&inflate_request, 0, sizeof(inflate_request));
        inflate_request.input_target = archive->target;
        inflate_request.output_target = output->target;
        inflate_request.work_bank = work_bank;
        inflate_request.compressed_size = record->compressed_size;
        inflate_request.output_size = record->size;
        inflate_request.expected_crc = record->crc;
        job_result = uz_job_run_inflate(package_bank, work_bank,
                                        uz_inflate_job_entry,
                                        &inflate_request);
        state->job_error = job_result ? UZ_INFLATE_JOB_OK :
                           uz_job_inflate_error();
    } else {
        state->job_error = UZ_STORE_JOB_STATE;
        job_result = 0u;
    }
    if (!job_result) {
        state->error = UZ_EXTRACT_JOB;
        discard_temp(state, output);
        return 0u;
    }
    if (!close_temp(state, output)) {
        state->temp_open = 0u;
        state->error = UZ_EXTRACT_CLOSE;
        discard_temp(state, output);
        return 0u;
    }
    state->temp_open = 0u;
    if (!uz_dos_rename(output, state->temp, state->leaf)) {
        state->error = UZ_EXTRACT_COMMIT;
        discard_temp(state, output);
        return 0u;
    }
    state->temp_created = 0u;
    return 1u;
}

#ifdef UZIP_READYOS_APP
#pragma bss-name(pop)
#pragma rodata-name(pop)
#pragma code-name(pop)
#endif
