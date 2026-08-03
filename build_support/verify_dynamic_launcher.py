#!/usr/bin/env python3
"""Static and host-side checks for dynamic launcher REU allocation."""

from __future__ import annotations

import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def fail(message: str) -> None:
    raise SystemExit(f"dynamic launcher check failed: {message}")


def check(name: str, condition: bool) -> None:
    if not condition:
        fail(name)
    print(f"OK: {name}")


def main() -> int:
    launcher = (ROOT / "src/apps/launcher/launcher.c").read_text(
        encoding="utf-8", errors="replace"
    )
    hotkeys = (ROOT / "src/lib/tui_hotkeys.c").read_text(
        encoding="utf-8", errors="replace"
    )
    tui_header = (ROOT / "src/lib/tui.h").read_text(
        encoding="utf-8", errors="replace"
    )
    catalog = (ROOT / "build_support/build_apps_catalog_petscii.py").read_text(
        encoding="utf-8", errors="replace"
    )
    rs_overlay = (ROOT / "src/apps/readyshell/platform/c64/rs_overlay_c64.c").read_text(
        encoding="utf-8", errors="replace"
    )
    rs_state = (ROOT / "src/apps/readyshell/core/rs_ui_state.h").read_text(
        encoding="utf-8", errors="replace"
    )
    makefile = (ROOT / "Makefile").read_text(encoding="utf-8", errors="replace")
    tui_alias = (ROOT / "src/lib/tui_readyos_alias.s").read_text(
        encoding="utf-8", errors="replace"
    )

    check("launcher exposes 64 app capacity",
          "#define APP_SLOT_CAPACITY 64" in launcher)
    check("catalog entries start without preallocated bank",
          "app_banks[idx] = 0u;" in launcher)
    check("launcher has lazy snapshot allocator",
          "launcher_alloc_snapshot_bank" in launcher and
          "launcher_bank_type(physical) == REU_FREE" in launcher and
          "launcher_bind_snapshot_token" in launcher)
    check("C64 allocation mirror is absent",
          "REU_ALLOC_TABLE" not in launcher)
    reu_mgr = (ROOT / "src/lib/reu_mgr.h").read_text(
        encoding="utf-8", errors="replace"
    )
    reu_control = (ROOT / "src/lib/reu_control_bank.c").read_text(
        encoding="utf-8", errors="replace"
    )
    shim = (ROOT / "src/boot/readyos_shim.inc").read_text(
        encoding="utf-8", errors="replace"
    )
    check("physical Skip is the ReadyOS bank and Skip+1 is first dynamic",
          "#define REU_FIRST_DYNAMIC 1" in reu_mgr and
          "*SHIM_READYOS_BANK + 1u" in reu_mgr)
    check("ReadyOS bank owns authoritative mapping and status pages",
          "REUCB_SHIM_LOOKUP_OFF" in
          (ROOT / "src/lib/reu_control_bank.h").read_text() and
          "REUCB_TOKEN_STATUS_OFF" in
          (ROOT / "src/lib/reu_control_bank.h").read_text() and
          "REU_ALLOC_TABLE" not in reu_control)
    check("shim resolves app tokens through ReadyOS-bank lookup",
          "lookup_app_bank" in shim and
          "LDA #>$B740" in shim and
          "LDA $C83D (resolved physical bank)" in shim)
    check("shim commits token loaded/resumable state",
          "mark_loaded" in shim and "LDA #VALID|LOADED|RESUMABLE" in shim)
    check("launcher validates ReadyOS token loaded state",
          "REUCB_TOKEN_LOADED" in launcher and "required_slots_loaded" in launcher)
    check("load-all progress rows wrap for 64-app catalogs",
          "#define LOAD_ALL_LIST_ROWS 19" in launcher and
          "loaded_count % LOAD_ALL_LIST_ROWS" in launcher)
    check("cartridge preloads publish explicit token mappings",
          "launcher_mark_embedded_preloads_loaded" in launcher and
          "readyos_easyflash_app_physical_banks" in launcher and
          "app_sizes[i] = APP_SAVE_SIZE;" in launcher)
    check("launcher has unload command",
          "unload_selected_from_reu" in launcher and "case TUI_KEY_F7" in launcher)
    check("launcher invalidates stale last-saved token on unload",
          "*SHIM_LAST_SAVED == bank" in launcher and
          "*SHIM_LAST_SAVED = 0xFFu;" in launcher and
          "launcher_set_snapshot_loaded(bank, 0u)" in launcher)
    check("launcher unload frees owner-recorded app allocations",
          "launcher_free_app_owned_alloc_records" in launcher and
          "REUCB_DEP_KIND_APP_ALLOC" in launcher and
          "launcher_bank_type(bank) == REU_APP_ALLOC" in launcher)
    check("launcher owns ReadyShell overlay resource banks",
          "APP_RESOURCE_READYSHELL_OVL" in launcher and
          "launcher_load_readyshell_resources" in launcher and
          "launcher_stream_prg_to_reu" in launcher)
    check("ReadyShell resource config token is validated",
          "RESOURCE_READYSHELL_OVL" in catalog and "rsovl" in catalog)
    check("launcher owns ReadyBASIC resource banks",
          "APP_RESOURCE_READYBASIC_CORE" in launcher and
          "launcher_load_readybasic_resources" in launcher and
          "REU_RB_CORE" in launcher and "REU_RB_CODE" in launcher)
    check("ReadyBASIC resource config token is validated",
          "RESOURCE_READYBASIC_CORE" in catalog and "rbcore" in catalog)
    check("dependency-list marker is validated in disk and manifest parser",
          "pending_dep_line_required" in launcher and
          "parse_dependency_list_line" in launcher and
          "RESOURCE_DEP_SUFFIX" in catalog)
    check("dependency-line writer is safe when source is launcher buffer",
          'line != (const char *)launcher_dep_line_buf' in launcher and
          "for (; i < REUCB_DEP_LINE_SIZE; ++i)" in launcher)
    check("ReadyShell dependency placement is config-driven in disk launcher",
          "launcher_parse_rs_dep_entry" in launcher and
          "readyshell_overlay_names" not in launcher and
          "readyshell_overlay_offsets" not in launcher)
    check("ReadyShell runtime reads overlay bank/offset metadata",
          "RS_REU_OVL_CACHE_META_VERSION  4u" in rs_state and
          "g_overlay_cache_offsets" in rs_overlay and
          "rs_cmd_registry_apply_overlay_cache" in rs_overlay)
    check("ReadyShell diagnostics have no C64-RAM mirror",
          "RS_RAM_DBG" not in rs_overlay and
          "RS_REU_DBG_DATA_OFF" in rs_overlay and
          "RS_REU_DBG_HEAD_OFF" in rs_overlay)
    check("ReadyShell overlay metadata does not overlap UI flags or value arena",
          "RS_REU_UI_FLAGS_REL 0x8114u" in rs_state and
          "RS_REU_UI_FLAGS_OFF rs_reu_state_abs(RS_REU_UI_FLAGS_REL)" in rs_state and
          "RS_REU_HEAP_ARENA_REL  0x8120u" in
          (ROOT / "src/apps/readyshell/core/rs_value.c").read_text(
              encoding="utf-8", errors="replace"
          ) and
          "RS_CMD_REU_HEAP_ARENA_REL  0x8120u" in
          (ROOT / "src/apps/readyshell/core/rs_cmd_ldv_local.h").read_text(
              encoding="utf-8", errors="replace"
          ))
    check("host dependency parser validates placement syntax",
          "dependency resource bank must be 0..2" in catalog and
          "dependency offset invalid for overlay slot" in catalog)
    check("host apps.cfg generator allows 64 apps",
          "len(apps) > 64" in catalog)
    check("global hotkeys allow dynamic logical banks",
          "#define APP_BANK_MAX TUI_APP_BANK_MAX" in hotkeys and
          "#define TUI_APP_BANK_MAX      64" in tui_header and
          "REUCB_TOKEN_STATUS_OFF" in hotkeys)
    check("TUI ReadyOS access avoids duplicate DMA micromodules",
          "TUI_READYOS_SRC = $(LIB_DIR)/tui_readyos_alias.s" in makefile and
          "TUI_READYOS_LITE_SRC = $(LIB_DIR)/tui_readyos.c" in makefile and
          "LIB_SYSINFO = $(TUI_BASE_NAV_MISC) $(TUI_HOTKEY_LITE_SRC)" in makefile and
          "LIB_UCITEST = $(TUI_UCITEST) $(TUI_HOTKEY_LITE_SRC)" in makefile and
          "jmp _readyos_bank_read_byte" in tui_alias and
          "jmp _readyos_bank_write_byte" in tui_alias and
          not (ROOT / "src/lib/tui_readyos_alias.c").exists())
    check("verify target includes dynamic launcher check",
          "verify_dynamic_launcher.py" in makefile)

    with tempfile.TemporaryDirectory() as td:
        src = Path(td) / "apps64.ini"
        out = Path(td) / "apps.cfg.seq"
        lines = [
            "[system]",
            "variant_name=test",
            "variant_boot_name=test",
            "reu_bank_skip=32",
            "[launcher]",
            "load_all_to_reu=0",
            "runappfirst=",
            "[apps]",
        ]
        for i in range(64):
            name = f"a{i:02d}"
            lines.append(f"8:{name}:{name}")
            lines.append("test app")
        src.write_text("\n".join(lines) + "\n", encoding="utf-8")
        subprocess.run(
            [
                "python3",
                str(ROOT / "build_support/build_apps_catalog_petscii.py"),
                "--input",
                str(src),
                "--output",
                str(out),
            ],
            check=True,
            cwd=ROOT,
            stdout=subprocess.DEVNULL,
        )
        check("host apps.cfg generator accepts 64 apps", out.exists())

    print("dynamic launcher checks passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
