/*
 * sysinfo.c - ReadyOS System Info
 *
 * Read-only split-pane hardware summary. The app does not allocate REU banks;
 * it only probes and restores bytes while reporting REU presence/size.
 */

#include "../../lib/tui.h"
#include "sysinfo_uci.h"

#include <c64.h>
#include <cbm.h>
#include <string.h>

#define HEADER_Y       0
#define PANE_Y         2
#define PANE_H         20
#define STATUS_Y       23
#define HELP_Y         24

#define LIST_X         0
#define LIST_W         10
#define DIVIDER_X      10
#define INFO_X         11
#define INFO_W         29

#define TAB_SYSTEM     0
#define TAB_ULTIMATE   1
#define TAB_DRIVES     2
#define TAB_COUNT      3

#define SHIM_CURRENT_BANK (*(volatile unsigned char*)0xC834)

#define KERNAL_REV_OFF  0x04ACu
#define KERNAL_BANNER_OFF 0x045Eu
#define KERNAL_BANNER_LEN 96u
#define CART_SIG_BASE   0x8004u
#define JIFFY_LO        (*(volatile unsigned char*)0x00A2)
#define JIFFY_MID       (*(volatile unsigned char*)0x00A1)
#define JIFFY_HI        (*(volatile unsigned char*)0x00A0)
#define VIC_CTRL1       (*(volatile unsigned char*)0xD011)
#define VIC_RASTER      (*(volatile unsigned char*)0xD012)

#define REU_COMMAND  (*(volatile unsigned char*)0xDF01)
#define REU_C64_LO   (*(volatile unsigned char*)0xDF02)
#define REU_C64_HI   (*(volatile unsigned char*)0xDF03)
#define REU_REU_LO   (*(volatile unsigned char*)0xDF04)
#define REU_REU_HI   (*(volatile unsigned char*)0xDF05)
#define REU_REU_BANK (*(volatile unsigned char*)0xDF06)
#define REU_LEN_LO   (*(volatile unsigned char*)0xDF07)
#define REU_LEN_HI   (*(volatile unsigned char*)0xDF08)

#define REU_CMD_STASH 0x90
#define REU_CMD_FETCH 0x91
#define REU_TEST_OFF  0xFFF0u
#define ULT_SPEED_U64   0u
#define ULT_SPEED_U64E2 1u
#define DRIVE_FIRST      8u
#define DRIVE_COUNT      4u
#define DRIVE_STATUS_MAX 24u
#define DRIVE_TYPE_MAX   8u
#define DRIVE_LFN_CMD    15u

static unsigned char running;
static unsigned char current_tab;
static unsigned char row_y;
static unsigned char ultimate_speed_table;
static unsigned char drives_cache_valid;

typedef struct DriveProbe {
    unsigned char device;
    unsigned char present;
    unsigned char status_code;
    char type[DRIVE_TYPE_MAX];
    char status[DRIVE_STATUS_MAX];
} DriveProbe;

static DriveProbe drive_cache[DRIVE_COUNT];

static unsigned char uci_data[SYSINFO_UCI_DATA_MAX];
static unsigned char uci_stat[SYSINFO_UCI_STAT_MAX];
static unsigned char uci_data_len;
static unsigned char uci_stat_len;
static char value_buf[32];

static const char *tab_names[TAB_COUNT] = {
    "system",
    "ultimate",
    "drives"
};

static void draw_shell(void);
static void draw_tabs(void);
static void draw_divider(void);
static void refresh_current_tab(void);
static void draw_system_tab(void);
static void draw_ultimate_tab(void);
static void draw_drives_tab(unsigned char force_refresh);
static void add_row(const char *label, const char *value, unsigned char color);
static void add_row_uint(const char *label, unsigned int value);
static void draw_centered_line(unsigned char y, const char *text, unsigned char color);
static void copy_text_fit(char *dst, unsigned char dst_len, const char *src);
static void copy_uci_text(char *dst, unsigned char dst_len);
static void format_uptime(char *dst);
static void format_reu(char *dst);
static void format_video(char *dst);
static void format_mac(char *dst, const unsigned char *src);
static void format_ip(char *dst, const unsigned char *src);
static void format_drive_line(char *dst, const unsigned char *src);
static void draw_drive_info_rows(const unsigned char *src, unsigned char len);
static unsigned char format_softiec_info(char *dst, const unsigned char *src, unsigned char len);
static void draw_ultimate_model(void);
static void draw_ultimate_cpu_speed(void);
static void draw_ultimate_http_target(void);
static void drives_refresh(void);
static void drives_probe_one(DriveProbe *probe, unsigned char device);
static void drives_parse_status(const char *line,
                                unsigned char *code_out,
                                char *msg_out,
                                unsigned char msg_cap);
static const char *drive_detect_type(const char *line);
static void draw_drive_probe_row(const DriveProbe *probe);
static unsigned char uci_status_ok(void);
static unsigned char text_contains_ci(const char *text, const char *pattern);
static const char *cart_name(void);
static void append_char(char *dst, unsigned char dst_len, char ch);
static void append_str(char *dst, unsigned char dst_len, const char *src);
static void append_uint(char *dst, unsigned char dst_len, unsigned int value);
static void append_hex2(char *dst, unsigned char dst_len, unsigned char value);
static void append_hex4(char *dst, unsigned char dst_len, unsigned int value);
static unsigned char reu_detect(void);
static unsigned int reu_detect_kb(void);
static void reu_stash_byte(unsigned char bank, unsigned int off, unsigned char value);
static unsigned char reu_fetch_byte(unsigned char bank, unsigned int off);
static const char *chargen_name(void);
static const char *kernal_name(unsigned char rev);
static const char *model_name(unsigned char rev);
static unsigned char kernal_banner_contains(const char *pattern);

