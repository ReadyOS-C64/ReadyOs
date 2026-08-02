/*
 * readyirc.c - ReadyOS one-channel IRC client for Ultimate 64/UCI TCP
 */

#include "../../lib/resume_state.h"
#include "../../lib/reu_mgr.h"
#include "../../lib/reu_owned_alloc.h"
#include "../../lib/tui.h"
#include "readyirc_uci.h"

#include <c64.h>
#include <cbm.h>
#include <conio.h>
#include <string.h>

#define SHIM_CURRENT_BANK (*(volatile unsigned char*)0xC834)

#define OUTPUT_TOP 2u
#define OUTPUT_H   20u
#define STATUS_Y   22u
#define INPUT_Y    23u
#define HELP_Y     24u

#define IRC_LINE_W     40u
#define LINE_REC_LEN   80u
#define REU_MAX_LINES  512u
#define RAM_MAX_LINES  OUTPUT_H
#define INPUT_MAX      96u
#define IRC_IN_MAX     510u
#define NET_READ_CHUNK 64u
#define NET_READ_BURST 2u

#define READYIRC_RESUME_APP_SCHEMA 3u

#define UI_MODE_SETUP 0u
#define UI_MODE_CHAT  1u

#define SERVER_MAX   26u
#define NICK_MAX     30u
#define CHANNEL_MAX  30u
#define PORT_TEXT_MAX 5u
#define SETUP_FIELD_COUNT 4u

#define UCI_STATUS_OK 0u
/* Ultimate firmware reports its normal 40 ms receive timeout as 02,NO DATA. */
#define UCI_STATUS_NO_DATA 2u

typedef struct ReadyIrcResumeV2 {
    char server[SERVER_MAX + 1u];
    char port_text[PORT_TEXT_MAX + 1u];
    char nick[NICK_MAX + 1u];
    char channel[CHANNEL_MAX + 1u];
    char input[INPUT_MAX + 1u];
    char partial_line[IRC_IN_MAX + 1u];
    char ram_chars[RAM_MAX_LINES][IRC_LINE_W];
    unsigned char ram_colors[RAM_MAX_LINES][IRC_LINE_W];
    unsigned int first_line;
    unsigned int line_count;
    unsigned int port;
    unsigned char input_len;
    unsigned char input_cursor;
    unsigned int partial_len;
    unsigned char dropping_line;
    unsigned char scroll_back;
    unsigned char scroll_bank;
    unsigned char reu_scroll;
    unsigned char ui_mode;
    unsigned char connected;
    unsigned char connection_wanted;
    unsigned char socket_id;
    unsigned char setup_focus;
    unsigned char setup_cursor[SETUP_FIELD_COUNT];
} ReadyIrcResumeV2;

static unsigned char running;
static unsigned char connected;
static unsigned char connection_wanted;
static unsigned char ui_mode;
static unsigned char socket_id;
static unsigned char scroll_bank;
static unsigned char reu_scroll;
static unsigned char scroll_back;
static unsigned int first_line;
static unsigned int line_count;
static unsigned int max_lines;

static char server_buf[SERVER_MAX + 1u];
static char port_text_buf[PORT_TEXT_MAX + 1u];
static char nick_buf[NICK_MAX + 1u];
static char channel_buf[CHANNEL_MAX + 1u];
static unsigned int port_value;
static TuiInput setup_inputs[SETUP_FIELD_COUNT];
static unsigned char setup_focus;
static char setup_status[41];

static char input_buf[INPUT_MAX + 1u];
static unsigned char input_len;
static unsigned char input_cursor;
static char in_line[IRC_IN_MAX + 1u];
static unsigned int in_len;
static unsigned char dropping_line;

static char build_chars[IRC_LINE_W];
static unsigned char build_colors[IRC_LINE_W];
static unsigned char build_len;

static unsigned char line_chars[IRC_LINE_W];
static unsigned char line_colors[IRC_LINE_W];
static unsigned char net_buf[NET_READ_CHUNK];
static char send_buf[128];
static ReadyIrcResumeV2 resume_blob;

static char ram_chars[RAM_MAX_LINES][IRC_LINE_W];
static unsigned char ram_colors[RAM_MAX_LINES][IRC_LINE_W];

static void draw_shell(void);
static void draw_header(void);
static void draw_output(void);
static void draw_output_row(unsigned char row, unsigned int rel_index);
static unsigned int output_start_rel(void);
static void shift_output_up(void);
static void shift_output_down(void);
static void draw_input(void);
static void draw_setup(void);
static void draw_status(const char *msg, unsigned char color);
static void add_status_line(const char *msg);
static void add_text_line(const char *msg, unsigned char color);
static void line_begin(void);
static void line_char(unsigned char ch, unsigned char color);
static void line_text(const char *msg, unsigned char color);
static void line_flush(void);
static void store_line(const char *chars, const unsigned char *colors);
static void fetch_line(unsigned int rel_index);
static unsigned int physical_line(unsigned int rel_index);
static unsigned char screen_ch(unsigned char ch);
static unsigned char lower_ascii(unsigned char ch);
static void normalize_display_in_place(char *s);
static void lowercase_in_place(char *s);
static unsigned char text_eq(const char *a, const char *b);
static unsigned char starts_with(const char *s, const char *prefix);
static char *find_substr(char *s, const char *needle);
static char *find_char(char *s, char needle);
static void append_fit(char *dst, unsigned char cap, const char *src);
static void append_wire_lower(char *dst, unsigned char cap, const char *src);
static void append_wire_eol(char *dst, unsigned char cap);
static unsigned char nick_color(const char *nick);
static void add_privmsg(const char *nick, const char *msg);
static void add_action(const char *nick, const char *msg);
static void add_join_part(const char *nick, const char *verb);
static void parse_irc_line(char *line);
static void poll_network(void);
static void process_rx_byte(unsigned char ch);
static void send_raw(const char *s);
static void send_login(void);
static unsigned char connect_irc(void);
static void disconnect_irc(void);
static void disconnect_to_setup(void);
static void connection_lost(void);
static void probe_resumed_connection(void);
static void setup_connect(void);
static void handle_input_submit(void);
static void handle_key(unsigned char key);
static void handle_setup_key(unsigned char key);
static void handle_chat_key(unsigned char key);
static void setup_inputs_init(void);
static void load_defaults(void);
static unsigned char validate_settings(void);
static unsigned char valid_channel(const char *channel);
static unsigned char parse_port_text(unsigned int *value_out);
static void resume_save_state(void);
static unsigned char resume_restore_state(void);
static void readyirc_return_to_launcher(void);
static void readyirc_switch_to_app(unsigned char bank);

