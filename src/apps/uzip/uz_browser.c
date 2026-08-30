#include "uz_browser.h"

#include <string.h>

#ifdef UZIP_READYOS_APP
#pragma code-name(push, "UI_CODE")
#pragma rodata-name(push, "UI_RODATA")
#pragma bss-name(push, "UI_BSS")
#endif

static UzBrowserPage *active_page;
static unsigned int active_skip;
static unsigned char active_folders_only;

unsigned char uz_browser_open_folder(UzDos *dos, const char *path) {
    if (dos == 0 || path == 0 || path[0] != '/' ||
        !uz_dos_change_absolute(dos, path)) return 0u;
    if (uz_dos_open_dir(dos)) return 1u;
    /* Physical Ultimate firmware 3.14 returns 01,DIRECTORY EMPTY after
     * successfully opening an empty folder. This exception is deliberately
     * scoped to browser OPEN_DIR; every other DOS operation still requires
     * its normal success status. */
    if (dos->transfer.flags == 0u && dos->transfer.stat_len >= 2u &&
        dos->status[0] == '0' && dos->status[1] == '1') {
        dos->message[0] = 0;
        return 1u;
    }
    return 0u;
}

static unsigned char dot_entry(const unsigned char *data,
                               unsigned int length) {
    return (unsigned char)(
        (length == 2u && data[1] == 0x2Eu) ||
        (length == 3u && data[1] == 0x2Eu && data[2] == 0x2Eu));
}

static void collect_entry(const unsigned char *data, unsigned int length,
                          const unsigned char *status,
                          unsigned int status_length) {
    UzBrowserEntry *entry;
    unsigned int name_length;
    unsigned int copy_length;
    unsigned char directory;

    (void)status;
    (void)status_length;
    if (active_page == 0 || length < 2u || dot_entry(data, length)) return;
    directory = (unsigned char)((data[0] & 0x10u) != 0u);
    if (active_folders_only && !directory) return;
    ++active_page->total;
    if (active_skip != 0u) {
        --active_skip;
        return;
    }
    if (active_page->count >= UZ_BROWSER_ROWS) {
        active_page->more = 1u;
        return;
    }
    entry = &active_page->entries[active_page->count++];
    name_length = length - 1u;
    entry->attributes = data[0];
    entry->directory = directory;
    entry->unusable = (unsigned char)(name_length >= UZ_BROWSER_NAME_CAP);
    if (entry->unusable) ++active_page->unusable;
    copy_length = name_length;
    if (copy_length >= UZ_BROWSER_NAME_CAP)
        copy_length = UZ_BROWSER_NAME_CAP - 1u;
    memcpy(entry->name, data + 1u, copy_length);
    entry->name[copy_length] = 0;
}

unsigned char uz_browser_list(UzDos *dos, const char *path,
                              unsigned char page,
                              unsigned char folders_only,
                              UzBrowserPage *out) {
    unsigned char result;

    if (dos == 0 || path == 0 || path[0] != '/' || out == 0 ||
        dos->data_cap < UZ_BROWSER_NAME_CAP + 1u ||
        (folders_only != UZ_BROWSER_SHOW_ALL &&
         folders_only != UZ_BROWSER_FOLDERS)) return 0u;
    memset(out, 0, sizeof(*out));
    out->page = page;
    if (!uz_browser_open_folder(dos, path)) return 0u;
    active_page = out;
    active_skip = (unsigned int)page * UZ_BROWSER_ROWS;
    active_folders_only = folders_only;
    /* uz_dos_read_dir uses the shared asynchronous UCI state machine. It owns
     * synchronization, PUSH/ABORT, complete queue draining, DATA_ACC, and the
     * final quiet-idle wait; this browser callback only retains one page. */
    result = uz_dos_read_dir(dos, collect_entry);
    active_page = 0;
    return result;
}

