#include "setup_backend.h"
#include "setup_uci.h"

#include <string.h>

#define DOS_TARGET       1u
#define DOS_IDENTIFY     0x01u
#define DOS_OPEN         0x02u
#define DOS_CLOSE        0x03u
#define DOS_READ         0x04u
#define DOS_WRITE        0x05u
#define DOS_DELETE       0x09u
#define DOS_RENAME       0x0Au
#define DOS_CHANGE_DIR   0x11u
#define DOS_OPEN_DIR     0x13u
#define DOS_READ_DIR     0x14u
#define DOS_MOUNT        0x23u

static unsigned char command[256];
static unsigned char data[240];
static unsigned char stat[40];
static SetupUciTransfer transfer;
static char status_text[40];
static char name_scratch[SETUP_NAME_CAP];
static SetupPage *list_page;
static unsigned int list_skip;

static void prepare_transfer(void) {
    memset(&transfer, 0, sizeof(transfer));
    transfer.data = data;
    transfer.data_cap = sizeof(data);
    transfer.stat = stat;
    transfer.stat_cap = sizeof(stat);
}

static unsigned char target_ok(void) {
    return (unsigned char)(transfer.stat_len >= 2u &&
                           stat[0] == '0' && stat[1] == '0');
}

static void remember_status(const char *fallback) {
    unsigned int i;
    if (transfer.flags != 0u) {
        strcpy(status_text, "uci transport failure");
        return;
    }
    if (transfer.stat_len != 0u) {
        i = 0u;
        while (i + 1u < sizeof(status_text) && i < transfer.stat_len) {
            status_text[i] = (stat[i] >= 32u && stat[i] < 127u) ?
                             (char)stat[i] : '.';
            ++i;
        }
        status_text[i] = 0;
        return;
    }
    strncpy(status_text, fallback, sizeof(status_text) - 1u);
    status_text[sizeof(status_text) - 1u] = 0;
}

const char *setup_backend_status(void) { return status_text; }

static unsigned char call_raw_mode(unsigned int length, unsigned char allow_quiet) {
    prepare_transfer();
    /* All app-level commands use SETUP's sole state-machine gateway. It owns
     * async PUSH/ABORT, queue drains, DATA_ACC, and quiet-IDLE completion. */
    if (!setup_uci_command(command, length, &transfer)) {
        remember_status("uci command failed");
        return 0u;
    }
    if (!target_ok() && !(allow_quiet && transfer.stat_len == 0u)) {
        remember_status("ultimate dos rejected command");
        return 0u;
    }
    status_text[0] = 0;
    return 1u;
}

static unsigned char call_raw(unsigned int length) {
    return call_raw_mode(length, 0u);
}

static unsigned char call(unsigned char code, const char *arg) {
    unsigned int length = 2u;
    command[0] = DOS_TARGET;
    command[1] = code;
    if (arg != 0) {
        while (*arg != 0 && length < sizeof(command))
            command[length++] = (unsigned char)*arg++;
        if (*arg != 0) { strcpy(status_text, "path too long"); return 0u; }
    }
    return call_raw(length);
}

unsigned char setup_backend_identify(void) {
    if (!setup_uci_detect()) {
        strcpy(status_text, "uci unavailable");
        return 0u;
    }
    if (!call(DOS_IDENTIFY, 0)) return 0u;
    /* Target 1 plus an OK status is the protocol identity authority. Firmware
     * generations expose the human-readable identity in different C64-facing
     * byte cases, so require a real reply without assuming one spelling. */
    if (transfer.data_len == 0u) {
        strcpy(status_text, "ultimate dos unavailable");
        return 0u;
    }
    return 1u;
}

static unsigned char change_absolute(const char *path) {
    char component[SETUP_NAME_CAP];
    unsigned char n;
    if (!call(DOS_CHANGE_DIR, "/")) return 0u;
    while (*path == '/') ++path;
    while (*path != 0) {
        n = 0u;
        while (*path != 0 && *path != '/') {
            if (n + 1u >= sizeof(component)) {
                strcpy(status_text, "folder name too long"); return 0u;
            }
            component[n++] = *path++;
        }
        component[n] = 0;
        while (*path == '/') ++path;
        if (n != 0u && !call(DOS_CHANGE_DIR, component)) return 0u;
    }
    return 1u;
}

