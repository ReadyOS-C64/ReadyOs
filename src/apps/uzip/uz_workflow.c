#include "uz_workflow.h"

#include "uz_create_job.h"
#include "uz_create_package.h"
#include "uz_catalog.h"
#include "uz_extract_fs.h"
#include "uz_extract_plan.h"
#include "uz_job.h"
#include "uz_package.h"
#include "../../lib/reu_mgr.h"

#include <string.h>

#define UZWF_INPUT_DOS UZ_CREATE_JOB_INPUT_DOS
#define UZWF_OUTPUT_DOS UZ_CREATE_JOB_OUTPUT_DOS
#define UZWF_INPUT_COMMAND UZ_CREATE_JOB_INPUT_COMMAND
#define UZWF_INPUT_DATA UZ_CREATE_JOB_INPUT_DATA
#define UZWF_STATUS UZ_CREATE_JOB_STATUS
#define UZWF_OUTPUT_COMMAND UZ_CREATE_JOB_OUTPUT_COMMAND
#define UZWF_RECORD ((UzZipRecord *)0x0900u)
#define UZWF_EXTRACT_ENTRY ((UzExtractPlanEntry *)0x0900u)
#define UZWF_EXTRACT_STATE ((UzExtractFs *)0x0A00u)
#define UZWF_PATH ((char *)0x0B20u)
#define UZWF_EXTRACT_COMMAND_CAP 288u
#define UZWF_EXTRACT_DATA_CAP 64u
#define UZWF_EXTRACT_STATUS_CAP 32u

static unsigned char workflow_error;
static unsigned char workflow_detail;
static unsigned int workflow_completed;

#ifdef UZIP_READYOS_APP
#pragma code-name(push, "UI_CODE")
#pragma rodata-name(push, "UI_RODATA")
#pragma bss-name(push, "UI_BSS")
#endif

static char workflow_temp[9];
static const char *workflow_leaf;

/* Unlike the create coordinator's deliberately disposable low-memory state,
 * these handles must survive the inflater's $0400-$07FB input/output buffers.
 * The normal job snapshot restores UI_BSS before close/rename, matching the
 * physically proven compact-v7 extraction probe lifecycle. */
static UzDos extract_input_dos;
static UzDos extract_output_dos;
static unsigned char extract_input_command[UZWF_EXTRACT_COMMAND_CAP];
static unsigned char extract_output_command[UZWF_EXTRACT_COMMAND_CAP];
static unsigned char extract_input_data[UZWF_EXTRACT_DATA_CAP];
static unsigned char extract_output_data[UZWF_EXTRACT_DATA_CAP];
static unsigned char extract_input_status[UZWF_EXTRACT_STATUS_CAP];
static unsigned char extract_output_status[UZWF_EXTRACT_STATUS_CAP];

static void init_dos(void) {
    /* Ultimate DOS targets are strictly serialized through the shared UCI
     * state machine.  Both target descriptors can therefore share the job's
     * 512-byte command staging area.  The smaller framing scratch at $0560
     * cannot hold a legal 255-byte absolute source path during UI preflight;
     * once files are open, every input/output job command is short. Deflate
     * rebinds input to the independent $0560 stage because compressed output
     * can remain live at $0604 between serialized transactions. */
    uz_dos_init(UZWF_INPUT_DOS, UZ_DOS_TARGET_READ,
                UZWF_OUTPUT_COMMAND, UZ_CREATE_JOB_OUTPUT_COMMAND_CAP,
                UZWF_INPUT_DATA, UZ_CREATE_JOB_DATA_CAP,
                UZWF_STATUS, UZ_CREATE_JOB_STATUS_CAP);
    uz_dos_init(UZWF_OUTPUT_DOS, UZ_DOS_TARGET_WRITE,
                UZWF_OUTPUT_COMMAND, UZ_CREATE_JOB_OUTPUT_COMMAND_CAP,
                UZWF_INPUT_DATA, UZ_CREATE_JOB_DATA_CAP,
                UZWF_STATUS, UZ_CREATE_JOB_STATUS_CAP);
}

