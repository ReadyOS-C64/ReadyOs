/*
 * tui_misc.c - Numeric formatting and progress bar helpers
 */

#include "tui.h"

void tui_progress_bar(unsigned char x, unsigned char y, unsigned char width,
                      unsigned char filled, unsigned char total,
                      unsigned char fill_color, unsigned char empty_color) {
    unsigned int screen_offset;
    unsigned char filled_width;
    unsigned char pos;

    if (total == 0) {
        filled_width = 0;
    } else {
        filled_width = (unsigned char)((unsigned int)filled * width / total);
    }

    screen_offset = (unsigned int)y * 40 + x;

    for (pos = 0; pos < width && (x + pos) < TUI_SCREEN_WIDTH; ++pos) {
        if (pos < filled_width) {
            TUI_SCREEN[screen_offset + pos] = 0xA0;
            TUI_COLOR_RAM[screen_offset + pos] = fill_color;
        } else {
            TUI_SCREEN[screen_offset + pos] = 0x66;
            TUI_COLOR_RAM[screen_offset + pos] = empty_color;
        }
    }
}

void tui_print_uint(unsigned char x, unsigned char y, unsigned int value,
                    unsigned char color) {
    static char buf[6];
    unsigned char pos;

    pos = 5;
    buf[5] = 0;

    do {
        --pos;
        buf[pos] = '0' + (value % 10);
        value /= 10;
    } while (value > 0 && pos > 0);

    tui_puts(x, y, &buf[pos], color);
}

void tui_print_hex8(unsigned char x, unsigned char y, unsigned char value,
                    unsigned char color) {
    static const char hex[] = "0123456789ABCDEF";

    tui_putc(x, y, tui_ascii_to_screen('$'), color);
    tui_putc(x + 1, y, tui_ascii_to_screen(hex[(value >> 4) & 0x0F]), color);
    tui_putc(x + 2, y, tui_ascii_to_screen(hex[value & 0x0F]), color);
}

void tui_print_hex16(unsigned char x, unsigned char y, unsigned int value,
                     unsigned char color) {
    static const char hex[] = "0123456789ABCDEF";

    tui_putc(x, y, tui_ascii_to_screen('$'), color);
    tui_putc(x + 1, y, tui_ascii_to_screen(hex[(value >> 12) & 0x0F]), color);
    tui_putc(x + 2, y, tui_ascii_to_screen(hex[(value >> 8) & 0x0F]), color);
    tui_putc(x + 3, y, tui_ascii_to_screen(hex[(value >> 4) & 0x0F]), color);
    tui_putc(x + 4, y, tui_ascii_to_screen(hex[value & 0x0F]), color);
}
