/*
 * ucitest.c - ReadyOS Ultimate Command Interface tester
 */

#include "../../lib/tui.h"
#include "../../lib/tui_controls.h"
#include "../../lib/tui_output.h"
#include "../../lib/tui_split.h"
#include "ucitest_catalog.h"
#include "ucitest_format.h"
#include "ucitest_uci.h"

#include <string.h>

#define SHIM_CURRENT_BANK (*(volatile unsigned char*)0xC834)

#define TARGET_X 0u
#define TARGET_Y 2u
#define TARGET_W 10u
#define TARGET_H 9u
#define CMD_X 10u
#define CMD_Y 2u
#define CMD_W 30u
#define CMD_H 9u
#define FORM_X 0u
#define FORM_Y 12u
#define FORM_W 40u
#define FORM_H 6u
#define OUT_X 0u
#define OUT_Y 19u
#define OUT_W 40u
#define OUT_H 5u
#define HELP_Y 24u

#define FOCUS_TARGET 0u
#define FOCUS_CMD    1u
#define FOCUS_FORM   2u
#define FOCUS_OUT    3u

#define CMD_BUF_MAX 896u
#define DATA_BUF_MAX 896u
#define STAT_BUF_MAX 256u
#define FORM_MAX_FIELDS 6u
#define FORM_TEXT_CAP 64u
#define OUT_LINES 30u

static unsigned char running;
static unsigned char focus_area;
static unsigned char selected_target;
static unsigned char selected_cmd_rel;
static unsigned char raw_mode;
static unsigned char last_socket;
static unsigned char last_http_header;
static unsigned char last_http_body;
static unsigned char selected_example;

static const char *target_items[8];
static const char *command_items[96];
static const char *example_items[16];
static TuiControlField form_fields[FORM_MAX_FIELDS];
static char form_text[FORM_MAX_FIELDS][FORM_TEXT_CAP];
static TuiControlForm form;
static char output_lines[OUT_LINES][TUI_OUTPUT_LINE_W + 1u];
static TuiOutput output;
static unsigned char cmd_buf[CMD_BUF_MAX];
static unsigned char data_buf[DATA_BUF_MAX];
static unsigned char stat_buf[STAT_BUF_MAX];
static UciTestTransfer xfer;
static char scratch[41];

static void draw_all(void);
static void draw_header(void);
static void draw_help(void);
static void prepare_form(void);
static void run_selected(void);
static void show_example_picker(void);
static void apply_example(unsigned char index);
static unsigned char current_command_index(void);
static const UciTestCommandSpec *current_command(void);
static void handle_global_nav(unsigned char key);
static unsigned char build_command(const UciTestCommandSpec *cmd,
                                   unsigned char target,
                                   unsigned int *len_out);

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

static unsigned char hex_value(unsigned char ch) {
    if (ch >= '0' && ch <= '9') {
        return (unsigned char)(ch - '0');
    }
    if (ch >= 'A' && ch <= 'F') {
        return (unsigned char)(ch - 'A' + 10u);
    }
    if (ch >= 'a' && ch <= 'f') {
        return (unsigned char)(ch - 'a' + 10u);
    }
    return 0xFFu;
}

static unsigned char parse_hex(const char *src,
                               unsigned char *dst,
                               unsigned int cap,
                               unsigned int *len_out) {
    unsigned int len;
    unsigned char value;
    unsigned char digit;
    unsigned char got;

    len = 0u;
    while (*src != 0) {
        while (*src == ' ' || *src == ',' || *src == '$') {
            ++src;
        }
        if (*src == 0) {
            break;
        }
        value = 0u;
        got = 0u;
        while (*src != 0 && got < 2u) {
            digit = hex_value((unsigned char)*src);
            if (digit == 0xFFu) {
                break;
            }
            value = (unsigned char)((value << 4) | digit);
            ++got;
            ++src;
        }
        if (got == 0u) {
            return 0u;
        }
        if (len >= cap) {
            return 0u;
        }
        dst[len] = value;
        ++len;
        while (*src != 0 && *src != ' ' && *src != ',' && *src != '$') {
            ++src;
        }
    }
    *len_out = len;
    return 1u;
}

