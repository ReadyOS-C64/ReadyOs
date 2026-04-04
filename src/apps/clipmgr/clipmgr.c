/*
 * clipmgr.c - Ready OS Clipboard Manager
 * View and manage multi-item clipboard
 *
 * For Commodore 64, compiled with CC65
 */

#include "../../lib/tui.h"
#include "../../lib/clipboard.h"
#include "../../lib/reu_mgr.h"
#include "../../lib/resume_state.h"
#include <c64.h>
#include <cbm.h>
#include <conio.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * Constants
 *---------------------------------------------------------------------------*/

#define TITLE_Y      0
#define LIST_START_Y 4
#define LIST_HEIGHT  16
#define DETAIL_Y     20
#define HELP_Y       22
#define STATUS_Y     24

/* Max chars to preview per item */
#define PREVIEW_LEN  25

/* Full-content popup window */
#define VIEW_BOX_X       1
#define VIEW_BOX_Y       2
#define VIEW_BOX_W       38
#define VIEW_BOX_H       20
#define VIEW_TEXT_X      (VIEW_BOX_X + 1)
#define VIEW_TEXT_Y      (VIEW_BOX_Y + 3)
#define VIEW_TEXT_COLS   (VIEW_BOX_W - 2)
#define VIEW_TEXT_LINES  15
#define VIEW_PAGE_BYTES  (VIEW_TEXT_COLS * VIEW_TEXT_LINES)

/* File I/O */
#define MAX_DIR_ENTRIES 20
#define DIR_NAME_LEN    17
#define IO_BUF_SIZE     128
#define LFN_DIR         1
#define LFN_FILE        2
#define LFN_CMD         15
#define CLIP_MAX_BYTES  65535U

#define SHIM_CURRENT_BANK (*(volatile unsigned char*)0xC834)

/*---------------------------------------------------------------------------
 * Static variables
 *---------------------------------------------------------------------------*/

static unsigned char running;
static unsigned char selected;
static unsigned char scroll_offset;

/* Temp buffer for previewing clipboard item data */
static char preview_buf[42];
static unsigned char view_buf[VIEW_PAGE_BYTES];
static char view_line_buf[VIEW_TEXT_COLS + 1];
static char io_buf[IO_BUF_SIZE];

/* File browser data */
static char dir_names[MAX_DIR_ENTRIES][DIR_NAME_LEN];
static char dir_types[MAX_DIR_ENTRIES][4];
static char dir_display[MAX_DIR_ENTRIES][21];
static const char *dir_ptrs[MAX_DIR_ENTRIES];
static unsigned char dir_count;
static char save_buf[17];

typedef struct {
    unsigned char selected;
    unsigned char scroll_offset;
} ClipMgrResumeV1;

static ClipMgrResumeV1 resume_blob;
static unsigned char resume_ready;

static void resume_save_state(void) {
    if (!resume_ready) {
        return;
    }
    resume_blob.selected = selected;
    resume_blob.scroll_offset = scroll_offset;
    (void)resume_save(&resume_blob, sizeof(resume_blob));
}

static unsigned char resume_restore_state(void) {
    unsigned char count;
    unsigned int payload_len = 0;

    if (!resume_ready) {
        return 0;
    }
    if (!resume_try_load(&resume_blob, sizeof(resume_blob), &payload_len)) {
        return 0;
    }
    if (payload_len != sizeof(resume_blob)) {
        return 0;
    }

    count = clip_item_count();
    if (count == 0) {
        selected = 0;
        scroll_offset = 0;
        return 1;
    }

    selected = resume_blob.selected;
    scroll_offset = resume_blob.scroll_offset;

    if (selected >= count) {
        selected = (unsigned char)(count - 1);
    }
    if (scroll_offset > selected) {
        scroll_offset = selected;
    }
    if (selected >= scroll_offset + LIST_HEIGHT) {
        scroll_offset = (unsigned char)(selected - LIST_HEIGHT + 1);
    }
    return 1;
}

