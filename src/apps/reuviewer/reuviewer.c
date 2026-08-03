/*
 * reuviewer.c - Ready OS REU Memory Map Viewer
 * Visual 16x16 grid showing all 256 REU banks
 *
 * For Commodore 64, compiled with CC65
 */

#include "../../lib/tui.h"
#include "../../lib/reu_mgr.h"
#include "../../lib/reu_control_bank.h"
#include "../../lib/reu_phys.h"
#include "../../lib/resume_state.h"
#include <c64.h>
#include <conio.h>

/*---------------------------------------------------------------------------
 * Constants
 *---------------------------------------------------------------------------*/

#define TITLE_Y    0
#define GRID_Y     4
#define GRID_ROWS  16
#define GRID_COLS  16
#define DETAIL_Y   20
#define HELP_Y     22
#define STATUS_Y   24

/* Screen codes for bank type display */
#define CHAR_FREE   0x2E  /* '.' screen code */
#define CHAR_APP    0x01  /* 'A' screen code */
#define CHAR_CLIP   0x03  /* 'C' screen code */
#define CHAR_ALLOC  0x15  /* 'U' screen code (user/alloc) */
#define CHAR_RSVD   0x12  /* 'R' screen code (reserved app slot) */
#define CHAR_LAUNCH 0x0C  /* 'L' screen code */
#define CHAR_GLOBAL 0x07  /* 'G' screen code */
#define CHAR_SKIP   0x18  /* 'X' screen code */
#define CHAR_UNAV   0x2D  /* '-' screen code */
#define CHAR_RSC    0x13  /* 'S' screen code */
#define CHAR_RST    0x14  /* 'T' screen code */

#define SHIM_CURRENT_BANK ((unsigned char*)0xC834)

/*---------------------------------------------------------------------------
 * Static variables
 *---------------------------------------------------------------------------*/

static unsigned char running;
static unsigned char cursor_x;  /* 0-15 in grid */
static unsigned char cursor_y_pos;  /* 0-15 in grid */
static unsigned char reu_physical_banks;
static unsigned char control_header[8];
static unsigned char control_bank_ok;
static unsigned char control_bank_generation;
static unsigned char detail_rec[REUCB_RSRC_REC_SIZE];
static char detail_app_name[REUCB_APP_META_SIZE + 1u];

typedef struct {
    unsigned char cursor_x;
    unsigned char cursor_y_pos;
} ReuViewerResumeV1;

static ReuViewerResumeV1 resume_blob;
static unsigned char resume_ready;

static void resume_save_state(void) {
    if (!resume_ready) {
        return;
    }
    resume_blob.cursor_x = cursor_x;
    resume_blob.cursor_y_pos = cursor_y_pos;
    (void)resume_save(&resume_blob, sizeof(resume_blob));
}

static unsigned char resume_restore_state(void) {
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
    if (resume_blob.cursor_x >= GRID_COLS || resume_blob.cursor_y_pos >= GRID_ROWS) {
        return 0;
    }

    cursor_x = resume_blob.cursor_x;
    cursor_y_pos = resume_blob.cursor_y_pos;
    return 1;
}

static unsigned char bank_is_unavailable(unsigned char bank) {
    return reu_phys_is_unavailable(reu_physical_banks, bank);
}

static void reuviewer_read_control_bank_header(void) {
    reu_dma_fetch((unsigned int)control_header, REU_READYOS_GLOBAL_PHYSICAL(),
                  REUCB_HEADER_OFF, 8u);
    control_bank_ok = (unsigned char)(
        control_header[0] == REUCB_MAGIC0 &&
        control_header[1] == REUCB_MAGIC1 &&
        control_header[2] == REUCB_MAGIC2 &&
        control_header[3] == REUCB_MAGIC3 &&
        control_header[4] == REUCB_SCHEMA_VERSION);
    control_bank_generation = control_header[6];
    if (control_bank_ok) {
        reu_dma_fetch((unsigned int)&reu_physical_banks,
                      REU_READYOS_GLOBAL_PHYSICAL(),
                      (unsigned int)(REUCB_HEADER_OFF +
                                     REUCB_HEADER_PHYS_BANKS), 1u);
    } else {
        reu_physical_banks = reu_phys_count_from_alloc_table();
    }
}

