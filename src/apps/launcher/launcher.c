/*
 * launcher.c - Ready OS Launcher (Home Screen)
 * Loads at $1000, uses shim at $C800 for app loading
 *
 * For Commodore 64, compiled with CC65
 */

#include "../../lib/tui.h"
#include "../../lib/resume_state.h"
#include "../../lib/reu_control_bank.h"
#include "../../lib/reu_phys.h"
#include "../../generated/build_version.h"
#ifndef LAUNCHER_DMA_LOAD
#define LAUNCHER_DMA_LOAD 0
#endif
#ifndef READYOS_LAUNCHER_VARIANT_EASYFLASH
#define READYOS_LAUNCHER_VARIANT_EASYFLASH 0
#endif
#if !READYOS_LAUNCHER_VARIANT_EASYFLASH
#include "../../lib/file_dialog.h"
#include "../../lib/storage_device.h"
#include <cbm_filetype.h>
#endif
#if READYOS_LAUNCHER_VARIANT_EASYFLASH
#include "../../generated/launcher_easyflash_catalog.h"
#endif
#include <c64.h>
#include <cbm.h>
#include <conio.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * Shim Interface (shim is at $C800-$C9FF, 512 bytes)
 *---------------------------------------------------------------------------*/

/* Shim jump table addresses (at $C800) */
#define SHIM_LOAD_DISK_RUN   0xC800   /* Load from disk, run */
#define SHIM_LOAD_REU_RUN    0xC803   /* Fetch from REU, run */
#define SHIM_RUN_APP         0xC806   /* Just run app at $1000 */
#define SHIM_PRELOAD_TO_REU  0xC809   /* Preload to REU, return to launcher */
#define SHIM_RETURN_LAUNCHER 0xC80C   /* Return to launcher */
#define SHIM_SWITCH_APP      0xC80F   /* Switch to another app */

/* Shim data addresses (at $C820) */
#define SHIM_APP_BANK   ((unsigned char*)0xC820)   /* Target bank for loading */
#define SHIM_APP_NAMELEN ((unsigned char*)0xC821)
#define SHIM_APP_SIZE   ((unsigned int*)0xC822)
#define SHIM_APP_NAME   ((char*)0xC824)
#define SHIM_CURRENT_BANK ((unsigned char*)0xC834) /* Currently running app's bank */
#define SHIM_LAST_SAVED   ((unsigned char*)0xC835) /* Last app saved by return_to_launcher */
#define SHIM_REU_BITMAP_LO ((unsigned char*)0xC836) /* Bitmap bits 0-7 */
#define SHIM_REU_BITMAP_HI ((unsigned char*)0xC837) /* Bitmap bits 8-15 */
#define SHIM_REU_BITMAP_XHI ((unsigned char*)0xC838) /* Bitmap bits 16-23 */
#define SHIM_REU_BANK_SKIP ((unsigned char*)0xC83B)  /* Physical REU banks skipped before ReadyOS */
#define SHIM_LAUNCHER_FLAGS ((unsigned char*)0xC83C) /* Launcher one-shot flags */
#define SHIM_LOAD_DISK_DEV_IMM ((unsigned char*)0xC84D) /* A2 xx at $C84C */
#define SHIM_PRELOAD_DEV_IMM   ((unsigned char*)0xC89C) /* A2 xx at $C89B */

/* REU registers for direct access */
#define REU_COMMAND  (*(unsigned char*)0xDF01)
#define REU_C64_LO   (*(unsigned char*)0xDF02)
#define REU_C64_HI   (*(unsigned char*)0xDF03)
#define REU_REU_LO   (*(unsigned char*)0xDF04)
#define REU_REU_HI   (*(unsigned char*)0xDF05)
#define REU_REU_BANK (*(unsigned char*)0xDF06)
#define REU_LEN_LO   (*(unsigned char*)0xDF07)
#define REU_LEN_HI   (*(unsigned char*)0xDF08)

#define REU_CMD_STASH 0x90
#define REU_CMD_FETCH 0x91

void reu_control_bank_sync_and_mirror(unsigned char writer_id);
void reu_control_bank_write_launcher_registry(
    unsigned char first_app_index,
    unsigned char app_count,
    const unsigned char *app_banks,
    const unsigned char *app_drives,
    const unsigned char *app_default_slots,
    const unsigned char *app_resource_sets,
    const unsigned char *app_resource_loaded,
    const unsigned char *app_rs_bank1,
    const unsigned char *app_rs_bank2,
    const unsigned char *app_rs_bank3,
    const unsigned char *app_rs_bank4,
    const unsigned char *apps_loaded);
void reu_dma_stash(unsigned int c64_addr, unsigned char bank,
                   unsigned int reu_offset, unsigned int length);
#define REUCB_WRITER_LAUNCHER 1u

/*---------------------------------------------------------------------------
 * Constants
 *---------------------------------------------------------------------------*/

#define TITLE_Y      0
#define APPS_START_Y 4
#define APPS_HEIGHT  12
#define STATUS_Y     18
#define HELP_Y       22
#define APP_MENU_WIDTH 37
#define APP_NAME_WIDTH 22
#define APP_BIND_LABEL_LEN 8
#define VARIANT_MAX_LEN 31
#define LAUNCHER_NOTICE_LEN 38
#define MENU_NO_APP 0xFFu
#define SHIM_LAUNCHER_FLAG_SUPPRESS_STARTUP 0x01u
#define LOAD_ALL_LIST_Y 4
#define LOAD_ALL_LIST_ROWS 19

/* REU indicator character */
#define REU_INDICATOR 0x2A  /* '*' in PETSCII screen code */

/* App catalog limits */
#define APP_SLOT_CAPACITY 64
#define MAX_APPS (APP_SLOT_CAPACITY + 1)  /* optional slot 0 + 64 app slots */
#define MAX_FILE_LEN 12      /* shim filename buffer is 12 bytes */
#define MAX_NAME_LEN 31
#define MAX_DESC_LEN 38
#define DEFAULT_DRIVE 8
#define APP_CFG_LFN 12
#define APP_CFG_OPEN_SPEC "apps.cfg,s,r"
#define APP_MANIFEST_LFN 13
#define APP_MANIFEST_PREFIX "app."

#define REU_ALLOC_TABLE  ((unsigned char*)0xC600)
#define REU_FREE       0
#define REU_APP_STATE  1
#define REU_RS_CACHE   5
#define REU_RESERVED   4
#define REU_RS_SCRATCH 13
#define REU_RB_CORE    14
#define REU_RB_CODE    15
#define REU_TOTAL_BANKS 256
#define REU_SNAPSHOT_LOGICAL_MIN 1
#define REU_SNAPSHOT_LOGICAL_SCAN_MAX 223

#define APP_RESOURCE_NONE          0
#define APP_RESOURCE_READYSHELL_OVL 1
#define APP_RESOURCE_READYBASIC_CORE 2
#define APP_RESOURCE_READYSHELL_TOKEN "rsovl"
#define APP_RESOURCE_READYBASIC_TOKEN "rbcore"
#define READYSHELL_RESOURCE_BANKS 4
#define READYSHELL_OVERLAY_COUNT 9
#define READYSHELL_OVERLAY_LFN 14
#define READYSHELL_OVERLAY_SLOT_LEN 0x3800u
#define READYSHELL_OVERLAY_LOAD_ADDR 0x8E00u
#define READYSHELL_META_OFF 0x80F0u
#define READYSHELL_META_VERSION 4u
#define READYSHELL_META_LEN 36u
#define READYSHELL_META_REC_OFF 8u
#define READYSHELL_META_REC_LEN 3u
#define READYSHELL_META_VALID_LO 0xFFu
#define READYSHELL_META_VALID_HI 0x01u
#define READYSHELL_STATE_BANK_CACHE ((unsigned char*)0xCFF2)
#define RESOURCE_IO_CHUNK 128
#define LAUNCHER_C64U_IMAGE_PATH_LEN 95
#define LAUNCHER_C64U_IMAGE_PATH_DEFAULT "/usb1/readyos.d81"

#ifndef LAUNCHER_CFG_VERBOSE
#define LAUNCHER_CFG_VERBOSE 0
#endif

#define CFG_ERR_OPEN         1
#define CFG_ERR_FORMAT       2
#define CFG_ERR_MISSING_DESC 3
#define CFG_ERR_TOO_MANY     4
#define CFG_ERR_DRIVE        5
#define CFG_ERR_PRG          6
#define CFG_ERR_LABEL        7
#define CFG_ERR_EMPTY        8
#define CFG_ERR_COUNT        9
#define CFG_ERR_PRG_EXT     10
#define CFG_ERR_HOTKEY      11
#define CFG_ERR_RESOURCE    12

#define CFG_ERR_PHASE_PARSE    1
#define CFG_ERR_PHASE_VALIDATE 2

#if LAUNCHER_CFG_VERBOSE
#define CFG_TITLE_TEXT "LAUNCHER CONFIG ERROR"
#define CFG_FAIL_TEXT "APP CATALOG FAILED VALIDATION."
#define CFG_CHECK_TEXT "CHECK APPS.CFG ON DISK 8."
#define CFG_PRESS_TEXT "PRESS ANY KEY TO RESET"
#define CFG_PHASE_PARSE_TEXT "PARSE PHASE"
#define CFG_PHASE_VALIDATE_TEXT "VALIDATE PHASE"
#define CFG_SHOW_REASON 1
#define CFG_REASON_OPEN_BASE "OPEN/BASE SLOT ERROR"
#define CFG_REASON_FORMAT "FORMAT/SLOT ERROR"
#define CFG_REASON_DESC "DESCRIPTION/SLOT ERROR"
#define CFG_REASON_CAPACITY "CAPACITY/DUPLICATE ERROR"
#define CFG_REASON_DRIVE "DRIVE FIELD ERROR"
#define CFG_REASON_PRG "PRG FIELD ERROR"
#define CFG_REASON_LABEL "DISPLAY NAME ERROR"
#define CFG_REASON_EMPTY "NO APP ENTRIES"
#define CFG_REASON_COUNT "APP COUNT INVALID"
#define CFG_REASON_PRG_EXT "REMOVE .PRG EXTENSION"
#define CFG_MSG_PRG_EMPTY "PRG NAME IS EMPTY"
#define CFG_MSG_PRG_COMMA "COMMA SUFFIX NOT ALLOWED"
#define CFG_MSG_PRG_EXT "DO NOT USE .PRG EXTENSION"
#define CFG_MSG_PRG_LEN "PRG NAME LENGTH INVALID"
#define CFG_MSG_PRG_CHAR "INVALID CHAR IN PRG NAME"
#define CFG_MSG_MISSING_COLON "MISSING ':' FIELD DELIMITERS"
#define CFG_MSG_DRIVE_EMPTY "DRIVE FIELD IS EMPTY"
#define CFG_MSG_DRIVE_NUMERIC "DRIVE MUST BE NUMERIC"
#define CFG_MSG_DRIVE_RANGE "DRIVE MUST BE 8..11"
#define CFG_MSG_LABEL_EMPTY "DISPLAY NAME IS EMPTY"
#define CFG_MSG_TOO_MANY "TOO MANY APPS IN CATALOG"
#define CFG_MSG_LABEL_LONG "DISPLAY NAME TOO LONG"
#define CFG_MSG_OPEN_FAIL "CANNOT OPEN APPS.CFG ON DRIVE 8"
#define CFG_MSG_DESC_MISSING "MISSING DESCRIPTION LINE"
#define CFG_MSG_NO_APPS "NO APPS FOUND IN APPS.CFG"
#define CFG_MSG_COUNT_RANGE "APP COUNT OUT OF RANGE"
#define CFG_MSG_SLOT0 "SLOT 0 MUST BE LAUNCHER"
#define CFG_MSG_BANK_RANGE "REU BANK OUT OF RANGE"
#define CFG_MSG_FILENAME_EMPTY "EMPTY APP FILENAME SLOT"
#define CFG_MSG_APP_DRIVE_RANGE "APP DRIVE OUT OF RANGE"
#define CFG_MSG_DUP_BANK "DUPLICATE REU BANK"
#define CFG_MSG_HOTKEY_EMPTY "HOTKEY SLOT IS EMPTY"
#define CFG_MSG_HOTKEY_NUMERIC "HOTKEY SLOT MUST BE NUMERIC"
#define CFG_MSG_HOTKEY_RANGE "HOTKEY SLOT MUST BE 1..9"
#define CFG_MSG_HOTKEY_EXTRA "TOO MANY ':' FIELDS"
#define CFG_MSG_RESOURCE "RESOURCE SET UNKNOWN"
#else
#define CFG_TITLE_TEXT "CFG ERROR"
#define CFG_FAIL_TEXT "CATALOG VALIDATION FAIL"
#define CFG_CHECK_TEXT "CHECK APPS.CFG D8."
#define CFG_PRESS_TEXT "PRESS KEY TO RESET"
#define CFG_PHASE_PARSE_TEXT "PARSE"
#define CFG_PHASE_VALIDATE_TEXT "VALIDATE"
#define CFG_SHOW_REASON 0
#endif

/* REU bank assignments */
#define REU_BANK_LAUNCHER  0   /* Logical launcher bank; physical bank is skip + 1 */
#define REU_LAUNCHER_PHYSICAL() ((unsigned char)(*SHIM_REU_BANK_SKIP + 1u))
#define REU_LOGICAL_TO_PHYSICAL(bank) \
    ((unsigned char)(*SHIM_REU_BANK_SKIP + \
     (((unsigned char)(bank) == 0u) ? 1u : (1u + (unsigned char)(bank)))))
#define LAUNCHER_RESUME_SCHEMA 9

/* App save size - must include code + data + BSS */
#define APP_SAVE_SIZE 0xB600  /* $1000-$C5FF (46KB) */
/* Valid app load range from cfg/ready_app.cfg: $1000-$C5FF */
#define APP_LOAD_START    0x1000
#define APP_LOAD_END_EXCL 0xC600

/*---------------------------------------------------------------------------
 * Static variables
 *---------------------------------------------------------------------------*/

static unsigned char app_banks[MAX_APPS];
static unsigned char app_drives[MAX_APPS];
static unsigned char app_default_slots[MAX_APPS];
static unsigned char app_resource_sets[MAX_APPS];
static unsigned char app_resource_loaded[MAX_APPS];
static unsigned char app_rs_bank1[MAX_APPS];
static unsigned char app_rs_bank2[MAX_APPS];
static unsigned char app_rs_bank3[MAX_APPS];
static unsigned char app_rs_bank4[MAX_APPS];
static char catalog_name_cache[APPS_HEIGHT][MAX_NAME_LEN + 1];
static char catalog_text_buf[MAX_DESC_LEN + 1];
static unsigned char catalog_cache_menu_start = 0xFFu;
static const char *launcher_menu_dummy[1];
static unsigned char app_count;

typedef struct {
    unsigned char selected;
    unsigned char scroll_offset;
    unsigned char suppress_startup_once;
    unsigned char reserved;
} LauncherResumeV1;

/* Track loaded apps */
static unsigned char apps_loaded[MAX_APPS];
static unsigned int app_sizes[MAX_APPS];
static LauncherResumeV1 launcher_resume_blob;
static unsigned char resume_ready;
static unsigned char launcher_cfg_load_all_to_reu;
static char launcher_variant_name[VARIANT_MAX_LEN + 1];
static char launcher_variant_boot_name[VARIANT_MAX_LEN + 1];
static char launcher_runappfirst_prg[MAX_FILE_LEN + 1];
static char launcher_notice[LAUNCHER_NOTICE_LEN + 1];
static unsigned char launcher_notice_color = TUI_COLOR_GRAY3;
static unsigned char launcher_rs_meta_buf[READYSHELL_META_LEN];
static unsigned char launcher_rsrc_rec_buf[REUCB_RSRC_REC_SIZE];
static unsigned char launcher_dep_line_buf[REUCB_DEP_LINE_SIZE];
#if !READYOS_LAUNCHER_VARIANT_EASYFLASH
static unsigned char launcher_resource_buf[RESOURCE_IO_CHUNK];
static FileDialogState launcher_file_dialog;
static DirPageEntry launcher_manifest_entry;
static char launcher_manifest_open_spec[24];
static char launcher_resource_open_spec[18];
#if LAUNCHER_DMA_LOAD
extern unsigned char launcher_uci_dma_detect(void);
extern unsigned char launcher_uci_dma_load_prg(void);
extern unsigned char launcher_uci_dma_available;
extern const char *launcher_uci_dma_name;
extern unsigned char launcher_uci_dma_reu_bank;
extern unsigned int launcher_uci_dma_reu_offset;
extern unsigned int launcher_uci_dma_max_len;
extern unsigned int launcher_uci_dma_expected_load_addr;
extern unsigned int launcher_uci_dma_loaded_size;
extern unsigned char launcher_uci_dma_last_error;
extern unsigned char launcher_uci_dma_dbg_stat0;
extern unsigned char launcher_uci_dma_dbg_stat1;
extern const char *launcher_uci_dma_image_dir;
extern const char *launcher_uci_dma_image_name;
extern const char *launcher_uci_dma_mount_name;
static char launcher_c64u_image_path[LAUNCHER_C64U_IMAGE_PATH_LEN + 1];
static char launcher_uci_dma_name_buf[MAX_FILE_LEN + 1];
static char launcher_uci_dma_dir_buf[LAUNCHER_C64U_IMAGE_PATH_LEN + 1];
static char launcher_uci_dma_image_buf[LAUNCHER_C64U_IMAGE_PATH_LEN + 1];
static char launcher_uci_dma_mount_buf[LAUNCHER_C64U_IMAGE_PATH_LEN + 1];
static unsigned char launcher_dma_checked;
static unsigned char launcher_dma_available;
static unsigned char launcher_dma_used;
static volatile unsigned char launcher_dma_breadcrumb;
#endif
#endif

/* Menu state */
static TuiMenu menu;
static unsigned char running;
static unsigned char slot_contract_ok = 1;
static unsigned char cfg_err_phase = 0;
#if LAUNCHER_CFG_VERBOSE
static char cfg_err_line[39];
static char cfg_err_prg[16];
static char cfg_err_reason[39];
#endif

/* Forward declarations for shared draw helpers */
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
static void launcher_seed_default_hotkeys(void);
#if READYOS_LAUNCHER_VARIANT_EASYFLASH
static unsigned char load_catalog_from_embedded(unsigned char *detail_a,
                                                unsigned char *detail_b,
                                                unsigned char *detail_c);
#endif
static unsigned char launcher_has_load_all_slot(void);
static unsigned char launcher_first_app_index(void);
static unsigned char launcher_is_app_slot(unsigned char index);
static unsigned char launcher_menu_extra_count(void);
static unsigned char launcher_menu_count(void);
static unsigned char launcher_menu_is_browse(unsigned char menu_index);
static unsigned char launcher_menu_to_app_index(unsigned char menu_index);
static unsigned char launcher_app_to_menu_index(unsigned char app_index);
static void catalog_invalidate_cache(void);
static const char *catalog_name_for_index(unsigned char index);
static const char *catalog_desc_for_index(unsigned char index);
static const char *catalog_file_for_index(unsigned char index);
static void launcher_mirror_reu_control(void);
static void launcher_bind_default_hotkey_for_index(unsigned char index);
static unsigned char launcher_prepare_app_resources(unsigned char index);
static void launcher_set_startup_suppressed(void);
#if !READYOS_LAUNCHER_VARIANT_EASYFLASH
static void launcher_free_app_resources(unsigned char index);
#endif
static void launcher_control_clear_resource_records(void);
static void launcher_control_clear_dependency_lines(void);
#if !READYOS_LAUNCHER_VARIANT_EASYFLASH
static void launcher_control_write_dep_line(unsigned char index,
                                            const char *line);