static unsigned char is_editable_kind(unsigned char kind) {
    return (unsigned char)(kind != UC_FIELD_CONST);
}

static unsigned char control_kind(unsigned char kind) {
    switch (kind) {
        case UC_FIELD_BYTE:
            return TUI_CTRL_BYTE;
        case UC_FIELD_WORD:
            return TUI_CTRL_WORD;
        case UC_FIELD_DWORD:
            return TUI_CTRL_DWORD;
        case UC_FIELD_BOOL:
            return TUI_CTRL_TOGGLE;
        default:
            return TUI_CTRL_TEXT;
    }
}

static void copy_default_text(char *dst, const char *src) {
    unsigned char i;

    if (src == 0) {
        src = "";
    }
    for (i = 0u; i + 1u < FORM_TEXT_CAP && src[i] != 0; ++i) {
        dst[i] = src[i];
    }
    dst[i] = 0;
}

static void fill_item_lists(void) {
    unsigned char i;
    unsigned char kind;
    unsigned char count;
    unsigned char cmd_index;

    for (i = 0u; i < ucitest_target_count; ++i) {
        target_items[i] = ucitest_targets[i].name;
    }
    kind = ucitest_targets[selected_target].kind;
    count = ucitest_command_count_for_kind(kind);
    for (i = 0u; i < count; ++i) {
        cmd_index = ucitest_command_index_for_kind(kind, i);
        command_items[i] = ucitest_commands[cmd_index].name;
    }
}

static unsigned char current_command_index(void) {
    return ucitest_command_index_for_kind(ucitest_targets[selected_target].kind,
                                          selected_cmd_rel);
}

static const UciTestCommandSpec *current_command(void) {
    return &ucitest_commands[current_command_index()];
}

static void prepare_form(void) {
    const UciTestCommandSpec *cmd;
    const UciTestFieldSpec *spec;
    unsigned char i;
    unsigned char out_idx;
    unsigned int def_value;

    cmd = current_command();
    out_idx = 0u;
    for (i = 0u; i < FORM_MAX_FIELDS; ++i) {
        form_text[i][0] = 0;
    }
    for (i = 0u; i < cmd->field_count && out_idx < FORM_MAX_FIELDS; ++i) {
        spec = &cmd->fields[i];
        if (!is_editable_kind(spec->kind)) {
            continue;
        }
        form_fields[out_idx].label = spec->label;
        form_fields[out_idx].kind = control_kind(spec->kind);
        form_fields[out_idx].value_lo = spec->def_lo;
        form_fields[out_idx].value_hi = spec->def_hi;
        form_fields[out_idx].min_value = 0u;
        form_fields[out_idx].max_value = 0u;
        form_fields[out_idx].text = form_text[out_idx];
        form_fields[out_idx].text_cap = FORM_TEXT_CAP;
        form_fields[out_idx].cursor = 0u;
        form_fields[out_idx].choices = spec->choices;
        form_fields[out_idx].choice_count = spec->choice_count;
        if (spec->kind == UC_FIELD_BYTE && spec->choices != 0 &&
            spec->choice_count != 0u) {
            form_fields[out_idx].kind = TUI_CTRL_ENUM;
            form_fields[out_idx].max_value =
                (unsigned int)(spec->choice_count - 1u);
        }
        if (form_fields[out_idx].kind == TUI_CTRL_BYTE) {
            form_fields[out_idx].max_value = 255u;
        }
        if (spec->kind == UC_FIELD_BOOL) {
            form_fields[out_idx].max_value = 1u;
        }
        if (spec->kind == UC_FIELD_TEXT || spec->kind == UC_FIELD_TEXTZ ||
            spec->kind == UC_FIELD_RAW || spec->kind == UC_FIELD_KEY ||
            spec->kind == UC_FIELD_LTEXT) {
            copy_default_text(form_text[out_idx], spec->def_text);
        }
        if (spec->kind == UC_FIELD_BYTE && spec->label != 0) {
            def_value = form_fields[out_idx].value_lo;
            if (strcmp(spec->label, "sock") == 0) {
                def_value = last_socket;
            } else if (strcmp(spec->label, "header") == 0 &&
                       cmd->kind == UC_KIND_HTTP) {
                def_value = last_http_header;
            } else if (strcmp(spec->label, "body") == 0 &&
                       cmd->kind == UC_KIND_HTTP) {
                def_value = last_http_body;
            } else if (strcmp(spec->label, "handle") == 0 &&
                       cmd->kind == UC_KIND_HTTP) {
                if (cmd->cmd == 0x12u || cmd->cmd == 0x13u ||
                    cmd->cmd == 0x14u || cmd->cmd == 0x15u) {
                    def_value = last_http_header;
                } else {
                    def_value = last_http_body;
                }
            }
            form_fields[out_idx].value_lo = def_value;
        }
        ++out_idx;
    }
    tui_form_init(&form, form_fields, out_idx, FORM_X, FORM_Y, FORM_W, FORM_H);
}