static unsigned char lower_ascii(unsigned char ch) {
    /* Canonical lowercase PETSCII letters occupy $41-$5A. */
    if (ch >= 0x61u && ch <= 0x7au) {
        return (unsigned char)(ch - 0x20u);
    }
    if (ch >= 0xc1u && ch <= 0xdau) {
        return (unsigned char)(ch - 0x80u);
    }
    return ch;
}

static void normalize_display_in_place(char *s) {
    unsigned char i;
    unsigned char ch;

    for (i = 0u; s[i] != 0; ++i) {
        ch = (unsigned char)s[i];
        if (ch < 32u && ch != 1u) {
            s[i] = ' ';
        } else {
            s[i] = (char)lower_ascii(ch);
        }
    }
}

static void lowercase_in_place(char *s) {
    while (*s != 0) {
        *s = (char)lower_ascii((unsigned char)*s);
        ++s;
    }
}

static unsigned char text_eq(const char *a, const char *b) {
    while (*a != 0 && *b != 0) {
        if (lower_ascii((unsigned char)*a) != lower_ascii((unsigned char)*b)) {
            return 0u;
        }
        ++a;
        ++b;
    }
    return (unsigned char)(*a == 0 && *b == 0);
}

static unsigned char starts_with(const char *s, const char *prefix) {
    while (*prefix != 0) {
        if (lower_ascii((unsigned char)*s) != lower_ascii((unsigned char)*prefix)) {
            return 0u;
        }
        ++s;
        ++prefix;
    }
    return 1u;
}

static char *find_char(char *s, char needle) {
    while (*s != 0) {
        if (*s == needle) {
            return s;
        }
        ++s;
    }
    return 0;
}

static char *find_substr(char *s, const char *needle) {
    unsigned char i;

    while (*s != 0) {
        i = 0u;
        while (needle[i] != 0 &&
               lower_ascii((unsigned char)s[i]) ==
               lower_ascii((unsigned char)needle[i])) {
            ++i;
        }
        if (needle[i] == 0) {
            return s;
        }
        ++s;
    }
    return 0;
}

static void append_fit(char *dst, unsigned char cap, const char *src) {
    unsigned char len;

    if (cap == 0u) {
        return;
    }
    len = (unsigned char)strlen(dst);
    while (*src != 0 && len + 1u < cap) {
        dst[len] = *src;
        ++len;
        ++src;
    }
    dst[len] = 0;
}

static void append_wire_lower(char *dst, unsigned char cap, const char *src) {
    unsigned char ch;
    unsigned char len;

    if (cap == 0u) {
        return;
    }
    len = (unsigned char)strlen(dst);
    while (*src != 0 && len + 1u < cap) {
        ch = (unsigned char)*src;
        if (ch >= 0x41u && ch <= 0x5au) {
            ch = (unsigned char)(ch + 0x20u);
        } else if (ch >= 0xc1u && ch <= 0xdau) {
            ch = (unsigned char)(ch - 0x60u);
        }
        dst[len] = (char)ch;
        ++len;
        ++src;
    }
    dst[len] = 0;
}

static void append_wire_eol(char *dst, unsigned char cap) {
    unsigned char len;

    len = (unsigned char)strlen(dst);
    if (len + 2u >= cap) {
        return;
    }
    dst[len] = 0x0d;
    dst[(unsigned char)(len + 1u)] = 0x0a;
    dst[(unsigned char)(len + 2u)] = 0;
}

static unsigned char screen_ch(unsigned char ch) {
    if (ch == 1u) {
        return 32u;
    }
    return tui_ascii_to_screen((char)lower_ascii(ch));
}

static unsigned char nick_color(const char *nick) {
    unsigned char hash;
    static const unsigned char colors[] = {
        TUI_COLOR_CYAN,
        TUI_COLOR_LIGHTGREEN,
        TUI_COLOR_YELLOW,
        TUI_COLOR_LIGHTBLUE,
        TUI_COLOR_LIGHTRED,
        TUI_COLOR_ORANGE
    };

    hash = 0u;
    while (*nick != 0) {
        hash = (unsigned char)(hash + (unsigned char)*nick);
        ++nick;
    }
    return colors[(unsigned char)(hash % sizeof(colors))];
}

static void force_lowercase_charset(void) {
    VIC.addr = 0x16u;
}

static void format_port_text(unsigned int value) {
    char reversed[PORT_TEXT_MAX];
    unsigned char len;
    unsigned char i;

    len = 0u;
    do {
        reversed[len] = (char)('0' + (value % 10u));
        value /= 10u;
        ++len;
    } while (value != 0u && len < PORT_TEXT_MAX);

    for (i = 0u; i < len; ++i) {
        port_text_buf[i] = reversed[(unsigned char)(len - i - 1u)];
    }
    port_text_buf[len] = 0;
}

static void setup_inputs_init(void) {
    tui_input_init(&setup_inputs[0], 10u, 4u, 29u, SERVER_MAX,
                   server_buf, TUI_COLOR_WHITE);
    tui_input_init(&setup_inputs[1], 10u, 7u, 8u, PORT_TEXT_MAX,
                   port_text_buf, TUI_COLOR_WHITE);
    tui_input_init(&setup_inputs[2], 10u, 10u, 29u, NICK_MAX,
                   nick_buf, TUI_COLOR_WHITE);
    tui_input_init(&setup_inputs[3], 10u, 13u, 29u, CHANNEL_MAX,
                   channel_buf, TUI_COLOR_WHITE);
    setup_focus = 0u;
}

static void load_defaults(void) {
    strcpy(server_buf, readyirc_config_server);
    strcpy(nick_buf, readyirc_config_nick);
    strcpy(channel_buf, readyirc_config_channel);
    port_value = readyirc_config_port;
    format_port_text(port_value);
    setup_inputs[0].cursor = (unsigned char)strlen(server_buf);
    setup_inputs[1].cursor = (unsigned char)strlen(port_text_buf);
    setup_inputs[2].cursor = (unsigned char)strlen(nick_buf);
    setup_inputs[3].cursor = (unsigned char)strlen(channel_buf);
    setup_status[0] = 0;
}

