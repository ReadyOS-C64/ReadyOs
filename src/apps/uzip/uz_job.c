#include "uz_job.h"

#include "uz_pack.h"
#include "uz_package.h"
#include "../../lib/reu_mgr.h"

#define UZ_JOB_UI_START 0x3000u
#define UZ_JOB_UI_LENGTH 0x9400u
#define UZ_JOB_UI_SNAPSHOT_OFFSET 0x0000u
#define UZ_JOB_COORD_RUN 0xA000u
#define UZ_JOB_COORD_MAX 0x0B00u
#define UZ_JOB_INFLATE_RUN 0xB000u
#define UZ_JOB_INFLATE_MAX 0x1000u
#define UZ_JOB_CPU_PORT (*(volatile unsigned char *)0x0001u)
#define UZ_JOB_LORAM 0x01u

static unsigned char active_work_bank;
static unsigned char saved_cpu_port;
static UzStoreJobRequest active_store_request;
static unsigned char captured_inflate_error;
static unsigned char captured_inflate_codec_error;

/* Only this small continuation must survive the destructive codec window.
 * The public wrappers validate and load phases while their UI bytes are still
 * intact; codec calls return here, the UI is restored, and only then can the
 * original UI caller resume. */
static void restore_ui_window(void) {
    reu_dma_fetch(UZ_JOB_UI_START, active_work_bank,
                  UZ_JOB_UI_SNAPSHOT_OFFSET, UZ_JOB_UI_LENGTH);
    UZ_JOB_CPU_PORT = saved_cpu_port;
}

static unsigned char run_deflate_loaded(UzJobDeflateEntry entry,
                                        unsigned char package_bank,
                                        unsigned char work_bank) {
    unsigned char result;

    result = entry(package_bank, work_bank);
    restore_ui_window();
    return result;
}

static unsigned char run_inflate_loaded(UzJobInflateEntry entry,
                                        const UzInflateJobRequest *request) {
    unsigned char result;

    result = entry(request);
#ifdef UZIP_SELF_SEED_PACKAGE
    /* Self-seeded probes deliberately carry a two-byte placeholder inflater
     * and never call this path.  Keep the generic trampoline linkable without
     * importing accessors from an image which those focused probes omit. */
    captured_inflate_error = UZ_INFLATE_JOB_STATE;
    captured_inflate_codec_error = 0u;
#else
    /* These accessors live inside the inflater image at $B000. Capture both
     * diagnostics before restore_ui_window() replaces that image with the
     * idle UI/BSS snapshot. Callers must never jump back into the displaced
     * overlay after this continuation returns. */
    captured_inflate_error = uz_inflate_job_error();
    captured_inflate_codec_error = uz_inflate_job_codec_error();
#endif
    restore_ui_window();
    return result;
}

static unsigned char run_store_loaded(UzJobStoreEntry entry) {
    unsigned char result;

    result = entry(&active_store_request);
    restore_ui_window();
    return result;
}

#ifdef UZIP_READYOS_APP
#pragma code-name(push, "UI_CODE")
#endif

unsigned char uz_job_run_deflate(unsigned char package_bank,
                                 unsigned char work_bank,
                                 UzJobDeflateEntry entry) {
    unsigned int coord_size;
    unsigned int coord_run;
    unsigned int coord_entry;
    unsigned int coord_offset;

    coord_size = uz_pack_deflate_coord_size();
    coord_run = uz_pack_deflate_coord_run();
    coord_entry = (unsigned int)entry;
    coord_offset = uz_package_phase_offset(UZ_PACKAGE_PHASE_COORD);
    if (package_bank == 0xFFu || work_bank == 0xFFu ||
        package_bank == work_bank || coord_run != UZ_JOB_COORD_RUN ||
        coord_size == 0u || coord_size > UZ_JOB_COORD_MAX ||
        coord_offset == 0u || entry == 0 ||
        coord_entry < coord_run || coord_entry >= coord_run + coord_size)
        return 0u;

    /* This resident trampoline is the only code executing while the idle UI
     * is absent. ReadyOS normally leaves BASIC ROM visible at $A000-$BFFF;
     * clear only LORAM so the packed coordinator/job RAM becomes executable
     * while KERNAL and I/O remain mapped. Snapshot and restore also run under
     * that exact mapping so the UI's underlying $A000-$AFFF bytes are real.
     * No UI-window function is called until the original CPU port is back. */
    saved_cpu_port = UZ_JOB_CPU_PORT;
    active_work_bank = work_bank;
    UZ_JOB_CPU_PORT = (unsigned char)(saved_cpu_port &
                                      (unsigned char)~UZ_JOB_LORAM);
    reu_dma_stash(UZ_JOB_UI_START, work_bank, UZ_JOB_UI_SNAPSHOT_OFFSET,
                  UZ_JOB_UI_LENGTH);
    reu_dma_fetch(coord_run, package_bank, coord_offset, coord_size);
    return run_deflate_loaded(entry, package_bank, work_bank);
}