static void draw_header(void) {
    unsigned int base;
    unsigned char id;
    unsigned char st;

    tui_clear_line(0u, 0u, 40u, TUI_COLOR_WHITE);
    tui_puts(0u, 0u, "UCI TESTER", TUI_COLOR_YELLOW);
    /* Diagnostic register samples only: they never imply command completion.
     * Executed commands use ucitest_uci_command's full state machine below. */
    base = ucitest_uci_base();
    scratch[0] = 0;
    append_str(scratch, sizeof(scratch), "base ");
    if (base == 0u) {
        append_str(scratch, sizeof(scratch), "none");
    } else {
        append_char(scratch, sizeof(scratch), '$');
        append_hex4(scratch, sizeof(scratch), base);
        id = ucitest_uci_id();
        st = ucitest_uci_status();
        append_str(scratch, sizeof(scratch), " id $");
        append_hex2(scratch, sizeof(scratch), id);
        append_str(scratch, sizeof(scratch), " st $");
        append_hex2(scratch, sizeof(scratch), st);
    }
    tui_puts_n(12u, 0u, scratch, 28u, base ? TUI_COLOR_LIGHTGREEN :
                                           TUI_COLOR_LIGHTRED);
    tui_clear_line(1u, 0u, 40u, TUI_COLOR_GRAY3);
    scratch[0] = 0;
    append_str(scratch, sizeof(scratch), ucitest_flag_text(current_command()->flags));
    append_str(scratch, sizeof(scratch), raw_mode ? " raw" : " decoded");
    tui_puts_n(0u, 1u, scratch, 40u, TUI_COLOR_GRAY3);
}

static void draw_help(void) {
    tui_clear_line(HELP_Y, 0u, 40u, TUI_COLOR_GRAY3);
    tui_puts_n(0u, HELP_Y, "f1tgt f3form f5run f6out f7raw f8ex", 40u,
               TUI_COLOR_GRAY3);
}

static void draw_all(void) {
    unsigned char count;

    fill_item_lists();
    draw_header();
    count = ucitest_command_count_for_kind(ucitest_targets[selected_target].kind);
    tui_split_list(TARGET_X, TARGET_Y, TARGET_W, TARGET_H, "target",
                   target_items, ucitest_target_count, selected_target,
                   focus_area == FOCUS_TARGET ? TUI_COLOR_CYAN : TUI_COLOR_WHITE);
    tui_split_list(CMD_X, CMD_Y, CMD_W, CMD_H, "command",
                   command_items, count, selected_cmd_rel,
                   focus_area == FOCUS_CMD ? TUI_COLOR_CYAN : TUI_COLOR_WHITE);
    tui_hline(0u, 11u, 40u, TUI_COLOR_LIGHTBLUE);
    tui_form_draw(&form);
    tui_hline(0u, 18u, 40u, TUI_COLOR_LIGHTBLUE);
    tui_output_draw(&output, OUT_X, OUT_Y, OUT_W, OUT_H);
    draw_help();
}