static unsigned char parse_port_text(unsigned int *value_out) {
    unsigned int value;
    unsigned char digit;
    unsigned char i;

    if (port_text_buf[0] == 0) {
        return 0u;
    }
    value = 0u;
    for (i = 0u; port_text_buf[i] != 0; ++i) {
        if (port_text_buf[i] < '0' || port_text_buf[i] > '9') {
            return 0u;
        }
        digit = (unsigned char)(port_text_buf[i] - '0');
        if (value > 6553u || (value == 6553u && digit > 5u)) {
            return 0u;
        }
        value = (unsigned int)(value * 10u + digit);
    }
    if (value == 0u) {
        return 0u;
    }
    *value_out = value;
    return 1u;
}

static unsigned char valid_channel(const char *channel) {
    unsigned char i;
    unsigned char ch;

    if ((channel[0] != '#' && channel[0] != '&') ||
        channel[1] == 0 || strlen(channel) > CHANNEL_MAX) {
        return 0u;
    }
    for (i = 1u; channel[i] != 0; ++i) {
        ch = (unsigned char)channel[i];
        if (ch < 33u || ch == ',') {
            return 0u;
        }
    }
    return 1u;
}

static unsigned char validate_settings(void) {
    unsigned char i;
    unsigned char ch;

    lowercase_in_place(server_buf);
    lowercase_in_place(nick_buf);
    lowercase_in_place(channel_buf);

    if (server_buf[0] == 0 || strlen(server_buf) > SERVER_MAX) {
        strcpy(setup_status, "server is required");
        return 0u;
    }
    for (i = 0u; server_buf[i] != 0; ++i) {
        ch = (unsigned char)server_buf[i];
        if (!((ch >= 'a' && ch <= 'z') ||
              (ch >= '0' && ch <= '9') || ch == '.' || ch == '-' ||
              ch == ':')) {
            strcpy(setup_status, "invalid server");
            return 0u;
        }
    }
    if (!parse_port_text(&port_value)) {
        strcpy(setup_status, "port must be 1-65535");
        return 0u;
    }
    if (nick_buf[0] == 0 || strlen(nick_buf) > NICK_MAX) {
        strcpy(setup_status, "nickname is required");
        return 0u;
    }
    for (i = 0u; nick_buf[i] != 0; ++i) {
        ch = (unsigned char)nick_buf[i];
        if (!((ch >= 'a' && ch <= 'z') ||
              (ch >= '0' && ch <= '9') || ch == '_' || ch == '-')) {
            strcpy(setup_status, "invalid nickname");
            return 0u;
        }
    }
    if ((channel_buf[0] != '#' && channel_buf[0] != '&') ||
        channel_buf[1] == 0 || strlen(channel_buf) > CHANNEL_MAX) {
        strcpy(setup_status, "channel must start with # or &");
        return 0u;
    }
    if (!valid_channel(channel_buf)) {
        strcpy(setup_status, "invalid channel");
        return 0u;
    }
    setup_status[0] = 0;
    return 1u;
}

static void draw_setup_field(unsigned char index, unsigned char y,
                             const char *label) {
    TuiInput *input;
    unsigned char color;

    input = &setup_inputs[index];
    color = (index == setup_focus) ? TUI_COLOR_CYAN : TUI_COLOR_GRAY3;
    tui_putc(1u, y, (index == setup_focus) ? tui_ascii_to_screen('>') : 32u,
             color);
    tui_puts_n(3u, y, label, 7u, color);
    if (index == setup_focus) {
        input->color = TUI_COLOR_YELLOW;
        tui_input_draw(input);
    } else {
        tui_puts_n(input->x, input->y, input->buffer, input->width,
                   TUI_COLOR_WHITE);
    }
}

static void draw_setup(void) {
    tui_init();
    force_lowercase_charset();
    tui_clear(TUI_THEME_BG);
    tui_puts_n(0u, 0u, "readyirc setup", 40u, TUI_COLOR_WHITE);
    tui_hline(0u, 1u, 40u, TUI_COLOR_LIGHTBLUE);
    draw_setup_field(0u, 4u, "server");
    draw_setup_field(1u, 7u, "port");
    draw_setup_field(2u, 10u, "nick");
    draw_setup_field(3u, 13u, "channel");
    tui_hline(0u, 16u, 40u, TUI_COLOR_LIGHTBLUE);
    tui_puts_n(0u, 18u,
               setup_status[0] != 0 ? setup_status : "return connects",
               40u, setup_status[0] != 0 ? TUI_COLOR_LIGHTRED :
                                           TUI_COLOR_LIGHTGREEN);
    tui_puts_n(0u, 22u, "up/down field  left/right/home edit", 40u,
               TUI_COLOR_GRAY3);
    tui_puts_n(0u, 23u, "del erases  return connect", 40u, TUI_COLOR_GRAY3);
    tui_puts_n(0u, 24u, "ctrl+b home  f2/f4 apps  runstop exit", 40u,
               TUI_COLOR_GRAY3);
}

static void draw_header(void) {
    char title[41];

    title[0] = 0;
    append_fit(title, sizeof(title), "readyirc ");
    append_fit(title, sizeof(title), connected ? "online " : "offline ");
    append_fit(title, sizeof(title), channel_buf);
    tui_clear_line(0u, 0u, 40u, TUI_COLOR_WHITE);
    tui_puts_n(0u, 0u, title, 40u, TUI_COLOR_WHITE);
    tui_clear_line(1u, 0u, 40u, TUI_COLOR_GRAY3);
    tui_puts_n(0u, 1u, server_buf, 24u, TUI_COLOR_GRAY3);
    tui_puts_n(25u, 1u, "tcp", 4u, TUI_COLOR_GRAY3);
    tui_puts_n(29u, 1u, port_text_buf, 11u, TUI_COLOR_GRAY3);
}

static void draw_shell(void) {
    tui_init();
    force_lowercase_charset();
    tui_clear(TUI_THEME_BG);
    draw_header();
    tui_hline(0u, (unsigned char)(OUTPUT_TOP - 1u), 40u, TUI_COLOR_LIGHTBLUE);
    tui_hline(0u, (unsigned char)(INPUT_Y - 1u), 40u, TUI_COLOR_LIGHTBLUE);
    tui_puts_n(0u, HELP_Y, "f1 disconnect ctrl+b home f2/f4 apps", 40u, TUI_COLOR_GRAY3);
    draw_status("starting", TUI_COLOR_GRAY3);
    draw_output();
    draw_input();
}

static void draw_status(const char *msg, unsigned char color) {
    tui_clear_line(STATUS_Y, 0u, 40u, color);
    tui_puts_n(0u, STATUS_Y, msg, 40u, color);
}

static unsigned int physical_line(unsigned int rel_index) {
    return (unsigned int)((first_line + rel_index) % max_lines);
}

