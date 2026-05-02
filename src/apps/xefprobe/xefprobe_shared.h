#ifndef XEFPROBE_SHARED_H
#define XEFPROBE_SHARED_H

#include <c64.h>
#include <string.h>
#include "../../lib/reu_mgr.h"

#define XEF_SCREEN     ((unsigned char*)0x0400)
#define XEF_COLOR_RAM  ((unsigned char*)0xD800)
#define XEF_DBG_BASE   ((unsigned char*)0xC7A0)
#define XEF_DBG_HEAD   (*(unsigned char*)0xC7DF)
#define XEF_RESULT_BASE ((unsigned char*)0xC7E8)

#define XEF_COLOR_BLACK       0u
#define XEF_COLOR_WHITE       1u
#define XEF_COLOR_RED         2u
#define XEF_COLOR_GREEN       5u
#define XEF_COLOR_BLUE        6u
#define XEF_COLOR_YELLOW      7u
#define XEF_COLOR_LIGHTRED   10u
#define XEF_COLOR_GRAY1      11u
#define XEF_COLOR_LIGHTGREEN 13u
#define XEF_COLOR_LIGHTBLUE  14u

#define XEFPROBE_PAYLOAD_REU_BANK 1u
#define XEFPROBE_DATA_REU_BANK    2u
#define XEFPROBE_SCRATCH_REU_BANK 3u
#define XEFPROBE_DATA_LEN         256u

#define XEFPROBE_HOST_FLAG        0x48u
#define XEFPROBE_HOST_DATA_FLAG   0x56u
#define XEFPROBE_PAYLOAD_FLAG     0x50u
#define XEFPROBE_STASH_FLAG       0x53u

#define SHIM_TARGET_BANK ((unsigned char*)0xC820)
#define SHIM_CURRENT_BANK ((unsigned char*)0xC834)

static unsigned char xefprobe_ascii_to_screen(unsigned char ch) {
    if (ch >= 'A' && ch <= 'Z') {
        return (unsigned char)(ch - 'A' + 1u);
    }
    if (ch >= '0' && ch <= '9') {
        return (unsigned char)(ch - '0' + 48u);
    }
    switch (ch) {
        case ' ': return 32u;
        case '-': return 45u;
        case '.': return 46u;
        case '/': return 47u;
        case ':': return 58u;
        case '>': return 62u;
        default: return 32u;
    }
}

static void xefprobe_dbg_put(unsigned char code) {
    unsigned char head = XEF_DBG_HEAD;

    XEF_DBG_BASE[head] = code;
    ++head;
    if (head >= 0x3Fu) {
        head = 0u;
    }
    XEF_DBG_HEAD = head;
}

static void xefprobe_clear(unsigned char border, unsigned char bg, unsigned char fg) {
    unsigned int i;

    VIC.bordercolor = border;
    VIC.bgcolor0 = bg;
    for (i = 0; i < 1000u; ++i) {
        XEF_SCREEN[i] = 32u;
        XEF_COLOR_RAM[i] = fg;
    }
}

static void xefprobe_puts(unsigned char x, unsigned char y,
                          const char *text, unsigned char color) {
    unsigned int offset = (unsigned int)y * 40u + x;
    unsigned char i = 0u;

    while (text[i] != 0) {
        XEF_SCREEN[offset + i] = xefprobe_ascii_to_screen((unsigned char)text[i]);
        XEF_COLOR_RAM[offset + i] = color;
        ++i;
    }
}

static unsigned char xefprobe_pattern_byte(unsigned char index) {
    return (unsigned char)(0x33u + (unsigned char)(index * 5u));
}

static void xefprobe_fill_expected(unsigned char *buffer) {
    unsigned char i = 0u;

    do {
        buffer[i] = xefprobe_pattern_byte(i);
        ++i;
    } while (i != 0u);
}

static void xefprobe_delay(void) {
    unsigned int i;

    for (i = 0; i < 60000u; ++i) {
        /* Busy wait so the host screen is briefly visible in interactive runs. */
    }
}

#endif