static void add_byte(unsigned int *len, unsigned char value) {
    if (*len < CMD_BUF_MAX) {
        cmd_buf[*len] = value;
        *len = (unsigned int)(*len + 1u);
    }
}

static void add_word(unsigned int *len, unsigned int value) {
    add_byte(len, (unsigned char)(value & 0xFFu));
    add_byte(len, (unsigned char)(value >> 8));
}

static void add_dword(unsigned int *len, unsigned int lo, unsigned int hi) {
    add_word(len, lo);
    add_word(len, hi);
}

static void add_text_bytes(unsigned int *len, const char *text,
                           unsigned char add_zero) {
    while (*text != 0 && *len < CMD_BUF_MAX) {
        add_byte(len, (unsigned char)*text);
        ++text;
    }
    if (add_zero) {
        add_byte(len, 0u);
    }
}

static unsigned char build_command(const UciTestCommandSpec *cmd,
                                   unsigned char target,
                                   unsigned int *len_out) {
    unsigned char i;
    unsigned char form_idx;
    unsigned int len;
    unsigned int raw_len;
    const UciTestFieldSpec *spec;
    const TuiControlField *field;

    len = 0u;
    form_idx = 0u;
    if ((cmd->flags & UC_FLAG_SPECIAL) != 0u && cmd->cmd == UC_SPECIAL_RAW) {
        return parse_hex(form_fields[0].text, cmd_buf, CMD_BUF_MAX, len_out);
    }
    add_byte(&len, target);
    add_byte(&len, cmd->cmd);
    for (i = 0u; i < cmd->field_count; ++i) {
        spec = &cmd->fields[i];
        field = 0;
        if (is_editable_kind(spec->kind)) {
            field = &form_fields[form_idx];
            ++form_idx;
        }
        switch (spec->kind) {
            case UC_FIELD_CONST:
                add_byte(&len, (unsigned char)(spec->def_lo & 0xFFu));
                break;
            case UC_FIELD_BYTE:
            case UC_FIELD_BOOL:
                add_byte(&len, (unsigned char)(field->value_lo & 0xFFu));
                break;
            case UC_FIELD_WORD:
                add_word(&len, field->value_lo);
                break;
            case UC_FIELD_DWORD:
                add_dword(&len, field->value_lo, field->value_hi);
                break;
            case UC_FIELD_TEXT:
                add_text_bytes(&len, field->text, 0u);
                break;
            case UC_FIELD_TEXTZ:
                add_text_bytes(&len, field->text, 1u);
                break;
            case UC_FIELD_KEY:
            case UC_FIELD_LTEXT:
                add_byte(&len, (unsigned char)strlen(field->text));
                add_text_bytes(&len, field->text, 0u);
                break;
            case UC_FIELD_RAW:
                if (!parse_hex(field->text, cmd_buf + len,
                               (unsigned int)(CMD_BUF_MAX - len), &raw_len)) {
                    return 0u;
                }
                len = (unsigned int)(len + raw_len);
                break;
            default:
                break;
        }
    }
    *len_out = len;
    return 1u;
}

