/*
 * tui.h - TUI Library for Ready OS
 * Text User Interface with PETSCII box drawing
 *
 * For Commodore 64, compiled with CC65
 */

#ifndef TUI_H
#define TUI_H

#include <cbm.h>

/* Screen dimensions */
#define TUI_SCREEN_WIDTH  40
#define TUI_SCREEN_HEIGHT 25

/* Screen memory */
#define TUI_SCREEN    ((unsigned char*)0x0400)
#define TUI_COLOR_RAM ((unsigned char*)0xD800)

/* PETSCII Box Drawing Characters (screen codes) */
#define TUI_CORNER_TL   0x70  /* Upper-left corner */
#define TUI_CORNER_TR   0x6E  /* Upper-right corner */
#define TUI_CORNER_BL   0x6D  /* Lower-left corner */
#define TUI_CORNER_BR   0x7D  /* Lower-right corner */
#define TUI_HLINE       0x40  /* Horizontal line */
#define TUI_VLINE       0x5D  /* Vertical line */
#define TUI_CROSS       0x5B  /* Cross/intersection */
#define TUI_T_DOWN      0x72  /* T pointing down */
#define TUI_T_UP        0x71  /* T pointing up */
#define TUI_T_RIGHT     0x6B  /* T pointing right */
#define TUI_T_LEFT      0x73  /* T pointing left */

/* Colors (C64 color values) */
#define TUI_COLOR_BLACK       0
#define TUI_COLOR_WHITE       1
#define TUI_COLOR_RED         2
#define TUI_COLOR_CYAN        3
#define TUI_COLOR_PURPLE      4
#define TUI_COLOR_GREEN       5
#define TUI_COLOR_BLUE        6
#define TUI_COLOR_YELLOW      7
#define TUI_COLOR_ORANGE      8
#define TUI_COLOR_BROWN       9
#define TUI_COLOR_LIGHTRED   10
#define TUI_COLOR_GRAY1      11
#define TUI_COLOR_GRAY2      12
#define TUI_COLOR_LIGHTGREEN 13
#define TUI_COLOR_LIGHTBLUE  14
#define TUI_COLOR_GRAY3      15

/* Default color theme */
#define TUI_THEME_BG          TUI_COLOR_BLUE
#define TUI_THEME_FG          TUI_COLOR_WHITE
#define TUI_THEME_BORDER      TUI_COLOR_LIGHTBLUE
#define TUI_THEME_TITLE       TUI_COLOR_YELLOW
#define TUI_THEME_HIGHLIGHT   TUI_COLOR_CYAN
#define TUI_THEME_STATUS      TUI_COLOR_GRAY3

/* Key codes */
#define TUI_KEY_RETURN  13
#define TUI_KEY_UP      145   /* Cursor up */
#define TUI_KEY_DOWN    17    /* Cursor down */
#define TUI_KEY_LEFT    157   /* Cursor left */
#define TUI_KEY_RIGHT   29    /* Cursor right */
#define TUI_KEY_HOME    19    /* Home */
#define TUI_KEY_DEL     20    /* Delete */
#define TUI_KEY_F1      133
#define TUI_KEY_F2      137   /* Shift+F1 */
#define TUI_KEY_F3      134
#define TUI_KEY_F4      138   /* Shift+F3 */
#define TUI_KEY_F5      135
#define TUI_KEY_F6      139   /* Shift+F5 */
#define TUI_KEY_F7      136
#define TUI_KEY_F8      140   /* Shift+F7 */
#define TUI_KEY_RUNSTOP 3     /* RUN/STOP */
#define TUI_KEY_LARROW  95    /* ← (back arrow) key */

/* App switching keys */
#define TUI_KEY_NEXT_APP TUI_KEY_F2  /* F2 = Next app */
#define TUI_KEY_PREV_APP TUI_KEY_F4  /* F4 = Previous app */

/* Modifier key bits (from $028D SHFLAG) */
#define TUI_MOD_SHIFT  0x01
#define TUI_MOD_CBM    0x02   /* Commodore key */
#define TUI_MOD_CTRL   0x04