/*---------------------------------------------------------------------------
 * Drawing
 *---------------------------------------------------------------------------*/

static void draw_header(void) {
    TuiRect box = {0, TITLE_Y, 40, 3};
    tui_window_title(&box, "CLIPBOARD MANAGER",
                     TUI_COLOR_LIGHTBLUE, TUI_COLOR_YELLOW);

    /* Item count */
    tui_puts(2, TITLE_Y + 1, "ITEMS: ", TUI_COLOR_WHITE);
    tui_print_uint(9, TITLE_Y + 1, clip_item_count(), TUI_COLOR_CYAN);
    tui_puts(12, TITLE_Y + 1, "/16", TUI_COLOR_GRAY3);
}

static void draw_column_header(void) {
    tui_puts(1, 3, "#  TYPE  SIZE  PREVIEW", TUI_COLOR_GRAY3);
}

static void draw_item(unsigned char list_row, unsigned char item_idx) {
    unsigned char y;
    unsigned char color;
    unsigned int size;
    unsigned int fetched;
    unsigned char plen;
    unsigned char pi;

    y = LIST_START_Y + list_row;

    if (item_idx >= clip_item_count()) {
        /* Empty row */
        tui_clear_line(y, 0, 40, TUI_COLOR_WHITE);
        return;
    }

    color = (item_idx == selected) ? TUI_COLOR_CYAN : TUI_COLOR_WHITE;

    /* Selection indicator */
    if (item_idx == selected) {
        tui_putc(0, y, 0x3E, color);  /* '>' */
    } else {
        tui_putc(0, y, 32, color);    /* space */
    }

    /* Item number (1-based) */
    tui_print_uint(1, y, item_idx + 1, color);
    tui_puts(3, y, ".", color);

    /* Type */
    if (clip_get_type(item_idx) == CLIP_TYPE_TEXT) {
        tui_puts(5, y, "TXT", color);
    } else {
        tui_puts(5, y, "???", color);
    }

    /* Size */
    size = clip_get_size(item_idx);
    if (size < 1000) {
        tui_puts_n(10, y, "", 5, color);
        tui_print_uint(10, y, size, color);
        tui_puts(14, y, "B", color);
    } else {
        tui_puts_n(10, y, "", 5, color);
        tui_print_uint(10, y, size / 1024, color);
        tui_puts(14, y, "K", color);
    }

    /* Preview: fetch first PREVIEW_LEN bytes */
    memset(preview_buf, 0, sizeof(preview_buf));
    fetched = clip_paste(item_idx, preview_buf, PREVIEW_LEN);

    /* Sanitize for display: replace non-printable chars */
    plen = (unsigned char)(fetched < PREVIEW_LEN ? fetched : PREVIEW_LEN);
    for (pi = 0; pi < plen; ++pi) {
        if (preview_buf[pi] < 32 || preview_buf[pi] >= 128) {
            preview_buf[pi] = '.';
        }
    }
    preview_buf[plen] = 0;

    tui_puts(16, y, "\"", color);
    tui_puts_n(17, y, preview_buf, 20, color);
    if (fetched > 20) {
        tui_puts(37, y, "...", TUI_COLOR_GRAY3);
    } else {
        tui_puts(17 + plen, y, "\"", color);
        /* Pad rest */
        tui_puts_n(18 + plen, y, "", 40 - 18 - plen, color);
    }
}

static void draw_list(void) {
    unsigned char row;
    unsigned char count;

    count = clip_item_count();

    for (row = 0; row < LIST_HEIGHT; ++row) {
        unsigned char idx = scroll_offset + row;
        if (idx < count) {
            draw_item(row, idx);
        } else {
            tui_clear_line(LIST_START_Y + row, 0, 40, TUI_COLOR_WHITE);
        }
    }
}

