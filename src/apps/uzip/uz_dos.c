#include "uz_dos.h"

#include <string.h>

#define DOS_IDENTIFY   0x01u
#define DOS_OPEN       0x02u
#define DOS_CLOSE      0x03u
#define DOS_READ       0x04u
#define DOS_WRITE      0x05u
#define DOS_SEEK       0x06u
#define DOS_FILE_INFO  0x07u
#define DOS_FILE_STAT  0x08u
#define DOS_DELETE     0x09u
#define DOS_RENAME     0x0Au
#define DOS_CHANGE_DIR 0x11u
#define DOS_GET_PATH   0x12u
#define DOS_OPEN_DIR   0x13u
#define DOS_READ_DIR   0x14u
#define DOS_CREATE_DIR 0x16u
#define DOS_LOAD_REU   0x21u
#define DOS_SAVE_REU   0x22u

static unsigned char dos_ascii(unsigned char value) {
    if (value >= 0xC1u && value <= 0xDAu) return (unsigned char)(value - 0x80u);
    if (value == 0xA4u) return 0x5Fu;
    return value;
}

static void reset_transfer(UzDos *dos) {
    memset(&dos->transfer, 0, sizeof(dos->transfer));
    dos->transfer.data = dos->data;
    dos->transfer.data_cap = dos->data_cap;
    dos->transfer.stat = dos->status;
    dos->transfer.stat_cap = dos->status_cap;
}

static void remember_failure(UzDos *dos, const char *fallback) {
    unsigned int index;

    if (dos->transfer.flags != 0u) {
        strcpy(dos->message, "uci transport failure");
        return;
    }
    if (dos->transfer.stat_len != 0u) {
        index = 0u;
        while (index + 1u < sizeof(dos->message) &&
               index < dos->transfer.stat_len) {
            dos->message[index] =
                (dos->status[index] >= 32u && dos->status[index] < 127u)
                    ? (char)dos->status[index] : '.';
            ++index;
        }
        dos->message[index] = 0;
        return;
    }
    strncpy(dos->message, fallback, sizeof(dos->message) - 1u);
    dos->message[sizeof(dos->message) - 1u] = 0;
}

static unsigned char target_ok(const UzDos *dos) {
    return (unsigned char)(dos->transfer.stat_len >= 2u &&
                           dos->status[0] == '0' && dos->status[1] == '0');
}

static unsigned char target_ok_or_quiet(const UzDos *dos) {
    return (unsigned char)(dos->transfer.stat_len == 0u || target_ok(dos));
}

static unsigned char call_raw(UzDos *dos, unsigned int length,
                              unsigned char allow_quiet) {
    reset_transfer(dos);
    /* Every Ultimate DOS operation uses the shared uZIP state-machine
     * gateway. It owns synchronization and asynchronous PUSH/ABORT handling.
     * It owns complete queue drains and DATA_ACC transitions, plus the final
     * quiet-IDLE wait. This is the final quiet-IDLE wait contract. */
    if (!uz_uci_command(dos->command, length, &dos->transfer)) {
        remember_failure(dos, "uci command failed");
        return 0u;
    }
    if (!target_ok(dos) && !(allow_quiet && target_ok_or_quiet(dos))) {
        remember_failure(dos, "ultimate dos rejected command");
        return 0u;
    }
    dos->message[0] = 0;
    return 1u;
}

static unsigned char call(UzDos *dos, unsigned char command,
                          const char *argument) {
    unsigned int length;

    length = 2u;
    dos->command[0] = dos->target;
    dos->command[1] = command;
    if (argument != 0) {
        while (*argument != 0 && length < dos->command_cap) {
            dos->command[length++] = dos_ascii((unsigned char)*argument++);
        }
        if (*argument != 0) {
            strcpy(dos->message, "path too long");
            return 0u;
        }
    }
    return call_raw(dos, length, 0u);
}