/* Shared global app hotkey slots in system RAM ($C7E0-$C7E8) */
#define TUI_HOTKEY_SLOT_COUNT 9
#define TUI_HOTKEY_NONE       0
#define TUI_HOTKEY_BIND_ONLY  0xFE
#define TUI_HOTKEY_LAUNCHER   0xFF
#define TUI_HOTKEY_BINDINGS   ((unsigned char*)0xC7E0)

/* Keyboard auto-repeat policy */
#define TUI_KEYREPEAT_CURSOR KBREPEAT_CURSOR
#define TUI_KEYREPEAT_NONE   KBREPEAT_NONE
#define TUI_KEYREPEAT_ALL    KBREPEAT_ALL

/* Rectangle structure */
typedef struct {
    unsigned char x;
    unsigned char y;
    unsigned char w;
    unsigned char h;
} TuiRect;

/* Text input field */
typedef struct {
    unsigned char x;
    unsigned char y;
    unsigned char width;
    unsigned char maxlen;
    unsigned char cursor;
    unsigned char color;
    char *buffer;
} TuiInput;

/* Menu/list selection */
typedef struct {
    unsigned char x;
    unsigned char y;
    unsigned char w;
    unsigned char h;
    unsigned char count;
    unsigned char selected;
    unsigned char scroll_offset;
    unsigned char item_color;
    unsigned char sel_color;
    const char **items;
} TuiMenu;

/* Scrollable text area */
typedef struct {
    unsigned char x;
    unsigned char y;
    unsigned char w;
    unsigned char h;
    unsigned int line_count;
    unsigned int scroll_pos;
    unsigned char color;
    const char **lines;
} TuiTextArea;

/*---------------------------------------------------------------------------
 * Core Functions
 *---------------------------------------------------------------------------*/

/* Initialize TUI system */
void tui_init(void);

/* Clear screen with background color */
void tui_clear(unsigned char bg_color);

/* Set cursor position (for compatibility) */
void tui_gotoxy(unsigned char x, unsigned char y);

/*---------------------------------------------------------------------------
 * Drawing Functions
 *---------------------------------------------------------------------------*/

/* Draw a single character at position */
void tui_putc(unsigned char x, unsigned char y, unsigned char ch, unsigned char color);

/* Draw a string at position (converts ASCII to screen codes) */
void tui_puts(unsigned char x, unsigned char y, const char *str, unsigned char color);

/* Draw a string with max length (truncates if needed) */
void tui_puts_n(unsigned char x, unsigned char y, const char *str,
                unsigned char maxlen, unsigned char color);

/* Draw horizontal line */
void tui_hline(unsigned char x, unsigned char y, unsigned char len, unsigned char color);

/* Draw vertical line */
void tui_vline(unsigned char x, unsigned char y, unsigned char len, unsigned char color);

/* Fill rectangle with character */
void tui_fill_rect(const TuiRect *rect, unsigned char ch, unsigned char color);

/* Clear rectangle (fill with spaces) */
void tui_clear_rect(const TuiRect *rect, unsigned char color);

/* Clear a single line segment with spaces */
void tui_clear_line(unsigned char y, unsigned char x, unsigned char len, unsigned char color);

/*---------------------------------------------------------------------------
 * Window/Panel Functions
 *---------------------------------------------------------------------------*/

/* Draw a window frame with PETSCII borders */
void tui_window(const TuiRect *rect, unsigned char border_color);

/* Draw a window with title */
void tui_window_title(const TuiRect *rect, const char *title,
                      unsigned char border_color, unsigned char title_color);

/* Draw a panel (no border, just cleared area with optional title) */
void tui_panel(const TuiRect *rect, const char *title, unsigned char bg_color);

/*---------------------------------------------------------------------------
 * Status Bar
 *---------------------------------------------------------------------------*/

/* Draw status bar at bottom of screen */
void tui_status_bar(const char *text, unsigned char color);

/* Draw status bar with multiple items */
void tui_status_bar_multi(const char **items, unsigned char count, unsigned char color);

/*---------------------------------------------------------------------------
 * Menu/List Functions
 *---------------------------------------------------------------------------*/

/* Initialize a menu */
void tui_menu_init(TuiMenu *menu, unsigned char x, unsigned char y,
                   unsigned char w, unsigned char h,
                   const char **items, unsigned char count);