static void draw_detail(void) {
    unsigned int size;
    unsigned int fetched;
    unsigned char i;

    tui_clear_line(DETAIL_Y, 0, 40, TUI_COLOR_WHITE);
    tui_clear_line(DETAIL_Y + 1, 0, 40, TUI_COLOR_WHITE);

    if (clip_item_count() == 0) {
        tui_puts(4, DETAIL_Y, "CLIPBOARD IS EMPTY", TUI_COLOR_GRAY3);
        return;
    }

    if (selected >= clip_item_count()) return;

    /* Show full first 38 chars */
    size = clip_get_size(selected);
    memset(preview_buf, 0, sizeof(preview_buf));
    fetched = clip_paste(selected, preview_buf, 38);

    /* Sanitize */
    for (i = 0; i < (unsigned char)fetched; ++i) {
        if (preview_buf[i] < 32 || preview_buf[i] >= 128) {
            preview_buf[i] = '.';
        }
    }
    preview_buf[fetched] = 0;

    tui_puts_n(1, DETAIL_Y, preview_buf, 38, TUI_COLOR_LIGHTGREEN);

    /* Size info */
    tui_puts(1, DETAIL_Y + 1, "SIZE:", TUI_COLOR_GRAY3);
    tui_print_uint(7, DETAIL_Y + 1, size, TUI_COLOR_WHITE);
    tui_puts(13, DETAIL_Y + 1, "BYTES", TUI_COLOR_GRAY3);
}

static void draw_help(void) {
    tui_puts(0, HELP_Y, "RET:VIEW F5:LOAD F6:SAVE DEL:DELETE", TUI_COLOR_GRAY3);
    tui_puts(0, HELP_Y + 1, "C:CLEAR ALL F2/F4:APPS CTRL+B:HOME", TUI_COLOR_GRAY3);
}

static void draw_status(void) {
    unsigned char free_banks;
    free_banks = reu_count_free();
    tui_puts_n(0, STATUS_Y, "FREE REU BANKS: ", 16, TUI_COLOR_GRAY3);
    tui_print_uint(16, STATUS_Y, free_banks, TUI_COLOR_WHITE);
    tui_puts_n(20, STATUS_Y, "", 20, TUI_COLOR_WHITE);
}

static unsigned char clip_item_bank(unsigned char index) {
    unsigned char *entry;
    entry = CLIP_TABLE + ((unsigned int)index * 8);
    return entry[0];
}

static void show_message(const char *msg, unsigned char color) {
    TuiRect win;
    unsigned char len;

    len = strlen(msg);
    win.x = 8;
    win.y = 10;
    win.w = 24;
    win.h = 5;
    tui_window(&win, TUI_COLOR_LIGHTBLUE);
    tui_puts(20 - len / 2, 11, msg, color);
    tui_puts(10, 13, "PRESS ANY KEY", TUI_COLOR_GRAY3);
    tui_getkey();
}

static void build_dir_display(unsigned char idx) {
    unsigned char i, len;

    strcpy(dir_display[idx], dir_names[idx]);
    len = strlen(dir_display[idx]);

    for (i = len; i < 17; ++i) {
        dir_display[idx][i] = ' ';
    }

    dir_display[idx][17] = dir_types[idx][0];
    dir_display[idx][18] = dir_types[idx][1];
    dir_display[idx][19] = dir_types[idx][2];
    dir_display[idx][20] = 0;
}

