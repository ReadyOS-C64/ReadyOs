 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
typedef int ptrdiff_t;
typedef char wchar_t;
typedef unsigned size_t;
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
struct __vic2 {
union {
struct {
unsigned char spr0_x; 
unsigned char spr0_y; 
unsigned char spr1_x; 
unsigned char spr1_y; 
unsigned char spr2_x; 
unsigned char spr2_y; 
unsigned char spr3_x; 
unsigned char spr3_y; 
unsigned char spr4_x; 
unsigned char spr4_y; 
unsigned char spr5_x; 
unsigned char spr5_y; 
unsigned char spr6_x; 
unsigned char spr6_y; 
unsigned char spr7_x; 
unsigned char spr7_y; 
};
struct {
unsigned char x; 
unsigned char y; 
} spr_pos[8];
};
unsigned char spr_hi_x; 
unsigned char ctrl1; 
unsigned char rasterline; 
union {
struct {
unsigned char strobe_x; 
unsigned char strobe_y; 
};
struct {
unsigned char x; 
unsigned char y; 
} strobe;
};
unsigned char spr_ena; 
unsigned char ctrl2; 
unsigned char spr_exp_y; 
unsigned char addr; 
unsigned char irr; 
unsigned char imr; 
unsigned char spr_bg_prio; 
unsigned char spr_mcolor; 
unsigned char spr_exp_x; 
unsigned char spr_coll; 
unsigned char spr_bg_coll; 
unsigned char bordercolor; 
union {
struct {
unsigned char bgcolor0; 
unsigned char bgcolor1; 
unsigned char bgcolor2; 
unsigned char bgcolor3; 
};
unsigned char bgcolor[4]; 
};
union {
struct {
unsigned char spr_mcolor0; 
unsigned char spr_mcolor1; 
};
 
unsigned char spr_mcolors[2]; 
};
union {
struct {
unsigned char spr0_color; 
unsigned char spr1_color; 
unsigned char spr2_color; 
unsigned char spr3_color; 
unsigned char spr4_color; 
unsigned char spr5_color; 
unsigned char spr6_color; 
unsigned char spr7_color; 
};
unsigned char spr_color[8]; 
};
 
unsigned char x_kbd; 
unsigned char clock; 
};
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
struct __sid_voice {
unsigned freq; 
unsigned pw; 
unsigned char ctrl; 
unsigned char ad; 
unsigned char sr; 
};
struct __sid {
struct __sid_voice v1; 
struct __sid_voice v2; 
struct __sid_voice v3; 
unsigned flt_freq; 
unsigned char flt_ctrl; 
unsigned char amp; 
unsigned char ad1; 
unsigned char ad2; 
unsigned char noise; 
unsigned char read3; 
};
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
struct __6526 {
unsigned char pra; 
unsigned char prb; 
unsigned char ddra; 
unsigned char ddrb; 
unsigned char ta_lo; 
unsigned char ta_hi; 
unsigned char tb_lo; 
unsigned char tb_hi; 
unsigned char tod_10; 
unsigned char tod_sec; 
unsigned char tod_min; 
unsigned char tod_hour; 
unsigned char sdr; 
unsigned char icr; 
unsigned char cra; 
unsigned char crb; 
};
 
 
 
 
 
 
 
extern void c64_65816_emd[];
extern void c64_c256k_emd[];
extern void c64_dqbb_emd[];
extern void c64_georam_emd[];
extern void c64_isepic_emd[];
extern void c64_ram_emd[];
extern void c64_ramcart_emd[];
extern void c64_reu_emd[];
extern void c64_vdc_emd[];
extern void dtv_himem_emd[];
extern void c64_hitjoy_joy[];
extern void c64_numpad_joy[];
extern void c64_ptvjoy_joy[];
extern void c64_stdjoy_joy[]; 
extern void c64_1351_mou[]; 
extern void c64_joy_mou[];
extern void c64_inkwell_mou[];
extern void c64_pot_mou[];
extern void c64_swlink_ser[];
extern void c64_hi_tgi[]; 
 
 
 
unsigned char get_ostype (void);
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
unsigned char __fastcall__ _cbm_filetype (unsigned char c);
 
 
 
 
 
 
extern char _filetype; 
 
 
 
 
 
 
 
struct cbm_dirent {
char name[17]; 
unsigned int size; 
unsigned char type;
unsigned char access;
};
 
 
 
unsigned char get_tv (void);
 
unsigned char __fastcall__ kbrepeat (unsigned char mode);
 
void waitvsync (void);
 
 
 
 
 
 
unsigned char cbm_k_acptr (void);
unsigned char cbm_k_basin (void);
void __fastcall__ cbm_k_bsout (unsigned char C);
unsigned char __fastcall__ cbm_k_chkin (unsigned char FN);
void __fastcall__ cbm_k_ciout (unsigned char C);
unsigned char __fastcall__ cbm_k_ckout (unsigned char FN);
void cbm_k_clall (void);
void __fastcall__ cbm_k_close (unsigned char FN);
void cbm_k_clrch (void);
unsigned char cbm_k_getin (void);
unsigned cbm_k_iobase (void);
void __fastcall__ cbm_k_listen (unsigned char dev);
unsigned int __fastcall__ cbm_k_load(unsigned char flag, unsigned addr);
unsigned char cbm_k_open (void);
unsigned char cbm_k_readst (void);
unsigned char __fastcall__ cbm_k_save(unsigned int start, unsigned int end);
void cbm_k_scnkey (void);
void __fastcall__ cbm_k_second (unsigned char addr);
void __fastcall__ cbm_k_setlfs (unsigned char LFN, unsigned char DEV,
unsigned char SA);
void __fastcall__ cbm_k_setnam (const char* Name);
void __fastcall__ cbm_k_settim (unsigned long timer);
void __fastcall__ cbm_k_talk (unsigned char dev);
void __fastcall__ cbm_k_tksa (unsigned char addr);
void cbm_k_udtim (void);
void cbm_k_unlsn (void);
void cbm_k_untlk (void);
 
 
 
 
unsigned int __fastcall__ cbm_load (const char* name, unsigned char device, void* data);
 
unsigned char __fastcall__ cbm_save (const char* name, unsigned char device,
const void* addr, unsigned int size);
 
unsigned char __fastcall__ cbm_open (unsigned char lfn, unsigned char device,
unsigned char sec_addr, const char* name);
 
void __fastcall__ cbm_close (unsigned char lfn);
 
int __fastcall__ cbm_read (unsigned char lfn, void* buffer, unsigned int size);
 
int __fastcall__ cbm_write (unsigned char lfn, const void* buffer,
unsigned int size);
 
unsigned char cbm_opendir (unsigned char lfn, unsigned char device, ...);
 
unsigned char __fastcall__ cbm_readdir (unsigned char lfn,
struct cbm_dirent* l_dirent);
 
void __fastcall__ cbm_closedir (unsigned char lfn);
 
 
 
 
 
 
 
 
 
 
 
 
 
typedef struct {
unsigned char x;
unsigned char y;
unsigned char w;
unsigned char h;
} TuiRect;
 
typedef struct {
unsigned char x;
unsigned char y;
unsigned char width;
unsigned char maxlen;
unsigned char cursor;
unsigned char color;
char *buffer;
} TuiInput;
 
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
 
 
void tui_init(void);
 
void tui_clear(unsigned char bg_color);
 
void tui_gotoxy(unsigned char x, unsigned char y);
 
 
void tui_putc(unsigned char x, unsigned char y, unsigned char ch, unsigned char color);
 
void tui_puts(unsigned char x, unsigned char y, const char *str, unsigned char color);
 
void tui_puts_n(unsigned char x, unsigned char y, const char *str,
unsigned char maxlen, unsigned char color);
 
void tui_hline(unsigned char x, unsigned char y, unsigned char len, unsigned char color);
 
void tui_vline(unsigned char x, unsigned char y, unsigned char len, unsigned char color);
 
void tui_fill_rect(const TuiRect *rect, unsigned char ch, unsigned char color);
 
void tui_clear_rect(const TuiRect *rect, unsigned char color);
 
void tui_clear_line(unsigned char y, unsigned char x, unsigned char len, unsigned char color);
 
 
void tui_window(const TuiRect *rect, unsigned char border_color);
 
void tui_window_title(const TuiRect *rect, const char *title,
unsigned char border_color, unsigned char title_color);
 
void tui_panel(const TuiRect *rect, const char *title, unsigned char bg_color);
 
 
void tui_status_bar(const char *text, unsigned char color);
 
void tui_status_bar_multi(const char **items, unsigned char count, unsigned char color);
 
 
void tui_menu_init(TuiMenu *menu, unsigned char x, unsigned char y,
unsigned char w, unsigned char h,
const char **items, unsigned char count);
 
void tui_menu_draw(TuiMenu *menu);
 
unsigned char tui_menu_input(TuiMenu *menu, unsigned char key);
 
unsigned char tui_menu_selected(TuiMenu *menu);
 
 
void tui_input_init(TuiInput *input, unsigned char x, unsigned char y,
unsigned char width, unsigned char maxlen,
char *buffer, unsigned char color);
 
void tui_input_draw(TuiInput *input);
 
unsigned char tui_input_key(TuiInput *input, unsigned char key);
 