static void reuviewer_fetch_app_name(unsigned char app_index) {
    if (!control_bank_ok || app_index >= REUCB_APP_META_COUNT) {
        detail_app_name[0] = 0;
        return;
    }
    reu_dma_fetch((unsigned int)detail_app_name, REU_READYOS_GLOBAL_PHYSICAL(),
                  (unsigned int)(REUCB_APP_META_OFF +
                                 ((unsigned int)app_index * REUCB_APP_META_SIZE)),
                  REUCB_APP_META_SIZE);
    detail_app_name[REUCB_APP_META_SIZE] = 0;
}

static unsigned char reuviewer_find_app_for_logical(unsigned char logical,
                                                    unsigned char *out_app) {
    unsigned char app_index;

    if (!control_bank_ok) {
        return 0u;
    }
    app_index = readyos_bank_read_byte((unsigned int)(REUCB_TOKEN_APP_OFF +
                                                       logical));
    if (app_index < REUCB_APP_REG_COUNT) {
        *out_app = app_index;
        return 1u;
    }
    return 0u;
}

static unsigned char reuviewer_find_logical_for_physical(unsigned char physical,
                                                         unsigned char *out_token) {
    unsigned char token;
    unsigned char status;

    for (token = 1u; token <= TUI_APP_BANK_MAX; ++token) {
        status = readyos_bank_read_byte((unsigned int)(REUCB_TOKEN_STATUS_OFF + token));
        if ((status & REUCB_TOKEN_VALID) &&
            readyos_bank_read_byte((unsigned int)(REUCB_SHIM_LOOKUP_OFF + token)) ==
                physical) {
            *out_token = token;
            return 1u;
        }
    }
    return 0u;
}

static unsigned char reuviewer_find_resource_for_bank(unsigned char bank) {
    unsigned char i;

    if (!control_bank_ok) {
        return 0u;
    }
    for (i = 0u; i < REUCB_RSRC_REC_COUNT; ++i) {
        reu_dma_fetch((unsigned int)detail_rec, REU_READYOS_GLOBAL_PHYSICAL(),
                      (unsigned int)(REUCB_RSRC_REC_OFF +
                                     ((unsigned int)i * REUCB_RSRC_REC_SIZE)),
                      REUCB_RSRC_REC_SIZE);
        if (detail_rec[0] != REUCB_NULL_REC && detail_rec[3] == bank) {
            return 1u;
        }
    }
    return 0u;
}

/*---------------------------------------------------------------------------
 * Drawing
 *---------------------------------------------------------------------------*/

static void draw_header(void) {
    TuiRect box = {0, TITLE_Y, 40, 3};
    tui_window_title(&box, "REU MEMORY MAP",
                     TUI_COLOR_LIGHTBLUE, TUI_COLOR_YELLOW);
}

static void draw_summary(void) {
    unsigned char free_count;

    free_count = reu_count_free();

    tui_puts(1, TITLE_Y + 1, "PHYS:", TUI_COLOR_WHITE);
    tui_print_uint(6, TITLE_Y + 1,
                   reu_phys_display_count(reu_physical_banks), TUI_COLOR_WHITE);

    tui_puts(12, TITLE_Y + 1, "RO:", TUI_COLOR_ORANGE);
    tui_print_uint(15, TITLE_Y + 1, *SHIM_READYOS_BANK, TUI_COLOR_ORANGE);

    tui_puts(20, TITLE_Y + 1, "FREE:", TUI_COLOR_GRAY2);
    tui_print_uint(25, TITLE_Y + 1, free_count, TUI_COLOR_GRAY2);

    tui_puts(31, TITLE_Y + 1, "CB:", TUI_COLOR_LIGHTGREEN);
    tui_puts(34, TITLE_Y + 1, control_bank_ok ? "OK" : "--",
             control_bank_ok ? TUI_COLOR_LIGHTGREEN : TUI_COLOR_LIGHTRED);
    tui_puts(37, TITLE_Y + 1, "G", TUI_COLOR_GRAY3);
    tui_print_uint(38, TITLE_Y + 1, control_bank_generation, TUI_COLOR_WHITE);
}