static void read_directory(void) {
    unsigned char buf[2];
    unsigned char ch;
    unsigned char in_quotes;
    unsigned char name_pos;
    unsigned char type_pos;
    unsigned char first_line;
    unsigned char past_space;
    int n;

    dir_count = 0;

    if (cbm_open(LFN_DIR, 8, 0, "$") != 0) {
        return;
    }

    cbm_read(LFN_DIR, buf, 2);
    first_line = 1;

    while (dir_count < MAX_DIR_ENTRIES) {
        n = cbm_read(LFN_DIR, buf, 2);
        if (n < 2 || (buf[0] == 0 && buf[1] == 0)) break;

        cbm_read(LFN_DIR, buf, 2);

        in_quotes = 0;
        name_pos = 0;

        while (1) {
            n = cbm_read(LFN_DIR, &ch, 1);
            if (n < 1 || ch == 0) break;

            if (ch == 0x22) {
                if (in_quotes) break;
                in_quotes = 1;
                continue;
            }

            if (in_quotes && !first_line) {
                if (name_pos < DIR_NAME_LEN - 1) {
                    dir_names[dir_count][name_pos] = ch;
                    ++name_pos;
                }
            }
        }

        type_pos = 0;
        past_space = 0;
        dir_types[dir_count][0] = 0;

        if (ch != 0) {
            while (1) {
                n = cbm_read(LFN_DIR, &ch, 1);
                if (n < 1 || ch == 0) break;

                if (!past_space) {
                    if (ch != ' ' && ch != 0xA0) {
                        past_space = 1;
                        if (type_pos < 3) {
                            dir_types[dir_count][type_pos] = ch;
                            ++type_pos;
                        }
                    }
                } else {
                    if (type_pos < 3 && ch != ' ' && ch != 0xA0) {
                        dir_types[dir_count][type_pos] = ch;
                        ++type_pos;
                    }
                }
            }
        }
        dir_types[dir_count][type_pos] = 0;

        if (first_line) {
            first_line = 0;
            continue;
        }

        if (name_pos > 0) {
            dir_names[dir_count][name_pos] = 0;
            build_dir_display(dir_count);
            dir_ptrs[dir_count] = dir_display[dir_count];
            ++dir_count;
        }
    }

    cbm_close(LFN_DIR);
}

static unsigned char show_open_dialog(void) {
    TuiRect win;
    TuiMenu menu;
    unsigned char key;
    unsigned char result;

    tui_clear(TUI_COLOR_BLUE);

    win.x = 0;
    win.y = 0;
    win.w = 40;
    win.h = 24;
    tui_window_title(&win, "LOAD TO CLIPBOARD", TUI_COLOR_LIGHTBLUE, TUI_COLOR_YELLOW);

    tui_puts(10, 11, "READING DISK...", TUI_COLOR_YELLOW);
    read_directory();
    tui_clear_line(11, 1, 38, TUI_COLOR_WHITE);

    if (dir_count == 0) {
        tui_puts(6, 10, "NO FILES FOUND ON DISK", TUI_COLOR_LIGHTRED);
        tui_puts(6, 14, "PRESS ANY KEY", TUI_COLOR_GRAY3);
        tui_getkey();
        return 255;
    }

    tui_print_uint(1, 22, dir_count, TUI_COLOR_GRAY3);
    tui_puts(4, 22, "FILE(S) ON DISK", TUI_COLOR_GRAY3);
    tui_puts(1, 24, "UP/DN:SELECT RET:LOAD STOP:CANCEL", TUI_COLOR_GRAY3);

    tui_menu_init(&menu, 1, 2, 38, 18, dir_ptrs, dir_count);
    menu.item_color = TUI_COLOR_WHITE;
    menu.sel_color = TUI_COLOR_CYAN;
    tui_menu_draw(&menu);

    while (1) {
        key = tui_getkey();

        if (key == TUI_KEY_RETURN) {
            return menu.selected;
        }
        if (key == TUI_KEY_RUNSTOP || key == TUI_KEY_LARROW) {
            return 255;
        }

        result = tui_menu_input(&menu, key);
        if (result != 255) {
            return result;
        }

        tui_menu_draw(&menu);
    }
}

static unsigned char clip_insert_bank_item(unsigned char bank, unsigned int size) {
    unsigned char count;
    unsigned char *entry;

    count = *CLIP_COUNT;
    if (count >= CLIP_MAX_ITEMS) {
        return 1;
    }

    if (count > 0) {
        memmove(CLIP_TABLE + 8, CLIP_TABLE, (unsigned int)count * 8);
    }

    entry = CLIP_TABLE;
    entry[0] = bank;
    entry[1] = CLIP_TYPE_TEXT;
    entry[2] = (unsigned char)(size & 0xFF);
    entry[3] = (unsigned char)(size >> 8);
    entry[4] = 0;
    entry[5] = 0;
    entry[6] = 0;
    entry[7] = 0;

    *CLIP_COUNT = count + 1;
    return 0;
}

