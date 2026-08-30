/* Physical compressor coordinator. It executes at `$A000-$AFFF` while MATCH
 * and EMIT replace one another at `$B000-$C3FF`. All mutable state is in the
 * screen-RAM diagnostic window or the allocator-owned work bank. */

#include "xuzdeflate_diag_internal.h"

#include "uz_pack.h"
#include "uz_package.h"
#if defined(UZIP_XUZZIP8_DIAGNOSTIC) || defined(UZIP_XUZMULTI_DIAGNOSTIC)
#define XUZD_ARCHIVE_DIAGNOSTIC 1
#include "uz_catalog.h"
#include "uz_zip_write.h"
#endif
#include "../../lib/reu_mgr.h"

#include <string.h>

#ifdef __CC65__
#pragma code-name(push, "DEFLATE_COORD_CODE")
#pragma rodata-name(push, "DEFLATE_COORD_RODATA")
#endif

#define XUZD_JOB_RUN 0xB000u
#ifdef XUZD_ARCHIVE_DIAGNOSTIC
#define XUZD_REU_ZIP_STATE_OFFSET 0xBA00u
#define XUZD_ZIP_WRITER ((UzZipWriter *)XUZD_INPUT)
#define XUZD_ZIP_RECORD \
    ((UzZipRecord *)(XUZD_INPUT + sizeof(UzZipWriter)))
#define XUZD_ZIP_STATE_SIZE \
    ((unsigned int)(sizeof(UzZipWriter) + sizeof(UzZipRecord)))
#endif

static unsigned int get16(const unsigned char *data, unsigned char offset) {
    return (unsigned int)(data[offset] |
                          ((unsigned int)data[(unsigned char)(offset + 1u)] << 8u));
}

static void note_stack(void) {
    unsigned int current;

    current = xuzdeflate_stack_pointer();
    if (current < XUZD_CONTEXT->stack_low) XUZD_CONTEXT->stack_low = current;
}

static unsigned char descriptor_at(unsigned char phase,
                                   unsigned char field) {
    return (unsigned char)(UZ_PACKAGE_DESC_BASE +
                           phase * UZ_PACKAGE_DESC_SIZE + field);
}

static unsigned char package_valid(const unsigned char *header) {
    unsigned char phase;
    unsigned int cursor;
    unsigned int payload_size;
    unsigned int size;
    unsigned int run;

    if (header[0] != 0x55u || header[1] != 0x5Au ||
        header[2] != 0x50u || header[3] != 0x4Bu ||
        header[4] != UZ_PACKAGE_VERSION ||
        header[5] != UZ_PACKAGE_PHASE_COUNT ||
        get16(header, 6u) != UZ_PACKAGE_HEADER_SIZE) return 0u;
    payload_size = get16(header, 8u);
    if (payload_size < UZ_PACKAGE_HEADER_SIZE ||
        payload_size > UZ_PACKAGE_MAX_SIZE) return 0u;
    cursor = UZ_PACKAGE_HEADER_SIZE;
    for (phase = 0u; phase < UZ_PACKAGE_PHASE_COUNT; ++phase) {
        size = get16(header,
                     descriptor_at(phase, UZ_PACKAGE_FIELD_SIZE));
        run = get16(header,
                    descriptor_at(phase, UZ_PACKAGE_FIELD_RUN));
        if (get16(header,
                  descriptor_at(phase, UZ_PACKAGE_FIELD_OFFSET)) != cursor ||
            size == 0u || cursor > payload_size ||
            size > payload_size - cursor ||
            run != (phase == UZ_PACKAGE_PHASE_COORD ? 0xA000u : 0xB000u))
            return 0u;
        cursor = (unsigned int)(cursor + size);
    }
    if (cursor != payload_size) return 0u;

    XUZD_CONTEXT->package_version = header[4];
    XUZD_CONTEXT->package_phase_count = header[5];
    XUZD_CONTEXT->job_offset = get16(
        header, descriptor_at(UZ_PACKAGE_PHASE_JOB, UZ_PACKAGE_FIELD_OFFSET));
    XUZD_CONTEXT->job_size = get16(
        header, descriptor_at(UZ_PACKAGE_PHASE_JOB, UZ_PACKAGE_FIELD_SIZE));
    XUZD_CONTEXT->match_offset = get16(
        header, descriptor_at(UZ_PACKAGE_PHASE_MATCH, UZ_PACKAGE_FIELD_OFFSET));
    XUZD_CONTEXT->match_size = get16(
        header, descriptor_at(UZ_PACKAGE_PHASE_MATCH, UZ_PACKAGE_FIELD_SIZE));
    XUZD_CONTEXT->emit_offset = get16(
        header, descriptor_at(UZ_PACKAGE_PHASE_EMIT, UZ_PACKAGE_FIELD_OFFSET));
    XUZD_CONTEXT->emit_size = get16(
        header, descriptor_at(UZ_PACKAGE_PHASE_EMIT, UZ_PACKAGE_FIELD_SIZE));
    return (unsigned char)(XUZD_CONTEXT->job_size <= 0x1100u &&
        XUZD_CONTEXT->match_size <= 0x1000u &&
        XUZD_CONTEXT->emit_size <= 0x0D00u);
}

