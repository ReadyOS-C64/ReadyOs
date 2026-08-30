#include "uz_create_job.h"

#include "uz_catalog.h"
#include "uz_u32.h"
#include "../../lib/reu_mgr.h"

#ifdef __CC65__
#pragma code-name(push, "CREATE_COORD_CODE")
#pragma rodata-name(push, "CREATE_COORD_RODATA")
#endif

#define UZ_CREATE_JOB_RUN 0xB000u
#define UZ_CREATE_JOB_WORKSPACE ((unsigned char *)0x3000u)
#define UZ_CREATE_JOB_INPUT ((unsigned char *)0x9800u)
#define UZ_CREATE_JOB_GUARD (*(volatile unsigned char *)0x2FFFu)

#define UZ_CREATE_JOB_REU_TOKEN_OFFSET  0xA000u
#define UZ_CREATE_JOB_REU_INPUT_OFFSET  0xB000u
#define UZ_CREATE_JOB_REU_OUTPUT_OFFSET 0xB800u
#define UZ_CREATE_JOB_REU_STATE_OFFSET  0xBA00u

#define UZ_CREATE_JOB_WRITER ((UzZipWriter *)UZ_CREATE_JOB_INPUT)
#define UZ_CREATE_JOB_RECORD \
    ((UzZipRecord *)(UZ_CREATE_JOB_INPUT + sizeof(UzZipWriter)))
#define UZ_CREATE_JOB_STATE_SIZE \
    ((unsigned int)(sizeof(UzZipWriter) + sizeof(UzZipRecord)))

static void set_error(unsigned char error) {
    if (UZ_CREATE_JOB_REQUEST->error == UZ_CREATE_JOB_OK)
        UZ_CREATE_JOB_REQUEST->error = error;
}

/* package_bank is intentionally supplied through the phase callback context;
 * the catalog bank remains dedicated to the frozen/final records. */
static unsigned char load_phase(void *context, unsigned char phase) {
    unsigned char package_bank;

    package_bank = *(unsigned char *)context;
    if (phase == UZ_DEFLATE_PHASE_MATCH) {
        reu_dma_fetch(UZ_CREATE_JOB_RUN, package_bank,
                      UZ_CREATE_JOB_REQUEST->match_offset,
                      UZ_CREATE_JOB_REQUEST->match_size);
    } else if (phase == UZ_DEFLATE_PHASE_EMIT) {
        reu_dma_fetch(UZ_CREATE_JOB_RUN, package_bank,
                      UZ_CREATE_JOB_REQUEST->emit_offset,
                      UZ_CREATE_JOB_REQUEST->emit_size);
    } else {
        return 0u;
    }
    return 1u;
}

static int read_input(void *context, unsigned char *destination,
                      unsigned int length) {
    unsigned int transferred;
    unsigned char work_bank;

    work_bank = *(unsigned char *)context;
    transferred = 0u;
    /* Direct input uses the shared asynchronous UCI state machine. It owns
     * idle synchronization, one PUSH, full queue draining, DATA_ACC,
     * recovery, and final quiet idle; this callback adds no timing delay. */
    if (!uz_dos_load_reu(UZ_CREATE_JOB_INPUT_DOS, work_bank,
                         UZ_CREATE_JOB_REU_INPUT_OFFSET, length,
                         &transferred) || transferred != length) return -1;
    reu_dma_fetch((unsigned int)destination, work_bank,
                  UZ_CREATE_JOB_REU_INPUT_OFFSET, length);
    return (int)length;
}

static unsigned char write_output(void *context,
                                  const unsigned char *source,
                                  unsigned int length) {
    unsigned int transferred;
    unsigned char work_bank;

    work_bank = *(unsigned char *)context;
    reu_dma_stash((unsigned int)source, work_bank,
                  UZ_CREATE_JOB_REU_OUTPUT_OFFSET, length);
    transferred = 0u;
    /* Direct output has the same complete async transport ownership as the
     * input callback and validates the firmware's exact transferred count. */
    return (unsigned char)(uz_dos_save_reu(UZ_CREATE_JOB_OUTPUT_DOS,
        work_bank, UZ_CREATE_JOB_REU_OUTPUT_OFFSET, length, &transferred) &&
        transferred == length);
}

