#include <assert.h>
#include <string.h>

#include "uz_store_job.h"
#include "uz_dos.h"

#define TEST_SIZE 9000u

extern unsigned char uz_store_job_host_buffer[512u];

static unsigned char source_data[TEST_SIZE];
static unsigned char output_data[TEST_SIZE];
static unsigned char reu_stage[4096u];
static unsigned int source_at;
static unsigned int output_at;
static unsigned int stage_length;
static unsigned int load_calls;
static unsigned int save_calls;
static unsigned int fetch_calls;
static unsigned char fail_load;
static unsigned char fail_save;

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
    assert(dos->file_open && bank == 9u && offset == 0xA000u);
    assert(length != 0u && length <= sizeof(reu_stage));
    if (command == UZ_DOS_REU_LOAD) {
        ++load_calls;
        if (fail_load) return 0u;
        assert(source_at + length <= TEST_SIZE);
        memcpy(reu_stage, source_data + source_at, length);
        source_at = (unsigned int)(source_at + length);
        stage_length = length;
    } else {
        assert(command == UZ_DOS_REU_SAVE);
        ++save_calls;
        if (fail_save) return 0u;
        assert(length == stage_length && output_at + length <= TEST_SIZE);
        memcpy(output_data + output_at, reu_stage, length);
        output_at = (unsigned int)(output_at + length);
    }
    *transferred = length;
    return 1u;
}

void reu_dma_fetch(unsigned int c64_addr, unsigned char bank,
                   unsigned int reu_offset, unsigned int length) {
    unsigned int relative;

    assert(c64_addr == 0x0400u && bank == 9u);
    assert(reu_offset >= 0xA000u);
    relative = (unsigned int)(reu_offset - 0xA000u);
    assert(relative + length <= stage_length && length <= 512u);
    memcpy(uz_store_job_host_buffer, reu_stage + relative, length);
    ++fetch_calls;
}

void reu_dma_stash(unsigned int c64_addr, unsigned char bank,
                   unsigned int reu_offset, unsigned int length) {
    (void)c64_addr;
    (void)bank;
    (void)reu_offset;
    (void)length;
    assert(0);
}

static void reset_transport(void) {
    memset(output_data, 0xA5, sizeof(output_data));
    memset(reu_stage, 0, sizeof(reu_stage));
    source_at = output_at = stage_length = 0u;
    load_calls = save_calls = fetch_calls = 0u;
    fail_load = fail_save = 0u;
}

int main(void) {
    UzStoreJobRequest request;
    UzCrc32 crc;
    unsigned int index;

    for (index = 0u; index < TEST_SIZE; ++index)
        source_data[index] = (unsigned char)(index * 37u + index / 11u);
    memset(&request, 0, sizeof(request));
    request.input_target = UZ_DOS_TARGET_READ;
    request.output_target = UZ_DOS_TARGET_WRITE;
    request.work_bank = 9u;
    request.size.lo = TEST_SIZE;
    uz_crc32_init(&crc);
    uz_crc32_update(&crc, source_data, TEST_SIZE);
    uz_crc32_finish(&crc);
    request.expected_crc = crc;

    reset_transport();
    assert(uz_store_job_entry(&request) == UZ_STORE_JOB_OK);
    assert(source_at == TEST_SIZE && output_at == TEST_SIZE);
    assert(load_calls == 3u && save_calls == 3u && fetch_calls == 18u);
    assert(memcmp(source_data, output_data, TEST_SIZE) == 0);

    reset_transport();
    fail_load = 1u;
    assert(uz_store_job_entry(&request) == UZ_STORE_JOB_INPUT_IO);
    assert(output_at == 0u);

    reset_transport();
    fail_save = 1u;
    assert(uz_store_job_entry(&request) == UZ_STORE_JOB_OUTPUT_IO);
    assert(output_at == 0u);

    reset_transport();
    request.expected_crc.byte[0] ^= 1u;
    assert(uz_store_job_entry(&request) == UZ_STORE_JOB_CRC);
    assert(output_at == TEST_SIZE);
    request.expected_crc.byte[0] ^= 1u;

    reset_transport();
    request.size.lo = 0u;
    uz_crc32_init(&request.expected_crc);
    uz_crc32_finish(&request.expected_crc);
    assert(uz_store_job_entry(&request) == UZ_STORE_JOB_OK);
    assert(load_calls == 0u && save_calls == 0u && fetch_calls == 0u);

    request.work_bank = 0xFFu;
    assert(uz_store_job_entry(&request) == UZ_STORE_JOB_STATE);
    return 0;
}
