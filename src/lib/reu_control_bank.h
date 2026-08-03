/*
 * reu_control_bank.h - ReadyOS bank schema and registry API
 *
 * Physical REU bank Skip is the ReadyOS bank.  It contains the launcher
 * snapshot at $0000-$B5FF and the authoritative ReadyOS metadata at
 * $B600-$FFFF.  There is no C64-RAM allocation/loaded-state mirror.
 */

#ifndef REU_CONTROL_BANK_H
#define REU_CONTROL_BANK_H

#include "reu_mgr.h"

#define REUCB_SCHEMA_VERSION 5

/* Binary ABI bytes, deliberately numeric: cc65 translates character literals
 * through the target character set (uppercase letters become PETSCII $C1+). */
#define REUCB_MAGIC0 0x52u /* ASCII R */
#define REUCB_MAGIC1 0x43u /* ASCII C */
#define REUCB_MAGIC2 0x42u /* ASCII B */
#define REUCB_MAGIC3 0x35u /* ASCII 5 */

#define REUCB_LAUNCHER_SNAPSHOT_OFF  0x0000u
#define REUCB_LAUNCHER_SNAPSHOT_SIZE 0xB600u

#define REUCB_HEADER_OFF        0xB600u
#define REUCB_HEADER_SIZE       0x0040u
#define REUCB_BANK_TYPE_OFF     0xB640u
#define REUCB_BANK_TYPE_SIZE    0x0100u
#define REUCB_SHIM_LOOKUP_OFF   0xB740u
#define REUCB_SHIM_LOOKUP_SIZE  0x0100u
#define REUCB_TOKEN_STATUS_OFF  0xB840u
#define REUCB_TOKEN_STATUS_SIZE 0x0100u
#define REUCB_CLIPBOARD_OFF     0xB940u
#define REUCB_CLIPBOARD_SIZE    0x0090u
#define REUCB_HOTKEY_OFF        0xB9D0u
#define REUCB_HOTKEY_SIZE       9u
#define REUCB_SETTINGS_OFF      0xB9D9u
#define REUCB_SETTINGS_SIZE     39u
#define REUCB_SETTINGS_MAGIC0   0x4Cu /* ASCII L */
#define REUCB_SETTINGS_MAGIC1   0x53u /* ASCII S */
#define REUCB_SETTINGS_VERSION  1u
#define REUCB_SETTINGS_OFF_MAGIC0       0u
#define REUCB_SETTINGS_OFF_MAGIC1       1u
#define REUCB_SETTINGS_OFF_VERSION      2u
#define REUCB_SETTINGS_OFF_FIRST_APP    3u
#define REUCB_SETTINGS_OFF_APP_COUNT    4u
#define REUCB_SETTINGS_OFF_LOAD_ALL     5u
#define REUCB_SETTINGS_OFF_VARIANT      6u
#define REUCB_SETTINGS_VARIANT_SIZE     32u
#define REUCB_APP_REG_OFF       0xBA00u
#define REUCB_APP_REG_SIZE      16u
#define REUCB_APP_REG_COUNT     64u
#define REUCB_TOKEN_APP_OFF     0xBE00u
#define REUCB_TOKEN_APP_SIZE    0x0100u
#define REUCB_APP_META_OFF      0xBF00u
#define REUCB_APP_META_SIZE     13u
#define REUCB_APP_META_COUNT    64u
#define REUCB_RSRC_REC_OFF      0xC240u
#define REUCB_RSRC_REC_SIZE     16u
#define REUCB_RSRC_REC_COUNT    64u
#define REUCB_DEP_LINE_OFF      0xC640u
#define REUCB_DEP_LINE_SIZE     128u
#define REUCB_DEP_LINE_COUNT    64u
#define REUCB_CATALOG_TEXT_OFF  0xE640u
#define REUCB_CATALOG_NAME_OFF  0xE640u
#define REUCB_CATALOG_NAME_SIZE 32u
#define REUCB_CATALOG_DESC_OFF  0xEE40u
#define REUCB_CATALOG_DESC_SIZE 39u
#define REUCB_CATALOG_FILE_OFF  0xF800u
#define REUCB_CATALOG_FILE_SIZE 13u
#define REUCB_AUDIT_OFF         0xFB40u
#define REUCB_AUDIT_SIZE        0x0100u
#define REUCB_RUNTIME_OFF       0xFC40u
#define REUCB_RUNTIME_SIZE      0x0080u
#define REUCB_RESERVED_OFF      0xFCC0u
#define REUCB_RESERVED_SIZE     0x0340u