static unsigned char store_tokens(void *context, unsigned int offset,
                                  const unsigned char *source,
                                  unsigned int length) {
    unsigned char work_bank;

    work_bank = *(unsigned char *)context;
    if (offset > UZ_DEFLATE_TOKEN_MAX ||
        length > UZ_DEFLATE_TOKEN_MAX - offset) return 0u;
    reu_dma_stash((unsigned int)source, work_bank,
                  (unsigned int)(UZ_CREATE_JOB_REU_TOKEN_OFFSET + offset),
                  length);
    return 1u;
}

static unsigned char load_tokens(void *context, unsigned int offset,
                                 unsigned char *destination,
                                 unsigned int length) {
    unsigned char work_bank;

    work_bank = *(unsigned char *)context;
    if (offset > UZ_DEFLATE_TOKEN_MAX ||
        length > UZ_DEFLATE_TOKEN_MAX - offset) return 0u;
    reu_dma_fetch((unsigned int)destination, work_bank,
                  (unsigned int)(UZ_CREATE_JOB_REU_TOKEN_OFFSET + offset),
                  length);
    return 1u;
}

static unsigned char catalog_stash(unsigned int index,
                                   const UzZipRecord *record) {
    unsigned int offset;

    if (index >= UZ_CATALOG_MAX_ENTRIES) return 0u;
    offset = (unsigned int)(index * (unsigned int)sizeof(UzZipRecord));
    reu_dma_stash((unsigned int)record,
                  UZ_CREATE_JOB_REQUEST->catalog_bank,
                  offset, sizeof(*record));
    return 1u;
}

static unsigned char catalog_fetch(unsigned int index, UzZipRecord *record) {
    unsigned int offset;

    if (index >= UZ_CATALOG_MAX_ENTRIES) return 0u;
    offset = (unsigned int)(index * (unsigned int)sizeof(UzZipRecord));
    reu_dma_fetch((unsigned int)record,
                  UZ_CREATE_JOB_REQUEST->catalog_bank,
                  offset, sizeof(*record));
    return 1u;
}

static unsigned char request_valid(unsigned char package_bank,
                                   unsigned char work_bank) {
    UzCreateJobRequest *request;

    request = UZ_CREATE_JOB_REQUEST;
    return (unsigned char)(package_bank != 0xFFu && work_bank != 0xFFu &&
        request->catalog_bank != 0xFFu && package_bank != work_bank &&
        package_bank != request->catalog_bank &&
        work_bank != request->catalog_bank &&
        request->entry_count != 0u &&
        request->entry_count <= UZ_CATALOG_MAX_ENTRIES &&
        request->entry_index < request->entry_count &&
        request->first_entry == (unsigned char)(request->entry_index == 0u) &&
        request->last_entry ==
            (unsigned char)(request->entry_index + 1u == request->entry_count) &&
        (request->method == 0u || request->method == 8u) &&
        (!request->directory || request->method == 0u) &&
        uz_zip_name_safe(request->archive_name, request->directory) &&
        request->job_offset != 0u && request->job_size != 0u &&
        request->job_size <= 0x1400u &&
        request->match_offset != 0u && request->match_size != 0u &&
        request->match_size <= 0x1000u &&
        request->emit_offset != 0u && request->emit_size != 0u &&
        request->emit_size <= 0x0D00u);
}

static unsigned char store_member(void) {
    int got;

    if (!UZ_CREATE_JOB_REQUEST->directory) {
        do {
            got = uz_dos_read(UZ_CREATE_JOB_INPUT_DOS,
                              UZ_CREATE_JOB_OUTPUT_BUFFER,
                              UZ_CREATE_JOB_DATA_CAP);
            if (got < 0 || (got != 0 &&
                !uz_zip_store_data(UZ_CREATE_JOB_WRITER,
                    UZ_CREATE_JOB_OUTPUT_BUFFER, (unsigned int)got)))
                return 0u;
        } while (got != 0);
    }
    if (!uz_zip_finish_store(UZ_CREATE_JOB_WRITER)) return 0u;
    if (!UZ_CREATE_JOB_REQUEST->directory &&
        (UZ_CREATE_JOB_RECORD->size.lo !=
             UZ_CREATE_JOB_REQUEST->input_size.lo ||
         UZ_CREATE_JOB_RECORD->size.hi !=
             UZ_CREATE_JOB_REQUEST->input_size.hi)) {
        set_error(UZ_CREATE_JOB_INPUT_CHANGED);
        return 0u;
    }
    return 1u;
}