static void append_char(char *dst, unsigned char dst_len, char ch) {
    unsigned char len;

    len = (unsigned char)strlen(dst);
    if (len + 1u >= dst_len) {
        return;
    }
    dst[len] = ch;
    dst[len + 1u] = 0;
}

static void append_str(char *dst, unsigned char dst_len, const char *src) {
    while (*src != 0) {
        append_char(dst, dst_len, *src);
        ++src;
    }
}

static void append_uint(char *dst, unsigned char dst_len, unsigned int value) {
    char rev[6];
    unsigned char count;

    count = 0u;
    do {
        rev[count] = (char)('0' + (value % 10u));
        value = (unsigned int)(value / 10u);
        ++count;
    } while (value != 0u && count < sizeof(rev));

    while (count > 0u) {
        --count;
        append_char(dst, dst_len, rev[count]);
    }
}

static void append_hex2(char *dst, unsigned char dst_len, unsigned char value) {
    static const char hex[] = "0123456789ABCDEF";

    append_char(dst, dst_len, hex[(value >> 4) & 0x0Fu]);
    append_char(dst, dst_len, hex[value & 0x0Fu]);
}

static void append_hex4(char *dst, unsigned char dst_len, unsigned int value) {
    append_hex2(dst, dst_len, (unsigned char)(value >> 8));
    append_hex2(dst, dst_len, (unsigned char)(value & 0xFFu));
}

static void append_2digit(char *dst, unsigned char dst_len, unsigned int value) {
    append_char(dst, dst_len, (char)('0' + ((value / 10u) % 10u)));
    append_char(dst, dst_len, (char)('0' + (value % 10u)));
}

static void add_row(const char *label, const char *value, unsigned char color) {
    if (row_y >= PANE_Y + PANE_H) {
        return;
    }
    tui_clear_line(row_y, INFO_X, INFO_W, TUI_COLOR_WHITE);
    tui_puts_n(INFO_X, row_y, label, 9u, TUI_COLOR_GRAY3);
    tui_puts_n(INFO_X + 9u, row_y, value, (unsigned char)(INFO_W - 9u), color);
    ++row_y;
}

static void add_row_uint(const char *label, unsigned int value) {
    value_buf[0] = 0;
    append_uint(value_buf, sizeof(value_buf), value);
    add_row(label, value_buf, TUI_COLOR_WHITE);
}

static void draw_centered_line(unsigned char y, const char *text, unsigned char color) {
    unsigned char text_len;
    unsigned char x;

    text_len = (unsigned char)strlen(text);
    tui_clear_line(y, 0u, 40u, color);
    if (text_len >= 40u) {
        tui_puts_n(0u, y, text, 40u, color);
        return;
    }
    x = (unsigned char)((40u - text_len) / 2u);
    tui_puts_n(x, y, text, text_len, color);
}

static void copy_text_fit(char *dst, unsigned char dst_len, const char *src) {
    if (dst_len == 0u || dst == 0 || src == 0) {
        return;
    }
    dst[0] = 0;
    while (*src != 0) {
        append_char(dst, dst_len, *src);
        ++src;
    }
}

static void draw_header(void) {
    TuiRect header;

    header.x = 0u;
    header.y = HEADER_Y;
    header.w = 40u;
    header.h = 2u;
    tui_window(&header, TUI_COLOR_LIGHTBLUE);
    tui_puts(1u, HEADER_Y, "SYSTEM INFO", TUI_COLOR_YELLOW);
}

static void draw_tabs(void) {
    unsigned char row;
    unsigned char y;
    unsigned char color;

    for (row = 0u; row < PANE_H; ++row) {
        y = (unsigned char)(PANE_Y + row);
        tui_clear_line(y, LIST_X, LIST_W, TUI_COLOR_BLUE);
        if (row >= TAB_COUNT) {
            continue;
        }
        color = (row == current_tab) ? TUI_COLOR_CYAN : TUI_COLOR_WHITE;
        tui_putc(0u, y, (row == current_tab) ? tui_ascii_to_screen('>') : 32u, color);
        tui_puts_n(1u, y, tab_names[row], 9u, color);
    }
}

static void draw_divider(void) {
    unsigned char y;

    for (y = PANE_Y; y < PANE_Y + PANE_H; ++y) {
        tui_putc(DIVIDER_X, y, TUI_VLINE, TUI_COLOR_GRAY2);
    }
}

static void draw_help(void) {
    if (current_tab == TAB_DRIVES) {
        draw_centered_line(HELP_Y, "R:REFRESH  CTRL+B:HOME", TUI_COLOR_GRAY3);
    } else {
        draw_centered_line(HELP_Y, "CTRL+B:HOME", TUI_COLOR_GRAY3);
    }
}

static void draw_status(void) {
    tui_clear_line(STATUS_Y, 0u, 40u, TUI_COLOR_WHITE);
}

static void draw_shell(void) {
    tui_clear(TUI_COLOR_BLUE);
    draw_header();
    draw_tabs();
    draw_divider();
    draw_status();
    draw_help();
}

static void clear_info_pane(void) {
    unsigned char y;

    for (y = PANE_Y; y < PANE_Y + PANE_H; ++y) {
        tui_clear_line(y, INFO_X, INFO_W, TUI_COLOR_WHITE);
    }
    row_y = PANE_Y;
}