static unsigned char open_mode_for_type(const char *type) {
    if (type[0] == 'P') return 'p';
    if (type[0] == 'S') return 's';
    if (type[0] == 'U') return 'u';
    return 's';
}

/* Load selected file into one clipboard slot, capped to 65535 bytes. */
static unsigned char file_load_to_clipboard(const char *name, const char *type,
                                            unsigned char *truncated_out) {
    char open_str[24];
    unsigned char len;
    unsigned char bank;
    unsigned int total;
    unsigned int n_u;
    unsigned int remaining;
    int n;
    unsigned char truncated;

    *truncated_out = 0;
    truncated = 0;

    if (*CLIP_COUNT >= CLIP_MAX_ITEMS) {
        return 2;
    }

    bank = reu_alloc_bank(REU_CLIPBOARD);
    if (bank == 0xFF) {
        return 3;
    }

    strcpy(open_str, name);
    len = strlen(open_str);
    open_str[len] = ',';
    open_str[len + 1] = open_mode_for_type(type);
    open_str[len + 2] = ',';
    open_str[len + 3] = 'r';
    open_str[len + 4] = 0;

    if (cbm_open(LFN_FILE, 8, 2, open_str) != 0) {
        reu_free_bank(bank);
        return 1;
    }

    total = 0;
    while (1) {
        n = cbm_read(LFN_FILE, io_buf, IO_BUF_SIZE);
        if (n <= 0) break;

        n_u = (unsigned int)(unsigned char)n;
        remaining = CLIP_MAX_BYTES - total;
        if (n_u > remaining) {
            n_u = remaining;
            truncated = 1;
        }

        if (n_u > 0) {
            reu_dma_stash((unsigned int)io_buf, bank, total, n_u);
            total += n_u;
        }

        if (truncated || total >= CLIP_MAX_BYTES) {
            break;
        }
    }

    cbm_close(LFN_FILE);

    if (clip_insert_bank_item(bank, total) != 0) {
        reu_free_bank(bank);
        return 2;
    }

    *truncated_out = truncated;
    return 0;
}

static unsigned char file_save_clip_entry(const char *name, unsigned char item_idx) {
    char cmd_str[24];
    char open_str[24];
    unsigned int size;
    unsigned int remain;
    unsigned int offset;
    unsigned int chunk;
    unsigned char bank;

    strcpy(cmd_str, "s:");
    strcat(cmd_str, name);
    cbm_open(LFN_CMD, 8, 15, cmd_str);
    cbm_close(LFN_CMD);

    strcpy(open_str, name);
    strcat(open_str, ",s,w");

    if (cbm_open(LFN_FILE, 8, 2, open_str) != 0) {
        return 1;
    }

    bank = clip_item_bank(item_idx);
    size = clip_get_size(item_idx);
    offset = 0;

    while (offset < size) {
        remain = size - offset;
        chunk = (remain < IO_BUF_SIZE) ? remain : IO_BUF_SIZE;
        reu_dma_fetch((unsigned int)io_buf, bank, offset, chunk);
        cbm_write(LFN_FILE, io_buf, chunk);
        offset += chunk;
    }

    cbm_close(LFN_FILE);
    return 0;
}

static void build_default_save_name(unsigned char item_idx) {
    unsigned int n;
    unsigned char pos;
    unsigned char tlen;
    char tmp[5];

    save_buf[0] = 'C';
    save_buf[1] = 'L';
    save_buf[2] = 'I';
    save_buf[3] = 'P';
    pos = 4;

    n = (unsigned int)item_idx + 1;
    tlen = 0;
    do {
        tmp[tlen++] = (char)('0' + (n % 10));
        n /= 10;
    } while (n > 0 && tlen < sizeof(tmp));

    while (tlen > 0 && pos < 16) {
        save_buf[pos++] = tmp[--tlen];
    }
    save_buf[pos] = 0;
}

