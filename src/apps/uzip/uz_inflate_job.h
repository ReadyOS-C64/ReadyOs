#ifndef UZ_INFLATE_JOB_H
#define UZ_INFLATE_JOB_H

#include "uz_crc32.h"
#include "uz_u32.h"

#define UZ_INFLATE_JOB_OK          0u
#define UZ_INFLATE_JOB_STATE       1u
#define UZ_INFLATE_JOB_INPUT_IO    2u
#define UZ_INFLATE_JOB_OUTPUT_IO   3u
#define UZ_INFLATE_JOB_CODEC       4u
#define UZ_INFLATE_JOB_SIZE        5u
#define UZ_INFLATE_JOB_CRC         6u

/* The UI prepares both Ultimate DOS targets and seeks the archive target to
 * the member's first compressed byte before entering the destructive 32K
 * dictionary window. The overlay immediately copies this small request; it
 * never retains a pointer into UI memory after decoding starts. */
typedef struct {
    unsigned char input_target;
    unsigned char output_target;
    unsigned char work_bank;
    UzU32 compressed_size;
    UzU32 output_size;
    UzCrc32 expected_crc;
} UzInflateJobRequest;

/* Explicit packed-overlay entry. A zero result means the streamed member was
 * rejected; uz_inflate_job_error() distinguishes transport, codec, size, and
 * CRC failures while the overlay is still present. */
unsigned char uz_inflate_job_entry(const UzInflateJobRequest *request);
unsigned char uz_inflate_job_error(void);
unsigned char uz_inflate_job_codec_error(void);

#endif