static unsigned char rom_upper(unsigned char ch) {
    if (ch >= 'a' && ch <= 'z') {
        return (unsigned char)(ch - ('a' - 'A'));
    }
    return ch;
}

static unsigned char text_contains_ci(const char *text, const char *pattern) {
    unsigned char i;
    unsigned char j;

    for (i = 0u; text[i] != 0; ++i) {
        j = 0u;
        while (pattern[j] != 0 &&
               rom_upper((unsigned char)text[(unsigned char)(i + j)]) ==
                   (unsigned char)pattern[j]) {
            ++j;
        }
        if (pattern[j] == 0) {
            return 1u;
        }
        if (text[(unsigned char)(i + j)] == 0) {
            return 0u;
        }
    }
    return 0u;
}

static unsigned char match_pattern_at_kernal(unsigned int off, const char *pattern) {
    unsigned char i;
    unsigned char ch;

    i = 0u;
    while (pattern[i] != 0) {
        ch = rom_upper(sysinfo_rom_asm_read_kernal((unsigned int)(off + i)));
        if (ch != (unsigned char)pattern[i]) {
            return 0u;
        }
        ++i;
    }
    return 1u;
}

static unsigned char kernal_banner_contains(const char *pattern) {
    unsigned int off;

    for (off = KERNAL_BANNER_OFF;
         off < (unsigned int)(KERNAL_BANNER_OFF + KERNAL_BANNER_LEN);
         ++off) {
        if (match_pattern_at_kernal(off, pattern)) {
            return 1u;
        }
    }
    return 0u;
}

static unsigned char kernal_is_c64gs(void) {
    return (unsigned char)(sysinfo_rom_asm_read_kernal(0x1C00u) == 'C' &&
                           sysinfo_rom_asm_read_kernal(0x1C0Fu) == 'C' &&
                           sysinfo_rom_asm_read_kernal(0x1C1Cu) == 'C');
}

static const char *kernal_name(unsigned char rev) {
    if (kernal_banner_contains("JAFFYDOS")) {
        return "JaffyDOS";
    }
    if (kernal_banner_contains("JIFFYDOS")) {
        return "JiffyDOS";
    }
    if (kernal_banner_contains("DOLPHIN")) {
        return "Dolphin DOS";
    }
    if (kernal_banner_contains("SPEEDDOS") ||
        kernal_banner_contains("SPEED DOS")) {
        return "SpeedDOS";
    }
    if (kernal_banner_contains("PROFESSIONAL")) {
        return "Professional DOS";
    }
    if (kernal_banner_contains("EXOS")) {
        return "EXOS";
    }
    if (kernal_banner_contains("OPEN ROMS") ||
        kernal_banner_contains("OPENROMS")) {
        return "Open ROMs";
    }

    if (kernal_is_c64gs()) {
        return "390852-01 gs";
    }
    switch (rev) {
        case 0x2Bu: return "901227-01";
        case 0x5Cu: return "901227-02";
        case 0x81u: return "901227-03";
        case 0x63u: return "901246-01 sx";
        case 0x00u: return "906145-02 jp";
        case 0xB3u: return "251104-04 sx";
        default:    return "custom/unknown";
    }
}

static const char *model_name(unsigned char rev) {
    if (kernal_is_c64gs()) {
        return "C64GS";
    }
    switch (rev) {
        case 0x63u:
        case 0xB3u:
            return "SX-64";
        case 0x00u:
            return "C64 JP";
        case 0x2Bu:
        case 0x5Cu:
        case 0x81u:
            return "C64";
        default:
            return "custom C64";
    }
}

static const char *chargen_name(void) {
    if (sysinfo_rom_asm_read_chargen(0x0000u) == 0x3Cu &&
        sysinfo_rom_asm_read_chargen(0x0001u) == 0x66u &&
        sysinfo_rom_asm_read_chargen(0x0002u) == 0x6Eu &&
        sysinfo_rom_asm_read_chargen(0x0003u) == 0x6Eu &&
        sysinfo_rom_asm_read_chargen(0x0004u) == 0x60u &&
        sysinfo_rom_asm_read_chargen(0x0005u) == 0x62u &&
        sysinfo_rom_asm_read_chargen(0x0006u) == 0x3Cu &&
        sysinfo_rom_asm_read_chargen(0x0007u) == 0x00u) {
        return "901225-01";
    }
    if (sysinfo_rom_asm_read_chargen(0x0000u) == 0x00u &&
        sysinfo_rom_asm_read_chargen(0x0001u) == 0x1Cu &&
        sysinfo_rom_asm_read_chargen(0x0002u) == 0x22u &&
        sysinfo_rom_asm_read_chargen(0x0003u) == 0x4Au &&
        sysinfo_rom_asm_read_chargen(0x0004u) == 0x56u &&
        sysinfo_rom_asm_read_chargen(0x0005u) == 0x4Cu &&
        sysinfo_rom_asm_read_chargen(0x0006u) == 0x20u &&
        sysinfo_rom_asm_read_chargen(0x0007u) == 0x1Eu) {
        return "906143-02 jp";
    }
    return "custom/unknown";
}

static const char *cart_name(void) {
    if (((volatile unsigned char*)CART_SIG_BASE)[0] == 'C' &&
        ((volatile unsigned char*)CART_SIG_BASE)[1] == 'B' &&
        ((volatile unsigned char*)CART_SIG_BASE)[2] == 'M' &&
        ((volatile unsigned char*)CART_SIG_BASE)[3] == '8' &&
        ((volatile unsigned char*)CART_SIG_BASE)[4] == '0') {
        return "autostart rom";
    }
    return "not visible";
}

