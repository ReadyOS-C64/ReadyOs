/*
 * rirc_rrnet.c - ReadyOS one-channel IRC client for RR-Net/IP65 TCP
 */

#include "../../lib/resume_state.h"
#include "../../lib/reu_mgr.h"
#include "../../lib/reu_owned_alloc.h"
#include "../../lib/tui.h"
#include "rirc_rrnet.h"

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
#define IRC_IN_MAX     150u
#define NET_READ_CHUNK 64u
#define NET_READ_BURST 2u

#define READYIRC_RRNET_RESUME_APP_SCHEMA RESUME_SCHEMA_V1

typedef struct ReadyIrcRrnetResumeV1 {
    char input[INPUT_MAX + 1u];
    unsigned char input_len;
    unsigned char scroll_back;
} ReadyIrcRrnetResumeV1;

static unsigned char running;
static unsigned char connected;
static unsigned char socket_id;
static unsigned char scroll_bank;
static unsigned char reu_scroll;
static unsigned char scroll_back;
static unsigned int first_line;
static unsigned int line_count;
static unsigned int max_lines;

static char input_buf[INPUT_MAX + 1u];
static unsigned char input_len;
static unsigned char input_cursor;
static char in_line[IRC_IN_MAX + 1u];
static unsigned char in_len;
static unsigned char dropping_line;

static char build_chars[IRC_LINE_W];
static unsigned char build_colors[IRC_LINE_W];
static unsigned char build_len;

static unsigned char line_chars[IRC_LINE_W];
static unsigned char line_colors[IRC_LINE_W];
static unsigned char net_buf[NET_READ_CHUNK];
static char send_buf[128];
static ReadyIrcRrnetResumeV1 resume_blob;

static char ram_chars[RAM_MAX_LINES][IRC_LINE_W];
static unsigned char ram_colors[RAM_MAX_LINES][IRC_LINE_W];

static void draw_shell(void);
static void draw_header(void);
static void draw_output(void);
static void draw_input(void);
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
static void normalize_in_place(char *s);
static unsigned char text_eq(const char *a, const char *b);
static unsigned char starts_with(const char *s, const char *prefix);
static char *find_substr(char *s, const char *needle);
static char *find_char(char *s, char needle);
static void append_fit(char *dst, unsigned char cap, const char *src);
static unsigned char nick_color(const char *nick);
static void add_privmsg(const char *nick, const char *msg);
static void add_action(const char *nick, const char *msg);
static void add_join_part(const char *nick, const char *verb);
static void parse_irc_line(char *line);
static void poll_network(void);
static void process_rx_byte(unsigned char ch);
static void send_raw(const char *s);
static void send_login(void);
static void connect_irc(void);
static void disconnect_irc(void);
static void handle_input_submit(void);
static void handle_key(unsigned char key);
static void resume_save_state(void);
static void resume_restore_state(void);
static void rirc_rrnet_return_to_launcher(void);
static void rirc_rrnet_switch_to_app(unsigned char bank);

static unsigned char lower_ascii(unsigned char ch) {
    if (ch >= 'A' && ch <= 'Z') {
        return (unsigned char)(ch + 32u);
    }
    return ch;
}