#endif
static void launcher_set_notice(const char *msg, unsigned char color);
static void launcher_set_notice_if_empty(const char *msg, unsigned char color);
#if READYOS_LAUNCHER_VARIANT_EASYFLASH
static void launcher_mark_embedded_preloads_loaded(void);
#endif
#if !READYOS_LAUNCHER_VARIANT_EASYFLASH
static void browse_and_load_manifest(void);
#endif

/* Launcher does not use F2/F4 global app cycling, but tui_hotkeys.c expects
 * these entry points when linked. Keep tiny local stubs instead of pulling in
 * the full nav micromodule. */
unsigned char tui_get_next_app(unsigned char current_bank) {
    (void)current_bank;
    return 0;
}

unsigned char tui_get_prev_app(unsigned char current_bank) {
    (void)current_bank;
    return 0;
}

static unsigned char launcher_has_load_all_slot(void) {
#if READYOS_LAUNCHER_VARIANT_EASYFLASH
    return 0;
#else
    return 1;
#endif
}

static unsigned char launcher_first_app_index(void) {
    return launcher_has_load_all_slot() ? 1u : 0u;
}

static unsigned char launcher_is_app_slot(unsigned char index) {
    if (index >= app_count) {
        return 0;
    }
    if (launcher_has_load_all_slot() && index == 0u) {
        return 0;
    }
    return 1;
}

static void launcher_set_startup_suppressed(void) {
    *SHIM_LAUNCHER_FLAGS =
        (unsigned char)(*SHIM_LAUNCHER_FLAGS | SHIM_LAUNCHER_FLAG_SUPPRESS_STARTUP);
}

static unsigned char launcher_logical_to_physical(unsigned char logical_bank) {
    unsigned int physical;

    physical = (unsigned int)(*SHIM_REU_BANK_SKIP) + 1u + logical_bank;
    if (physical > 255u) {
        return 0xFFu;
    }
    if (REU_ALLOC_TABLE[physical] == REU_UNAVAIL) {
        return 0xFFu;
    }
    return (unsigned char)physical;
}

#if READYOS_LAUNCHER_VARIANT_EASYFLASH
static void launcher_mark_bank_if_available(unsigned char bank, unsigned char type) {
    if (REU_ALLOC_TABLE[bank] != REU_UNAVAIL) {
        REU_ALLOC_TABLE[bank] = type;
    }
}
#endif

static unsigned char launcher_catalog_uses_bank(unsigned char bank,
                                                unsigned char except_index) {
    unsigned char i;

    for (i = launcher_first_app_index(); i < app_count; ++i) {
        if (i != except_index && app_banks[i] == bank) {
            return 1u;
        }
    }
    return 0u;
}

static unsigned char launcher_alloc_snapshot_bank(unsigned char index) {
    unsigned char bank;
    unsigned char physical;

    if (!launcher_is_app_slot(index)) {
        return 0;
    }
    if (app_banks[index] != 0u) {
        return app_banks[index];
    }

    for (bank = REU_SNAPSHOT_LOGICAL_MIN;
         bank <= REU_SNAPSHOT_LOGICAL_SCAN_MAX;
         ++bank) {
        physical = launcher_logical_to_physical(bank);
        if (physical == 0xFFu) {
            break;
        }
        if (launcher_catalog_uses_bank(bank, index)) {
            continue;
        }
        if (REU_ALLOC_TABLE[physical] == REU_FREE) {
            app_banks[index] = bank;
            REU_ALLOC_TABLE[physical] = REU_APP_STATE;
            launcher_mirror_reu_control();
            return bank;
        }
    }

    return 0;
}

static unsigned char launcher_resolve_snapshot_bank(unsigned char index) {
    return launcher_alloc_snapshot_bank(index);
}

static unsigned char launcher_menu_extra_count(void) {
#if READYOS_LAUNCHER_VARIANT_EASYFLASH
    return 0u;
#else
    return 1u;
#endif
}

static unsigned char launcher_menu_count(void) {
    return (unsigned char)(app_count + launcher_menu_extra_count());
}

static unsigned char launcher_menu_is_browse(unsigned char menu_index) {
#if READYOS_LAUNCHER_VARIANT_EASYFLASH
    (void)menu_index;
    return 0u;
#else
    return (unsigned char)(menu_index == 0u);
#endif
}

static unsigned char launcher_menu_to_app_index(unsigned char menu_index) {
#if READYOS_LAUNCHER_VARIANT_EASYFLASH
    return menu_index;
#else
    if (menu_index == 0u) {
        return MENU_NO_APP;
    }
    return (unsigned char)(menu_index - 1u);
#endif
}

static unsigned char launcher_app_to_menu_index(unsigned char app_index) {
    return (unsigned char)(app_index + launcher_menu_extra_count());
}

/*---------------------------------------------------------------------------
 * Shim bitmap helpers ($C836-$C838)
 *---------------------------------------------------------------------------*/
static unsigned char shim_bitmap_has_bank(unsigned char bank) {
    if (bank < 8) {
        return (unsigned char)(*SHIM_REU_BITMAP_LO & (unsigned char)(1U << bank));
    }
    if (bank < 16) {
        return (unsigned char)(*SHIM_REU_BITMAP_HI & (unsigned char)(1U << (bank - 8)));
    }
    if (bank < 24) {
        return (unsigned char)(*SHIM_REU_BITMAP_XHI & (unsigned char)(1U << (bank - 16)));
    }
    return 0;
}

static void shim_bitmap_clear_bank(unsigned char bank) {
    if (bank < 8) {
        *SHIM_REU_BITMAP_LO &= (unsigned char)~(unsigned char)(1U << bank);
    } else if (bank < 16) {
        *SHIM_REU_BITMAP_HI &= (unsigned char)~(unsigned char)(1U << (bank - 8));
    } else if (bank < 24) {
        *SHIM_REU_BITMAP_XHI &= (unsigned char)~(unsigned char)(1U << (bank - 16));
    }
}

#if !READYOS_LAUNCHER_VARIANT_EASYFLASH && LAUNCHER_DMA_LOAD
static void shim_bitmap_set_bank(unsigned char bank) {
    if (bank < 8) {
        *SHIM_REU_BITMAP_LO |= (unsigned char)(1U << bank);
    } else if (bank < 16) {
        *SHIM_REU_BITMAP_HI |= (unsigned char)(1U << (bank - 8));
    } else if (bank < 24) {
        *SHIM_REU_BITMAP_XHI |= (unsigned char)(1U << (bank - 16));
    }
}
#endif

#if !READYOS_LAUNCHER_VARIANT_EASYFLASH
static void launcher_free_snapshot_bank(unsigned char index) {
    unsigned char bank;
    unsigned char physical;

    if (index >= app_count) {
        return;
    }
    bank = app_banks[index];
    if (bank == 0u) {
        return;
    }
    shim_bitmap_clear_bank(bank);
    if (*SHIM_LAST_SAVED == bank) {
        *SHIM_LAST_SAVED = 0xFFu;
    }
    physical = launcher_logical_to_physical(bank);
    if (physical != 0xFFu) {
        REU_ALLOC_TABLE[physical] = REU_FREE;
    }
    launcher_free_app_resources(index);
    app_banks[index] = 0u;
    apps_loaded[index] = 0u;
    app_sizes[index] = 0u;
    launcher_mirror_reu_control();
}
#endif

/*---------------------------------------------------------------------------
 * Sync apps_loaded[] from shim's reu_bitmap ($C836-$C838)
 * The shim updates reu_bitmap whenever an app is stashed to REU,
 * so this reflects the actual REU contents.
 *---------------------------------------------------------------------------*/
static void sync_from_reu_bitmap(void) {
    unsigned char i;
    unsigned char bank;
    unsigned char last_saved;

    /* Resilience: if return_to_launcher recorded a saved bank but bitmap
     * wasn't updated (e.g., interrupted path), heal bitmap from last_saved. */
    last_saved = *SHIM_LAST_SAVED;
    if (last_saved < 24 && launcher_catalog_uses_bank(last_saved, 0xFFu)) {
        if (last_saved < 8) {
            *SHIM_REU_BITMAP_LO |= (unsigned char)(1U << last_saved);
        } else if (last_saved < 16) {
            *SHIM_REU_BITMAP_HI |= (unsigned char)(1U << (last_saved - 8));
        } else {
            *SHIM_REU_BITMAP_XHI |= (unsigned char)(1U << (last_saved - 16));
        }
    }

    for (i = launcher_first_app_index(); i < app_count; ++i) {
        bank = app_banks[i];
        if (bank != 0) {
            if (bank < 24u && shim_bitmap_has_bank(bank)) {
                apps_loaded[i] = 1;
                app_sizes[i] = APP_SAVE_SIZE;
            } else if (bank >= 24u && last_saved == bank) {
                apps_loaded[i] = 1;
                app_sizes[i] = APP_SAVE_SIZE;
            } else if (bank < 24u) {
                apps_loaded[i] = 0;
                app_sizes[i] = 0;
            }
        }
    }

    /* Clear the last_saved flag - we've synced state */
    *SHIM_LAST_SAVED = 0xFF;
}

/*---------------------------------------------------------------------------
 * Slot contract helpers
 *---------------------------------------------------------------------------*/