static unsigned int read_raster_line(void) {
    unsigned char hi;
    unsigned char lo;
    unsigned char hi2;

    hi = (unsigned char)(VIC_CTRL1 & 0x80u);
    lo = VIC_RASTER;
    hi2 = (unsigned char)(VIC_CTRL1 & 0x80u);
    if (hi != hi2) {
        lo = VIC_RASTER;
        hi = hi2;
    }
    return (unsigned int)lo + (hi ? 256u : 0u);
}

static unsigned int detect_raster_lines(void) {
    unsigned int guard;
    unsigned int line;
    unsigned int prev;
    unsigned int max_line;

    guard = 60000u;
    do {
        line = read_raster_line();
        --guard;
    } while (line < 250u && guard != 0u);

    guard = 60000u;
    do {
        line = read_raster_line();
        --guard;
    } while (line >= 20u && guard != 0u);

    prev = line;
    max_line = line;
    guard = 60000u;
    do {
        line = read_raster_line();
        if (line != prev) {
            if (line > max_line) {
                max_line = line;
            }
            if (prev > 200u && line < 20u) {
                break;
            }
            prev = line;
        }
        --guard;
    } while (guard != 0u);

    return (unsigned int)(max_line + 1u);
}

static void format_video(char *dst) {
    unsigned int lines;

    lines = detect_raster_lines();
    dst[0] = 0;
    if (lines >= 311u && lines <= 313u) {
        append_str(dst, 32u, "pal ");
    } else if (lines == 263u) {
        append_str(dst, 32u, "ntsc ");
    } else if (lines == 262u) {
        append_str(dst, 32u, "old ntsc ");
    } else {
        append_str(dst, 32u, "unknown ");
    }
    append_uint(dst, 32u, lines);
    append_str(dst, 32u, " lines");
}

static void format_uptime(char *dst) {
    unsigned long ticks;
    unsigned long total_seconds;
    unsigned int hours;
    unsigned int minutes;
    unsigned int seconds;

    ticks = ((unsigned long)JIFFY_HI << 16) |
            ((unsigned long)JIFFY_MID << 8) |
            (unsigned long)JIFFY_LO;
    total_seconds = ticks / 60UL;
    hours = (unsigned int)(total_seconds / 3600UL);
    minutes = (unsigned int)((total_seconds / 60UL) % 60UL);
    seconds = (unsigned int)(total_seconds % 60UL);

    dst[0] = 0;
    append_uint(dst, 32u, hours);
    append_char(dst, 32u, ':');
    append_2digit(dst, 32u, minutes);
    append_char(dst, 32u, ':');
    append_2digit(dst, 32u, seconds);
}

static void reu_stash_byte(unsigned char bank, unsigned int off, unsigned char value) {
    static unsigned char byte_value;

    byte_value = value;
    REU_C64_LO = (unsigned char)((unsigned int)&byte_value & 0xFFu);
    REU_C64_HI = (unsigned char)((unsigned int)&byte_value >> 8);
    REU_REU_LO = (unsigned char)(off & 0xFFu);
    REU_REU_HI = (unsigned char)(off >> 8);
    REU_REU_BANK = bank;
    REU_LEN_LO = 1u;
    REU_LEN_HI = 0u;
    REU_COMMAND = REU_CMD_STASH;
}

static unsigned char reu_fetch_byte(unsigned char bank, unsigned int off) {
    static unsigned char byte_value;

    byte_value = 0u;
    REU_C64_LO = (unsigned char)((unsigned int)&byte_value & 0xFFu);
    REU_C64_HI = (unsigned char)((unsigned int)&byte_value >> 8);
    REU_REU_LO = (unsigned char)(off & 0xFFu);
    REU_REU_HI = (unsigned char)(off >> 8);
    REU_REU_BANK = bank;
    REU_LEN_LO = 1u;
    REU_LEN_HI = 0u;
    REU_COMMAND = REU_CMD_FETCH;
    return byte_value;
}

static unsigned char reu_detect(void) {
    unsigned char orig;
    unsigned char got;

    orig = reu_fetch_byte(0u, REU_TEST_OFF);
    reu_stash_byte(0u, REU_TEST_OFF, 0xA5u);
    got = reu_fetch_byte(0u, REU_TEST_OFF);
    reu_stash_byte(0u, REU_TEST_OFF, orig);
    return (unsigned char)(got == 0xA5u);
}

static unsigned int reu_detect_kb(void) {
    static const unsigned char candidates[] = {
        1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u
    };
    unsigned char base_orig;
    unsigned char cand_orig;
    unsigned char got;
    unsigned char i;

    if (!reu_detect()) {
        return 0u;
    }

    base_orig = reu_fetch_byte(0u, REU_TEST_OFF);
    reu_stash_byte(0u, REU_TEST_OFF, 0x5Au);

    for (i = 0u; i < sizeof(candidates); ++i) {
        cand_orig = reu_fetch_byte(candidates[i], REU_TEST_OFF);
        reu_stash_byte(candidates[i], REU_TEST_OFF, 0xC3u);
        got = reu_fetch_byte(0u, REU_TEST_OFF);
        reu_stash_byte(candidates[i], REU_TEST_OFF, cand_orig);
        if (got == 0xC3u) {
            reu_stash_byte(0u, REU_TEST_OFF, base_orig);
            return (unsigned int)candidates[i] * 64u;
        }
    }

    reu_stash_byte(0u, REU_TEST_OFF, base_orig);
    return 16384u;
}