static void fetch_line(unsigned int rel_index) {
    unsigned int idx;

    idx = physical_line(rel_index);
    if (reu_scroll) {
        reu_dma_fetch((unsigned int)line_chars, scroll_bank,
                      (unsigned int)(idx * LINE_REC_LEN), IRC_LINE_W);
        reu_dma_fetch((unsigned int)line_colors, scroll_bank,
                      (unsigned int)(idx * LINE_REC_LEN + IRC_LINE_W), IRC_LINE_W);
    } else {
        memcpy(line_chars, ram_chars[idx], IRC_LINE_W);
        memcpy(line_colors, ram_colors[idx], IRC_LINE_W);
    }
}

static unsigned int output_start_rel(void) {
    unsigned int start;

    if (line_count <= OUTPUT_H) {
        return 0u;
    }
    start = (unsigned int)(line_count - OUTPUT_H);
    if (scroll_back > start) {
        scroll_back = (unsigned char)start;
    }
    return (unsigned int)(start - scroll_back);
}

static void draw_output_row(unsigned char row, unsigned int rel_index) {
    unsigned char col;
    unsigned int offset;

    offset = (unsigned int)(OUTPUT_TOP + row) * 40u;
    if (rel_index >= line_count) {
        for (col = 0u; col < 40u; ++col) {
            TUI_SCREEN[offset + col] = 32u;
            TUI_COLOR_RAM[offset + col] = TUI_COLOR_WHITE;
        }
        return;
    }

    fetch_line(rel_index);
    for (col = 0u; col < 40u; ++col) {
        TUI_SCREEN[offset + col] = screen_ch(line_chars[col]);
    }
    memcpy(TUI_COLOR_RAM + offset, line_colors, IRC_LINE_W);
}

static void shift_output_up(void) {
    unsigned int offset;

    offset = (unsigned int)OUTPUT_TOP * 40u;
    memmove(TUI_SCREEN + offset, TUI_SCREEN + offset + 40u,
            (unsigned int)(OUTPUT_H - 1u) * 40u);
    memmove(TUI_COLOR_RAM + offset, TUI_COLOR_RAM + offset + 40u,
            (unsigned int)(OUTPUT_H - 1u) * 40u);
}

static void shift_output_down(void) {
    unsigned int offset;

    offset = (unsigned int)OUTPUT_TOP * 40u;
    memmove(TUI_SCREEN + offset + 40u, TUI_SCREEN + offset,
            (unsigned int)(OUTPUT_H - 1u) * 40u);
    memmove(TUI_COLOR_RAM + offset + 40u, TUI_COLOR_RAM + offset,
            (unsigned int)(OUTPUT_H - 1u) * 40u);
}

static void draw_output(void) {
    unsigned char row;
    unsigned int start;

    start = output_start_rel();
    for (row = 0u; row < OUTPUT_H; ++row) {
        draw_output_row(row, (unsigned int)(start + row));
    }
}

static void draw_input(void) {
    unsigned char i;
    unsigned char display_start;
    unsigned char width;
    unsigned int offset;

    width = 38u;
    if (input_cursor >= width) {
        display_start = (unsigned char)(input_cursor - width + 1u);
    } else {
        display_start = 0u;
    }

    tui_clear_line(INPUT_Y, 0u, 40u, TUI_COLOR_WHITE);
    tui_putc(0u, INPUT_Y, tui_ascii_to_screen('>'), TUI_COLOR_LIGHTGREEN);
    offset = (unsigned int)INPUT_Y * 40u + 2u;
    for (i = 0u; i < width; ++i) {
        if ((unsigned char)(display_start + i) < input_len) {
            TUI_SCREEN[offset + i] = screen_ch((unsigned char)input_buf[(unsigned char)(display_start + i)]);
        } else {
            TUI_SCREEN[offset + i] = 32u;
        }
        TUI_COLOR_RAM[offset + i] = TUI_COLOR_WHITE;
    }
    if (input_cursor >= display_start && input_cursor < (unsigned char)(display_start + width)) {
        TUI_COLOR_RAM[offset + (input_cursor - display_start)] = TUI_COLOR_YELLOW;
    }
}

static void line_begin(void) {
    unsigned char i;

    build_len = 0u;
    for (i = 0u; i < IRC_LINE_W; ++i) {
        build_chars[i] = ' ';
        build_colors[i] = TUI_COLOR_WHITE;
    }
}

static void line_flush(void) {
    if (build_len == 0u) {
        return;
    }
    store_line(build_chars, build_colors);
    line_begin();
}

static void line_char(unsigned char ch, unsigned char color) {
    if (ch == '\r' || ch == '\n') {
        line_flush();
        return;
    }
    if (ch < 32u && ch != 1u) {
        return;
    }
    if (build_len >= IRC_LINE_W) {
        line_flush();
    }
    build_chars[build_len] = (char)lower_ascii(ch);
    build_colors[build_len] = color;
    ++build_len;
}

static void line_text(const char *msg, unsigned char color) {
    while (*msg != 0) {
        line_char((unsigned char)*msg, color);
        ++msg;
    }
}

static void store_line(const char *chars, const unsigned char *colors) {
    unsigned int idx;
    unsigned int old_count;
    unsigned int start;
    unsigned char old_scroll;

    old_count = line_count;
    old_scroll = scroll_back;

    if (line_count < max_lines) {
        idx = physical_line(line_count);
        ++line_count;
    } else {
        idx = first_line;
        first_line = (unsigned int)((first_line + 1u) % max_lines);
    }

    if (reu_scroll) {
        reu_dma_stash((unsigned int)chars, scroll_bank,
                      (unsigned int)(idx * LINE_REC_LEN), IRC_LINE_W);
        reu_dma_stash((unsigned int)colors, scroll_bank,
                      (unsigned int)(idx * LINE_REC_LEN + IRC_LINE_W), IRC_LINE_W);
    } else {
        memcpy(ram_chars[idx], chars, IRC_LINE_W);
        memcpy(ram_colors[idx], colors, IRC_LINE_W);
    }

    if (old_scroll != 0u) {
        start = line_count > OUTPUT_H ?
                (unsigned int)(line_count - OUTPUT_H) : 0u;
        if (scroll_back < start && scroll_back < 255u) {
            ++scroll_back;
            return;
        }
        shift_output_up();
        start = output_start_rel();
        draw_output_row((unsigned char)(OUTPUT_H - 1u),
                        (unsigned int)(start + OUTPUT_H - 1u));
        return;
    }

    if (old_count < OUTPUT_H) {
        draw_output_row((unsigned char)old_count, old_count);
        return;
    }
    shift_output_up();
    start = output_start_rel();
    draw_output_row((unsigned char)(OUTPUT_H - 1u),
                    (unsigned int)(start + OUTPUT_H - 1u));
}