static unsigned char deflate_member(unsigned char package_bank,
                                    unsigned char work_bank) {
    int trailing;
    unsigned char compressed;

    /* Absolute-path setup needs the shared 512-byte command stage, but a
     * Deflate block may leave compressed bytes buffered at $0604 between its
     * EMIT and the next LOAD_REU. Give input its independent short-command
     * stage before streaming so that LOAD_REU cannot replace those bytes. */
    UZ_CREATE_JOB_INPUT_DOS->command = UZ_CREATE_JOB_INPUT_COMMAND;
    UZ_CREATE_JOB_INPUT_DOS->command_cap =
        UZ_CREATE_JOB_INPUT_COMMAND_CAP;
    reu_dma_stash((unsigned int)UZ_CREATE_JOB_WRITER, work_bank,
                  UZ_CREATE_JOB_REU_STATE_OFFSET,
                  UZ_CREATE_JOB_STATE_SIZE);
    uz_deflate_init(UZ_CREATE_JOB_DEFLATE_STATE,
                    read_input, &work_bank, write_output, &work_bank,
                    store_tokens, load_tokens, &work_bank,
                    load_phase, &package_bank,
                    UZ_CREATE_JOB_WORKSPACE,
                    UZ_CREATE_JOB_INPUT, UZ_DEFLATE_BLOCK_SIZE,
                    UZ_CREATE_JOB_OUTPUT_BUFFER, UZ_DOS_WRITE_MAX,
                    UZ_CREATE_JOB_TOKEN_STAGE,
                    UZ_CREATE_JOB_TOKEN_STAGE_CAP,
                    &UZ_CREATE_JOB_REQUEST->input_size);
    compressed = uz_deflate_run(UZ_CREATE_JOB_DEFLATE_STATE);
    /* MATCH/EMIT may be the last image left at $B000 on either success or
     * failure. Restore Store before framing, DOS close, or error cleanup. */
    reu_dma_fetch(UZ_CREATE_JOB_RUN, package_bank,
                  UZ_CREATE_JOB_REQUEST->job_offset,
                  UZ_CREATE_JOB_REQUEST->job_size);
    reu_dma_fetch((unsigned int)UZ_CREATE_JOB_WRITER, work_bank,
                  UZ_CREATE_JOB_REU_STATE_OFFSET,
                  UZ_CREATE_JOB_STATE_SIZE);
    if (!compressed) {
        UZ_CREATE_JOB_REQUEST->codec_error =
            UZ_CREATE_JOB_DEFLATE_STATE->error;
        set_error(UZ_CREATE_JOB_DEFLATE);
        return 0u;
    }
    trailing = uz_dos_read(UZ_CREATE_JOB_INPUT_DOS,
                           UZ_CREATE_JOB_OUTPUT_BUFFER, 1u);
    if (trailing != 0) {
        set_error(UZ_CREATE_JOB_INPUT_CHANGED);
        return 0u;
    }
    UZ_CREATE_JOB_RECORD->size = UZ_CREATE_JOB_DEFLATE_STATE->input_size;
    UZ_CREATE_JOB_RECORD->compressed_size =
        UZ_CREATE_JOB_DEFLATE_STATE->output_size;
    UZ_CREATE_JOB_RECORD->crc = UZ_CREATE_JOB_DEFLATE_STATE->crc;
    return uz_zip_finish_deflate(UZ_CREATE_JOB_WRITER);
}

