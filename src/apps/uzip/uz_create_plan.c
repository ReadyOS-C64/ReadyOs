#include "uz_create_plan.h"

#include <string.h>

#if defined(UZIP_CREATE_PLAN_OVERLAY)
#pragma code-name(push, "CREATE_PLAN_CODE")
#pragma rodata-name(push, "CREATE_PLAN_RODATA")
#elif defined(UZIP_READYOS_APP)
#pragma code-name(push, "UI_CODE")
#pragma rodata-name(push, "UI_RODATA")
#endif

static unsigned char ascii_fold(unsigned char value) {
    if (value >= 0x61u && value <= 0x7Au)
        return (unsigned char)(value - 0x20u);
    return value;
}

static unsigned char contains_separator(const char *name) {
    while (*name != 0) {
        if (*name == '/' || *name == '\\') return 1u;
        ++name;
    }
    return 0u;
}

static unsigned char component_safe(const char *start, unsigned int length) {
    return (unsigned char)(length != 0u && length < UZ_BROWSER_NAME_CAP &&
        !(length == 1u && start[0] == '.') &&
        !(length == 2u && start[0] == '.' && start[1] == '.'));
}

static unsigned char absolute_safe(const char *path) {
    const char *component;
    unsigned int component_len;
    unsigned int total;
    unsigned char value;

    if (path == 0 || path[0] != '/') return 0u;
    if (path[1] == 0) return 1u;
    component = path + 1u;
    component_len = 0u;
    total = 1u;
    ++path;
    while (*path != 0) {
        value = (unsigned char)*path++;
        ++total;
        if (total >= UZ_BROWSER_PATH_CAP || value < 0x20u || value > 0x7Eu ||
            value == '\\' || value == ':') return 0u;
        if (value == '/') {
            if (!component_safe(component, component_len)) return 0u;
            component = path;
            component_len = 0u;
        } else {
            ++component_len;
        }
    }
    return component_safe(component, component_len);
}

static unsigned char equal_folded(const char *left, const char *right) {
    while (*left != 0 && *right != 0) {
        if (ascii_fold((unsigned char)*left++) !=
            ascii_fold((unsigned char)*right++)) return 0u;
    }
    return (unsigned char)(*left == 0 && *right == 0);
}

static unsigned char prefix_folded(const char *prefix, const char *path) {
    while (*prefix != 0) {
        if (ascii_fold((unsigned char)*prefix++) !=
            ascii_fold((unsigned char)*path++)) return 0u;
    }
    return 1u;
}

static unsigned char compose_absolute(UzCreatePlan *plan,
                                      const UzZipRecord *record) {
    unsigned int base_len;
    unsigned int name_len;
    unsigned int needed;
    unsigned int destination;

    base_len = strlen(plan->source_base);
    name_len = strlen(record->name);
    if (record->directory) {
        if (name_len == 0u || record->name[name_len - 1u] != '/') return 0u;
        --name_len;
    }
    needed = base_len + name_len + 1u;
    if (base_len != 1u) ++needed;
    if (needed > plan->path_cap) return 0u;
    memcpy(plan->path, plan->source_base, base_len);
    destination = base_len;
    if (base_len != 1u) plan->path[destination++] = '/';
    memcpy(plan->path + destination, record->name, name_len);
    plan->path[destination + name_len] = 0;
    return absolute_safe(plan->path);
}

static unsigned char map_catalog_error(UzCreatePlan *plan) {
    if (plan->catalog->error == UZ_CATALOG_ERR_CONFLICT)
        plan->error = UZ_CREATE_PLAN_ERR_CONFLICT;
    else if (plan->catalog->error == UZ_CATALOG_ERR_FULL)
        plan->error = UZ_CREATE_PLAN_ERR_FULL;
    else if (plan->catalog->error == UZ_CATALOG_ERR_RECORD)
        plan->error = UZ_CREATE_PLAN_ERR_NAME;
    else
        plan->error = UZ_CREATE_PLAN_ERR_CATALOG;
    return 0u;
}