void uz_dos_init(UzDos *dos, unsigned char target,
                 unsigned char *command, unsigned int command_cap,
                 unsigned char *data, unsigned int data_cap,
                 unsigned char *status, unsigned int status_cap) {
    memset(dos, 0, sizeof(*dos));
    dos->target = target;
    dos->command = command;
    dos->command_cap = command_cap;
    dos->data = data;
    dos->data_cap = data_cap;
    dos->status = status;
    dos->status_cap = status_cap;
}

const char *uz_dos_message(const UzDos *dos) {
    return dos->message;
}

#ifdef UZIP_READYOS_APP
#pragma code-name(push, "UI_CODE")
#endif
unsigned char uz_dos_identify(UzDos *dos) {
    if (!uz_uci_detect()) {
        strcpy(dos->message, "uci unavailable");
        return 0u;
    }
    if (!call(dos, DOS_IDENTIFY, 0)) return 0u;
    /* A cold target normally supplies a human-readable identity payload.
     * Physical firmware 3.14 may return only status 00 when ReadyOS launcher
     * has already identified/used the same target to DMA-load this app. The
     * protocol status plus a detected UCI is the launcher-proven authority;
     * the following absolute-path command remains the functional DOS check. */
    return 1u;
}
#ifdef UZIP_READYOS_APP
#pragma code-name(pop)
#endif

#ifndef UZ_DOS_INFLATE_PROBE_MINIMAL
#ifdef UZIP_READYOS_APP
#pragma code-name(push, "UI_CODE")
#endif
unsigned char uz_dos_change_absolute(UzDos *dos, const char *path) {
    if (dos == 0) return 0u;
    if (path == 0 || path[0] != '/') {
        strcpy(dos->message, "absolute path required");
        return 0u;
    }
    /* Ultimate's Path::cd parser treats a leading slash as an atomic reset
     * and accepts the remaining nested path in the same command.  The xuzio
     * physical probe has promoted that exact full-path form.  Keeping this
     * operation atomic also avoids transient component state after CLOSE. */
    return call(dos, DOS_CHANGE_DIR, path);
}
#ifdef UZIP_READYOS_APP
#pragma code-name(pop)
#endif
#endif

unsigned char uz_dos_change_path(UzDos *dos, const char *path) {
    if (path == 0 || path[0] == 0) {
        strcpy(dos->message, "path required");
        return 0u;
    }
    return call(dos, DOS_CHANGE_DIR, path);
}

#ifndef UZ_DOS_INFLATE_PROBE_MINIMAL
#ifdef UZIP_READYOS_APP
#pragma code-name(push, "UI_CODE")
#endif
unsigned char uz_dos_get_path(UzDos *dos) {
    return call(dos, DOS_GET_PATH, 0);
}

#ifdef UZIP_READYOS_APP
#endif
#endif

unsigned char uz_dos_create_dir(UzDos *dos, const char *name) {
    return call(dos, DOS_CREATE_DIR, name);
}

#ifndef UZ_DOS_INFLATE_PROBE_MINIMAL
unsigned char uz_dos_open_dir(UzDos *dos) {
    return call(dos, DOS_OPEN_DIR, 0);
}

unsigned char uz_dos_read_dir(UzDos *dos, UzUciBlockHandler handler) {
    dos->command[0] = dos->target;
    dos->command[1] = DOS_READ_DIR;
    reset_transfer(dos);
    dos->transfer.on_block = handler;
    /* READ_DIR uses the same complete async gateway and drains every entry
     * block even if the consumer elects not to retain it. */
    if (!uz_uci_command(dos->command, 2u, &dos->transfer)) {
        remember_failure(dos, "directory read failed");
        return 0u;
    }
    dos->message[0] = 0;
    return 1u;
}
#endif

