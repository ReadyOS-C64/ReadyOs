#ifndef XUZDEFLATE_DIAG_INTERNAL_H
#define XUZDEFLATE_DIAG_INTERNAL_H

#include "uz_deflate.h"
#include "uz_dos.h"

#define XUZD_RESULT ((volatile unsigned char *)0x033Cu)
#define XUZD_TRACE_STAGE XUZD_RESULT[124]
#define XUZD_TRACE_DETAIL XUZD_RESULT[125]
#define XUZD_TRACE_AUX_LO XUZD_RESULT[126]
#define XUZD_TRACE_AUX_HI XUZD_RESULT[127]
#define XUZD_DEFLATE ((UzDeflate *)0x0400u)
#define XUZD_INPUT_DOS ((UzDos *)0x0480u)
#define XUZD_OUTPUT_DOS ((UzDos *)0x04D0u)
#define XUZD_CONTEXT ((XuzDeflateContext *)0x0520u)
#define XUZD_INPUT_COMMAND ((unsigned char *)0x0560u)
#define XUZD_TOKEN_STAGE ((unsigned char *)0x059Cu)
#define XUZD_INPUT_DATA ((unsigned char *)0x05A0u)
#define XUZD_SHARED_STATUS ((unsigned char *)0x05E0u)
#define XUZD_OUTPUT_COMMAND ((unsigned char *)0x0600u)
#define XUZD_OUTPUT_BUFFER ((unsigned char *)0x0604u)

#define XUZD_DOS_COMMAND_CAP 64u
#define XUZD_DOS_DATA_CAP 64u
#define XUZD_DOS_STATUS_CAP 32u
#define XUZD_OUTPUT_COMMAND_CAP 512u
#define XUZD_TOKEN_STAGE_CAP 4u

#define XUZD_WORKSPACE ((unsigned char *)0x3000u)
#define XUZD_INPUT ((unsigned char *)0x9800u)
#define XUZD_WORKSPACE_GUARD (*(volatile unsigned char *)0x2FFFu)

#define XUZD_REU_TOKEN_OFFSET 0xA000u
#define XUZD_REU_INPUT_OFFSET 0xB000u
#define XUZD_REU_OUTPUT_OFFSET 0xB800u

#define XUZD_STAGE_READY 1u
#define XUZD_STAGE_BANKS 2u
#define XUZD_STAGE_IDENTIFY 3u
#define XUZD_STAGE_OWNER 4u
#define XUZD_STAGE_PATHS 5u
#define XUZD_STAGE_COMPRESS 6u
#define XUZD_STAGE_DONE 7u

/* Persistent coordinator trace values survive the shared UI snapshot and are
 * sampled only after the physical plan's finite REST-silent interval. */
#define XUZD_TRACE_ENTRY          0x10u
#define XUZD_TRACE_HEADER         0x11u
#define XUZD_TRACE_INITIALIZED    0x12u
#define XUZD_TRACE_RUN            0x13u
#define XUZD_TRACE_PHASE_BEFORE   0x20u
#define XUZD_TRACE_PHASE_AFTER    0x21u
#define XUZD_TRACE_READ_BEFORE    0x30u
#define XUZD_TRACE_READ_AFTER     0x31u
#define XUZD_TRACE_WRITE_STASH    0x40u
#define XUZD_TRACE_WRITE_BEFORE   0x41u
#define XUZD_TRACE_WRITE_AFTER    0x42u
#define XUZD_TRACE_TOKEN_STORE    0x50u
#define XUZD_TRACE_TOKEN_LOAD     0x51u
#define XUZD_TRACE_RUN_RETURNED   0x60u
#define XUZD_TRACE_STORE_RELOAD   0x61u
#define XUZD_TRACE_CLOSED         0x62u

typedef struct {
    unsigned char package_bank;
    unsigned char work_bank;
#if defined(UZIP_XUZZIP8_DIAGNOSTIC) || defined(UZIP_XUZMULTI_DIAGNOSTIC)
    unsigned char catalog_bank;
#endif
    unsigned char package_version;
    unsigned char package_phase_count;
    unsigned int job_offset;
    unsigned int job_size;
    unsigned int match_offset;
    unsigned int match_size;
    unsigned int emit_offset;
    unsigned int emit_size;
    UzU32 input_size;
    unsigned int stack_initial;
    unsigned int stack_low;
    unsigned char phase_match_count;
    unsigned char phase_emit_count;
#ifdef UZIP_XUZMULTI_DIAGNOSTIC
    unsigned char entry_index;
    unsigned char entry_count;
    unsigned char method;
    unsigned char directory;
    unsigned char first_entry;
    unsigned char last_entry;
#else
    char member_name[11];
#endif
} XuzDeflateContext;

unsigned char xuzdeflate_coord_entry(unsigned char package_bank,
                                     unsigned char work_bank);
unsigned int xuzdeflate_stack_pointer(void);
unsigned int xuzdeflate_stack_watermark_init(void);
unsigned int xuzdeflate_stack_watermark_low(void);

#endif