static unsigned char show_save_dialog(unsigned char item_idx) {
    TuiRect win;
    TuiInput input;
    unsigned char key;

    tui_clear(TUI_COLOR_BLUE);

    win.x = 5;
    win.y = 7;
    win.w = 30;
    win.h = 8;
    tui_window_title(&win, "SAVE CLIP AS SEQ", TUI_COLOR_LIGHTBLUE, TUI_COLOR_YELLOW);

    tui_puts(7, 10, "FILENAME:", TUI_COLOR_WHITE);

    tui_input_init(&input, 7, 11, 20, 16, save_buf, TUI_COLOR_CYAN);
    build_default_save_name(item_idx);
    input.cursor = strlen(save_buf);
    tui_input_draw(&input);

    tui_puts(7, 13, "RET:SAVE  STOP:CANCEL", TUI_COLOR_GRAY3);

    while (1) {
        key = tui_getkey();

        if (key == TUI_KEY_RUNSTOP || key == TUI_KEY_LARROW) {
            return 0;
        }

        if (tui_input_key(&input, key)) {
            if (save_buf[0] == 0) {
                continue;
            }

            tui_puts(7, 12, "SAVING...", TUI_COLOR_YELLOW);
            if (file_save_clip_entry(save_buf, item_idx) != 0) {
                show_message("SAVE ERROR!", TUI_COLOR_LIGHTRED);
                return 0;
            }
            return 1;
        }

        tui_input_draw(&input);
    }
}

static unsigned char sanitize_byte(unsigned char b) {
    if (b >= 32 && b < 128) return b;
    return '.';
}

