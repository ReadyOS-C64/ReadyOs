#include <assert.h>
#include <string.h>

#include "uz_inflate_job.h"
#include "uz_dos.h"
#include "uz_inflate6502.h"

static UzInflate6502Read saved_read;
static UzInflate6502Write saved_write;
static UzU32 actual_size;
static UzCrc32 actual_crc;
static unsigned char scenario;
static unsigned char codec_failure;
static unsigned int load_count;
static unsigned int save_count;
static unsigned int fetch_count;
static unsigned int stash_count;

void uz_dos_init(UzDos *dos, unsigned char target,
                 unsigned char *command, unsigned int command_cap,
                 unsigned char *data, unsigned int data_cap,
                 unsigned char *status, unsigned int status_cap) {
    memset(dos, 0, sizeof(*dos));
    dos->target = target;
    dos->command = command;
    dos->command_cap = command_cap;
    dos->data = data;
    dos->data_cap = data_cap;
    dos->status = status;
    dos->status_cap = status_cap;
}

unsigned char uz_dos_reu_transfer(UzDos *dos, unsigned char command,
                                  unsigned char bank, unsigned int offset,
                                  unsigned int length,
                                  unsigned int *transferred) {
    assert(dos->file_open);
    assert(bank == 7u && offset == 0xA000u && length != 0u);
    if (command == UZ_DOS_REU_LOAD) {
        ++load_count;
        if (scenario == 1u) return 0u;
    } else {
        assert(command == UZ_DOS_REU_SAVE);
        ++save_count;
        if (scenario == 2u) return 0u;
    }
    *transferred = length;
    return 1u;
}

void reu_dma_fetch(unsigned int c64_addr, unsigned char bank,
                   unsigned int reu_offset, unsigned int length) {
    assert(c64_addr == 0x0400u && bank == 7u &&
           reu_offset == 0xA000u && length == 11u);
    ++fetch_count;
}

void reu_dma_stash(unsigned int c64_addr, unsigned char bank,
                   unsigned int reu_offset, unsigned int length) {
    assert(c64_addr == 0x0600u && bank == 7u &&
           reu_offset == 0xA000u && length == 7u);
    ++stash_count;
}

void uz_inflate6502_init(UzInflate6502Read read, void *read_context,
                        UzInflate6502Write write, void *write_context,
                        unsigned char *input, unsigned int input_cap,
                        unsigned char *output, unsigned int output_cap,
                        const UzU32 *compressed_size,
                        const UzU32 *expected_output_size) {
    (void)read_context;
    (void)write_context;
    assert(input == (unsigned char *)0x0400u && input_cap == 512u);
    assert(output == (unsigned char *)0x0600u && output_cap == 508u);
    assert(compressed_size->lo == 11u && compressed_size->hi == 0u);
    assert(expected_output_size->lo == 7u && expected_output_size->hi == 0u);
    saved_read = read;
    saved_write = write;
}

unsigned char uz_inflate6502_run(void) {
    if (scenario == 3u) {
        codec_failure = UZ_INFLATE_TREE;
        return 0u;
    }
    if (saved_read(0, (unsigned char *)0x0400u, 11u) != 11) {
        codec_failure = UZ_INFLATE_IO;
        return 0u;
    }
    if (!saved_write(0, (const unsigned char *)0x0600u, 7u)) {
        codec_failure = UZ_INFLATE_IO;
        return 0u;
    }
    return 1u;
}

unsigned char uz_inflate6502_error(void) {
    return codec_failure;
}

const UzU32 *uz_inflate6502_output_size(void) {
    return &actual_size;
}

const UzCrc32 *uz_inflate6502_crc(void) {
    return &actual_crc;
}

static void reset_case(UzInflateJobRequest *request) {
    memset(request, 0, sizeof(*request));
    request->input_target = UZ_DOS_TARGET_READ;
    request->output_target = UZ_DOS_TARGET_WRITE;
    request->work_bank = 7u;
    request->compressed_size.lo = 11u;
    request->output_size.lo = 7u;
    request->expected_crc.byte[0] = 1u;
    request->expected_crc.byte[1] = 2u;
    request->expected_crc.byte[2] = 3u;
    request->expected_crc.byte[3] = 4u;
    actual_size = request->output_size;
    actual_crc = request->expected_crc;
    scenario = 0u;
    codec_failure = 0u;
    load_count = save_count = fetch_count = stash_count = 0u;
}

int main(void) {
    UzInflateJobRequest request;

    reset_case(&request);
    assert(uz_inflate_job_entry(&request));
    assert(uz_inflate_job_error() == UZ_INFLATE_JOB_OK);
    assert(load_count == 1u && save_count == 1u &&
           fetch_count == 1u && stash_count == 1u);

    reset_case(&request);
    scenario = 1u;
    assert(!uz_inflate_job_entry(&request));
    assert(uz_inflate_job_error() == UZ_INFLATE_JOB_INPUT_IO);

    reset_case(&request);
    scenario = 2u;
    assert(!uz_inflate_job_entry(&request));
    assert(uz_inflate_job_error() == UZ_INFLATE_JOB_OUTPUT_IO);

    reset_case(&request);
    scenario = 3u;
    assert(!uz_inflate_job_entry(&request));
    assert(uz_inflate_job_error() == UZ_INFLATE_JOB_CODEC);
    assert(uz_inflate_job_codec_error() == UZ_INFLATE_TREE);

    reset_case(&request);
    actual_size.lo = 6u;
    assert(!uz_inflate_job_entry(&request));
    assert(uz_inflate_job_error() == UZ_INFLATE_JOB_SIZE);

    reset_case(&request);
    actual_crc.byte[3] ^= 0x80u;
    assert(!uz_inflate_job_entry(&request));
    assert(uz_inflate_job_error() == UZ_INFLATE_JOB_CRC);

    reset_case(&request);
    request.compressed_size.lo = 0u;
    assert(!uz_inflate_job_entry(&request));
    assert(uz_inflate_job_error() == UZ_INFLATE_JOB_STATE);
    return 0;
}
