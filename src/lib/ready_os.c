/*
 * ready_os.c - Ready OS API Implementation
 * Syscall wrappers that call into the shim
 *
 * For Commodore 64, compiled with CC65
 */

#include "ready_os.h"
#include <string.h>

/*---------------------------------------------------------------------------
 * Static helper for syscalls
 *---------------------------------------------------------------------------*/

/* Function pointer types for syscalls */
typedef unsigned char (*syscall_init_t)(void *header);
typedef void (*syscall_void_t)(void);
typedef void (*syscall_byte_t)(unsigned char val);
typedef unsigned char (*syscall_copy_t)(unsigned char type, void *data, unsigned int size);
typedef unsigned int (*syscall_paste_t)(void *buffer, unsigned int maxsize);
typedef void (*syscall_deeplink_t)(void *link);
typedef unsigned int (*syscall_query_t)(unsigned char type);

/* Syscall function pointers */
#define call_init       ((syscall_init_t)SYSCALL_INIT)
#define call_suspend    ((syscall_void_t)SYSCALL_SUSPEND)
#define call_exit       ((syscall_byte_t)SYSCALL_EXIT)
#define call_clip_copy  ((syscall_copy_t)SYSCALL_CLIP_COPY)
#define call_clip_paste ((syscall_paste_t)SYSCALL_CLIP_PASTE)
#define call_deeplink   ((syscall_deeplink_t)SYSCALL_DEEPLINK)
#define call_query      ((syscall_query_t)SYSCALL_QUERY)

/*---------------------------------------------------------------------------
 * Syscall Implementations
 *---------------------------------------------------------------------------*/

unsigned char ready_init(ReadyAppHeader *header) {
    return call_init(header);
}

void ready_suspend(void) {
    call_suspend();
}

void ready_exit(unsigned char code) {
    call_exit(code);
}

unsigned char ready_clip_copy(unsigned char type, void *data, unsigned int size) {
    return call_clip_copy(type, data, size);
}

unsigned int ready_clip_paste(void *buffer, unsigned int maxsize) {
    return call_clip_paste(buffer, maxsize);
}

unsigned int ready_clip_avail(void) {
    return ready_query(QUERY_CLIP_SIZE);
}

unsigned char ready_clip_type(void) {
    return (unsigned char)ready_query(QUERY_CLIP_TYPE);
}

void ready_deeplink(DeepLink *link) {
    call_deeplink(link);
}

unsigned int ready_query(unsigned char query_type) {
    return call_query(query_type);
}

/*---------------------------------------------------------------------------
 * Convenience Functions
 *---------------------------------------------------------------------------*/

unsigned char ready_app_count(void) {
    return (unsigned char)ready_query(QUERY_APP_COUNT);
}

unsigned char ready_current_app(void) {
    return (unsigned char)ready_query(QUERY_CURRENT_APP);
}

unsigned char ready_has_reu(void) {
    return (unsigned char)ready_query(QUERY_REU_STATUS);
}

void ready_launch(const char *app_name) {
    static DeepLink link;

    /* Clear link structure */
    memset(&link, 0, sizeof(link));

    /* Copy app name */
    strncpy(link.target_app, app_name, MAX_APP_NAME - 1);
    link.target_app[MAX_APP_NAME - 1] = 0;

    /* Set action to normal launch */
    link.action = DL_ACTION_LAUNCH;
    link.param_len = 0;

    /* Execute deeplink */
    ready_deeplink(&link);
}

void ready_open_file(const char *app_name, const char *filename) {
    static DeepLink link;
    unsigned char len;

    /* Clear link structure */
    memset(&link, 0, sizeof(link));

    /* Copy app name */
    strncpy(link.target_app, app_name, MAX_APP_NAME - 1);
    link.target_app[MAX_APP_NAME - 1] = 0;

    /* Set action to open file */
    link.action = DL_ACTION_OPEN_FILE;

    /* Copy filename to params */
    len = strlen(filename);
    if (len > MAX_DEEPLINK_PARAMS - 1) {
        len = MAX_DEEPLINK_PARAMS - 1;
    }
    memcpy(link.params, filename, len);
    link.params[len] = 0;
    link.param_len = len;

    /* Execute deeplink */
    ready_deeplink(&link);
}