static unsigned char ends_d81(const char *name) {
    unsigned int n = strlen(name);
    unsigned char a;
    unsigned char b;
    unsigned char c;
    if (n < 4u || (unsigned char)name[n - 4u] != 0x2Eu) return 0u;
    a = (unsigned char)name[n - 3u];
    b = (unsigned char)name[n - 2u];
    c = (unsigned char)name[n - 1u];
    if (a >= 0x41u && a <= 0x5Au) a = (unsigned char)(a + 0x20u);
    if (b >= 0x41u && b <= 0x5Au) b = (unsigned char)(b + 0x20u);
    /* Directory-stream names are Ultimate DOS ASCII. Use byte values here:
     * cc65 character literals are target-charset translated. */
    return (unsigned char)(a == 0x64u && b == 0x38u && c == 0x31u);
}

static void collect_entry(const unsigned char *block, unsigned int length,
                          const unsigned char *block_stat,
                          unsigned int stat_len) {
    unsigned char directory;
    unsigned int n;
    SetupEntry *entry;
    (void)block_stat;
    (void)stat_len;
    if (list_page == 0 || length < 2u) return;
    directory = (unsigned char)((block[0] & 0x10u) != 0u);
    n = length - 1u;
    if (n >= SETUP_NAME_CAP) n = SETUP_NAME_CAP - 1u;
    memcpy(name_scratch, block + 1u, n);
    name_scratch[n] = 0;
    if (!directory && !ends_d81(name_scratch)) return;
    if (list_page->total++ < list_skip) return;
    if (list_page->count >= SETUP_PAGE_ROWS) { list_page->more = 1u; return; }
    entry = &list_page->entries[list_page->count++];
    memcpy(entry->name, block + 1u, n);
    entry->name[n] = 0;
    entry->directory = directory;
}

unsigned char setup_backend_list(const char *path, unsigned char page,
                                 SetupPage *out) {
    if (out == 0 || !change_absolute(path) || !call(DOS_OPEN_DIR, 0)) return 0u;
    memset(out, 0, sizeof(*out));
    out->page = page;
    list_page = out;
    list_skip = (unsigned int)page * SETUP_PAGE_ROWS;
    command[0] = DOS_TARGET;
    command[1] = DOS_READ_DIR;
    prepare_transfer();
    transfer.on_block = collect_entry;
    /* READ_DIR is a multi-block stream; the same transport drains every entry
     * even after this page is full and performs every DATA_ACC transition. */
    if (!setup_uci_command(command, 2u, &transfer)) {
        remember_status("directory read failed");
        list_page = 0;
        return 0u;
    }
    list_page = 0;
    return 1u;
}

static unsigned char split_image(const char *full, char *parent, char *leaf) {
    const char *slash;
    unsigned int n;
    if (full == 0 || full[0] != '/' || !ends_d81(full)) return 0u;
    slash = strrchr(full, '/');
    if (slash == 0 || slash[1] == 0 || strlen(slash + 1u) >= SETUP_NAME_CAP)
        return 0u;
    n = (unsigned int)(slash - full);
    if (n == 0u) strcpy(parent, "/");
    else { memcpy(parent, full, n); parent[n] = 0; }
    strcpy(leaf, slash + 1u);
    return 1u;
}

static unsigned char enter_image(const char *full) {
    static char parent[SETUP_PATH_CAP];
    static char leaf[SETUP_NAME_CAP];
    unsigned int length;
    if (!split_image(full, parent, leaf)) {
        strcpy(status_text, "not a d81 path"); return 0u;
    }
    if (!change_absolute(parent)) return 0u;
    command[0] = DOS_TARGET;
    command[1] = DOS_MOUNT;
    command[2] = 8u;
    strcpy((char *)command + 3u, leaf);
    length = (unsigned int)(3u + strlen(leaf));
    prepare_transfer();
    /* MOUNT follows the same target-status contract proven by ReadyFS; the
     * following CHANGE_DIR and APPS.CFG open additionally validate content. */
    if (!setup_uci_command(command, length, &transfer) || !target_ok()) {
        remember_status("mount transport failed"); return 0u;
    }
    if (!call(DOS_CHANGE_DIR, leaf)) return 0u;
    return 1u;
}

static unsigned char open_file(const char *name, unsigned char flags) {
    unsigned int length;
    command[0] = DOS_TARGET;
    command[1] = DOS_OPEN;
    command[2] = flags;
    strcpy((char *)command + 3u, name);
    length = (unsigned int)(3u + strlen(name));
    return call_raw(length);
}