static void draw_grid(void) {
    unsigned char row, col;
    unsigned char bank;
    unsigned char type;
    unsigned char ch;
    unsigned char color;
    unsigned char screen_x, screen_y;
    unsigned int offset;

    /* Column headers: 0-F */
    for (col = 0; col < GRID_COLS; ++col) {
        tui_putc(4 + col * 2, GRID_Y - 1,
                 tui_ascii_to_screen("0123456789ABCDEF"[col]),
                 TUI_COLOR_GRAY3);
    }

    for (row = 0; row < GRID_ROWS; ++row) {
        screen_y = GRID_Y + row;

        /* Row header: 0x-Fx */
        tui_putc(1, screen_y,
                 tui_ascii_to_screen("0123456789ABCDEF"[row]),
                 TUI_COLOR_GRAY3);
        tui_putc(2, screen_y, tui_ascii_to_screen('x'), TUI_COLOR_GRAY3);

        for (col = 0; col < GRID_COLS; ++col) {
            bank = row * 16 + col;
            screen_x = 4 + col * 2;

            if (bank_is_unavailable(bank)) {
                ch = CHAR_UNAV;
                color = TUI_COLOR_GRAY3;
            } else {
                type = reu_bank_type(bank);
                switch (type) {
                case REU_APP_STATE:
                    ch = CHAR_APP;
                    color = TUI_COLOR_CYAN;
                    break;
                case REU_CLIPBOARD:
                    ch = CHAR_CLIP;
                    color = TUI_COLOR_YELLOW;
                    break;
                case REU_APP_ALLOC:
                    ch = CHAR_ALLOC;
                    color = TUI_COLOR_LIGHTGREEN;
                    break;
                case REU_RESERVED:
                    ch = CHAR_RSVD;
                    color = TUI_COLOR_LIGHTRED;
                    break;
                case REU_LAUNCHER:
                    ch = CHAR_LAUNCH;
                    color = TUI_COLOR_WHITE;
                    break;
                case REU_GLOBAL:
                    ch = CHAR_GLOBAL;
                    color = TUI_COLOR_YELLOW;
                    break;
                case REU_SKIPPED:
                    ch = CHAR_SKIP;
                    color = TUI_COLOR_ORANGE;
                    break;
                case REU_RS_CACHE:
                    ch = CHAR_RSC;
                    color = TUI_COLOR_LIGHTBLUE;
                    break;
                case REU_RS_SCRATCH:
                    ch = CHAR_RST;
                    color = TUI_COLOR_ORANGE;
                    break;
                case REU_RB_CORE:
                    ch = 'B';
                    color = TUI_COLOR_LIGHTGREEN;
                    break;
                case REU_RB_CODE:
                    ch = 'C';
                    color = TUI_COLOR_CYAN;
                    break;
                default:
                    ch = CHAR_FREE;
                    color = TUI_COLOR_GRAY2;
                    break;
                }
            }

            offset = (unsigned int)screen_y * 40 + screen_x;

            /* If this is the cursor position, show reversed */
            if (row == cursor_y_pos && col == cursor_x) {
                TUI_SCREEN[offset] = ch | 0x80;  /* Reverse */
                TUI_COLOR_RAM[offset] = TUI_COLOR_WHITE;
            } else {
                TUI_SCREEN[offset] = ch;
                TUI_COLOR_RAM[offset] = color;
            }
        }
    }
}

