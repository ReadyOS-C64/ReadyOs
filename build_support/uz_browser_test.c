#include "uz_browser.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned char listing_mode;
static unsigned char open_dir_mode;
static char changed_path[UZ_BROWSER_PATH_CAP];

unsigned char uz_dos_change_absolute(UzDos *dos, const char *path) {
    (void)dos;
    strcpy(changed_path, path);
    return 1u;
}

unsigned char uz_dos_open_dir(UzDos *dos) {
    dos->transfer.flags = 0u;
    dos->transfer.stat_len = 2u;
    dos->status[0] = '0';
    dos->status[1] = open_dir_mode == 1u ? '1' : '2';
    if (open_dir_mode == 0u) {
        dos->status[1] = '0';
        return 1u;
    }
    return 0u;
}

static void emit(UzUciBlockHandler handler, unsigned char attributes,
                 const char *name) {
    unsigned char block[UZ_BROWSER_NAME_CAP + 4u];
    unsigned int length;

    length = strlen(name);
    block[0] = attributes;
    memcpy(block + 1u, name, length);
    handler(block, length + 1u, 0, 0u);
}

unsigned char uz_dos_read_dir(UzDos *dos, UzUciBlockHandler handler) {
    unsigned char index;
    char name[8];
    char long_name[UZ_BROWSER_NAME_CAP + 2u];

    (void)dos;
    emit(handler, 0x10u, ".");
    emit(handler, 0x10u, "..");
    if (listing_mode == 0u) {
        for (index = 0u; index < 17u; ++index) {
            sprintf(name, "E%02u", index);
            emit(handler, (unsigned char)((index % 5u) == 0u ? 0x10u : 0u),
                 name);
        }
    } else {
        emit(handler, 0x10u, "FOLDER");
        emit(handler, 0u, "FILE.ZIP");
        memset(long_name, 'A', sizeof(long_name) - 1u);
        long_name[sizeof(long_name) - 1u] = 0;
        emit(handler, 0x10u, long_name);
    }
    return 1u;
}

static void test_pages(void) {
    UzDos dos;
    UzBrowserPage page;
    unsigned char data[UZ_BROWSER_NAME_CAP + 1u];
    unsigned char status[24];

    memset(&dos, 0, sizeof(dos));
    dos.data = data;
    dos.data_cap = sizeof(data);
    dos.status = status;
    open_dir_mode = 0u;
    listing_mode = 0u;
    assert(uz_browser_list(&dos, "/USB1", 0u, UZ_BROWSER_SHOW_ALL, &page));
    assert(strcmp(changed_path, "/USB1") == 0);
    assert(page.total == 17u && page.count == 14u && page.more == 1u);
    assert(strcmp(page.entries[0].name, "E00") == 0);
    assert(strcmp(page.entries[13].name, "E13") == 0);
    assert(uz_browser_list(&dos, "/USB1", 1u, UZ_BROWSER_SHOW_ALL, &page));
    assert(page.total == 17u && page.count == 3u && page.more == 0u);
    assert(strcmp(page.entries[0].name, "E14") == 0);
}

static void test_filter_and_limits(void) {
    UzDos dos;
    UzBrowserPage page;
    unsigned char data[UZ_BROWSER_NAME_CAP + 1u];
    unsigned char status[24];

    memset(&dos, 0, sizeof(dos));
    dos.data = data;
    dos.data_cap = sizeof(data);
    dos.status = status;
    open_dir_mode = 0u;
    listing_mode = 1u;
    assert(uz_browser_list(&dos, "/", 0u, UZ_BROWSER_FOLDERS, &page));
    assert(page.total == 2u && page.count == 2u && page.unusable == 1u);
    assert(strcmp(page.entries[0].name, "FOLDER") == 0);
    assert(page.entries[1].unusable == 1u);
    dos.data_cap = UZ_BROWSER_NAME_CAP;
    assert(!uz_browser_list(&dos, "/", 0u, UZ_BROWSER_FOLDERS, &page));
}

static void test_empty_folder_status(void) {
    UzDos dos;
    unsigned char data[UZ_BROWSER_NAME_CAP + 1u];
    unsigned char status[24];

    memset(&dos, 0, sizeof(dos));
    dos.data = data;
    dos.data_cap = sizeof(data);
    dos.status = status;
    open_dir_mode = 1u;
    assert(uz_browser_open_folder(&dos, "/USB1/EMPTY"));
    assert(strcmp(changed_path, "/USB1/EMPTY") == 0);
    assert(dos.message[0] == 0);
    open_dir_mode = 2u;
    assert(!uz_browser_open_folder(&dos, "/USB1/MISSING"));
    open_dir_mode = 0u;
}

static void test_paths(void) {
    char path[32];
    char before[32];
    char parent[32];
    char leaf[32];
    char display[16];

    strcpy(path, "/");
    assert(uz_browser_enter(path, sizeof(path), "USB1"));
    assert(strcmp(path, "/USB1") == 0);
    assert(uz_browser_enter(path, sizeof(path), "MY FILES"));
    assert(strcmp(path, "/USB1/MY FILES") == 0);
    assert(uz_browser_parent(path) && strcmp(path, "/USB1") == 0);
    assert(uz_browser_parent(path) && strcmp(path, "/") == 0);
    assert(uz_browser_parent(path) && strcmp(path, "/") == 0);
    strcpy(before, path);
    assert(!uz_browser_enter(path, sizeof(path), ".."));
    assert(!uz_browser_enter(path, sizeof(path), "BAD/NAME"));
    assert(strcmp(path, before) == 0);
    strcpy(path, "/123456789012345678901234567890");
    strcpy(before, path);
    assert(!uz_browser_enter(path, sizeof(path), "X"));
    assert(strcmp(path, before) == 0);
    assert(uz_browser_split_current("/USB1/TREE", parent, sizeof(parent),
                                    leaf, sizeof(leaf)));
    assert(strcmp(parent, "/USB1") == 0);
    assert(strcmp(leaf, "TREE") == 0);
    assert(uz_browser_split_current("/USB1/", parent, sizeof(parent),
                                    leaf, sizeof(leaf)));
    assert(strcmp(parent, "/") == 0);
    assert(strcmp(leaf, "USB1") == 0);
    assert(!uz_browser_split_current("/", parent, sizeof(parent),
                                     leaf, sizeof(leaf)));
    assert(!uz_browser_split_current("/USB1/BAD:NAME", parent,
                                     sizeof(parent), leaf, sizeof(leaf)));
    uz_browser_display(display, sizeof(display), "Az_09");
    assert((unsigned char)display[0] == 0xC1u);
    assert((unsigned char)display[1] == 0xDAu);
    assert((unsigned char)display[2] == 0xA4u);
    assert(display[3] == '0' && display[4] == '9' && display[5] == 0);
}

int main(void) {
    test_pages();
    test_filter_and_limits();
    test_empty_folder_status();
    test_paths();
    puts("uZIP browser paging/path tests passed");
    return 0;
}