/* Draw a menu */
void tui_menu_draw(TuiMenu *menu);

/* Handle menu input, returns selected item (0-255) or 255 if no selection */
unsigned char tui_menu_input(TuiMenu *menu, unsigned char key);

/* Get currently selected item */
unsigned char tui_menu_selected(TuiMenu *menu);

/*---------------------------------------------------------------------------
 * Text Input Functions
 *---------------------------------------------------------------------------*/

/* Initialize an input field */
void tui_input_init(TuiInput *input, unsigned char x, unsigned char y,
                    unsigned char width, unsigned char maxlen,
                    char *buffer, unsigned char color);

/* Draw an input field */
void tui_input_draw(TuiInput *input);

/* Handle input field keypress, returns 1 if enter pressed, 0 otherwise */
unsigned char tui_input_key(TuiInput *input, unsigned char key);

/* Clear input field */
void tui_input_clear(TuiInput *input);

/*---------------------------------------------------------------------------
 * Text Area Functions
 *---------------------------------------------------------------------------*/

/* Initialize a text area */
void tui_textarea_init(TuiTextArea *area, unsigned char x, unsigned char y,
                       unsigned char w, unsigned char h,
                       const char **lines, unsigned int line_count);

/* Draw a text area */
void tui_textarea_draw(TuiTextArea *area);

/* Scroll text area */
void tui_textarea_scroll(TuiTextArea *area, int delta);

/*---------------------------------------------------------------------------
 * Input Functions
 *---------------------------------------------------------------------------*/

/* Wait for keypress and return key code */
unsigned char tui_getkey(void);

/* Check if key is available */
unsigned char tui_kbhit(void);

#define tui_keyrepeat_set(mode) kbrepeat(mode)
#define tui_keyrepeat_default() ((void)kbrepeat(TUI_KEYREPEAT_NONE))

/* Get current modifier key state */
unsigned char tui_get_modifiers(void);

/* Check if "back to launcher" combo is pressed (C= + ←) */
unsigned char tui_is_back_pressed(void);

/* Return to launcher (saves current app state, loads launcher) */
void tui_return_to_launcher(void);

/* Switch directly to another app (saves current app, loads target app)
 * bank: REU bank of target app (1=editor, 2=calcplus, 3=hexview, etc.)
 * Only works if target app is already in REU!
 */
void tui_switch_to_app(unsigned char bank);

/* Get next/previous app bank for cycling through loaded apps
 * Returns the bank number, or 0 if no valid app found
 */
unsigned char tui_get_next_app(unsigned char current_bank);
unsigned char tui_get_prev_app(unsigned char current_bank);

/* Resolve launcher/app-switch/bind hotkeys.
 * Returns:
 *   0               = not handled
 *   1..15           = switch to bound/next/prev app bank
 *   TUI_HOTKEY_BIND_ONLY = binding updated, no navigation
 *   TUI_HOTKEY_LAUNCHER  = return to launcher
 */
unsigned char tui_handle_global_hotkey(unsigned char key,
                                       unsigned char current_bank,
                                       unsigned char allow_bind);

/*---------------------------------------------------------------------------
 * Utility Functions
 *---------------------------------------------------------------------------*/

/* Draw a progress bar using PETSCII block characters
 * filled/total are item counts (e.g., 1/3 apps loaded)
 * Uses screen code 0xA0 (solid block) for filled, 0x66 (checker) for empty
 */
void tui_progress_bar(unsigned char x, unsigned char y, unsigned char width,
                      unsigned char filled, unsigned char total,
                      unsigned char fill_color, unsigned char empty_color);

/* Convert ASCII character to screen code */
unsigned char tui_ascii_to_screen(unsigned char ascii);

/* Convert string to screen codes (in place) */
void tui_str_to_screen(char *str);

/* Print unsigned int as decimal */
void tui_print_uint(unsigned char x, unsigned char y, unsigned int value,
                    unsigned char color);

/* Print unsigned char as hex */
void tui_print_hex8(unsigned char x, unsigned char y, unsigned char value,
                    unsigned char color);

/* Print unsigned int as hex */
void tui_print_hex16(unsigned char x, unsigned char y, unsigned int value,
                     unsigned char color);

#endif /* TUI_H */