void tui_input_clear(TuiInput *input);
 
 
void tui_textarea_init(TuiTextArea *area, unsigned char x, unsigned char y,
unsigned char w, unsigned char h,
const char **lines, unsigned int line_count);
 
void tui_textarea_draw(TuiTextArea *area);
 
void tui_textarea_scroll(TuiTextArea *area, int delta);
 
 
unsigned char tui_getkey(void);
 
unsigned char tui_kbhit(void);
 
unsigned char tui_get_modifiers(void);
 
unsigned char tui_is_back_pressed(void);
 
void tui_return_to_launcher(void);
 
void tui_switch_to_app(unsigned char bank);
 
unsigned char tui_get_next_app(unsigned char current_bank);
unsigned char tui_get_prev_app(unsigned char current_bank);
 
unsigned char tui_handle_global_hotkey(unsigned char key,
unsigned char current_bank,
unsigned char allow_bind);
 
 
void tui_progress_bar(unsigned char x, unsigned char y, unsigned char width,
unsigned char filled, unsigned char total,
unsigned char fill_color, unsigned char empty_color);
 
unsigned char tui_ascii_to_screen(unsigned char ascii);
 
void tui_str_to_screen(char *str);
 
void tui_print_uint(unsigned char x, unsigned char y, unsigned int value,
unsigned char color);
 
void tui_print_hex8(unsigned char x, unsigned char y, unsigned char value,
unsigned char color);
 
void tui_print_hex16(unsigned char x, unsigned char y, unsigned int value,
unsigned char color);
 
 
 
 
 
 
 
void reu_mgr_init(void);
 
unsigned char reu_alloc_bank(unsigned char type);
 
void reu_free_bank(unsigned char bank);
 
unsigned char reu_bank_type(unsigned char bank);
 
unsigned char reu_count_free(void);
 
unsigned char reu_count_type(unsigned char type);
 
void reu_dma_stash(unsigned int c64_addr, unsigned char bank,
unsigned int reu_offset, unsigned int length);
void reu_dma_fetch(unsigned int c64_addr, unsigned char bank,
unsigned int reu_offset, unsigned int length);
 
typedef struct {
const void *ptr;
unsigned int len;
} ResumeWriteSegment;
typedef struct {
void *ptr;
unsigned int len;
} ResumeReadSegment;
 
void resume_init_for_app(unsigned char bank, unsigned char app_id,
unsigned char schema_version);
 
unsigned char resume_try_load(void *dst, unsigned int dst_len,
unsigned int *out_len);
 
unsigned char resume_save(const void *src, unsigned int src_len);
 
unsigned char resume_save_segments(const ResumeWriteSegment *segments,
unsigned char segment_count);
unsigned char resume_load_segments(const ResumeReadSegment *segments,
unsigned char segment_count,
unsigned int *out_len);
 
void resume_invalidate(void);
 
 
static const unsigned char readyos_easyflash_app_banks[16] = {
1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16
};
static const unsigned char readyos_easyflash_default_slots[16] = {
1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};
static const char *const readyos_easyflash_prgs[16] = {
"editor",
"readyshell",
"simplefiles",
"clipmgr",
"cal26",
"tasklist",
"reuviewer",
"quicknotes",
"calcplus",
"hexview",
"simplecells",
"game2048",
"sidetris",
"deminer",
"dizzy",
"readme",
};
static const char *const readyos_easyflash_labels[16] = {
"editor",
"readyshell (beta)",
"simple files",
"clipboard",
"calendar 26",
"task list",
"reu viewer",
"quicknotes",
"calc plus",
"hex viewer",
"simple cells (alpha)",
"2048 game",
"sidetris",
"deminer",
"dizzy kanban",
"read.me",
};
static const char *const readyos_easyflash_descs[16] = {
"text editor with clipboard",
"command shell poc scaffold",
"dual pane file manager and seq viewer",
"manage clipboard items",
"calendar for 2026 with appointments",
"outliner with notes and search",
"view reu memory usage",
"reu-backed note editor",
"keyboard-first expression calculator",
"browse memory in hex format",
"spreadsheet with formulas and colors",
"2048 tile puzzle",
"sideways tetris",
"minesweeper-style puzzle",
"fizzy inspired kanban app",
"readyos, ultimate buddy, precog poc os",
};
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
typedef unsigned char* va_list;
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
void clrscr (void);
 
unsigned char kbhit (void);
 
void __fastcall__ gotox (unsigned char x);
 
void __fastcall__ gotoy (unsigned char y);
 
void __fastcall__ gotoxy (unsigned char x, unsigned char y);
 
unsigned char wherex (void);
 
unsigned char wherey (void);
 
void __fastcall__ cputc (char c);
 
void __fastcall__ cputcxy (unsigned char x, unsigned char y, char c);
 
void __fastcall__ cputs (const char* s);
 
void __fastcall__ cputsxy (unsigned char x, unsigned char y, const char* s);
 
int cprintf (const char* format, ...);
 
int __fastcall__ vcprintf (const char* format, va_list ap);
 
char cgetc (void);
 
int cscanf (const char* format, ...);
 
int __fastcall__ vcscanf (const char* format, va_list ap);
 
char cpeekc (void);
 
unsigned char cpeekcolor (void);
 
unsigned char cpeekrevers (void);
 
void __fastcall__ cpeeks (char* s, unsigned int length);
 
unsigned char __fastcall__ cursor (unsigned char onoff);
 
unsigned char __fastcall__ revers (unsigned char onoff);
 
unsigned char __fastcall__ textcolor (unsigned char color);
 
unsigned char __fastcall__ bgcolor (unsigned char color);
 
unsigned char __fastcall__ bordercolor (unsigned char color);
 
void __fastcall__ chline (unsigned char length);
 
void __fastcall__ chlinexy (unsigned char x, unsigned char y, unsigned char length);
 
void __fastcall__ cvline (unsigned char length);
 
void __fastcall__ cvlinexy (unsigned char x, unsigned char y, unsigned char length);
 
void __fastcall__ cclear (unsigned char length);
 
void __fastcall__ cclearxy (unsigned char x, unsigned char y, unsigned char length);
 
void __fastcall__ screensize (unsigned char* x, unsigned char* y);
 
void __fastcall__ cputhex8 (unsigned char val);
void __fastcall__ cputhex16 (unsigned val);
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
 
char* __fastcall__ strcat (char* dest, const char* src);
char* __fastcall__ strchr (const char* s, int c);
int __fastcall__ strcmp (const char* s1, const char* s2);
int __fastcall__ strcoll (const char* s1, const char* s2);
char* __fastcall__ strcpy (char* dest, const char* src);
size_t __fastcall__ strcspn (const char* s1, const char* s2);
char* __fastcall__ strerror (int errcode);
size_t __fastcall__ strlen (const char* s);
char* __fastcall__ strncat (char* s1, const char* s2, size_t count);
int __fastcall__ strncmp (const char* s1, const char* s2, size_t count);
char* __fastcall__ strncpy (char* dest, const char* src, size_t count);
char* __fastcall__ strpbrk (const char* str, const char* set);
char* __fastcall__ strrchr (const char* s, int c);
size_t __fastcall__ strspn (const char* s1, const char* s2);
char* __fastcall__ strstr (const char* str, const char* substr);
char* __fastcall__ strtok (char* s1, const char* s2);
size_t __fastcall__ strxfrm (char* s1, const char* s2, size_t count);
void* __fastcall__ memchr (const void* mem, int c, size_t count);
int __fastcall__ memcmp (const void* p1, const void* p2, size_t count);
void* __fastcall__ memcpy (void* dest, const void* src, size_t count);
void* __fastcall__ memmove (void* dest, const void* src, size_t count);
void* __fastcall__ memset (void* s, int c, size_t count);
 
void* __fastcall__ _bzero (void* ptr, size_t n);
 
void __fastcall__ bzero (void* ptr, size_t n); 
char* __fastcall__ strdup (const char* s); 
int __fastcall__ stricmp (const char* s1, const char* s2); 
int __fastcall__ strcasecmp (const char* s1, const char* s2); 
int __fastcall__ strnicmp (const char* s1, const char* s2, size_t count); 
int __fastcall__ strncasecmp (const char* s1, const char* s2, size_t count); 
char* __fastcall__ strlwr (char* s);
char* __fastcall__ strlower (char* s);
char* __fastcall__ strupr (char* s);
char* __fastcall__ strupper (char* s);
char* __fastcall__ strqtok (char* s1, const char* s2);
const char* __fastcall__ _stroserror (unsigned char errcode);
 
 
 
 
 
 
 
 
 
 
 
 
 
static const char *app_names[24];
static const char *app_descs[24];
static const char *app_files[24];
static unsigned char app_banks[24];
static unsigned char app_drives[24];
static unsigned char app_default_slots[24];
static char app_name_buf[24][31 + 1];
static char app_desc_buf[24][38 + 1];
static char app_file_buf[24][12 + 1];
static unsigned char app_count;
typedef struct {
unsigned char selected;
unsigned char scroll_offset;
unsigned char suppress_startup_once;
unsigned char reserved;
} LauncherResumeV1;
typedef struct {
unsigned char app_count;
unsigned char app_banks[24];
unsigned char app_drives[24];
unsigned char app_default_slots[24];
char app_name_buf[24][31 + 1];
char app_desc_buf[24][38 + 1];
char app_file_buf[24][12 + 1];
unsigned char launcher_cfg_load_all_to_reu;
char launcher_variant_name[31 + 1];
char launcher_variant_boot_name[31 + 1];
char launcher_runappfirst_prg[12 + 1];
} LauncherCatalogCacheV1;
 