unsigned char uz_dos_open(UzDos *dos, const char *name, unsigned char flags) {
    unsigned int length;

    if (dos->file_open) {
        strcpy(dos->message, "target file already open");
        return 0u;
    }
    if (dos->command_cap < 4u) return 0u;
    dos->command[0] = dos->target;
    dos->command[1] = DOS_OPEN;
    dos->command[2] = flags;
    length = 3u;
    while (*name != 0 && length < dos->command_cap) {
        dos->command[length++] = dos_ascii((unsigned char)*name++);
    }
    if (*name != 0) {
        strcpy(dos->message, "file name too long");
        return 0u;
    }
    if (!call_raw(dos, length, 0u)) return 0u;
    dos->file_open = 1u;
    return 1u;
}

unsigned char uz_dos_close(UzDos *dos) {
    unsigned char result;

    if (!dos->file_open) return 1u;
    result = call(dos, DOS_CLOSE, 0);
    dos->file_open = 0u;
    return result;
}

#ifdef UZIP_READYOS_APP
#pragma code-name(pop)
#pragma code-name(push, "JOB_CODE")
#endif

unsigned char uz_dos_job_close(UzDos *dos) {
    unsigned char result;

    if (!dos->file_open) return 1u;
    result = call(dos, DOS_CLOSE, 0);
    dos->file_open = 0u;
    return result;
}

int uz_dos_read(UzDos *dos, void *destination, unsigned int length) {
    if (!dos->file_open || length > dos->data_cap || length > UZ_DOS_QUEUE_MAX) {
        strcpy(dos->message, "invalid read length");
        return -1;
    }
    dos->command[0] = dos->target;
    dos->command[1] = DOS_READ;
    dos->command[2] = (unsigned char)length;
    dos->command[3] = (unsigned char)(length >> 8u);
    if (!call_raw(dos, 4u, 1u)) return -1;
    if (dos->transfer.data_len != 0u) {
        memcpy(destination, dos->data, dos->transfer.data_len);
    }
    return (int)dos->transfer.data_len;
}

unsigned char uz_dos_write(UzDos *dos, const void *source,
                           unsigned int length) {
    if (!dos->file_open || length > UZ_DOS_WRITE_MAX ||
        length + 4u > dos->command_cap) {
        strcpy(dos->message, "invalid write length");
        return 0u;
    }
    dos->command[0] = dos->target;
    dos->command[1] = DOS_WRITE;
    dos->command[2] = 0u;
    dos->command[3] = 0u;
    memcpy(dos->command + 4u, source, length);
    return call_raw(dos, length + 4u, 1u);
}

#ifndef UZ_DOS_INFLATE_PROBE_MINIMAL
#ifdef UZIP_READYOS_APP
/* Direct file/REU transfer is shared by every active codec phase, so it stays
 * in the job-safe resident core rather than being duplicated in an overlay. */
#pragma code-name(pop)
#endif

static unsigned char hex_digit(unsigned char value, unsigned char *digit) {
    if (value >= '0' && value <= '9') {
        *digit = (unsigned char)(value - '0');
        return 1u;
    }
    if (value >= 'A' && value <= 'F') {
        *digit = (unsigned char)(value - 'A' + 10u);
        return 1u;
    }
    if (value >= 'a' && value <= 'f') {
        *digit = (unsigned char)(value - 'a' + 10u);
        return 1u;
    }
    return 0u;
}

static unsigned char reu_transferred(UzDos *dos, unsigned int requested,
                                     unsigned int *transferred) {
    unsigned int index;
    unsigned int value;
    unsigned char digit;
    unsigned char found;

    index = 0u;
    while (index < dos->transfer.data_len && dos->data[index] != '$') ++index;
    if (index == dos->transfer.data_len) return 0u;
    ++index;
    while (index < dos->transfer.data_len && dos->data[index] == ' ') ++index;
    value = 0u;
    found = 0u;
    while (index < dos->transfer.data_len &&
           hex_digit(dos->data[index], &digit)) {
        if (value > 0x0FFFu) return 0u;
        value = (unsigned int)((value << 4u) | digit);
        found = 1u;
        ++index;
    }
    if (!found || value > requested) return 0u;
    *transferred = value;
    return 1u;
}