static void add_special_result(const UciTestCommandSpec *cmd) {
    unsigned int base;
    unsigned char value;

    tui_output_clear(&output);
    if (cmd->cmd == UC_SPECIAL_DETECT) {
        base = ucitest_uci_base();
        if (base == 0u) {
            tui_output_add(&output, "uci not detected");
        } else {
            scratch[0] = 0;
            append_str(scratch, sizeof(scratch), "uci base $");
            append_hex4(scratch, sizeof(scratch), base);
            tui_output_add(&output, scratch);
        }
    } else if (cmd->cmd == UC_SPECIAL_ID) {
        value = ucitest_uci_id();
        scratch[0] = 0;
        append_str(scratch, sizeof(scratch), "id $");
        append_hex2(scratch, sizeof(scratch), value);
        tui_output_add(&output, scratch);
    } else if (cmd->cmd == UC_SPECIAL_STATUS) {
        value = ucitest_uci_status();
        scratch[0] = 0;
        append_str(scratch, sizeof(scratch), "status $");
        append_hex2(scratch, sizeof(scratch), value);
        tui_output_add(&output, scratch);
    } else if (cmd->cmd == UC_SPECIAL_ABORT) {
        /* Explicit recovery control. This call requests asynchronous ABORT
         * once, services pending queues, and waits for fully quiet IDLE. */
        ucitest_uci_abort();
        tui_output_add(&output, "abort requested and interface serviced");
    } else if (cmd->cmd == UC_SPECIAL_CLEAR) {
        /* Manual recovery-only control; command transactions never erase an
         * ERROR raised while their PUSH is active. */
        ucitest_uci_clear_error();
        tui_output_add(&output, "error flag cleared");
    } else if (cmd->cmd == UC_SPECIAL_NORMS) {
        tui_output_add(&output, "uci protocol norms");
        tui_output_add(&output, "1 idle + pending bits clear first");
        tui_output_add(&output, "2 write bytes, then push exactly once");
        tui_output_add(&output, "3 push is async; idle is not done");
        tui_output_add(&output, "4 wait for data last or data more");
        tui_output_add(&output, "5 drain data_av and stat_av queues");
        tui_output_add(&output, "6 accept only after both are clear");
        tui_output_add(&output, "7 accept async; more must go busy");
        tui_output_add(&output, "8 last: accept, then wait for idle");
        tui_output_add(&output, "9 abort is async; wait for idle too");
        tui_output_add(&output, "10 abort_p pending: do not reissue");
        tui_output_add(&output, "bounds cover high cpu; never pace uci");
        tui_output_add(&output, "bound drains above queue capacities");
        tui_output_add(&output, "queues: command/data 896, status 256");
        tui_output_add(&output, "verify on real hardware at high speed");
    }
}

static void update_handles(const UciTestCommandSpec *cmd) {
    if (xfer.data_len == 0u) {
        return;
    }
    if (cmd->kind == UC_KIND_NETWORK &&
        (cmd->cmd == 0x07u || cmd->cmd == 0x08u)) {
        last_socket = xfer.data[0];
    }
    if (cmd->kind == UC_KIND_HTTP && cmd->cmd == 0x11u) {
        last_http_header = xfer.data[0];
    }
    if (cmd->kind == UC_KIND_HTTP && cmd->cmd == 0x21u) {
        last_http_body = xfer.data[0];
    }
    if (cmd->kind == UC_KIND_HTTP && cmd->cmd == 0x31u &&
        xfer.data_len >= 2u) {
        last_http_header = xfer.data[0];
        last_http_body = xfer.data[1];
    }
}

static void run_selected(void) {
    const UciTestCommandSpec *cmd;
    unsigned int cmd_len;
    unsigned char target;

    cmd = current_command();
    if ((cmd->flags & UC_FLAG_SPECIAL) != 0u && cmd->cmd != UC_SPECIAL_RAW) {
        add_special_result(cmd);
        draw_all();
        return;
    }

    target = ucitest_targets[selected_target].target;
    if (!build_command(cmd, target, &cmd_len)) {
        tui_output_clear(&output);
        tui_output_add(&output, "bad command bytes/form");
        draw_all();
        return;
    }

    xfer.data = data_buf;
    xfer.data_cap = DATA_BUF_MAX;
    xfer.stat = stat_buf;
    xfer.stat_cap = STAT_BUF_MAX;
    tui_output_clear(&output);
    /* One complete state-driven transaction; no UI delay is protocol pacing. */
    if (!ucitest_uci_command(cmd_buf, cmd_len, &xfer)) {
        tui_output_add(&output, "command failed or timed out");
        if ((xfer.flags & UCITEST_UCI_TIMEOUT) != 0u) {
            tui_output_add(&output, "timeout flag set");
        }
        if ((xfer.flags & UCITEST_UCI_ERROR) != 0u) {
            tui_output_add(&output, "uci transport error flag set");
        }
        draw_all();
        return;
    }
    update_handles(cmd);
    ucitest_format_response(&output, cmd, &xfer, raw_mode);
    prepare_form();
    draw_all();
}