static void init_extract_dos(void) {
    uz_dos_init(&extract_input_dos, UZ_DOS_TARGET_READ,
                extract_input_command, sizeof(extract_input_command),
                extract_input_data, sizeof(extract_input_data),
                extract_input_status, sizeof(extract_input_status));
    uz_dos_init(&extract_output_dos, UZ_DOS_TARGET_WRITE,
                extract_output_command, sizeof(extract_output_command),
                extract_output_data, sizeof(extract_output_data),
                extract_output_status, sizeof(extract_output_status));
}

static unsigned char safe_leaf(const char *name) {
    unsigned int length;
    unsigned char value;

    if (name == 0 || name[0] == 0) return 0u;
    length = 0u;
    while (name[length] != 0) {
        value = (unsigned char)name[length++];
        if (value < 0x20u || value > 0x7Eu || value == '/' ||
            value == '\\' || value == ':' || length >= 128u) return 0u;
    }
    return (unsigned char)(!(length == 1u && name[0] == '.') &&
        !(length == 2u && name[0] == '.' && name[1] == '.'));
}

static unsigned char split_archive(const char *path, char *parent,
                                   const char **leaf) {
    const char *slash;
    const char *cursor;
    unsigned int length;

    if (path == 0 || path[0] != '/' || path[1] == 0) return 0u;
    slash = path;
    cursor = path;
    while (*cursor != 0) {
        if (*cursor == '/') slash = cursor;
        ++cursor;
    }
    *leaf = slash + 1u;
    if (!safe_leaf(*leaf)) return 0u;
    length = (unsigned int)(slash - path);
    if (length == 0u) length = 1u;
    memcpy(parent, path, length);
    parent[length] = 0;
    return 1u;
}

static unsigned char hex_char(unsigned char value) {
    value &= 0x0Fu;
    return (unsigned char)(value < 10u ? '0' + value : 'a' + value - 10u);
}

static unsigned char open_unique_archive(UzDos *output) {
    unsigned int attempt;

    /* Spell this out so cc65 does not hoist a new literal into resident
     * RODATA. Absolute resident-address shifts would change the frozen v7
     * extraction phases even though this Create-only function is UI code. */
    workflow_temp[0] = '.';
    workflow_temp[1] = 'u';
    workflow_temp[2] = 'z';
    workflow_temp[3] = 't';
    workflow_temp[4] = 'm';
    workflow_temp[5] = 'p';
    workflow_temp[6] = '0';
    workflow_temp[7] = '0';
    workflow_temp[8] = 0;
    for (attempt = 0u; attempt < 256u; ++attempt) {
        workflow_temp[6] = (char)hex_char((unsigned char)(attempt >> 4u));
        workflow_temp[7] = (char)hex_char((unsigned char)attempt);
        if (uz_dos_open(output, workflow_temp, UZ_DOS_OPEN_WRITE_NEW))
            return 1u;
    }
    return 0u;
}

static unsigned char compose_member_parent(const char *source_base,
                                           const char *archive_name) {
    unsigned int base_len;
    unsigned int name_len;
    unsigned int parent_len;
    unsigned int slash_at;
    unsigned int at;

    if (source_base == 0 || source_base[0] != '/' || archive_name == 0 ||
        archive_name[0] == 0) return 0u;
    name_len = 0u;
    slash_at = 0u;
    while (archive_name[name_len] != 0) {
        if (archive_name[name_len] == '/')
            slash_at = (unsigned int)(name_len + 1u);
        ++name_len;
    }
    workflow_leaf = archive_name + slash_at;
    if (!safe_leaf(workflow_leaf)) return 0u;
    base_len = strlen(source_base);
    parent_len = slash_at == 0u ? 0u : (unsigned int)(slash_at - 1u);
    if (base_len + parent_len + (base_len == 1u ? 1u : 2u) >
        UZ_DOS_PATH_CAP) return 0u;
    memcpy(UZWF_PATH, source_base, base_len);
    at = base_len;
    if (parent_len != 0u) {
        if (base_len != 1u) UZWF_PATH[at++] = '/';
        memcpy(UZWF_PATH + at, archive_name, parent_len);
        at = (unsigned int)(at + parent_len);
    }
    UZWF_PATH[at] = 0;
    return 1u;
}