/* Compatibility names for code which treats the app-record resource fields
 * as dependency slots.  Schema v5 stores those bytes in each app record. */
#define REUCB_DEP_OFF           REUCB_APP_REG_OFF
#define REUCB_DEP_SIZE          REUCB_APP_REG_SIZE
#define REUCB_DEP_COUNT         REUCB_APP_REG_COUNT
#define REUCB_RESOURCE_OFF      REUCB_RSRC_REC_OFF
#define REUCB_RESOURCE_SIZE     REUCB_RSRC_REC_SIZE
#define REUCB_RESOURCE_COUNT    REUCB_RSRC_REC_COUNT

#define REUCB_HEADER_REU_SKIP       8u
#define REUCB_HEADER_CONTROL_BANK   9u
#define REUCB_HEADER_LAUNCHER_BANK  10u
#define REUCB_HEADER_LAUNCHER_OVL   11u
#define REUCB_HEADER_FIRST_DYNAMIC  12u
#define REUCB_HEADER_LOGICAL_BANKS  13u
#define REUCB_HEADER_PHYS_BANKS     44u
#define REUCB_HEADER_FIRST_UNAVAIL  45u
#define REUCB_HEADER_FLAGS          46u
#define REUCB_HEADER_FLAG_PHYS_SIZE 0x01u

#define REUCB_TOKEN_VALID        0x01u
#define REUCB_TOKEN_LOADED       0x02u
#define REUCB_TOKEN_RESUMABLE    0x04u

#define REUCB_APP_REC_TOKEN      0u
#define REUCB_APP_REC_PHYSICAL   1u
#define REUCB_APP_REC_FLAGS      2u
#define REUCB_APP_REC_DRIVE      3u
#define REUCB_APP_REC_HOTKEY     4u
#define REUCB_APP_REC_RSRC_SET   5u
#define REUCB_APP_REC_RSRC_READY 6u
#define REUCB_APP_REC_FIRST_RSRC 7u
#define REUCB_APP_REC_RSRC1      8u
#define REUCB_APP_REC_RSRC2      9u
#define REUCB_APP_REC_RSRC3      10u
#define REUCB_APP_REC_RSRC4      11u
#define REUCB_APP_REC_SIZE_LO    12u
#define REUCB_APP_REC_SIZE_HI    13u

#define REUCB_NULL_DEP          0xFFu
#define REUCB_NULL_REC          0xFFu

#define REUCB_APP_FLAG_LOADED   0x01u
#define REUCB_APP_FLAG_HAS_DEPS 0x02u

#define REUCB_DEP_KIND_RS_CACHE 1u
#define REUCB_DEP_KIND_RB_CORE  2u
#define REUCB_DEP_KIND_RB_CODE  3u
#define REUCB_DEP_KIND_RS_OVL   4u
#define REUCB_DEP_KIND_RS_STATE 5u
#define REUCB_DEP_KIND_APP_ALLOC 6u

#define REUCB_WRITER_LAUNCHER   1u
#define REUCB_WRITER_REUVIEWER  2u

#define REUCB_OWNER_SYSTEM      1u
#define REUCB_OWNER_LAUNCHER    2u
#define REUCB_OWNER_READYSHELL  3u
#define REUCB_OWNER_READYBASIC  4u

#define REUCB_ROLE_GLOBAL       1u
#define REUCB_ROLE_SNAPSHOT     2u
#define REUCB_ROLE_OVERLAY      3u
#define REUCB_ROLE_CACHE        4u
#define REUCB_ROLE_DEBUG        5u
#define REUCB_ROLE_SCRATCH      6u
#define REUCB_ROLE_CORE         7u
#define REUCB_ROLE_CODE         8u

void reu_control_bank_sync_and_mirror(unsigned char writer_id);
void reu_control_bank_prepare(unsigned char physical_banks);
unsigned char reu_control_bank_is_valid(void);

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
    const unsigned char *apps_loaded,
    const unsigned int *app_sizes);

#endif /* REU_CONTROL_BANK_H */