static void apply_example(unsigned char index) {
    const UciTestExampleSpec *example;
    unsigned char i;

    if (index >= ucitest_example_count) {
        return;
    }
    example = &ucitest_examples[index];
    for (i = 0u; i < ucitest_target_count; ++i) {
        if (ucitest_targets[i].kind == example->kind) {
            selected_target = i;
            break;
        }
    }
    selected_cmd_rel = ucitest_command_rel_for_kind_cmd(example->kind,
                                                        example->cmd);
    prepare_form();
    if ((example->value_mask & 0x01u) != 0u && form.count > 0u) {
        form_fields[0].value_lo = example->value0;
    }
    if ((example->value_mask & 0x02u) != 0u && form.count > 1u) {
        form_fields[1].value_lo = example->value1;
    }
    if (example->text0 != 0 && form.count > 0u &&
        form_fields[0].text != 0) {
        copy_default_text(form_fields[0].text, example->text0);
    }
    if (example->text1 != 0 && form.count > 1u &&
        form_fields[1].text != 0) {
        copy_default_text(form_fields[1].text, example->text1);
    }
    focus_area = form.count == 0u ? FOCUS_CMD : FOCUS_FORM;
    tui_output_clear(&output);
    scratch[0] = 0;
    append_str(scratch, sizeof(scratch), "example: ");
    append_str(scratch, sizeof(scratch), example->name);
    tui_output_add(&output, scratch);
    tui_output_add(&output, example->hint1);
    tui_output_add(&output, "review fields, then press f5");
}

static void show_example_picker(void) {
    TuiRect win;
    unsigned char key;
    const UciTestExampleSpec *example;

    win.x = 1u;
    win.y = 1u;
    win.w = 38u;
    win.h = 23u;
    for (;;) {
        tui_window_title(&win, "PREFILL EXAMPLE", TUI_COLOR_LIGHTBLUE,
                         TUI_COLOR_YELLOW);
        tui_split_list(3u, 3u, 34u, 11u, "select a safe starting point",
                       example_items, ucitest_example_count, selected_example,
                       TUI_COLOR_WHITE);
        example = &ucitest_examples[selected_example];
        tui_clear_line(15u, 3u, 34u, TUI_COLOR_GRAY3);
        tui_clear_line(16u, 3u, 34u, TUI_COLOR_GRAY3);
        tui_puts_n(3u, 15u, example->hint1, 34u, TUI_COLOR_WHITE);
        tui_puts_n(3u, 16u, example->hint2, 34u, TUI_COLOR_GRAY3);
        tui_puts_n(3u, 20u, "return:prefill  f8/stop:cancel", 34u,
                   TUI_COLOR_CYAN);
        key = tui_getkey();
        if (key == TUI_KEY_UP && selected_example > 0u) {
            --selected_example;
        } else if (key == TUI_KEY_DOWN &&
                   selected_example + 1u < ucitest_example_count) {
            ++selected_example;
        } else if (key == TUI_KEY_RETURN) {
            apply_example(selected_example);
            draw_all();
            return;
        } else if (key == TUI_KEY_F8 || key == TUI_KEY_RUNSTOP ||
                   key == TUI_KEY_LARROW) {
            draw_all();
            return;
        }
    }
}

static void select_target_delta(signed char delta) {
    if (delta < 0) {
        if (selected_target > 0u) {
            --selected_target;
            selected_cmd_rel = 0u;
            prepare_form();
        }
    } else if (delta > 0) {
        if (selected_target + 1u < ucitest_target_count) {
            ++selected_target;
            selected_cmd_rel = 0u;
            prepare_form();
        }
    }
}

