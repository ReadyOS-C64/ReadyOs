/*
 * tui_core.c - Core TUI primitives shared by most apps
 */

#include "tui.h"
#include <c64.h>
#include <conio.h>
#include <string.h>

static unsigned int screen_offset;
static unsigned char i;

void tui_init(void) {
    VIC.bordercolor = TUI_THEME_BG;
    VIC.bgcolor0 = TUI_THEME_BG;
    textcolor(TUI_THEME_FG);
    (void)kbrepeat(TUI_KEYREPEAT_NONE);
    clrscr();
}

void tui_clear(unsigned char bg_color) {
    VIC.bgcolor0 = bg_color;
    memset(TUI_SCREEN, 32, 1000);
    memset(TUI_COLOR_RAM, TUI_THEME_FG, 1000);
}

void tui_putc(unsigned char x, unsigned char y, unsigned char ch, unsigned char color) {
    screen_offset = (unsigned int)y * 40 + x;
    TUI_SCREEN[screen_offset] = ch;
    TUI_COLOR_RAM[screen_offset] = color;
}

void tui_puts(unsigned char x, unsigned char y, const char *str, unsigned char color) {
    screen_offset = (unsigned int)y * 40 + x;

    for (i = 0; str[i] != 0 && (x + i) < TUI_SCREEN_WIDTH; ++i) {
        TUI_SCREEN[screen_offset + i] = tui_ascii_to_screen(str[i]);
        TUI_COLOR_RAM[screen_offset + i] = color;
    }
}

void tui_puts_n(unsigned char x, unsigned char y, const char *str,
                unsigned char maxlen, unsigned char color) {
    screen_offset = (unsigned int)y * 40 + x;

    for (i = 0; str[i] != 0 && i < maxlen && (x + i) < TUI_SCREEN_WIDTH; ++i) {
        TUI_SCREEN[screen_offset + i] = tui_ascii_to_screen(str[i]);
        TUI_COLOR_RAM[screen_offset + i] = color;
    }

    for (; i < maxlen && (x + i) < TUI_SCREEN_WIDTH; ++i) {
        TUI_SCREEN[screen_offset + i] = 32;
        TUI_COLOR_RAM[screen_offset + i] = color;
    }
}

unsigned char tui_getkey(void) {
    return cgetc();
}

unsigned char tui_kbhit(void) {
    return kbhit();
}

unsigned char tui_ascii_to_screen(unsigned char ascii) {
    if (ascii >= 'A' && ascii <= 'Z') {
        return ascii - 'A' + 1;
    }
    if (ascii >= 'a' && ascii <= 'z') {
        return ascii - 'a' + 1;
    }
    if (ascii >= '0' && ascii <= '9') {
        return ascii - '0' + 48;
    }

    switch (ascii) {
        case ' ':  return 32;
        case '!':  return 33;
        case '"':  return 34;
        case '#':  return 35;
        case '$':  return 36;
        case '%':  return 37;
        case '&':  return 38;
        case '\'': return 39;
        case '(':  return 40;
        case ')':  return 41;
        case '*':  return 42;
        case '+':  return 43;
        case ',':  return 44;
        case '-':  return 45;
        case '.':  return 46;
        case '/':  return 47;
        case ':':  return 58;
        case ';':  return 59;
        case '<':  return 60;
        case '=':  return 61;
        case '>':  return 62;
        case '?':  return 63;
        case '@':  return 0;
        case '[':  return 27;
        case ']':  return 29;
        case '_':  return 100;
        default:   return 32;
    }
}