static void add_text_line(const char *msg, unsigned char color) {
    line_begin();
    line_text(msg, color);
    line_flush();
}

static void add_status_line(const char *msg) {
    add_text_line(msg, TUI_COLOR_GRAY3);
}

static void add_privmsg(const char *nick, const char *msg) {
    unsigned char color;

    color = nick_color(nick);
    line_begin();
    line_char('<', TUI_COLOR_CYAN);
    line_text(nick, color);
    line_text("> ", TUI_COLOR_CYAN);
    line_text(msg, TUI_COLOR_WHITE);
    line_flush();
}

static void add_action(const char *nick, const char *msg) {
    line_begin();
    line_text(" * ", TUI_COLOR_YELLOW);
    line_text(nick, nick_color(nick));
    line_char(' ', TUI_COLOR_YELLOW);
    line_text(msg, TUI_COLOR_YELLOW);
    line_flush();
}

static void add_join_part(const char *nick, const char *verb) {
    line_begin();
    line_text(" * ", TUI_COLOR_GRAY3);
    line_text(nick, nick_color(nick));
    line_char(' ', TUI_COLOR_GRAY3);
    line_text(verb, TUI_COLOR_GRAY3);
    line_flush();
}

static void send_raw(const char *s) {
    if (connected) {
        (void)readyirc_uci_socket_write(socket_id, s, (unsigned int)strlen(s));
    }
}

static void send_login(void) {
    send_buf[0] = 0;
    append_fit(send_buf, sizeof(send_buf), "nick ");
    append_wire_lower(send_buf, sizeof(send_buf), nick_buf);
    append_wire_eol(send_buf, sizeof(send_buf));
    send_raw(send_buf);

    send_buf[0] = 0;
    append_fit(send_buf, sizeof(send_buf), "user ");
    append_wire_lower(send_buf, sizeof(send_buf), nick_buf);
    append_fit(send_buf, sizeof(send_buf), " * 0 :");
    append_wire_lower(send_buf, sizeof(send_buf), nick_buf);
    append_wire_eol(send_buf, sizeof(send_buf));
    send_raw(send_buf);

    send_buf[0] = 0;
    append_fit(send_buf, sizeof(send_buf), "join ");
    append_wire_lower(send_buf, sizeof(send_buf), channel_buf);
    append_wire_eol(send_buf, sizeof(send_buf));
    send_raw(send_buf);
}

static unsigned char connect_irc(void) {
    if (!readyirc_uci_detect()) {
        add_status_line("uci command interface not detected");
        draw_status("uci not detected", TUI_COLOR_LIGHTRED);
        strcpy(setup_status, "uci not detected");
        return 0u;
    }
    add_status_line("connecting");
    draw_status("connecting", TUI_COLOR_YELLOW);
    if (!readyirc_uci_tcp_connect(server_buf,
                                  port_value,
                                  &socket_id)) {
        add_status_line("connect failed");
        add_text_line(readyirc_uci_last_status(), TUI_COLOR_GRAY3);
        draw_status("connect failed", TUI_COLOR_LIGHTRED);
        connected = 0u;
        strcpy(setup_status, "connect failed");
        return 0u;
    }
    connected = 1u;
    draw_header();
    draw_status("connected", TUI_COLOR_LIGHTGREEN);
    add_status_line("connected");
    send_login();
    return 1u;
}

static void disconnect_irc(void) {
    if (connected) {
        readyirc_uci_socket_close(socket_id);
        connected = 0u;
        draw_header();
        add_status_line("disconnected");
        draw_status("disconnected", TUI_COLOR_GRAY3);
    }
}

static void disconnect_to_setup(void) {
    if (connected) {
        strcpy(send_buf, "quit :readyirc disconnect");
        append_wire_eol(send_buf, sizeof(send_buf));
        send_raw(send_buf);
    }
    disconnect_irc();
    connection_wanted = 0u;
    ui_mode = UI_MODE_SETUP;
    strcpy(setup_status, "disconnected");
    draw_setup();
}

static void connection_lost(void) {
    connected = 0u;
    add_status_line("connection expired");
    draw_status("reconnecting", TUI_COLOR_YELLOW);
    readyirc_uci_socket_close(socket_id);
    if (connection_wanted && connect_irc()) {
        add_status_line("reconnected");
        return;
    }
    connection_wanted = 0u;
    ui_mode = UI_MODE_SETUP;
    strcpy(setup_status, "reconnect failed");
    draw_setup();
}

static void setup_connect(void) {
    if (!validate_settings()) {
        draw_setup();
        return;
    }
    connection_wanted = 1u;
    ui_mode = UI_MODE_CHAT;
    draw_shell();
    if (!connect_irc()) {
        connection_wanted = 0u;
        ui_mode = UI_MODE_SETUP;
        draw_setup();
    }
}