static void load_image(unsigned int package_offset, unsigned int size) {
    reu_dma_fetch(XUZD_JOB_RUN, XUZD_CONTEXT->package_bank,
                  package_offset, size);
}

static unsigned char load_phase(void *context, unsigned char phase) {
    (void)context;
    note_stack();
    if (phase == UZ_DEFLATE_PHASE_MATCH) {
        load_image(XUZD_CONTEXT->match_offset, XUZD_CONTEXT->match_size);
        ++XUZD_CONTEXT->phase_match_count;
    } else if (phase == UZ_DEFLATE_PHASE_EMIT) {
        load_image(XUZD_CONTEXT->emit_offset, XUZD_CONTEXT->emit_size);
        ++XUZD_CONTEXT->phase_emit_count;
    } else {
        return 0u;
    }
    XUZD_TRACE_STAGE = XUZD_TRACE_PHASE_AFTER;
    return 1u;
}

static int read_input(void *context, unsigned char *data,
                      unsigned int length) {
    unsigned int transferred;

    (void)context;
    note_stack();
    transferred = 0u;
    /* Direct transfers still use the shared async UCI state machine: it owns
     * synchronization, PUSH/ABORT, complete queue draining, DATA_ACC, and the
     * final quiet-IDLE wait. This callback supplies no timing assumptions. */
    if (!uz_dos_load_reu(XUZD_INPUT_DOS, XUZD_CONTEXT->work_bank,
                         XUZD_REU_INPUT_OFFSET, length, &transferred) ||
        transferred != length) return -1;
    reu_dma_fetch((unsigned int)data, XUZD_CONTEXT->work_bank,
                  XUZD_REU_INPUT_OFFSET, length);
    return (int)length;
}

static unsigned char write_output(void *context,
                                  const unsigned char *data,
                                  unsigned int length) {
    unsigned int transferred;

    (void)context;
    note_stack();
    reu_dma_stash((unsigned int)data, XUZD_CONTEXT->work_bank,
                  XUZD_REU_OUTPUT_OFFSET, length);
    /* As above, uz_dos_save_reu delegates the entire asynchronous protocol
     * contract to the one shared gateway and validates the returned count. */
    XUZD_TRACE_STAGE = XUZD_TRACE_WRITE_BEFORE;
    if (!uz_dos_save_reu(XUZD_OUTPUT_DOS, XUZD_CONTEXT->work_bank,
                         XUZD_REU_OUTPUT_OFFSET, length, &transferred)) return 0u;
    return (unsigned char)(transferred == length);
}

static unsigned char store_tokens(void *context, unsigned int offset,
                                  const unsigned char *data,
                                  unsigned int length) {
    (void)context;
    note_stack();
    if (offset > UZ_DEFLATE_TOKEN_MAX ||
        length > UZ_DEFLATE_TOKEN_MAX - offset) return 0u;
    reu_dma_stash((unsigned int)data, XUZD_CONTEXT->work_bank,
                  (unsigned int)(XUZD_REU_TOKEN_OFFSET + offset), length);
    return 1u;
}