static unsigned char apps_loaded[24];
static unsigned int app_sizes[24];
static LauncherResumeV1 launcher_resume_blob;
static LauncherCatalogCacheV1 launcher_catalog_cache;
static unsigned char resume_ready;
static unsigned char launcher_cfg_load_all_to_reu;
static char launcher_variant_name[31 + 1];
static char launcher_variant_boot_name[31 + 1];
static char launcher_runappfirst_prg[12 + 1];
static char launcher_notice[38 + 1];
static unsigned char launcher_notice_color = 15;
static const unsigned char bit_masks[8] = { 1u, 2u, 4u, 8u, 16u, 32u, 64u, 128u };
 
static TuiMenu menu;
static unsigned char running;
static unsigned char slot_contract_ok = 1;
static unsigned char cfg_err_phase = 0;
 
static void draw_drive_field(unsigned int screen_offset, unsigned char drive);
static void draw_drive_prefixed_name(unsigned char x,
unsigned char y,
unsigned char index,
unsigned char name_color,
unsigned char name_maxlen);
static void launcher_sync_visible_window(void);
static unsigned char validate_slot_contract(unsigned char *detail_a,
unsigned char *detail_b,
unsigned char *detail_c);
static unsigned char load_all_to_reu_internal(unsigned char interactive);
static void launch_app(unsigned char index);
static void launcher_force_text_mode(void);
static void launcher_dbg_draw_overlay(void);
static void launcher_easyflash_tui_bootstrap(void);
static void launcher_draw_visual_probe(void);
static unsigned char launcher_dbg_reu_byte;
static unsigned char launcher_dbg_reu_head_buf[2];
static unsigned int launcher_dbg_reu_pos;
static unsigned char launcher_dbg_ram_pos;
static unsigned char launcher_dbg_screen_pos;
static char launcher_dbg_line0[39];
static char launcher_dbg_line1[39];
static unsigned char launcher_dbg_screen_code(unsigned char code) {
if (code >= 'A' && code <= 'Z') {
return (unsigned char)(code - 'A' + 1u);
}
if (code >= 'a' && code <= 'z') {
return (unsigned char)(code - 'a' + 1u);
}
if (code >= '0' && code <= '9') {
return code;
}
if (code == ' ') {
return 0x20u;
}
if (code == '=') {
return 0x3Du;
}
if (code == ':') {
return 0x3Au;
}
if (code == '-') {
return 0x2Du;
}
return 0x2Eu;
}
static void launcher_dbg_screen_clear(void) {
unsigned char i;
for (i = 0; i < 40u; ++i) {
((unsigned char*)0x07C0)[i] = 0x20u;
((unsigned char*)0xDBC0)[i] = 7;
}
launcher_dbg_screen_pos = 0u;
}
static void launcher_dbg_screen_put(unsigned char code) {
if (launcher_dbg_screen_pos >= 40u) {
launcher_dbg_screen_pos = 0u;
}
((unsigned char*)0x07C0)[launcher_dbg_screen_pos] = launcher_dbg_screen_code(code);
((unsigned char*)0xDBC0)[launcher_dbg_screen_pos] = 7;
++launcher_dbg_screen_pos;
}
static void launcher_dbg_reset(void) {
unsigned char i;
launcher_dbg_reu_pos = 0;
launcher_dbg_ram_pos = 0;
for (i = 0; i < 0x3Fu; ++i) {
((unsigned char*)0xC7A0)[i] = 0;
}
(*(unsigned char*)0xC7DF) = 0;
(*(unsigned char*)0xC7EC) = 0;
(*(unsigned char*)0xC7ED) = 0;
launcher_dbg_screen_clear();
launcher_dbg_reu_head_buf[0] = 0;
launcher_dbg_reu_head_buf[1] = 0;
}
static void launcher_dbg_put(unsigned char code) {
((unsigned char*)0xC7A0)[launcher_dbg_ram_pos] = code;
(*(unsigned char*)0xC7ED) = code;
launcher_dbg_screen_put(code);
++launcher_dbg_ram_pos;
if (launcher_dbg_ram_pos >= 0x3Fu) {
launcher_dbg_ram_pos = 0;
}
(*(unsigned char*)0xC7DF) = launcher_dbg_ram_pos;
}
static void launcher_dbg_kv(unsigned char tag, unsigned char value) {
launcher_dbg_put(tag);
launcher_dbg_put(value);
}
static char launcher_dbg_hex(unsigned char value) {
value &= 0x0Fu;
if (value < 10u) {
return (char)('0' + value);
}
return (char)('a' + (value - 10u));
}
static void launcher_dbg_draw_overlay(void) {
unsigned char i;
unsigned char pos = 0;
unsigned char cursor;
unsigned char src;
unsigned char code;
launcher_dbg_line0[0] = 'D';
launcher_dbg_line0[1] = 'B';
launcher_dbg_line0[2] = 'G';
launcher_dbg_line0[3] = ' ';
launcher_dbg_line0[4] = 'H';
launcher_dbg_line0[5] = '=';
launcher_dbg_line0[6] = launcher_dbg_hex((unsigned char)((*(unsigned char*)0xC7DF) >> 4));
launcher_dbg_line0[7] = launcher_dbg_hex((*(unsigned char*)0xC7DF));
launcher_dbg_line0[8] = ' ';
pos = 9;
cursor = (*(unsigned char*)0xC7DF);
for (i = 0; i < 7u && pos < 36u; ++i) {
if (cursor == 0u) {
src = (unsigned char)(0x3Fu - 1u);
} else {
src = (unsigned char)(cursor - 1u);
}
code = ((unsigned char*)0xC7A0)[src];
launcher_dbg_line0[pos++] = launcher_dbg_hex((unsigned char)(code >> 4));
launcher_dbg_line0[pos++] = launcher_dbg_hex(code);
if (i != 6u) {
launcher_dbg_line0[pos++] = ' ';
}
cursor = src;
}
while (pos < 38u) {
launcher_dbg_line0[pos++] = ' ';
}
launcher_dbg_line0[38] = 0;
launcher_dbg_line1[0] = 'A';
launcher_dbg_line1[1] = '=';
launcher_dbg_line1[2] = launcher_dbg_hex((unsigned char)(app_count >> 4));
launcher_dbg_line1[3] = launcher_dbg_hex(app_count);
launcher_dbg_line1[4] = ' ';
launcher_dbg_line1[5] = 'M';
launcher_dbg_line1[6] = '=';
launcher_dbg_line1[7] = launcher_dbg_hex((unsigned char)(menu.count >> 4));
launcher_dbg_line1[8] = launcher_dbg_hex(menu.count);
launcher_dbg_line1[9] = ' ';
launcher_dbg_line1[10] = 'S';
launcher_dbg_line1[11] = '=';
launcher_dbg_line1[12] = launcher_dbg_hex((unsigned char)(menu.selected >> 4));
launcher_dbg_line1[13] = launcher_dbg_hex(menu.selected);
launcher_dbg_line1[14] = ' ';
launcher_dbg_line1[15] = 'R';
launcher_dbg_line1[16] = '=';
launcher_dbg_line1[17] = launcher_dbg_hex((unsigned char)(resume_ready >> 4));
launcher_dbg_line1[18] = launcher_dbg_hex(resume_ready);
pos = 19;
while (pos < 38u) {
launcher_dbg_line1[pos++] = ' ';
}
launcher_dbg_line1[38] = 0;
tui_puts_n(1, 23, launcher_dbg_line0, 38, 7);
tui_puts_n(1, 24, launcher_dbg_line1, 38, 13);
}
static void launcher_dump_hex_byte(char *out, unsigned char value) {
out[0] = launcher_dbg_hex((unsigned char)(value >> 4));
out[1] = launcher_dbg_hex(value);
}
static void launcher_force_text_mode(void) {
(*(unsigned char*)0xDD02) = 0x03u;
(*(unsigned char*)0xDD00) = (unsigned char)(((*(unsigned char*)0xDD00) & 0xFCu) | 0x03u);
(*(unsigned char*)0xD011) = 0x1Bu;
(*(unsigned char*)0xD016) = 0x08u;
(*(unsigned char*)0xD018) = 0x14;
}
static void launcher_set_stage(unsigned char color) {
(void)color;
(*(unsigned char*)0xC7EC) = color;
}
void ef_linit_entry(void) { (*(unsigned char*)0xC7EC) = 0x21u; }
void ef_linit_before_bootstrap(void) { (*(unsigned char*)0xC7EC) = 0x22u; }
void ef_linit_after_bootstrap(void) { (*(unsigned char*)0xC7EC) = 0x23u; }
void ef_linit_before_catalog_defaults(void) { (*(unsigned char*)0xC7EC) = 0x24u; }
void ef_linit_after_catalog_defaults(void) { (*(unsigned char*)0xC7EC) = 0x25u; }
static void launcher_easyflash_tui_bootstrap(void) {
launcher_dbg_reset();
launcher_dbg_put('0');
launcher_set_stage(1u);
launcher_force_text_mode();
launcher_dbg_put('1');
tui_clear(6);
launcher_dbg_put('2');
launcher_dbg_put('3');
}
 
