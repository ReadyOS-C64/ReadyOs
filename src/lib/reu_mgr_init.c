/*
 * reu_mgr_init.c - app-side ReadyOS-bank allocator initialization
 *
 * The launcher owns schema creation.  Apps must never create or rebuild global
 * state from a private RAM mirror; initialization is therefore intentionally a
 * validation-only no-op.  Allocation operations read the ReadyOS bank directly.
 */

#include "reu_mgr.h"

void reu_mgr_init(void) {
    /* Kept for source/ABI compatibility with existing apps. */
}