static unsigned char load_tokens(void *context, unsigned int offset,
                                 unsigned char *data,
                                 unsigned int length) {
    (void)context;
    note_stack();
    if (offset > UZ_DEFLATE_TOKEN_MAX ||
        length > UZ_DEFLATE_TOKEN_MAX - offset) return 0u;
    reu_dma_fetch((unsigned int)data, XUZD_CONTEXT->work_bank,
                  (unsigned int)(XUZD_REU_TOKEN_OFFSET + offset), length);
    return 1u;
}

#ifdef XUZD_ARCHIVE_DIAGNOSTIC
static unsigned char catalog_stash(unsigned int index,
                                   const UzZipRecord *record) {
    unsigned int offset;

    if (index >= UZ_CATALOG_MAX_ENTRIES) return 0u;
    offset = (unsigned int)(index * (unsigned int)sizeof(UzZipRecord));
    reu_dma_stash((unsigned int)record, XUZD_CONTEXT->catalog_bank,
                  offset, sizeof(*record));
    return 1u;
}

static unsigned char catalog_fetch(unsigned int index, UzZipRecord *record) {
    unsigned int offset;

    if (index >= UZ_CATALOG_MAX_ENTRIES) return 0u;
    offset = (unsigned int)(index * (unsigned int)sizeof(UzZipRecord));
    reu_dma_fetch((unsigned int)record, XUZD_CONTEXT->catalog_bank,
                  offset, sizeof(*record));
    return 1u;
}
#endif

