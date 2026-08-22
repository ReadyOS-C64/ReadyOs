#include "setup_backend.h"
#include "setup_config.h"
#include "setup_uci.h"
#include "tui.h"

#include <cbm.h>
#include <conio.h>
#include <string.h>

#define REU_COMMAND  (*(volatile unsigned char*)0xDF01)
#define REU_C64_LO   (*(volatile unsigned char*)0xDF02)
#define REU_C64_HI   (*(volatile unsigned char*)0xDF03)
#define REU_REU_LO   (*(volatile unsigned char*)0xDF04)
#define REU_REU_HI   (*(volatile unsigned char*)0xDF05)
#define REU_REU_BANK (*(volatile unsigned char*)0xDF06)
#define REU_LEN_LO   (*(volatile unsigned char*)0xDF07)
#define REU_LEN_HI   (*(volatile unsigned char*)0xDF08)
#define REU_STASH    0x90u
#define REU_FETCH    0x91u
#define REU_TEST_OFF 0xFFF0u

static unsigned char config_data[SETUP_CONFIG_CAP];
static unsigned int config_length;
static char configured_path[SETUP_PATH_CAP];
static char browser_path[SETUP_PATH_CAP];
static SetupPage page;
static const char *menu_items[SETUP_PAGE_ROWS];
static char menu_labels[SETUP_PAGE_ROWS][38];
static char display_path[SETUP_PATH_CAP];
static TuiMenu menu;
static unsigned char reu_byte;
static unsigned int reu_kb;
static unsigned char prereq_ready;
static unsigned char configured_valid;
static unsigned char uci_ready;
static char notice[40];
static unsigned char notice_color;

static void reu_transfer(unsigned char command_code, unsigned char bank) {
    unsigned int address = (unsigned int)&reu_byte;
    REU_C64_LO = (unsigned char)address;
    REU_C64_HI = (unsigned char)(address >> 8u);
    REU_REU_LO = (unsigned char)REU_TEST_OFF;
    REU_REU_HI = (unsigned char)(REU_TEST_OFF >> 8u);
    REU_REU_BANK = bank;
    REU_LEN_LO = 1u;
    REU_LEN_HI = 0u;
    REU_COMMAND = command_code;
}

static unsigned char reu_fetch(unsigned char bank) {
    reu_byte = 0u;
    reu_transfer(REU_FETCH, bank);
    return reu_byte;
}

static void reu_stash(unsigned char bank, unsigned char value) {
    reu_byte = value;
    reu_transfer(REU_STASH, bank);
}

static unsigned int detect_reu_kb(void) {
    static const unsigned char banks[] = {1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u};
    unsigned char original0;
    unsigned char original;
    unsigned char got;
    unsigned char i;
    original0 = reu_fetch(0u);
    reu_stash(0u, 0xA5u);
    got = reu_fetch(0u);
    reu_stash(0u, original0);
    if (got != 0xA5u) return 0u;
    original0 = reu_fetch(0u);
    reu_stash(0u, 0x5Au);
    for (i = 0u; i < sizeof(banks); ++i) {
        original = reu_fetch(banks[i]);
        reu_stash(banks[i], 0xC3u);
        got = reu_fetch(0u);
        reu_stash(banks[i], original);
        if (got == 0xC3u) {
            reu_stash(0u, original0);
            return (unsigned int)banks[i] * 64u;
        }
    }
    reu_stash(0u, original0);
    return 16384u;
}

static unsigned char read_local_config(void) {
    int got;
    config_length = 0u;
    if (cbm_open(2u, 8u, 2u, "apps.cfg,s,r") != 0u) return 0u;
    while (config_length < SETUP_CONFIG_CAP) {
        got = cbm_read(2u, config_data + config_length,
                       (unsigned int)(SETUP_CONFIG_CAP - config_length));
        if (got < 0) { cbm_close(2u); cbm_k_clrch(); return 0u; }
        config_length = (unsigned int)(config_length + (unsigned int)got);
        if (got == 0) break;
    }
    cbm_close(2u);
    cbm_k_clrch();
    return setup_config_find_path(config_data, config_length,
                                  configured_path, sizeof(configured_path));
}

static void set_notice(const char *text, unsigned char color) {
    strncpy(notice, text, sizeof(notice) - 1u);
    notice[sizeof(notice) - 1u] = 0;
    notice_color = color;
}