unsigned char uz_dos_reu_transfer(UzDos *dos, unsigned char command,
                                  unsigned char bank, unsigned int offset,
                                  unsigned int length,
                                  unsigned int *transferred) {
    if (!dos->file_open || dos->command_cap < 10u || bank == 0xFFu ||
        (command != DOS_LOAD_REU && command != DOS_SAVE_REU) || length == 0u ||
        transferred == 0 ||
        length - 1u > (unsigned int)(0xFFFFu - offset)) {
        strcpy(dos->message, "invalid reu transfer");
        return 0u;
    }
    dos->command[0] = dos->target;
    dos->command[1] = command;
    dos->command[2] = (unsigned char)offset;
    dos->command[3] = (unsigned char)(offset >> 8u);
    dos->command[4] = bank;
    dos->command[5] = 0u;
    dos->command[6] = (unsigned char)length;
    dos->command[7] = (unsigned char)(length >> 8u);
    dos->command[8] = 0u;
    dos->command[9] = 0u;
    if (!call_raw(dos, 10u, 0u) ||
        !reu_transferred(dos, length, transferred)) {
        if (dos->message[0] == 0) strcpy(dos->message, "bad reu byte count");
        return 0u;
    }
    return 1u;
}

#endif

#ifndef UZ_DOS_INFLATE_PROBE_MINIMAL
/* Random-access ZIP parsing replaces the Store image at $B000. Keep SEEK
 * beside the already-resident direct file/REU gateway so the parser's
 * callback never calls code in the overlay it displaced. */
unsigned char uz_dos_seek(UzDos *dos, const UzU32 *offset) {
    /* Callers already own the operation-specific error text; avoiding a
     * second resident literal keeps this gateway inside the $1000-$2FFF
     * contract. */
    if (!dos->file_open) return 0u;
    dos->command[0] = dos->target;
    dos->command[1] = DOS_SEEK;
    uz_u32_to_le(dos->command + 2u, offset);
    return call_raw(dos, 6u, 0u);
}
#endif

#ifdef UZIP_READYOS_APP
#pragma code-name(push, "UI_CODE")
#endif

unsigned char uz_dos_file_info(UzDos *dos, UzU32 *size) {
    if (!dos->file_open || !call(dos, DOS_FILE_INFO, 0)) return 0u;
    if (dos->transfer.data_len < 4u) {
        strcpy(dos->message, "short file info");
        return 0u;
    }
    uz_u32_from_le(size, dos->data);
    return 1u;
}

#ifndef UZ_DOS_INFLATE_PROBE_MINIMAL
unsigned char uz_dos_file_stat(UzDos *dos, const char *name, UzU32 *size) {
    if (!call(dos, DOS_FILE_STAT, name)) return 0u;
    if (dos->transfer.data_len < 4u) {
        strcpy(dos->message, "short file stat");
        return 0u;
    }
    uz_u32_from_le(size, dos->data);
    return 1u;
}
unsigned char uz_dos_delete(UzDos *dos, const char *name) {
    return call(dos, DOS_DELETE, name);
}

unsigned char uz_dos_rename(UzDos *dos, const char *old_name,
                            const char *new_name) {
    unsigned int length;

    length = 2u;
    dos->command[0] = dos->target;
    dos->command[1] = DOS_RENAME;
    while (*old_name != 0 && length < dos->command_cap) {
        dos->command[length++] = dos_ascii((unsigned char)*old_name++);
    }
    if (*old_name != 0 || length >= dos->command_cap) {
        strcpy(dos->message, "rename name too long");
        return 0u;
    }
    dos->command[length++] = 0u;
    while (*new_name != 0 && length < dos->command_cap) {
        dos->command[length++] = dos_ascii((unsigned char)*new_name++);
    }
    if (*new_name != 0) {
        strcpy(dos->message, "rename name too long");
        return 0u;
    }
    return call_raw(dos, length, 0u);
}
#ifdef UZIP_READYOS_APP
#pragma code-name(pop)
#endif
#endif