static void parse_irc_line(char *line) {
    char *p;
    char *cmd;
    char *target;
    char *msg;
    char *channel;
    char *end;
    char nick[32];
    unsigned char i;

    if (starts_with(line, "ping ")) {
        send_buf[0] = 0;
        append_fit(send_buf, sizeof(send_buf), "pong ");
        append_fit(send_buf, sizeof(send_buf), line + 5);
        append_wire_eol(send_buf, sizeof(send_buf));
        send_raw(send_buf);
        return;
    }

    if (line[0] != ':') {
        add_status_line(line);
        return;
    }

    p = line + 1;
    end = find_char(p, '!');
    if (end == 0) {
        end = find_char(p, ' ');
    }
    if (end == 0) {
        add_status_line(line);
        return;
    }

    i = 0u;
    while (p < end && i + 1u < sizeof(nick)) {
        nick[i] = *p;
        ++i;
        ++p;
    }
    nick[i] = 0;

    cmd = find_char(end, ' ');
    if (cmd == 0) {
        add_status_line(line);
        return;
    }
    ++cmd;
    target = find_char(cmd, ' ');
    if (target == 0) {
        add_status_line(cmd);
        return;
    }
    *target = 0;
    ++target;

    if (text_eq(cmd, "353")) {
        msg = find_substr(target, " :");
        if (msg == 0) {
            return;
        }
        *msg = 0;
        msg += 2;
        channel = target;
        while (*channel != 0 && *channel != '#' && *channel != '&') {
            ++channel;
        }
        end = channel;
        while (*end != 0 && *end != ' ') {
            ++end;
        }
        if (*end != 0) {
            *end = 0;
        }
        line_begin();
        line_text("names ", TUI_COLOR_GRAY3);
        line_text(*channel != 0 ? channel : channel_buf, TUI_COLOR_CYAN);
        line_text(": ", TUI_COLOR_GRAY3);
        line_text(msg, TUI_COLOR_WHITE);
        line_flush();
        return;
    }

    if (text_eq(cmd, "366")) {
        channel = target;
        while (*channel != 0 && *channel != '#' && *channel != '&') {
            ++channel;
        }
        end = channel;
        while (*end != 0 && *end != ' ' && *end != ':') {
            ++end;
        }
        if (*end != 0) {
            *end = 0;
        }
        line_begin();
        line_text("end names ", TUI_COLOR_GRAY3);
        line_text(*channel != 0 ? channel : channel_buf, TUI_COLOR_CYAN);
        line_flush();
        return;
    }

    if (text_eq(cmd, "privmsg")) {
        msg = find_substr(target, " :");
        if (msg == 0) {
            return;
        }
        *msg = 0;
        msg += 2;
        if (!text_eq(target, channel_buf)) {
            add_join_part(nick, "dm ignored");
            return;
        }
        if (msg[0] == 1 && starts_with(msg + 1, "action ")) {
            msg += 8;
            end = find_char(msg, 1);
            if (end != 0) {
                *end = 0;
            }
            add_action(nick, msg);
        } else {
            add_privmsg(nick, msg);
        }
        return;
    }

    if (text_eq(cmd, "join")) {
        add_join_part(nick, "joined");
        return;
    }
    if (text_eq(cmd, "part")) {
        add_join_part(nick, "left");
        return;
    }
    if (text_eq(cmd, "quit")) {
        add_join_part(nick, "quit");
        return;
    }
    if (text_eq(cmd, "nick")) {
        msg = find_char(target, ':');
        add_join_part(nick, "is now known as");
        if (msg != 0) {
            add_text_line(msg + 1, nick_color(msg + 1));
        }
        return;
    }

    msg = find_substr(target, " :");
    if (msg != 0) {
        add_status_line(msg + 2);
    } else {
        add_status_line(target);
    }
}

static void process_rx_byte(unsigned char ch) {
    if (ch == 0x0au) {
        return;
    }
    if (ch == 0x0du) {
        if (!dropping_line && in_len != 0u) {
            in_line[in_len] = 0;
            parse_irc_line(in_line);
        }
        in_len = 0u;
        dropping_line = 0u;
        return;
    }

    if (dropping_line) {
        return;
    }
    if (in_len < IRC_IN_MAX) {
        in_line[in_len] = (char)ch;
        ++in_len;
    } else {
        dropping_line = 1u;
    }
}

static void poll_network(void) {
    unsigned char burst;
    unsigned char i;
    unsigned char status_code;
    unsigned int n;

    if (!connected) {
        return;
    }
    for (burst = 0u; burst < NET_READ_BURST; ++burst) {
        n = readyirc_uci_socket_read(socket_id, net_buf, NET_READ_CHUNK);
        status_code = readyirc_uci_last_status_code();
        if (status_code == UCI_STATUS_NO_DATA) {
            break;
        }
        if (status_code != UCI_STATUS_OK) {
            connection_lost();
            return;
        }
        if (n == 0u) {
            break;
        }
        for (i = 0u; i < n; ++i) {
            process_rx_byte(net_buf[i]);
        }
    }
}

static void probe_resumed_connection(void) {
    unsigned int n;
    unsigned char i;
    unsigned char status_code;

    if (!connection_wanted || !connected) {
        return;
    }
    n = readyirc_uci_socket_read(socket_id, net_buf, NET_READ_CHUNK);
    status_code = readyirc_uci_last_status_code();
    if (status_code == UCI_STATUS_NO_DATA) {
        draw_status("connected", TUI_COLOR_LIGHTGREEN);
        return;
    }
    if (status_code != UCI_STATUS_OK) {
        connection_lost();
        return;
    }
    for (i = 0u; i < n; ++i) {
        process_rx_byte(net_buf[i]);
    }
    draw_status("connected", TUI_COLOR_LIGHTGREEN);
}

static void handle_input_submit(void) {
    char *arg;

    if (input_len == 0u) {
        return;
    }
    input_buf[input_len] = 0;
    normalize_display_in_place(input_buf);

    if (text_eq(input_buf, "/disconnect")) {
        input_len = 0u;
        input_cursor = 0u;
        input_buf[0] = 0;
        disconnect_to_setup();
        return;
    }

    if (text_eq(input_buf, "/join") || starts_with(input_buf, "/join ")) {
        if (!connected) {
            add_status_line("not connected");
        } else {
            arg = input_buf + 5;
            if (*arg == ' ') {
                ++arg;
            }
            if (!valid_channel(arg)) {
                add_status_line("usage /join #channel");
            } else if (text_eq(arg, channel_buf)) {
                add_status_line("already in channel");
            } else {
                send_buf[0] = 0;
                append_fit(send_buf, sizeof(send_buf), "part ");
                append_wire_lower(send_buf, sizeof(send_buf), channel_buf);
                append_fit(send_buf, sizeof(send_buf), " :changing channel");
                append_wire_eol(send_buf, sizeof(send_buf));
                send_raw(send_buf);

                strcpy(channel_buf, arg);
                lowercase_in_place(channel_buf);
                send_buf[0] = 0;
                append_fit(send_buf, sizeof(send_buf), "join ");
                append_wire_lower(send_buf, sizeof(send_buf), channel_buf);
                append_wire_eol(send_buf, sizeof(send_buf));
                send_raw(send_buf);
                draw_header();

                send_buf[0] = 0;
                append_fit(send_buf, sizeof(send_buf), "joining ");
                append_fit(send_buf, sizeof(send_buf), channel_buf);
                add_status_line(send_buf);
            }
        }
        input_len = 0u;
        input_cursor = 0u;
        input_buf[0] = 0;
        draw_input();
        return;
    }

    if (text_eq(input_buf, "/names") || starts_with(input_buf, "/names ")) {
        if (!connected) {
            add_status_line("not connected");
        } else {
            arg = input_buf + 6;
            if (*arg == ' ') {
                ++arg;
            }
            if (*arg == 0) {
                arg = channel_buf;
            }
            if (!valid_channel(arg)) {
                add_status_line("usage /names #channel");
            } else {
                send_buf[0] = 0;
                append_fit(send_buf, sizeof(send_buf), "names ");
                append_wire_lower(send_buf, sizeof(send_buf), arg);
                append_wire_eol(send_buf, sizeof(send_buf));
                send_raw(send_buf);
            }
        }
        input_len = 0u;
        input_cursor = 0u;
        input_buf[0] = 0;
        draw_input();
        return;
    }

    if (starts_with(input_buf, "/quit")) {
        strcpy(send_buf, "quit :readyirc");
        append_wire_eol(send_buf, sizeof(send_buf));
        send_raw(send_buf);
        disconnect_irc();
        connection_wanted = 0u;
        ui_mode = UI_MODE_SETUP;
        readyirc_return_to_launcher();
        return;
    }
    if (starts_with(input_buf, "/me ")) {
        send_buf[0] = 0;
        append_fit(send_buf, sizeof(send_buf), "privmsg ");
        append_wire_lower(send_buf, sizeof(send_buf), channel_buf);
        append_fit(send_buf, sizeof(send_buf), " :\x01");
        append_fit(send_buf, sizeof(send_buf), "action ");
        append_wire_lower(send_buf, sizeof(send_buf), input_buf + 4);
        append_fit(send_buf, sizeof(send_buf), "\x01");
        append_wire_eol(send_buf, sizeof(send_buf));
        send_raw(send_buf);
        add_action(nick_buf, input_buf + 4);
    } else if (starts_with(input_buf, "/msg ")) {
        add_status_line("dm support disabled");
    } else if (connected) {
        send_buf[0] = 0;
        append_fit(send_buf, sizeof(send_buf), "privmsg ");
        append_wire_lower(send_buf, sizeof(send_buf), channel_buf);
        append_fit(send_buf, sizeof(send_buf), " :");
        append_wire_lower(send_buf, sizeof(send_buf), input_buf);
        append_wire_eol(send_buf, sizeof(send_buf));
        send_raw(send_buf);
        add_privmsg(nick_buf, input_buf);
    } else {
        add_status_line("not connected");
    }

    input_len = 0u;
    input_cursor = 0u;
    input_buf[0] = 0;
    draw_input();
}

