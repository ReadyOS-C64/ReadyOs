#include "uz_inflate_job.h"

#include "uz_dos.h"
#include "uz_inflate6502.h"

#include "../../lib/reu_mgr.h"

#ifdef __CC65__
#ifdef UZIP_READYOS_APP
#pragma code-name(push, "INFLATE_CODE")
#pragma bss-name(push, "INFLATE_BSS")
#endif
#endif

#define UZ_INFLATE_JOB_INPUT_BUFFER  ((unsigned char *)0x0400u)
#define UZ_INFLATE_JOB_OUTPUT_BUFFER ((unsigned char *)0x0600u)
#define UZ_INFLATE_JOB_INPUT_CAP     512u
#define UZ_INFLATE_JOB_OUTPUT_CAP    508u
#define UZ_INFLATE_JOB_STAGE_OFFSET  0xA000u
#define UZ_INFLATE_JOB_COMMAND_CAP   16u
#define UZ_INFLATE_JOB_DATA_CAP      64u
#define UZ_INFLATE_JOB_STATUS_CAP    16u

#ifdef __CC65__
#define UZ_INFLATE_CPU_ADDR(pointer) ((unsigned int)(pointer))
#else
/* Host boundary tests use the same 16-bit C64 addresses without asking the
 * compiler to treat a native pointer as though it were 16 bits wide. */
#define UZ_INFLATE_CPU_ADDR(pointer) \
    ((unsigned int)(unsigned long)(pointer))
#endif

static UzInflateJobRequest active_request;
static UzDos input_dos;
static UzDos output_dos;
static unsigned char input_command[UZ_INFLATE_JOB_COMMAND_CAP];
static unsigned char output_command[UZ_INFLATE_JOB_COMMAND_CAP];
static unsigned char input_data[UZ_INFLATE_JOB_DATA_CAP];
static unsigned char output_data[UZ_INFLATE_JOB_DATA_CAP];
static unsigned char input_status[UZ_INFLATE_JOB_STATUS_CAP];
static unsigned char output_status[UZ_INFLATE_JOB_STATUS_CAP];
static unsigned char job_error;
static unsigned char codec_error;

static unsigned char u32_same(const UzU32 *left, const UzU32 *right) {
    return (unsigned char)(left->lo == right->lo && left->hi == right->hi);
}

static unsigned char crc_same(const UzCrc32 *left, const UzCrc32 *right) {
    return (unsigned char)(left->byte[0] == right->byte[0] &&
                           left->byte[1] == right->byte[1] &&
                           left->byte[2] == right->byte[2] &&
                           left->byte[3] == right->byte[3]);
}

static int inflate_job_read(void *context, unsigned char *destination,
                            unsigned int length) {
    unsigned int transferred;

    (void)context;
    transferred = 0u;
    if (length == 0u || length > UZ_INFLATE_JOB_INPUT_CAP ||
        !uz_dos_load_reu(&input_dos, active_request.work_bank,
                         UZ_INFLATE_JOB_STAGE_OFFSET, length, &transferred) ||
        transferred != length) {
        job_error = UZ_INFLATE_JOB_INPUT_IO;
        return -1;
    }
    reu_dma_fetch(UZ_INFLATE_CPU_ADDR(destination), active_request.work_bank,
                  UZ_INFLATE_JOB_STAGE_OFFSET, length);
    return (int)length;
}

static unsigned char inflate_job_write(void *context,
                                       const unsigned char *source,
                                       unsigned int length) {
    unsigned int transferred;

    (void)context;
    transferred = 0u;
    if (length == 0u || length > UZ_INFLATE_JOB_OUTPUT_CAP) {
        job_error = UZ_INFLATE_JOB_OUTPUT_IO;
        return 0u;
    }
    reu_dma_stash(UZ_INFLATE_CPU_ADDR(source), active_request.work_bank,
                  UZ_INFLATE_JOB_STAGE_OFFSET, length);
    if (!uz_dos_save_reu(&output_dos, active_request.work_bank,
                         UZ_INFLATE_JOB_STAGE_OFFSET, length, &transferred) ||
        transferred != length) {
        job_error = UZ_INFLATE_JOB_OUTPUT_IO;
        return 0u;
    }
    return 1u;
}

unsigned char uz_inflate_job_entry(const UzInflateJobRequest *request) {
    if (request == 0 || request->work_bank == 0xFFu ||
        request->input_target == request->output_target ||
        (request->compressed_size.hi == 0u &&
         request->compressed_size.lo == 0u)) {
        job_error = UZ_INFLATE_JOB_STATE;
        return 0u;
    }

    /* Copy before the first output byte claims dictionary RAM. In production
     * request points into the UI window which is intentionally destroyed. */
    active_request = *request;
    job_error = UZ_INFLATE_JOB_OK;
    codec_error = 0u;
    uz_dos_init(&input_dos, active_request.input_target,
                input_command, sizeof(input_command),
                input_data, sizeof(input_data),
                input_status, sizeof(input_status));
    uz_dos_init(&output_dos, active_request.output_target,
                output_command, sizeof(output_command),
                output_data, sizeof(output_data),
                output_status, sizeof(output_status));
    /* The UI opened both files before its memory was snapshotted. These local
     * handles describe those same Ultimate DOS target sessions and never own
     * close/rename policy. */
    input_dos.file_open = 1u;
    output_dos.file_open = 1u;

    uz_inflate6502_init(inflate_job_read, 0, inflate_job_write, 0,
                        UZ_INFLATE_JOB_INPUT_BUFFER,
                        UZ_INFLATE_JOB_INPUT_CAP,
                        UZ_INFLATE_JOB_OUTPUT_BUFFER,
                        UZ_INFLATE_JOB_OUTPUT_CAP,
                        &active_request.compressed_size,
                        &active_request.output_size);
    if (!uz_inflate6502_run()) {
        codec_error = uz_inflate6502_error();
        if (job_error == UZ_INFLATE_JOB_OK)
            job_error = UZ_INFLATE_JOB_CODEC;
        return 0u;
    }
    if (!u32_same(uz_inflate6502_output_size(), &active_request.output_size)) {
        job_error = UZ_INFLATE_JOB_SIZE;
        return 0u;
    }
    if (!crc_same(uz_inflate6502_crc(), &active_request.expected_crc)) {
        job_error = UZ_INFLATE_JOB_CRC;
        return 0u;
    }
    return 1u;
}

unsigned char uz_inflate_job_error(void) {
    return job_error;
}

unsigned char uz_inflate_job_codec_error(void) {
    return codec_error;
}

#if defined(__CC65__) && defined(UZIP_READYOS_APP)
#pragma bss-name(pop)
#pragma code-name(pop)
#endif