static void close_file(void) { (void)call(DOS_CLOSE, 0); }

static unsigned char read_named(const char *name, unsigned char *config,
                                unsigned int *length) {
    unsigned int used = 0u;
    if (!open_file(name, 0x01u)) return 0u;
    for (;;) {
        command[0] = DOS_TARGET;
        command[1] = DOS_READ;
        command[2] = sizeof(data);
        command[3] = 0u;
        if (!call_raw_mode(4u, 1u)) { close_file(); return 0u; }
        if (transfer.data_len == 0u) break;
        if (used > SETUP_CONFIG_CAP - transfer.data_len) {
            strcpy(status_text, "apps.cfg too large"); close_file(); return 0u;
        }
        memcpy(config + used, data, transfer.data_len);
        used = (unsigned int)(used + transfer.data_len);
        if (transfer.data_len < sizeof(data)) break;
    }
    close_file();
    *length = used;
    return 1u;
}

static unsigned char read_config(unsigned char *config, unsigned int *length) {
    /* Ultimate DOS exposes mounted-image type as a final FAT-style suffix:
     * APPS.CFG (SEQ) is addressed as APPS.CFG.SEQ. */
    return read_named("apps.cfg.seq", config, length);
}

static unsigned char delete_name(const char *name, unsigned char allow_missing) {
    if (call(DOS_DELETE, name)) return 1u;
    return allow_missing;
}

static unsigned char rename_name(const char *old_name, const char *new_name) {
    unsigned int length = 2u;
    command[0] = DOS_TARGET;
    command[1] = DOS_RENAME;
    strcpy((char *)command + length, old_name);
    length = (unsigned int)(length + strlen(old_name) + 1u);
    strcpy((char *)command + length, new_name);
    length = (unsigned int)(length + strlen(new_name));
    return call_raw(length);
}

static unsigned char write_config(const unsigned char *config, unsigned int length) {
    unsigned int offset = 0u;
    unsigned int chunk;
    /* Ultimate DOS derives a newly created mounted-image file's C64 type
     * from its extension. Stage as .seq so the final APPS.CFG remains SEQ. */
    if (!open_file("rdyset.seq", 0x06u)) return 0u;
    while (offset < length) {
        chunk = (unsigned int)(length - offset);
        if (chunk > 220u) chunk = 220u;
        command[0] = DOS_TARGET;
        command[1] = DOS_WRITE;
        command[2] = 0u;
        command[3] = 0u;
        memcpy(command + 4u, config + offset, chunk);
        if (!call_raw_mode(chunk + 4u, 1u)) { close_file(); return 0u; }
        offset = (unsigned int)(offset + chunk);
    }
    close_file();
    return 1u;
}

unsigned char setup_backend_validate_image(const char *full_path,
                                           unsigned char *config,
                                           unsigned int *config_len) {
    if (!enter_image(full_path)) return 0u;
    return read_config(config, config_len);
}

unsigned char setup_backend_configure_image(const char *full_path,
                                            unsigned char *config,
                                            unsigned int *config_len) {
    static unsigned char verify[SETUP_CONFIG_CAP];
    unsigned int verify_len;
    if (!enter_image(full_path) || !read_config(config, config_len)) return 0u;
    if (!setup_config_prepare(config, config_len, SETUP_CONFIG_CAP, full_path)) {
        strcpy(status_text, "apps.cfg keys missing"); return 0u;
    }
    if (!write_config(config, *config_len)) return 0u;
    if (!read_named("rdyset.seq", verify, &verify_len) || verify_len != *config_len ||
        memcmp(verify, config, verify_len) != 0) {
        (void)delete_name("rdyset.seq", 1u);
        strcpy(status_text, "staged config verify failed"); return 0u;
    }
    if (!rename_name("apps.cfg.seq", "rdyset.bak.seq")) return 0u;
    if (!rename_name("rdyset.seq", "apps.cfg.seq")) {
        (void)rename_name("rdyset.bak.seq", "apps.cfg.seq");
        return 0u;
    }
    if (!read_config(verify, &verify_len) || verify_len != *config_len ||
        memcmp(verify, config, verify_len) != 0) {
        (void)rename_name("apps.cfg.seq", "rdyset.seq");
        (void)rename_name("rdyset.bak.seq", "apps.cfg.seq");
        strcpy(status_text, "committed config verify failed"); return 0u;
    }
    (void)delete_name("rdyset.bak.seq", 0u);
    status_text[0] = 0;
    return 1u;
}