static void format_reu(char *dst) {
    unsigned int kb;

    kb = reu_detect_kb();
    dst[0] = 0;
    if (kb == 0u) {
        append_str(dst, 32u, "absent");
        return;
    }
    append_uint(dst, 32u, kb);
    append_str(dst, 32u, " kb");
    if (kb >= 1024u) {
        append_str(dst, 32u, " / ");
        append_uint(dst, 32u, (unsigned int)(kb / 1024u));
        append_str(dst, 32u, " mb");
    }
}

static void draw_system_tab(void) {
    unsigned char kernal_rev;

    kernal_rev = sysinfo_rom_asm_read_kernal(KERNAL_REV_OFF);
    clear_info_pane();
    add_row("model:", model_name(kernal_rev), TUI_COLOR_WHITE);
    add_row("kernal:", kernal_name(kernal_rev), TUI_COLOR_WHITE);

    value_buf[0] = '$';
    value_buf[1] = 0;
    append_hex2(value_buf, sizeof(value_buf), kernal_rev);
    add_row("k byte:", value_buf, TUI_COLOR_GRAY3);

    add_row("charset:", chargen_name(), TUI_COLOR_WHITE);
    format_video(value_buf);
    add_row("video:", value_buf, TUI_COLOR_CYAN);

    format_reu(value_buf);
    add_row("reu:", value_buf, TUI_COLOR_YELLOW);

    add_row("cart:", cart_name(), TUI_COLOR_WHITE);

    format_uptime(value_buf);
    add_row("up since:", value_buf, TUI_COLOR_LIGHTGREEN);
    add_row("", "(faster above 1mhz)", TUI_COLOR_YELLOW);
}

static unsigned char run_uci(const unsigned char *cmd, unsigned char len) {
    /* Sole app-level UCI command gateway. sysinfo_uci_command synchronizes,
     * waits for LAST/MORE (never immediate IDLE), drains both queues, issues
     * DATA_ACC, and returns only after quiet IDLE. */
    uci_data_len = 0u;
    uci_stat_len = 0u;
    return sysinfo_uci_command(cmd, len,
                               uci_data, sizeof(uci_data), &uci_data_len,
                               uci_stat, sizeof(uci_stat), &uci_stat_len);
}

static unsigned char uci_status_ok(void) {
    return (unsigned char)(uci_stat_len >= 2u &&
                           uci_stat[0] == '0' &&
                           uci_stat[1] == '0');
}

static void copy_uci_text(char *dst, unsigned char dst_len) {
    unsigned char i;
    unsigned char ch;

    dst[0] = 0;
    for (i = 0u; i < uci_data_len && i + 1u < dst_len; ++i) {
        ch = uci_data[i];
        if (ch < 32u || ch > 126u) {
            ch = ' ';
        }
        append_char(dst, dst_len, (char)ch);
    }
    if (dst[0] == 0) {
        append_str(dst, dst_len, "no data");
    }
}

static void drives_cleanup_io(void) {
    cbm_k_clrch();
    cbm_k_clall();
}

static void drives_parse_status(const char *line,
                                unsigned char *code_out,
                                char *msg_out,
                                unsigned char msg_cap) {
    unsigned int code;
    unsigned char i;
    const char *p;

    code = 0u;
    p = line;
    while (*p >= '0' && *p <= '9') {
        code = (unsigned int)(code * 10u + (unsigned int)(*p - '0'));
        ++p;
    }
    if (p == line) {
        code = 255u;
    }
    if (code_out != 0) {
        *code_out = (code > 255u) ? 255u : (unsigned char)code;
    }
    if (msg_out == 0 || msg_cap == 0u) {
        return;
    }

    if (*p == ',') {
        ++p;
    }
    while (*p == ' ') {
        ++p;
    }

    i = 0u;
    while (*p != 0 && *p != ',' && *p != '\r' && *p != '\n' &&
           i + 1u < msg_cap) {
        msg_out[i] = *p;
        ++i;
        ++p;
    }
    msg_out[i] = 0;
    if (msg_out[0] == 0) {
        copy_text_fit(msg_out, msg_cap, "NO STATUS");
    }
}

static const char *drive_detect_type(const char *line) {
    if (text_contains_ci(line, "SD2IEC")) {
        return "sd2iec";
    }
    if (text_contains_ci(line, "PI1541")) {
        return "pi1541";
    }
    if (text_contains_ci(line, "ULTIMATE") || text_contains_ci(line, "U64")) {
        return "u64";
    }
    if (text_contains_ci(line, "VICE")) {
        return "vice";
    }
    if (text_contains_ci(line, "CMD")) {
        return "cmd";
    }
    if (text_contains_ci(line, "1581")) {
        return "1581";
    }
    if (text_contains_ci(line, "1571")) {
        return "1571";
    }
    if (text_contains_ci(line, "1541")) {
        return "1541";
    }
    return "unknown";
}