static void reset_record_payload(UzZipRecord *record,
                                 unsigned char directory,
                                 unsigned int method) {
    uz_u32_zero(&record->local_offset);
    uz_u32_zero(&record->size);
    uz_u32_zero(&record->compressed_size);
    memset(&record->crc, 0, sizeof(record->crc));
    record->flags = 0u;
    record->method = directory ? 0u : method;
    record->directory = directory;
}

static unsigned char append_candidate(UzCreatePlan *plan) {
    if (!uz_catalog_append_unique(plan->catalog, plan->record,
                                  plan->scratch))
        return map_catalog_error(plan);
    if (plan->record->directory) ++plan->directories;
    else ++plan->files;
    return 1u;
}

void uz_create_plan_init(UzCreatePlan *plan, UzCatalog *catalog,
                         UzCreatePlanList list, void *list_context,
                         const char *source_base, char *path,
                         unsigned int path_cap, UzBrowserPage *page,
                         UzZipRecord *record, UzZipRecord *scratch,
                         unsigned int file_method) {
    memset(plan, 0, sizeof(*plan));
    plan->catalog = catalog;
    plan->list = list;
    plan->list_context = list_context;
    plan->source_base = source_base;
    plan->path = path;
    plan->path_cap = path_cap;
    plan->page = page;
    plan->record = record;
    plan->scratch = scratch;
    plan->file_method = (unsigned char)file_method;
    if (catalog == 0 || catalog->error != UZ_CATALOG_OK || list == 0 ||
        path == 0 || path_cap < 2u || page == 0 || record == 0 ||
        scratch == 0 || (file_method != 0u && file_method != 8u)) {
        plan->error = UZ_CREATE_PLAN_ERR_STATE;
    } else if (!absolute_safe(source_base)) {
        plan->error = UZ_CREATE_PLAN_ERR_BASE;
    }
}

unsigned char uz_create_plan_seed(UzCreatePlan *plan, const char *name,
                                  unsigned char directory) {
    unsigned int name_len;

    if (plan == 0 || plan->error != UZ_CREATE_PLAN_OK || plan->built) {
        if (plan != 0 && plan->error == UZ_CREATE_PLAN_OK)
            plan->error = UZ_CREATE_PLAN_ERR_STATE;
        return 0u;
    }
    if (name == 0) {
        plan->error = UZ_CREATE_PLAN_ERR_NAME;
        return 0u;
    }
    name_len = strlen(name);
    if (name_len == 0u || name_len + (directory ? 2u : 1u) >
        UZ_ZIP_NAME_CAP || contains_separator(name)) {
        plan->error = UZ_CREATE_PLAN_ERR_NAME;
        return 0u;
    }
    memset(plan->record, 0, sizeof(*plan->record));
    memcpy(plan->record->name, name, name_len);
    if (directory) plan->record->name[name_len++] = '/';
    plan->record->name[name_len] = 0;
    reset_record_payload(plan->record, directory, plan->file_method);
    if (!append_candidate(plan)) return 0u;
    ++plan->seed_count;
    return 1u;
}

static unsigned char output_safe(UzCreatePlan *plan,
                                 const char *absolute_output) {
    unsigned int index;
    unsigned int selected_len;

    if (!absolute_safe(absolute_output) || absolute_output[1] == 0) {
        plan->error = UZ_CREATE_PLAN_ERR_OUTPUT_PATH;
        return 0u;
    }
    for (index = 0u; index < plan->seed_count; ++index) {
        if (!uz_catalog_get(plan->catalog, index, plan->record))
            return map_catalog_error(plan);
        if (!compose_absolute(plan, plan->record)) {
            plan->error = UZ_CREATE_PLAN_ERR_PATH;
            return 0u;
        }
        if (equal_folded(plan->path, absolute_output)) {
            plan->error = UZ_CREATE_PLAN_ERR_OUTPUT_INSIDE;
            return 0u;
        }
        if (plan->record->directory) {
            selected_len = strlen(plan->path);
            if (prefix_folded(plan->path, absolute_output) &&
                absolute_output[selected_len] == '/') {
                plan->error = UZ_CREATE_PLAN_ERR_OUTPUT_INSIDE;
                return 0u;
            }
        }
    }
    return 1u;
}

