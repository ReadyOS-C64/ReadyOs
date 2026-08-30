#ifndef UZ_EXTRACT_FS_H
#define UZ_EXTRACT_FS_H

#include "uz_job.h"
#include "uz_zip_write.h"

#define UZ_EXTRACT_OK          0u
#define UZ_EXTRACT_STATE       1u
#define UZ_EXTRACT_PATH        2u
#define UZ_EXTRACT_DIRECTORY   3u
#define UZ_EXTRACT_TEMP        4u
#define UZ_EXTRACT_SEEK        5u
#define UZ_EXTRACT_JOB         6u
#define UZ_EXTRACT_CLOSE       7u
#define UZ_EXTRACT_COMMIT      8u

#define UZ_EXTRACT_TEMP_CAP 12u

typedef struct {
    char component[UZ_ZIP_NAME_CAP];
    char leaf[UZ_ZIP_NAME_CAP];
    char temp[UZ_EXTRACT_TEMP_CAP];
    unsigned char error;
    unsigned char job_error;
    unsigned char temp_open;
    unsigned char temp_created;
} UzExtractFs;

void uz_extract_fs_init(UzExtractFs *state);

/* Extract one already-preflighted member below an absolute destination root.
 * Existing destination files are never deleted or overwritten. A file is
 * visible under its final name only after codec size/CRC validation, close,
 * and successful rename of a unique temporary sibling. */
unsigned char uz_extract_member(UzExtractFs *state,
                                UzDos *archive,
                                UzDos *output,
                                const char *destination_root,
                                const UzZipRecord *record,
                                const UzU32 *data_offset,
                                unsigned char package_bank,
                                unsigned char work_bank);

#endif