static void drives_probe_one(DriveProbe *probe, unsigned char device) {
    char line[40];
    int n;

    probe->device = device;
    probe->present = 0u;
    probe->status_code = 255u;
    copy_text_fit(probe->type, sizeof(probe->type), "absent");
    copy_text_fit(probe->status, sizeof(probe->status), "CMD OPEN");

    drives_cleanup_io();
    if (cbm_open(DRIVE_LFN_CMD, device, 15u, "ui") != 0) {
        drives_cleanup_io();
        return;
    }

    n = cbm_read(DRIVE_LFN_CMD, line, sizeof(line) - 1u);
    if (n < 0) {
        n = 0;
    }
    line[n] = 0;

    cbm_close(DRIVE_LFN_CMD);
    drives_cleanup_io();

    probe->present = 1u;
    drives_parse_status(line, &probe->status_code,
                        probe->status, sizeof(probe->status));
    copy_text_fit(probe->type, sizeof(probe->type), drive_detect_type(line));
}

static void drives_refresh(void) {
    unsigned char i;

    for (i = 0u; i < DRIVE_COUNT; ++i) {
        drives_probe_one(&drive_cache[i], (unsigned char)(DRIVE_FIRST + i));
    }
    drives_cache_valid = 1u;
}

static void draw_drive_probe_row(const DriveProbe *probe) {
    char label[5];

    label[0] = 'd';
    if (probe->device < 10u) {
        label[1] = (char)('0' + probe->device);
        label[2] = ':';
        label[3] = 0;
    } else {
        label[1] = (char)('0' + (probe->device / 10u));
        label[2] = (char)('0' + (probe->device % 10u));
        label[3] = ':';
        label[4] = 0;
    }

    value_buf[0] = 0;
    append_str(value_buf, sizeof(value_buf), probe->type);
    if (probe->present) {
        append_char(value_buf, sizeof(value_buf), ' ');
        append_uint(value_buf, sizeof(value_buf), probe->status_code);
        append_char(value_buf, sizeof(value_buf), ' ');
        append_str(value_buf, sizeof(value_buf), probe->status);
    }
    add_row(label, value_buf, probe->present ? TUI_COLOR_WHITE : TUI_COLOR_GRAY3);
}

static void draw_drives_tab(unsigned char force_refresh) {
    unsigned char i;

    clear_info_pane();
    if (force_refresh || !drives_cache_valid) {
        add_row("drives:", "scanning 8-11", TUI_COLOR_YELLOW);
        drives_refresh();
        clear_info_pane();
    }

    for (i = 0u; i < DRIVE_COUNT; ++i) {
        draw_drive_probe_row(&drive_cache[i]);
    }
    add_row("", "press r to refresh", TUI_COLOR_GRAY3);
}

static void format_mac(char *dst, const unsigned char *src) {
    unsigned char i;

    dst[0] = 0;
    for (i = 0u; i < 6u; ++i) {
        if (i != 0u) {
            append_char(dst, 32u, ':');
        }
        append_hex2(dst, 32u, src[i]);
    }
}

static void format_ip(char *dst, const unsigned char *src) {
    unsigned char i;

    dst[0] = 0;
    for (i = 0u; i < 4u; ++i) {
        if (i != 0u) {
            append_char(dst, 32u, '.');
        }
        append_uint(dst, 32u, src[i]);
    }
}

static const char *drive_type_name(unsigned char type) {
    switch (type) {
        case 0x00u: return "1541";
        case 0x01u: return "1571";
        case 0x02u: return "1581";
        case 0x03u: return "undec";
        case 0x0Fu: return "softiec";
        case 0x50u: return "printer";
        default:    return "other";
    }
}

static void format_drive_line(char *dst, const unsigned char *src) {
    dst[0] = 0;
    append_char(dst, 32u, 'd');
    append_uint(dst, 32u, src[1]);
    append_char(dst, 32u, ' ');
    append_str(dst, 32u, drive_type_name(src[0]));
    append_char(dst, 32u, ' ');
    append_str(dst, 32u, src[2] ? "on" : "off");
}

static void draw_drive_info_rows(const unsigned char *src, unsigned char len) {
    unsigned char count;
    unsigned char i;
    unsigned char off;

    if (len == 0u) {
        add_row("drives:", "no data", TUI_COLOR_GRAY3);
        return;
    }
    count = src[0];
    value_buf[0] = 0;
    append_uint(value_buf, sizeof(value_buf), count);
    add_row("drives:", value_buf, TUI_COLOR_WHITE);

    off = 1u;
    for (i = 0u; i < count && off + 2u < len; ++i) {
        format_drive_line(value_buf, src + off);
        add_row("", value_buf, TUI_COLOR_WHITE);
        off = (unsigned char)(off + 3u);
    }
}

static unsigned char format_softiec_info(char *dst, const unsigned char *src, unsigned char len) {
    unsigned char count;
    unsigned char i;
    unsigned char off;
    unsigned char max_off;
    unsigned char step;
    unsigned char softiec_bus;

    if (len != 0u) {
        count = src[0];

        for (step = 3u; step <= 6u; step = (unsigned char)(step + 3u)) {
            off = 1u;
            for (i = 0u; i < count && off + 2u < len; ++i) {
                if (src[off] == 0x0Fu) {
                    dst[0] = 0;
                    append_char(dst, 32u, 'd');
                    append_uint(dst, 32u, src[(unsigned char)(off + 1u)]);
                    append_char(dst, 32u, ' ');
                    append_str(dst, 32u, src[(unsigned char)(off + 2u)] ? "on" : "off");
                    return 1u;
                }
                off = (unsigned char)(off + step);
            }
        }

        if (len >= 3u) {
            max_off = (unsigned char)(len - 2u);
            for (off = 1u; off < max_off; ++off) {
                if (src[off] == 0x0Fu && src[(unsigned char)(off + 1u)] >= 4u &&
                    src[(unsigned char)(off + 1u)] <= 30u && src[(unsigned char)(off + 2u)] <= 1u) {
                    dst[0] = 0;
                    append_char(dst, 32u, 'd');
                    append_uint(dst, 32u, src[(unsigned char)(off + 1u)]);
                    append_char(dst, 32u, ' ');
                    append_str(dst, 32u, src[(unsigned char)(off + 2u)] ? "on" : "off");
                    return 1u;
                }
            }
        }
    }

    /* This is the documented adjacent SoftIEC bus register, not a UCI command
     * transaction; it is read only after the command gateway has quiesced. */
    softiec_bus = sysinfo_uci_asm_read_softiec_bus();
    if (softiec_bus >= 4u && softiec_bus <= 30u) {
        dst[0] = 0;
        append_char(dst, 32u, 'd');
        append_uint(dst, 32u, softiec_bus);
        append_str(dst, 32u, " present");
        return 1u;
    }
    return 0u;
}