static void compute_required_slot_bitmap(unsigned char *expected_lo,
                                         unsigned char *expected_hi,
                                         unsigned char *expected_xhi) {
    unsigned char i;
    unsigned char bank;
    unsigned char lo = 0;
    unsigned char hi = 0;
    unsigned char xhi = 0;

    for (i = launcher_first_app_index(); i < app_count; ++i) {
        bank = app_banks[i];
        if (bank == 0u) {
            continue;
        }
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
    unsigned char i;

    compute_required_slot_bitmap(&expected_lo, &expected_hi, &expected_xhi);
    if (((*SHIM_REU_BITMAP_LO & expected_lo) != expected_lo) ||
        ((*SHIM_REU_BITMAP_HI & expected_hi) != expected_hi) ||
        ((*SHIM_REU_BITMAP_XHI & expected_xhi) != expected_xhi)) {
        return 0u;
    }

    for (i = launcher_first_app_index(); i < app_count; ++i) {
        if (launcher_is_app_slot(i) &&
            (app_banks[i] == 0u || !apps_loaded[i])) {
            return 0u;
        }
    }
    return 1u;
}

static void copy_text_limit(char *dst, unsigned char cap, const char *src) {
    strncpy(dst, src, (unsigned int)(cap - 1));
    dst[cap - 1] = 0;
}

static unsigned char catalog_app_id_for_index(unsigned char index) {
    unsigned char first;

    first = launcher_first_app_index();
    if (index < first) {
        return 0xFFu;
    }
    return (unsigned char)(index - first);
}

static unsigned int catalog_text_offset(unsigned int base,
                                        unsigned char stride,
                                        unsigned char app_id) {
    return (unsigned int)(base + ((unsigned int)app_id * stride));
}

static void catalog_store_text(unsigned int base,
                               unsigned char stride,
                               unsigned char app_id,
                               const char *text) {
    unsigned char i;

    copy_text_limit(catalog_text_buf, stride, text);
    for (i = (unsigned char)strlen(catalog_text_buf); i < stride; ++i) {
        catalog_text_buf[i] = 0;
    }
    reu_dma_stash((unsigned int)catalog_text_buf,
                  REU_READYOS_GLOBAL_PHYSICAL(),
                  catalog_text_offset(base, stride, app_id),
                  stride);
}

static void catalog_fetch_text(unsigned int base,
                               unsigned char stride,
                               unsigned char app_id) {
    reu_dma_fetch((unsigned int)catalog_text_buf,
                  REU_READYOS_GLOBAL_PHYSICAL(),
                  catalog_text_offset(base, stride, app_id),
                  stride);
    catalog_text_buf[(unsigned char)(stride - 1u)] = 0;
}

static void catalog_store_entry(unsigned char index,
                                const char *prg,
                                const char *label,
                                const char *desc) {
    unsigned char app_id;

    app_id = catalog_app_id_for_index(index);
    if (app_id >= APP_SLOT_CAPACITY) {
        return;
    }
    catalog_store_text(REUCB_CATALOG_NAME_OFF, REUCB_CATALOG_NAME_SIZE,
                       app_id, label);
    catalog_store_text(REUCB_CATALOG_DESC_OFF, REUCB_CATALOG_DESC_SIZE,
                       app_id, desc);
    catalog_store_text(REUCB_CATALOG_FILE_OFF, REUCB_CATALOG_FILE_SIZE,
                       app_id, prg);
    catalog_store_text(REUCB_APP_META_OFF, REUCB_CATALOG_FILE_SIZE,
                       app_id, prg);
    catalog_invalidate_cache();
}

static void catalog_clear_entry(unsigned char index) {
    catalog_store_entry(index, "", "", "");
}

static void catalog_clear_all_entries(void) {
    unsigned char app_id;

    for (app_id = 0u; app_id < APP_SLOT_CAPACITY; ++app_id) {
        catalog_store_text(REUCB_CATALOG_NAME_OFF, REUCB_CATALOG_NAME_SIZE,
                           app_id, "");
        catalog_store_text(REUCB_CATALOG_DESC_OFF, REUCB_CATALOG_DESC_SIZE,
                           app_id, "");
        catalog_store_text(REUCB_CATALOG_FILE_OFF, REUCB_CATALOG_FILE_SIZE,
                           app_id, "");
        catalog_store_text(REUCB_APP_META_OFF, REUCB_CATALOG_FILE_SIZE,
                           app_id, "");
    }
    catalog_invalidate_cache();
}

static const char *catalog_name_for_index(unsigned char index) {
    unsigned char app_id;

    if (launcher_has_load_all_slot() && index == 0u) {
        return "LOAD ALL TO REU";
    }
    app_id = catalog_app_id_for_index(index);
    if (app_id >= APP_SLOT_CAPACITY) {
        catalog_text_buf[0] = 0;
        return catalog_text_buf;
    }
    catalog_fetch_text(REUCB_CATALOG_NAME_OFF, REUCB_CATALOG_NAME_SIZE, app_id);
    return catalog_text_buf;
}

static const char *catalog_desc_for_index(unsigned char index) {
    unsigned char app_id;

    if (launcher_has_load_all_slot() && index == 0u) {
        return "Load all apps from disk into REU";
    }
    app_id = catalog_app_id_for_index(index);
    if (app_id >= APP_SLOT_CAPACITY) {
        catalog_text_buf[0] = 0;
        return catalog_text_buf;
    }
    catalog_fetch_text(REUCB_CATALOG_DESC_OFF, REUCB_CATALOG_DESC_SIZE, app_id);
    return catalog_text_buf;
}

static const char *catalog_file_for_index(unsigned char index) {
    unsigned char app_id;

    app_id = catalog_app_id_for_index(index);
    if (app_id >= APP_SLOT_CAPACITY) {
        catalog_text_buf[0] = 0;
        return catalog_text_buf;
    }
    catalog_fetch_text(REUCB_CATALOG_FILE_OFF, REUCB_CATALOG_FILE_SIZE, app_id);
    return catalog_text_buf;
}

static void catalog_invalidate_cache(void) {
    catalog_cache_menu_start = 0xFFu;
}

static void catalog_refresh_name_cache(void) {
    unsigned char row;
    unsigned char menu_index;
    unsigned char app_index;

    if (catalog_cache_menu_start == menu.scroll_offset) {
        return;
    }
    catalog_cache_menu_start = menu.scroll_offset;
    for (row = 0u; row < APPS_HEIGHT; ++row) {
        menu_index = (unsigned char)(menu.scroll_offset + row);
        if (menu_index >= menu.count) {
            catalog_name_cache[row][0] = 0;
            continue;
        }
        if (launcher_menu_is_browse(menu_index)) {
            copy_text_limit(catalog_name_cache[row],
                            sizeof(catalog_name_cache[row]),
                            "BROWSE AND LOAD");
            continue;
        }
        app_index = launcher_menu_to_app_index(menu_index);
        copy_text_limit(catalog_name_cache[row],
                        sizeof(catalog_name_cache[row]),
                        catalog_name_for_index(app_index));
    }
}

static const char *catalog_menu_name(unsigned char menu_index) {
    unsigned char row;

    if (menu_index >= catalog_cache_menu_start) {
        row = (unsigned char)(menu_index - catalog_cache_menu_start);
        if (row < APPS_HEIGHT) {
            return catalog_name_cache[row];
        }
    }
    if (launcher_menu_is_browse(menu_index)) {
        return "BROWSE AND LOAD";
    }
    return catalog_name_for_index(launcher_menu_to_app_index(menu_index));
}

#if LAUNCHER_CFG_VERBOSE
static void copy_text_cap(char *dst, unsigned char cap, const char *src) {
    strncpy(dst, src, (unsigned int)(cap - 1));
    dst[cap - 1] = 0;
}

static void clear_cfg_diag(void) {
    cfg_err_line[0] = 0;
    cfg_err_prg[0] = 0;
    cfg_err_reason[0] = 0;
}

static void set_cfg_reason(const char *msg) {
    copy_text_cap(cfg_err_reason, (unsigned char)sizeof(cfg_err_reason), msg);
}
#else
#define clear_cfg_diag() ((void)0)
#define set_cfg_reason(msg) ((void)0)
#endif

#if !READYOS_LAUNCHER_VARIANT_EASYFLASH
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

static unsigned char cfg_read_line_lfn(unsigned char lfn, char *out, unsigned char cap) {
    unsigned char ch;
    unsigned char raw;
    unsigned char len = 0;
    int n;

    while (1) {
        n = cbm_read(lfn, &ch, 1);
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

static unsigned char cfg_read_line(char *out, unsigned char cap) {
    return cfg_read_line_lfn(APP_CFG_LFN, out, cap);
}
#endif

#if !READYOS_LAUNCHER_VARIANT_EASYFLASH
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
#if LAUNCHER_CFG_VERBOSE
    cfg_err_prg[0] = 0;
#endif

    trim_in_place(field_prg);
#if LAUNCHER_CFG_VERBOSE
    copy_text_cap(cfg_err_prg, (unsigned char)sizeof(cfg_err_prg), field_prg);
#endif
    prg_len = (unsigned char)strlen(field_prg);
    if (prg_len == 0) {
        set_cfg_reason(CFG_MSG_PRG_EMPTY);
        return CFG_ERR_PRG;
    }

    comma = strchr(field_prg, ',');
    if (comma != 0) {
        *out_detail = (unsigned char)comma[1];
        set_cfg_reason(CFG_MSG_PRG_COMMA);
        return CFG_ERR_PRG;
    }

    if (ends_with_suffix(field_prg, ".prg")) {
        set_cfg_reason(CFG_MSG_PRG_EXT);
        return CFG_ERR_PRG_EXT;
    }

    prg_len = (unsigned char)strlen(field_prg);
    if (prg_len == 0 || prg_len > MAX_FILE_LEN) {
        *out_detail = prg_len;
        set_cfg_reason(CFG_MSG_PRG_LEN);
        return CFG_ERR_PRG;
    }

    for (i = 0; i < prg_len; ++i) {
        if (!is_valid_prg_char((unsigned char)field_prg[i])) {
            *out_detail = (unsigned char)field_prg[i];
            set_cfg_reason(CFG_MSG_PRG_CHAR);
            return CFG_ERR_PRG;
        }
        out_prg[i] = field_prg[i];
    }
    out_prg[prg_len] = 0;
#if LAUNCHER_CFG_VERBOSE
    copy_text_cap(cfg_err_prg, (unsigned char)sizeof(cfg_err_prg), out_prg);
#endif
    return 0;
}

static unsigned char parse_resource_field(char *field_resource,
                                          unsigned char *out_resource_set,
                                          unsigned char *out_dep_line_required) {
    unsigned char len;

    trim_in_place(field_resource);
    *out_dep_line_required = 0u;
    len = (unsigned char)strlen(field_resource);
    if (len > 0u && field_resource[(unsigned char)(len - 1u)] == '+') {
        field_resource[(unsigned char)(len - 1u)] = 0;
        trim_in_place(field_resource);
        *out_dep_line_required = 1u;
    }
    if (field_resource[0] == 0) {
        *out_resource_set = APP_RESOURCE_NONE;
        return 0;
    }
    if (strcmp(field_resource, APP_RESOURCE_READYSHELL_TOKEN) == 0) {
        *out_resource_set = APP_RESOURCE_READYSHELL_OVL;
        return 0;
    }
    if (strcmp(field_resource, APP_RESOURCE_READYBASIC_TOKEN) == 0) {
        *out_resource_set = APP_RESOURCE_READYBASIC_CORE;
        return 0;
    }
    set_cfg_reason(CFG_MSG_RESOURCE);
    return CFG_ERR_RESOURCE;
}

static unsigned char parse_dep_hex_word(const char *text,
                                        unsigned int *out_value) {
    unsigned char i;
    unsigned char ch;
    unsigned int value = 0u;

    for (i = 0u; text[i] != 0; ++i) {
        ch = (unsigned char)text[i];
        value = (unsigned int)(value << 4);
        if (ch >= '0' && ch <= '9') {
            value = (unsigned int)(value + (unsigned int)(ch - '0'));
        } else if (ch >= 'a' && ch <= 'f') {
            value = (unsigned int)(value + (unsigned int)(10u + ch - 'a'));
        } else {
            return 0u;
        }
    }
    *out_value = value;
    return (unsigned char)(i != 0u);
}

static unsigned char parse_dependency_list_line(char *line,
                                                unsigned char default_drive,
                                                unsigned char resource_set,
                                                unsigned char *out_detail) {
    (void)default_drive;
    (void)resource_set;
    *out_detail = 0u;
    trim_in_place(line);
    if (line[0] == 0) {
        set_cfg_reason(CFG_MSG_RESOURCE);
        return CFG_ERR_RESOURCE;
    }
    return 0u;
}
#endif

static void catalog_init_defaults(void) {
    unsigned char i;

    for (i = 0; i < MAX_APPS; ++i) {
        apps_loaded[i] = 0;
        app_sizes[i] = 0;
        app_banks[i] = 0;
        app_drives[i] = DEFAULT_DRIVE;
        app_default_slots[i] = 0;
        app_resource_sets[i] = APP_RESOURCE_NONE;
        app_resource_loaded[i] = 0u;
        app_rs_bank1[i] = 0u;
        app_rs_bank2[i] = 0u;
        app_rs_bank3[i] = 0u;
        app_rs_bank4[i] = 0u;
    }

    launcher_cfg_load_all_to_reu = 0;
    launcher_variant_name[0] = 0;
    launcher_variant_boot_name[0] = 0;
    launcher_runappfirst_prg[0] = 0;
#if !READYOS_LAUNCHER_VARIANT_EASYFLASH && LAUNCHER_DMA_LOAD
    copy_text_limit(launcher_c64u_image_path, sizeof(launcher_c64u_image_path),
                    LAUNCHER_C64U_IMAGE_PATH_DEFAULT);
#endif
    launcher_notice[0] = 0;
    launcher_notice_color = TUI_COLOR_GRAY3;
    copy_text_limit(launcher_variant_name, sizeof(launcher_variant_name), "readyos");
    if (launcher_has_load_all_slot()) {
        app_count = 1;
    } else {
        app_count = 0;
    }
    catalog_invalidate_cache();
    cfg_err_phase = 0;
    clear_cfg_diag();
}

static void catalog_rebind_views(void) {
    catalog_invalidate_cache();
}

static void launcher_resume_save(unsigned char selected,
                                 unsigned char scroll_offset,
                                 unsigned char suppress_startup_once) {
    static ResumeWriteSegment segs[15];

    if (!resume_ready) {
        return;
    }

    launcher_resume_blob.selected = selected;
    launcher_resume_blob.scroll_offset = scroll_offset;
    launcher_resume_blob.suppress_startup_once = suppress_startup_once;
    launcher_resume_blob.reserved = 0;

    segs[0].ptr = &launcher_resume_blob;
    segs[0].len = sizeof(launcher_resume_blob);
    segs[1].ptr = &app_banks[0];
    segs[1].len = sizeof(app_banks);
    segs[2].ptr = &app_drives[0];
    segs[2].len = sizeof(app_drives);
    segs[3].ptr = &app_default_slots[0];
    segs[3].len = sizeof(app_default_slots);
    segs[4].ptr = &app_count;
    segs[4].len = sizeof(app_count);
    segs[5].ptr = &launcher_cfg_load_all_to_reu;
    segs[5].len = sizeof(launcher_cfg_load_all_to_reu);
    segs[6].ptr = &launcher_variant_name[0];
    segs[6].len = sizeof(launcher_variant_name);
    segs[7].ptr = &launcher_variant_boot_name[0];
    segs[7].len = sizeof(launcher_variant_boot_name);
    segs[8].ptr = &launcher_runappfirst_prg[0];
    segs[8].len = sizeof(launcher_runappfirst_prg);
    segs[9].ptr = &app_resource_sets[0];
    segs[9].len = sizeof(app_resource_sets);
    segs[10].ptr = &app_resource_loaded[0];
    segs[10].len = sizeof(app_resource_loaded);
    segs[11].ptr = &app_rs_bank1[0];
    segs[11].len = sizeof(app_rs_bank1);
    segs[12].ptr = &app_rs_bank2[0];
    segs[12].len = sizeof(app_rs_bank2);
    segs[13].ptr = &app_rs_bank3[0];
    segs[13].len = sizeof(app_rs_bank3);
    segs[14].ptr = &app_rs_bank4[0];
    segs[14].len = sizeof(app_rs_bank4);
    (void)resume_save_segments(segs, 15);
}

static unsigned char launcher_resume_restore(unsigned char *out_selected,
                                             unsigned char *out_scroll_offset,
                                             unsigned char *out_suppress_startup_once) {
    unsigned int payload_len = 0;
    static ResumeReadSegment segs[15];
    if (!resume_ready) {
        return 0;
    }
    segs[0].ptr = &launcher_resume_blob;
    segs[0].len = sizeof(launcher_resume_blob);
    segs[1].ptr = &app_banks[0];
    segs[1].len = sizeof(app_banks);
    segs[2].ptr = &app_drives[0];
    segs[2].len = sizeof(app_drives);
    segs[3].ptr = &app_default_slots[0];
    segs[3].len = sizeof(app_default_slots);
    segs[4].ptr = &app_count;
    segs[4].len = sizeof(app_count);
    segs[5].ptr = &launcher_cfg_load_all_to_reu;
    segs[5].len = sizeof(launcher_cfg_load_all_to_reu);
    segs[6].ptr = &launcher_variant_name[0];
    segs[6].len = sizeof(launcher_variant_name);
    segs[7].ptr = &launcher_variant_boot_name[0];
    segs[7].len = sizeof(launcher_variant_boot_name);
    segs[8].ptr = &launcher_runappfirst_prg[0];
    segs[8].len = sizeof(launcher_runappfirst_prg);
    segs[9].ptr = &app_resource_sets[0];
    segs[9].len = sizeof(app_resource_sets);
    segs[10].ptr = &app_resource_loaded[0];
    segs[10].len = sizeof(app_resource_loaded);
    segs[11].ptr = &app_rs_bank1[0];
    segs[11].len = sizeof(app_rs_bank1);
    segs[12].ptr = &app_rs_bank2[0];
    segs[12].len = sizeof(app_rs_bank2);
    segs[13].ptr = &app_rs_bank3[0];
    segs[13].len = sizeof(app_rs_bank3);
    segs[14].ptr = &app_rs_bank4[0];
    segs[14].len = sizeof(app_rs_bank4);
    if (!resume_load_segments(segs, 15, &payload_len)) {
        return 0;
    }
    if (payload_len != (sizeof(launcher_resume_blob) +
                        sizeof(app_banks) +
                        sizeof(app_drives) +
                        sizeof(app_default_slots) +
                        sizeof(app_count) +
                        sizeof(launcher_cfg_load_all_to_reu) +
                        sizeof(launcher_variant_name) +
                        sizeof(launcher_variant_boot_name) +
                        sizeof(launcher_runappfirst_prg) +
                        sizeof(app_resource_sets) +
                        sizeof(app_resource_loaded) +
                        sizeof(app_rs_bank1) +
                        sizeof(app_rs_bank2) +
                        sizeof(app_rs_bank3) +
                        sizeof(app_rs_bank4))) {
        return 0;
    }

    catalog_rebind_views();

    if (out_selected != 0 && launcher_resume_blob.selected < launcher_menu_count()) {
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

#if !READYOS_LAUNCHER_VARIANT_EASYFLASH
static unsigned char parse_catalog_entry_line(char *line,
                                              unsigned char *out_drive,
                                              char *out_prg,
                                              char *out_label,
                                              unsigned char *out_default_slot,
                                              unsigned char *out_resource_set,
                                              unsigned char *out_dep_line_required,
                                              unsigned char *out_detail) {
    char *first_colon;
    char *second_colon;
    char *third_colon;
    char *field_drive;
    char *field_prg;
    char *field_label;
    char *field_slot = 0;
    char *field_resource = 0;
    unsigned char i;
    unsigned int drive_val = 0;

    first_colon = strchr(line, ':');
    if (first_colon == 0) {
        set_cfg_reason(CFG_MSG_MISSING_COLON);
        return CFG_ERR_FORMAT;
    }
    *first_colon = 0;

    second_colon = strchr(first_colon + 1, ':');
    if (second_colon == 0) {
        set_cfg_reason(CFG_MSG_MISSING_COLON);
        return CFG_ERR_FORMAT;
    }
    *second_colon = 0;

    third_colon = strchr(second_colon + 1, ':');
    if (third_colon != 0) {
        *third_colon = 0;
        field_slot = third_colon + 1;
        field_resource = strchr(field_slot, ':');
        if (field_resource != 0) {
            *field_resource = 0;
            ++field_resource;
        }
        if (field_resource != 0 && strchr(field_resource, ':') != 0) {
            set_cfg_reason(CFG_MSG_HOTKEY_EXTRA);
            return CFG_ERR_HOTKEY;
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
    if (field_resource != 0) {
        trim_in_place(field_resource);
    }

    if (field_drive[0] == 0) {
        set_cfg_reason(CFG_MSG_DRIVE_EMPTY);
        return CFG_ERR_DRIVE;
    }

    for (i = 0; field_drive[i] != 0; ++i) {
        if (field_drive[i] < '0' || field_drive[i] > '9') {
            *out_detail = (unsigned char)field_drive[i];
            set_cfg_reason(CFG_MSG_DRIVE_NUMERIC);
            return CFG_ERR_DRIVE;
        }
        drive_val = drive_val * 10U + (unsigned int)(field_drive[i] - '0');
    }

    if (drive_val < 8U || drive_val > 11U) {
        *out_detail = (unsigned char)drive_val;
        set_cfg_reason(CFG_MSG_DRIVE_RANGE);
        return CFG_ERR_DRIVE;
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
        set_cfg_reason(CFG_MSG_LABEL_EMPTY);
        return CFG_ERR_LABEL;
    }
    strncpy(out_label, field_label, MAX_NAME_LEN);
    out_label[MAX_NAME_LEN] = 0;
    *out_default_slot = 0;
    *out_resource_set = APP_RESOURCE_NONE;
    *out_dep_line_required = 0u;

    if (field_slot != 0) {
        if ((field_slot[0] < '0' || field_slot[0] > '9') && field_resource == 0) {
            return parse_resource_field(field_slot, out_resource_set,
                                        out_dep_line_required);
        }
        if (field_slot[0] == 0) {
            if (field_resource == 0) {
                set_cfg_reason(CFG_MSG_HOTKEY_EMPTY);
                return CFG_ERR_HOTKEY;
            }
        }
        if (field_slot[0] != 0 && field_slot[1] != 0) {
            *out_detail = (unsigned char)field_slot[1];
            set_cfg_reason(CFG_MSG_HOTKEY_NUMERIC);
            return CFG_ERR_HOTKEY;
        }
        if (field_slot[0] != 0 && (field_slot[0] < '0' || field_slot[0] > '9')) {
            *out_detail = (unsigned char)field_slot[0];
            set_cfg_reason(CFG_MSG_HOTKEY_NUMERIC);
            return CFG_ERR_HOTKEY;
        }
        if (field_slot[0] == '0') {
            *out_detail = 0;
            set_cfg_reason(CFG_MSG_HOTKEY_RANGE);
            return CFG_ERR_HOTKEY;
        }
        if (field_slot[0] != 0) {
            *out_default_slot = (unsigned char)(field_slot[0] - '0');
        }
    }

    if (field_resource != 0) {
        return parse_resource_field(field_resource, out_resource_set,
                                    out_dep_line_required);
    }

    return 0;
}
#endif

static unsigned char add_catalog_entry(unsigned char drive,
                                       const char *prg,
                                       const char *label,
                                       const char *desc,
                                       unsigned char default_slot,
                                       unsigned char resource_set) {
    unsigned char idx;

    if (app_count >= MAX_APPS) {
        set_cfg_reason(CFG_MSG_TOO_MANY);
        return CFG_ERR_TOO_MANY;
    }

    idx = app_count;
    if (resource_set == APP_RESOURCE_NONE && strcmp(prg, "readyshell") == 0) {
        resource_set = APP_RESOURCE_READYSHELL_OVL;
    } else if (resource_set == APP_RESOURCE_NONE && strcmp(prg, "readybasic") == 0) {
        resource_set = APP_RESOURCE_READYBASIC_CORE;
    }
    app_banks[idx] = 0u;
    app_drives[idx] = drive;
    app_default_slots[idx] = default_slot;
    app_resource_sets[idx] = resource_set;
    app_resource_loaded[idx] = 0u;
    app_rs_bank1[idx] = 0u;
    app_rs_bank2[idx] = 0u;
    app_rs_bank3[idx] = 0u;
    app_rs_bank4[idx] = 0u;

    catalog_store_entry(idx, prg, label, desc);

    ++app_count;
    return 0;
}

#if READYOS_LAUNCHER_VARIANT_EASYFLASH
static unsigned char load_catalog_from_embedded(unsigned char *detail_a,
                                                unsigned char *detail_b,
                                                unsigned char *detail_c) {
    unsigned char i;
    unsigned char err;

    launcher_cfg_load_all_to_reu = 0;
    copy_text_limit(launcher_variant_name, sizeof(launcher_variant_name),
                    READYOS_EASYFLASH_VARIANT_NAME);
    copy_text_limit(launcher_variant_boot_name, sizeof(launcher_variant_boot_name),
                    READYOS_EASYFLASH_VARIANT_BOOT_NAME);
    if (READYOS_EASYFLASH_RUNAPPFIRST[0] != 0) {
        copy_text_limit(launcher_runappfirst_prg, sizeof(launcher_runappfirst_prg),
                        READYOS_EASYFLASH_RUNAPPFIRST);
    }

    for (i = 0; i < READYOS_EASYFLASH_APP_COUNT; ++i) {
        err = add_catalog_entry(DEFAULT_DRIVE,
                                readyos_easyflash_prgs[i],
                                readyos_easyflash_labels[i],
                                readyos_easyflash_descs[i],
                                readyos_easyflash_default_slots[i],
                                readyos_easyflash_resource_sets[i]);
        if (err != 0) {
            *detail_a = i;
            *detail_b = err;
            *detail_c = app_count;
            return err;
        }
        app_banks[(unsigned char)(app_count - 1)] = readyos_easyflash_app_banks[i];
    }

    *detail_a = 0;
    *detail_b = 0;
    *detail_c = 0;
    return 0;
}
#endif

#if !READYOS_LAUNCHER_VARIANT_EASYFLASH
static unsigned char load_catalog_from_disk(unsigned char *detail_a,
                                            unsigned char *detail_b,
                                            unsigned char *detail_c) {
    char line[REUCB_DEP_LINE_SIZE];
    char *key;
    char *value;
    char pending_prg[MAX_FILE_LEN + 1];
    char pending_label[MAX_NAME_LEN + 1];
    char pending_desc_text[MAX_DESC_LEN + 1];
    unsigned char pending_drive = 0;
    unsigned char pending_slot = 0;
    unsigned char pending_resource_set = APP_RESOURCE_NONE;
    unsigned char pending_dep_line_required = 0u;
    unsigned char entry_index = 1;
    unsigned char err;
    unsigned char parse_detail;
    unsigned char section = 0;
    unsigned char pending_state = 0;

    cfg_err_phase = CFG_ERR_PHASE_PARSE;
    if (cbm_open(APP_CFG_LFN, DEFAULT_DRIVE, 2, APP_CFG_OPEN_SPEC) != 0) {
        *detail_a = 0;
        *detail_b = 0;
        *detail_c = 0;
        set_cfg_reason(CFG_MSG_OPEN_FAIL);
        return CFG_ERR_OPEN;
    }

    while (cfg_read_line(line, sizeof(line))) {
        trim_in_place(line);
        lowercase_in_place(line);
        if (is_blank_or_comment(line)) {
            continue;
        }

#if LAUNCHER_CFG_VERBOSE
        copy_text_cap(cfg_err_line, (unsigned char)sizeof(cfg_err_line), line);
#endif

        if (line[0] == '[') {
            if (pending_state != 0u) {
                cbm_close(APP_CFG_LFN);
                *detail_a = entry_index;
                *detail_b = pending_drive;
                *detail_c = 0;
                set_cfg_reason((pending_state == 1u) ? CFG_MSG_DESC_MISSING
                                                      : CFG_MSG_RESOURCE);
                return (pending_state == 1u) ? CFG_ERR_MISSING_DESC
                                             : CFG_ERR_RESOURCE;
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
                            cbm_close(APP_CFG_LFN);
                            *detail_a = err;
                            *detail_b = 0;
                            *detail_c = parse_detail;
                            return err;
                        }
                    }
#if LAUNCHER_DMA_LOAD
                } else if (strcmp(key, "c64u_image_path") == 0) {
                    copy_text_limit(launcher_c64u_image_path,
                                    sizeof(launcher_c64u_image_path), value);
#endif
                }
            }
            continue;
        }

        if (section != 3) {
            continue;
        }

        if (pending_state == 0u) {
            parse_detail = 0;
            pending_slot = 0;
            pending_resource_set = APP_RESOURCE_NONE;
            pending_dep_line_required = 0u;
            err = parse_catalog_entry_line(line, &pending_drive, pending_prg,
                                           pending_label, &pending_slot,
                                           &pending_resource_set,
                                           &pending_dep_line_required,
                                           &parse_detail);
            if (err != 0) {
                cbm_close(APP_CFG_LFN);
                *detail_a = err;
                *detail_b = entry_index;
                *detail_c = parse_detail;
                return err;
            }
            pending_state = 1u;
            continue;
        }

        if (pending_state == 1u) {
            copy_text_limit(pending_desc_text, sizeof(pending_desc_text), line);
            if (pending_dep_line_required) {
                pending_state = 2u;
                continue;
            }
        } else {
            parse_detail = 0u;
            copy_text_limit((char *)launcher_dep_line_buf,
                            sizeof(launcher_dep_line_buf), line);
            err = parse_dependency_list_line(line, pending_drive,
                                             pending_resource_set,
                                             &parse_detail);
            if (err != 0u) {
                cbm_close(APP_CFG_LFN);
                *detail_a = err;
                *detail_b = entry_index;
                *detail_c = parse_detail;
                return err;
            }
        }

        err = add_catalog_entry(pending_drive, pending_prg, pending_label, pending_desc_text,
                                pending_slot, pending_resource_set);
        if (err != 0) {
            cbm_close(APP_CFG_LFN);
            *detail_a = entry_index;
            *detail_b = err;
            *detail_c = app_count;
            return err;
        }
        if (pending_dep_line_required) {
            launcher_control_write_dep_line((unsigned char)(app_count - 1u),
                                            (const char *)launcher_dep_line_buf);
        }

        pending_state = 0u;
        ++entry_index;
    }

    cbm_close(APP_CFG_LFN);

    if (pending_state != 0u) {
        *detail_a = entry_index;
        *detail_b = pending_drive;
        *detail_c = 0;
        set_cfg_reason((pending_state == 1u) ? CFG_MSG_DESC_MISSING
                                              : CFG_MSG_RESOURCE);
        return (pending_state == 1u) ? CFG_ERR_MISSING_DESC
                                     : CFG_ERR_RESOURCE;
    }

    if (launcher_variant_name[0] == 0) {
        copy_text_limit(launcher_variant_name, sizeof(launcher_variant_name), "readyos");
    }

    if (app_count <= launcher_first_app_index()) {
        *detail_a = 0;
        *detail_b = 0;
        *detail_c = 0;
        set_cfg_reason(CFG_MSG_NO_APPS);
        return CFG_ERR_EMPTY;
    }

    return 0;
}
#endif

static unsigned char validate_slot_contract(unsigned char *detail_a,
                                            unsigned char *detail_b,
                                            unsigned char *detail_c) {
    unsigned char i;
    unsigned char j;
    unsigned char bank_i;

    cfg_err_phase = CFG_ERR_PHASE_VALIDATE;
    if (app_count <= launcher_first_app_index() || app_count > MAX_APPS) {
        *detail_a = app_count;
        *detail_b = 0;
        *detail_c = 0;
        set_cfg_reason(CFG_MSG_COUNT_RANGE);
        return CFG_ERR_COUNT;
    }

    if (launcher_has_load_all_slot() && app_banks[0] != 0) {
        *detail_a = app_banks[0];
        *detail_b = 0;
        *detail_c = 0;
        set_cfg_reason(CFG_MSG_SLOT0);
        return CFG_ERR_OPEN;
    }

    for (i = launcher_first_app_index(); i < app_count; ++i) {
        if (catalog_file_for_index(i)[0] == 0) {
            *detail_a = i;
            *detail_b = app_banks[i];
            *detail_c = 0;
            set_cfg_reason(CFG_MSG_FILENAME_EMPTY);
            return CFG_ERR_MISSING_DESC;
        }
        if (app_drives[i] < 8 || app_drives[i] > 11) {
            *detail_a = i;
            *detail_b = app_drives[i];
            *detail_c = 0;
            set_cfg_reason(CFG_MSG_APP_DRIVE_RANGE);
            return CFG_ERR_DRIVE;
        }
        if (app_default_slots[i] > TUI_HOTKEY_SLOT_COUNT) {
            *detail_a = i;
            *detail_b = app_default_slots[i];
            *detail_c = 0;
            set_cfg_reason(CFG_MSG_HOTKEY_RANGE);
            return CFG_ERR_HOTKEY;
        }
        if (app_resource_sets[i] > APP_RESOURCE_READYBASIC_CORE) {
            *detail_a = i;
            *detail_b = app_resource_sets[i];
            *detail_c = 0;
            set_cfg_reason(CFG_MSG_RESOURCE);
            return CFG_ERR_RESOURCE;
        }
        bank_i = app_banks[i];
        if (bank_i != 0u) {
            if (launcher_logical_to_physical(bank_i) == 0xFFu) {
                *detail_a = i;
                *detail_b = bank_i;
                *detail_c = 0;
                set_cfg_reason(CFG_MSG_BANK_RANGE);
                return CFG_ERR_FORMAT;
            }
            for (j = (unsigned char)(i + 1); j < app_count; ++j) {
                if (app_banks[j] == bank_i) {
                    *detail_a = i;
                    *detail_b = j;
                    *detail_c = bank_i;
                    set_cfg_reason(CFG_MSG_DUP_BANK);
                    return CFG_ERR_TOO_MANY;
                }
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
#if CFG_SHOW_REASON
    const char *fallback_reason;
#else
    (void)err;
#endif

    phase_msg = (cfg_err_phase == CFG_ERR_PHASE_PARSE) ? CFG_PHASE_PARSE_TEXT : CFG_PHASE_VALIDATE_TEXT;
#if CFG_SHOW_REASON
    fallback_reason = "UNKNOWN";
    if (err == CFG_ERR_OPEN) fallback_reason = CFG_REASON_OPEN_BASE;
    else if (err == CFG_ERR_FORMAT) fallback_reason = CFG_REASON_FORMAT;
    else if (err == CFG_ERR_MISSING_DESC) fallback_reason = CFG_REASON_DESC;
    else if (err == CFG_ERR_TOO_MANY) fallback_reason = CFG_REASON_CAPACITY;
    else if (err == CFG_ERR_DRIVE) fallback_reason = CFG_REASON_DRIVE;
    else if (err == CFG_ERR_PRG) fallback_reason = CFG_REASON_PRG;
    else if (err == CFG_ERR_LABEL) fallback_reason = CFG_REASON_LABEL;
    else if (err == CFG_ERR_EMPTY) fallback_reason = CFG_REASON_EMPTY;
    else if (err == CFG_ERR_COUNT) fallback_reason = CFG_REASON_COUNT;
    else if (err == CFG_ERR_PRG_EXT) fallback_reason = CFG_REASON_PRG_EXT;
    else if (err == CFG_ERR_HOTKEY) fallback_reason = "HOTKEY SLOT ERROR";
    else if (err == CFG_ERR_RESOURCE) fallback_reason = "RESOURCE SET ERROR";
#endif

    tui_clear(TUI_COLOR_BLUE);
    {
        TuiRect title_box = {1, 0, 38, 3};
        tui_window_title(&title_box, CFG_TITLE_TEXT,
                         TUI_COLOR_LIGHTRED, TUI_COLOR_YELLOW);
    }
    tui_puts(2, 5, CFG_FAIL_TEXT, TUI_COLOR_WHITE);
    tui_puts(2, 7, "ERR:", TUI_COLOR_LIGHTRED);
    tui_print_uint(7, 7, err, TUI_COLOR_LIGHTRED);
    tui_puts(2, 9, "A:", TUI_COLOR_WHITE);
    tui_print_uint(5, 9, detail_a, TUI_COLOR_WHITE);
    tui_puts(11, 9, "B:", TUI_COLOR_WHITE);
    tui_print_uint(14, 9, detail_b, TUI_COLOR_WHITE);
    tui_puts(20, 9, "C:", TUI_COLOR_WHITE);
    tui_print_uint(23, 9, detail_c, TUI_COLOR_WHITE);

    tui_puts(2, 11, phase_msg, TUI_COLOR_YELLOW);
#if CFG_SHOW_REASON
    tui_puts_n(2, 13, "WHY:", 4, TUI_COLOR_LIGHTRED);
    if (cfg_err_reason[0] != 0) {
        tui_puts_n(7, 13, cfg_err_reason, 31, TUI_COLOR_WHITE);
    } else {
        tui_puts_n(7, 13, fallback_reason, 31, TUI_COLOR_WHITE);
    }

    #if LAUNCHER_CFG_VERBOSE
    if (cfg_err_phase == CFG_ERR_PHASE_PARSE) {
        tui_puts_n(2, 15, "LINE:", 5, TUI_COLOR_LIGHTRED);
        tui_puts_n(8, 15, cfg_err_line, 30, TUI_COLOR_WHITE);
        tui_puts_n(2, 16, "PRG:", 4, TUI_COLOR_LIGHTRED);
        tui_puts_n(7, 16, cfg_err_prg, 31, TUI_COLOR_WHITE);
    }
#endif
#endif

    tui_puts(2, 18, CFG_CHECK_TEXT, TUI_COLOR_LIGHTRED);
    tui_puts(2, 22, CFG_PRESS_TEXT, TUI_COLOR_WHITE);
    tui_getkey();
}

/*---------------------------------------------------------------------------
 * Helper: Set shim app name
 *---------------------------------------------------------------------------*/
static void set_shim_name(const char *name) {
    unsigned char len = 0;
    /* Shim filename buffer is $C824-$C82F (12 bytes). */
    while (name[len] && len < 12) {
        SHIM_APP_NAME[len] = name[len];
        ++len;
    }
    *SHIM_APP_NAMELEN = len;
}

/*---------------------------------------------------------------------------
 * Helper: Set shim REU params
 *---------------------------------------------------------------------------*/
static void set_shim_reu(unsigned char bank, unsigned int size) {
    *SHIM_APP_BANK = bank;
    *SHIM_APP_SIZE = size;
}

/*---------------------------------------------------------------------------
 * Helper: Patch shim load/preload device for per-app drive
 *---------------------------------------------------------------------------*/
static void set_shim_drive(unsigned char drive) {
    *SHIM_LOAD_DISK_DEV_IMM = drive;
    *SHIM_PRELOAD_DEV_IMM = drive;
}

static void launcher_mirror_reu_control(void) {
    reu_control_bank_sync_and_mirror(REUCB_WRITER_LAUNCHER);
    reu_control_bank_write_launcher_registry(
        launcher_first_app_index(),
        app_count,
        app_banks,
        app_drives,
        app_default_slots,
        app_resource_sets,
        app_resource_loaded,
        app_rs_bank1,
        app_rs_bank2,
        app_rs_bank3,
        app_rs_bank4,
        apps_loaded);
}

static unsigned int launcher_control_dep_line_off(unsigned char index) {
    return (unsigned int)(REUCB_DEP_LINE_OFF +
                          ((unsigned int)index * REUCB_DEP_LINE_SIZE));
}

static unsigned char launcher_control_app_id(unsigned char index) {
    unsigned char first;

    first = launcher_first_app_index();
    if (index < first) {
        return REUCB_NULL_REC;
    }
    return (unsigned char)(index - first);
}

static unsigned int launcher_control_rsrc_rec_off(unsigned char rec_index) {
    return (unsigned int)(REUCB_RSRC_REC_OFF +
                          ((unsigned int)rec_index * REUCB_RSRC_REC_SIZE));
}

#if !READYOS_LAUNCHER_VARIANT_EASYFLASH
static void launcher_control_write_dep_line(unsigned char index,
                                            const char *line) {
    unsigned char i;
    unsigned char control_bank;
    unsigned char app_id;

    app_id = launcher_control_app_id(index);
    if (app_id >= REUCB_DEP_LINE_COUNT) {
        return;
    }
    for (i = 0u; i < (unsigned char)(REUCB_DEP_LINE_SIZE - 1u) &&
                 line != 0 && line[i] != 0; ++i) {
        if (line != (const char *)launcher_dep_line_buf) {
            launcher_dep_line_buf[i] = (unsigned char)line[i];
        }
    }
    for (; i < REUCB_DEP_LINE_SIZE; ++i) {
        launcher_dep_line_buf[i] = 0u;
    }
    control_bank = REU_READYOS_GLOBAL_PHYSICAL();
    reu_dma_stash((unsigned int)launcher_dep_line_buf, control_bank,
                  launcher_control_dep_line_off(app_id), REUCB_DEP_LINE_SIZE);
}

static void launcher_control_read_dep_line(unsigned char index) {
    unsigned char control_bank;
    unsigned char app_id;

    app_id = launcher_control_app_id(index);
    if (app_id >= REUCB_DEP_LINE_COUNT) {
        launcher_dep_line_buf[0] = 0u;
        return;
    }
    control_bank = REU_READYOS_GLOBAL_PHYSICAL();
    reu_dma_fetch((unsigned int)launcher_dep_line_buf, control_bank,
                  launcher_control_dep_line_off(app_id), REUCB_DEP_LINE_SIZE);
    launcher_dep_line_buf[REUCB_DEP_LINE_SIZE - 1u] = 0u;
}
#endif

static void launcher_control_clear_resource_records(void) {
    unsigned char i;
    unsigned char control_bank;

    control_bank = REU_READYOS_GLOBAL_PHYSICAL();
    for (i = 0u; i < REUCB_RSRC_REC_SIZE; ++i) {
        launcher_rsrc_rec_buf[i] = 0u;
    }
    launcher_rsrc_rec_buf[0] = REUCB_NULL_REC;
    for (i = 0u; i < REUCB_RSRC_REC_COUNT; ++i) {
        reu_dma_stash((unsigned int)launcher_rsrc_rec_buf, control_bank,
                      launcher_control_rsrc_rec_off(i), REUCB_RSRC_REC_SIZE);
    }
}

static void launcher_control_clear_dependency_lines(void) {
    unsigned char i;
    unsigned char control_bank;

    control_bank = REU_READYOS_GLOBAL_PHYSICAL();
    memset(launcher_dep_line_buf, 0, sizeof(launcher_dep_line_buf));
    for (i = 0u; i < REUCB_DEP_LINE_COUNT; ++i) {
        reu_dma_stash((unsigned int)launcher_dep_line_buf, control_bank,
                      launcher_control_dep_line_off(i), REUCB_DEP_LINE_SIZE);
    }
}

static void launcher_control_clear_app_resource_records(unsigned char index) {
    unsigned char i;
    unsigned char control_bank;
    unsigned char app_id;

    app_id = launcher_control_app_id(index);
    if (app_id == REUCB_NULL_REC) {
        return;
    }
    control_bank = REU_READYOS_GLOBAL_PHYSICAL();
    for (i = 0u; i < REUCB_RSRC_REC_COUNT; ++i) {
        reu_dma_fetch((unsigned int)launcher_rsrc_rec_buf, control_bank,
                      launcher_control_rsrc_rec_off(i), REUCB_RSRC_REC_SIZE);
        if (launcher_rsrc_rec_buf[0] == app_id) {
            memset(launcher_rsrc_rec_buf, 0, sizeof(launcher_rsrc_rec_buf));
            launcher_rsrc_rec_buf[0] = REUCB_NULL_REC;
            reu_dma_stash((unsigned int)launcher_rsrc_rec_buf, control_bank,
                          launcher_control_rsrc_rec_off(i), REUCB_RSRC_REC_SIZE);
        }
    }
}

static void launcher_free_app_owned_alloc_records(unsigned char index) {
    unsigned char i;
    unsigned char app_id;
    unsigned char bank;
    unsigned char control_bank;

    app_id = launcher_control_app_id(index);
    if (app_id == REUCB_NULL_REC) {
        return;
    }

    control_bank = REU_READYOS_GLOBAL_PHYSICAL();
    for (i = 0u; i < REUCB_RSRC_REC_COUNT; ++i) {
        reu_dma_fetch((unsigned int)launcher_rsrc_rec_buf, control_bank,
                      launcher_control_rsrc_rec_off(i), REUCB_RSRC_REC_SIZE);
        if (launcher_rsrc_rec_buf[0] == app_id &&
            launcher_rsrc_rec_buf[2] == REUCB_DEP_KIND_APP_ALLOC) {
            bank = launcher_rsrc_rec_buf[3];
            if (bank != 0u && REU_ALLOC_TABLE[bank] == REU_APP_ALLOC) {
                REU_ALLOC_TABLE[bank] = REU_FREE;
            }
        }
    }
}

static unsigned char launcher_control_alloc_resource_record(void) {
    unsigned char i;
    unsigned char control_bank;

    control_bank = REU_READYOS_GLOBAL_PHYSICAL();
    for (i = 0u; i < REUCB_RSRC_REC_COUNT; ++i) {
        reu_dma_fetch((unsigned int)launcher_rsrc_rec_buf, control_bank,
                      launcher_control_rsrc_rec_off(i), REUCB_RSRC_REC_SIZE);
        if (launcher_rsrc_rec_buf[0] == REUCB_NULL_REC) {
            return i;
        }
    }
    return REUCB_NULL_REC;
}

static void launcher_control_write_resource_record(unsigned char rec_index,
                                                   unsigned char app_index,
                                                   unsigned char resource_set,
                                                   unsigned char kind,
                                                   unsigned char bank,
                                                   unsigned int offset,
                                                   unsigned int length,
                                                   unsigned char flags,
                                                   unsigned char slot_id,
                                                   unsigned char drive,
                                                   const char *name) {
    unsigned char i;
    unsigned char app_id;

    if (rec_index >= REUCB_RSRC_REC_COUNT) {
        return;
    }
    app_id = launcher_control_app_id(app_index);
    if (app_id == REUCB_NULL_REC) {
        return;
    }
    memset(launcher_rsrc_rec_buf, 0, sizeof(launcher_rsrc_rec_buf));
    launcher_rsrc_rec_buf[0] = app_id;
    launcher_rsrc_rec_buf[1] = resource_set;
    launcher_rsrc_rec_buf[2] = kind;
    launcher_rsrc_rec_buf[3] = bank;
    launcher_rsrc_rec_buf[4] = (unsigned char)(offset & 0xFFu);
    launcher_rsrc_rec_buf[5] = (unsigned char)(offset >> 8);
    launcher_rsrc_rec_buf[6] = (unsigned char)(length & 0xFFu);
    launcher_rsrc_rec_buf[7] = (unsigned char)(length >> 8);
    launcher_rsrc_rec_buf[8] = flags;
    launcher_rsrc_rec_buf[9] = REUCB_NULL_REC;
    launcher_rsrc_rec_buf[10] = slot_id;
    launcher_rsrc_rec_buf[11] = drive;
    for (i = 0u; i < 4u && name != 0 && name[i] != 0; ++i) {
        launcher_rsrc_rec_buf[(unsigned char)(12u + i)] = (unsigned char)name[i];
    }
    reu_dma_stash((unsigned int)launcher_rsrc_rec_buf, REU_READYOS_GLOBAL_PHYSICAL(),
                  launcher_control_rsrc_rec_off(rec_index), REUCB_RSRC_REC_SIZE);
}

static void launcher_init_readyshell_meta(void) {
    memset(launcher_rs_meta_buf, 0, sizeof(launcher_rs_meta_buf));
    launcher_rs_meta_buf[0] = 'O';
    launcher_rs_meta_buf[1] = 'V';
    launcher_rs_meta_buf[2] = READYSHELL_META_VERSION;
    launcher_rs_meta_buf[4] = (unsigned char)(READYSHELL_OVERLAY_SLOT_LEN & 0xFFu);
    launcher_rs_meta_buf[5] = (unsigned char)(READYSHELL_OVERLAY_SLOT_LEN >> 8);
}

static void launcher_add_readyshell_meta_record(unsigned char overlay_num,
                                                unsigned char bank,
                                                unsigned int offset) {
    unsigned char rec_off;
    unsigned short bit;

    if (overlay_num == 0u || overlay_num > READYSHELL_OVERLAY_COUNT) {
        return;
    }
    rec_off = (unsigned char)(READYSHELL_META_REC_OFF +
                              ((unsigned char)(overlay_num - 1u) *
                               READYSHELL_META_REC_LEN));
    launcher_rs_meta_buf[rec_off] = bank;
    launcher_rs_meta_buf[(unsigned char)(rec_off + 1u)] =
        (unsigned char)(offset & 0xFFu);
    launcher_rs_meta_buf[(unsigned char)(rec_off + 2u)] =
        (unsigned char)(offset >> 8);
    bit = (unsigned short)(1u << (overlay_num - 1u));
    launcher_rs_meta_buf[3] =
        (unsigned char)(launcher_rs_meta_buf[3] | (unsigned char)(bit & 0xFFu));
    launcher_rs_meta_buf[6] =
        (unsigned char)(launcher_rs_meta_buf[6] | (unsigned char)(bit >> 8));
}

static unsigned char launcher_readyshell_state_bank(unsigned char index) {
    return app_rs_bank4[index];
}

static void launcher_commit_readyshell_meta(unsigned char index) {
    unsigned char bank;

    bank = launcher_readyshell_state_bank(index);
    if (bank == 0u) {
        return;
    }
    reu_dma_stash((unsigned int)launcher_rs_meta_buf, bank,
                  READYSHELL_META_OFF, sizeof(launcher_rs_meta_buf));
}

static unsigned char launcher_restore_readyshell_meta(unsigned char index) {
    unsigned char i;
    unsigned char count = 0u;
    unsigned char app_id;
    unsigned int off;

    app_id = launcher_control_app_id(index);
    if (app_id == REUCB_NULL_REC) {
        return 0u;
    }
    launcher_init_readyshell_meta();
    for (i = 0u; i < REUCB_RSRC_REC_COUNT; ++i) {
        reu_dma_fetch((unsigned int)launcher_rsrc_rec_buf,
                      REU_READYOS_GLOBAL_PHYSICAL(),
                      launcher_control_rsrc_rec_off(i), REUCB_RSRC_REC_SIZE);
        if (launcher_rsrc_rec_buf[0] != app_id ||
            launcher_rsrc_rec_buf[2] != REUCB_DEP_KIND_RS_OVL) {
            continue;
        }
        off = (unsigned int)launcher_rsrc_rec_buf[4] |
              ((unsigned int)launcher_rsrc_rec_buf[5] << 8);
        launcher_add_readyshell_meta_record(launcher_rsrc_rec_buf[10],
                                            launcher_rsrc_rec_buf[3], off);
        ++count;
    }
    if (count != READYSHELL_OVERLAY_COUNT) {
        return 0u;
    }
    launcher_commit_readyshell_meta(index);
    return 1u;
}

#if !READYOS_LAUNCHER_VARIANT_EASYFLASH
static unsigned char launcher_alloc_physical_resource_bank(unsigned char type) {
    unsigned int bank;
    unsigned int first;

    first = (unsigned int)(*SHIM_REU_BANK_SKIP) + 2u;
    if (first >= REU_TOTAL_BANKS) {
        return 0u;
    }
    for (bank = first; bank < REU_TOTAL_BANKS; ++bank) {
        if (REU_ALLOC_TABLE[bank] == REU_FREE) {
            REU_ALLOC_TABLE[bank] = type;
            return (unsigned char)bank;
        }
    }
    return 0u;
}

static void launcher_zero_readyshell_meta(unsigned char index) {
    memset(launcher_rs_meta_buf, 0, sizeof(launcher_rs_meta_buf));
    if (app_rs_bank4[index] != 0u) {
        reu_dma_stash((unsigned int)launcher_rs_meta_buf, app_rs_bank4[index],
                      READYSHELL_META_OFF, sizeof(launcher_rs_meta_buf));
    }
}

static void launcher_mark_readybasic_banks(unsigned char index) {
    if (app_rs_bank1[index] != 0u) {
        REU_ALLOC_TABLE[app_rs_bank1[index]] = REU_RB_CORE;
    }
    if (app_rs_bank2[index] != 0u) {
        REU_ALLOC_TABLE[app_rs_bank2[index]] = REU_RB_CODE;
    }
}

static unsigned char launcher_ensure_readyshell_banks(unsigned char index) {
    if (app_rs_bank1[index] == 0u) {
        app_rs_bank1[index] = launcher_alloc_physical_resource_bank(REU_RS_CACHE);
    }
    if (app_rs_bank2[index] == 0u) {
        app_rs_bank2[index] = launcher_alloc_physical_resource_bank(REU_RS_CACHE);
    }
    if (app_rs_bank3[index] == 0u) {
        app_rs_bank3[index] = launcher_alloc_physical_resource_bank(REU_RS_CACHE);
    }
    if (app_rs_bank4[index] == 0u) {
        app_rs_bank4[index] = launcher_alloc_physical_resource_bank(REU_RS_SCRATCH);
    }
    return (unsigned char)(app_rs_bank1[index] != 0u &&
                           app_rs_bank2[index] != 0u &&
                           app_rs_bank3[index] != 0u &&
                           app_rs_bank4[index] != 0u);
}

static unsigned char launcher_ensure_readybasic_banks(unsigned char index) {
    if (app_rs_bank1[index] == 0u) {
        app_rs_bank1[index] = launcher_alloc_physical_resource_bank(REU_RB_CORE);
    }
    if (app_rs_bank2[index] == 0u) {
        app_rs_bank2[index] = launcher_alloc_physical_resource_bank(REU_RB_CODE);
    }
    if (app_rs_bank1[index] == 0u || app_rs_bank2[index] == 0u) {
        if (app_rs_bank1[index] != 0u) {
            REU_ALLOC_TABLE[app_rs_bank1[index]] = REU_FREE;
            app_rs_bank1[index] = 0u;
        }
        if (app_rs_bank2[index] != 0u) {
            REU_ALLOC_TABLE[app_rs_bank2[index]] = REU_FREE;
            app_rs_bank2[index] = 0u;
        }
        return 0u;
    }
    launcher_mark_readybasic_banks(index);
    return 1u;
}

static unsigned char launcher_build_resource_open_spec(const char *name) {
    unsigned char len;

    len = (unsigned char)strlen(name);
    if (len == 0u || len + 4u >= sizeof(launcher_resource_open_spec)) {
        return 0u;
    }
    strcpy(launcher_resource_open_spec, name);
    strcat(launcher_resource_open_spec, ",p,r");
    return 1u;
}

static void launcher_zero_reu_range(unsigned char bank,
                                    unsigned int reu_off,
                                    unsigned int len) {
    unsigned int pos;
    unsigned int chunk;

    memset(launcher_resource_buf, 0, sizeof(launcher_resource_buf));
    pos = 0u;
    while (pos < len) {
        chunk = (unsigned int)(len - pos);
        if (chunk > sizeof(launcher_resource_buf)) {
            chunk = sizeof(launcher_resource_buf);
        }
        reu_dma_stash((unsigned int)launcher_resource_buf, bank,
                      (unsigned int)(reu_off + pos), chunk);
        pos = (unsigned int)(pos + chunk);
    }
}

#if !READYOS_LAUNCHER_VARIANT_EASYFLASH && LAUNCHER_DMA_LOAD
static unsigned char launcher_dma_hex(unsigned char value) {
    value &= 0x0Fu;
    return (unsigned char)(value < 10u ? ('0' + value)
                                       : ('A' + (value - 10u)));
}

static unsigned char launcher_dma_check_available(void) {
    if (!launcher_dma_checked) {
        launcher_dma_checked = 1u;
        if (launcher_c64u_image_path[0] == 0u) {
            launcher_dma_available = 0u;
        } else {
            launcher_dma_available = 1u;
        }
    }
    return launcher_dma_available;
}

static unsigned char launcher_dma_try_prg_to_reu(unsigned char drive,
                                                 const char *name,
                                                 unsigned char bank,
                                                 unsigned int reu_off,
                                                 unsigned int max_len,
                                                 unsigned int load_addr) {
    char *slash;
    char *cursor;
    char *dir_start;
    unsigned char i;
    unsigned char dir_len;
    unsigned char img_len;

    /* Hardware note: these screen breadcrumbs are not just cosmetic.
     * The C64U UCI/DOS transaction proved code-shape/pacing sensitive;
     * replacing them with private RAM breadcrumbs made DMA fall back to
     * the KERNAL path on hardware. The success dialog clears this row
     * before drawing OK, while failures leave useful diagnostics. */
    launcher_dma_breadcrumb = 0x31u;
    (*(volatile unsigned char*)0x052D) = 0x31;
    if ((drive != 8u && drive != 9u) || name[0] == 0u) {
        return 0u;
    }
    for (i = 0u; i < MAX_FILE_LEN && name[i] != 0; ++i) {
        launcher_uci_dma_name_buf[i] = name[i];
    }
    if (name[i] != 0) {
        return 0u;
    }
    launcher_uci_dma_name_buf[i] = 0;
    launcher_dma_breadcrumb = 0x32u;
    (*(volatile unsigned char*)0x052D) = 0x32;
    if (launcher_c64u_image_path[0] == 0u) {
        return 0u;
    }
    launcher_dma_breadcrumb = 0x33u;
    (*(volatile unsigned char*)0x052D) = 0x33;
    if (!launcher_dma_check_available()) {
        return 0u;
    }
    launcher_dma_breadcrumb = 0x34u;
    (*(volatile unsigned char*)0x052D) = 0x34;

    slash = 0;
    cursor = launcher_c64u_image_path;
    while (*cursor != 0) {
        if (*cursor == '/') {
            slash = cursor;
        }
        ++cursor;
    }
    if (slash == 0 || slash[1] == 0) {
        return 0u;
    }
    if (launcher_c64u_image_path[0] == '/') {
        if (slash == launcher_c64u_image_path) {
            launcher_uci_dma_dir_buf[0] = '/';
            launcher_uci_dma_dir_buf[1] = 0;
        } else {
            dir_start = launcher_c64u_image_path + 1;
            dir_len = (unsigned char)(slash - dir_start);
            if (dir_len == 0u || dir_len >= sizeof(launcher_uci_dma_dir_buf)) {
                return 0u;
            }
            for (i = 0u; i < dir_len; ++i) {
                launcher_uci_dma_dir_buf[i] = dir_start[i];
            }
            launcher_uci_dma_dir_buf[dir_len] = 0;
        }
        img_len = (unsigned char)(cursor - (slash + 1));
        if (img_len == 0u || img_len >= sizeof(launcher_uci_dma_image_buf)) {
            return 0u;
        }
        for (i = 0u; i < img_len; ++i) {
            launcher_uci_dma_image_buf[i] = slash[1 + i];
            launcher_uci_dma_mount_buf[i] = slash[1 + i];
        }
        launcher_uci_dma_image_buf[img_len] = 0;
        launcher_uci_dma_mount_buf[img_len] = 0;
    } else {
        dir_start = launcher_c64u_image_path;
        dir_len = (unsigned char)(slash - dir_start);
        if (dir_len == 0u || dir_len >= sizeof(launcher_uci_dma_dir_buf)) {
            return 0u;
        }
        for (i = 0u; i < dir_len; ++i) {
            launcher_uci_dma_dir_buf[i] = dir_start[i];
        }
        launcher_uci_dma_dir_buf[dir_len] = 0;
        img_len = (unsigned char)(cursor - (slash + 1));
        if (img_len == 0u || img_len >= sizeof(launcher_uci_dma_image_buf)) {
            return 0u;
        }
        for (i = 0u; i < img_len; ++i) {
            launcher_uci_dma_image_buf[i] = slash[1 + i];
            launcher_uci_dma_mount_buf[i] = slash[1 + i];
        }
        launcher_uci_dma_image_buf[img_len] = 0;
        launcher_uci_dma_mount_buf[img_len] = 0;
    }
    launcher_uci_dma_image_dir = launcher_uci_dma_dir_buf;
    launcher_uci_dma_image_name = launcher_uci_dma_image_buf;
    launcher_uci_dma_mount_name = launcher_uci_dma_mount_buf;
    launcher_uci_dma_name = launcher_uci_dma_name_buf;
    launcher_uci_dma_reu_bank = bank;
    launcher_uci_dma_reu_offset = reu_off;
    launcher_uci_dma_max_len = max_len;
    launcher_uci_dma_expected_load_addr = load_addr;
    launcher_dma_breadcrumb = 0x35u;
    (*(volatile unsigned char*)0x052D) = 0x35;
    if (launcher_uci_dma_load_prg()) {
        launcher_dma_available = 1u;
        launcher_dma_used = 1u;
        return 1u;
    }
    launcher_dma_breadcrumb = launcher_uci_dma_last_error;
    (*(volatile unsigned char*)0x052E) =
        launcher_dma_hex((unsigned char)(launcher_uci_dma_last_error >> 4));
    (*(volatile unsigned char*)0x052F) =
        launcher_dma_hex(launcher_uci_dma_last_error);
    (*(volatile unsigned char*)0x0530) =
        launcher_dma_hex((unsigned char)(launcher_uci_dma_dbg_stat0 >> 4));
    (*(volatile unsigned char*)0x0531) =
        launcher_dma_hex(launcher_uci_dma_dbg_stat0);
    (*(volatile unsigned char*)0x0532) =
        launcher_dma_hex((unsigned char)(launcher_uci_dma_dbg_stat1 >> 4));
    (*(volatile unsigned char*)0x0533) =
        launcher_dma_hex(launcher_uci_dma_dbg_stat1);
    launcher_dma_available = 0u;
    return 0u;
}
#endif

static unsigned char launcher_stream_prg_to_reu(unsigned char drive,
                                                const char *name,
                                                unsigned char bank,
                                                unsigned int reu_off) {
    unsigned char load_hdr[2];
    unsigned int load_addr;
    unsigned int pos;
    unsigned int chunk;
    int n;

#if !READYOS_LAUNCHER_VARIANT_EASYFLASH && LAUNCHER_DMA_LOAD
    launcher_zero_reu_range(bank, reu_off, READYSHELL_OVERLAY_SLOT_LEN);
    if (launcher_dma_try_prg_to_reu(drive, name, bank, reu_off,
                                    READYSHELL_OVERLAY_SLOT_LEN,
                                    READYSHELL_OVERLAY_LOAD_ADDR)) {
        return 1u;
    }
#endif

    if (!launcher_build_resource_open_spec(name)) {
        return 0u;
    }
    if (cbm_open(READYSHELL_OVERLAY_LFN, drive, 2,
                 launcher_resource_open_spec) != 0) {
        return 0u;
    }
    n = cbm_read(READYSHELL_OVERLAY_LFN, load_hdr, 2u);
    if (n != 2) {
        cbm_close(READYSHELL_OVERLAY_LFN);
        return 0u;
    }
    load_addr = (unsigned int)load_hdr[0] | ((unsigned int)load_hdr[1] << 8);
    if (load_addr != READYSHELL_OVERLAY_LOAD_ADDR) {
        cbm_close(READYSHELL_OVERLAY_LFN);
        return 0u;
    }

    launcher_zero_reu_range(bank, reu_off, READYSHELL_OVERLAY_SLOT_LEN);
    pos = 0u;
    while (pos < READYSHELL_OVERLAY_SLOT_LEN) {
        n = cbm_read(READYSHELL_OVERLAY_LFN, launcher_resource_buf,
                     sizeof(launcher_resource_buf));
        if (n <= 0) {
            break;
        }
        chunk = (unsigned int)n;
        if ((unsigned int)(pos + chunk) > READYSHELL_OVERLAY_SLOT_LEN) {
            cbm_close(READYSHELL_OVERLAY_LFN);
            return 0u;
        }
        reu_dma_stash((unsigned int)launcher_resource_buf, bank,
                      (unsigned int)(reu_off + pos), chunk);
        pos = (unsigned int)(pos + chunk);
    }
    cbm_close(READYSHELL_OVERLAY_LFN);
    return (unsigned char)(pos != 0u);
}

static unsigned char launcher_readyshell_physical_bank(unsigned char index,
                                                       unsigned char ordinal) {
    if (ordinal == 0u) {
        return app_rs_bank1[index];
    }
    if (ordinal == 1u) {
        return app_rs_bank2[index];
    }
    if (ordinal == 2u) {
        return app_rs_bank3[index];
    }
    return 0u;
}

static unsigned char launcher_parse_rs_dep_entry(char *entry,
                                                 char **out_name,
                                                 unsigned char *out_ordinal,
                                                 unsigned int *out_offset) {
    char *at;
    char *colon;

    trim_in_place(entry);
    at = strchr(entry, '@');
    colon = at ? strchr(at, ':') : 0;
    if (entry[0] == 0 || at == 0 || colon == 0) {
        return 0u;
    }
    *at = 0;
    *colon = 0;
    trim_in_place(entry);
    trim_in_place((char *)(at + 1));
    trim_in_place((char *)(colon + 1));
    if ((at + 1)[0] < '0' || (at + 1)[0] > '2' || (at + 1)[1] != 0) {
        return 0u;
    }
    if (!parse_dep_hex_word((char *)(colon + 1), out_offset)) {
        return 0u;
    }
    *out_name = entry;
    *out_ordinal = (unsigned char)((at + 1)[0] - '0');
    return 1u;
}

static unsigned char launcher_load_readyshell_resources(unsigned char index) {
    char *cursor;
    char *comma;
    char *name;
    unsigned char bank;
    unsigned char ordinal;
    unsigned char overlay_num = 1u;
    unsigned char rec_index;
    unsigned int reu_off;

    if (!launcher_ensure_readyshell_banks(index)) {
        launcher_set_notice("rs fail banks", TUI_COLOR_LIGHTRED);
        return 0u;
    }

    launcher_control_read_dep_line(index);
    if (launcher_dep_line_buf[0] == 0u) {
        launcher_set_notice("rs fail dep", TUI_COLOR_LIGHTRED);
        return 0u;
    }
    launcher_control_clear_app_resource_records(index);
    launcher_init_readyshell_meta();
    rec_index = launcher_control_alloc_resource_record();
    if (rec_index == REUCB_NULL_REC) {
        launcher_set_notice("rs fail state", TUI_COLOR_LIGHTRED);
        return 0u;
    }
    launcher_control_write_resource_record(rec_index, index,
                                           APP_RESOURCE_READYSHELL_OVL,
                                           REUCB_DEP_KIND_RS_STATE,
                                           app_rs_bank4[index], 0u,
                                           0xFFFFu, 1u, 0u,
                                           app_drives[index], "rsst");

    cursor = (char *)launcher_dep_line_buf;
    while (cursor != 0 && cursor[0] != 0) {
        comma = strchr(cursor, ',');
        if (comma != 0) {
            *comma = 0;
        }
        if (!launcher_parse_rs_dep_entry(cursor, &name, &ordinal, &reu_off)) {
            launcher_set_notice("rs fail parse", TUI_COLOR_LIGHTRED);
            return 0u;
        }
        bank = launcher_readyshell_physical_bank(index, ordinal);
        if (bank == 0u ||
            !launcher_stream_prg_to_reu(app_drives[index], name, bank, reu_off)) {
            launcher_set_notice("rs fail load", TUI_COLOR_LIGHTRED);
            return 0u;
        }
        rec_index = launcher_control_alloc_resource_record();
        if (rec_index == REUCB_NULL_REC) {
            launcher_set_notice("rs fail rec", TUI_COLOR_LIGHTRED);
            return 0u;
        }
        launcher_control_write_resource_record(rec_index, index,
                                               APP_RESOURCE_READYSHELL_OVL,
                                               REUCB_DEP_KIND_RS_OVL,
                                               bank, reu_off,
                                               READYSHELL_OVERLAY_SLOT_LEN,
                                               1u, overlay_num,
                                               app_drives[index], name);
        launcher_add_readyshell_meta_record(overlay_num, bank, reu_off);
        ++overlay_num;
        if (comma == 0) {
            break;
        }
        cursor = (char *)(comma + 1);
    }

    if (overlay_num != (unsigned char)(READYSHELL_OVERLAY_COUNT + 1u)) {
        launcher_set_notice("rs fail count", TUI_COLOR_LIGHTRED);
        return 0u;
    }
    launcher_commit_readyshell_meta(index);
    *READYSHELL_STATE_BANK_CACHE = app_rs_bank4[index];
    app_resource_loaded[index] = 1u;
    launcher_mirror_reu_control();
    return 1u;
}

static unsigned char launcher_load_readybasic_resources(unsigned char index) {
    unsigned char rec_index;

    if (!launcher_ensure_readybasic_banks(index)) {
        return 0u;
    }
    launcher_control_clear_app_resource_records(index);
    rec_index = launcher_control_alloc_resource_record();
    if (rec_index == REUCB_NULL_REC) {
        return 0u;
    }
    launcher_control_write_resource_record(rec_index, index,
                                           APP_RESOURCE_READYBASIC_CORE,
                                           REUCB_DEP_KIND_RB_CORE,
                                           app_rs_bank1[index], 0u, 0u,
                                           1u, 1u, app_drives[index],
                                           "rbcore");
    rec_index = launcher_control_alloc_resource_record();
    if (rec_index == REUCB_NULL_REC) {
        return 0u;
    }
    launcher_control_write_resource_record(rec_index, index,
                                           APP_RESOURCE_READYBASIC_CORE,
                                           REUCB_DEP_KIND_RB_CODE,
                                           app_rs_bank2[index], 0u, 0u,
                                           1u, 2u, app_drives[index],
                                           "rbcode");
    app_resource_loaded[index] = 1u;
    launcher_mirror_reu_control();
    return 1u;
}
#endif

static unsigned char launcher_prepare_app_resources(unsigned char index) {
    if (index >= app_count) {
        return 0u;
    }
    if (app_resource_sets[index] == APP_RESOURCE_NONE) {
        return 1u;
    }
    if (app_resource_loaded[index]) {
        if (app_resource_sets[index] == APP_RESOURCE_READYSHELL_OVL) {
            if (app_rs_bank4[index] != 0u) {
                REU_ALLOC_TABLE[app_rs_bank4[index]] = REU_RS_SCRATCH;
                *READYSHELL_STATE_BANK_CACHE = app_rs_bank4[index];
            }
            return launcher_restore_readyshell_meta(index);
        } else if (app_resource_sets[index] == APP_RESOURCE_READYBASIC_CORE) {
#if !READYOS_LAUNCHER_VARIANT_EASYFLASH
            launcher_mark_readybasic_banks(index);
#endif
        } else {
            return 0u;
        }
        return 1u;
    }
#if READYOS_LAUNCHER_VARIANT_EASYFLASH
    return 0u;
#else
    if (app_resource_sets[index] == APP_RESOURCE_READYSHELL_OVL) {
        return launcher_load_readyshell_resources(index);
    }
    if (app_resource_sets[index] == APP_RESOURCE_READYBASIC_CORE) {
        return launcher_load_readybasic_resources(index);
    }
    return 0u;
#endif
}

#if !READYOS_LAUNCHER_VARIANT_EASYFLASH
static void launcher_free_app_resources(unsigned char index) {
    if (index >= app_count) {
        return;
    }
    if (app_resource_sets[index] == APP_RESOURCE_READYSHELL_OVL) {
        launcher_zero_readyshell_meta(index);
    }
    launcher_free_app_owned_alloc_records(index);
    launcher_control_clear_app_resource_records(index);
    if (app_rs_bank1[index] != 0u) {
        REU_ALLOC_TABLE[app_rs_bank1[index]] = REU_FREE;
    }
    if (app_rs_bank2[index] != 0u) {
        REU_ALLOC_TABLE[app_rs_bank2[index]] = REU_FREE;
    }
    if (app_rs_bank3[index] != 0u) {
        REU_ALLOC_TABLE[app_rs_bank3[index]] = REU_FREE;
    }
    if (app_rs_bank4[index] != 0u) {
        REU_ALLOC_TABLE[app_rs_bank4[index]] = REU_FREE;
    }
    app_resource_loaded[index] = 0u;
    app_rs_bank1[index] = 0u;
    app_rs_bank2[index] = 0u;
    app_rs_bank3[index] = 0u;
    app_rs_bank4[index] = 0u;
}
#endif


/*---------------------------------------------------------------------------
 * Save launcher state to REU bank 0
 *---------------------------------------------------------------------------*/
static void save_launcher_to_reu(void) {
    REU_C64_LO = 0x00;
    REU_C64_HI = 0x10;
    REU_REU_LO = 0x00;
    REU_REU_HI = 0x00;
    REU_REU_BANK = REU_LAUNCHER_PHYSICAL();
    REU_LEN_LO = 0x00;
    REU_LEN_HI = 0xB6;  /* $B600 bytes */
    REU_COMMAND = REU_CMD_STASH;
}

/*---------------------------------------------------------------------------
 * Load single app from disk to REU
 * Uses shim's preload routine at $C809 which:
 * 1. Stashes launcher to REU bank 0
 * 2. Loads app from disk to $1000
 * 3. Stashes app to target REU bank
 * 4. Fetches launcher back from REU bank 0
 * 5. Returns via RTS
 *---------------------------------------------------------------------------*/
static unsigned int load_app_to_reu(unsigned char index) {
    const char *filename;
    unsigned char bank;
    unsigned char loaded_in_bitmap;
    unsigned int end_addr;
    unsigned int file_size;

    if (!launcher_is_app_slot(index)) {
        return 0;
    }

    filename = catalog_file_for_index(index);
    if (filename[0] == 0) {
        return 0;
    }
    bank = launcher_resolve_snapshot_bank(index);
    if (bank == 0) {
        return 0;
    }

    /* Set target bank in shim data area */
    *SHIM_APP_BANK = bank;

    /* Set filename in shim data area */
    set_shim_name(filename);

    set_shim_drive(app_drives[index]);

#if !READYOS_LAUNCHER_VARIANT_EASYFLASH && LAUNCHER_DMA_LOAD
    {
        unsigned char physical;
        /* See launcher_dma_try_prg_to_reu(): these volatile stores preserve
         * the hardware-proven UCI transaction shape. */
        launcher_dma_breadcrumb = 0x41u;
        (*(volatile unsigned char*)0x052C) = 0x31;
        physical = launcher_logical_to_physical(bank);
        launcher_dma_breadcrumb = 0x42u;
        (*(volatile unsigned char*)0x052C) = 0x32;
        if (physical != 0xFFu) {
            launcher_dma_breadcrumb = 0x43u;
            (*(volatile unsigned char*)0x052C) = 0x33;
            if (launcher_dma_try_prg_to_reu(app_drives[index], filename,
                                            physical, 0u, APP_SAVE_SIZE,
                                            APP_LOAD_START)) {
                file_size = launcher_uci_dma_loaded_size;
                *SHIM_APP_SIZE = file_size;
                if (bank < 24u) {
                    shim_bitmap_set_bank(bank);
                }
                apps_loaded[index] = 1;
                app_sizes[index] = file_size;
                if (!launcher_prepare_app_resources(index)) {
                    apps_loaded[index] = 0;
                    app_sizes[index] = 0;
                    shim_bitmap_clear_bank(bank);
                    launcher_set_notice_if_empty("app resources failed",
                                                 TUI_COLOR_LIGHTRED);
                    launcher_mirror_reu_control();
                    return 0;
                }
                launcher_bind_default_hotkey_for_index(index);
                launcher_mirror_reu_control();
                return file_size;
            }
        }
    }
#endif

    /* Call shim's preload routine - it handles everything and returns */
    __asm__("jsr $C809");

    /* Read actual file size from shim data area.
     * Shim saves KERNAL LOAD end address at $C830-$C831.
     * If this value is invalid, treat load as failure. */
    end_addr = ((unsigned int)(*(unsigned char*)0xC831) << 8)
             | (*(unsigned char*)0xC830);
    loaded_in_bitmap = (bank < 24u) ? shim_bitmap_has_bank(bank) : 0u;

    if (end_addr <= APP_LOAD_START || end_addr > APP_LOAD_END_EXCL) {
        /* Some preload paths can leave end_addr invalid while still stashing app. */
        if (loaded_in_bitmap) {
            apps_loaded[index] = 1;
            app_sizes[index] = APP_SAVE_SIZE;
            if (!launcher_prepare_app_resources(index)) {
                apps_loaded[index] = 0;
                app_sizes[index] = 0;
                launcher_set_notice_if_empty("app resources failed", TUI_COLOR_LIGHTRED);
                launcher_mirror_reu_control();
                return 0;
            }
            launcher_bind_default_hotkey_for_index(index);
            launcher_mirror_reu_control();
            return APP_SAVE_SIZE;
        }
        apps_loaded[index] = 0;
        app_sizes[index] = 0;
        shim_bitmap_clear_bank(bank);
        launcher_mirror_reu_control();
        return 0;
    }

    file_size = end_addr - APP_LOAD_START;
    if (file_size > APP_SAVE_SIZE) {
        if (loaded_in_bitmap) {
            apps_loaded[index] = 1;
            app_sizes[index] = APP_SAVE_SIZE;
            if (!launcher_prepare_app_resources(index)) {
                apps_loaded[index] = 0;
                app_sizes[index] = 0;
                launcher_set_notice_if_empty("app resources failed", TUI_COLOR_LIGHTRED);
                launcher_mirror_reu_control();
                return 0;
            }
            launcher_bind_default_hotkey_for_index(index);
            launcher_mirror_reu_control();
            return APP_SAVE_SIZE;
        }
        apps_loaded[index] = 0;
        app_sizes[index] = 0;
        shim_bitmap_clear_bank(bank);
        launcher_mirror_reu_control();
        return 0;
    }

    /* Treat bitmap as authoritative: only mark loaded if target bit is set. */
    loaded_in_bitmap = (bank < 24u) ? shim_bitmap_has_bank(bank) : 1u;
    if (bank < 24u && !loaded_in_bitmap) {
        apps_loaded[index] = 0;
        app_sizes[index] = 0;
        launcher_mirror_reu_control();
        return 0;
    }

    /* On return, launcher is back in memory and app is valid in REU. */
    apps_loaded[index] = 1;
    app_sizes[index] = file_size;
    if (!launcher_prepare_app_resources(index)) {
        apps_loaded[index] = 0;
        app_sizes[index] = 0;
        launcher_set_notice_if_empty("app resources failed", TUI_COLOR_LIGHTRED);
        launcher_mirror_reu_control();
        return 0;
    }
    launcher_bind_default_hotkey_for_index(index);
    launcher_mirror_reu_control();
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

static void launcher_set_notice_if_empty(const char *msg, unsigned char color) {
    if (launcher_notice[0] == 0) {
        launcher_set_notice(msg, color);
    }
}

static unsigned char launcher_find_app_by_prg(const char *prg) {
    unsigned char i;

    for (i = launcher_first_app_index(); i < app_count; ++i) {
        if (strcmp(catalog_file_for_index(i), prg) == 0) {
            return i;
        }
    }
    return app_count;
}

/*---------------------------------------------------------------------------
 * Load all apps to REU
 *---------------------------------------------------------------------------*/
static unsigned char load_all_to_reu_internal(unsigned char interactive) {
    unsigned char i;
    unsigned int size;
    unsigned char total_to_load;
    unsigned char loaded_count;
    unsigned char y;
    unsigned char bar_y;
    unsigned char counter_y;
    unsigned char loaded_ok;
    unsigned char retried;
    unsigned char missing_count;
    unsigned char bitmap_complete;
    unsigned char status_x;
    unsigned char done_x;
    unsigned char separator_x;
    unsigned char success;
    static unsigned char missing_slots[MAX_APPS];

    sync_from_reu_bitmap();

    /* Count how many apps need loading */
    total_to_load = 0;
    for (i = launcher_first_app_index(); i < app_count; ++i) {
        if (launcher_is_app_slot(i) && !apps_loaded[i]) {
            ++total_to_load;
        }
    }

    if (total_to_load == 0) {
        if (interactive) {
            tui_clear(TUI_COLOR_BLUE);
            tui_puts(4, 10, "ALL APPS ALREADY IN REU!", TUI_COLOR_LIGHTGREEN);
            tui_puts(13, 14, "PRESS ANY KEY...", TUI_COLOR_WHITE);
            tui_getkey();
        }
        return 1;
    }

    tui_clear(TUI_COLOR_BLUE);
    {
        TuiRect title_box = {1, 0, 38, 3};
        tui_window_title(&title_box, "LOADING APPS TO REU",
                         TUI_COLOR_LIGHTBLUE, TUI_COLOR_YELLOW);
    }

    /* Keep progress fixed; reuse list rows for catalogs larger than the screen. */
    counter_y = 23;
    bar_y = 24;

    /* Draw empty unified progress bar */
    tui_progress_bar(4, bar_y, 32, 0, total_to_load,
                     TUI_COLOR_LIGHTGREEN, TUI_COLOR_GRAY2);
    tui_puts(4, counter_y, "0/", TUI_COLOR_WHITE);
    tui_print_uint(6, counter_y, total_to_load, TUI_COLOR_WHITE);

    status_x = 24;
    loaded_count = 0;
    missing_count = 0;
    for (i = launcher_first_app_index(); i < app_count; ++i) {
        if (!launcher_is_app_slot(i)) continue;
        if (apps_loaded[i]) continue;

        y = (unsigned char)(LOAD_ALL_LIST_Y +
                            (loaded_count % LOAD_ALL_LIST_ROWS));
        tui_puts_n(0, y, "", TUI_SCREEN_WIDTH, TUI_COLOR_WHITE);

        /* Display app name with LOADING status */
        draw_drive_prefixed_name(4, y, i, TUI_COLOR_CYAN, 16);
        tui_puts(status_x, y, "LOADING...", TUI_COLOR_YELLOW);

        /* Load app; retry once if target bank bit was not set. */
        retried = 0;
        size = load_app_to_reu(i);
        sync_from_reu_bitmap();
        loaded_ok = apps_loaded[i];
        if (!loaded_ok) {
            size = load_app_to_reu(i);
            sync_from_reu_bitmap();
            loaded_ok = apps_loaded[i];
            retried = 1;
        }

        /* Update line status */
        tui_puts_n(status_x, y, "", 16, TUI_COLOR_WHITE);  /* Clear status area */
        draw_drive_prefixed_name(4, y, i, TUI_COLOR_CYAN, APP_NAME_WIDTH);

        if (loaded_ok) {
            tui_puts(30, y, "OK", TUI_COLOR_LIGHTGREEN);
            tui_print_uint(33, y, size / 1024, TUI_COLOR_GRAY3);
            tui_puts(37, y, "KB", TUI_COLOR_GRAY3);
            if (retried) {
                tui_puts(status_x, y, "RETRY", TUI_COLOR_YELLOW);
            }
        } else {
            tui_puts(30, y, "FAIL", TUI_COLOR_LIGHTRED);
            if (!missing_list_contains(missing_slots, missing_count, i)) {
                missing_slots[missing_count] = i;
                ++missing_count;
            }
        }

        ++loaded_count;

        /* Update unified progress bar */
        tui_progress_bar(4, bar_y, 32, loaded_count, total_to_load,
                         TUI_COLOR_LIGHTGREEN, TUI_COLOR_GRAY2);
        tui_puts_n(4, counter_y, "", 8, TUI_COLOR_WHITE);  /* Clear counter */
        tui_print_uint(4, counter_y, loaded_count, TUI_COLOR_WHITE);
        separator_x = 5;
        if (loaded_count >= 10) {
            ++separator_x;
        }
        if (loaded_count >= 100) {
            ++separator_x;
        }
        tui_puts(separator_x, counter_y, "/", TUI_COLOR_WHITE);
        tui_print_uint((unsigned char)(separator_x + 1), counter_y,
                       total_to_load, TUI_COLOR_WHITE);
    }

    /* Final authoritative check: all catalog-assigned banks must be present. */
    sync_from_reu_bitmap();
    bitmap_complete = required_slots_loaded();

    for (i = launcher_first_app_index(); i < app_count; ++i) {
        if (launcher_is_app_slot(i) && !apps_loaded[i] &&
            !missing_list_contains(missing_slots, missing_count, i)) {
            missing_slots[missing_count] = i;
            ++missing_count;
        }
    }

    tui_puts_n(2, counter_y, "", 38, TUI_COLOR_WHITE);
    success = (unsigned char)(missing_count == 0 && bitmap_complete);

    if (success) {
        done_x = 9;
        if (interactive) {
            tui_puts(done_x, counter_y, "DONE! PRESS ANY KEY...", TUI_COLOR_WHITE);
        }
    } else {
        done_x = 4;
        if (interactive) {
            tui_puts(done_x, counter_y, "INCOMPLETE LOAD. PRESS ANY KEY...", TUI_COLOR_LIGHTRED);
        }
    }

    if (interactive) {
        tui_getkey();
    }
    return success;
}

static void load_all_to_reu(void) {
    (void)load_all_to_reu_internal(1);
}

#if !READYOS_LAUNCHER_VARIANT_EASYFLASH
static void launcher_clear_hotkey_bank(unsigned char bank) {
    unsigned char slot;

    if (bank == 0u) {
        return;
    }
    for (slot = 0u; slot < TUI_HOTKEY_SLOT_COUNT; ++slot) {
        if (TUI_HOTKEY_BINDINGS[slot] == bank) {
            TUI_HOTKEY_BINDINGS[slot] = 0u;
        }
    }
}
#endif

static void launcher_bind_default_hotkey_for_index(unsigned char index) {
    unsigned char slot;

    if (index >= app_count || app_banks[index] == 0u) {
        return;
    }
    slot = app_default_slots[index];
    if (slot == 0u || slot > TUI_HOTKEY_SLOT_COUNT) {
        return;
    }
    if (TUI_HOTKEY_BINDINGS[(unsigned char)(slot - 1u)] == 0u) {
        TUI_HOTKEY_BINDINGS[(unsigned char)(slot - 1u)] = app_banks[index];
    }
}

#if READYOS_LAUNCHER_VARIANT_EASYFLASH
#if READYOS_EASYFLASH_RS_OVERLAY_COUNT != READYSHELL_OVERLAY_COUNT
#error EasyFlash ReadyShell overlay catalog count must match launcher runtime
#endif
static void launcher_write_easyflash_readyshell_records(unsigned char index) {
    unsigned char i;
    unsigned char rec_index;

    launcher_control_clear_app_resource_records(index);
    launcher_init_readyshell_meta();
    rec_index = launcher_control_alloc_resource_record();
    if (rec_index == REUCB_NULL_REC) {
        return;
    }
    launcher_control_write_resource_record(rec_index, index,
                                           APP_RESOURCE_READYSHELL_OVL,
                                           REUCB_DEP_KIND_RS_STATE,
                                           app_rs_bank4[index], 0u,
                                           0xFFFFu, 1u, 0u,
                                           app_drives[index], "rsst");
    for (i = 0u; i < READYOS_EASYFLASH_RS_OVERLAY_COUNT; ++i) {
        rec_index = launcher_control_alloc_resource_record();
        if (rec_index == REUCB_NULL_REC) {
            return;
        }
        launcher_control_write_resource_record(rec_index, index,
                                               APP_RESOURCE_READYSHELL_OVL,
                                               REUCB_DEP_KIND_RS_OVL,
                                               readyos_easyflash_rs_overlay_banks[i],
                                               readyos_easyflash_rs_overlay_offsets[i],
                                               READYSHELL_OVERLAY_SLOT_LEN,
                                               1u, (unsigned char)(i + 1u),
                                               app_drives[index],
                                               readyos_easyflash_rs_overlay_names[i]);
        launcher_add_readyshell_meta_record((unsigned char)(i + 1u),
                                            readyos_easyflash_rs_overlay_banks[i],
                                            readyos_easyflash_rs_overlay_offsets[i]);
    }
    launcher_commit_readyshell_meta(index);
}

static void launcher_write_easyflash_readybasic_records(unsigned char index) {
    unsigned char rec_index;

    launcher_control_clear_app_resource_records(index);
    rec_index = launcher_control_alloc_resource_record();
    if (rec_index != REUCB_NULL_REC) {
        launcher_control_write_resource_record(rec_index, index,
                                               APP_RESOURCE_READYBASIC_CORE,
                                               REUCB_DEP_KIND_RB_CORE,
                                               app_rs_bank1[index], 0u, 0u,
                                               1u, 1u, app_drives[index],
                                               "rbcore");
    }
    rec_index = launcher_control_alloc_resource_record();
    if (rec_index != REUCB_NULL_REC) {
        launcher_control_write_resource_record(rec_index, index,
                                               APP_RESOURCE_READYBASIC_CORE,
                                               REUCB_DEP_KIND_RB_CODE,
                                               app_rs_bank2[index], 0u, 0u,
                                               1u, 2u, app_drives[index],
                                               "rbcode");
    }
}

static void launcher_mark_embedded_preloads_loaded(void) {
    unsigned char i;
    unsigned char bank;
    unsigned char physical;

    for (i = launcher_first_app_index(); i < app_count; ++i) {
        if (!launcher_is_app_slot(i)) {
            continue;
        }
        bank = app_banks[i];
        if (bank == 0u) {
            continue;
        }
        physical = launcher_logical_to_physical(bank);
        if (physical != 0xFFu) {
            REU_ALLOC_TABLE[physical] = REU_APP_STATE;
        }
        if (app_resource_sets[i] == APP_RESOURCE_READYSHELL_OVL) {
            app_rs_bank1[i] = READYOS_EASYFLASH_RS_CACHE_BANK1;
            app_rs_bank2[i] = READYOS_EASYFLASH_RS_CACHE_BANK2;
            app_rs_bank3[i] = READYOS_EASYFLASH_RS_CACHE_BANK3;
            app_rs_bank4[i] = READYOS_EASYFLASH_RS_STATE_BANK;
            launcher_mark_bank_if_available(app_rs_bank1[i], REU_RS_CACHE);
            launcher_mark_bank_if_available(app_rs_bank2[i], REU_RS_CACHE);
            launcher_mark_bank_if_available(app_rs_bank3[i], REU_RS_CACHE);
            launcher_mark_bank_if_available(app_rs_bank4[i], REU_RS_SCRATCH);
            app_resource_loaded[i] = 1u;
            launcher_write_easyflash_readyshell_records(i);
        } else if (app_resource_sets[i] == APP_RESOURCE_READYBASIC_CORE) {
            app_rs_bank1[i] = READYOS_EASYFLASH_RB_CORE_BANK;
            app_rs_bank2[i] = READYOS_EASYFLASH_RB_CODE_BANK;
            app_rs_bank3[i] = 0u;
            launcher_mark_bank_if_available(app_rs_bank1[i], REU_RB_CORE);
            launcher_mark_bank_if_available(app_rs_bank2[i], REU_RB_CODE);
            app_resource_loaded[i] = 1u;
            launcher_write_easyflash_readybasic_records(i);
        }
        apps_loaded[i] = 1u;
        app_sizes[i] = APP_SAVE_SIZE;
    }
}
#endif

#if !READYOS_LAUNCHER_VARIANT_EASYFLASH
static void unload_selected_from_reu(unsigned char index) {
    unsigned char bank;

    if (!launcher_is_app_slot(index)) {
        return;
    }
    bank = app_banks[index];
    if (bank == 0u || !apps_loaded[index]) {
        launcher_set_notice("selected app not loaded", TUI_COLOR_GRAY3);
        return;
    }

    launcher_clear_hotkey_bank(bank);
    launcher_free_snapshot_bank(index);
    launcher_set_notice("selected app unloaded from reu", TUI_COLOR_LIGHTGREEN);
    launcher_resume_save(menu.selected, menu.scroll_offset, 0u);
}
#endif

/*---------------------------------------------------------------------------
 * Load selected app to REU (F3)
 *---------------------------------------------------------------------------*/
static void load_selected_to_reu(unsigned char index) {
#if READYOS_LAUNCHER_VARIANT_EASYFLASH
    unsigned char loaded_ok;
    unsigned char total;

    total = (unsigned char)(app_count - launcher_first_app_index());
    sync_from_reu_bitmap();
    loaded_ok = (unsigned char)(launcher_is_app_slot(index) && apps_loaded[index]);

    tui_clear(TUI_COLOR_BLUE);
    {
        TuiRect title_box = {1, 0, 38, 3};
        tui_window_title(&title_box, "CARTRIDGE STATUS",
                         TUI_COLOR_LIGHTBLUE, TUI_COLOR_YELLOW);
    }
    tui_puts(4, 6, "ALL APPS PRELOADED IN BOOTER", TUI_COLOR_LIGHTGREEN);
    tui_puts(4, 8, "CATALOG APPS:", TUI_COLOR_WHITE);
    tui_print_uint(18, 8, total, TUI_COLOR_WHITE);
    if (launcher_is_app_slot(index)) {
        draw_drive_prefixed_name(4, 10, index, TUI_COLOR_CYAN, 24);
        tui_puts(4, 12,
                 loaded_ok ? "SELECTED APP READY IN REU"
                           : "SELECTED APP MISSING FROM REU",
                 loaded_ok ? TUI_COLOR_LIGHTGREEN : TUI_COLOR_LIGHTRED);
    }
    tui_puts(9, 16, "PRESS ANY KEY...", TUI_COLOR_WHITE);
    tui_getkey();
    return;
#else
    unsigned int size;
    unsigned char loaded_ok;

    if (!launcher_is_app_slot(index)) {
        return;
    }

    sync_from_reu_bitmap();
    if (apps_loaded[index]) {
        return;
    }

    tui_clear(TUI_COLOR_BLUE);
    {
        TuiRect title_box = {1, 0, 38, 3};
        tui_window_title(&title_box, "LOADING TO REU",
                         TUI_COLOR_LIGHTBLUE, TUI_COLOR_YELLOW);
    }

    /* Display app name */
    draw_drive_prefixed_name(4, 5, index, TUI_COLOR_CYAN, 20);
    tui_puts(4, 7, "LOADING...", TUI_COLOR_YELLOW);

    /* Load the app (blocking) */
    size = load_app_to_reu(index);
    sync_from_reu_bitmap();
    loaded_ok = apps_loaded[index];

    /* Show result */
    tui_puts_n(4, 7, "", 32, TUI_COLOR_WHITE);

    if (loaded_ok) {
        tui_puts(4, 7, "OK", TUI_COLOR_LIGHTGREEN);
        tui_puts(8, 7, "-", TUI_COLOR_WHITE);
        tui_print_uint(10, 7, size / 1024, TUI_COLOR_GRAY3);
        tui_puts(15, 7, "KB", TUI_COLOR_GRAY3);
    } else {
        tui_puts(4, 7, "FAILED", TUI_COLOR_LIGHTRED);
    }

    tui_puts(4, 10, "PRESS ANY KEY...", TUI_COLOR_WHITE);
    tui_getkey();
#endif
}

#if !READYOS_LAUNCHER_VARIANT_EASYFLASH
static void launcher_show_message(const char *title,
                                  const char *msg,
                                  unsigned char color) {
    TuiRect title_box;

    tui_clear(TUI_COLOR_BLUE);
    title_box.x = 1u;
    title_box.y = 0u;
    title_box.w = 38u;
    title_box.h = 3u;
    tui_window_title(&title_box, title, TUI_COLOR_LIGHTBLUE, TUI_COLOR_YELLOW);
    tui_puts_n(2u, 8u, msg, 36u, color);
    tui_puts(10u, 14u, "PRESS ANY KEY...", TUI_COLOR_WHITE);
    tui_getkey();
}

static unsigned char manifest_name_starts_app(const char *name) {
    unsigned char i;
    unsigned char ch;
    static const char prefix[] = APP_MANIFEST_PREFIX;

    for (i = 0u; prefix[i] != 0; ++i) {
        ch = (unsigned char)name[i];
        if (ch >= 'A' && ch <= 'Z') {
            ch = (unsigned char)(ch + ('a' - 'A'));
        }
        if (ch != (unsigned char)prefix[i]) {
            return 0u;
        }
    }
    return 1u;
}

static unsigned char build_manifest_open_spec(const char *name) {
    unsigned char len;

    len = (unsigned char)strlen(name);
    if (len == 0u || len + 4u >= sizeof(launcher_manifest_open_spec)) {
        return 0u;
    }
    strcpy(launcher_manifest_open_spec, name);
    strcat(launcher_manifest_open_spec, ",s,r");
    return 1u;
}

static unsigned char parse_manifest_from_disk(unsigned char manifest_drive,
                                              const char *manifest_name,
                                              unsigned char *out_index,
                                              unsigned char *out_existing) {
    char line[REUCB_DEP_LINE_SIZE];
    char pending_prg[MAX_FILE_LEN + 1];
    char pending_label[MAX_NAME_LEN + 1];
    char pending_desc[MAX_DESC_LEN + 1];
    unsigned char pending_drive = 0u;
    unsigned char pending_slot = 0u;
    unsigned char pending_resource_set = APP_RESOURCE_NONE;
    unsigned char pending_dep_line_required = 0u;
    unsigned char state = 0u;
    unsigned char err;
    unsigned char parse_detail = 0u;
    unsigned char existing;

    *out_index = 0u;
    *out_existing = 0u;

    if (!build_manifest_open_spec(manifest_name)) {
        return CFG_ERR_PRG;
    }

    if (cbm_open(APP_MANIFEST_LFN, manifest_drive, 2,
                 launcher_manifest_open_spec) != 0) {
        return CFG_ERR_OPEN;
    }

    while (cfg_read_line_lfn(APP_MANIFEST_LFN, line, sizeof(line))) {
        trim_in_place(line);
        lowercase_in_place(line);
        if (is_blank_or_comment(line)) {
            continue;
        }

        if (state == 0u) {
            pending_slot = 0u;
            pending_resource_set = APP_RESOURCE_NONE;
            pending_dep_line_required = 0u;
            parse_detail = 0u;
            err = parse_catalog_entry_line(line, &pending_drive, pending_prg,
                                           pending_label, &pending_slot,
                                           &pending_resource_set,
                                           &pending_dep_line_required,
                                           &parse_detail);
            if (err != 0u) {
                cbm_close(APP_MANIFEST_LFN);
                return err;
            }
            state = 1u;
            continue;
        }

        if (state == 1u) {
            copy_text_limit(pending_desc, sizeof(pending_desc), line);
            state = pending_dep_line_required ? 2u : 3u;
            continue;
        }

        if (state == 2u) {
            parse_detail = 0u;
            copy_text_limit((char *)launcher_dep_line_buf,
                            sizeof(launcher_dep_line_buf), line);
            err = parse_dependency_list_line(line, pending_drive,
                                             pending_resource_set,
                                             &parse_detail);
            if (err != 0u) {
                cbm_close(APP_MANIFEST_LFN);
                return err;
            }
            state = 3u;
            continue;
        }

        cbm_close(APP_MANIFEST_LFN);
        return CFG_ERR_FORMAT;
    }

    cbm_close(APP_MANIFEST_LFN);

    if (state == 0u) {
        return CFG_ERR_EMPTY;
    }
    if (state == 1u) {
        return CFG_ERR_MISSING_DESC;
    }
    if (state == 2u) {
        return CFG_ERR_RESOURCE;
    }

    existing = launcher_find_app_by_prg(pending_prg);
    if (existing < app_count) {
        *out_index = existing;
        *out_existing = 1u;
        if (pending_dep_line_required) {
            launcher_control_write_dep_line(existing,
                                            (const char *)launcher_dep_line_buf);
        }
        return 0u;
    }

    if (app_count >= MAX_APPS) {
        return CFG_ERR_TOO_MANY;
    }

    err = add_catalog_entry(pending_drive, pending_prg, pending_label,
                            pending_desc, pending_slot, pending_resource_set);
    if (err != 0u) {
        return err;
    }
    if (pending_dep_line_required) {
        launcher_control_write_dep_line((unsigned char)(app_count - 1u),
                                        (const char *)launcher_dep_line_buf);
    }

    catalog_rebind_views();
    menu.count = launcher_menu_count();
    *out_index = (unsigned char)(app_count - 1u);
    return 0u;
}

static void rollback_manifest_app(unsigned char index) {
    if (index == 0u || index >= app_count || index + 1u != app_count) {
        return;
    }

    apps_loaded[index] = 0u;
    app_sizes[index] = 0u;
    app_banks[index] = 0u;
    launcher_free_app_resources(index);
    app_drives[index] = DEFAULT_DRIVE;
    app_default_slots[index] = 0u;
    catalog_clear_entry(index);
    --app_count;
    catalog_rebind_views();
    menu.count = launcher_menu_count();
}

static void browse_and_load_manifest(void) {
    static const FileDialogConfig cfg = {
        "BROWSE APP MANIFEST",
        "LOAD",
        "NO SEQ FILES FOUND",
        CBM_T_SEQ,
        1u
    };
    unsigned char rc;
    unsigned char manifest_drive;
    unsigned char index;
    unsigned char existing;
    unsigned int size;

    storage_device_set_default(DEFAULT_DRIVE);
    rc = file_dialog_pick(&launcher_file_dialog, &cfg, &launcher_manifest_entry);
    if (rc == FILE_DIALOG_RC_CANCEL) {
        return;
    }
    if (rc != FILE_DIALOG_RC_OK) {
        launcher_show_message("BROWSE FAILED", "DISK READ ERROR",
                              TUI_COLOR_LIGHTRED);
        return;
    }

    manifest_drive = storage_device_get_default();
    if (!manifest_name_starts_app(launcher_manifest_entry.name)) {
        launcher_show_message("NOT APP MANIFEST", "FILENAME MUST START APP.",
                              TUI_COLOR_LIGHTRED);
        return;
    }

    rc = parse_manifest_from_disk(manifest_drive, launcher_manifest_entry.name,
                                  &index, &existing);
    if (rc != 0u) {
        launcher_show_message("MANIFEST ERROR", "CANNOT READ APP MANIFEST",
                              TUI_COLOR_LIGHTRED);
        return;
    }

    menu.selected = launcher_app_to_menu_index(index);
    launcher_sync_visible_window();
    sync_from_reu_bitmap();

    if (existing) {
        if (!apps_loaded[index]) {
            load_selected_to_reu(index);
        } else {
            launcher_set_notice("manifest app already in reu", TUI_COLOR_LIGHTGREEN);
        }
        launcher_resume_save(menu.selected, menu.scroll_offset, 0u);
        return;
    }

    tui_clear(TUI_COLOR_BLUE);
    {
        TuiRect title_box = {1u, 0u, 38u, 3u};
        tui_window_title(&title_box, "LOADING MANIFEST APP",
                         TUI_COLOR_LIGHTBLUE, TUI_COLOR_YELLOW);
    }
    draw_drive_prefixed_name(4u, 6u, index, TUI_COLOR_CYAN, 24u);
    tui_puts(4u, 8u, "LOADING TO REU...", TUI_COLOR_YELLOW);

    size = load_app_to_reu(index);
    sync_from_reu_bitmap();
    if (!apps_loaded[index]) {
        rollback_manifest_app(index);
        launcher_show_message("LOAD FAILED", "APP PRG DID NOT LOAD",
                              TUI_COLOR_LIGHTRED);
        return;
    }

    launcher_seed_default_hotkeys();
    launcher_resume_save(menu.selected, menu.scroll_offset, 0u);
    tui_puts_n(4u, 8u, "", 20u, TUI_COLOR_WHITE);
    tui_puts(4u, 8u, "LOADED TO REU", TUI_COLOR_LIGHTGREEN);
    tui_print_uint(18u, 8u, size / 1024u, TUI_COLOR_GRAY3);
    tui_puts(23u, 8u, "KB", TUI_COLOR_GRAY3);
    tui_puts(10u, 14u, "PRESS ANY KEY...", TUI_COLOR_WHITE);
    tui_getkey();
}
#endif

/*---------------------------------------------------------------------------
 * Launch app from REU (fast)
 *---------------------------------------------------------------------------*/
static void launch_from_reu(unsigned char index) {
    unsigned char bank;
    unsigned int size;

    if (!apps_loaded[index]) return;

    bank = launcher_resolve_snapshot_bank(index);
    if (bank == 0) return;
    if (!launcher_prepare_app_resources(index)) {
        launcher_set_notice_if_empty("app resources failed", TUI_COLOR_LIGHTRED);
        return;
    }
    size = app_sizes[index];

    launcher_set_startup_suppressed();
    launcher_resume_save(launcher_app_to_menu_index(index), menu.scroll_offset, 1);

    /* Save current launcher state to REU bank 0 first */
    save_launcher_to_reu();

    /* Set REU params in shim */
    set_shim_reu(bank, size);
    set_shim_drive(app_drives[index]);

    /* Set current app bank so apps can return to launcher */
    *SHIM_CURRENT_BANK = bank;

    /* Call shim to DMA from REU and run - jump table at $C803 */
    __asm__("jmp $C803");
}

/*---------------------------------------------------------------------------
 * Launch app from disk (slow)
 *---------------------------------------------------------------------------*/
#if !READYOS_LAUNCHER_VARIANT_EASYFLASH
static void launch_from_disk(unsigned char index) {
    const char *filename;
    unsigned char bank;

    if (catalog_file_for_index(index)[0] == 0) return;
    bank = launcher_resolve_snapshot_bank(index);
    if (bank == 0) return;
    if (!launcher_prepare_app_resources(index)) {
        launcher_set_notice_if_empty("app resources failed", TUI_COLOR_LIGHTRED);
        return;
    }
    launcher_bind_default_hotkey_for_index(index);

    tui_clear(TUI_COLOR_BLUE);
    tui_puts(4, 5, "LOADING FROM DISK:", TUI_COLOR_WHITE);
    draw_drive_prefixed_name(23, 5, index, TUI_COLOR_CYAN, 12);
    tui_puts(4, 7, "PLEASE WAIT...", TUI_COLOR_YELLOW);

    /* Set filename in shim */
    filename = catalog_file_for_index(index);
    set_shim_name(filename);
    set_shim_drive(app_drives[index]);

    launcher_set_startup_suppressed();
    launcher_resume_save(launcher_app_to_menu_index(index), menu.scroll_offset, 1);

    /* Save launcher to REU first so we can return to it */
    save_launcher_to_reu();

    /* Set current app bank so apps can return to launcher */
    *SHIM_CURRENT_BANK = bank;

    /* Call shim to load and run - use jump table entry at $C800 */
    __asm__("jmp $C800");
}
#endif

/*---------------------------------------------------------------------------
 * Launch app (from REU if available, else disk)
 *---------------------------------------------------------------------------*/
static void launch_app(unsigned char index) {
    if (!slot_contract_ok) {
        return;
    }

    if (launcher_has_load_all_slot() && index == 0u) {
        load_all_to_reu();
        return;
    }

    if (!launcher_is_app_slot(index)) {
        tui_puts(4, STATUS_Y + 2, "NOT AVAILABLE", TUI_COLOR_LIGHTRED);
        return;
    }

    /* Bitmap is authoritative for REU presence. */
    sync_from_reu_bitmap();

    if (apps_loaded[index]) {
        launch_from_reu(index);
    } else {
#if READYOS_LAUNCHER_VARIANT_EASYFLASH
        launcher_set_notice("preload missing for selected app", TUI_COLOR_LIGHTRED);
#else
        launch_from_disk(index);
#endif
    }
}

/*---------------------------------------------------------------------------
 * Drawing
 *---------------------------------------------------------------------------*/

static void draw_header(void) {
    TuiRect box = {0, 0, 40, 3};
    const char *variant = launcher_resolved_variant_title();
    unsigned char len;
    unsigned char x;

    tui_window_title(&box, READYOS_TITLE_TEXT, TUI_COLOR_LIGHTBLUE, TUI_COLOR_YELLOW);
    len = (unsigned char)strlen(variant);
    if (len > 38) {
        len = 38;
    }
    x = (unsigned char)((40 - len) / 2);
    tui_puts_n(x, 1, variant, len, TUI_COLOR_LIGHTGREEN);
}

static void draw_status(void) {
    TuiRect box = {0, STATUS_Y, 40, 3};
    tui_window(&box, TUI_COLOR_LIGHTBLUE);

    tui_puts(2, STATUS_Y + 1, "REU: 16MB", TUI_COLOR_WHITE);
#if !READYOS_LAUNCHER_VARIANT_EASYFLASH && LAUNCHER_DMA_LOAD
    tui_puts(12, STATUS_Y + 1, "DMA:", TUI_COLOR_GRAY3);
    if (launcher_dma_used) {
        tui_puts(16, STATUS_Y + 1, "ON ", TUI_COLOR_LIGHTGREEN);
    } else if (!launcher_dma_checked) {
        tui_puts(16, STATUS_Y + 1, "?? ", TUI_COLOR_GRAY2);
    } else if (launcher_dma_available) {
        tui_puts(16, STATUS_Y + 1, "YES", TUI_COLOR_LIGHTGREEN);
    } else {
        tui_puts(16, STATUS_Y + 1, "NO ", TUI_COLOR_GRAY2);
    }

    /* Legend for REU indicator */
    tui_putc(22, STATUS_Y + 1, REU_INDICATOR, TUI_COLOR_LIGHTGREEN);
    tui_puts(23, STATUS_Y + 1, "=IN REU", TUI_COLOR_GRAY3);
#else
    /* Legend for REU indicator */
    tui_putc(20, STATUS_Y + 1, REU_INDICATOR, TUI_COLOR_LIGHTGREEN);
    tui_puts(21, STATUS_Y + 1, "=IN REU", TUI_COLOR_GRAY3);
#endif
}

static void draw_help(void) {
#if READYOS_LAUNCHER_VARIANT_EASYFLASH
    tui_puts(1, HELP_Y, "RET:LAUNCH            F3:STATUS", TUI_COLOR_GRAY3);
#else
    tui_puts(1, HELP_Y, "F1:ALL F3:LOAD F5:BROWSE F7:UN", TUI_COLOR_GRAY3);
#endif
    tui_puts(1, HELP_Y + 1, "F2:NEXT APP  F4:PREV  STOP:QUIT", TUI_COLOR_GRAY3);
}

static void draw_notice(void) {
    tui_puts_n(1, (unsigned char)(HELP_Y - 1), launcher_notice,
               LAUNCHER_NOTICE_LEN, launcher_notice_color);
}

static void draw_drive_field(unsigned int screen_offset, unsigned char drive) {
    unsigned char tens = 32;
    unsigned char ones;

#if READYOS_LAUNCHER_VARIANT_EASYFLASH
    (void)drive;
    ones = (unsigned char)'C';
#else
    if (drive >= 10) {
        tens = (unsigned char)('0' + (drive / 10));
        ones = (unsigned char)('0' + (drive % 10));
    } else {
        ones = (unsigned char)('0' + drive);
    }
#endif

    TUI_SCREEN[screen_offset] = tens;
    TUI_COLOR_RAM[screen_offset] = TUI_COLOR_GRAY2;
    TUI_SCREEN[screen_offset + 1] = ones;
    TUI_COLOR_RAM[screen_offset + 1] = TUI_COLOR_GRAY2;
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
        tui_puts_n(x, y, catalog_name_for_index(index), name_maxlen, name_color);
        return;
    }

    screen_offset = (unsigned int)y * 40 + x;
    TUI_SCREEN[screen_offset] = 32;
    TUI_COLOR_RAM[screen_offset] = name_color;
    draw_drive_field(screen_offset + 1, app_drives[index]);
    TUI_SCREEN[screen_offset + 3] = 32;
    TUI_COLOR_RAM[screen_offset + 3] = name_color;
    tui_puts_n((unsigned char)(x + 4), y, catalog_name_for_index(index), name_maxlen, name_color);
}

static void clear_menu_span(unsigned int start, unsigned char len, unsigned char color) {
    unsigned char pos;

    for (pos = 0; pos < len; ++pos) {
        TUI_SCREEN[start + pos] = 32;
        TUI_COLOR_RAM[start + pos] = color;
    }
}

static unsigned char launcher_hotkey_slot_for_bank(unsigned char bank) {
    unsigned char slot;

    if (bank == 0) {
        return 0;
    }

    for (slot = 1; slot <= TUI_HOTKEY_SLOT_COUNT; ++slot) {
        if (TUI_HOTKEY_BINDINGS[(unsigned char)(slot - 1)] == bank) {
            return slot;
        }
    }

    return 0;
}

static void draw_binding_tag(unsigned int start, unsigned char slot, unsigned char color) {
    if (slot < 1 || slot > TUI_HOTKEY_SLOT_COUNT) {
        clear_menu_span(start, APP_BIND_LABEL_LEN, color);
        return;
    }

    TUI_SCREEN[start + 0] = tui_ascii_to_screen('(');
    TUI_SCREEN[start + 1] = tui_ascii_to_screen('C');
    TUI_SCREEN[start + 2] = tui_ascii_to_screen('T');
    TUI_SCREEN[start + 3] = tui_ascii_to_screen('R');
    TUI_SCREEN[start + 4] = tui_ascii_to_screen('L');
    TUI_SCREEN[start + 5] = tui_ascii_to_screen('+');
    TUI_SCREEN[start + 6] = tui_ascii_to_screen((unsigned char)('0' + slot));
    TUI_SCREEN[start + 7] = tui_ascii_to_screen(')');
    TUI_COLOR_RAM[start + 0] = color;
    TUI_COLOR_RAM[start + 1] = color;
    TUI_COLOR_RAM[start + 2] = color;
    TUI_COLOR_RAM[start + 3] = color;
    TUI_COLOR_RAM[start + 4] = color;
    TUI_COLOR_RAM[start + 5] = color;
    TUI_COLOR_RAM[start + 6] = color;
    TUI_COLOR_RAM[start + 7] = color;
}

static void draw_menu_item(unsigned char idx) {
    unsigned char row;
    unsigned char y;
    unsigned char color;
    unsigned char prefix;
    unsigned char app_index;
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
        prefix = 0x3E;  /* '>' in screen code */
    } else {
        color = menu.item_color;
        prefix = 32;    /* Space */
    }

    app_index = launcher_menu_to_app_index(idx);
    screen_offset = (unsigned int)y * 40 + menu.x;
    TUI_SCREEN[screen_offset] = prefix;
    TUI_COLOR_RAM[screen_offset] = color;
    TUI_SCREEN[screen_offset + 1] = 32;
    TUI_COLOR_RAM[screen_offset + 1] = color;
    if (app_index != MENU_NO_APP && launcher_is_app_slot(app_index)) {
        draw_drive_field(screen_offset + 2, app_drives[app_index]);
    } else {
        TUI_SCREEN[screen_offset + 2] = 32;
        TUI_COLOR_RAM[screen_offset + 2] = color;
        TUI_SCREEN[screen_offset + 3] = 32;
        TUI_COLOR_RAM[screen_offset + 3] = color;
    }
    TUI_SCREEN[screen_offset + 4] = 32;
    TUI_COLOR_RAM[screen_offset + 4] = color;

    str = catalog_menu_name(idx);
    name_len = APP_NAME_WIDTH;
    text_offset = screen_offset + 5;
    for (pos = 0; str[pos] != 0 && pos < name_len; ++pos) {
        TUI_SCREEN[text_offset + pos] = tui_ascii_to_screen(str[pos]);
        TUI_COLOR_RAM[text_offset + pos] = color;
    }
    for (; pos < name_len; ++pos) {
        TUI_SCREEN[text_offset + pos] = 32;
        TUI_COLOR_RAM[text_offset + pos] = color;
    }

    reu_offset = screen_offset + menu.w - 1;
    binding_offset = reu_offset - (APP_BIND_LABEL_LEN + 1);
    slot = 0;
    if (app_index != MENU_NO_APP && launcher_is_app_slot(app_index)) {
        slot = launcher_hotkey_slot_for_bank(app_banks[app_index]);
    }
    draw_binding_tag(binding_offset, slot, color);
    TUI_SCREEN[reu_offset - 1] = 32;
    TUI_COLOR_RAM[reu_offset - 1] = color;
    if (app_index != MENU_NO_APP &&
        launcher_is_app_slot(app_index) &&
        apps_loaded[app_index]) {
        TUI_SCREEN[reu_offset] = REU_INDICATOR;
        TUI_COLOR_RAM[reu_offset] = TUI_COLOR_LIGHTGREEN;
    } else {
        TUI_SCREEN[reu_offset] = 32;
        TUI_COLOR_RAM[reu_offset] = color;
    }
}

static void draw_menu(void) {
    unsigned char row;
    unsigned char item_idx;

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
    unsigned char app_index = launcher_menu_to_app_index(sel);
#if !READYOS_LAUNCHER_VARIANT_EASYFLASH
    static char launch_line[39];
#endif

    /* Overwrite both description lines in-place (no clear needed) */
    if (launcher_menu_is_browse(sel)) {
        tui_puts_n(2, APPS_START_Y + APPS_HEIGHT,
                   "choose app.* seq manifest", 38, TUI_COLOR_GRAY3);
        tui_puts_n(2, APPS_START_Y + APPS_HEIGHT + 1,
                   "F3 CHANGES MANIFEST DRIVE", 38, TUI_COLOR_GRAY3);
    } else if (app_index < app_count) {
        tui_puts_n(2, APPS_START_Y + APPS_HEIGHT, catalog_desc_for_index(app_index), 38, TUI_COLOR_GRAY3);

        /* Show launch source */
        if (apps_loaded[app_index] && app_banks[app_index] != 0) {
            tui_puts_n(2, APPS_START_Y + APPS_HEIGHT + 1,
                       "LAUNCH FROM REU (INSTANT)", 38, TUI_COLOR_LIGHTGREEN);
        } else if (app_banks[app_index] != 0) {
#if READYOS_LAUNCHER_VARIANT_EASYFLASH
            tui_puts_n(2, APPS_START_Y + APPS_HEIGHT + 1,
                       "PRELOAD MISSING", 38, TUI_COLOR_LIGHTRED);
#else
            if (app_drives[app_index] == 8) {
                tui_puts_n(2, APPS_START_Y + APPS_HEIGHT + 1,
                           "LAUNCH FROM DISK", 38, TUI_COLOR_GRAY3);
            } else {
                strcpy(launch_line, "LAUNCH FROM DISK ");
                if (app_drives[app_index] >= 10) {
                    launch_line[17] = (char)('0' + (app_drives[app_index] / 10));
                    launch_line[18] = (char)('0' + (app_drives[app_index] % 10));
                    launch_line[19] = 0;
                } else {
                    launch_line[17] = (char)('0' + app_drives[app_index]);
                    launch_line[18] = 0;
                }
                tui_puts_n(2, APPS_START_Y + APPS_HEIGHT + 1, launch_line, 38, TUI_COLOR_GRAY3);
            }
#endif
        } else {
            tui_puts_n(2, APPS_START_Y + APPS_HEIGHT + 1, "", 38, TUI_COLOR_WHITE);
        }
    } else {
        tui_puts_n(2, APPS_START_Y + APPS_HEIGHT, "", 38, TUI_COLOR_WHITE);
        tui_puts_n(2, APPS_START_Y + APPS_HEIGHT + 1, "", 38, TUI_COLOR_WHITE);
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
    catalog_refresh_name_cache();
}

static void launcher_seed_default_hotkeys(void) {
    unsigned char i;
    unsigned char slot;

    for (i = launcher_first_app_index(); i < app_count; ++i) {
        slot = app_default_slots[i];
        if (slot == 0) {
            continue;
        }
        if (TUI_HOTKEY_BINDINGS[(unsigned char)(slot - 1)] == 0) {
            TUI_HOTKEY_BINDINGS[(unsigned char)(slot - 1)] = app_banks[i];
        }
    }
}

static unsigned char launcher_index_for_bank(unsigned char bank) {
    unsigned char i;

    for (i = launcher_first_app_index(); i < app_count; ++i) {
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

#if !READYOS_LAUNCHER_VARIANT_EASYFLASH
    if (launcher_cfg_load_all_to_reu) {
        if (!load_all_to_reu_internal(0)) {
            launcher_set_notice("auto preload incomplete", TUI_COLOR_LIGHTRED);
            return;
        }

        if (launcher_runappfirst_prg[0] != 0) {
            index = launcher_find_app_by_prg(launcher_runappfirst_prg);
            if (index == 0) {
                launcher_set_notice("runappfirst app not found", TUI_COLOR_LIGHTRED);
                return;
            }
            launcher_runappfirst_prg[0] = 0;
            launch_app(index);
            return;
        }
        return;
    }
#endif

    if (launcher_runappfirst_prg[0] != 0) {
        index = launcher_find_app_by_prg(launcher_runappfirst_prg);
        if (index >= app_count) {
            launcher_set_notice("runappfirst app not found", TUI_COLOR_LIGHTRED);
            return;
        }
        launcher_runappfirst_prg[0] = 0;
        launch_app(index);
    }
}

static void launcher_draw(void) {
    launcher_sync_visible_window();
    tui_clear(TUI_COLOR_BLUE);
    draw_header();
    tui_puts(2, APPS_START_Y - 1, "APPLICATIONS:", TUI_COLOR_WHITE);
    draw_menu();
    draw_app_desc();
    draw_status();
    draw_notice();
    draw_help();
}

/*---------------------------------------------------------------------------
 * Main
 *---------------------------------------------------------------------------*/

static void launcher_init(void) {
    unsigned char i;
    unsigned char saved_selected = 0;
    unsigned char saved_scroll_offset = 0;
    unsigned char saved_suppress_startup_once = 0;
    unsigned char shim_suppress_startup_once;
    unsigned char restored_resume = 0;
    unsigned char used_cached_catalog = 0;
    unsigned char err;
    unsigned char detail_a;
    unsigned char detail_b;
    unsigned char detail_c;

    tui_init();
    reu_phys_apply_to_alloc_table(reu_phys_detect_bank_count());
    shim_suppress_startup_once =
        (unsigned char)((*SHIM_LAUNCHER_FLAGS & SHIM_LAUNCHER_FLAG_SUPPRESS_STARTUP) != 0u);
    *SHIM_LAUNCHER_FLAGS =
        (unsigned char)(*SHIM_LAUNCHER_FLAGS &
                        (unsigned char)~SHIM_LAUNCHER_FLAG_SUPPRESS_STARTUP);
    catalog_init_defaults();
    catalog_rebind_views();
    resume_ready = 0;

    resume_init_for_app(REU_BANK_LAUNCHER, REU_BANK_LAUNCHER,
                        LAUNCHER_RESUME_SCHEMA);
    resume_ready = 1;
    restored_resume = launcher_resume_restore(&saved_selected, &saved_scroll_offset,
                                              &saved_suppress_startup_once);

    if (restored_resume) {
        err = validate_slot_contract(&detail_a, &detail_b, &detail_c);
        if (err == 0) {
            slot_contract_ok = 1;
            used_cached_catalog = 1;
        } else {
            catalog_init_defaults();
            catalog_rebind_views();
            saved_selected = 0;
            saved_scroll_offset = 0;
            saved_suppress_startup_once = 0;
        }
    }

    if (!used_cached_catalog) {
        catalog_clear_all_entries();
        launcher_control_clear_dependency_lines();
        launcher_control_clear_resource_records();
#if READYOS_LAUNCHER_VARIANT_EASYFLASH
        err = load_catalog_from_embedded(&detail_a, &detail_b, &detail_c);
#else
        err = load_catalog_from_disk(&detail_a, &detail_b, &detail_c);
#endif
        if (err != 0) {
            slot_contract_ok = 0;
            show_slot_contract_error(err, detail_a, detail_b, detail_c);
            running = 0;
            return;
        }

        err = validate_slot_contract(&detail_a, &detail_b, &detail_c);
        if (err != 0) {
            slot_contract_ok = 0;
            show_slot_contract_error(err, detail_a, detail_b, detail_c);
            running = 0;
            return;
        }
        slot_contract_ok = 1;
    }
    if (shim_suppress_startup_once) {
        saved_suppress_startup_once = 1;
    }

    /* Initialize menu */
    tui_menu_init(&menu, 2, APPS_START_Y, APP_MENU_WIDTH, APPS_HEIGHT,
                  launcher_menu_dummy, launcher_menu_count());
    menu.item_color = TUI_COLOR_WHITE;
    menu.sel_color = TUI_COLOR_CYAN;

    if (saved_selected < launcher_menu_count()) {
        menu.selected = saved_selected;
    }
    menu.scroll_offset = saved_scroll_offset;
    launcher_sync_visible_window();

    if (restored_resume && saved_suppress_startup_once) {
        /* Clear the one-shot auto-run suppression marker so it only
         * prevents immediate relaunch after an app returns once. */
        launcher_resume_save(menu.selected, menu.scroll_offset, 0);
    }

    /* ALWAYS sync apps_loaded from shim's reu_bitmap - this is the
     * authoritative source for what's actually in REU. Don't rely on
     * stale values from before the REU restore. */
    for (i = 0; i < app_count; ++i) {
        apps_loaded[i] = 0;
        app_sizes[i] = 0;
    }
    set_shim_drive(8);
    sync_from_reu_bitmap();
#if READYOS_LAUNCHER_VARIANT_EASYFLASH
    launcher_mark_embedded_preloads_loaded();
#endif
    launcher_mirror_reu_control();
    launcher_seed_default_hotkeys();

    running = 1;
    launcher_apply_startup_actions(saved_suppress_startup_once);
}

static void launcher_loop(void) {
    unsigned char key;
    unsigned char result;
    unsigned char bank;
    unsigned char old_selected;
    unsigned char old_scroll_offset;

    /* apps_loaded[] is already synced from reu_bitmap in launcher_init() */
    if (!running) {
        return;
    }

    launcher_draw();

    while (running) {
        key = tui_getkey();

        if (key != 2 && key != TUI_KEY_NEXT_APP && key != TUI_KEY_PREV_APP) {
            bank = tui_handle_global_hotkey(key, REU_BANK_LAUNCHER, 0);
            if (bank >= 1 && bank < MAX_APPS) {
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
#if !READYOS_LAUNCHER_VARIANT_EASYFLASH
            if (launcher_menu_is_browse(result)) {
                browse_and_load_manifest();
            } else
#endif
            {
                result = launcher_menu_to_app_index(result);
                if (result != MENU_NO_APP) {
                    launch_app(result);
                }
            }
            launcher_draw();
            continue;
        }

        switch (key) {
            case TUI_KEY_F1:
#if !READYOS_LAUNCHER_VARIANT_EASYFLASH
                load_all_to_reu();
                launcher_draw();
#endif
                break;

            case TUI_KEY_F3:
                result = launcher_menu_to_app_index(tui_menu_selected(&menu));
                if (result != MENU_NO_APP) {
                    load_selected_to_reu(result);
                }
                launcher_draw();
                break;

#if !READYOS_LAUNCHER_VARIANT_EASYFLASH
            case TUI_KEY_F5:
                browse_and_load_manifest();
                launcher_draw();
                break;

            case TUI_KEY_F7:
                result = launcher_menu_to_app_index(tui_menu_selected(&menu));
                if (result != MENU_NO_APP) {
                    unload_selected_from_reu(result);
                }
                launcher_draw();
                break;
#endif

            case TUI_KEY_RUNSTOP:
                running = 0;
                break;

            default:
                /* Only update if selection changed */
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

    /* Reset on exit */
    __asm__("jmp $FCE2");
}

int main(void) {
    launcher_init();
    launcher_loop();
    return 0;
}