static void normalize_in_place(char *s) {
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

static void draw_header(void) {
    char title[41];

    title[0] = 0;
    append_fit(title, sizeof(title), "readyirc rrnet ");
    append_fit(title, sizeof(title), connected ? "online " : "offline ");
    append_fit(title, sizeof(title), rirc_rrnet_config_channel);
    tui_clear_line(0u, 0u, 40u, TUI_COLOR_WHITE);
    tui_puts_n(0u, 0u, title, 40u, TUI_COLOR_WHITE);
    tui_clear_line(1u, 0u, 40u, TUI_COLOR_GRAY3);
    tui_puts_n(0u, 1u, rirc_rrnet_config_server, 24u, TUI_COLOR_GRAY3);
    tui_puts_n(25u, 1u, "rrnet tcp", 15u, TUI_COLOR_GRAY3);
}

static void draw_shell(void) {
    tui_init();
    tui_clear(TUI_THEME_BG);
    draw_header();
    tui_hline(0u, (unsigned char)(OUTPUT_TOP - 1u), 40u, TUI_COLOR_LIGHTBLUE);
    tui_hline(0u, (unsigned char)(INPUT_Y - 1u), 40u, TUI_COLOR_LIGHTBLUE);
    tui_puts_n(0u, HELP_Y, "ctrl+b home  f2/f4 apps  up/down scroll", 40u, TUI_COLOR_GRAY3);
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

static void draw_output(void) {
    unsigned char row;
    unsigned char col;
    unsigned int start;
    unsigned int rel;
    unsigned int offset;

    for (row = 0u; row < OUTPUT_H; ++row) {
        offset = (unsigned int)(OUTPUT_TOP + row) * 40u;
        for (col = 0u; col < 40u; ++col) {
            TUI_SCREEN[offset + col] = 32u;
            TUI_COLOR_RAM[offset + col] = TUI_COLOR_WHITE;
        }
    }

    if (line_count == 0u) {
        return;
    }
    if (line_count > OUTPUT_H) {
        start = (unsigned int)(line_count - OUTPUT_H);
        if (scroll_back > start) {
            scroll_back = (unsigned char)start;
        }
        start = (unsigned int)(start - scroll_back);
    } else {
        start = 0u;
        scroll_back = 0u;
    }

    for (row = 0u; row < OUTPUT_H; ++row) {
        rel = (unsigned int)(start + row);
        if (rel >= line_count) {
            break;
        }
        fetch_line(rel);
        offset = (unsigned int)(OUTPUT_TOP + row) * 40u;
        for (col = 0u; col < 40u; ++col) {
            TUI_SCREEN[offset + col] = screen_ch(line_chars[col]);
            TUI_COLOR_RAM[offset + col] = line_colors[col];
        }
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

    scroll_back = 0u;
    draw_output();
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
        (void)rirc_rrnet_socket_write(socket_id, s, (unsigned int)strlen(s));
    }
}

static void send_login(void) {
    send_buf[0] = 0;
    append_fit(send_buf, sizeof(send_buf), "nick ");
    append_fit(send_buf, sizeof(send_buf), rirc_rrnet_config_nick);
    append_fit(send_buf, sizeof(send_buf), "\r\n");
    send_raw(send_buf);

    send_buf[0] = 0;
    append_fit(send_buf, sizeof(send_buf), "user ");
    append_fit(send_buf, sizeof(send_buf), rirc_rrnet_config_nick);
    append_fit(send_buf, sizeof(send_buf), " * 0 :");
    append_fit(send_buf, sizeof(send_buf), rirc_rrnet_config_nick);
    append_fit(send_buf, sizeof(send_buf), "\r\n");
    send_raw(send_buf);

    send_buf[0] = 0;
    append_fit(send_buf, sizeof(send_buf), "join ");
    append_fit(send_buf, sizeof(send_buf), rirc_rrnet_config_channel);
    append_fit(send_buf, sizeof(send_buf), "\r\n");
    send_raw(send_buf);
}

static void connect_irc(void) {
    if (!rirc_rrnet_detect()) {
        add_status_line("rr-net not initialized");
        draw_status("rr-net unavailable", TUI_COLOR_LIGHTRED);
        return;
    }
    add_status_line("connecting");
    draw_status("connecting", TUI_COLOR_YELLOW);
    if (!rirc_rrnet_tcp_connect(rirc_rrnet_config_server,
                                  rirc_rrnet_config_port,
                                  &socket_id)) {
        add_status_line("connect failed");
        add_text_line(rirc_rrnet_last_status(), TUI_COLOR_GRAY3);
        draw_status("connect failed", TUI_COLOR_LIGHTRED);
        connected = 0u;
        return;
    }
    connected = 1u;
    draw_header();
    draw_status("connected", TUI_COLOR_LIGHTGREEN);
    add_status_line("connected");
    send_login();
}

static void disconnect_irc(void) {
    if (connected) {
        rirc_rrnet_socket_close(socket_id);
        connected = 0u;
        draw_header();
        add_status_line("disconnected");
        draw_status("disconnected", TUI_COLOR_GRAY3);
    }
}

static void parse_irc_line(char *line) {
    char *p;
    char *cmd;
    char *target;
    char *msg;
    char *end;
    char nick[32];
    unsigned char i;

    normalize_in_place(line);

    if (starts_with(line, "ping ")) {
        send_buf[0] = 0;
        append_fit(send_buf, sizeof(send_buf), "pong ");
        append_fit(send_buf, sizeof(send_buf), line + 5);
        append_fit(send_buf, sizeof(send_buf), "\r\n");
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

    if (text_eq(cmd, "privmsg")) {
        msg = find_substr(target, " :");
        if (msg == 0) {
            return;
        }
        *msg = 0;
        msg += 2;
        if (!text_eq(target, rirc_rrnet_config_channel)) {
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
    if (ch == '\n') {
        return;
    }
    if (ch == '\r') {
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
    unsigned int n;

    if (!connected) {
        return;
    }
    for (burst = 0u; burst < NET_READ_BURST; ++burst) {
        n = rirc_rrnet_socket_read(socket_id, net_buf, NET_READ_CHUNK);
        if (n == 0u) {
            break;
        }
        for (i = 0u; i < n; ++i) {
            process_rx_byte(net_buf[i]);
        }
    }
}

static void handle_input_submit(void) {
    if (input_len == 0u) {
        return;
    }
    input_buf[input_len] = 0;
    normalize_in_place(input_buf);

    if (starts_with(input_buf, "/quit")) {
        send_raw("quit :readyirc rrnet\r\n");
        disconnect_irc();
        rirc_rrnet_return_to_launcher();
        return;
    }
    if (starts_with(input_buf, "/me ")) {
        send_buf[0] = 0;
        append_fit(send_buf, sizeof(send_buf), "privmsg ");
        append_fit(send_buf, sizeof(send_buf), rirc_rrnet_config_channel);
        append_fit(send_buf, sizeof(send_buf), " :\x01");
        append_fit(send_buf, sizeof(send_buf), "action ");
        append_fit(send_buf, sizeof(send_buf), input_buf + 4);
        append_fit(send_buf, sizeof(send_buf), "\x01\r\n");
        send_raw(send_buf);
        add_action(rirc_rrnet_config_nick, input_buf + 4);
    } else if (starts_with(input_buf, "/msg ")) {
        add_status_line("dm support disabled");
    } else if (connected) {
        send_buf[0] = 0;
        append_fit(send_buf, sizeof(send_buf), "privmsg ");
        append_fit(send_buf, sizeof(send_buf), rirc_rrnet_config_channel);
        append_fit(send_buf, sizeof(send_buf), " :");
        append_fit(send_buf, sizeof(send_buf), input_buf);
        append_fit(send_buf, sizeof(send_buf), "\r\n");
        send_raw(send_buf);
        add_privmsg(rirc_rrnet_config_nick, input_buf);
    } else {
        add_status_line("not connected");
    }

    input_len = 0u;
    input_cursor = 0u;
    input_buf[0] = 0;
    draw_input();
}

static void handle_key(unsigned char key) {
    unsigned char i;
    unsigned char nav_action;
    unsigned int max_scroll;

    nav_action = tui_handle_global_hotkey(key, SHIM_CURRENT_BANK, 1u);
    if (nav_action == TUI_HOTKEY_LAUNCHER) {
        rirc_rrnet_return_to_launcher();
    }
    if (nav_action >= TUI_APP_BANK_MIN && nav_action <= TUI_APP_BANK_MAX) {
        rirc_rrnet_switch_to_app(nav_action);
        return;
    }
    if (nav_action == TUI_HOTKEY_BIND_ONLY) {
        return;
    }

    switch (key) {
        case TUI_KEY_RUNSTOP:
            disconnect_irc();
            running = 0u;
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
                    draw_output();
                }
            }
            break;
        case TUI_KEY_DOWN:
            if (scroll_back > 0u) {
                --scroll_back;
                draw_output();
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

static void resume_save_state(void) {
    memcpy(resume_blob.input, input_buf, sizeof(input_buf));
    resume_blob.input_len = input_len;
    resume_blob.scroll_back = scroll_back;
    (void)resume_save(&resume_blob, sizeof(resume_blob));
}

static void resume_restore_state(void) {
    unsigned int payload_len;

    if (resume_try_load(&resume_blob, sizeof(resume_blob), &payload_len) &&
        payload_len == sizeof(resume_blob) &&
        resume_blob.input_len <= INPUT_MAX) {
        memcpy(input_buf, resume_blob.input, sizeof(input_buf));
        input_len = resume_blob.input_len;
        input_cursor = input_len;
        scroll_back = resume_blob.scroll_back;
    }
}

static void rirc_rrnet_return_to_launcher(void) {
    resume_save_state();
    tui_return_to_launcher();
}

static void rirc_rrnet_switch_to_app(unsigned char bank) {
    resume_save_state();
    tui_switch_to_app(bank);
}

int main(void) {
    unsigned char key;

    reu_mgr_init();
    resume_init_for_app(SHIM_CURRENT_BANK, SHIM_CURRENT_BANK, READYIRC_RRNET_RESUME_APP_SCHEMA);

    running = 1u;
    connected = 0u;
    socket_id = 0u;
    scroll_bank = reu_alloc_owned_bank(1u, "scrl");
    reu_scroll = (unsigned char)(scroll_bank != 0xFFu);
    max_lines = reu_scroll ? REU_MAX_LINES : RAM_MAX_LINES;
    first_line = 0u;
    line_count = 0u;
    input_len = 0u;
    input_cursor = 0u;
    input_buf[0] = 0;
    in_len = 0u;
    dropping_line = 0u;
    scroll_back = 0u;

    resume_restore_state();
    draw_shell();
    if (!reu_scroll) {
        add_status_line("reu scrollback unavailable");
    }
    connect_irc();

    while (running) {
        poll_network();
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
