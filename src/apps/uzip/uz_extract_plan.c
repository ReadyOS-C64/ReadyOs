#include "uz_extract_plan.h"

#include "uz_dos.h"
#include "uz_pack.h"
#include "uz_package.h"
#include "uz_zip_read.h"
#include "../../lib/reu_mgr.h"

#include <string.h>

#define UZXP_DOS ((UzDos *)0x0400u)
#define UZXP_COMMAND ((unsigned char *)0x0460u)
#define UZXP_DATA ((unsigned char *)0x0500u)
#define UZXP_STATUS ((unsigned char *)0x0540u)
#define UZXP_READER ((UzZipReader *)0x0580u)
#define UZXP_RECORD ((UzZipRecord *)0x05C0u)
#define UZXP_SCRATCH ((unsigned char *)0x0700u)
#define UZXP_ENTRY ((UzExtractPlanEntry *)0x0900u)

#define UZXP_COMMAND_CAP 160u
#define UZXP_DATA_CAP 64u
#define UZXP_STATUS_CAP 64u
#define UZXP_SCRATCH_CAP 512u
#define UZXP_READ_OFFSET 0xA000u
#define UZXP_UI_START 0x3000u
#define UZXP_UI_LENGTH 0x9400u

static unsigned char active_work_bank;
static unsigned int plan_count;
static unsigned char plan_error;

#ifdef UZIP_READYOS_APP
#pragma code-name(push, "UI_CODE")
#pragma rodata-name(push, "UI_RODATA")
#endif

static unsigned char split_path(const char *path, char *parent,
                                char *leaf) {
    const char *slash;
    const char *cursor;
    unsigned int parent_len;
    unsigned int leaf_len;

    if (path == 0 || path[0] != '/' || path[1] == 0) return 0u;
    slash = path;
    cursor = path;
    while (*cursor != 0) {
        if (*cursor == '/') slash = cursor;
        ++cursor;
    }
    if (slash[1] == 0) return 0u;
    parent_len = (unsigned int)(slash - path);
    if (parent_len == 0u) parent_len = 1u;
    leaf_len = strlen(slash + 1u);
    if (parent_len >= 256u || leaf_len == 0u || leaf_len >= 128u) return 0u;
    memcpy(parent, path, parent_len);
    parent[parent_len] = 0;
    strcpy(leaf, slash + 1u);
    return 1u;
}

static unsigned char read_at(void *context, const UzU32 *offset,
                             unsigned char *destination,
                             unsigned int length) {
    unsigned int transferred;

    (void)context;
    if (length == 0u || length > UZXP_SCRATCH_CAP) return 0u;
    transferred = 0u;
    /* The shared UCI gateway owns synchronization, asynchronous PUSH/ABORT,
     * full queue draining, DATA_ACC, recovery, and final quiet idle. */
    if (!uz_dos_seek(UZXP_DOS, offset) ||
        !uz_dos_load_reu(UZXP_DOS, active_work_bank, UZXP_READ_OFFSET,
                         length, &transferred) || transferred != length)
        return 0u;
    reu_dma_fetch((unsigned int)destination, active_work_bank,
                  UZXP_READ_OFFSET, length);
    return 1u;
}

unsigned char uz_extract_plan_build(unsigned char package_bank,
                                    unsigned char work_bank,
                                    unsigned char catalog_bank,
                                    const char *archive_path) {
    UzU32 archive_size;
    unsigned int index;
    unsigned int catalog_offset;
    unsigned char result;

    plan_count = 0u;
    plan_error = UZ_EXTRACT_PLAN_STATE;
    if (package_bank == 0xFFu || work_bank == 0xFFu ||
        catalog_bank == 0xFFu || package_bank == work_bank ||
        package_bank == catalog_bank || work_bank == catalog_bank ||
        !split_path(archive_path, (char *)UZXP_SCRATCH,
                    (char *)(UZXP_SCRATCH + 256u))) return 0u;
    uz_dos_init(UZXP_DOS, UZ_DOS_TARGET_READ,
                UZXP_COMMAND, UZXP_COMMAND_CAP,
                UZXP_DATA, UZXP_DATA_CAP,
                UZXP_STATUS, UZXP_STATUS_CAP);
    plan_error = UZ_EXTRACT_PLAN_PATH;
    if (!uz_dos_change_absolute(UZXP_DOS, (const char *)UZXP_SCRATCH))
        return 0u;
    plan_error = UZ_EXTRACT_PLAN_OPEN;
    if (!uz_dos_open(UZXP_DOS, (const char *)(UZXP_SCRATCH + 256u),
                     UZ_DOS_OPEN_READ)) return 0u;
    plan_error = UZ_EXTRACT_PLAN_INFO;
    if (!uz_dos_file_info(UZXP_DOS, &archive_size)) {
        (void)uz_dos_close(UZXP_DOS);
        return 0u;
    }

    active_work_bank = work_bank;
    reu_dma_stash(UZXP_UI_START, work_bank, 0u, UZXP_UI_LENGTH);
    reu_dma_fetch(uz_pack_zip_read_run(), package_bank,
                  uz_package_phase_offset(UZ_PACKAGE_PHASE_READER),
                  uz_pack_zip_read_size());
    uz_zip_reader_init_at(UZXP_READER, &archive_size, read_at, 0);
    result = uz_zip_reader_begin(UZXP_READER, UZXP_SCRATCH,
                                 UZXP_SCRATCH_CAP);
    plan_error = UZ_EXTRACT_PLAN_PARSE;
    if (result && UZXP_READER->entry_count > 400u) {
        result = 0u;
        plan_error = UZ_EXTRACT_PLAN_FULL;
    }
    for (index = 0u; result && index < UZXP_READER->entry_count; ++index) {
        if (!uz_zip_reader_next(UZXP_READER, UZXP_RECORD) ||
            !uz_zip_reader_local(UZXP_READER, UZXP_RECORD,
                                 &UZXP_ENTRY->data_offset,
                                 UZXP_SCRATCH, UZXP_SCRATCH_CAP)) {
            result = 0u;
            break;
        }
        memcpy(&UZXP_ENTRY->record, UZXP_RECORD, sizeof(UzZipRecord));
        catalog_offset = (unsigned int)(index *
                         (unsigned int)sizeof(UzExtractPlanEntry));
        reu_dma_stash((unsigned int)UZXP_ENTRY, catalog_bank,
                      catalog_offset, sizeof(UzExtractPlanEntry));
    }
    if (result && !uz_zip_reader_finished(UZXP_READER)) result = 0u;
    if (!uz_dos_close(UZXP_DOS)) {
        result = 0u;
        plan_error = UZ_EXTRACT_PLAN_CLOSE;
    }
    reu_dma_fetch(UZXP_UI_START, work_bank, 0u, UZXP_UI_LENGTH);
    if (!result) return 0u;
    /* UZXP_READER was displaced by the restore; use the loop result. */
    plan_count = index;
    plan_error = UZ_EXTRACT_PLAN_OK;
    return 1u;
}

unsigned int uz_extract_plan_count(void) { return plan_count; }
unsigned char uz_extract_plan_error(void) { return plan_error; }

#ifdef UZIP_READYOS_APP
#pragma rodata-name(pop)
#pragma code-name(pop)
#endif