static unsigned char host_char_for_tui(unsigned char value) {
    /* Ultimate DOS directory streams can contain host ASCII even though
     * cc65 string literals use the target PETSCII character set.  Normalize
     * only the display copy; commands must retain the exact host bytes. */
    if (value >= 0x41u && value <= 0x5Au) return (unsigned char)(value + 0x80u);
    if (value >= 0x61u && value <= 0x7Au) return (unsigned char)(value + 0x60u);
    if (value == 0x5Fu) return 0xA4u;
    return value;
}

static void copy_host_text_for_tui(char *out, unsigned int cap,
                                   const char *text) {
    unsigned int n = 0u;
    if (cap == 0u) return;
    while (text[n] != 0 && n + 1u < cap) {
        out[n] = (char)host_char_for_tui((unsigned char)text[n]);
        ++n;
    }
    out[n] = 0;
}

static void draw_chrome(void) {
    /* Match the shallow, borderless-body header used by ReadyOS apps.  SETUP
     * needs one extra interior row so its prerequisite labels and values can
     * remain independently scannable. */
    TuiRect header = {0u, 0u, 40u, 4u};
    tui_clear(TUI_THEME_BG);
    tui_window_title(&header, "READYOS ULTIMATE SETUP",
                     TUI_COLOR_LIGHTBLUE, TUI_COLOR_YELLOW);
    tui_puts(2u, 1u, "REU", TUI_COLOR_GRAY3);
    tui_puts(14u, 1u, "UCI", TUI_COLOR_GRAY3);
    tui_puts(25u, 1u, "ULT.DOS", TUI_COLOR_GRAY3);
    if (reu_kb != 0u) {
        tui_puts(2u, 2u, "OK", TUI_COLOR_LIGHTGREEN);
        tui_print_uint(5u, 2u, reu_kb, TUI_COLOR_WHITE);
        tui_puts(10u, 2u, "K", TUI_COLOR_WHITE);
    } else tui_puts(2u, 2u, "MISSING", TUI_COLOR_LIGHTRED);
    tui_puts(14u, 2u, uci_ready ? "OK" : "MISSING",
             uci_ready ? TUI_COLOR_LIGHTGREEN : TUI_COLOR_LIGHTRED);
    tui_puts(25u, 2u, prereq_ready ? "OK" : "OFF/ERROR",
             prereq_ready ? TUI_COLOR_LIGHTGREEN : TUI_COLOR_LIGHTRED);
    tui_puts(1u, 4u, "PATH:", TUI_COLOR_GRAY3);
    copy_host_text_for_tui(display_path, sizeof(display_path), browser_path);
    tui_puts_n(7u, 4u, display_path, 31u, TUI_COLOR_WHITE);
    /* Leave a quiet row between the path and the browser contents. */
    tui_clear_line(5u, 0u, 40u, TUI_THEME_BG);
    tui_clear_line(24u, 0u, 40u, notice_color);
    tui_puts_n(1u, 24u, notice, 38u, notice_color);
}

static void draw_help_failure(void) {
    tui_puts(2u, 7u, "READYOS NEEDS REU, UCI, AND", TUI_COLOR_WHITE);
    tui_puts(2u, 8u, "ULTIMATE DOS TO BE WORKING.", TUI_COLOR_WHITE);
    tui_puts(2u, 10u, "IN THE ULTIMATE MENU:", TUI_COLOR_YELLOW);
    tui_puts(3u, 12u, "- ENABLE REU (16MB RECOMMENDED)", TUI_COLOR_WHITE);
    tui_puts(3u, 13u, "- ENABLE COMMAND INTERFACE/UCI", TUI_COLOR_WHITE);
    tui_puts(3u, 14u, "- ENABLE ULTIMATE DOS", TUI_COLOR_WHITE);
    tui_puts(2u, 16u, "THEN RESET AND RUN SETUP AGAIN.", TUI_COLOR_WHITE);
    tui_puts(2u, 19u, "F5 RETEST   RUN/STOP EXIT", TUI_COLOR_CYAN);
}

