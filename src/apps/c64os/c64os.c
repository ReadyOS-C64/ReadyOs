/* Experimental bridge for the fixed 16MB REU + IDE64 v4.1 environment. */
#include <stdio.h>
#include "../../lib/tui.h"
#include "../../lib/reu_control_bank.h"

extern void bridge_enter(void);
extern unsigned char bridge_target, bridge_proof, bridge_failprobe;
static unsigned char header[64];
static unsigned char record[8];
static const unsigned char signature[5] = {0x52,0x42,0x47,0x33,3};
static unsigned char shim[1024];
static unsigned int cycles;
static unsigned int i;
static unsigned char key;
static unsigned char action;
static unsigned char nav_action;
static TuiMenu menu;
static const TuiRect title = {0, 0, 40, 3};
static const char *items[] = {
    "Start / resume C64 OS (c)",
    "Test RAM save / restore (s)",
    "Test boot failure recovery (x)"
};
static char status[40];

static void show_status(const char *text, unsigned char color) {
    tui_puts_n(0, 24, text, 40, color);
}

static void draw(void) {
    tui_clear(TUI_THEME_BG);
    tui_window_title(&title, "C64 OS BRIDGE", TUI_THEME_BORDER, TUI_THEME_TITLE);
    tui_puts(1, 1, "READYOS / C64 OS", TUI_COLOR_CYAN);
    tui_menu_draw(&menu);
    tui_puts(1, 10, "Switch back using the ReadyOS utility", TUI_COLOR_GRAY3);
    tui_puts(1, 11, "in the C64 OS menu.", TUI_COLOR_GRAY3);
    tui_puts(1, 15, "REU 0-31: C64 OS", TUI_COLOR_LIGHTBLUE);
    tui_puts(1, 16, "REU 32-38: bridge snapshots / reserve", TUI_COLOR_LIGHTBLUE);
    tui_puts(1, 17, "REU 39+: ReadyOS", TUI_COLOR_LIGHTBLUE);
    tui_puts(1, 22, "UP/DOWN:SELECT  RETURN:OPEN", TUI_COLOR_GRAY3);
    tui_puts(1, 23, "F2:NEXT APP  F4:PREV APP  CTRL+B:HOME", TUI_COLOR_GRAY3);
    show_status("Choose an action", TUI_COLOR_CYAN);
}

static unsigned char supported(void) {
    if (*SHIM_READYOS_BANK != 39u) return 0;
    /* v4.1, STD cartridge mapping: OPEN would hide the app itself. */
    if ((*(volatile unsigned char*)0xde32 & 0xe3u) != 0x23u) return 0;
    readyos_bank_read(REUCB_HEADER_OFF, header, sizeof(header));
    return header[0] == REUCB_MAGIC0 && header[1] == REUCB_MAGIC1 &&
        header[2] == REUCB_MAGIC2 && header[3] == REUCB_MAGIC3 &&
        header[REUCB_HEADER_CONTROL_BANK] == 39u &&
        (header[REUCB_HEADER_FLAGS] & REUCB_HEADER_FLAG_PHYS_SIZE) &&
        header[REUCB_HEADER_PHYS_BANKS] == 0u;
}

int main(void) {
    tui_init();
    tui_menu_init(&menu, 1, 5, 38, 3, items, 3);
    draw();
    if (!supported()) {
        show_status("requires skip 39, 16mb reu, ide64 v4.1", TUI_COLOR_LIGHTRED);
        tui_getkey();
        tui_return_to_launcher();
    }
    readyos_bank_read(REUCB_RESERVED_OFF, record, sizeof(record));
    for (i = 0; i < sizeof(signature); ++i) {
        if (record[i] != signature[i]) break;
    }
    if (i != sizeof(signature)) {
        for (i = 0; i < sizeof(record); ++i) record[i] = 0;
        for (i = 0; i < sizeof(signature); ++i) record[i] = signature[i];
        readyos_bank_write(REUCB_RESERVED_OFF, record, sizeof(record));
    }
    for (;;) {
        key = tui_getkey();
        nav_action = tui_handle_global_hotkey(key, *(unsigned char*)0xc834, 1);
        if (nav_action == TUI_HOTKEY_LAUNCHER || key == TUI_KEY_F1) {
            tui_return_to_launcher();
        }
        if (nav_action >= TUI_APP_BANK_MIN && nav_action <= TUI_APP_BANK_MAX) {
            tui_switch_to_app(nav_action);
            continue;
        }
        if (nav_action == TUI_HOTKEY_BIND_ONLY) continue;
        action = tui_menu_input(&menu, key);
        if (key == 'c') action = 0;
        if (key == 's') action = 1;
        if (key == 'x') action = 2;
        if (action == 255) {
            tui_menu_draw(&menu);
            continue;
        }
        menu.selected = action;
        tui_menu_draw(&menu);
        bridge_proof = action == 1;
        bridge_failprobe = action == 2;
        bridge_target = 32;
        if (!bridge_proof) {
            readyos_bank_read(REUCB_RESERVED_OFF, record, sizeof(record));
            bridge_target = record[6] == 1 ? 34 : 255;
            if (bridge_failprobe) bridge_target = 255;
        }
        record[7] = 0;
        readyos_bank_write(REUCB_RESERVED_OFF + 7, &record[7], 1);
        /* The REU API legitimately updates shim DMA parameters. Compare the
         * boundary state after those calls, immediately before suspension. */
        for (i = 0; i < sizeof(shim); ++i)
            shim[i] = ((volatile unsigned char*)0xc600)[i];
        bridge_enter();
        for (i = 0; i < sizeof(shim); ++i) {
            if (shim[i] != ((volatile unsigned char*)0xc600)[i]) {
                show_status("Shim mismatch - stop", TUI_COLOR_LIGHTRED);
                for (;;) {}
            }
        }
        readyos_bank_read(REUCB_RESERVED_OFF + 7, &record[7], 1);
        key = record[7];
        if (key) {
            sprintf(status, "Cold boot failed: %u; ReadyOS restored", key);
            show_status(status, TUI_COLOR_LIGHTRED);
            continue;
        }
        ++cycles;
        sprintf(status, "Restored ZP/stack/shim: %u", cycles);
        show_status(status, TUI_COLOR_LIGHTGREEN);
    }
    return 0;
}
