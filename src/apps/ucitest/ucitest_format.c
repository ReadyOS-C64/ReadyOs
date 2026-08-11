#include "ucitest_format.h"

#include <string.h>

static char line[41];

static void append_char(char *dst, unsigned char cap, char ch) {
    unsigned char len;

    len = (unsigned char)strlen(dst);
    if (len + 1u >= cap) {
        return;
    }
    dst[len] = ch;
    dst[(unsigned char)(len + 1u)] = 0;
}

static void append_str(char *dst, unsigned char cap, const char *src) {
    while (*src != 0) {
        append_char(dst, cap, *src);
        ++src;
    }
}

static void append_hex2(char *dst, unsigned char cap, unsigned char value) {
    static const char hex[] = "0123456789ABCDEF";

    append_char(dst, cap, hex[(value >> 4) & 0x0Fu]);
    append_char(dst, cap, hex[value & 0x0Fu]);
}

static void append_hex4(char *dst, unsigned char cap, unsigned int value) {
    append_hex2(dst, cap, (unsigned char)(value >> 8));
    append_hex2(dst, cap, (unsigned char)(value & 0xFFu));
}

void ucitest_format_hex8(char *dst, unsigned char value) {
    dst[0] = '$';
    dst[1] = 0;
    append_hex2(dst, 5u, value);
}

void ucitest_format_hex16(char *dst, unsigned int value) {
    dst[0] = '$';
    dst[1] = 0;
    append_hex4(dst, 7u, value);
}

static void add_status(TuiOutput *out, const UciTestTransfer *xfer) {
    unsigned int i;
    unsigned char ch;

    line[0] = 0;
    if (xfer->stat_len == 0u) {
        append_str(line, sizeof(line), "stat: ");
        append_str(line, sizeof(line), "(none)");
    } else {
        i = 0u;
        if (xfer->stat[0] < 32u || xfer->stat[0] > 126u) {
            append_str(line, sizeof(line), "stat $");
            append_hex2(line, sizeof(line), xfer->stat[0]);
            append_str(line, sizeof(line), ": ");
            i = 1u;
        } else {
            append_str(line, sizeof(line), "stat: ");
        }
        for (; i < xfer->stat_len && i < 32u; ++i) {
            ch = xfer->stat[i];
            if (ch < 32u || ch > 126u) {
                ch = '.';
            }
            append_char(line, sizeof(line), (char)ch);
        }
    }
    tui_output_add(out, line);
}

static void add_text(TuiOutput *out, const unsigned char *data,
                     unsigned int len) {
    unsigned int pos;
    unsigned char i;
    unsigned char ch;

    pos = 0u;
    while (pos < len) {
        line[0] = 0;
        for (i = 0u; i < 39u && pos < len; ++i) {
            ch = data[pos];
            if (ch < 32u || ch > 126u) {
                ch = '.';
            }
            append_char(line, sizeof(line), (char)ch);
            ++pos;
        }
        tui_output_add(out, line);
    }
}

static void add_mac(TuiOutput *out, const unsigned char *data,
                    unsigned int len) {
    unsigned char i;

    if (len < 6u) {
        tui_output_add(out, "mac: short response");
        return;
    }
    line[0] = 0;
    append_str(line, sizeof(line), "mac: ");
    for (i = 0u; i < 6u; ++i) {
        if (i != 0u) {
            append_char(line, sizeof(line), ':');
        }
        append_hex2(line, sizeof(line), data[i]);
    }
    tui_output_add(out, line);
}

static void append_ip(char *dst, const unsigned char *data) {
    unsigned char i;

    for (i = 0u; i < 4u; ++i) {
        if (i != 0u) {
            append_char(dst, sizeof(line), '.');
        }
        append_hex2(dst, sizeof(line), data[i]);
    }
}

static void add_ip(TuiOutput *out, const unsigned char *data,
                   unsigned int len) {
    if (len < 12u) {
        tui_output_add(out, "ip: short response");
        return;
    }
    line[0] = 0;
    append_str(line, sizeof(line), "ip: ");
    append_ip(line, data);
    tui_output_add(out, line);
    line[0] = 0;
    append_str(line, sizeof(line), "mask: ");
    append_ip(line, data + 4u);
    tui_output_add(out, line);
    line[0] = 0;
    append_str(line, sizeof(line), "gw: ");
    append_ip(line, data + 8u);
    tui_output_add(out, line);
}

