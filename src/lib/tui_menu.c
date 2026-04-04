/*
 * tui_menu.c - Menu widgets
 */

#include "tui.h"

static unsigned int screen_offset;

void tui_menu_init(TuiMenu *menu, unsigned char x, unsigned char y,
                   unsigned char w, unsigned char h,
                   const char **items, unsigned char count) {
    menu->x = x;
    menu->y = y;
    menu->w = w;
    menu->h = h;
    menu->items = items;
    menu->count = count;
    menu->selected = 0;
    menu->scroll_offset = 0;
    menu->item_color = TUI_THEME_FG;
    menu->sel_color = TUI_THEME_HIGHLIGHT;
}

void tui_menu_draw(TuiMenu *menu) {
    unsigned char row;
    unsigned char item_idx;
    unsigned char col;

    for (row = 0; row < menu->h; ++row) {
        item_idx = menu->scroll_offset + row;
        screen_offset = (unsigned int)(menu->y + row) * 40 + menu->x;

        if (item_idx < menu->count) {
            unsigned char color;
            unsigned char prefix;

            if (item_idx == menu->selected) {
                color = menu->sel_color;
                prefix = 0x3E;
            } else {
                color = menu->item_color;
                prefix = 32;
            }

            TUI_SCREEN[screen_offset] = prefix;
            TUI_COLOR_RAM[screen_offset] = color;
            TUI_SCREEN[screen_offset + 1] = 32;
            TUI_COLOR_RAM[screen_offset + 1] = color;

            {
                const char *str = menu->items[item_idx];
                unsigned char pos;
                unsigned char maxlen = menu->w - 2;
                unsigned int text_offset = screen_offset + 2;

                for (pos = 0; str[pos] != 0 && pos < maxlen; ++pos) {
                    TUI_SCREEN[text_offset + pos] = tui_ascii_to_screen(str[pos]);
                    TUI_COLOR_RAM[text_offset + pos] = color;
                }
                for (; pos < maxlen; ++pos) {
                    TUI_SCREEN[text_offset + pos] = 32;
                    TUI_COLOR_RAM[text_offset + pos] = color;
                }
            }
        } else {
            for (col = 0; col < menu->w; ++col) {
                TUI_SCREEN[screen_offset + col] = 32;
                TUI_COLOR_RAM[screen_offset + col] = menu->item_color;
            }
        }
    }
}

unsigned char tui_menu_input(TuiMenu *menu, unsigned char key) {
    switch (key) {
        case TUI_KEY_UP:
            if (menu->selected > 0) {
                --menu->selected;
                if (menu->selected < menu->scroll_offset) {
                    menu->scroll_offset = menu->selected;
                }
            }
            break;

        case TUI_KEY_DOWN:
            if (menu->selected < menu->count - 1) {
                ++menu->selected;
                if (menu->selected >= menu->scroll_offset + menu->h) {
                    menu->scroll_offset = menu->selected - menu->h + 1;
                }
            }
            break;

        case TUI_KEY_RETURN:
            return menu->selected;

        case TUI_KEY_HOME:
            menu->selected = 0;
            menu->scroll_offset = 0;
            break;
    }

    return 255;
}

unsigned char tui_menu_selected(TuiMenu *menu) {
    return menu->selected;
}
