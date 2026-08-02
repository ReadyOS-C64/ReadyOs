#include "readyirc_uci.h"

#include <string.h>

#define UCI_ID_MASK    0x7F
#define UCI_ID_MATCH   0x49
#define UCI_STAT_DATA  0x80
#define UCI_STAT_STAT  0x40
#define UCI_STATE_MASK 0x30
#define UCI_STATE_IDLE 0x00
#define UCI_STATE_LAST 0x20
#define UCI_STATE_MORE 0x30
#define UCI_STAT_ABORT 0x04
#define UCI_STAT_ERROR 0x08

#define UCI_WAIT_SHORT  900u
#define UCI_WAIT_LONG   5000u

#define TARGET_NETWORK 0x03
#define NET_CMD_TCP_SOCKET_CONNECT 0x07
#define NET_CMD_SOCKET_CLOSE       0x09
#define NET_CMD_SOCKET_READ        0x10
#define NET_CMD_SOCKET_WRITE       0x11

#define READYIRC_WRITE_CHUNK 56u

static unsigned int uci_base_addr;
static unsigned char uci_data[READYIRC_UCI_DATA_MAX];
static unsigned char uci_stat[READYIRC_UCI_STAT_MAX];
static unsigned char uci_data_len;
static unsigned char uci_stat_len;
static char last_status[READYIRC_UCI_STAT_MAX + 1u];
static unsigned char last_status_code = 0xFFu;

static unsigned char id_matches(unsigned char id) {
    return (unsigned char)((id & UCI_ID_MASK) == UCI_ID_MATCH);
}

static unsigned char probe_base(unsigned int base) {
    readyirc_uci_asm_set_base(base);
    if (id_matches(readyirc_uci_asm_id())) {
        uci_base_addr = base;
        return 1u;
    }
    return 0u;
}

static void save_status(void) {
    unsigned char i;
    unsigned char n;

    n = uci_stat_len;
    if (n > READYIRC_UCI_STAT_MAX) {
        n = READYIRC_UCI_STAT_MAX;
    }
    for (i = 0u; i < n; ++i) {
        last_status[i] = (char)uci_stat[i];
    }
    last_status[n] = 0;
    last_status_code = 0xFFu;
    if (n >= 2u && last_status[0] >= '0' && last_status[0] <= '9' &&
        last_status[1] >= '0' && last_status[1] <= '9') {
        last_status_code = (unsigned char)(((unsigned char)(last_status[0] - '0') * 10u) +
                                           (unsigned char)(last_status[1] - '0'));
    }
}

static unsigned char status_ok(void) {
    return (unsigned char)(uci_stat_len >= 2u &&
                           uci_stat[0] == '0' &&
                           uci_stat[1] == '0');
}

static unsigned char wait_idle(void) {
    unsigned int tries;
    unsigned char st;

    for (tries = 0u; tries < UCI_WAIT_LONG; ++tries) {
        st = readyirc_uci_asm_status();
        if ((st & UCI_STAT_ERROR) != 0u) {
            readyirc_uci_asm_clear_error();
        }
        if ((st & UCI_STATE_MASK) == UCI_STATE_IDLE && (st & 0x01u) == 0u) {
            return 1u;
        }
    }
    return 0u;
}

static unsigned char wait_data_state(void) {
    unsigned int tries;
    unsigned char state;
    unsigned char st;

    for (tries = 0u; tries < UCI_WAIT_LONG; ++tries) {
        st = readyirc_uci_asm_status();
        if ((st & UCI_STAT_ERROR) != 0u) {
            return 0u;
        }
        state = (unsigned char)(st & UCI_STATE_MASK);
        if (state == UCI_STATE_LAST ||
            state == UCI_STATE_MORE ||
            state == UCI_STATE_IDLE) {
            return 1u;
        }
    }
    return 0u;
}