static void draw_view_page(unsigned char item_idx, unsigned int page_offset) {
    TuiRect box = {VIEW_BOX_X, VIEW_BOX_Y, VIEW_BOX_W, VIEW_BOX_H};
    unsigned int size;
    unsigned int remain;
    unsigned int fetched;
    unsigned int src;
    unsigned int end_off;
    unsigned int page_no;
    unsigned int page_total;
    unsigned int i;
    unsigned char row;
    unsigned char col;
    unsigned char ch;
    unsigned char bank;

    size = clip_get_size(item_idx);
    remain = (page_offset < size) ? (size - page_offset) : 0;
    fetched = (remain < VIEW_PAGE_BYTES) ? remain : VIEW_PAGE_BYTES;

    tui_window(&box, TUI_COLOR_CYAN);
    tui_puts_n(3, VIEW_BOX_Y, "CLIP ITEM VIEW", 20, TUI_COLOR_YELLOW);

    tui_puts_n(VIEW_TEXT_X, VIEW_BOX_Y + 1, "BYTE ", 5, TUI_COLOR_GRAY3);
    tui_print_uint(VIEW_TEXT_X + 5, VIEW_BOX_Y + 1, page_offset, TUI_COLOR_WHITE);
    tui_puts_n(VIEW_TEXT_X + 10, VIEW_BOX_Y + 1, "-", 1, TUI_COLOR_GRAY3);

    end_off = page_offset;
    if (fetched > 0) {
        end_off = page_offset + fetched - 1;
    }
    tui_print_uint(VIEW_TEXT_X + 11, VIEW_BOX_Y + 1, end_off, TUI_COLOR_WHITE);
    tui_puts_n(VIEW_TEXT_X + 17, VIEW_BOX_Y + 1, " OF ", 4, TUI_COLOR_GRAY3);
    tui_print_uint(VIEW_TEXT_X + 21, VIEW_BOX_Y + 1, size, TUI_COLOR_WHITE);

    page_no = (page_offset / VIEW_PAGE_BYTES) + 1;
    page_total = (size == 0) ? 1 : ((size - 1) / VIEW_PAGE_BYTES) + 1;
    tui_puts_n(VIEW_TEXT_X + 28, VIEW_BOX_Y + 1, "P", 1, TUI_COLOR_GRAY3);
    tui_print_uint(VIEW_TEXT_X + 29, VIEW_BOX_Y + 1, page_no, TUI_COLOR_WHITE);
    tui_puts_n(VIEW_TEXT_X + 32, VIEW_BOX_Y + 1, "/", 1, TUI_COLOR_GRAY3);
    tui_print_uint(VIEW_TEXT_X + 33, VIEW_BOX_Y + 1, page_total, TUI_COLOR_WHITE);

    for (row = 0; row < VIEW_TEXT_LINES; ++row) {
        tui_puts_n(VIEW_TEXT_X, VIEW_TEXT_Y + row, "", VIEW_TEXT_COLS,
                   TUI_COLOR_LIGHTGREEN);
    }

    if (fetched == 0) {
        tui_puts_n(VIEW_TEXT_X, VIEW_TEXT_Y, "(EMPTY)", VIEW_TEXT_COLS, TUI_COLOR_GRAY3);
    } else {
        bank = clip_item_bank(item_idx);
        reu_dma_fetch((unsigned int)view_buf, bank, page_offset, fetched);

        src = 0;
        for (row = 0; row < VIEW_TEXT_LINES; ++row) {
            memset(view_line_buf, ' ', VIEW_TEXT_COLS);
            view_line_buf[VIEW_TEXT_COLS] = 0;

            col = 0;
            while (col < VIEW_TEXT_COLS && src < fetched) {
                ch = view_buf[src++];
                if (ch == 13 || ch == 10) {
                    if (ch == 13 && src < fetched && view_buf[src] == 10) {
                        ++src;  /* Coalesce CRLF */
                    }
                    break;
                }
                view_line_buf[col++] = (char)sanitize_byte(ch);
            }

            tui_puts_n(VIEW_TEXT_X, VIEW_TEXT_Y + row, view_line_buf, VIEW_TEXT_COLS,
                       TUI_COLOR_LIGHTGREEN);

            if (src >= fetched) {
                for (i = (unsigned int)(row + 1); i < VIEW_TEXT_LINES; ++i) {
                    tui_puts_n(VIEW_TEXT_X, VIEW_TEXT_Y + (unsigned char)i, "", VIEW_TEXT_COLS,
                               TUI_COLOR_LIGHTGREEN);
                }
                break;
            }
        }
    }

    tui_puts_n(VIEW_TEXT_X, VIEW_BOX_Y + VIEW_BOX_H - 2,
               "UP/DN:PAGE  RET/STOP:BACK", VIEW_TEXT_COLS, TUI_COLOR_GRAY3);
}

static void show_full_item_view(unsigned char item_idx) {
    unsigned char key;
    unsigned int size;
    unsigned int page_offset;

    size = clip_get_size(item_idx);
    page_offset = 0;

    while (1) {
        draw_view_page(item_idx, page_offset);
        key = tui_getkey();

        if (key == TUI_KEY_UP) {
            if (page_offset >= VIEW_PAGE_BYTES) {
                page_offset -= VIEW_PAGE_BYTES;
            } else {
                page_offset = 0;
            }
            continue;
        }

        if (key == TUI_KEY_DOWN) {
            if (page_offset + VIEW_PAGE_BYTES < size) {
                page_offset += VIEW_PAGE_BYTES;
            }
            continue;
        }

        if (key == TUI_KEY_RETURN || key == TUI_KEY_RUNSTOP || key == TUI_KEY_LARROW) {
            break;
        }
    }
}

static void clipmgr_draw(void) {
    tui_clear(TUI_COLOR_BLUE);
    draw_header();
    draw_column_header();
    draw_list();
    draw_detail();
    draw_help();
    draw_status();
}

/*---------------------------------------------------------------------------
 * Input handling
 *---------------------------------------------------------------------------*/