static void build_labels(void) {
    unsigned char i;
    unsigned char n;
    for (i = 0u; i < page.count; ++i) {
        menu_labels[i][0] = page.entries[i].directory ? '/' : ' ';
        menu_labels[i][1] = ' ';
        n = strlen(page.entries[i].name);
        if (n > 34u) n = 34u;
        copy_host_text_for_tui(menu_labels[i] + 2u,
                               sizeof(menu_labels[i]) - 2u,
                               page.entries[i].name);
        menu_labels[i][n + 2u] = 0;
        menu_items[i] = menu_labels[i];
    }
    tui_menu_init(&menu, 1u, 6u, 38u, SETUP_PAGE_ROWS,
                  menu_items, page.count);
    menu.item_color = TUI_COLOR_WHITE;
    menu.sel_color = TUI_COLOR_CYAN;
}

static unsigned char load_page(unsigned char number) {
    if (!setup_backend_list(browser_path, number, &page)) {
        set_notice(setup_backend_status(), TUI_COLOR_LIGHTRED);
        return 0u;
    }
    build_labels();
    if (page.count == 0u && number != 0u) return load_page(number - 1u);
    return 1u;
}

static void draw_browser(void) {
    unsigned char y;
    draw_chrome();
    tui_menu_draw(&menu);
    for (y = (unsigned char)(6u + page.count); y < 20u; ++y)
        tui_clear_line(y, 1u, 38u, TUI_COLOR_WHITE);
    tui_puts(2u, 21u, "PAGE", TUI_COLOR_GRAY3);
    tui_print_uint(7u, 21u, (unsigned int)page.page + 1u, TUI_COLOR_GRAY3);
    tui_puts(12u, 21u, "F1/F3 PAGE", TUI_COLOR_GRAY3);
    tui_puts(26u, 21u, "LEFT=UP", TUI_COLOR_GRAY3);
    tui_puts(2u, 22u, "RETURN OPEN  F5 RETEST  F7 PATH", TUI_COLOR_CYAN);
}

static unsigned char path_is_absolute_d81(const char *path) {
    unsigned int length = strlen(path);
    const char *suffix;
    if (length < 5u || path[0] != '/') return 0u;
    suffix = path + length - 4u;
    /* Ultimate DOS paths are ASCII. cc65 translates lowercase character
     * literals to PETSCII, so spell the ASCII lowercase byte explicitly. */
    return (unsigned char)((unsigned char)suffix[0] == 0x2Eu &&
           ((unsigned char)suffix[1] == 0x64u ||
            (unsigned char)suffix[1] == 0x44u) &&
           (unsigned char)suffix[2] == 0x38u &&
           (unsigned char)suffix[3] == 0x31u);
}

static unsigned char prompt_absolute_path(char *out) {
    unsigned int length = 0u;
    unsigned int shown;
    unsigned char key;
    out[0] = 0;
    for (;;) {
        tui_clear_line(23u, 1u, 38u, TUI_COLOR_WHITE);
        tui_puts(2u, 23u, "PATH>", TUI_COLOR_YELLOW);
        shown = length > 31u ? length - 31u : 0u;
        copy_host_text_for_tui(display_path, sizeof(display_path), out + shown);
        tui_puts_n(8u, 23u, display_path, 31u, TUI_COLOR_WHITE);
        key = cgetc();
        if (key == TUI_KEY_RUNSTOP) return 0u;
        if (key == TUI_KEY_RETURN) {
            if (path_is_absolute_d81(out)) return 1u;
            set_notice("absolute .d81 path required", TUI_COLOR_LIGHTRED);
            return 0u;
        }
        if (key == TUI_KEY_DEL) {
            if (length != 0u) out[--length] = 0;
        } else if (key >= 32u && key < 127u && length + 1u < SETUP_PATH_CAP) {
            out[length++] = (char)key;
            out[length] = 0;
        }
    }
}

static unsigned char append_component(const char *name) {
    unsigned int have = strlen(browser_path);
    unsigned int add = strlen(name);
    if (have + add + 2u >= sizeof(browser_path)) {
        set_notice("path too long", TUI_COLOR_LIGHTRED); return 0u;
    }
    if (have > 1u) browser_path[have++] = '/';
    strcpy(browser_path + have, name);
    return 1u;
}

static void parent_path(void) {
    char *slash;
    if (strcmp(browser_path, "/") == 0) return;
    slash = strrchr(browser_path, '/');
    if (slash == browser_path) browser_path[1] = 0;
    else if (slash != 0) *slash = 0;
}

static unsigned char selected_full_path(char *out, const char *name) {
    unsigned int have = strlen(browser_path);
    unsigned int add = strlen(name);
    if (have + add + 2u >= SETUP_PATH_CAP) return 0u;
    strcpy(out, browser_path);
    if (have > 1u) strcat(out, "/");
    strcat(out, name);
    return 1u;
}