unsigned char uz_create_job_entry(unsigned char package_bank,
                                  unsigned char work_bank) {
    UzU32 central_offset;
    unsigned int index;
    unsigned char framed;
    unsigned char input_closed;
    unsigned char output_closed;

    UZ_CREATE_JOB_REQUEST->error = UZ_CREATE_JOB_OK;
    UZ_CREATE_JOB_REQUEST->zip_error = UZ_ZIP_OK;
    UZ_CREATE_JOB_REQUEST->codec_error = UZ_DEFLATE_OK;
    UZ_CREATE_JOB_GUARD = 0xA5u;
    if (!request_valid(package_bank, work_bank)) {
        set_error(UZ_CREATE_JOB_STATE);
        return 0u;
    }

    if (UZ_CREATE_JOB_REQUEST->first_entry) {
        uz_zip_writer_init(UZ_CREATE_JOB_WRITER,
                           UZ_CREATE_JOB_OUTPUT_DOS);
    } else {
        reu_dma_fetch((unsigned int)UZ_CREATE_JOB_WRITER, work_bank,
                      UZ_CREATE_JOB_REU_STATE_OFFSET,
                      sizeof(UzZipWriter));
    }
    framed = uz_zip_begin_streamed(UZ_CREATE_JOB_WRITER,
                                   UZ_CREATE_JOB_RECORD,
                                   UZ_CREATE_JOB_REQUEST->archive_name,
                                   UZ_CREATE_JOB_REQUEST->directory,
                                   UZ_CREATE_JOB_REQUEST->method);
    if (framed && UZ_CREATE_JOB_REQUEST->method == 8u)
        framed = deflate_member(package_bank, work_bank);
    else if (framed && UZ_CREATE_JOB_REQUEST->method == 0u)
        framed = store_member();
    if (!framed && UZ_CREATE_JOB_REQUEST->error == UZ_CREATE_JOB_OK) {
        UZ_CREATE_JOB_REQUEST->zip_error = UZ_CREATE_JOB_WRITER->error;
        set_error(UZ_CREATE_JOB_FRAME);
    }

    if (framed && !catalog_stash(UZ_CREATE_JOB_REQUEST->entry_index,
                                 UZ_CREATE_JOB_RECORD)) {
        set_error(UZ_CREATE_JOB_CATALOG);
        framed = 0u;
    }
    if (framed && UZ_CREATE_JOB_REQUEST->last_entry) {
        central_offset = UZ_CREATE_JOB_WRITER->offset;
        for (index = 0u; index < UZ_CREATE_JOB_REQUEST->entry_count; ++index) {
            if (!catalog_fetch(index, UZ_CREATE_JOB_RECORD) ||
                !uz_zip_emit_central(UZ_CREATE_JOB_WRITER,
                                     UZ_CREATE_JOB_RECORD)) {
                set_error(UZ_CREATE_JOB_CATALOG);
                framed = 0u;
                break;
            }
        }
        if (framed && !uz_zip_finish_archive(UZ_CREATE_JOB_WRITER,
                &central_offset, UZ_CREATE_JOB_REQUEST->entry_count)) {
            UZ_CREATE_JOB_REQUEST->zip_error = UZ_CREATE_JOB_WRITER->error;
            set_error(UZ_CREATE_JOB_FRAME);
            framed = 0u;
        }
    } else if (framed) {
        reu_dma_stash((unsigned int)UZ_CREATE_JOB_WRITER, work_bank,
                      UZ_CREATE_JOB_REU_STATE_OFFSET,
                      sizeof(UzZipWriter));
    }

    /* These closes also delegate to the one asynchronous UCI gateway. No
     * subsequent entry begins until both target transactions are quiet. */
    input_closed = UZ_CREATE_JOB_REQUEST->directory ? 1u :
                   uz_dos_job_close(UZ_CREATE_JOB_INPUT_DOS);
    output_closed = UZ_CREATE_JOB_REQUEST->last_entry ?
                    uz_dos_job_close(UZ_CREATE_JOB_OUTPUT_DOS) : 1u;
    if (!input_closed) set_error(UZ_CREATE_JOB_INPUT_CLOSE);
    if (!output_closed) set_error(UZ_CREATE_JOB_OUTPUT_CLOSE);
    return (unsigned char)(framed && input_closed && output_closed &&
                           UZ_CREATE_JOB_GUARD == 0xA5u &&
                           UZ_CREATE_JOB_REQUEST->error == UZ_CREATE_JOB_OK);
}

#ifdef __CC65__
#pragma rodata-name(pop)
#pragma code-name(pop)
#endif