static unsigned char compose_output_path(const char *output_dir,
                                         const char *name) {
    unsigned int length;
    unsigned int name_len;

    length = strlen(output_dir);
    name_len = strlen(name);
    if (length + name_len + (length == 1u ? 1u : 2u) >
        UZ_DOS_PATH_CAP) return 0u;
    memcpy(UZWF_PATH, output_dir, length);
    if (length != 1u) UZWF_PATH[length++] = '/';
    memcpy(UZWF_PATH + length, name, name_len + 1u);
    return 1u;
}

unsigned char uz_workflow_create(unsigned char package_bank,
                                 unsigned char work_bank,
                                 unsigned char catalog_bank,
                                 const char *source_base,
                                 const char *output_dir,
                                 const char *output_name,
                                 unsigned int entry_count,
                                 UzWorkflowProgress progress,
                                 void *progress_context) {
    UzU32 input_size;
    unsigned int index;
    unsigned int offset;
    unsigned char output_created;
    unsigned char ok;

    workflow_error = UZ_WORKFLOW_STATE;
    workflow_detail = 0u;
    workflow_completed = 0u;
    output_created = 0u;
    if (package_bank == 0xFFu || work_bank == 0xFFu ||
        catalog_bank == 0xFFu || package_bank == work_bank ||
        package_bank == catalog_bank || work_bank == catalog_bank ||
        source_base == 0 || source_base[0] != '/' ||
        output_dir == 0 || output_dir[0] != '/' ||
        !safe_leaf(output_name) || entry_count == 0u ||
        entry_count > UZ_CATALOG_MAX_ENTRIES ||
        !uz_create_package_open(package_bank)) return 0u;

    init_dos();
    workflow_error = UZ_WORKFLOW_DOS;
    workflow_detail = UZ_WORKFLOW_DOS_IDENTIFY_INPUT;
    if (!uz_dos_identify(UZWF_INPUT_DOS)) return 0u;
    workflow_detail = UZ_WORKFLOW_DOS_IDENTIFY_OUTPUT;
    if (!uz_dos_identify(UZWF_OUTPUT_DOS)) return 0u;
    workflow_detail = UZ_WORKFLOW_DOS_CD_OUTPUT;
    if (!uz_dos_change_absolute(UZWF_OUTPUT_DOS, output_dir)) return 0u;
    workflow_error = UZ_WORKFLOW_OUTPUT;
    if (!open_unique_archive(UZWF_OUTPUT_DOS)) return 0u;
    output_created = 1u;

    for (index = 0u; index < entry_count; ++index) {
        offset = (unsigned int)(index * (unsigned int)sizeof(UzZipRecord));
        reu_dma_fetch((unsigned int)UZWF_RECORD, catalog_bank,
                      offset, sizeof(UzZipRecord));
        if (progress != 0) {
            ok = progress(progress_context, index, entry_count,
                          UZWF_RECORD->name);
            /* Create's modal DOS descriptors and buffers deliberately live
             * in screen RAM at $0480-$07FF.  A TUI progress redraw therefore
             * erases their C-side state, although the Ultimate target keeps
             * the already-open archive handle.  Rebuild the descriptors at
             * the quiet member boundary and retain only that open-handle
             * fact before either continuing or performing cancel cleanup. */
            init_dos();
            UZWF_OUTPUT_DOS->file_open = 1u;
            if (!ok) {
                workflow_error = UZ_WORKFLOW_CANCEL;
                workflow_detail = (unsigned char)index;
                goto failed;
            }
        }
        input_size.lo = 0u;
        input_size.hi = 0u;
        if (!UZWF_RECORD->directory) {
            /* The prior Deflate member deliberately rebound input to the
             * non-overlapping short-command stage. Restore the large shared
             * command area for this member's possibly long absolute path. */
            UZWF_INPUT_DOS->command = UZWF_OUTPUT_COMMAND;
            UZWF_INPUT_DOS->command_cap =
                UZ_CREATE_JOB_OUTPUT_COMMAND_CAP;
            workflow_error = UZ_WORKFLOW_DOS;
            workflow_detail = UZ_WORKFLOW_DOS_SPLIT_INPUT;
            if (!compose_member_parent(source_base, UZWF_RECORD->name))
                goto failed;
            workflow_detail = UZ_WORKFLOW_DOS_CD_INPUT;
            if (!uz_dos_change_absolute(UZWF_INPUT_DOS, UZWF_PATH))
                goto failed;
            workflow_detail = UZ_WORKFLOW_DOS_OPEN_INPUT;
            if (!uz_dos_open(UZWF_INPUT_DOS, workflow_leaf,
                             UZ_DOS_OPEN_READ))
                goto failed;
            workflow_detail = UZ_WORKFLOW_DOS_INFO_INPUT;
            if (!uz_dos_file_info(UZWF_INPUT_DOS, &input_size)) {
                (void)uz_dos_close(UZWF_INPUT_DOS);
                goto failed;
            }
        }

        memset(UZ_CREATE_JOB_REQUEST, 0, sizeof(*UZ_CREATE_JOB_REQUEST));
        UZ_CREATE_JOB_REQUEST->input_target = UZWF_INPUT_DOS->target;
        UZ_CREATE_JOB_REQUEST->output_target = UZWF_OUTPUT_DOS->target;
        UZ_CREATE_JOB_REQUEST->catalog_bank = catalog_bank;
        UZ_CREATE_JOB_REQUEST->method = UZWF_RECORD->method;
        UZ_CREATE_JOB_REQUEST->directory = UZWF_RECORD->directory;
        UZ_CREATE_JOB_REQUEST->first_entry = (unsigned char)(index == 0u);
        UZ_CREATE_JOB_REQUEST->last_entry =
            (unsigned char)(index + 1u == entry_count);
        UZ_CREATE_JOB_REQUEST->entry_index = index;
        UZ_CREATE_JOB_REQUEST->entry_count = entry_count;
        UZ_CREATE_JOB_REQUEST->input_size = input_size;
        UZ_CREATE_JOB_REQUEST->job_offset =
            uz_package_phase_offset(UZ_PACKAGE_PHASE_JOB);
        UZ_CREATE_JOB_REQUEST->job_size =
            uz_package_phase_size(UZ_PACKAGE_PHASE_JOB);
        UZ_CREATE_JOB_REQUEST->match_offset =
            uz_package_phase_offset(UZ_PACKAGE_PHASE_MATCH);
        UZ_CREATE_JOB_REQUEST->match_size =
            uz_package_phase_size(UZ_PACKAGE_PHASE_MATCH);
        UZ_CREATE_JOB_REQUEST->emit_offset =
            uz_package_phase_offset(UZ_PACKAGE_PHASE_EMIT);
        UZ_CREATE_JOB_REQUEST->emit_size =
            uz_package_phase_size(UZ_PACKAGE_PHASE_EMIT);
        strcpy(UZ_CREATE_JOB_REQUEST->archive_name, UZWF_RECORD->name);
        workflow_error = UZ_WORKFLOW_CREATE;
        ok = uz_job_run_create(package_bank, work_bank,
                               uz_create_package_offset(),
                               uz_create_package_size(),
                               (UzJobDeflateEntry)uz_create_package_entry());
        if (!ok) {
            workflow_detail = UZ_CREATE_JOB_REQUEST->error;
            goto failed;
        }
        ++workflow_completed;
    }

    workflow_error = UZ_WORKFLOW_VERIFY;
    if (!compose_output_path(output_dir, workflow_temp) ||
        !uz_extract_plan_build(package_bank, work_bank, catalog_bank,
                               UZWF_PATH) ||
        uz_extract_plan_count() != entry_count) goto failed;
    workflow_error = UZ_WORKFLOW_COMMIT;
    if (!uz_dos_change_absolute(UZWF_OUTPUT_DOS, output_dir) ||
        !uz_dos_rename(UZWF_OUTPUT_DOS, workflow_temp, output_name))
        goto failed;
    output_created = 0u;
    workflow_error = UZ_WORKFLOW_OK;
    workflow_detail = 0u;
    return 1u;

failed:
    (void)uz_dos_close(UZWF_INPUT_DOS);
    (void)uz_dos_close(UZWF_OUTPUT_DOS);
    if (output_created && uz_dos_change_absolute(UZWF_OUTPUT_DOS, output_dir))
        (void)uz_dos_delete(UZWF_OUTPUT_DOS, workflow_temp);
    return 0u;
}