static unsigned char sync_interface(void) {
    unsigned int tries;
    unsigned char state;
    unsigned char st;

    for (tries = 0u; tries < UCI_WAIT_SHORT; ++tries) {
        st = readyirc_uci_asm_status();
        state = (unsigned char)(st & UCI_STATE_MASK);

        if ((st & UCI_STAT_ERROR) != 0u) {
            readyirc_uci_asm_clear_error();
            tries = 0u;
            continue;
        }
        if ((st & UCI_STAT_ABORT) != 0u) {
            readyirc_uci_asm_abort();
            tries = 0u;
            continue;
        }
        if ((st & UCI_STAT_DATA) != 0u) {
            (void)readyirc_uci_asm_read_data();
            tries = 0u;
            continue;
        }
        if ((st & UCI_STAT_STAT) != 0u) {
            (void)readyirc_uci_asm_read_stat();
            tries = 0u;
            continue;
        }
        if (state == UCI_STATE_LAST || state == UCI_STATE_MORE) {
            readyirc_uci_asm_accept_data();
            tries = 0u;
            continue;
        }
        if (state == UCI_STATE_IDLE && (st & 0x01u) == 0u) {
            return 1u;
        }
    }

    readyirc_uci_asm_abort();
    return wait_idle();
}

static unsigned char command(const unsigned char *cmd,
                             unsigned char cmd_len,
                             unsigned char data_cap) {
    unsigned char i;
    unsigned char st;
    unsigned char state;
    unsigned int drain_guard;

    uci_data_len = 0u;
    uci_stat_len = 0u;
    last_status[0] = 0;
    last_status_code = 0xFFu;

    if (!readyirc_uci_detect() || cmd == 0 || cmd_len == 0u) {
        return 0u;
    }
    if (!sync_interface()) {
        readyirc_uci_asm_abort();
        if (!wait_idle()) {
            return 0u;
        }
    }

    for (i = 0u; i < cmd_len; ++i) {
        readyirc_uci_asm_write_cmd(cmd[i]);
    }
    readyirc_uci_asm_push_cmd();

    if (!wait_data_state()) {
        readyirc_uci_asm_abort();
        return 0u;
    }

    for (;;) {
        st = readyirc_uci_asm_status();
        state = (unsigned char)(st & UCI_STATE_MASK);
        if (state == UCI_STATE_IDLE) {
            break;
        }
        if (state != UCI_STATE_LAST && state != UCI_STATE_MORE) {
            if (!wait_data_state()) {
                readyirc_uci_asm_abort();
                return 0u;
            }
            continue;
        }

        drain_guard = 0u;
        while (drain_guard < UCI_WAIT_SHORT) {
            st = readyirc_uci_asm_status();
            if ((st & UCI_STAT_DATA) != 0u) {
                if (uci_data_len < data_cap) {
                    uci_data[uci_data_len] = readyirc_uci_asm_read_data();
                    ++uci_data_len;
                } else {
                    (void)readyirc_uci_asm_read_data();
                }
                drain_guard = 0u;
                continue;
            }
            if ((st & UCI_STAT_STAT) != 0u) {
                if (uci_stat_len < READYIRC_UCI_STAT_MAX) {
                    uci_stat[uci_stat_len] = readyirc_uci_asm_read_stat();
                    ++uci_stat_len;
                } else {
                    (void)readyirc_uci_asm_read_stat();
                }
                drain_guard = 0u;
                continue;
            }
            ++drain_guard;
        }

        readyirc_uci_asm_accept_data();
        if (state == UCI_STATE_LAST) {
            (void)wait_idle();
            break;
        }
        if (!wait_data_state()) {
            readyirc_uci_asm_abort();
            return 0u;
        }
    }

    save_status();
    return 1u;
}

unsigned char readyirc_uci_detect(void) {
    static const unsigned int bases[] = {
        0xDF1Cu,
        0xDE1Cu,
        0xDFFCu
    };
    unsigned char i;

    if (uci_base_addr != 0u) {
        if (probe_base(uci_base_addr)) {
            return 1u;
        }
        uci_base_addr = 0u;
    }

    for (i = 0u; i < sizeof(bases) / sizeof(bases[0]); ++i) {
        if (probe_base(bases[i])) {
            return 1u;
        }
    }
    return 0u;
}

unsigned int readyirc_uci_base(void) {
    if (!readyirc_uci_detect()) {
        return 0u;
    }
    return uci_base_addr;
}

