/*
 * tui_nav.c - App switching and launcher return via shim
 */

#include "tui.h"
#include "reu_control_bank.h"
#include "tui_readyos.h"

#define SHIM_TARGET_BANK ((unsigned char*)0xC820)

#define APP_BANK_EDITOR   1
#define APP_BANK_MAX      TUI_APP_BANK_MAX

static unsigned char tui_bank_loaded(unsigned char bank) {
    return (unsigned char)(tui_readyos_read_byte(
        (unsigned int)(REUCB_TOKEN_STATUS_OFF + bank)) & REUCB_TOKEN_LOADED);
}

void tui_return_to_launcher(void) {
    __asm__("jmp $C80C");
}

void tui_switch_to_app(unsigned char bank) {
    *SHIM_TARGET_BANK = bank;
    __asm__("jmp $C80F");
}

unsigned char tui_get_next_app(unsigned char current_bank) {
    unsigned char next = current_bank;
    unsigned char tries = 0;

    if (current_bank < 1 || current_bank > APP_BANK_MAX) {
        current_bank = APP_BANK_EDITOR;
        next = current_bank;
    }

    while (tries < APP_BANK_MAX) {
        ++next;
        if (next > APP_BANK_MAX) {
            next = APP_BANK_EDITOR;
        }
        if (next != current_bank && tui_bank_loaded(next)) {
            return next;
        }
        ++tries;
    }

    return 0;
}

unsigned char tui_get_prev_app(unsigned char current_bank) {
    unsigned char prev = current_bank;
    unsigned char tries = 0;

    if (current_bank < 1 || current_bank > APP_BANK_MAX) {
        current_bank = APP_BANK_EDITOR;
        prev = current_bank;
    }

    while (tries < APP_BANK_MAX) {
        if (prev <= APP_BANK_EDITOR) {
            prev = APP_BANK_MAX;
        } else {
            --prev;
        }
        if (prev != current_bank && tui_bank_loaded(prev)) {
            return prev;
        }
        ++tries;
    }

    return 0;
}
