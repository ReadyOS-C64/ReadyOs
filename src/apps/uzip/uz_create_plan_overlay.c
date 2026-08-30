#include "uz_create_plan_overlay.h"

#include "uz_catalog.h"
#include "uz_create_plan.h"
#include "../../lib/reu_mgr.h"

#include <string.h>

#pragma code-name(push, "CREATE_PLAN_CODE")
#pragma rodata-name(push, "CREATE_PLAN_RODATA")
#pragma bss-name(push, "CREATE_PLAN_BSS")

static UzCatalog catalog;
static UzCreatePlan plan;
static UzZipRecord record;
static UzZipRecord scratch;
static unsigned char active_catalog_bank;

static unsigned char contains_separator(const char *name) {
    while (*name != 0) {
        if (*name == '/' || *name == '\\') return 1u;
        ++name;
    }
    return 0u;
}

static unsigned char catalog_write(void *context, unsigned int offset,
                                   const void *source,
                                   unsigned int length) {
    (void)context;
    if (active_catalog_bank == 0xFFu || offset > 0xFFFFu - length)
        return 0u;
    reu_dma_stash((unsigned int)source, active_catalog_bank, offset, length);
    return 1u;
}

static unsigned char catalog_read(void *context, unsigned int offset,
                                  void *destination,
                                  unsigned int length) {
    (void)context;
    if (active_catalog_bank == 0xFFu || offset > 0xFFFFu - length)
        return 0u;
    reu_dma_fetch((unsigned int)destination, active_catalog_bank,
                  offset, length);
    return 1u;
}

static unsigned char list_page(void *context, const char *path,
                               unsigned char page_number,
                               UzBrowserPage *result) {
    UzCreatePlanOverlayRequest *request;

    request = (UzCreatePlanOverlayRequest *)context;
    /* uz_browser_list delegates one fully drained READ_DIR transaction to the
     * shared asynchronous UCI transport. The overlay never paces the port or
     * leaves a queue block active across catalog DMA. */
    return uz_browser_list(request->dos, path, page_number,
                           UZ_BROWSER_SHOW_ALL, result);
}

static unsigned char import_seed(UzCreatePlanOverlayRequest *request,
                                 unsigned int index) {
    unsigned int offset;
    unsigned int length;
    unsigned char directory;

    offset = (unsigned int)(index * (unsigned int)sizeof(UzZipRecord));
    reu_dma_fetch((unsigned int)&scratch, active_catalog_bank,
                  offset, sizeof(scratch));
    length = strlen(scratch.name);
    directory = scratch.directory;
    if (length == 0u || length >= UZ_ZIP_NAME_CAP) return 0u;
    if (directory) {
        if (scratch.name[length - 1u] != '/') return 0u;
        --length;
    }
    if (length == 0u || length >= request->path_cap) return 0u;
    memcpy(request->path, scratch.name, length);
    request->path[length] = 0;
    if (contains_separator(request->path)) return 0u;
    return uz_create_plan_seed(&plan, request->path, directory);
}

unsigned char uz_create_plan_overlay_entry(
    UzCreatePlanOverlayRequest *request) {
    unsigned int index;

    if (request == 0) return 0u;
    request->entry_count = 0u;
    request->files = 0u;
    request->directories = 0u;
    request->error = UZ_CREATE_PLAN_ERR_STATE;
    if (request->dos == 0 || request->source_base == 0 ||
        request->absolute_output == 0 || request->path == 0 ||
        request->path_cap < 2u || request->page == 0 ||
        request->catalog_bank == 0xFFu || request->seed_count == 0u ||
        request->seed_count > UZ_CATALOG_MAX_ENTRIES ||
        (request->method != 0u && request->method != 8u)) return 0u;

    active_catalog_bank = request->catalog_bank;
    uz_catalog_init(&catalog, catalog_write, catalog_read, 0);
    uz_create_plan_init(&plan, &catalog, list_page, request,
                        request->source_base, request->path,
                        request->path_cap, request->page,
                        &record, &scratch, request->method);
    if (plan.error != UZ_CREATE_PLAN_OK) {
        request->error = plan.error;
        return 0u;
    }
    /* The raw marked siblings already occupy the first catalog slots. Read
     * each before uz_create_plan_seed validates and rewrites that same slot. */
    for (index = 0u; index < request->seed_count; ++index) {
        if (!import_seed(request, index)) {
            request->error = plan.error != UZ_CREATE_PLAN_OK ?
                             plan.error : UZ_CREATE_PLAN_ERR_NAME;
            return 0u;
        }
    }
    if (!uz_create_plan_build(&plan, request->absolute_output)) {
        request->error = plan.error;
        return 0u;
    }
    request->entry_count = catalog.count;
    request->files = plan.files;
    request->directories = plan.directories;
    request->error = UZ_CREATE_PLAN_OK;
    return 1u;
}

#pragma bss-name(pop)
#pragma rodata-name(pop)
#pragma code-name(pop)
