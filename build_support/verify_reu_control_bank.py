#!/usr/bin/env python3
"""Static checks for the authoritative schema-v5 ReadyOS bank."""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text()


def require(condition: bool, message: str) -> None:
    if not condition:
        print(f"FAIL: {message}")
        raise SystemExit(1)
    print(f"OK: {message}")


def define_int(text: str, name: str) -> int:
    pattern = rf"(?m)^#define\s+{re.escape(name)}\s+(.+?)\s*$"
    match = re.search(pattern, text)
    if not match:
        raise ValueError(f"missing define {name}")
    raw = match.group(1).strip()
    raw = raw.rstrip("uUlL")
    if raw.startswith("'") and raw.endswith("'") and len(raw) == 3:
        return ord(raw[1])
    return int(raw, 0)


def make_var(text: str, name: str) -> str:
    match = re.search(rf"(?m)^{re.escape(name)}\s*=\s*(.+)$", text)
    if not match:
        raise ValueError(f"missing Makefile variable {name}")
    return match.group(1)


def main() -> int:
    hdr = read("src/lib/reu_control_bank.h")
    src = read("src/lib/reu_control_bank.c")
    registry = read("src/lib/reu_control_registry.c")
    launcher = read("src/apps/launcher/launcher.c")
    reuviewer = read("src/apps/reuviewer/reuviewer.c")
    makefile = read("Makefile")

    require(define_int(hdr, "REUCB_SCHEMA_VERSION") == 5, "schema version is 5")
    require(define_int(hdr, "REUCB_LAUNCHER_SNAPSHOT_SIZE") == 0xB600,
            "ReadyOS bank begins with the $B600 launcher snapshot")
    require(define_int(hdr, "REUCB_HEADER_OFF") == 0xB600, "header starts at $B600")
    require(define_int(hdr, "REUCB_HEADER_SIZE") == 0x0040, "header is 64 bytes")
    require(define_int(hdr, "REUCB_BANK_TYPE_OFF") == 0xB640, "bank types start at $B640")
    require(define_int(hdr, "REUCB_SHIM_LOOKUP_OFF") == 0xB740,
            "token-to-physical map starts at $B740")
    require(define_int(hdr, "REUCB_TOKEN_STATUS_OFF") == 0xB840,
            "token status starts at $B840")
    require(define_int(hdr, "REUCB_CLIPBOARD_OFF") == 0xB940,
            "clipboard metadata starts at $B940")
    require(define_int(hdr, "REUCB_HOTKEY_OFF") == 0xB9D0,
            "hotkeys start at $B9D0")
    require(define_int(hdr, "REUCB_SETTINGS_OFF") == 0xB9D9,
            "launcher settings start at $B9D9")
    require(define_int(hdr, "REUCB_SETTINGS_SIZE") == 39,
            "launcher settings fill the pre-registry gap")
    require(define_int(hdr, "REUCB_RUNTIME_OFF") == 0xFC40,
            "launcher UI resume state starts at $FC40")
    require(define_int(hdr, "REUCB_RUNTIME_SIZE") == 0x0080,
            "launcher UI resume state has a 128-byte envelope")
    require(define_int(hdr, "REUCB_DMA_OFF") ==
            define_int(hdr, "REUCB_RUNTIME_OFF") + define_int(hdr, "REUCB_RUNTIME_SIZE"),
            "DMA service record follows UI state without overlap")
    require(define_int(hdr, "REUCB_DMA_SIZE") == 128 and
            define_int(hdr, "REUCB_DMA_OFF_PATH") + define_int(hdr, "REUCB_DMA_PATH_SIZE") <= 128,
            "DMA record contains its bounded host path")
    require(define_int(hdr, "REUCB_RESERVED_OFF") ==
            define_int(hdr, "REUCB_DMA_OFF") + define_int(hdr, "REUCB_DMA_SIZE") and
            define_int(hdr, "REUCB_RESERVED_OFF") + define_int(hdr, "REUCB_RESERVED_SIZE") == 65536,
            "DMA record and remaining reservation exactly fill the bank tail")
    require(all(define_int(hdr, name) < define_int(hdr, "REUCB_HEADER_SIZE") for name in (
        "REUCB_HEADER_DMA_OFF_LO", "REUCB_HEADER_DMA_OFF_HI",
        "REUCB_HEADER_DMA_SIZE", "REUCB_HEADER_DMA_VERSION")),
        "DMA descriptor fits in the ReadyOS header")
    require(define_int(hdr, "REUCB_APP_REG_OFF") == 0xBA00, "64-app registry starts at $BA00")
    require(define_int(hdr, "REUCB_APP_REG_SIZE") == 16, "app registry records are 16 bytes")
    require(define_int(hdr, "REUCB_APP_REG_COUNT") == 64, "app registry has 64 entries")
    require(define_int(hdr, "REUCB_TOKEN_APP_OFF") == 0xBE00,
            "token-to-app index starts at $BE00")
    require(define_int(hdr, "REUCB_APP_META_OFF") == 0xBF00, "app metadata starts at $BF00")
    require(define_int(hdr, "REUCB_APP_META_SIZE") == 13, "app filename records are 13 bytes")
    require(define_int(hdr, "REUCB_RSRC_REC_OFF") == 0xC240, "rich resource records start at $C240")
    require(define_int(hdr, "REUCB_RSRC_REC_SIZE") == 16, "rich resource records are 16 bytes")
    require(define_int(hdr, "REUCB_RSRC_REC_COUNT") == 64, "rich resource record capacity is 64")
    require(define_int(hdr, "REUCB_DEP_LINE_OFF") == 0xC640, "dependency lines start at $C640")
    require(define_int(hdr, "REUCB_DEP_LINE_SIZE") == 128, "dependency line records are 128 bytes")
    require(define_int(hdr, "REUCB_CATALOG_NAME_OFF") == 0xE640, "catalog names start at $E640")
    require(define_int(hdr, "REUCB_CATALOG_NAME_SIZE") == 32, "catalog name records are 32 bytes")
    require(define_int(hdr, "REUCB_CATALOG_DESC_OFF") == 0xEE40, "catalog descriptions start at $EE40")
    require(define_int(hdr, "REUCB_CATALOG_DESC_SIZE") == 39, "catalog description records are 39 bytes")
    require(define_int(hdr, "REUCB_CATALOG_FILE_OFF") == 0xF800, "catalog file tokens start at $F800")
    require(define_int(hdr, "REUCB_CATALOG_FILE_SIZE") == 13, "catalog file token records are 13 bytes")
    require(define_int(hdr, "REUCB_HEADER_PHYS_BANKS") == 44, "header records physical bank count")
    require(define_int(hdr, "REUCB_HEADER_FIRST_UNAVAIL") == 45, "header records first unavailable bank")

    require("REU_READYOS_GLOBAL_PHYSICAL()" in src, "schema records direct ReadyOS bank")
    require("readyos_bank_write_byte" in src, "schema writes bank types directly in REU")
    require("REU_ALLOC_TABLE" not in src, "schema has no C64-RAM allocation mirror")
    require("REU_BANK_RS_CACHE" not in src, "control mirror no longer records fixed ReadyShell cache banks")
    require("REU_BANK_RS_SCRATCH" not in src, "control mirror no longer records fixed ReadyShell scratch bank")
    require("REU_BANK_RS_DEBUG" not in src, "control mirror no longer records fixed ReadyShell debug bank")
    require("REU_BANK_RB_CORE" not in src and "REU_BANK_RB_CODE" not in src,
            "control mirror must not record fixed ReadyBASIC core/code banks")
    require("REUCB_HEADER_PHYS_BANKS" in src, "schema publishes physical REU size")
    require("reu_control_bank_write_launcher_registry" in registry,
            "control mirror writes launcher 64-app registry")
    require("REUCB_TOKEN_STATUS_OFF" in registry and "REUCB_TOKEN_APP_OFF" in registry,
            "registry publishes authoritative token status and token-to-app index")
    require("REUCB_APP_REC_SIZE_LO" in registry and "app_sizes" in registry,
            "registry publishes app snapshot sizes")

    require("REU_CONTROL_BANK_SRC = $(LIB_DIR)/reu_control_bank.c" in makefile,
            "Makefile defines REU_CONTROL_BANK_SRC")
    require("REU_CONTROL_REGISTRY_SRC = $(LIB_DIR)/reu_control_registry.c" in makefile,
            "Makefile defines launcher-only control registry source")
    require("REU_PHYS_SRC = $(LIB_DIR)/reu_phys.c" in makefile and
            "REU_PHYS_PROBE_SRC = $(LIB_DIR)/reu_phys_probe.c" in makefile,
            "Makefile defines physical REU modules")
    require("$(REU_CONTROL_BANK_SRC)" in make_var(makefile, "LIB_LAUNCHER"),
            "launcher links control bank module")
    require("$(REU_PHYS_SRC)" in make_var(makefile, "LIB_LAUNCHER") and
            "$(REU_PHYS_PROBE_SRC)" in make_var(makefile, "LIB_LAUNCHER"),
            "launcher links physical REU probe and table module")
    require("$(REU_CONTROL_REGISTRY_SRC)" in make_var(makefile, "LIB_LAUNCHER"),
            "launcher links control registry module")
    require("$(REU_CONTROL_BANK_SRC)" in make_var(makefile, "LIB_REUVIEWER"),
            "reuviewer links control bank module")
    require("$(REU_PHYS_SRC)" in make_var(makefile, "LIB_REUVIEWER") and
            "$(REU_PHYS_PROBE_SRC)" not in make_var(makefile, "LIB_REUVIEWER"),
            "reuviewer links physical table helpers without probe")
    require("$(REU_CONTROL_REGISTRY_SRC)" not in make_var(makefile, "LIB_REUVIEWER"),
            "reuviewer does not link launcher registry writer")
    require("$(REU_CONTROL_BANK_SRC)" not in make_var(makefile, "LIB_REU_DMA"),
            "normal REU DMA library does not link control bank module")
    require("$(REU_CONTROL_BANK_SRC)" not in make_var(makefile, "LIB_REU_DMA_STATS"),
            "normal REU DMA/stats library does not link control bank module")

    require("launcher_mirror_reu_control();" in launcher,
            "launcher refreshes ReadyOS-bank registry")
    require("launcher_publish_settings" in launcher and
            "REUCB_SETTINGS_MAGIC0" in launcher and
            "launcher_restore_registry_from_readyos" in launcher,
            "launcher publishes and restores its settings and registry from the ReadyOS bank")
    require("reu_phys_apply_to_alloc_table(reu_phys_detect_bank_count())" in launcher,
            "launcher probes physical REU size before allocation")
    require("reu_control_bank_write_launcher_registry(" in launcher,
            "launcher publishes app registry into ReadyOS bank")
    require("catalog_store_entry" in launcher and
            "REUCB_CATALOG_NAME_OFF" in launcher and
            "REUCB_CATALOG_DESC_OFF" in launcher and
            "REUCB_CATALOG_FILE_OFF" in launcher,
            "launcher stores cold catalog strings in the ReadyOS bank")
    require("catalog_name_cache[APPS_HEIGHT]" in launcher and
            "catalog_text_buf[MAX_DESC_LEN + 1]" in launcher,
            "launcher keeps only visible catalog-name cache and one text scratch buffer")
    require("app_name_buf[" not in launcher and
            "app_desc_buf[" not in launcher and
            "app_file_buf[" not in launcher and
            "launcher_menu_items[" not in launcher,
            "launcher no longer keeps full catalog text arrays in C64 RAM")
    require("tui_handle_global_hotkey" in launcher and
            "$(TUI_HOTKEY_SRC)" in make_var(makefile, "LIB_LAUNCHER"),
            "launcher still uses the shared TUI hotkey path")
    require("launcher_control_write_resource_record" in launcher and
            "launcher_control_write_dep_line" in launcher,
            "launcher writes rich resource/dependency metadata")
    require("launcher_free_app_owned_alloc_records" in launcher and
            "REUCB_DEP_KIND_APP_ALLOC" in launcher and
            "launcher_bank_type(bank) == REU_APP_ALLOC" in launcher,
            "launcher unload frees owner-recorded app allocation banks")
    require("readyshell_overlay_names" not in launcher and
            "readyshell_overlay_offsets" not in launcher,
            "disk launcher does not carry hard-coded ReadyShell overlay placement tables")
    require("launcher_resolve_snapshot_bank" in launcher,
            "launcher has snapshot-bank resolver")
    require("bank = launcher_resolve_snapshot_bank(index);" in launcher,
            "launcher preload/REU launch paths use resolver")
    load_app = re.search(
        r"static unsigned int load_app_to_reu\(unsigned char index\) \{(.*?)\n\}",
        launcher,
        re.DOTALL,
    )
    require(load_app is not None, "launcher preload implementation is available for ordering checks")
    if load_app is not None:
        body = load_app.group(1)
        require(body.find("bank = launcher_resolve_snapshot_bank(index);") <
                body.find("filename = catalog_file_for_index(index);"),
                "snapshot allocation publishes metadata before taking a catalog scratch pointer")
        require("filename = catalog_file_for_index(index);\n        set_shim_name(filename);" in body,
                "DMA fallback reacquires its catalog filename after metadata publication")
    require("*SHIM_CURRENT_BANK = bank;" in launcher,
            "launcher disk path writes resolved bank to shim current bank")
    require("reu_control_bank_sync_and_mirror(REUCB_WRITER_REUVIEWER)" in reuviewer,
            "reuviewer refreshes ReadyOS-bank header")
    require("reuviewer_read_control_bank_header" in reuviewer and '"CB:"' in reuviewer and
            "control_bank_generation" in reuviewer,
            "reuviewer displays compact control-bank header status")
    require("REUCB_HEADER_PHYS_BANKS" in reuviewer and
            "reu_phys_display_count" in reuviewer and
            "reu_phys_detect_bank_count" not in reuviewer,
            "reuviewer displays launcher-published physical REU size")
    require("reuviewer_find_resource_for_bank" in reuviewer and "OWNER:" in reuviewer,
            "reuviewer decodes rich app/resource ownership records")
    require("REUCB_DEP_KIND_APP_ALLOC" in reuviewer and '"TAG"' in reuviewer,
            "reuviewer displays app-owned allocation record tags")

    print("ReadyOS bank schema-v5 static checks passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