unsigned char tui_get_next_app(unsigned char current_bank) {
(void)current_bank;
return 0;
}
unsigned char tui_get_prev_app(unsigned char current_bank) {
(void)current_bank;
return 0;
}
 
static unsigned char shim_bitmap_has_bank(unsigned char bank) {
if (bank < 8) {
return (unsigned char)(*((unsigned char*)0xC836) & (unsigned char)(1U << bank));
}
if (bank < 16) {
return (unsigned char)(*((unsigned char*)0xC837) & (unsigned char)(1U << (bank - 8)));
}
if (bank < 24) {
return (unsigned char)(*((unsigned char*)0xC838) & (unsigned char)(1U << (bank - 16)));
}
return 0;
}
static void shim_bitmap_clear_bank(unsigned char bank) {
if (bank < 8) {
*((unsigned char*)0xC836) &= (unsigned char)~(unsigned char)(1U << bank);
} else if (bank < 16) {
*((unsigned char*)0xC837) &= (unsigned char)~(unsigned char)(1U << (bank - 8));
} else if (bank < 24) {
*((unsigned char*)0xC838) &= (unsigned char)~(unsigned char)(1U << (bank - 16));
}
}
 
static void sync_from_reu_bitmap(void) {
unsigned char i;
unsigned char bank;
unsigned char last_saved;
unsigned char bitmap_lo;
unsigned char bitmap_hi;
unsigned char bitmap_xhi;
unsigned char loaded;
for (i = 1; i < app_count; ++i) {
if (app_banks[i] != 0) {
apps_loaded[i] = 1;
app_sizes[i] = 0xB600;
} else {
apps_loaded[i] = 0;
app_sizes[i] = 0;
}
}
*((unsigned char*)0xC835) = 0xFF;
return;
 
last_saved = *((unsigned char*)0xC835);
if (last_saved < 24) {
if (last_saved < 8) {
*((unsigned char*)0xC836) |= bit_masks[last_saved];
} else if (last_saved < 16) {
*((unsigned char*)0xC837) |= bit_masks[(unsigned char)(last_saved - 8)];
} else {
*((unsigned char*)0xC838) |= bit_masks[(unsigned char)(last_saved - 16)];
}
}
bitmap_lo = *((unsigned char*)0xC836);
bitmap_hi = *((unsigned char*)0xC837);
bitmap_xhi = *((unsigned char*)0xC838);
for (i = 1; i < app_count; ++i) {
bank = app_banks[i];
loaded = 0;
if (bank < 8) {
loaded = (unsigned char)((bitmap_lo & bit_masks[bank]) != 0u);
} else if (bank < 16) {
loaded = (unsigned char)((bitmap_hi & bit_masks[(unsigned char)(bank - 8)]) != 0u);
} else if (bank < 24) {
loaded = (unsigned char)((bitmap_xhi & bit_masks[(unsigned char)(bank - 16)]) != 0u);
}
app_sizes[i] = loaded ? 0xB600 : 0;
}
 
*((unsigned char*)0xC835) = 0xFF;
}
 