static void clipmgr_loop(void) {
    unsigned char key;
    unsigned char count;
    unsigned char sel_idx;
    unsigned char truncated;
    unsigned char rc;

    clipmgr_draw();

    while (running) {
        key = tui_getkey();
        count = clip_item_count();

        /* Check for CTRL+B (back to launcher) */
        if (key == 2) {
            resume_save_state();
            tui_return_to_launcher();
        }

        switch (key) {
            case TUI_KEY_UP:
                if (selected > 0) {
                    --selected;
                    if (selected < scroll_offset) {
                        scroll_offset = selected;
                    }
                    draw_list();
                    draw_detail();
                }
                break;

            case TUI_KEY_DOWN:
                if (count > 0 && selected < count - 1) {
                    ++selected;
                    if (selected >= scroll_offset + LIST_HEIGHT) {
                        scroll_offset = selected - LIST_HEIGHT + 1;
                    }
                    draw_list();
                    draw_detail();
                }
                break;

            case TUI_KEY_DEL:
                if (count > 0 && selected < count) {
                    clip_delete(selected);
                    count = clip_item_count();
                    if (selected >= count && selected > 0) {
                        --selected;
                    }
                    clipmgr_draw();
                }
                break;

            case TUI_KEY_RETURN:
                if (count > 0 && selected < count) {
                    show_full_item_view(selected);
                    clipmgr_draw();
                }
                break;

            case TUI_KEY_F5:
                sel_idx = show_open_dialog();
                if (sel_idx != 255) {
                    tui_clear(TUI_COLOR_BLUE);
                    tui_puts(13, 12, "LOADING...", TUI_COLOR_YELLOW);
                    rc = file_load_to_clipboard(dir_names[sel_idx], dir_types[sel_idx], &truncated);
                    if (rc == 0) {
                        selected = 0;
                        scroll_offset = 0;
                        if (truncated) {
                            show_message("LOADED (TRUNC 64K)", TUI_COLOR_YELLOW);
                        } else {
                            show_message("LOADED", TUI_COLOR_LIGHTGREEN);
                        }
                    } else if (rc == 2) {
                        show_message("CLIPBOARD FULL", TUI_COLOR_LIGHTRED);
                    } else if (rc == 3) {
                        show_message("NO REU SPACE", TUI_COLOR_LIGHTRED);
                    } else {
                        show_message("LOAD ERROR!", TUI_COLOR_LIGHTRED);
                    }
                }
                clipmgr_draw();
                break;

            case TUI_KEY_F6:
                if (count == 0 || selected >= count) {
                    show_message("CLIPBOARD EMPTY", TUI_COLOR_LIGHTRED);
                    clipmgr_draw();
                    break;
                }
                if (show_save_dialog(selected)) {
                    show_message("SAVED", TUI_COLOR_LIGHTGREEN);
                }
                clipmgr_draw();
                break;

            case 'c':
            case 'C':
                if (count > 0) {
                    clip_clear();
                    selected = 0;
                    scroll_offset = 0;
                    clipmgr_draw();
                }
                break;

            case TUI_KEY_NEXT_APP:
                {
                    unsigned char current = *((unsigned char*)0xC834);
                    unsigned char next = tui_get_next_app(current);
                    if (next != 0) {
                        resume_save_state();
                        tui_switch_to_app(next);
                    }
                }
                break;

            case TUI_KEY_PREV_APP:
                {
                    unsigned char current = *((unsigned char*)0xC834);
                    unsigned char prev = tui_get_prev_app(current);
                    if (prev != 0) {
                        resume_save_state();
                        tui_switch_to_app(prev);
                    }
                }
                break;

            case TUI_KEY_RUNSTOP:
                running = 0;
                break;
        }
    }

    __asm__("jmp $FCE2");
}

/*---------------------------------------------------------------------------
 * Main
 *---------------------------------------------------------------------------*/

int main(void) {
    unsigned char bank;

    tui_init();
    reu_mgr_init();

    selected = 0;
    scroll_offset = 0;
    resume_ready = 0;
    bank = SHIM_CURRENT_BANK;
    if (bank >= 1 && bank <= 15) {
        resume_init_for_app(bank, bank, RESUME_SCHEMA_V1);
        resume_ready = 1;
    }
    (void)resume_restore_state();
    running = 1;

    clipmgr_loop();
    return 0;
}