static void handle_chat_key(unsigned char key) {
    unsigned char i;
    unsigned int max_scroll;

    switch (key) {
        case TUI_KEY_RUNSTOP:
            disconnect_irc();
            connection_wanted = 0u;
            running = 0u;
            break;
        case TUI_KEY_F1:
            disconnect_to_setup();
            break;
        case TUI_KEY_RETURN:
            handle_input_submit();
            break;
        case TUI_KEY_DEL:
            if (input_cursor > 0u && input_len > 0u) {
                --input_cursor;
                --input_len;
                for (i = input_cursor; i < input_len; ++i) {
                    input_buf[i] = input_buf[(unsigned char)(i + 1u)];
                }
                input_buf[input_len] = 0;
                draw_input();
            }
            break;
        case TUI_KEY_LEFT:
            if (input_cursor > 0u) {
                --input_cursor;
                draw_input();
            }
            break;
        case TUI_KEY_RIGHT:
            if (input_cursor < input_len) {
                ++input_cursor;
                draw_input();
            }
            break;
        case TUI_KEY_UP:
            if (line_count > OUTPUT_H) {
                max_scroll = (unsigned int)(line_count - OUTPUT_H);
                if (scroll_back < max_scroll && scroll_back < 255u) {
                    ++scroll_back;
                    shift_output_down();
                    draw_output_row(0u, output_start_rel());
                }
            }
            break;
        case TUI_KEY_DOWN:
            if (scroll_back > 0u) {
                --scroll_back;
                shift_output_up();
                max_scroll = output_start_rel();
                draw_output_row((unsigned char)(OUTPUT_H - 1u),
                                (unsigned int)(max_scroll + OUTPUT_H - 1u));
            }
            break;
        default:
            if (key >= 32u && key < 127u && input_len < INPUT_MAX) {
                for (i = input_len; i > input_cursor; --i) {
                    input_buf[i] = input_buf[(unsigned char)(i - 1u)];
                }
                input_buf[input_cursor] = (char)lower_ascii(key);
                ++input_cursor;
                ++input_len;
                input_buf[input_len] = 0;
                draw_input();
            }
            break;
    }
}

static void handle_setup_key(unsigned char key) {
    if (key == TUI_KEY_RUNSTOP) {
        connection_wanted = 0u;
        running = 0u;
        return;
    }
    if (key == TUI_KEY_UP) {
        if (setup_focus == 0u) {
            setup_focus = SETUP_FIELD_COUNT - 1u;
        } else {
            --setup_focus;
        }
        draw_setup();
        return;
    }
    if (key == TUI_KEY_DOWN) {
        ++setup_focus;
        if (setup_focus >= SETUP_FIELD_COUNT) {
            setup_focus = 0u;
        }
        draw_setup();
        return;
    }
    if (key == TUI_KEY_RETURN) {
        setup_connect();
        return;
    }
    if (key >= 'A' && key <= 'Z') {
        key = lower_ascii(key);
    }
    if (tui_input_key(&setup_inputs[setup_focus], key)) {
        setup_connect();
        return;
    }
    setup_status[0] = 0;
    draw_setup();
}

static void handle_key(unsigned char key) {
    unsigned char nav_action;

    nav_action = tui_handle_global_hotkey(key, SHIM_CURRENT_BANK, 1u);
    if (nav_action == TUI_HOTKEY_LAUNCHER) {
        readyirc_return_to_launcher();
        return;
    }
    if (nav_action >= TUI_APP_BANK_MIN && nav_action <= TUI_APP_BANK_MAX) {
        readyirc_switch_to_app(nav_action);
        return;
    }
    if (nav_action == TUI_HOTKEY_BIND_ONLY) {
        return;
    }

    if (ui_mode == UI_MODE_SETUP) {
        handle_setup_key(key);
    } else {
        handle_chat_key(key);
    }
}