static unsigned char ultimate_cpu_mhz(unsigned char index) {
    static const unsigned char speeds_u64[] = {
        1u, 2u, 3u, 4u, 5u, 6u, 8u, 10u,
        12u, 14u, 16u, 20u, 24u, 32u, 40u, 48u
    };
    static const unsigned char speeds_u64e2[] = {
        1u, 2u, 3u, 4u, 6u, 8u, 10u, 12u,
        14u, 16u, 20u, 24u, 32u, 40u, 48u, 64u
    };

    if (ultimate_speed_table == ULT_SPEED_U64E2) {
        return speeds_u64e2[index & 0x0Fu];
    }
    return speeds_u64[index & 0x0Fu];
}

static void draw_ultimate_cpu_speed(void) {
    unsigned char reg;
    unsigned char enable_reg;

    reg = sysinfo_uci_asm_read_u64_turbo();
    if (reg == 0xFFu) {
        enable_reg = sysinfo_uci_asm_read_u64_turbo_enable();
        if (enable_reg == 0xFFu) {
            add_row("cpu:", "not exposed", TUI_COLOR_GRAY3);
        } else if ((enable_reg & 0x01u) != 0u) {
            add_row("cpu:", "menu turbo", TUI_COLOR_LIGHTGREEN);
        } else {
            add_row("cpu:", "1 mhz", TUI_COLOR_LIGHTGREEN);
        }
        return;
    }

    value_buf[0] = 0;
    append_uint(value_buf, sizeof(value_buf), ultimate_cpu_mhz((unsigned char)(reg & 0x0Fu)));
    append_str(value_buf, sizeof(value_buf), " mhz");
    if ((reg & 0x80u) != 0u) {
        append_str(value_buf, sizeof(value_buf), " no badlines");
    }
    add_row("cpu:", value_buf, TUI_COLOR_LIGHTGREEN);
}

static void draw_ultimate_model(void) {
    static const unsigned char cmd_hwinfo_doc[] = {0x04u, 0x28u};
    static const unsigned char cmd_hwinfo_legacy[] = {0x04u, 0x28u, 0x00u};

    if ((!run_uci(cmd_hwinfo_doc, sizeof(cmd_hwinfo_doc)) || uci_data_len == 0u || !uci_status_ok()) &&
        (!run_uci(cmd_hwinfo_legacy, sizeof(cmd_hwinfo_legacy)) || uci_data_len == 0u || !uci_status_ok())) {
        add_row("model:", "not exposed", TUI_COLOR_GRAY3);
        return;
    }

    copy_uci_text(value_buf, sizeof(value_buf));
    if (text_contains_ci(value_buf, "COMMODORE 64 ULTIMATE") ||
        text_contains_ci(value_buf, "C64 ULTIMATE")) {
        ultimate_speed_table = ULT_SPEED_U64E2;
        add_row("model:", "Commodore 64", TUI_COLOR_CYAN);
        add_row("", "Ultimate", TUI_COLOR_CYAN);
    } else if (text_contains_ci(value_buf, "ULTIMATE 64")) {
        if (text_contains_ci(value_buf, "ELITE")) {
            ultimate_speed_table = ULT_SPEED_U64E2;
        } else {
            ultimate_speed_table = ULT_SPEED_U64;
        }
        add_row("model:", "Ultimate 64", TUI_COLOR_CYAN);
    } else {
        ultimate_speed_table = ULT_SPEED_U64;
        add_row("model:", value_buf, TUI_COLOR_CYAN);
    }
}

static void draw_ultimate_http_target(void) {
    static const unsigned char cmd_http_ident[] = {0x06u, 0x01u};

    if (!run_uci(cmd_http_ident, sizeof(cmd_http_ident))) {
        add_row("http tgt:", "not exposed", TUI_COLOR_GRAY3);
        return;
    }
    if (!uci_status_ok()) {
        add_row("http tgt:", "not present", TUI_COLOR_GRAY3);
        return;
    }
    if (uci_data_len == 0u) {
        add_row("http tgt:", "present", TUI_COLOR_LIGHTGREEN);
        return;
    }
    copy_uci_text(value_buf, sizeof(value_buf));
    add_row("http tgt:", value_buf, TUI_COLOR_LIGHTGREEN);
}