static void select_command_delta(signed char delta) {
    unsigned char count;

    count = ucitest_command_count_for_kind(ucitest_targets[selected_target].kind);
    if (delta < 0) {
        if (selected_cmd_rel > 0u) {
            --selected_cmd_rel;
            prepare_form();
        }
    } else if (delta > 0) {
        if (selected_cmd_rel + 1u < count) {
            ++selected_cmd_rel;
            prepare_form();
        }
    }
}

static void handle_global_nav(unsigned char key) {
    unsigned char nav;

    nav = tui_handle_global_hotkey(key, SHIM_CURRENT_BANK, 1u);
    if (nav == TUI_HOTKEY_LAUNCHER) {
        tui_return_to_launcher();
    }
    if (nav != TUI_HOTKEY_NONE && nav != TUI_HOTKEY_BIND_ONLY &&
        nav != TUI_HOTKEY_LAUNCHER) {
        tui_switch_to_app(nav);
    }
}

int main(void) {
    unsigned char key;
    unsigned char i;

    running = 1u;
    focus_area = FOCUS_CMD;
    selected_target = 0u;
    selected_cmd_rel = 0u;
    raw_mode = 0u;
    last_socket = 0xFFu;
    last_http_header = 0xFFu;
    last_http_body = 0xFFu;
    selected_example = 0u;
    for (i = 0u; i < ucitest_example_count; ++i) {
        example_items[i] = ucitest_examples[i].name;
    }

    tui_init();
    tui_clear(TUI_THEME_BG);
    tui_output_init(&output, output_lines, OUT_LINES);
    tui_output_add(&output, "select a command and press f5");
    prepare_form();
    draw_all();

    while (running) {
        key = tui_getkey();
        handle_global_nav(key);

        if (key == TUI_KEY_F1) {
            focus_area = FOCUS_TARGET;
        } else if (key == TUI_KEY_F3) {
            focus_area = FOCUS_FORM;
        } else if (key == TUI_KEY_F5 || key == TUI_KEY_RETURN) {
            run_selected();
            continue;
        } else if (key == TUI_KEY_F6) {
            focus_area = FOCUS_OUT;
        } else if (key == TUI_KEY_F7) {
            raw_mode = (unsigned char)(raw_mode ? 0u : 1u);
        } else if (key == TUI_KEY_F8) {
            show_example_picker();
            continue;
        } else if (key == TUI_KEY_LARROW || key == TUI_KEY_RUNSTOP) {
            tui_return_to_launcher();
        } else if (focus_area == FOCUS_TARGET) {
            if (key == TUI_KEY_UP) {
                select_target_delta(-1);
            } else if (key == TUI_KEY_DOWN) {
                select_target_delta(1);
            } else if (key == TUI_KEY_RIGHT) {
                focus_area = FOCUS_CMD;
            }
        } else if (focus_area == FOCUS_CMD) {
            if (key == TUI_KEY_UP) {
                select_command_delta(-1);
            } else if (key == TUI_KEY_DOWN) {
                select_command_delta(1);
            } else if (key == TUI_KEY_LEFT) {
                focus_area = FOCUS_TARGET;
            } else if (key == TUI_KEY_RIGHT) {
                focus_area = FOCUS_FORM;
            }
        } else if (focus_area == FOCUS_FORM) {
            if (key == TUI_KEY_LEFT && form.count == 0u) {
                focus_area = FOCUS_CMD;
            } else {
                (void)tui_form_key(&form, key);
            }
        } else if (focus_area == FOCUS_OUT) {
            if (key == TUI_KEY_UP) {
                tui_output_scroll(&output, -1);
            } else if (key == TUI_KEY_DOWN) {
                tui_output_scroll(&output, 1);
            }
        }
        draw_all();
    }
    return 0;
}