static void add_word(TuiOutput *out, const unsigned char *data,
                     unsigned int len) {
    unsigned int value;

    if (len < 2u) {
        tui_output_add(out, "word: short response");
        return;
    }
    value = (unsigned int)data[0] | ((unsigned int)data[1] << 8);
    line[0] = 0;
    append_str(line, sizeof(line), "word: $");
    append_hex4(line, sizeof(line), value);
    tui_output_add(out, line);
}

static void add_dword(TuiOutput *out, const unsigned char *data,
                      unsigned int len) {
    unsigned int lo;
    unsigned int hi;

    if (len < 4u) {
        tui_output_add(out, "dword: short response");
        return;
    }
    lo = (unsigned int)data[0] | ((unsigned int)data[1] << 8);
    hi = (unsigned int)data[2] | ((unsigned int)data[3] << 8);
    line[0] = 0;
    append_str(line, sizeof(line), "dword: $");
    append_hex4(line, sizeof(line), hi);
    append_hex4(line, sizeof(line), lo);
    tui_output_add(out, line);
}

static void add_socket_read(TuiOutput *out, const unsigned char *data,
                            unsigned int len) {
    unsigned int payload_len;
    unsigned int captured;

    if (len < 2u) {
        tui_output_add(out, "socket: short response");
        return;
    }
    payload_len = (unsigned int)data[0] | ((unsigned int)data[1] << 8);
    captured = (unsigned int)(len - 2u);
    if (payload_len < captured) {
        captured = payload_len;
    }
    line[0] = 0;
    append_str(line, sizeof(line), "socket bytes: $");
    append_hex4(line, sizeof(line), payload_len);
    tui_output_add(out, line);
    if (captured != 0u) {
        add_text(out, data + 2u, captured);
    }
}

static void add_http_handles(TuiOutput *out, const unsigned char *data,
                             unsigned int len) {
    if (len < 2u) {
        tui_output_add(out, "http handles: short response");
        return;
    }
    line[0] = 0;
    append_str(line, sizeof(line), "response header: $");
    append_hex2(line, sizeof(line), data[0]);
    tui_output_add(out, line);
    line[0] = 0;
    append_str(line, sizeof(line), "response body: $");
    append_hex2(line, sizeof(line), data[1]);
    tui_output_add(out, line);
}

static void add_iec_name(TuiOutput *out, const unsigned char *data,
                         unsigned int len) {
    if (len == 0u) {
        tui_output_add(out, "iec name: no response");
        return;
    }
    line[0] = 0;
    append_str(line, sizeof(line), "file type: $");
    append_hex2(line, sizeof(line), data[0]);
    tui_output_add(out, line);
    if (len > 1u) {
        add_text(out, data + 1u, (unsigned int)(len - 1u));
    }
}

static void add_handle(TuiOutput *out, const unsigned char *data,
                       unsigned int len) {
    if (len == 0u) {
        tui_output_add(out, "handle: none");
        return;
    }
    line[0] = 0;
    append_str(line, sizeof(line), "handle: $");
    append_hex2(line, sizeof(line), data[0]);
    tui_output_add(out, line);
}

static void add_dir(TuiOutput *out, const unsigned char *data,
                    unsigned int len) {
    if (len == 0u) {
        tui_output_add(out, "dir: no entry");
        return;
    }
    line[0] = 0;
    append_str(line, sizeof(line), "attr $");
    append_hex2(line, sizeof(line), data[0]);
    append_str(line, sizeof(line), " ");
    if ((data[0] & 0x10u) != 0u) {
        append_str(line, sizeof(line), "dir ");
    }
    tui_output_add(out, line);
    add_text(out, data + 1u, (unsigned int)(len - 1u));
}

static void add_file_info(TuiOutput *out, const unsigned char *data,
                          unsigned int len) {
    unsigned int size_lo;
    unsigned int size_hi;

    if (len < 12u) {
        tui_output_add(out, "file info: short response");
        if (len != 0u) {
            add_text(out, data, len);
        }
        return;
    }
    size_lo = (unsigned int)data[0] | ((unsigned int)data[1] << 8);
    size_hi = (unsigned int)data[2] | ((unsigned int)data[3] << 8);
    line[0] = 0;
    append_str(line, sizeof(line), "size: $");
    append_hex4(line, sizeof(line), size_hi);
    append_hex4(line, sizeof(line), size_lo);
    tui_output_add(out, line);
    line[0] = 0;
    append_str(line, sizeof(line), "date/time: $");
    append_hex2(line, sizeof(line), data[5]);
    append_hex2(line, sizeof(line), data[4]);
    append_char(line, sizeof(line), ' ');
    append_hex2(line, sizeof(line), data[7]);
    append_hex2(line, sizeof(line), data[6]);
    tui_output_add(out, line);
    line[0] = 0;
    append_str(line, sizeof(line), "ext/attr: ");
    append_char(line, sizeof(line), (char)data[8]);
    append_char(line, sizeof(line), (char)data[9]);
    append_char(line, sizeof(line), (char)data[10]);
    append_str(line, sizeof(line), " $");
    append_hex2(line, sizeof(line), data[11]);
    tui_output_add(out, line);
    if (len > 12u) {
        add_text(out, data + 12u, (unsigned int)(len - 12u));
    }
}