static unsigned char append_child(UzCreatePlan *plan,
                                  unsigned int parent_index,
                                  const UzBrowserEntry *entry) {
    unsigned int parent_len;
    unsigned int child_len;
    unsigned int needed;

    if (entry->unusable || !uz_catalog_get(plan->catalog, parent_index,
                                            plan->record)) {
        if (entry->unusable) plan->error = UZ_CREATE_PLAN_ERR_NAME;
        else map_catalog_error(plan);
        return 0u;
    }
    parent_len = strlen(plan->record->name);
    child_len = strlen(entry->name);
    needed = parent_len + child_len + (entry->directory ? 2u : 1u);
    if (child_len == 0u || needed > UZ_ZIP_NAME_CAP) {
        plan->error = UZ_CREATE_PLAN_ERR_NAME;
        return 0u;
    }
    memcpy(plan->record->name + parent_len, entry->name, child_len);
    parent_len = (unsigned int)(parent_len + child_len);
    if (entry->directory) plan->record->name[parent_len++] = '/';
    plan->record->name[parent_len] = 0;
    reset_record_payload(plan->record, entry->directory, plan->file_method);
    return append_candidate(plan);
}

unsigned char uz_create_plan_build(UzCreatePlan *plan,
                                   const char *absolute_output) {
    unsigned int index;
    unsigned char page_number;
    unsigned char entry_index;

    if (plan == 0 || plan->error != UZ_CREATE_PLAN_OK || plan->built) {
        if (plan != 0 && plan->error == UZ_CREATE_PLAN_OK)
            plan->error = UZ_CREATE_PLAN_ERR_STATE;
        return 0u;
    }
    plan->built = 1u;
    if (plan->seed_count == 0u) {
        plan->error = UZ_CREATE_PLAN_ERR_EMPTY;
        return 0u;
    }
    if (!output_safe(plan, absolute_output)) return 0u;

    for (index = 0u; index < plan->catalog->count; ++index) {
        if (!uz_catalog_get(plan->catalog, index, plan->record))
            return map_catalog_error(plan);
        if (!plan->record->directory) continue;
        if (!compose_absolute(plan, plan->record)) {
            plan->error = UZ_CREATE_PLAN_ERR_PATH;
            return 0u;
        }
        page_number = 0u;
        do {
            if (!plan->list(plan->list_context, plan->path, page_number,
                            plan->page)) {
                plan->error = UZ_CREATE_PLAN_ERR_LIST;
                return 0u;
            }
            if (plan->page->count > UZ_BROWSER_ROWS ||
                (plan->page->more && plan->page->count == 0u) ||
                plan->page->unusable != 0u) {
                plan->error = plan->page->unusable != 0u ?
                    UZ_CREATE_PLAN_ERR_NAME : UZ_CREATE_PLAN_ERR_PAGE;
                return 0u;
            }
            for (entry_index = 0u; entry_index < plan->page->count;
                 ++entry_index) {
                if (!append_child(plan, index,
                                  &plan->page->entries[entry_index]))
                    return 0u;
            }
            if (plan->page->more) {
                if (page_number == 0xFFu) {
                    plan->error = UZ_CREATE_PLAN_ERR_PAGE;
                    return 0u;
                }
                ++page_number;
                /* Each callback fully drains one READ_DIR transaction. The
                 * next page may rescan the directory, but no UCI transaction
                 * remains active while catalog/REU work occurs. */
                if (!uz_catalog_get(plan->catalog, index, plan->record) ||
                    !compose_absolute(plan, plan->record)) {
                    plan->error = UZ_CREATE_PLAN_ERR_PATH;
                    return 0u;
                }
            }
        } while (plan->page->more);
    }
    plan->error = UZ_CREATE_PLAN_OK;
    return 1u;
}

#if defined(UZIP_READYOS_APP) || defined(UZIP_CREATE_PLAN_OVERLAY)
#pragma rodata-name(pop)
#pragma code-name(pop)
#endif
