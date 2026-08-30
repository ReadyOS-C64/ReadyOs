#ifndef UZ_CREATE_JOB_H
#define UZ_CREATE_JOB_H

#include "uz_deflate.h"
#include "uz_dos.h"
#include "uz_zip_write.h"

#define UZ_CREATE_JOB_OK             0u
#define UZ_CREATE_JOB_STATE          1u
#define UZ_CREATE_JOB_FRAME          2u
#define UZ_CREATE_JOB_DEFLATE        3u
#define UZ_CREATE_JOB_CATALOG        4u
#define UZ_CREATE_JOB_INPUT_CHANGED  5u
#define UZ_CREATE_JOB_INPUT_CLOSE    6u
#define UZ_CREATE_JOB_OUTPUT_CLOSE   7u

/* The modal job owns screen/BASIC scratch while the visible TUI is absent.
 * Its request survives both the $3000-$C3FF snapshot and $B000 phase swaps. */
#define UZ_CREATE_JOB_REQUEST ((UzCreateJobRequest *)0x0800u)
#define UZ_CREATE_JOB_DEFLATE_STATE ((UzDeflate *)0x0400u)
#define UZ_CREATE_JOB_INPUT_DOS ((UzDos *)0x0480u)
#define UZ_CREATE_JOB_OUTPUT_DOS ((UzDos *)0x04D0u)
#define UZ_CREATE_JOB_INPUT_COMMAND ((unsigned char *)0x0560u)
#define UZ_CREATE_JOB_TOKEN_STAGE ((unsigned char *)0x059Cu)
#define UZ_CREATE_JOB_INPUT_DATA ((unsigned char *)0x05A0u)
#define UZ_CREATE_JOB_STATUS ((unsigned char *)0x05E0u)
#define UZ_CREATE_JOB_OUTPUT_COMMAND ((unsigned char *)0x0600u)
#define UZ_CREATE_JOB_OUTPUT_BUFFER ((unsigned char *)0x0604u)

#define UZ_CREATE_JOB_INPUT_COMMAND_CAP 64u
#define UZ_CREATE_JOB_DATA_CAP          64u
#define UZ_CREATE_JOB_STATUS_CAP        32u
#define UZ_CREATE_JOB_OUTPUT_COMMAND_CAP 512u
#define UZ_CREATE_JOB_TOKEN_STAGE_CAP    4u

typedef struct {
    unsigned char input_target;
    unsigned char output_target;
    unsigned char catalog_bank;
    unsigned char method;
    unsigned char directory;
    unsigned char first_entry;
    unsigned char last_entry;
    unsigned char error;
    unsigned char zip_error;
    unsigned char codec_error;
    unsigned int entry_index;
    unsigned int entry_count;
    UzU32 input_size;
    unsigned int job_offset;
    unsigned int job_size;
    unsigned int match_offset;
    unsigned int match_size;
    unsigned int emit_offset;
    unsigned int emit_size;
    char archive_name[UZ_ZIP_NAME_CAP];
} UzCreateJobRequest;

/* Entry address is inside the packed $A000 coordinator image. The caller
 * fills UZ_CREATE_JOB_REQUEST, opens the input when needed and the output on
 * the first entry, then invokes it through uz_job_run_deflate(). */
unsigned char uz_create_job_entry(unsigned char package_bank,
                                  unsigned char work_bank);

#endif