static void add_http_value(TuiOutput *out, const unsigned char *data,
                           unsigned int len) {
    unsigned char type;
    unsigned int value;

    if (len == 0u) {
        tui_output_add(out, "http value: none");
        return;
    }
    type = data[0];
    if (type == 1u && len >= 5u) {
        value = (unsigned int)data[1] | ((unsigned int)data[2] << 8);
        line[0] = 0;
        append_str(line, sizeof(line), "int lo: $");
        append_hex4(line, sizeof(line), value);
        tui_output_add(out, line);
        return;
    }
    if (type == 2u && len >= 2u) {
        tui_output_add(out, data[1] ? "bool: true" : "bool: false");
        return;
    }
    if (type == 3u && len >= 2u) {
        tui_output_add(out, "string:");
        add_text(out, data + 2u, data[1]);
        return;
    }
    if (type == 4u) {
        tui_output_add(out, "object:");
        tui_output_add_hex(out, data, len);
        return;
    }
    if (type == 5u) {
        tui_output_add(out, "array:");
        tui_output_add_hex(out, data, len);
        return;
    }
    tui_output_add(out, "http value raw:");
    tui_output_add_hex(out, data, len);
}

void ucitest_format_response(TuiOutput *out,
                             const UciTestCommandSpec *cmd,
                             const UciTestTransfer *xfer,
                             unsigned char raw_mode) {
    line[0] = 0;
    append_str(line, sizeof(line), "cmd: ");
    append_str(line, sizeof(line), cmd->name);
    tui_output_add(out, line);
    add_status(out, xfer);
    line[0] = 0;
    append_str(line, sizeof(line), "data bytes: ");
    append_char(line, sizeof(line), '$');
    append_hex4(line, sizeof(line), xfer->data_len);
    if ((xfer->flags & UCITEST_UCI_TRUNC_DATA) != 0u) {
        append_str(line, sizeof(line), " trunc");
    }
    tui_output_add(out, line);
    if ((xfer->flags & UCITEST_UCI_TRUNC_STAT) != 0u) {
        tui_output_add(out, "status capture truncated");
    }

    if (raw_mode || xfer->data_len == 0u) {
        if (xfer->data_len != 0u) {
            tui_output_add_hex(out, xfer->data, xfer->data_len);
        }
        return;
    }

    switch (cmd->decoder) {
        case UC_DEC_TEXT:
            add_text(out, xfer->data, xfer->data_len);
            break;
        case UC_DEC_HANDLE:
            add_handle(out, xfer->data, xfer->data_len);
            break;
        case UC_DEC_WORD:
            add_word(out, xfer->data, xfer->data_len);
            break;
        case UC_DEC_MAC:
            add_mac(out, xfer->data, xfer->data_len);
            break;
        case UC_DEC_IP:
            add_ip(out, xfer->data, xfer->data_len);
            break;
        case UC_DEC_DIR:
            add_dir(out, xfer->data, xfer->data_len);
            break;
        case UC_DEC_FILE_INFO:
            add_file_info(out, xfer->data, xfer->data_len);
            break;
        case UC_DEC_HTTP_VALUE:
            add_http_value(out, xfer->data, xfer->data_len);
            break;
        case UC_DEC_DWORD:
            add_dword(out, xfer->data, xfer->data_len);
            break;
        case UC_DEC_SOCKET_READ:
            add_socket_read(out, xfer->data, xfer->data_len);
            break;
        case UC_DEC_HTTP_HANDLES:
            add_http_handles(out, xfer->data, xfer->data_len);
            break;
        case UC_DEC_IEC_NAME:
            add_iec_name(out, xfer->data, xfer->data_len);
            break;
        default:
            tui_output_add_hex(out, xfer->data, xfer->data_len);
            break;
    }
}