static void resume_save_state(void) {
    unsigned char i;

    memcpy(resume_blob.server, server_buf, sizeof(server_buf));
    memcpy(resume_blob.port_text, port_text_buf, sizeof(port_text_buf));
    memcpy(resume_blob.nick, nick_buf, sizeof(nick_buf));
    memcpy(resume_blob.channel, channel_buf, sizeof(channel_buf));
    memcpy(resume_blob.input, input_buf, sizeof(input_buf));
    memcpy(resume_blob.partial_line, in_line, sizeof(in_line));
    memcpy(resume_blob.ram_chars, ram_chars, sizeof(ram_chars));
    memcpy(resume_blob.ram_colors, ram_colors, sizeof(ram_colors));
    resume_blob.first_line = first_line;
    resume_blob.line_count = line_count;
    resume_blob.port = port_value;
    resume_blob.input_len = input_len;
    resume_blob.input_cursor = input_cursor;
    resume_blob.partial_len = in_len;
    resume_blob.dropping_line = dropping_line;
    resume_blob.scroll_back = scroll_back;
    resume_blob.scroll_bank = scroll_bank;
    resume_blob.reu_scroll = reu_scroll;
    resume_blob.ui_mode = ui_mode;
    resume_blob.connected = connected;
    resume_blob.connection_wanted = connection_wanted;
    resume_blob.socket_id = socket_id;
    resume_blob.setup_focus = setup_focus;
    for (i = 0u; i < SETUP_FIELD_COUNT; ++i) {
        resume_blob.setup_cursor[i] = setup_inputs[i].cursor;
    }
    (void)resume_save(&resume_blob, sizeof(resume_blob));
}

static unsigned char resume_restore_state(void) {
    unsigned int payload_len;
    unsigned int restored_max;
    unsigned char i;

    if (!resume_try_load(&resume_blob, sizeof(resume_blob), &payload_len) ||
        payload_len != sizeof(resume_blob)) {
        return 0u;
    }
    restored_max = resume_blob.reu_scroll ? REU_MAX_LINES : RAM_MAX_LINES;
    if (resume_blob.server[SERVER_MAX] != 0 ||
        resume_blob.port_text[PORT_TEXT_MAX] != 0 ||
        resume_blob.nick[NICK_MAX] != 0 ||
        resume_blob.channel[CHANNEL_MAX] != 0 ||
        resume_blob.input[INPUT_MAX] != 0 ||
        resume_blob.partial_line[IRC_IN_MAX] != 0 ||
        resume_blob.input_len > INPUT_MAX ||
        resume_blob.input_cursor > resume_blob.input_len ||
        resume_blob.partial_len > IRC_IN_MAX ||
        resume_blob.ui_mode > UI_MODE_CHAT ||
        resume_blob.setup_focus >= SETUP_FIELD_COUNT ||
        resume_blob.line_count > restored_max ||
        (resume_blob.line_count != 0u && resume_blob.first_line >= restored_max) ||
        (resume_blob.reu_scroll && resume_blob.scroll_bank == 0xFFu) ||
        resume_blob.port == 0u) {
        return 0u;
    }
    memcpy(server_buf, resume_blob.server, sizeof(server_buf));
    memcpy(port_text_buf, resume_blob.port_text, sizeof(port_text_buf));
    memcpy(nick_buf, resume_blob.nick, sizeof(nick_buf));
    memcpy(channel_buf, resume_blob.channel, sizeof(channel_buf));
    memcpy(input_buf, resume_blob.input, sizeof(input_buf));
    memcpy(in_line, resume_blob.partial_line, sizeof(in_line));
    memcpy(ram_chars, resume_blob.ram_chars, sizeof(ram_chars));
    memcpy(ram_colors, resume_blob.ram_colors, sizeof(ram_colors));
    first_line = resume_blob.first_line;
    line_count = resume_blob.line_count;
    port_value = resume_blob.port;
    input_len = resume_blob.input_len;
    input_cursor = resume_blob.input_cursor;
    in_len = resume_blob.partial_len;
    dropping_line = resume_blob.dropping_line;
    scroll_back = resume_blob.scroll_back;
    scroll_bank = resume_blob.scroll_bank;
    reu_scroll = resume_blob.reu_scroll;
    max_lines = restored_max;
    ui_mode = resume_blob.ui_mode;
    connected = resume_blob.connected;
    connection_wanted = resume_blob.connection_wanted;
    socket_id = resume_blob.socket_id;
    setup_focus = resume_blob.setup_focus;
    for (i = 0u; i < SETUP_FIELD_COUNT; ++i) {
        setup_inputs[i].cursor = resume_blob.setup_cursor[i];
        if (setup_inputs[i].cursor > strlen(setup_inputs[i].buffer)) {
            setup_inputs[i].cursor = (unsigned char)strlen(setup_inputs[i].buffer);
        }
    }
    if (!connection_wanted) {
        connected = 0u;
    }
    setup_status[0] = 0;
    return 1u;
}

static void readyirc_return_to_launcher(void) {
    resume_save_state();
    tui_return_to_launcher();
}

static void readyirc_switch_to_app(unsigned char bank) {
    resume_save_state();
    tui_switch_to_app(bank);
}

int main(void) {
    unsigned char key;
    unsigned char resumed;

    reu_mgr_init();
    resume_init_for_app(SHIM_CURRENT_BANK, SHIM_CURRENT_BANK, READYIRC_RESUME_APP_SCHEMA);
    setup_inputs_init();

    running = 1u;
    connected = 0u;
    connection_wanted = 0u;
    ui_mode = UI_MODE_SETUP;
    socket_id = 0u;
    scroll_bank = 0xFFu;
    reu_scroll = 0u;
    max_lines = RAM_MAX_LINES;
    first_line = 0u;
    line_count = 0u;
    input_len = 0u;
    input_cursor = 0u;
    input_buf[0] = 0;
    in_len = 0u;
    dropping_line = 0u;
    scroll_back = 0u;

    resumed = resume_restore_state();
    if (!resumed) {
        load_defaults();
        scroll_bank = reu_alloc_owned_bank(1u, "scrl");
        reu_scroll = (unsigned char)(scroll_bank != 0xFFu);
        max_lines = reu_scroll ? REU_MAX_LINES : RAM_MAX_LINES;
        draw_setup();
    } else if (ui_mode == UI_MODE_SETUP) {
        draw_setup();
    } else {
        draw_shell();
        if (!reu_scroll) {
            add_status_line("reu scrollback unavailable");
        }
        if (connection_wanted && connected) {
            probe_resumed_connection();
        } else if (connection_wanted) {
            if (!connect_irc()) {
                connection_wanted = 0u;
                ui_mode = UI_MODE_SETUP;
                strcpy(setup_status, "reconnect failed");
                draw_setup();
            }
        }
    }

    while (running) {
        if (ui_mode == UI_MODE_CHAT) {
            poll_network();
        }
        if (tui_kbhit()) {
            key = tui_getkey();
            handle_key(key);
        } else {
            waitvsync();
        }
    }

    if (scroll_bank != 0xFFu) {
        reu_free_owned_bank(scroll_bank);
    }
    __asm__("jmp $FCE2");
    return 0;
}