unsigned char readyirc_uci_tcp_connect(const char *host,
                                       unsigned int port,
                                       unsigned char *socket_out) {
    static unsigned char cmd[32];
    unsigned char ch;
    unsigned char i;

    if (socket_out != 0) {
        *socket_out = 0u;
    }
    if (host == 0) {
        return 0u;
    }

    cmd[0] = TARGET_NETWORK;
    cmd[1] = NET_CMD_TCP_SOCKET_CONNECT;
    cmd[2] = (unsigned char)(port & 0xFFu);
    cmd[3] = (unsigned char)(port >> 8);
    for (i = 0u; host[i] != 0 && i < 26u; ++i) {
        ch = (unsigned char)host[i];
        if (ch >= 0x41u && ch <= 0x5au) {
            ch = (unsigned char)(ch + 0x20u);
        } else if (ch >= 0xc1u && ch <= 0xdau) {
            ch = (unsigned char)(ch - 0x60u);
        }
        cmd[(unsigned char)(4u + i)] = ch;
    }
    cmd[(unsigned char)(4u + i)] = 0u;

    if (!command(cmd, (unsigned char)(5u + i), READYIRC_UCI_DATA_MAX)) {
        return 0u;
    }
    if (!status_ok() || uci_data_len == 0u) {
        return 0u;
    }
    if (socket_out != 0) {
        *socket_out = uci_data[0];
    }
    return 1u;
}

unsigned int readyirc_uci_socket_read(unsigned char socket_id,
                                      unsigned char *dst,
                                      unsigned int dst_len) {
    unsigned char cmd[5];
    unsigned char cap;
    unsigned int reported;
    unsigned int copied;

    if (dst == 0 || dst_len == 0u) {
        return 0u;
    }
    if (dst_len > (READYIRC_UCI_DATA_MAX - 2u)) {
        dst_len = READYIRC_UCI_DATA_MAX - 2u;
    }

    cmd[0] = TARGET_NETWORK;
    cmd[1] = NET_CMD_SOCKET_READ;
    cmd[2] = socket_id;
    cmd[3] = (unsigned char)(dst_len & 0xFFu);
    cmd[4] = (unsigned char)(dst_len >> 8);

    cap = (unsigned char)(dst_len + 2u);
    if (!command(cmd, sizeof(cmd), cap) || uci_data_len < 2u) {
        return 0u;
    }

    reported = (unsigned int)uci_data[0] | ((unsigned int)uci_data[1] << 8);
    if (reported == 0xFFFFu || reported == 0u) {
        return 0u;
    }

    copied = (unsigned int)(uci_data_len - 2u);
    if (copied > reported) {
        copied = reported;
    }
    if (copied > dst_len) {
        copied = dst_len;
    }
    memcpy(dst, uci_data + 2u, copied);
    return copied;
}

unsigned char readyirc_uci_socket_write(unsigned char socket_id,
                                        const char *data,
                                        unsigned int len) {
    static unsigned char cmd[3u + READYIRC_WRITE_CHUNK];
    unsigned int pos;
    unsigned char chunk;
    unsigned char i;

    if (data == 0) {
        return 0u;
    }
    pos = 0u;
    while (pos < len) {
        chunk = (unsigned char)(len - pos);
        if (chunk > READYIRC_WRITE_CHUNK) {
            chunk = READYIRC_WRITE_CHUNK;
        }

        cmd[0] = TARGET_NETWORK;
        cmd[1] = NET_CMD_SOCKET_WRITE;
        cmd[2] = socket_id;
        for (i = 0u; i < chunk; ++i) {
            cmd[(unsigned char)(3u + i)] = (unsigned char)data[pos + i];
        }
        if (!command(cmd, (unsigned char)(3u + chunk), READYIRC_UCI_DATA_MAX)) {
            return 0u;
        }
        if (!status_ok()) {
            return 0u;
        }
        pos = (unsigned int)(pos + chunk);
    }
    return 1u;
}

void readyirc_uci_socket_close(unsigned char socket_id) {
    unsigned char cmd[3];

    cmd[0] = TARGET_NETWORK;
    cmd[1] = NET_CMD_SOCKET_CLOSE;
    cmd[2] = socket_id;
    (void)command(cmd, sizeof(cmd), READYIRC_UCI_DATA_MAX);
}

const char *readyirc_uci_last_status(void) {
    return last_status;
}

unsigned char readyirc_uci_last_status_code(void) {
    return last_status_code;
}