static void draw_detail(void) {
    unsigned char bank;
    unsigned char logical;
    unsigned char app_index;
    unsigned char type;
    unsigned int rec_off;
    const char *type_str;

    bank = cursor_y_pos * 16 + cursor_x;
    type = bank_is_unavailable(bank) ? REU_UNAVAIL : reu_bank_type(bank);

    tui_clear_line(DETAIL_Y, 0, 40, TUI_COLOR_WHITE);
    tui_clear_line(DETAIL_Y + 1, 0, 40, TUI_COLOR_WHITE);
    tui_clear_line(HELP_Y, 0, 40, TUI_COLOR_WHITE);

    tui_puts(1, DETAIL_Y, "PHYS ", TUI_COLOR_WHITE);
    tui_print_hex8(6, DETAIL_Y, bank, TUI_COLOR_CYAN);

    tui_puts(12, DETAIL_Y, "TYPE: ", TUI_COLOR_WHITE);

    switch (type) {
        case REU_FREE:      type_str = "FREE"; break;
        case REU_APP_STATE: type_str = "APP STATE"; break;
        case REU_CLIPBOARD: type_str = "CLIPBOARD"; break;
        case REU_APP_ALLOC: type_str = "APP ALLOC"; break;
        case REU_RESERVED:  type_str = "APP SLOT RSV"; break;
        case REU_LAUNCHER:  type_str = "LAUNCHER"; break;
        case REU_GLOBAL:    type_str = "READYOS BANK"; break;
        case REU_SKIPPED:   type_str = "SKIPPED"; break;
        case REU_UNAVAIL:   type_str = "UNAVAILABLE"; break;
        case REU_RS_CACHE:  type_str = "RS CACHE"; break;
        case REU_RS_SCRATCH:type_str = "RS STATE/SCRATCH"; break;
        case REU_RB_CORE:   type_str = "RB CORE/SYSTEM"; break;
        case REU_RB_CODE:   type_str = "RB COMMAND CODE"; break;
        default:            type_str = "UNKNOWN"; break;
    }
    tui_puts(18, DETAIL_Y, type_str, TUI_COLOR_YELLOW);

    tui_puts(1, DETAIL_Y + 1, "DEC: ", TUI_COLOR_GRAY3);
    tui_print_uint(6, DETAIL_Y + 1, bank, TUI_COLOR_WHITE);
    tui_puts(12, DETAIL_Y + 1, "LOG: ", TUI_COLOR_GRAY3);
    if (!bank_is_unavailable(bank) &&
        reuviewer_find_logical_for_physical(bank, &logical)) {
        tui_print_hex8(17, DETAIL_Y + 1, logical, TUI_COLOR_CYAN);
        if (reuviewer_find_app_for_logical(logical, &app_index)) {
            reuviewer_fetch_app_name(app_index);
            tui_puts(22, DETAIL_Y + 1, "APP", TUI_COLOR_GRAY3);
            tui_print_uint(26, DETAIL_Y + 1, app_index, TUI_COLOR_WHITE);
            tui_puts_n(30, DETAIL_Y + 1, detail_app_name, 10, TUI_COLOR_YELLOW);
        }
    } else {
        tui_puts(17, DETAIL_Y + 1, "--", TUI_COLOR_GRAY3);
    }

    if (reuviewer_find_resource_for_bank(bank)) {
        reuviewer_fetch_app_name(detail_rec[0]);
        rec_off = (unsigned int)detail_rec[4] |
                  ((unsigned int)detail_rec[5] << 8);
        tui_puts(0, HELP_Y, "OWNER:", TUI_COLOR_GRAY3);
        tui_puts_n(7, HELP_Y, detail_app_name, 10, TUI_COLOR_YELLOW);
        tui_puts(18, HELP_Y, "SLOT", TUI_COLOR_GRAY3);
        tui_print_uint(23, HELP_Y, detail_rec[10], TUI_COLOR_WHITE);
        if (detail_rec[2] == REUCB_DEP_KIND_APP_ALLOC) {
            tui_puts(27, HELP_Y, "TAG", TUI_COLOR_GRAY3);
            tui_puts_n(31, HELP_Y, (const char *)&detail_rec[12], 4u,
                       TUI_COLOR_CYAN);
        } else {
            tui_puts(27, HELP_Y, "OFF", TUI_COLOR_GRAY3);
            tui_print_hex16(31, HELP_Y, rec_off, TUI_COLOR_CYAN);
        }
    }
}

