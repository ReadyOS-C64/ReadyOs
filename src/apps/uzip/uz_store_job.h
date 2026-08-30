#ifndef UZ_STORE_JOB_H
#define UZ_STORE_JOB_H

#include "uz_crc32.h"
#include "uz_u32.h"

#define UZ_STORE_JOB_OK        0u
#define UZ_STORE_JOB_STATE     1u
#define UZ_STORE_JOB_INPUT_IO  2u
#define UZ_STORE_JOB_OUTPUT_IO 3u
#define UZ_STORE_JOB_CRC       4u

typedef struct {
    unsigned char input_target;
    unsigned char output_target;
    unsigned char work_bank;
    UzU32 size;
    UzCrc32 expected_crc;
} UzStoreJobRequest;

/* Both Ultimate DOS targets must already be open and the archive target must
 * be positioned at the member data. Returns an exact UZ_STORE_JOB_* code. */
unsigned char uz_store_job_entry(const UzStoreJobRequest *request);

#endif
