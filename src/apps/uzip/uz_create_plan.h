#ifndef UZ_CREATE_PLAN_H
#define UZ_CREATE_PLAN_H

#include "uz_browser.h"
#include "uz_catalog.h"

#define UZ_CREATE_PLAN_OK                0u
#define UZ_CREATE_PLAN_ERR_STATE         1u
#define UZ_CREATE_PLAN_ERR_BASE          2u
#define UZ_CREATE_PLAN_ERR_EMPTY         3u
#define UZ_CREATE_PLAN_ERR_NAME          4u
#define UZ_CREATE_PLAN_ERR_PATH          5u
#define UZ_CREATE_PLAN_ERR_OUTPUT_PATH   6u
#define UZ_CREATE_PLAN_ERR_OUTPUT_INSIDE 7u
#define UZ_CREATE_PLAN_ERR_CONFLICT      8u
#define UZ_CREATE_PLAN_ERR_FULL          9u
#define UZ_CREATE_PLAN_ERR_CATALOG      10u
#define UZ_CREATE_PLAN_ERR_LIST         11u
#define UZ_CREATE_PLAN_ERR_PAGE         12u

typedef unsigned char (*UzCreatePlanList)(void *context,
                                          const char *absolute_path,
                                          unsigned char page,
                                          UzBrowserPage *result);

/* All large scratch is caller-owned so the final TUI can reuse its one
 * browser page, two catalog records, and absolute-path buffer. The catalog is
 * both the breadth-first work queue and the frozen central-directory plan. */
typedef struct {
    UzCatalog *catalog;
    UzCreatePlanList list;
    void *list_context;
    const char *source_base;
    char *path;
    unsigned int path_cap;
    UzBrowserPage *page;
    UzZipRecord *record;
    UzZipRecord *scratch;
    unsigned int seed_count;
    unsigned int files;
    unsigned int directories;
    unsigned char file_method;
    unsigned char built;
    unsigned char error;
} UzCreatePlan;

void uz_create_plan_init(UzCreatePlan *plan, UzCatalog *catalog,
                         UzCreatePlanList list, void *list_context,
                         const char *source_base, char *path,
                         unsigned int path_cap, UzBrowserPage *page,
                         UzZipRecord *record, UzZipRecord *scratch,
                         unsigned int file_method);

/* Seed one marked sibling below source_base. Directories are expanded only
 * after every mark and the output-safety preflight have succeeded. */
unsigned char uz_create_plan_seed(UzCreatePlan *plan, const char *name,
                                  unsigned char directory);

/* Reject an output equal to a marked file or anywhere below a marked folder,
 * then breadth-first enumerate all folders. No output file is opened here. */
unsigned char uz_create_plan_build(UzCreatePlan *plan,
                                   const char *absolute_output);

#endif