unsigned char uz_workflow_extract(unsigned char package_bank,
                                  unsigned char work_bank,
                                  unsigned char catalog_bank,
                                  const char *archive_path,
                                  const char *destination_root,
                                  UzWorkflowProgress progress,
                                  void *progress_context) {
    const char *leaf;
    unsigned int index;
    unsigned int offset;
    unsigned char ok;

    workflow_error = UZ_WORKFLOW_STATE;
    workflow_detail = 0u;
    workflow_completed = 0u;
    if (package_bank == 0xFFu || work_bank == 0xFFu ||
        catalog_bank == 0xFFu || destination_root == 0 ||
        destination_root[0] != '/') return 0u;
    workflow_error = UZ_WORKFLOW_PREFLIGHT;
    if (!uz_extract_plan_build(package_bank, work_bank, catalog_bank,
                               archive_path)) {
        workflow_detail = uz_extract_plan_error();
        return 0u;
    }
    init_extract_dos();
    workflow_error = UZ_WORKFLOW_DOS;
    if (!split_archive(archive_path, UZWF_PATH, &leaf) ||
        !uz_dos_change_absolute(&extract_input_dos, UZWF_PATH) ||
        !uz_dos_open(&extract_input_dos, leaf, UZ_DOS_OPEN_READ)) return 0u;
    for (index = 0u; index < uz_extract_plan_count(); ++index) {
        offset = (unsigned int)(index *
                 (unsigned int)sizeof(UzExtractPlanEntry));
        reu_dma_fetch((unsigned int)UZWF_EXTRACT_ENTRY, catalog_bank,
                      offset, sizeof(UzExtractPlanEntry));
        if (progress != 0 && !progress(progress_context, index,
                                       uz_extract_plan_count(),
                                       UZWF_EXTRACT_ENTRY->record.name)) {
            workflow_error = UZ_WORKFLOW_CANCEL;
            workflow_detail = (unsigned char)index;
            (void)uz_dos_close(&extract_input_dos);
            return 0u;
        }
        uz_extract_fs_init(UZWF_EXTRACT_STATE);
        workflow_error = UZ_WORKFLOW_EXTRACT;
        workflow_detail = (unsigned char)index;
        ok = uz_extract_member(UZWF_EXTRACT_STATE,
                               &extract_input_dos, &extract_output_dos,
                               destination_root,
                               &UZWF_EXTRACT_ENTRY->record,
                               &UZWF_EXTRACT_ENTRY->data_offset,
                               package_bank, work_bank);
        if (!ok) {
            workflow_detail = UZWF_EXTRACT_STATE->error;
            (void)uz_dos_close(&extract_input_dos);
            return 0u;
        }
        ++workflow_completed;
    }
    workflow_error = UZ_WORKFLOW_CLOSE;
    if (!uz_dos_close(&extract_input_dos)) return 0u;
    workflow_error = UZ_WORKFLOW_OK;
    workflow_detail = 0u;
    return 1u;
}

unsigned char uz_workflow_error(void) { return workflow_error; }
unsigned char uz_workflow_detail(void) { return workflow_detail; }
unsigned int uz_workflow_completed(void) { return workflow_completed; }

#ifdef UZIP_READYOS_APP
#pragma bss-name(pop)
#pragma rodata-name(pop)
#pragma code-name(pop)
#endif