unsigned char xuzdeflate_coord_entry(unsigned char package_bank,
                                     unsigned char work_bank) {
    unsigned char compressed;
    unsigned char framed;
    unsigned char input_closed;
    unsigned char output_closed;
#if defined(UZIP_XUZZIP8_DIAGNOSTIC)
    UzU32 central_offset;
#elif defined(UZIP_XUZMULTI_DIAGNOSTIC)
    UzU32 central_offset;
    unsigned int catalog_index;
    int got;
#endif

    XUZD_CONTEXT->package_bank = package_bank;
    XUZD_CONTEXT->work_bank = work_bank;
    XUZD_CONTEXT->phase_match_count = 0u;
    XUZD_CONTEXT->phase_emit_count = 0u;
    framed = 1u;
    XUZD_CONTEXT->stack_initial = xuzdeflate_stack_watermark_init();
    XUZD_CONTEXT->stack_low = XUZD_CONTEXT->stack_initial;
    reu_dma_fetch((unsigned int)XUZD_OUTPUT_BUFFER, package_bank, 0u,
                  UZ_PACKAGE_HEADER_SIZE);
    if (!package_valid(XUZD_OUTPUT_BUFFER)) return 0u;

#ifdef UZIP_XUZMULTI_DIAGNOSTIC
    XUZD_RESULT[22] = 0u;
    /* The UI stages the relative archive name at $9800. Preserve it before
     * restoring the writer into that same bounded coordinator workspace. */
    strcpy((char *)XUZD_WORKSPACE, (const char *)XUZD_INPUT);
    if (XUZD_CONTEXT->first_entry) {
        uz_zip_writer_init(XUZD_ZIP_WRITER, XUZD_OUTPUT_DOS);
    } else {
        reu_dma_fetch((unsigned int)XUZD_ZIP_WRITER, work_bank,
                      XUZD_REU_ZIP_STATE_OFFSET, sizeof(UzZipWriter));
    }
    /* Every Store/read/framing call below delegates to the shared async UCI
     * transport. It owns idle synchronization, one asynchronous PUSH,
     * complete queue draining, DATA_ACC transitions, recovery, and the final
     * quiet-idle wait; this coordinator supplies no timing delays. */
    compressed = 1u;
    if (!uz_zip_begin_streamed(XUZD_ZIP_WRITER, XUZD_ZIP_RECORD,
                               (const char *)XUZD_WORKSPACE,
                               XUZD_CONTEXT->directory,
                               XUZD_CONTEXT->method)) {
        XUZD_RESULT[22] = XUZD_ZIP_WRITER->error;
        framed = 0u;
    } else if (XUZD_CONTEXT->method == 8u) {
        reu_dma_stash((unsigned int)XUZD_ZIP_WRITER, work_bank,
                      XUZD_REU_ZIP_STATE_OFFSET, XUZD_ZIP_STATE_SIZE);
        uz_deflate_init(XUZD_DEFLATE,
                        read_input, 0, write_output, 0,
                        store_tokens, load_tokens, 0,
                        load_phase, 0,
                        XUZD_WORKSPACE,
                        XUZD_INPUT, UZ_DEFLATE_BLOCK_SIZE,
                        XUZD_OUTPUT_BUFFER, UZ_DOS_WRITE_MAX,
                        XUZD_TOKEN_STAGE, XUZD_TOKEN_STAGE_CAP,
                        &XUZD_CONTEXT->input_size);
        compressed = uz_deflate_run(XUZD_DEFLATE);
        note_stack();
        load_image(XUZD_CONTEXT->job_offset, XUZD_CONTEXT->job_size);
        reu_dma_fetch((unsigned int)XUZD_ZIP_WRITER, work_bank,
                      XUZD_REU_ZIP_STATE_OFFSET, XUZD_ZIP_STATE_SIZE);
        if (compressed) {
            XUZD_ZIP_RECORD->size = XUZD_DEFLATE->input_size;
            XUZD_ZIP_RECORD->compressed_size = XUZD_DEFLATE->output_size;
            XUZD_ZIP_RECORD->crc = XUZD_DEFLATE->crc;
            framed = uz_zip_finish_deflate(XUZD_ZIP_WRITER);
        } else {
            framed = 0u;
        }
    } else if (XUZD_CONTEXT->method == 0u) {
        framed = 1u;
        if (!XUZD_CONTEXT->directory) {
            do {
                got = uz_dos_read(XUZD_INPUT_DOS, XUZD_OUTPUT_BUFFER,
                                  XUZD_DOS_DATA_CAP);
                if (got < 0 || (got != 0 &&
                    !uz_zip_store_data(XUZD_ZIP_WRITER,
                                       XUZD_OUTPUT_BUFFER,
                                       (unsigned int)got))) {
                    framed = 0u;
                    break;
                }
            } while (got != 0);
        }
        if (framed) framed = uz_zip_finish_store(XUZD_ZIP_WRITER);
    } else {
        framed = 0u;
    }

    if (framed && !catalog_stash(XUZD_CONTEXT->entry_index,
                                 XUZD_ZIP_RECORD)) {
        XUZD_RESULT[22] = 0x81u;
        framed = 0u;
    }
    if (framed && XUZD_CONTEXT->last_entry) {
        central_offset = XUZD_ZIP_WRITER->offset;
        for (catalog_index = 0u;
             catalog_index < XUZD_CONTEXT->entry_count; ++catalog_index) {
            if (!catalog_fetch(catalog_index, XUZD_ZIP_RECORD) ||
                !uz_zip_emit_central(XUZD_ZIP_WRITER, XUZD_ZIP_RECORD)) {
                XUZD_RESULT[22] = XUZD_ZIP_WRITER->error != 0u ?
                                  XUZD_ZIP_WRITER->error : 0x82u;
                framed = 0u;
                break;
            }
        }
        if (framed && !uz_zip_finish_archive(XUZD_ZIP_WRITER,
                                             &central_offset,
                                             XUZD_CONTEXT->entry_count)) {
            XUZD_RESULT[22] = XUZD_ZIP_WRITER->error;
            framed = 0u;
        }
    } else if (framed) {
        reu_dma_stash((unsigned int)XUZD_ZIP_WRITER, work_bank,
                      XUZD_REU_ZIP_STATE_OFFSET, sizeof(UzZipWriter));
    }
    if (!framed && XUZD_RESULT[22] == 0u)
        XUZD_RESULT[22] = XUZD_ZIP_WRITER->error != 0u ?
                          XUZD_ZIP_WRITER->error : 0x83u;

    input_closed = XUZD_CONTEXT->directory ? 1u :
                   uz_dos_job_close(XUZD_INPUT_DOS);
    output_closed = XUZD_CONTEXT->last_entry ?
                    uz_dos_job_close(XUZD_OUTPUT_DOS) : 1u;
#else
#ifdef UZIP_XUZZIP8_DIAGNOSTIC
    XUZD_RESULT[22] = 0u;
    uz_zip_writer_init(XUZD_ZIP_WRITER, XUZD_OUTPUT_DOS);
    if (!uz_zip_begin_deflate(XUZD_ZIP_WRITER, XUZD_ZIP_RECORD,
                              XUZD_CONTEXT->member_name)) {
        XUZD_RESULT[22] = XUZD_ZIP_WRITER->error;
        (void)uz_dos_job_close(XUZD_INPUT_DOS);
        (void)uz_dos_job_close(XUZD_OUTPUT_DOS);
        return 0u;
    }
    reu_dma_stash((unsigned int)XUZD_ZIP_WRITER, work_bank,
                  XUZD_REU_ZIP_STATE_OFFSET, XUZD_ZIP_STATE_SIZE);
#endif

    uz_deflate_init(XUZD_DEFLATE,
                    read_input, 0, write_output, 0,
                    store_tokens, load_tokens, 0,
                    load_phase, 0,
                    XUZD_WORKSPACE,
                    XUZD_INPUT, UZ_DEFLATE_BLOCK_SIZE,
                    XUZD_OUTPUT_BUFFER, UZ_DOS_WRITE_MAX,
                    XUZD_TOKEN_STAGE, XUZD_TOKEN_STAGE_CAP,
                    &XUZD_CONTEXT->input_size);
    compressed = uz_deflate_run(XUZD_DEFLATE);
    note_stack();

    /* Restore the Store/job image before close: open/close deliberately live
     * with ZIP container I/O, not the mutually exclusive codec images. */
    load_image(XUZD_CONTEXT->job_offset, XUZD_CONTEXT->job_size);
#ifdef XUZD_ARCHIVE_DIAGNOSTIC
    reu_dma_fetch((unsigned int)XUZD_ZIP_WRITER, work_bank,
                  XUZD_REU_ZIP_STATE_OFFSET, XUZD_ZIP_STATE_SIZE);
    if (compressed) {
        XUZD_ZIP_RECORD->size = XUZD_DEFLATE->input_size;
        XUZD_ZIP_RECORD->compressed_size = XUZD_DEFLATE->output_size;
        XUZD_ZIP_RECORD->crc = XUZD_DEFLATE->crc;
        framed = uz_zip_finish_deflate(XUZD_ZIP_WRITER);
        if (framed && !catalog_stash(0u, XUZD_ZIP_RECORD)) {
            XUZD_RESULT[22] = 0x81u;
            framed = 0u;
        }
        central_offset = XUZD_ZIP_WRITER->offset;
        if (framed && !catalog_fetch(0u, XUZD_ZIP_RECORD)) {
            XUZD_RESULT[22] = 0x82u;
            framed = 0u;
        }
        framed = (unsigned char)(framed &&
            uz_zip_emit_central(XUZD_ZIP_WRITER, XUZD_ZIP_RECORD) &&
            uz_zip_finish_archive(XUZD_ZIP_WRITER, &central_offset, 1u));
        if (!framed) XUZD_RESULT[22] = XUZD_ZIP_WRITER->error;
    } else {
        framed = 0u;
    }
#endif
    input_closed = uz_dos_job_close(XUZD_INPUT_DOS);
    output_closed = uz_dos_job_close(XUZD_OUTPUT_DOS);
#endif
    note_stack();
    {
        unsigned int watermark_low;

        watermark_low = xuzdeflate_stack_watermark_low();
        if (watermark_low < XUZD_CONTEXT->stack_low)
            XUZD_CONTEXT->stack_low = watermark_low;
    }
    return (unsigned char)(compressed && framed && input_closed && output_closed &&
                           XUZD_WORKSPACE_GUARD == 0xA5u &&
                           XUZD_CONTEXT->stack_low >= 0xC400u);
}

#ifdef __CC65__
#pragma rodata-name(pop)
#pragma code-name(pop)
#endif