static void select_image(const char *name) {
    static char full_path[SETUP_PATH_CAP];
    unsigned char key;
    if (!selected_full_path(full_path, name)) {
        set_notice("selected path too long", TUI_COLOR_LIGHTRED); return;
    }
    set_notice("write this d81 apps.cfg? Y/N", TUI_COLOR_YELLOW);
    draw_browser();
    key = cgetc();
    if (key != 'y' && key != 'Y') {
        set_notice("selection cancelled", TUI_COLOR_GRAY3); return;
    }
    set_notice("mounting and staging apps.cfg...", TUI_COLOR_YELLOW);
    draw_browser();
    if (!setup_backend_configure_image(full_path, config_data, &config_length)) {
        set_notice(setup_backend_status(), TUI_COLOR_LIGHTRED); return;
    }
    strcpy(configured_path, full_path);
    configured_valid = 1u;
    set_notice("CONFIGURED - READYOS DMA IS READY", TUI_COLOR_LIGHTGREEN);
}

static void test_prerequisites(void) {
    reu_kb = detect_reu_kb();
    prereq_ready = 0u;
    configured_valid = 0u;
    uci_ready = 0u;
    configured_path[0] = 0;
    if (reu_kb == 0u) {
        set_notice("REU missing", TUI_COLOR_LIGHTRED); return;
    }
    uci_ready = setup_uci_detect();
    if (!uci_ready) {
        set_notice("UCI missing", TUI_COLOR_LIGHTRED); return;
    }
    if (!setup_backend_identify()) {
        set_notice(setup_backend_status(), TUI_COLOR_LIGHTRED); return;
    }
    prereq_ready = 1u;
    if (!read_local_config()) {
        set_notice("mount Ultimate D81 on drive 8", TUI_COLOR_LIGHTRED);
        return;
    }
    if (configured_path[0] != 0u &&
        setup_backend_validate_image(configured_path,
                                     config_data, &config_length)) {
        configured_valid = 1u;
        set_notice("CONFIGURED - PATH VALID", TUI_COLOR_LIGHTGREEN);
    } else if (configured_path[0] != 0u) {
        set_notice("saved path invalid - choose d81", TUI_COLOR_LIGHTRED);
    } else set_notice("choose the ReadyOS d81", TUI_COLOR_YELLOW);
}

int main(void) {
    unsigned char key;
    unsigned char chosen;
    strcpy(browser_path, "/");
    tui_init();
    test_prerequisites();
    if (prereq_ready) (void)load_page(0u);
    for (;;) {
        draw_chrome();
        if (!prereq_ready) draw_help_failure();
        else draw_browser();
        key = cgetc();
        if (key == TUI_KEY_RUNSTOP) break;
        if (key == TUI_KEY_F5) {
            test_prerequisites();
            if (prereq_ready) (void)load_page(0u);
            continue;
        }
        if (!prereq_ready) continue;
        if (key == TUI_KEY_F7) {
            if (configured_path[0] == 0u &&
                !prompt_absolute_path(configured_path)) continue;
            if (setup_backend_configure_image(configured_path,
                                              config_data,
                                              &config_length)) {
                configured_valid = 1u;
                set_notice("CONFIGURED - READYOS DMA IS READY",
                           TUI_COLOR_LIGHTGREEN);
            } else {
                set_notice(setup_backend_status(), TUI_COLOR_LIGHTRED);
                configured_path[0] = 0;
            }
            continue;
        }
        if (key == TUI_KEY_LEFT || key == TUI_KEY_DEL) {
            parent_path();
            (void)load_page(0u);
        } else if (key == TUI_KEY_F1 && page.page != 0u) {
            (void)load_page((unsigned char)(page.page - 1u));
        } else if (key == TUI_KEY_F3 && page.more) {
            (void)load_page((unsigned char)(page.page + 1u));
        } else if (page.count != 0u) {
            chosen = tui_menu_input(&menu, key);
            if (chosen != 255u && chosen < page.count) {
                if (page.entries[chosen].directory) {
                    if (append_component(page.entries[chosen].name))
                        (void)load_page(0u);
                } else select_image(page.entries[chosen].name);
            }
        }
    }
    clrscr();
    return configured_valid ? 0 : 1;
}
