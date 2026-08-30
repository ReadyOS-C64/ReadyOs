#include <assert.h>
#include <string.h>

#include "uz_dos.h"

#define TEST_PARTS 8u
#define TEST_NAME  32u

static char parts[TEST_PARTS][TEST_NAME];
static unsigned char part_count;
static unsigned char get_calls;
static unsigned char parent_calls;
static unsigned char enter_calls;

static void set_path(const char *a, const char *b, const char *c) {
    part_count = 0u;
    if (a != 0) strcpy(parts[part_count++], a);
    if (b != 0) strcpy(parts[part_count++], b);
    if (c != 0) strcpy(parts[part_count++], c);
}

static void status_ok(UzUciTransfer *transfer) {
    transfer->stat[0] = '0';
    transfer->stat[1] = '0';
    transfer->stat_len = 2u;
}

static void get_path_reply(UzUciTransfer *transfer) {
    char path[256];
    unsigned int index;
    unsigned int length;
    unsigned int copy;

    path[0] = 0;
    for (index = 0u; index < part_count; ++index) {
        strcat(path, "/");
        strcat(path, parts[index]);
    }
    length = strlen(path);
    copy = length;
    if (copy > transfer->data_cap) copy = transfer->data_cap;
    memcpy(transfer->data, path, copy);
    transfer->data_len = copy;
    transfer->data_seen = length;
    transfer->flags = (unsigned char)(copy != length ? UZ_UCI_TRUNC_DATA : 0u);
    status_ok(transfer);
}

unsigned char uz_uci_detect(void) {
    return 1u;
}

unsigned int uz_uci_base(void) {
    return 0xDF1Cu;
}

unsigned char uz_uci_command(const unsigned char *command,
                             unsigned int command_len,
                             UzUciTransfer *transfer) {
    unsigned int length;
    char name[TEST_NAME];

    assert(command != 0 && transfer != 0);
    assert(command_len >= 2u && command[0] == UZ_DOS_TARGET_READ);
    if (command[1] == 0x12u) {
        assert(command_len == 2u);
        ++get_calls;
        get_path_reply(transfer);
        return 1u;
    }
    assert(command[1] == 0x11u);
    length = command_len - 2u;
    assert(length != 0u && length < sizeof(name));
    memcpy(name, command + 2u, length);
    name[length] = 0;
    assert(name[0] == '/');
    set_path(0, 0, 0);
    if (strcmp(name, "/usb1/out/nest") == 0)
        set_path("usb1", "out", "nest");
    else
        assert(strcmp(name, "/") == 0);
    ++enter_calls;
    transfer->data_len = 0u;
    transfer->flags = 0u;
    status_ok(transfer);
    return 1u;
}

int main(void) {
    UzDos dos;
    unsigned char command[80];
    unsigned char data[8];
    unsigned char status[24];
    unsigned int before;

    uz_dos_init(&dos, UZ_DOS_TARGET_READ,
                command, sizeof(command), data, sizeof(data),
                status, sizeof(status));

    /* Absolute positioning is one atomic firmware path command. */
    set_path("usb1", "readyos_uzip_test", "owned");
    assert(uz_dos_change_absolute(&dos, "/usb1/out/nest"));
    assert(part_count == 3u);
    assert(strcmp(parts[0], "usb1") == 0);
    assert(strcmp(parts[1], "out") == 0);
    assert(strcmp(parts[2], "nest") == 0);
    assert(parent_calls == 0u && enter_calls == 1u && get_calls == 0u);

    before = get_calls;
    set_path(0, 0, 0);
    assert(uz_dos_change_absolute(&dos, "/"));
    assert(part_count == 0u && get_calls == before && enter_calls == 2u);

    before = get_calls;
    assert(!uz_dos_change_absolute(&dos, "relative"));
    assert(get_calls == before);

    return 0;
}
