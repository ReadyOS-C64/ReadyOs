#ifndef UZ_CREATE_PLAN_OVERLAY_H
#define UZ_CREATE_PLAN_OVERLAY_H

#include "uz_browser.h"

#define UZ_CREATE_PLAN_OVERLAY_VERSION     1u
#define UZ_CREATE_PLAN_OVERLAY_HEADER_SIZE 16u
#define UZ_CREATE_PLAN_OVERLAY_RUN          0x9000u
#define UZ_CREATE_PLAN_OVERLAY_MAX_SIZE     0x2000u
#define UZ_CREATE_PLAN_OVERLAY_CACHE_OFFSET 0xD000u
#define UZ_CREATE_PLAN_OVERLAY_SAVE_OFFSET  0xD000u

/* The planner is cached in the package bank at $D000-$EFFF. Its caller saves
 * the overlapping UI tail to the operation work bank, installs this modal
 * $9000-$AFFF image, then restores the UI before observing the result. The
 * seed records already occupy catalog slots 0..seed_count-1; the overlay
 * validates and rewrites those slots before breadth-first expansion. */
typedef struct {
    UzDos *dos;
    const char *source_base;
    const char *absolute_output;
    char *path;
    unsigned int path_cap;
    UzBrowserPage *page;
    unsigned int seed_count;
    unsigned int entry_count;
    unsigned int files;
    unsigned int directories;
    unsigned char catalog_bank;
    unsigned char method;
    unsigned char error;
} UzCreatePlanOverlayRequest;

unsigned char uz_create_plan_overlay_entry(
    UzCreatePlanOverlayRequest *request);

#endif