static void compute_required_slot_bitmap(unsigned char *expected_lo,
unsigned char *expected_hi,
unsigned char *expected_xhi) {
unsigned char i;
unsigned char bank;
unsigned char lo = 0;
unsigned char hi = 0;
unsigned char xhi = 0;
for (i = 1; i < app_count; ++i) {
bank = app_banks[i];
if (bank < 8) {
lo |= (unsigned char)(1U << bank);
} else if (bank < 16) {
hi |= (unsigned char)(1U << (bank - 8));
} else if (bank < 24) {
xhi |= (unsigned char)(1U << (bank - 16));
}
}
*expected_lo = lo;
*expected_hi = hi;
*expected_xhi = xhi;
}
static unsigned char required_slots_loaded(void) {
unsigned char expected_lo;
unsigned char expected_hi;
unsigned char expected_xhi;
compute_required_slot_bitmap(&expected_lo, &expected_hi, &expected_xhi);
return (unsigned char)(((*((unsigned char*)0xC836) & expected_lo) == expected_lo) &&
((*((unsigned char*)0xC837) & expected_hi) == expected_hi) &&
((*((unsigned char*)0xC838) & expected_xhi) == expected_xhi));
}
static unsigned char is_space_char(char ch) {
return (unsigned char)(ch == ' ' || ch == '\t');
}
static void trim_in_place(char *s) {
unsigned int start = 0;
unsigned int len;
while (s[start] != 0 && is_space_char(s[start])) {
++start;
}
if (start != 0) {
memmove(s, s + start, strlen(s + start) + 1);
}
len = strlen(s);
while (len > 0 && is_space_char(s[len - 1])) {
--len;
}
s[len] = 0;
}
static void lowercase_in_place(char *s) {
unsigned int i;
for (i = 0; s[i] != 0; ++i) {
if (s[i] >= 'A' && s[i] <= 'Z') {
s[i] = (char)(s[i] + ('a' - 'A'));
}
}
}
static void copy_text_limit(char *dst, unsigned char cap, const char *src) {
strncpy(dst, src, (unsigned int)(cap - 1));
dst[cap - 1] = 0;
}
static unsigned char split_key_value(char *line, char **out_key, char **out_value) {
char *eq = strchr(line, '=');
if (eq == 0) {
return 0;
}
*eq = 0;
*out_key = line;
*out_value = eq + 1;
trim_in_place(*out_key);
trim_in_place(*out_value);
return 1;
}
static unsigned char is_blank_or_comment(const char *s) {
return (unsigned char)(s[0] == 0 || s[0] == '#' || s[0] == ';');
}
static unsigned char cfg_read_line(char *out, unsigned char cap) {
unsigned char ch;
unsigned char raw;
unsigned char len = 0;
int n;
while (1) {
n = cbm_read(12, &ch, 1);
if (n <= 0) {
if (len == 0) {
out[0] = 0;
return 0;
}
break;
}
raw = ch;
ch &= 0x7F;
if (raw == 0xA4 || ch == 0x5F) {
ch = '_';
}
if (ch == 0x0A && len == 0) {
continue;
}
if (ch == 0x0D || ch == 0x0A) {
break;
}
if (ch == 0) {
continue;
}
if (len < (unsigned char)(cap - 1)) {
out[len++] = (char)ch;
}
}
out[len] = 0;
return 1;
}
static unsigned char is_valid_prg_char(unsigned char ch) {
if (ch >= 'a' && ch <= 'z') return 1;
if (ch >= '0' && ch <= '9') return 1;
if (ch == '_' || ch == '-' || ch == '.') return 1;
return 0;
}
static unsigned char ends_with_suffix(const char *s, const char *suffix) {
unsigned int s_len = strlen(s);
unsigned int suf_len = strlen(suffix);
unsigned int i;
if (s_len < suf_len) {
return 0;
}
for (i = 0; i < suf_len; ++i) {
if ((unsigned char)s[s_len - suf_len + i] != (unsigned char)suffix[i]) {
return 0;
}
}
return 1;
}
static unsigned char normalize_prg_field(char *field_prg,
char *out_prg,
unsigned char *out_detail) {
unsigned char i;
unsigned char prg_len;
char *comma;
*out_detail = 0;
trim_in_place(field_prg);
prg_len = (unsigned char)strlen(field_prg);
if (prg_len == 0) {
((void)0);
return 6;
}
comma = strchr(field_prg, ',');
if (comma != 0) {
*out_detail = (unsigned char)comma[1];
((void)0);
return 6;
}
if (ends_with_suffix(field_prg, ".prg")) {
((void)0);
return 10;
}
prg_len = (unsigned char)strlen(field_prg);
if (prg_len == 0 || prg_len > 12) {
*out_detail = prg_len;
((void)0);
return 6;
}
for (i = 0; i < prg_len; ++i) {
if (!is_valid_prg_char((unsigned char)field_prg[i])) {
*out_detail = (unsigned char)field_prg[i];
((void)0);
return 6;
}
out_prg[i] = field_prg[i];
}
out_prg[prg_len] = 0;
return 0;
}
static void catalog_init_defaults(void) {
unsigned char i;
for (i = 0; i < 24; ++i) {
apps_loaded[i] = 0;
app_sizes[i] = 0;
app_banks[i] = 0;
app_drives[i] = 8;
app_default_slots[i] = 0;
app_name_buf[i][0] = 0;
app_desc_buf[i][0] = 0;
app_file_buf[i][0] = 0;
}
launcher_cfg_load_all_to_reu = 0;
launcher_variant_name[0] = 0;
launcher_variant_boot_name[0] = 0;
launcher_runappfirst_prg[0] = 0;
launcher_notice[0] = 0;
launcher_notice_color = 15;
copy_text_limit(launcher_variant_name, sizeof(launcher_variant_name), "readyos");
strcpy(app_name_buf[0], "ALL APPS PRELOADED");
strcpy(app_desc_buf[0], "EasyFlash preload complete");
app_count = 1;
cfg_err_phase = 0;
((void)0);
}
static void catalog_rebind_views(void) {
unsigned char i;
for (i = 0; i < 24; ++i) {
app_names[i] = app_name_buf[i];
app_descs[i] = app_desc_buf[i];
app_files[i] = app_file_buf[i];
}
}
static void launcher_snapshot_catalog_cache(void) {
memcpy(&launcher_catalog_cache.app_banks[0], &app_banks[0], sizeof(app_banks));
memcpy(&launcher_catalog_cache.app_drives[0], &app_drives[0], sizeof(app_drives));
memcpy(&launcher_catalog_cache.app_default_slots[0], &app_default_slots[0],
sizeof(app_default_slots));
memcpy(&launcher_catalog_cache.app_name_buf[0][0], &app_name_buf[0][0],
sizeof(app_name_buf));
memcpy(&launcher_catalog_cache.app_desc_buf[0][0], &app_desc_buf[0][0],
sizeof(app_desc_buf));
memcpy(&launcher_catalog_cache.app_file_buf[0][0], &app_file_buf[0][0],
sizeof(app_file_buf));
launcher_catalog_cache.app_count = app_count;
launcher_catalog_cache.launcher_cfg_load_all_to_reu = launcher_cfg_load_all_to_reu;
memcpy(&launcher_catalog_cache.launcher_variant_name[0], &launcher_variant_name[0],
sizeof(launcher_variant_name));
memcpy(&launcher_catalog_cache.launcher_variant_boot_name[0], &launcher_variant_boot_name[0],
sizeof(launcher_variant_boot_name));
memcpy(&launcher_catalog_cache.launcher_runappfirst_prg[0], &launcher_runappfirst_prg[0],
sizeof(launcher_runappfirst_prg));
}
static void launcher_restore_catalog_cache(void) {
memcpy(&app_banks[0], &launcher_catalog_cache.app_banks[0], sizeof(app_banks));
memcpy(&app_drives[0], &launcher_catalog_cache.app_drives[0], sizeof(app_drives));
memcpy(&app_default_slots[0], &launcher_catalog_cache.app_default_slots[0],
sizeof(app_default_slots));
memcpy(&app_name_buf[0][0], &launcher_catalog_cache.app_name_buf[0][0],
sizeof(app_name_buf));
memcpy(&app_desc_buf[0][0], &launcher_catalog_cache.app_desc_buf[0][0],
sizeof(app_desc_buf));
memcpy(&app_file_buf[0][0], &launcher_catalog_cache.app_file_buf[0][0],
sizeof(app_file_buf));
app_count = launcher_catalog_cache.app_count;
launcher_cfg_load_all_to_reu = launcher_catalog_cache.launcher_cfg_load_all_to_reu;
memcpy(&launcher_variant_name[0], &launcher_catalog_cache.launcher_variant_name[0],
sizeof(launcher_variant_name));
memcpy(&launcher_variant_boot_name[0], &launcher_catalog_cache.launcher_variant_boot_name[0],
sizeof(launcher_variant_boot_name));
memcpy(&launcher_runappfirst_prg[0], &launcher_catalog_cache.launcher_runappfirst_prg[0],
sizeof(launcher_runappfirst_prg));
}
static void launcher_resume_save(unsigned char selected,
unsigned char scroll_offset,
unsigned char suppress_startup_once) {
ResumeWriteSegment segs[2];
if (!resume_ready) {
return;
}
launcher_resume_blob.selected = selected;
launcher_resume_blob.scroll_offset = scroll_offset;
launcher_resume_blob.suppress_startup_once = suppress_startup_once;
launcher_resume_blob.reserved = 0;
launcher_snapshot_catalog_cache();
segs[0].ptr = &launcher_resume_blob;
segs[0].len = sizeof(launcher_resume_blob);
segs[1].ptr = &launcher_catalog_cache;
segs[1].len = sizeof(launcher_catalog_cache);
(void)resume_save_segments(segs, 2);
}
static unsigned char launcher_resume_restore(unsigned char *out_selected,
unsigned char *out_scroll_offset,
unsigned char *out_suppress_startup_once) {
unsigned int payload_len = 0;
ResumeReadSegment segs[2];
if (!resume_ready) {
return 0;
}
segs[0].ptr = &launcher_resume_blob;
segs[0].len = sizeof(launcher_resume_blob);
segs[1].ptr = &launcher_catalog_cache;
segs[1].len = sizeof(launcher_catalog_cache);
if (!resume_load_segments(segs, 2, &payload_len)) {
return 0;
}
if (payload_len != (sizeof(launcher_resume_blob) + sizeof(launcher_catalog_cache))) {
return 0;
}
launcher_restore_catalog_cache();
if (out_selected != 0 && launcher_resume_blob.selected < app_count) {
*out_selected = launcher_resume_blob.selected;
}
if (out_scroll_offset != 0) {
*out_scroll_offset = launcher_resume_blob.scroll_offset;
}
if (out_suppress_startup_once != 0) {
*out_suppress_startup_once = launcher_resume_blob.suppress_startup_once;
}
return 1;
}
static unsigned char parse_catalog_entry_line(char *line,
unsigned char *out_drive,
char *out_prg,
char *out_label,
unsigned char *out_default_slot,
unsigned char *out_detail) {
char *first_colon;
char *second_colon;
char *third_colon;
char *field_drive;
char *field_prg;
char *field_label;
char *field_slot = 0;
unsigned char i;
unsigned int drive_val = 0;
first_colon = strchr(line, ':');
if (first_colon == 0) {
((void)0);
return 2;
}
*first_colon = 0;
second_colon = strchr(first_colon + 1, ':');
if (second_colon == 0) {
((void)0);
return 2;
}
*second_colon = 0;
third_colon = strchr(second_colon + 1, ':');
if (third_colon != 0) {
*third_colon = 0;
field_slot = third_colon + 1;
if (strchr(field_slot, ':') != 0) {
((void)0);
return 11;
}
}
field_drive = line;
field_prg = first_colon + 1;
field_label = second_colon + 1;
trim_in_place(field_drive);
trim_in_place(field_prg);
trim_in_place(field_label);
if (field_slot != 0) {
trim_in_place(field_slot);
}
if (field_drive[0] == 0) {
((void)0);
return 5;
}
for (i = 0; field_drive[i] != 0; ++i) {
if (field_drive[i] < '0' || field_drive[i] > '9') {
*out_detail = (unsigned char)field_drive[i];
((void)0);
return 5;
}
drive_val = drive_val * 10U + (unsigned int)(field_drive[i] - '0');
}
if (drive_val < 8U || drive_val > 11U) {
*out_detail = (unsigned char)drive_val;
((void)0);
return 5;
}
*out_drive = (unsigned char)drive_val;
{
unsigned char norm_detail = 0;
unsigned char norm_rc = normalize_prg_field(field_prg, out_prg, &norm_detail);
if (norm_rc != 0) {
*out_detail = norm_detail;
return norm_rc;
}
}
if (field_label[0] == 0) {
((void)0);
return 7;
}
strncpy(out_label, field_label, 31);
out_label[31] = 0;
*out_default_slot = 0;
if (field_slot != 0) {
if (field_slot[0] == 0) {
((void)0);
return 11;
}
if (field_slot[1] != 0) {
*out_detail = (unsigned char)field_slot[1];
((void)0);
return 11;
}
if (field_slot[0] < '0' || field_slot[0] > '9') {
*out_detail = (unsigned char)field_slot[0];
((void)0);
return 11;
}
if (field_slot[0] == '0') {
*out_detail = 0;
((void)0);
return 11;
}
*out_default_slot = (unsigned char)(field_slot[0] - '0');
}
return 0;
}
static unsigned char add_catalog_entry(unsigned char bank,
unsigned char drive,
const char *prg,
const char *label,
const char *desc,
unsigned char default_slot) {
unsigned char idx;
if (app_count >= 24) {
((void)0);
return 4;
}
idx = app_count;
app_banks[idx] = bank;
app_drives[idx] = drive;
app_default_slots[idx] = default_slot;
strncpy(app_file_buf[idx], prg, 12);
app_file_buf[idx][12] = 0;
strncpy(app_name_buf[idx], label, 31);
app_name_buf[idx][31] = 0;
strncpy(app_desc_buf[idx], desc, 38);
app_desc_buf[idx][38] = 0;
++app_count;
return 0;
}
static unsigned char load_catalog_from_embedded(unsigned char *detail_a,
unsigned char *detail_b,
unsigned char *detail_c) {
unsigned char i;
unsigned char err;
unsigned char embedded_count = 16;
if (16 < embedded_count) {
embedded_count = 16;
}
copy_text_limit(launcher_variant_name, sizeof(launcher_variant_name),
"precog easyflash");
copy_text_limit(launcher_variant_boot_name, sizeof(launcher_variant_boot_name),
"precog easyflash");
if (""[0] != 0) {
copy_text_limit(launcher_runappfirst_prg, sizeof(launcher_runappfirst_prg),
"");
}
for (i = 0; i < embedded_count; ++i) {
err = add_catalog_entry(readyos_easyflash_app_banks[i],
8,
readyos_easyflash_prgs[i],
readyos_easyflash_labels[i],
readyos_easyflash_descs[i],
readyos_easyflash_default_slots[i]);
if (err != 0) {
*detail_a = i;
*detail_b = err;
*detail_c = app_count;
return err;
}
apps_loaded[(unsigned char)(app_count - 1)] = 1;
app_sizes[(unsigned char)(app_count - 1)] = 0xB600;
}
*detail_a = 0;
*detail_b = 0;
*detail_c = 0;
return 0;
}
static unsigned char load_catalog_from_disk(unsigned char *detail_a,
unsigned char *detail_b,
unsigned char *detail_c) {
char line[96];
char *key;
char *value;
char pending_prg[12 + 1];
char pending_label[31 + 1];
unsigned char pending_drive = 0;
unsigned char pending_slot = 0;
unsigned char entry_index = 1;
unsigned char err;
unsigned char parse_detail;
unsigned char section = 0;
unsigned char pending_desc = 0;
cfg_err_phase = 1;
if (cbm_open(12, 8, 2, "apps.cfg,s,r") != 0) {
*detail_a = 0;
*detail_b = 0;
*detail_c = 0;
((void)0);
return 1;
}
while (cfg_read_line(line, sizeof(line))) {
trim_in_place(line);
lowercase_in_place(line);
if (is_blank_or_comment(line)) {
continue;
}
if (line[0] == '[') {
if (pending_desc) {
cbm_close(12);
*detail_a = entry_index;
*detail_b = pending_drive;
*detail_c = 0;
((void)0);
return 3;
}
if (strcmp(line, "[system]") == 0) {
section = 1;
} else if (strcmp(line, "[launcher]") == 0) {
section = 2;
} else if (strcmp(line, "[apps]") == 0) {
section = 3;
} else {
section = 0;
}
continue;
}
if (section == 1 || section == 2) {
if (!split_key_value(line, &key, &value)) {
continue;
}
if (section == 1) {
if (strcmp(key, "variant_name") == 0) {
if (value[0] != 0) {
copy_text_limit(launcher_variant_name,
sizeof(launcher_variant_name), value);
}
} else if (strcmp(key, "variant_boot_name") == 0) {
copy_text_limit(launcher_variant_boot_name,
sizeof(launcher_variant_boot_name), value);
}
} else {
if (strcmp(key, "load_all_to_reu") == 0) {
launcher_cfg_load_all_to_reu = (unsigned char)(strcmp(value, "1") == 0);
} else if (strcmp(key, "runappfirst") == 0) {
if (value[0] != 0) {
parse_detail = 0;
err = normalize_prg_field(value, launcher_runappfirst_prg,
&parse_detail);
if (err != 0) {
cbm_close(12);
*detail_a = err;
*detail_b = 0;
*detail_c = parse_detail;
return err;
}
}
}
}
continue;
}
if (section != 3) {
continue;
}
if (!pending_desc) {
parse_detail = 0;
pending_slot = 0;
err = parse_catalog_entry_line(line, &pending_drive, pending_prg,
pending_label, &pending_slot,
&parse_detail);
if (err != 0) {
cbm_close(12);
*detail_a = err;
*detail_b = entry_index;
*detail_c = parse_detail;
return err;
}
pending_desc = 1;
continue;
}
err = add_catalog_entry(app_count, pending_drive, pending_prg, pending_label, line,
pending_slot);
if (err != 0) {
cbm_close(12);
*detail_a = entry_index;
*detail_b = err;
*detail_c = app_count;
return err;
}
pending_desc = 0;
++entry_index;
}
cbm_close(12);
if (pending_desc) {
*detail_a = entry_index;
*detail_b = pending_drive;
*detail_c = 0;
((void)0);
return 3;
}
if (launcher_variant_name[0] == 0) {
copy_text_limit(launcher_variant_name, sizeof(launcher_variant_name), "readyos");
}
if (app_count <= 1) {
*detail_a = 0;
*detail_b = 0;
*detail_c = 0;
((void)0);
return 8;
}
return 0;
}
static unsigned char validate_slot_contract(unsigned char *detail_a,
unsigned char *detail_b,
unsigned char *detail_c) {
unsigned char i;
unsigned char j;
unsigned char bank_i;
cfg_err_phase = 2;
if (app_count <= 1 || app_count > 24) {
*detail_a = app_count;
*detail_b = 0;
*detail_c = 0;
((void)0);
return 9;
}
if (app_banks[0] != 0 || app_files[0][0] != 0) {
*detail_a = app_banks[0];
*detail_b = (unsigned char)app_files[0][0];
*detail_c = 0;
((void)0);
return 1;
}
for (i = 1; i < app_count; ++i) {
bank_i = app_banks[i];
if (bank_i == 0 || bank_i >= 24) {
*detail_a = i;
*detail_b = bank_i;
*detail_c = 0;
((void)0);
return 2;
}
if (app_files[i][0] == 0) {
*detail_a = i;
*detail_b = bank_i;
*detail_c = 0;
((void)0);
return 3;
}
if (app_drives[i] < 8 || app_drives[i] > 11) {
*detail_a = i;
*detail_b = app_drives[i];
*detail_c = 0;
((void)0);
return 5;
}
if (app_default_slots[i] > 9) {
*detail_a = i;
*detail_b = app_default_slots[i];
*detail_c = 0;
((void)0);
return 11;
}
for (j = (unsigned char)(i + 1); j < app_count; ++j) {
if (app_banks[j] == bank_i) {
*detail_a = i;
*detail_b = j;
*detail_c = bank_i;
((void)0);
return 4;
}
}
}
return 0;
}
static void show_slot_contract_error(unsigned char err,
unsigned char detail_a,
unsigned char detail_b,
unsigned char detail_c) {
const char *phase_msg;
(void)err;
phase_msg = (cfg_err_phase == 1) ? "PARSE" : "VALIDATE";
launcher_dbg_put('X');
launcher_dbg_kv('e', err);
launcher_dbg_kv('1', detail_a);
launcher_dbg_kv('2', detail_b);
launcher_dbg_kv('3', detail_c);
launcher_force_text_mode();
tui_clear(6);
{
TuiRect title_box = {1, 0, 38, 3};
tui_window_title(&title_box, "CFG ERROR",
10, 7);
}
tui_puts(2, 5, "CATALOG VALIDATION FAIL", 1);
tui_puts(2, 7, "ERR:", 10);
tui_print_uint(7, 7, err, 10);
tui_puts(2, 9, "A:", 1);
tui_print_uint(5, 9, detail_a, 1);
tui_puts(11, 9, "B:", 1);
tui_print_uint(14, 9, detail_b, 1);
tui_puts(20, 9, "C:", 1);
tui_print_uint(23, 9, detail_c, 1);
tui_puts(2, 11, phase_msg, 7);
tui_puts(2, 18, "CHECK APPS.CFG D8.", 10);
tui_puts(2, 22, "PRESS KEY TO RESET", 1);
launcher_dbg_draw_overlay();
tui_getkey();
}
 
