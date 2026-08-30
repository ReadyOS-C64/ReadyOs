#ifndef UZ_JOB_H
#define UZ_JOB_H

#include "uz_inflate_job.h"
#include "uz_store_job.h"

typedef unsigned char (*UzJobDeflateEntry)(unsigned char package_bank,
                                           unsigned char work_bank);
typedef unsigned char (*UzJobInflateEntry)(
    const UzInflateJobRequest *request);
typedef unsigned char (*UzJobStoreEntry)(const UzStoreJobRequest *request);

/* Run a packed coordinator while preserving the exact idle/UI bytes which
 * share its compressor workspace. Both banks must be ReadyOS-owned. The
 * callable entry is explicit because the coordinator image also contains
 * support routines and its linker run base is not necessarily a C entry. */
unsigned char uz_job_run_deflate(unsigned char package_bank,
                                 unsigned char work_bank,
                                 UzJobDeflateEntry entry);

/* Run the separately described create coordinator. Its image follows, but is
 * not part of, the frozen six-phase uZPK v7 extraction payload. */
unsigned char uz_job_run_create(unsigned char package_bank,
                                unsigned char work_bank,
                                unsigned int image_offset,
                                unsigned int image_size,
                                UzJobDeflateEntry entry);

/* Run the packed inflater after snapshotting the entire UI/dictionary window.
 * The request is consumed by the overlay before dictionary output can replace
 * it. Both banks must be distinct ReadyOS-owned allocations. */
unsigned char uz_job_run_inflate(unsigned char package_bank,
                                 unsigned char work_bank,
                                 UzJobInflateEntry entry,
                                 const UzInflateJobRequest *request);

/* The inflater's own accessors are part of the displaced $B000 image. These
 * resident getters return values captured before the UI snapshot is restored. */
unsigned char uz_job_inflate_error(void);
unsigned char uz_job_inflate_codec_error(void);

/* Load Store and its persistent coordinator together, stream one already-
 * opened member, and restore the exact UI window before returning. The exact
 * UZ_STORE_JOB_* result is returned. */
unsigned char uz_job_run_store(unsigned char package_bank,
                               unsigned char work_bank,
                               UzJobStoreEntry entry,
                               const UzStoreJobRequest *request);

#endif
