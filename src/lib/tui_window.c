/*
 * tui_window.c - Box and window rendering helpers
 */

#include "tui.h"
#include <string.h>

static unsigned int screen_offset;
static unsigned char i;

void tui_hline(unsigned char x, unsigned char y, unsigned char len, unsigned char color) {
    screen_offset = (unsigned int)y * 40 + x;

    for (i = 0; i < len && (x + i) < TUI_SCREEN_WIDTH; ++i) {
        TUI_SCREEN[screen_offset + i] = TUI_HLINE;
        TUI_COLOR_RAM[screen_offset + i] = color;
    }
}

void tui_vline(unsigned char x, unsigned char y, unsigned char len, unsigned char color) {
    screen_offset = (unsigned int)y * 40 + x;

    for (i = 0; i < len && (y + i) < TUI_SCREEN_HEIGHT; ++i) {
        TUI_SCREEN[screen_offset] = TUI_VLINE;
        TUI_COLOR_RAM[screen_offset] = color;
        screen_offset += 40;
    }
}

void tui_clear_line(unsigned char y, unsigned char x, unsigned char len, unsigned char color) {
    screen_offset = (unsigned int)y * 40 + x;
    for (i = 0; i < len && (x + i) < TUI_SCREEN_WIDTH; ++i) {
        TUI_SCREEN[screen_offset + i] = 32;
        TUI_COLOR_RAM[screen_offset + i] = color;
    }
}

void tui_window(const TuiRect *rect, unsigned char border_color) {
    unsigned char x2;
    unsigned char y2;
    unsigned char row;
    unsigned char col;

    x2 = rect->x + rect->w - 1;
    y2 = rect->y + rect->h - 1;

    tui_putc(rect->x, rect->y, TUI_CORNER_TL, border_color);
    tui_putc(x2, rect->y, TUI_CORNER_TR, border_color);
    tui_putc(rect->x, y2, TUI_CORNER_BL, border_color);
    tui_putc(x2, y2, TUI_CORNER_BR, border_color);

    tui_hline(rect->x + 1, rect->y, rect->w - 2, border_color);
    tui_hline(rect->x + 1, y2, rect->w - 2, border_color);
    tui_vline(rect->x, rect->y + 1, rect->h - 2, border_color);
    tui_vline(x2, rect->y + 1, rect->h - 2, border_color);

    if (rect->w <= 2 || rect->h <= 2) {
        return;
    }

    for (row = 1; row < (unsigned char)(rect->h - 1); ++row) {
        screen_offset = (unsigned int)(rect->y + row) * 40 + rect->x + 1;
        for (col = 1; col < (unsigned char)(rect->w - 1); ++col) {
            TUI_SCREEN[screen_offset + col - 1] = 32;
            TUI_COLOR_RAM[screen_offset + col - 1] = TUI_THEME_FG;
        }
    }
}

void tui_window_title(const TuiRect *rect, const char *title,
                      unsigned char border_color, unsigned char title_color) {
    unsigned char title_len;
    unsigned char title_x;

    tui_window(rect, border_color);

    title_len = strlen(title);
    if (title_len > rect->w - 4) {
        title_len = rect->w - 4;
    }
    title_x = rect->x + (rect->w - title_len) / 2;

    tui_puts_n(title_x, rect->y, title, title_len, title_color);
}