static void set_shim_name(const char *name) {
unsigned char len = 0;
 
while (name[len] && len < 12) {
((char*)0xC824)[len] = name[len];
++len;
}
*((unsigned char*)0xC821) = len;
}
 
static void set_shim_reu(unsigned char bank, unsigned int size) {
*((unsigned char*)0xC820) = bank;
*((unsigned int*)0xC822) = size;
}
 
static void set_shim_drive(unsigned char drive) {
*((unsigned char*)0xC84D) = drive;
*((unsigned char*)0xC89C) = drive;
}
 
static void save_launcher_to_reu(void) {
(*(unsigned char*)0xDF02) = 0x00;
(*(unsigned char*)0xDF03) = 0x10;
(*(unsigned char*)0xDF04) = 0x00;
(*(unsigned char*)0xDF05) = 0x00;
(*(unsigned char*)0xDF06) = 0;
(*(unsigned char*)0xDF07) = 0x00;
(*(unsigned char*)0xDF08) = 0xB6; 
(*(unsigned char*)0xDF01) = 0x90;
}
 
static unsigned int load_app_to_reu(unsigned char index) {
const char *filename;
unsigned char bank;
unsigned char loaded_in_bitmap;
unsigned int end_addr;
unsigned int file_size;
if (index == 0 || index >= app_count || app_banks[index] == 0) {
return 0;
}
filename = app_files[index];
bank = app_banks[index];
 
*((unsigned char*)0xC820) = bank;
 
set_shim_name(filename);
set_shim_drive(app_drives[index]);
 
__asm__("jsr $C809");
 
end_addr = ((unsigned int)(*(unsigned char*)0xC831) << 8)
| (*(unsigned char*)0xC830);
loaded_in_bitmap = shim_bitmap_has_bank(bank);
if (end_addr <= 0x1000 || end_addr > 0xC600) {
 
if (loaded_in_bitmap) {
apps_loaded[index] = 1;
app_sizes[index] = 0xB600;
return 0xB600;
}
apps_loaded[index] = 0;
app_sizes[index] = 0;
shim_bitmap_clear_bank(bank);
return 0;
}
file_size = end_addr - 0x1000;
if (file_size > 0xB600) {
if (loaded_in_bitmap) {
apps_loaded[index] = 1;
app_sizes[index] = 0xB600;
return 0xB600;
}
apps_loaded[index] = 0;
app_sizes[index] = 0;
shim_bitmap_clear_bank(bank);
return 0;
}
 
loaded_in_bitmap = shim_bitmap_has_bank(bank);
if (!loaded_in_bitmap) {
apps_loaded[index] = 0;
app_sizes[index] = 0;
return 0;
}
 
apps_loaded[index] = 1;
app_sizes[index] = file_size;
return file_size;
}
static unsigned char missing_list_contains(const unsigned char *list,
unsigned char count,
unsigned char index) {
unsigned char i;
for (i = 0; i < count; ++i) {
if (list[i] == index) {
return 1;
}
}
return 0;
}
static const char *launcher_resolved_variant_title(void) {
if (launcher_variant_boot_name[0] != 0) {
return launcher_variant_boot_name;
}
if (launcher_variant_name[0] != 0) {
return launcher_variant_name;
}
return "readyos";
}
static void launcher_set_notice(const char *msg, unsigned char color) {
copy_text_limit(launcher_notice, sizeof(launcher_notice), msg);
launcher_notice_color = color;
}
static unsigned char launcher_find_app_by_prg(const char *prg) {
unsigned char i;
for (i = 1; i < app_count; ++i) {
if (strcmp(app_files[i], prg) == 0) {
return i;
}
}
return 0;
}
 