static void draw_ultimate_tab(void) {
    static const unsigned char cmd_ctrl_ident[] = {0x04u, 0x01u};
    static const unsigned char cmd_drvinfo[] = {0x04u, 0x29u, 0x01u};
    static const unsigned char cmd_dos_time[] = {0x01u, 0x26u, 0x00u};
    static const unsigned char cmd_net_ident[] = {0x03u, 0x01u};
    static const unsigned char cmd_net_count[] = {0x03u, 0x02u};
    static const unsigned char cmd_net_mac0[] = {0x03u, 0x04u, 0x00u};
    static const unsigned char cmd_net_ip0[] = {0x03u, 0x05u, 0x00u};
    unsigned int uci_base;
    unsigned char softiec_present;

    clear_info_pane();
    /* Detection only probes the documented bases. Every command below goes
     * through run_uci so refreshes cannot overlap asynchronous UCI state. */
    uci_base = sysinfo_uci_base();
    if (uci_base == 0u) {
        add_row("uci:", "not detected", TUI_COLOR_LIGHTRED);
        add_row("hint:", "enable the uci", TUI_COLOR_YELLOW);
        add_row("", "to detect ultimate", TUI_COLOR_YELLOW);
        return;
    }

    value_buf[0] = '$';
    value_buf[1] = 0;
    append_hex4(value_buf, sizeof(value_buf), uci_base);
    add_row("uci:", value_buf, TUI_COLOR_LIGHTGREEN);
    ultimate_speed_table = ULT_SPEED_U64;
    if (run_uci(cmd_ctrl_ident, sizeof(cmd_ctrl_ident))) {
        copy_uci_text(value_buf, sizeof(value_buf));
        add_row("ctrl:", value_buf, TUI_COLOR_WHITE);
    }
    draw_ultimate_model();
    draw_ultimate_cpu_speed();

    softiec_present = 0u;
    if (run_uci(cmd_drvinfo, sizeof(cmd_drvinfo)) && uci_status_ok()) {
        draw_drive_info_rows(uci_data, uci_data_len);
        softiec_present = format_softiec_info(value_buf, uci_data, uci_data_len);
    }
    if (softiec_present) {
        add_row("softiec:", value_buf, TUI_COLOR_LIGHTGREEN);
    } else {
        add_row("softiec:", "not exposed", TUI_COLOR_GRAY3);
    }
    draw_ultimate_http_target();
    if (run_uci(cmd_dos_time, sizeof(cmd_dos_time)) && uci_data_len > 0u && uci_status_ok()) {
        copy_uci_text(value_buf, sizeof(value_buf));
        add_row("time:", value_buf, TUI_COLOR_LIGHTGREEN);
    }
    if (run_uci(cmd_net_ident, sizeof(cmd_net_ident))) {
        copy_uci_text(value_buf, sizeof(value_buf));
        add_row("net:", value_buf, TUI_COLOR_WHITE);
    }
    if (run_uci(cmd_net_count, sizeof(cmd_net_count)) && uci_data_len > 0u) {
        add_row_uint("ifaces:", uci_data[0]);
        if (uci_data[0] > 0u) {
            if (run_uci(cmd_net_mac0, sizeof(cmd_net_mac0)) && uci_data_len >= 6u) {
                format_mac(value_buf, uci_data);
                add_row("mac0:", value_buf, TUI_COLOR_WHITE);
            }
            if (run_uci(cmd_net_ip0, sizeof(cmd_net_ip0)) && uci_data_len >= 12u) {
                format_ip(value_buf, uci_data);
                add_row("ip0:", value_buf, TUI_COLOR_LIGHTGREEN);
                format_ip(value_buf, uci_data + 4u);
                add_row("mask:", value_buf, TUI_COLOR_WHITE);
                format_ip(value_buf, uci_data + 8u);
                add_row("gw:", value_buf, TUI_COLOR_WHITE);
            }
        }
    }
}

static void refresh_current_tab(void) {
    draw_tabs();
    draw_status();
    draw_help();
    if (current_tab == TAB_SYSTEM) {
        draw_system_tab();
    } else if (current_tab == TAB_ULTIMATE) {
        draw_ultimate_tab();
    } else {
        draw_drives_tab(0u);
    }
}

static void sysinfo_loop(void) {
    unsigned char key;
    unsigned char nav_action;

    draw_shell();
    refresh_current_tab();

    while (running) {
        key = tui_getkey();
        nav_action = tui_handle_global_hotkey(key, SHIM_CURRENT_BANK, 1u);
        if (nav_action == TUI_HOTKEY_LAUNCHER) {
            tui_return_to_launcher();
        }
        if (nav_action >= TUI_APP_BANK_MIN && nav_action <= TUI_APP_BANK_MAX) {
            tui_switch_to_app(nav_action);
            continue;
        }
        if (nav_action == TUI_HOTKEY_BIND_ONLY) {
            continue;
        }

        switch (key) {
            case TUI_KEY_UP:
                if (current_tab > 0u) {
                    --current_tab;
                    refresh_current_tab();
                }
                break;
            case TUI_KEY_DOWN:
                if (current_tab + 1u < TAB_COUNT) {
                    ++current_tab;
                    refresh_current_tab();
                }
                break;
            case TUI_KEY_LEFT:
            case TUI_KEY_RIGHT:
            case TUI_KEY_RETURN:
                refresh_current_tab();
                break;
            case 'r':
            case 'R':
                if (current_tab == TAB_DRIVES) {
                    draw_tabs();
                    draw_status();
                    draw_help();
                    draw_drives_tab(1u);
                }
                break;
            case TUI_KEY_RUNSTOP:
                running = 0u;
                break;
        }
    }
    __asm__("jmp $FCE2");
}

int main(void) {
    tui_init();
    current_tab = TAB_SYSTEM;
    running = 1u;
    sysinfo_loop();
    return 0;
}