static void draw_help(void) {
    tui_puts(0, HELP_Y + 1, "S/D:RS  F2/F4:APPS  CTRL+B", TUI_COLOR_GRAY3);
}

static void draw_status(void) {
    unsigned char free_count = reu_count_free();
    tui_puts_n(0, STATUS_Y, "FREE: ", 6, TUI_COLOR_GRAY3);
    tui_print_uint(6, STATUS_Y, free_count, TUI_COLOR_WHITE);
    tui_puts(10, STATUS_Y, "/ PHYS ", TUI_COLOR_GRAY3);
    tui_print_uint(17, STATUS_Y,
                   reu_phys_display_count(reu_physical_banks), TUI_COLOR_GRAY3);
    tui_puts(25, STATUS_Y, "CBGEN:", TUI_COLOR_GRAY3);
    tui_print_uint(31, STATUS_Y, control_bank_generation, TUI_COLOR_WHITE);
}

static void reuviewer_draw(void) {
    tui_clear(TUI_COLOR_BLUE);
    draw_header();
    draw_summary();
    draw_grid();
    draw_detail();
    draw_help();
    draw_status();
}

/*---------------------------------------------------------------------------
 * Main loop
 *---------------------------------------------------------------------------*/

static void reuviewer_loop(void) {
    unsigned char key;
    unsigned char nav_action;

    reuviewer_draw();

    while (running) {
        key = tui_getkey();
        nav_action = tui_handle_global_hotkey(key, *SHIM_CURRENT_BANK, 1);
        if (nav_action == TUI_HOTKEY_LAUNCHER) {
            resume_save_state();
            tui_return_to_launcher();
        }
        if (nav_action >= TUI_APP_BANK_MIN && nav_action <= TUI_APP_BANK_MAX) {
            resume_save_state();
            tui_switch_to_app(nav_action);
            continue;
        }
        if (nav_action == TUI_HOTKEY_BIND_ONLY) {
            continue;
        }

        switch (key) {
            case TUI_KEY_UP:
                if (cursor_y_pos > 0) {
                    --cursor_y_pos;
                    draw_grid();
                    draw_detail();
                }
                break;

            case TUI_KEY_DOWN:
                if (cursor_y_pos < GRID_ROWS - 1) {
                    ++cursor_y_pos;
                    draw_grid();
                    draw_detail();
                }
                break;

            case TUI_KEY_LEFT:
                if (cursor_x > 0) {
                    --cursor_x;
                    draw_grid();
                    draw_detail();
                }
                break;

            case TUI_KEY_RIGHT:
                if (cursor_x < GRID_COLS - 1) {
                    ++cursor_x;
                    draw_grid();
                    draw_detail();
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
    reu_control_bank_sync_and_mirror(REUCB_WRITER_REUVIEWER);
    reuviewer_read_control_bank_header();

    resume_ready = 0;
    bank = *SHIM_CURRENT_BANK;
    if (bank >= TUI_APP_BANK_MIN && bank <= TUI_APP_BANK_MAX) {
        resume_init_for_app(bank, bank, RESUME_SCHEMA_V1);
        resume_ready = 1;
    }

    if (!resume_restore_state()) {
        cursor_x = 0;
        cursor_y_pos = 0;
    }
    running = 1;

    reuviewer_loop();
    return 0;
}