static unsigned char component_safe(const char *name, unsigned int *length) {
    unsigned int used;
    unsigned char value;

    if (name == 0 || name[0] == 0) return 0u;
    used = 0u;
    while (name[used] != 0) {
        value = (unsigned char)name[used];
        if (value < 0x20u || value > 0x7Eu || value == 0x2Fu ||
            value == 0x5Cu || value == 0x3Au ||
            used + 1u >= UZ_BROWSER_NAME_CAP) return 0u;
        ++used;
    }
    if ((used == 1u && name[0] == 0x2Eu) ||
        (used == 2u && name[0] == 0x2Eu && name[1] == 0x2Eu)) return 0u;
    *length = used;
    return 1u;
}

unsigned char uz_browser_enter(char *path, unsigned int capacity,
                               const char *name) {
    unsigned int path_length;
    unsigned int name_length;
    unsigned int needed;

    if (path == 0 || capacity < 2u || path[0] != '/' ||
        !component_safe(name, &name_length)) return 0u;
    path_length = strlen(path);
    while (path_length > 1u && path[path_length - 1u] == '/')
        path[--path_length] = 0;
    needed = path_length + name_length + 1u;
    if (path_length != 1u) ++needed;
    if (needed > capacity) return 0u;
    if (path_length != 1u) path[path_length++] = '/';
    memcpy(path + path_length, name, name_length);
    path[path_length + name_length] = 0;
    return 1u;
}

unsigned char uz_browser_parent(char *path) {
    unsigned int length;

    if (path == 0 || path[0] != '/') return 0u;
    length = strlen(path);
    if (length == 0u) return 0u;
    while (length > 1u && path[length - 1u] == '/') path[--length] = 0;
    if (length == 1u) return 1u;
    while (length > 0u && path[length - 1u] != '/') --length;
    if (length <= 1u) path[1] = 0;
    else path[length - 1u] = 0;
    return 1u;
}

unsigned char uz_browser_split_current(const char *path,
                                       char *parent,
                                       unsigned int parent_capacity,
                                       char *leaf,
                                       unsigned int leaf_capacity) {
    unsigned int end;
    unsigned int leaf_at;
    unsigned int leaf_length;
    unsigned int parent_length;

    if (path == 0 || parent == 0 || leaf == 0 || path[0] != '/' ||
        parent_capacity < 2u || leaf_capacity < 2u) return 0u;
    end = strlen(path);
    while (end > 1u && path[end - 1u] == '/') --end;
    if (end <= 1u) return 0u;
    leaf_at = end;
    while (leaf_at > 0u && path[leaf_at - 1u] != '/') --leaf_at;
    if (leaf_at == 0u || leaf_at >= end) return 0u;
    leaf_length = (unsigned int)(end - leaf_at);
    if (leaf_length + 1u > leaf_capacity) return 0u;
    memcpy(leaf, path + leaf_at, leaf_length);
    leaf[leaf_length] = 0;
    if (!component_safe(leaf, &leaf_length)) return 0u;
    parent_length = (unsigned int)(leaf_at - 1u);
    if (parent_length == 0u) {
        parent[0] = '/';
        parent[1] = 0;
    } else {
        if (parent_length + 1u > parent_capacity) return 0u;
        memcpy(parent, path, parent_length);
        parent[parent_length] = 0;
    }
    return 1u;
}

void uz_browser_display(char *destination, unsigned int capacity,
                        const char *host_ascii) {
    unsigned int index;
    unsigned char value;

    if (destination == 0 || capacity == 0u) return;
    index = 0u;
    if (host_ascii != 0) {
        while (host_ascii[index] != 0 && index + 1u < capacity) {
            value = (unsigned char)host_ascii[index];
            if (value >= 0x41u && value <= 0x5Au)
                value = (unsigned char)(value + 0x80u);
            else if (value >= 0x61u && value <= 0x7Au)
                value = (unsigned char)(value + 0x60u);
            else if (value == 0x5Fu)
                value = 0xA4u;
            destination[index++] = (char)value;
        }
    }
    destination[index] = 0;
}

#ifdef UZIP_READYOS_APP
#pragma bss-name(pop)
#pragma rodata-name(pop)
#pragma code-name(pop)
#endif
