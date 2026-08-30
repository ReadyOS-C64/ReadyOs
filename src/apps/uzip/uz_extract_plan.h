#ifndef UZ_EXTRACT_PLAN_H
#define UZ_EXTRACT_PLAN_H

#include "uz_zip_write.h"

#define UZ_EXTRACT_PLAN_OK       0u
#define UZ_EXTRACT_PLAN_STATE    1u
#define UZ_EXTRACT_PLAN_PATH     2u
#define UZ_EXTRACT_PLAN_OPEN     3u
#define UZ_EXTRACT_PLAN_INFO     4u
#define UZ_EXTRACT_PLAN_PARSE    5u
#define UZ_EXTRACT_PLAN_FULL     6u
#define UZ_EXTRACT_PLAN_CLOSE    7u

typedef struct {
    UzZipRecord record;
    UzU32 data_offset;
} UzExtractPlanEntry;

/* Preflight the complete archive before any destination is created. Records
 * and verified local-data offsets are stored consecutively in catalog_bank.
 * The parser runs as the frozen $B000 reader overlay while the exact UI image
 * is snapshotted in work_bank. */
unsigned char uz_extract_plan_build(unsigned char package_bank,
                                    unsigned char work_bank,
                                    unsigned char catalog_bank,
                                    const char *archive_path);
unsigned int uz_extract_plan_count(void);
unsigned char uz_extract_plan_error(void);

#endif
