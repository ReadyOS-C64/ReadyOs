/* Experimental bridge for a fixed 16MB REU; IDE64 v4.1 or plain KERNAL I/O. */
#include <stdio.h>
#include "config.h"
#include "../../lib/tui.h"
#include "../../lib/reu_control_bank.h"

extern void bridge_enter(void);
extern unsigned char bridge_detect_ide(void);
extern unsigned char bridge_target, bridge_proof, bridge_failprobe;
static unsigned char header[64];
static unsigned char record[8];
static const unsigned char signature[5] = {0x52,0x42,0x47,0x34,4};
static unsigned char shim[1024];
static unsigned int cycles;
static unsigned int i;
static unsigned char key;
static unsigned char action;
static unsigned char nav_action;
static TuiMenu menu;
static const TuiRect title = {0, 0, 40, 3};
static const char *items[] = {
    "Start / resume C64OS (c)",
    "Test RAM save / restore (s)",
    "Test boot failure recovery (x)",
    "Boot location / settings (l)"
};
static char status[40];

static void show_status(const char *text, unsigned char color) {
    tui_puts_n(0, 24, text, 40, color);
}

#define CENTER_X(text) ((TUI_SCREEN_WIDTH - (sizeof(text) - 1u)) / 2u)

static void draw(void) {
    tui_clear(TUI_THEME_BG);
    tui_window_title(&title, "C64OS BRIDGE", TUI_THEME_BORDER, TUI_THEME_TITLE);
    tui_puts(CENTER_X("READYOS / C64OS"), 1, "READYOS / C64OS", TUI_COLOR_CYAN);
    tui_menu_draw(&menu);
    tui_puts(CENTER_X("Switch back using the ReadyOS utility"), 10, "Switch back using the ReadyOS utility", TUI_COLOR_GRAY3);
    tui_puts(CENTER_X("via C64OS Utilities."), 11, "via C64OS Utilities.", TUI_COLOR_GRAY3);
    tui_puts(1, 15, "REU 0-31: C64OS", TUI_COLOR_LIGHTBLUE);
    tui_puts(1, 16, "REU 32-38: bridge snapshots / reserve", TUI_COLOR_LIGHTBLUE);
    tui_puts(1, 17, "REU 39+: ReadyOS", TUI_COLOR_LIGHTBLUE);
    tui_puts(CENTER_X("UP/DOWN:SELECT  RETURN:OPEN"), 22, "UP/DOWN:SELECT  RETURN:OPEN", TUI_COLOR_GRAY3);
    tui_puts(CENTER_X("F2:NEXT APP  F4:PREV APP  CTRL+B:HOME"), 23, "F2:NEXT APP  F4:PREV APP  CTRL+B:HOME", TUI_COLOR_GRAY3);
}

static unsigned char supported(void) {
    if (*SHIM_READYOS_BANK != 39u) return 0;
    readyos_bank_read(REUCB_HEADER_OFF, header, sizeof(header));
    return header[0] == REUCB_MAGIC0 && header[1] == REUCB_MAGIC1 &&
        header[2] == REUCB_MAGIC2 && header[3] == REUCB_MAGIC3 &&
        header[REUCB_HEADER_CONTROL_BANK] == 39u &&
        (header[REUCB_HEADER_FLAGS] & REUCB_HEADER_FLAG_PHYS_SIZE) &&
        header[REUCB_HEADER_PHYS_BANKS] == 0u;
}

int main(void) {
    tui_init();
    tui_menu_init(&menu, 1, 5, 38, 4, items, 4);
    bridge_config_load(bridge_detect_ide() != 0);
    draw();
    if (!supported()) {
        tui_clear(TUI_THEME_BG);
        tui_window_title(&title,"C64OS BRIDGE SETUP REQUIRED",TUI_THEME_BORDER,TUI_THEME_TITLE);
        tui_puts(1,5,"Requires skip 39 and 16mb REU",TUI_COLOR_LIGHTRED);
        sprintf(status,"This ReadyOS boot skips %u banks",*SHIM_READYOS_BANK);
        tui_puts(1,7,status,TUI_COLOR_WHITE);
        tui_puts(1,10,"Normal D81 uses skip 0: cannot switch.",TUI_COLOR_GRAY3);
        tui_puts(1,12,"Build ReadyOS with reu_bank_skip=39.",TUI_COLOR_GRAY3);
        tui_puts(1,14,"C64OS 1.09: run Bridge Setup first.",TUI_COLOR_GRAY3);
        tui_puts(1,15,"Select 8-32 banks (0.5-2 MB), reboot.",TUI_COLOR_GRAY3);
        tui_puts(1,17,"ReadyOS skip stays 39 for every cap.",TUI_COLOR_GRAY3);
        show_status("L: boot location  Other key: launcher",TUI_COLOR_CYAN);
        if (tui_getkey()=='l') bridge_config_edit();
        tui_return_to_launcher();
    }
    if (bridge_detect_ide() == 255u) {
        show_status("unsupported ide64 revision / mode", TUI_COLOR_LIGHTRED);
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
        if (key == 'l') action = 3;
        if (action == 255) {
            tui_menu_draw(&menu);
            continue;
        }
        if (action == 3) {
            bridge_config_edit();
            draw();
            continue;
        }
        bridge_config_stage();
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