unsigned char uz_job_run_create(unsigned char package_bank,
                                unsigned char work_bank,
                                unsigned int image_offset,
                                unsigned int image_size,
                                UzJobDeflateEntry entry) {
    unsigned int entry_address;

    entry_address = (unsigned int)entry;
    if (package_bank == 0xFFu || work_bank == 0xFFu ||
        package_bank == work_bank || image_offset == 0u ||
        image_size == 0u || image_size > 0x1100u || entry == 0 ||
        entry_address < UZ_JOB_COORD_RUN ||
        entry_address >= UZ_JOB_COORD_RUN + image_size) return 0u;

    saved_cpu_port = UZ_JOB_CPU_PORT;
    active_work_bank = work_bank;
    UZ_JOB_CPU_PORT = (unsigned char)(saved_cpu_port &
                                      (unsigned char)~UZ_JOB_LORAM);
    reu_dma_stash(UZ_JOB_UI_START, work_bank, UZ_JOB_UI_SNAPSHOT_OFFSET,
                  UZ_JOB_UI_LENGTH);
    /* Creation begins with ZIP framing and Store/DOS helpers at $B000.
     * MATCH and EMIT replace that image later and the coordinator restores it
     * before framing/close. The snapshot above still contains the true idle
     * UI/BSS bytes, so the resident continuation restores the UI exactly. */
    reu_dma_fetch(uz_pack_job_run(), package_bank,
                  uz_package_phase_offset(UZ_PACKAGE_PHASE_JOB),
                  uz_pack_job_size());
    reu_dma_fetch(UZ_JOB_COORD_RUN, package_bank, image_offset, image_size);
    return run_deflate_loaded(entry, package_bank, work_bank);
}

unsigned char uz_job_run_inflate(unsigned char package_bank,
                                 unsigned char work_bank,
                                 UzJobInflateEntry entry,
                                 const UzInflateJobRequest *request) {
    unsigned int inflate_size;
    unsigned int inflate_run;
    unsigned int inflate_bss_run;
    unsigned int inflate_bss_size;
    unsigned int inflate_entry;
    unsigned int inflate_offset;

    inflate_size = uz_pack_inflate_size();
    inflate_run = uz_pack_inflate_run();
    inflate_bss_run = uz_pack_inflate_bss_run();
    inflate_bss_size = uz_pack_inflate_bss_size();
    inflate_entry = (unsigned int)entry;
    inflate_offset = uz_package_phase_offset(UZ_PACKAGE_PHASE_INFLATE);
    captured_inflate_error = UZ_INFLATE_JOB_STATE;
    captured_inflate_codec_error = 0u;
    if (package_bank == 0xFFu || work_bank == 0xFFu ||
        package_bank == work_bank || request == 0 || entry == 0 ||
        inflate_run != UZ_JOB_INFLATE_RUN || inflate_size == 0u ||
        inflate_size > UZ_JOB_INFLATE_MAX || inflate_offset == 0u ||
        inflate_entry < inflate_run ||
        inflate_entry >= inflate_run + inflate_size ||
        inflate_bss_run < inflate_run + inflate_size ||
        inflate_bss_run + inflate_bss_size > 0xC400u)
        return 0u;

    saved_cpu_port = UZ_JOB_CPU_PORT;
    active_work_bank = work_bank;
    UZ_JOB_CPU_PORT = (unsigned char)(saved_cpu_port &
                                      (unsigned char)~UZ_JOB_LORAM);
    reu_dma_stash(UZ_JOB_UI_START, work_bank, UZ_JOB_UI_SNAPSHOT_OFFSET,
                  UZ_JOB_UI_LENGTH);
    reu_dma_fetch(inflate_run, package_bank, inflate_offset,
                  inflate_size);
    return run_inflate_loaded(entry, request);
}

unsigned char uz_job_inflate_error(void) {
    return captured_inflate_error;
}

unsigned char uz_job_inflate_codec_error(void) {
    return captured_inflate_codec_error;
}

unsigned char uz_job_run_store(unsigned char package_bank,
                               unsigned char work_bank,
                               UzJobStoreEntry entry,
                               const UzStoreJobRequest *request) {
    unsigned int coord_size;
    unsigned int coord_run;
    unsigned int coord_entry;
    unsigned int coord_offset;

    coord_size = uz_pack_deflate_coord_size();
    coord_run = uz_pack_deflate_coord_run();
    coord_entry = (unsigned int)entry;
    coord_offset = uz_package_phase_offset(UZ_PACKAGE_PHASE_COORD);
    if (package_bank == 0xFFu || work_bank == 0xFFu ||
        package_bank == work_bank || request == 0 || entry == 0 ||
        coord_run != UZ_JOB_COORD_RUN || coord_size == 0u ||
        coord_size > UZ_JOB_COORD_MAX || coord_offset == 0u ||
        coord_entry < coord_run || coord_entry >= coord_run + coord_size)
        return UZ_STORE_JOB_STATE;

    active_store_request = *request;
    saved_cpu_port = UZ_JOB_CPU_PORT;
    active_work_bank = work_bank;
    UZ_JOB_CPU_PORT = (unsigned char)(saved_cpu_port &
                                      (unsigned char)~UZ_JOB_LORAM);
    reu_dma_stash(UZ_JOB_UI_START, work_bank, UZ_JOB_UI_SNAPSHOT_OFFSET,
                  UZ_JOB_UI_LENGTH);
    reu_dma_fetch(coord_run, package_bank, coord_offset, coord_size);
    return run_store_loaded(entry);
}

#ifdef UZIP_READYOS_APP
#pragma code-name(pop)
#endif
