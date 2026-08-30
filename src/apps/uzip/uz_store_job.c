#include "uz_store_job.h"

#include "uz_dos.h"

#include "../../lib/reu_mgr.h"

#ifdef __CC65__
#ifdef UZIP_READYOS_APP
#pragma code-name(push, "DEFLATE_COORD_CODE")
#pragma rodata-name(push, "DEFLATE_COORD_RODATA")
#pragma bss-name(push, "DEFLATE_COORD_BSS")
#endif
#endif

#define UZ_STORE_JOB_REU_OFFSET 0xA000u
#define UZ_STORE_JOB_REU_CHUNK  4096u
#ifdef UZ_STORE_JOB_HOST_TEST
unsigned char uz_store_job_host_buffer[512u];
#define UZ_STORE_JOB_CPU_BUFFER uz_store_job_host_buffer
#define UZ_STORE_JOB_CPU_ADDRESS 0x0400u
#else
#define UZ_STORE_JOB_CPU_BUFFER ((unsigned char *)0x0400u)
#define UZ_STORE_JOB_CPU_ADDRESS ((unsigned int)UZ_STORE_JOB_CPU_BUFFER)
#endif
#define UZ_STORE_JOB_CPU_CHUNK  512u
#define UZ_STORE_JOB_COMMAND_CAP 16u
#define UZ_STORE_JOB_DATA_CAP    64u
#define UZ_STORE_JOB_STATUS_CAP  16u

/* Single active job by design. Keeping transport state here avoids putting
 * more than 300 bytes on cc65's 512-byte software stack. */
static UzDos store_input;
static UzDos store_output;
static UzU32 store_remaining;
static UzCrc32 store_crc;
static unsigned char store_input_command[UZ_STORE_JOB_COMMAND_CAP];
static unsigned char store_output_command[UZ_STORE_JOB_COMMAND_CAP];
static unsigned char store_input_data[UZ_STORE_JOB_DATA_CAP];
static unsigned char store_output_data[UZ_STORE_JOB_DATA_CAP];
static unsigned char store_input_status[UZ_STORE_JOB_STATUS_CAP];
static unsigned char store_output_status[UZ_STORE_JOB_STATUS_CAP];
static unsigned int store_block;
static unsigned int store_at;
static unsigned int store_part;
static unsigned int store_transferred;

static unsigned int next_block(const UzU32 *remaining) {
    if (remaining->hi != 0u || remaining->lo > UZ_STORE_JOB_REU_CHUNK)
        return UZ_STORE_JOB_REU_CHUNK;
    return remaining->lo;
}

unsigned char uz_store_job_entry(const UzStoreJobRequest *request) {
    if (request == 0 || request->work_bank == 0xFFu ||
        request->input_target == request->output_target)
        return UZ_STORE_JOB_STATE;
    uz_dos_init(&store_input, request->input_target,
                store_input_command, UZ_STORE_JOB_COMMAND_CAP,
                store_input_data, UZ_STORE_JOB_DATA_CAP,
                store_input_status, UZ_STORE_JOB_STATUS_CAP);
    uz_dos_init(&store_output, request->output_target,
                store_output_command, UZ_STORE_JOB_COMMAND_CAP,
                store_output_data, UZ_STORE_JOB_DATA_CAP,
                store_output_status, UZ_STORE_JOB_STATUS_CAP);
    store_input.file_open = 1u;
    store_output.file_open = 1u;
    store_remaining = request->size;
    uz_crc32_init(&store_crc);
    while (store_remaining.hi != 0u || store_remaining.lo != 0u) {
        store_block = next_block(&store_remaining);
        store_transferred = 0u;
        if (!uz_dos_load_reu(&store_input, request->work_bank,
                             UZ_STORE_JOB_REU_OFFSET, store_block,
                             &store_transferred) ||
            store_transferred != store_block) return UZ_STORE_JOB_INPUT_IO;
        store_at = 0u;
        while (store_at < store_block) {
            store_part = (unsigned int)(store_block - store_at);
            if (store_part > UZ_STORE_JOB_CPU_CHUNK)
                store_part = UZ_STORE_JOB_CPU_CHUNK;
            reu_dma_fetch(UZ_STORE_JOB_CPU_ADDRESS,
                          request->work_bank,
                          (unsigned int)(UZ_STORE_JOB_REU_OFFSET + store_at),
                          store_part);
            uz_crc32_update(&store_crc, UZ_STORE_JOB_CPU_BUFFER, store_part);
            store_at = (unsigned int)(store_at + store_part);
        }
        store_transferred = 0u;
        if (!uz_dos_save_reu(&store_output, request->work_bank,
                             UZ_STORE_JOB_REU_OFFSET, store_block,
                             &store_transferred) ||
            store_transferred != store_block) return UZ_STORE_JOB_OUTPUT_IO;
        uz_u32_sub_u16(&store_remaining, store_block);
    }
    uz_crc32_finish(&store_crc);
    if (!uz_crc32_equal(&store_crc, &request->expected_crc))
        return UZ_STORE_JOB_CRC;
    return UZ_STORE_JOB_OK;
}

#if defined(__CC65__) && defined(UZIP_READYOS_APP)
#pragma bss-name(pop)
#pragma rodata-name(pop)
#pragma code-name(pop)
#endif