static unsigned char load_all_to_reu_internal(unsigned char interactive) {
(void)interactive;
launcher_set_notice("all apps preloaded", 13);
return 1;
}
static void load_all_to_reu(void) {
(void)load_all_to_reu_internal(1);
}
 
static void load_selected_to_reu(unsigned char index) {
(void)index;
tui_clear(6);
tui_puts(5, 9, "ALL APPS ALREADY PRELOADED", 13);
tui_puts(11, 13, "PRESS ANY KEY...", 1);
tui_getkey();
}
 
static void launch_from_reu(unsigned char index) {
unsigned char bank;
unsigned int size;
if (!apps_loaded[index]) return;
bank = app_banks[index];
size = app_sizes[index];
tui_clear(6);
tui_puts(8, 12, "LAUNCHING FROM REU...", 3);
launcher_resume_save(index, menu.scroll_offset, 1);
 
save_launcher_to_reu();
 
set_shim_reu(bank, size);
set_shim_drive(app_drives[index]);
 
*((unsigned char*)0xC834) = bank;
 
__asm__("jmp $C803");
}
 
static void launch_from_disk(unsigned char index) {
const char *filename;
if (app_files[index][0] == 0) return;
filename = app_files[index];
tui_clear(6);
tui_puts(4, 5, "LOADING FROM DISK:", 1);
draw_drive_prefixed_name(23, 5, index, 3, 12);
tui_puts(4, 7, "PLEASE WAIT...", 7);
 
set_shim_name(filename);
set_shim_drive(app_drives[index]);
launcher_resume_save(index, menu.scroll_offset, 1);
 
save_launcher_to_reu();
 
*((unsigned char*)0xC834) = app_banks[index];
 
__asm__("jmp $C800");
}
 
static void launch_app(unsigned char index) {
if (!slot_contract_ok) {
return;
}
if (index == 0) {
load_all_to_reu();
return;
}
if (index >= app_count || app_banks[index] == 0) {
tui_puts(4, 18 + 2, "NOT AVAILABLE", 10);
return;
}
 
if (apps_loaded[index]) {
launch_from_reu(index);
return;
}
launcher_set_notice("preload missing for selected app", 10);
}
 
static void draw_header(void) {
const char *variant = launcher_resolved_variant_title();
unsigned char len;
unsigned char x;
tui_puts_n(0, 0, "", 40, 14);
tui_puts_n(0, 1, "", 40, 6);
tui_puts_n(0, 2, "", 40, 15);
tui_puts(14, 0, "READY OS v0.2T", 7);
tui_puts_n(0, 2, "----------------------------------------", 40, 15);
len = (unsigned char)strlen(variant);
if (len > 38) {
len = 38;
}
x = (unsigned char)((40 - len) / 2);
tui_puts_n(x, 1, variant, len, 13);
}
static void draw_status(void) {
tui_puts_n(0, 18, "----------------------------------------", 40, 15);
tui_puts_n(0, 18 + 1, "", 40, 6);
tui_puts_n(0, 18 + 2, "----------------------------------------", 40, 15);
tui_puts(2, 18 + 1, "REU: 16MB", 1);
tui_putc(20, 18 + 1, 0x2A, 13);
tui_puts(22, 18 + 1, "IN REU", 15);
}
static void draw_help(void) {
tui_puts(1, 22, "RET:LAUNCH F3:STATUS F1:PRELOADED", 15);
tui_puts(1, 22 + 1, "F2:NEXT APP  F4:PREV  STOP:QUIT", 15);
}
static void draw_notice(void) {
tui_puts_n(1, (unsigned char)(22 - 1), launcher_notice,
38, launcher_notice_color);
}
static void draw_drive_field(unsigned int screen_offset, unsigned char drive) {
(void)drive;
((unsigned char*)0x0400)[screen_offset] = 32;
((unsigned char*)0xD800)[screen_offset] = 12;
((unsigned char*)0x0400)[screen_offset + 1] = 32;
((unsigned char*)0xD800)[screen_offset + 1] = 12;
}
static void draw_drive_prefixed_name(unsigned char x,
unsigned char y,
unsigned char index,
unsigned char name_color,
unsigned char name_maxlen) {
unsigned int screen_offset;
if (index >= app_count) {
tui_puts_n(x, y, "", name_maxlen, name_color);
return;
}
if (app_banks[index] == 0) {
tui_puts_n(x, y, app_names[index], name_maxlen, name_color);
return;
}
screen_offset = (unsigned int)y * 40 + x;
((unsigned char*)0x0400)[screen_offset] = 32;
((unsigned char*)0xD800)[screen_offset] = name_color;
((unsigned char*)0x0400)[screen_offset + 1] = 32;
((unsigned char*)0xD800)[screen_offset + 1] = name_color;
((unsigned char*)0x0400)[screen_offset + 2] = 32;
((unsigned char*)0xD800)[screen_offset + 2] = name_color;
((unsigned char*)0x0400)[screen_offset + 3] = 32;
((unsigned char*)0xD800)[screen_offset + 3] = name_color;
tui_puts_n((unsigned char)(x + 4), y, app_names[index], name_maxlen, name_color);
}
static void clear_menu_span(unsigned int start, unsigned char len, unsigned char color) {
unsigned char pos;
for (pos = 0; pos < len; ++pos) {
((unsigned char*)0x0400)[start + pos] = 32;
((unsigned char*)0xD800)[start + pos] = color;
}
}
static unsigned char launcher_hotkey_slot_for_bank(unsigned char bank) {
unsigned char slot;
if (bank == 0) {
return 0;
}
for (slot = 1; slot <= 9; ++slot) {
if (((unsigned char*)0xC7E0)[(unsigned char)(slot - 1)] == bank) {
return slot;
}
}
return 0;
}
static void draw_binding_tag(unsigned int start, unsigned char slot, unsigned char color) {
if (slot < 1 || slot > 9) {
clear_menu_span(start, 8, color);
return;
}
((unsigned char*)0x0400)[start + 0] = tui_ascii_to_screen('(');
((unsigned char*)0x0400)[start + 1] = tui_ascii_to_screen('C');
((unsigned char*)0x0400)[start + 2] = tui_ascii_to_screen('T');
((unsigned char*)0x0400)[start + 3] = tui_ascii_to_screen('R');
((unsigned char*)0x0400)[start + 4] = tui_ascii_to_screen('L');
((unsigned char*)0x0400)[start + 5] = tui_ascii_to_screen('+');
((unsigned char*)0x0400)[start + 6] = tui_ascii_to_screen((unsigned char)('0' + slot));
((unsigned char*)0x0400)[start + 7] = tui_ascii_to_screen(')');
((unsigned char*)0xD800)[start + 0] = color;
((unsigned char*)0xD800)[start + 1] = color;
((unsigned char*)0xD800)[start + 2] = color;
((unsigned char*)0xD800)[start + 3] = color;
((unsigned char*)0xD800)[start + 4] = color;
((unsigned char*)0xD800)[start + 5] = color;
((unsigned char*)0xD800)[start + 6] = color;
((unsigned char*)0xD800)[start + 7] = color;
}
static void draw_menu_item(unsigned char idx) {
unsigned char row;
unsigned char y;
unsigned char color;
unsigned char prefix;
unsigned int screen_offset;
const char *str;
unsigned char pos;
unsigned char name_len;
unsigned char slot;
unsigned int text_offset;
unsigned int binding_offset;
unsigned int reu_offset;
if (idx < menu.scroll_offset || idx >= menu.count) {
return;
}
row = (unsigned char)(idx - menu.scroll_offset);
if (row >= menu.h) {
return;
}
y = (unsigned char)(menu.y + row);
if (idx == menu.selected) {
color = menu.sel_color;
prefix = 0x3E; 
} else {
color = menu.item_color;
prefix = 32; 
}
screen_offset = (unsigned int)y * 40 + menu.x;
((unsigned char*)0x0400)[screen_offset] = prefix;
((unsigned char*)0xD800)[screen_offset] = color;
((unsigned char*)0x0400)[screen_offset + 1] = 32;
((unsigned char*)0xD800)[screen_offset + 1] = color;
((unsigned char*)0x0400)[screen_offset + 2] = 32;
((unsigned char*)0xD800)[screen_offset + 2] = color;
((unsigned char*)0x0400)[screen_offset + 3] = 32;
((unsigned char*)0xD800)[screen_offset + 3] = color;
((unsigned char*)0x0400)[screen_offset + 4] = 32;
((unsigned char*)0xD800)[screen_offset + 4] = color;
str = menu.items[idx];
name_len = 22;
text_offset = screen_offset + 5;
for (pos = 0; str[pos] != 0 && pos < name_len; ++pos) {
((unsigned char*)0x0400)[text_offset + pos] = tui_ascii_to_screen(str[pos]);
((unsigned char*)0xD800)[text_offset + pos] = color;
}
for (; pos < name_len; ++pos) {
((unsigned char*)0x0400)[text_offset + pos] = 32;
((unsigned char*)0xD800)[text_offset + pos] = color;
}
reu_offset = screen_offset + menu.w - 1;
binding_offset = reu_offset - (8 + 1);
slot = 0;
if (idx > 0 && idx < app_count && app_banks[idx] != 0) {
slot = launcher_hotkey_slot_for_bank(app_banks[idx]);
}
draw_binding_tag(binding_offset, slot, color);
((unsigned char*)0x0400)[reu_offset - 1] = 32;
((unsigned char*)0xD800)[reu_offset - 1] = color;
if (idx > 0 && idx < app_count && apps_loaded[idx] && app_banks[idx] != 0) {
((unsigned char*)0x0400)[reu_offset] = 0x2A;
((unsigned char*)0xD800)[reu_offset] = 13;
} else {
((unsigned char*)0x0400)[reu_offset] = 32;
((unsigned char*)0xD800)[reu_offset] = color;
}
}
static void draw_menu(void) {
unsigned char row;
unsigned char item_idx;
tui_menu_draw(&menu);
return;
for (row = 0; row < menu.h; ++row) {
item_idx = menu.scroll_offset + row;
if (item_idx < menu.count) {
draw_menu_item(item_idx);
} else {
tui_puts_n(menu.x, (unsigned char)(menu.y + row), "", menu.w, menu.item_color);
}
}
}
static void draw_app_desc(void) {
unsigned char sel = tui_menu_selected(&menu);
static char launch_line[39];
 
if (sel < app_count) {
tui_puts_n(2, 4 + 12, app_descs[sel], 38, 15);
 
if (apps_loaded[sel] && app_banks[sel] != 0) {
tui_puts_n(2, 4 + 12 + 1,
"LAUNCH FROM REU (PRELOADED)", 38, 13);
} else if (app_banks[sel] != 0) {
tui_puts_n(2, 4 + 12 + 1,
"WAITING FOR PRELOAD RECOVERY", 38, 10);
} else {
tui_puts_n(2, 4 + 12 + 1, "", 38, 1);
}
} else {
tui_puts_n(2, 4 + 12, "", 38, 1);
tui_puts_n(2, 4 + 12 + 1, "", 38, 1);
}
}
static void launcher_sync_visible_window(void) {
unsigned char max_scroll = 0;
if (menu.count == 0) {
menu.selected = 0;
menu.scroll_offset = 0;
return;
}
if (menu.selected >= menu.count) {
menu.selected = (unsigned char)(menu.count - 1);
}
if (menu.count > menu.h) {
max_scroll = (unsigned char)(menu.count - menu.h);
}
if (menu.scroll_offset > max_scroll) {
menu.scroll_offset = max_scroll;
}
if (menu.selected < menu.scroll_offset) {
menu.scroll_offset = menu.selected;
} else if (menu.selected >= (unsigned char)(menu.scroll_offset + menu.h)) {
menu.scroll_offset = (unsigned char)(menu.selected - menu.h + 1);
}
}
static void launcher_seed_default_hotkeys(void) {
unsigned char i;
unsigned char slot;
for (i = 1; i < app_count; ++i) {
slot = app_default_slots[i];
if (slot == 0) {
continue;
}
if (((unsigned char*)0xC7E0)[(unsigned char)(slot - 1)] == 0) {
((unsigned char*)0xC7E0)[(unsigned char)(slot - 1)] = app_banks[i];
}
}
}
static unsigned char launcher_index_for_bank(unsigned char bank) {
unsigned char i;
for (i = 1; i < app_count; ++i) {
if (app_banks[i] == bank) {
return i;
}
}
return 0;
}
static void launcher_apply_startup_actions(unsigned char suppress_startup_once) {
unsigned char index;
if (suppress_startup_once) {
return;
}
if (launcher_cfg_load_all_to_reu) {
launcher_set_notice("all apps preloaded", 13);
}
if (launcher_runappfirst_prg[0] != 0) {
index = launcher_find_app_by_prg(launcher_runappfirst_prg);
if (index == 0) {
launcher_set_notice("runappfirst app not found", 10);
return;
}
launch_app(index);
}
}
static void launcher_draw(void) {
launcher_dbg_put('D');
launcher_dbg_kv('a', app_count);
launcher_dbg_kv('m', menu.count);
launcher_dbg_kv('s', menu.selected);
launcher_set_stage(5u);
launcher_force_text_mode();
launcher_sync_visible_window();
tui_clear(6);
draw_header();
tui_puts(2, 4 - 1, "APPLICATIONS:", 1);
draw_menu();
draw_app_desc();
draw_status();
draw_notice();
draw_help();
launcher_dbg_put('R');
launcher_set_stage(13u);
launcher_dbg_draw_overlay();
}
 
