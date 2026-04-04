/*
 * tui_input.c - Single-line text input widgets
 */

#include "tui.h"
#include <string.h>

static unsigned int screen_offset;
static unsigned char i;

void tui_input_init(TuiInput *input, unsigned char x, unsigned char y,
                    unsigned char width, unsigned char maxlen,
                    char *buffer, unsigned char color) {
    input->x = x;
    input->y = y;
    input->width = width;
    input->maxlen = maxlen;
    input->cursor = 0;
    input->buffer = buffer;
    input->color = color;
    buffer[0] = 0;
}

void tui_input_draw(TuiInput *input) {
    unsigned char len;
    unsigned char display_start;
    unsigned char display_len;

    len = strlen(input->buffer);
    screen_offset = (unsigned int)input->y * 40 + input->x;

    if (input->cursor >= input->width) {
        display_start = input->cursor - input->width + 1;
    } else {
        display_start = 0;
    }

    display_len = len - display_start;
    if (display_len > input->width) {
        display_len = input->width;
    }

    for (i = 0; i < input->width; ++i) {
        unsigned char ch;

        if (i < display_len) {
            ch = tui_ascii_to_screen(input->buffer[display_start + i]);
        } else {
            ch = 32;
        }

        if (display_start + i == input->cursor) {
            ch = 0xA0;
        }

        TUI_SCREEN[screen_offset + i] = ch;
        TUI_COLOR_RAM[screen_offset + i] = input->color;
    }
}

unsigned char tui_input_key(TuiInput *input, unsigned char key) {
    unsigned char len;

    len = strlen(input->buffer);

    if (key == TUI_KEY_RETURN) {
        return 1;
    }

    if (key == TUI_KEY_DEL) {
        if (input->cursor > 0) {
            --input->cursor;
            for (i = input->cursor; i < len; ++i) {
                input->buffer[i] = input->buffer[i + 1];
            }
        }
    } else if (key == TUI_KEY_LEFT) {
        if (input->cursor > 0) {
            --input->cursor;
        }
    } else if (key == TUI_KEY_RIGHT) {
        if (input->cursor < len) {
            ++input->cursor;
        }
    } else if (key == TUI_KEY_HOME) {
        input->cursor = 0;
    } else if (key >= 32 && key < 128) {
        if (len < input->maxlen) {
            for (i = len + 1; i > input->cursor; --i) {
                input->buffer[i] = input->buffer[i - 1];
            }
            input->buffer[input->cursor] = key;
            ++input->cursor;
        }
    }

    return 0;
}