static void launcher_init_easyflash(void) {
unsigned char err;
unsigned char detail_a;
unsigned char detail_b;
unsigned char detail_c;
ef_linit_entry();
launcher_dbg_put('i');
ef_linit_before_bootstrap();
launcher_easyflash_tui_bootstrap();
ef_linit_after_bootstrap();
launcher_dbg_put('I');
launcher_set_stage(2u);
launcher_dbg_put('T');
ef_linit_before_catalog_defaults();
catalog_init_defaults();
catalog_rebind_views();
ef_linit_after_catalog_defaults();
launcher_dbg_put('C');
launcher_set_stage(8u);
err = load_catalog_from_embedded(&detail_a, &detail_b, &detail_c);
launcher_dbg_put('L');
launcher_dbg_kv('e', err);
launcher_dbg_kv('a', app_count);
if (err != 0) {
copy_text_limit(launcher_notice, sizeof(launcher_notice),
"visual probe: embedded catalog failed");
launcher_notice_color = 10;
} else {
copy_text_limit(launcher_notice, sizeof(launcher_notice),
"visual probe: embedded catalog ok");
launcher_notice_color = 13;
}
running = 1;
launcher_dbg_put('P');
launcher_set_stage(11u);
tui_menu_init(&menu, 2, 4, 37, 12, app_names, app_count);
menu.item_color = 1;
menu.sel_color = 3;
menu.selected = (unsigned char)(app_count > 1 ? 1 : 0);
menu.scroll_offset = 0;
launcher_sync_visible_window();
launcher_dbg_put('d');
set_shim_drive(8);
launcher_dbg_put('D');
launcher_dbg_put('y');
launcher_dbg_put('Y');
launcher_dbg_put('h');
launcher_dbg_put('H');
launcher_draw();
launcher_dbg_put('K');
launcher_set_stage(12u);
}
static void launcher_init(void) {
launcher_init_easyflash();
return;
}
static void launcher_loop(void) {
unsigned char key;
unsigned char result;
unsigned char bank;
unsigned char old_selected;
unsigned char old_scroll_offset;
 
if (!running) {
return;
}
launcher_draw();
while (running) {
launcher_dbg_put('K');
launcher_set_stage(12u);
key = tui_getkey();
if (key != 2 && key != 137 && key != 138) {
bank = tui_handle_global_hotkey(key, 0, 0);
if (bank >= 1 && bank < 24) {
result = launcher_index_for_bank(bank);
if (result != 0) {
launch_app(result);
launcher_draw();
continue;
}
}
}
old_selected = menu.selected;
old_scroll_offset = menu.scroll_offset;
result = tui_menu_input(&menu, key);
launcher_sync_visible_window();
if (result != 255) {
launch_app(result);
launcher_draw();
continue;
}
switch (key) {
case 133:
load_all_to_reu();
launcher_draw();
break;
case 134:
load_selected_to_reu(tui_menu_selected(&menu));
launcher_draw();
break;
case 3:
running = 0;
break;
default:
 
if (old_selected != menu.selected) {
if (old_scroll_offset != menu.scroll_offset) {
draw_menu();
} else {
draw_menu_item(old_selected);
draw_menu_item(menu.selected);
}
draw_app_desc();
}
break;
}
}
 
__asm__("jmp $FCE2");
}
static void launcher_draw_visual_probe(void) {
launcher_dbg_put('D');
launcher_set_stage(5u);
launcher_force_text_mode();
tui_clear(6);
draw_header();
tui_puts(11, 7, "CRT VISUAL PROBE", 7);
tui_puts(4, 9, "EMBEDDED CATALOG LOADED", 1);
tui_puts(4, 10, "AND VALIDATED", 1);
tui_puts(2, 12, "APP COUNT:", 15);
tui_print_uint(13, 12, app_count, 1);
if (app_count > 1) {
tui_puts_n(2, 13, app_names[1], 30, 13);
}
if (app_count > 2) {
tui_puts_n(2, 14, app_names[2], 30, 13);
}
if (app_count > 3) {
tui_puts_n(2, 15, app_names[3], 30, 13);
}
draw_status();
draw_notice();
draw_help();
launcher_dbg_put('R');
launcher_set_stage(13u);
launcher_dbg_draw_overlay();
}
int main(void) {
launcher_init();
__asm__("lda $dc0d");
__asm__("lda $dd0d");
__asm__("cli");
launcher_loop();
return 0;
}
