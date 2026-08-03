#include "../rs_overlay.h"
#include "../rs_edit.h"
#include "../rs_platform.h"
#include "../rs_memcfg.h"
#include "../../core/rs_cmd_overlay.h"
#include "../../core/rs_cmd_registry.h"
#include "../../core/rs_ui_state.h"

#ifndef RS_C64_OVERLAY_RUNTIME
#define RS_C64_OVERLAY_RUNTIME 0
#endif

#ifndef RS_C64_OVERLAY_PRELOADED
#define RS_C64_OVERLAY_PRELOADED 0
#endif

#if RS_C64_OVERLAY_RUNTIME

#include <cbm.h>
#include <string.h>

#define RS_OVERLAY_COUNT 9u
#define RS_OVL_RC_NOT_BOOTED 0xE3u
#define RS_OVL_RC_REU_PARSE  0xE4u
#define RS_OVL_RC_REU_EXEC   0xE5u
#define RS_OVL_RC_REU_REQUIRED 0xE9u
#define RS_OVL_RC_REU_CACHE    0xEAu
#define RS_OVL_RC_REU_CMD      0xEBu
#define RS_OVL_RC_REU_REG      0xECu
#define RS_OVL_RC_REU_META_IO  0xEDu
#define RS_OVL_RC_REU_META_HDR 0xEEu
#define RS_OVL_RC_REU_META_MASK 0xEFu
#define RS_OVL_RC_REU_META_LEN 0xF0u
#define RS_OVL_RC_REU_META_BANK 0xF1u

static int g_overlay_loaded = 0;
static int g_overlay_cached_reu = 0;
static unsigned char g_overlay_last_rc = 0u;
static unsigned char g_overlay_active_phase = RS_OVERLAY_PHASE_NONE;
static unsigned short g_dbg_pos = 0u;
static unsigned char g_overlay_meta_buf[RS_REU_OVL_CACHE_META_LEN];
static unsigned char g_overlay_cache_banks[RS_OVERLAY_COUNT];
static unsigned short g_overlay_cache_offsets[RS_OVERLAY_COUNT];
/* 0 = unknown, 1 = disabled (no REU), 2 = enabled */
static unsigned char g_dbg_state = 0u;

static void rs_overlay_progress_tick(RSOverlayProgressFn progress,
                                     void* user,
                                     unsigned char stage) {
  if (progress) {
    progress(stage, user);
  }
}

static void rs_overlay_clear_phase(void) {
  g_overlay_active_phase = RS_OVERLAY_PHASE_NONE;
}

static void rs_overlay_set_phase(unsigned char phase) {
  g_overlay_active_phase = phase;
  g_overlay_last_rc = 0u;
}

static unsigned long rs_overlay_abs(unsigned char bank, unsigned short rel) {
  return ((unsigned long)bank << 16u) + (unsigned long)rel;
}

static unsigned long rs_overlay_parse_off(void) {
  return rs_overlay_abs(g_overlay_cache_banks[0], g_overlay_cache_offsets[0]);
}

static unsigned long rs_overlay_exec_off(void) {
  return rs_overlay_abs(g_overlay_cache_banks[1], g_overlay_cache_offsets[1]);
}

static unsigned long rs_overlay_edit_off(void) {
  return rs_overlay_abs(g_overlay_cache_banks[8], g_overlay_cache_offsets[8]);
}

static void rs_overlay_dbg_reset(void) {
  unsigned char head[2];
  g_dbg_pos = 0u;
  head[0] = 0u;
  head[1] = 0u;
  (void)rs_reu_write(RS_REU_DBG_HEAD_OFF, head, 2u);
}

static void rs_overlay_dbg_put(unsigned char code) {
  unsigned char b;
  unsigned char head[2];

  if (g_dbg_state == 0u) {
    g_dbg_state = rs_reu_available() ? 2u : 1u;
    if (g_dbg_state == 2u) {
      rs_overlay_dbg_reset();
    }
  }
  if (g_dbg_state != 2u) {
    return;
  }
  b = code;
  (void)rs_reu_write((unsigned long)(RS_REU_DBG_DATA_OFF + (unsigned long)g_dbg_pos), &b, 1u);
  ++g_dbg_pos;
  if (g_dbg_pos >= RS_REU_DBG_DATA_LEN) {
    g_dbg_pos = 0u;
  }
  head[0] = (unsigned char)(g_dbg_pos & 0xFFu);
  head[1] = (unsigned char)((g_dbg_pos >> 8u) & 0xFFu);
  (void)rs_reu_write(RS_REU_DBG_HEAD_OFF, head, 2u);
}

void rs_overlay_debug_mark(unsigned char code) {
  rs_overlay_dbg_put(code);
}

/* Overlay payload lives under BASIC ROM window ($A000-$BFFF): expose RAM while touching it. */
static void rs_overlay_window_enter(void) {
  rs_memcfg_push_ram_under_basic();
  rs_overlay_dbg_put('<');
}

static void rs_overlay_window_leave(void) {
  rs_overlay_dbg_put('>');
  rs_memcfg_pop();
}

extern unsigned char _OVERLAY1_LOAD__[];
#define RS_OVERLAY_LOAD_RAM _OVERLAY1_LOAD__

extern int rs_vmovl_overlay3(unsigned char handler, RSCommandFrame* frame);
extern int rs_vmovl_overlay4(unsigned char handler, RSCommandFrame* frame);
extern int rs_vmovl_overlay5(unsigned char handler, RSCommandFrame* frame);
extern int rs_vmovl_overlay6(unsigned char handler, RSCommandFrame* frame);
extern int rs_vmovl_overlay7(unsigned char handler, RSCommandFrame* frame);
extern int rs_vmovl_overlay8(unsigned char handler, RSCommandFrame* frame);

static int rs_overlay_read_from_reu(unsigned long off, void* dst, unsigned short size) {
  int rc;
  if (!dst || size == 0u) {
    return -1;
  }
  rs_overlay_window_enter();
  rc = rs_reu_read(off, dst, size);
  rs_overlay_window_leave();
  return rc == 0 ? 0 : -1;
}

static unsigned short rs_overlay_valid_bit(unsigned char overlay_num);

static int rs_overlay_meta_read(unsigned short needed_mask) {
  unsigned short slot_len;
  unsigned short valid_mask;
  unsigned char i;
  unsigned char rec_off;

  memset(g_overlay_meta_buf, 0, sizeof(g_overlay_meta_buf));
  if (rs_reu_read(RS_REU_OVL_CACHE_META_OFF, g_overlay_meta_buf, sizeof(g_overlay_meta_buf)) != 0) {
    g_overlay_last_rc = RS_OVL_RC_REU_META_IO;
    return -1;
  }
  if (g_overlay_meta_buf[0] != 'O' ||
      g_overlay_meta_buf[1] != 'V' ||
      g_overlay_meta_buf[2] != RS_REU_OVL_CACHE_META_VERSION) {
    g_overlay_last_rc = RS_OVL_RC_REU_META_HDR;
    return -1;
  }
  valid_mask = (unsigned short)g_overlay_meta_buf[3] |
               ((unsigned short)g_overlay_meta_buf[6] << 8u);
  if ((valid_mask & needed_mask) != needed_mask) {
    g_overlay_last_rc = RS_OVL_RC_REU_META_MASK;
    return -1;
  }

  slot_len = (unsigned short)g_overlay_meta_buf[4] |
             ((unsigned short)g_overlay_meta_buf[5] << 8u);
  if (slot_len != RS_REU_OVL_CACHE_SLOT_LEN) {
    g_overlay_last_rc = RS_OVL_RC_REU_META_LEN;
    return -1;
  }
  for (i = 0u; i < RS_OVERLAY_COUNT; ++i) {
    rec_off = (unsigned char)(RS_REU_OVL_CACHE_META_REC_OFF +
                              (i * RS_REU_OVL_CACHE_META_REC_LEN));
    g_overlay_cache_banks[i] = g_overlay_meta_buf[rec_off];
    g_overlay_cache_offsets[i] =
        (unsigned short)g_overlay_meta_buf[(unsigned char)(rec_off + 1u)] |
        ((unsigned short)g_overlay_meta_buf[(unsigned char)(rec_off + 2u)] << 8u);
    if ((valid_mask & rs_overlay_valid_bit((unsigned char)(i + 1u))) != 0u &&
        g_overlay_cache_banks[i] == 0u) {
      g_overlay_last_rc = RS_OVL_RC_REU_META_BANK;
      return -1;
    }
  }
  if (rs_cmd_registry_apply_overlay_cache(g_overlay_cache_banks,
                                          g_overlay_cache_offsets) != 0) {
    g_overlay_last_rc = RS_OVL_RC_REU_REG;
    return -1;
  }
  return 0;
}

static unsigned short rs_overlay_valid_bit(unsigned char overlay_num) {
  if (overlay_num == 0u || overlay_num > RS_OVERLAY_COUNT) {
    return 0u;
  }
  return (unsigned short)(1u << (overlay_num - 1u));
}

static int rs_overlay_fetch_slot(unsigned char overlay_num,
                                 unsigned char phase,
                                 unsigned long abs_off) {
  if (overlay_num == 0u || overlay_num > RS_OVERLAY_COUNT || abs_off == 0ul) {
    return -1;
  }
  if (rs_overlay_read_from_reu(abs_off, RS_OVERLAY_LOAD_RAM, RS_REU_OVL_CACHE_SLOT_LEN) == 0) {
    rs_overlay_set_phase(phase);
    return 0;
  }
  return -1;
}

static unsigned char rs_overlay_num_from_state_index(unsigned char index) {
  return (unsigned char)(index + 3u);
}

static int rs_overlay_sync_preloaded_registry(void) {
  unsigned char overlay_index;

  for (overlay_index = 0u; overlay_index < 6u; ++overlay_index) {
    if (rs_cmd_registry_update_overlay_state(
            overlay_index,
            (unsigned char)(RS_CMD_OVL_STATE_CACHE_VALID |
                            RS_CMD_OVL_STATE_SESSION_LOADED),
            0u) != 0) {
      g_overlay_last_rc = RS_OVL_RC_REU_REG;
      return -1;
    }
  }

  g_overlay_loaded = 1;
  g_overlay_cached_reu = 1;
  rs_overlay_set_phase(RS_OVERLAY_PHASE_EXEC);
  rs_overlay_dbg_put('P');
  return 0;
}


int rs_overlay_boot_with_progress(RSOverlayProgressFn progress, void* user) {
  int reu_ok;
  const unsigned short preload_mask =
      RS_REU_OVL_CACHE_VALID_PARSE |
      RS_REU_OVL_CACHE_VALID_EXEC |
      RS_REU_OVL_CACHE_VALID_CMD3 |
      RS_REU_OVL_CACHE_VALID_CMD4 |
      RS_REU_OVL_CACHE_VALID_CMD5 |
      RS_REU_OVL_CACHE_VALID_CMD6 |
      RS_REU_OVL_CACHE_VALID_CMD7 |
      RS_REU_OVL_CACHE_VALID_CMD8 |
      RS_REU_OVL_CACHE_VALID_EDIT;

  /* Clear any stale logical files/channels left by autostart. */
  cbm_k_clall();
  g_dbg_state = 0u;
  rs_overlay_dbg_put('B');
  rs_overlay_progress_tick(progress, user, 1u);

  g_overlay_loaded = 0;
  rs_overlay_clear_phase();
  g_overlay_cached_reu = 0;

  reu_ok = rs_reu_available();
  rs_overlay_dbg_put(reu_ok ? 'Q' : 'q');
  if (!reu_ok) {
    g_overlay_last_rc = RS_OVL_RC_REU_REQUIRED;
    return -1;
  }
  if (rs_cmd_registry_seed() != 0) {
    g_overlay_last_rc = RS_OVL_RC_REU_REG;
    return -1;
  }
  if (rs_overlay_meta_read(preload_mask) != 0) {
    if (g_overlay_last_rc == 0u) {
      g_overlay_last_rc = RS_OVL_RC_REU_CACHE;
    }
    return -1;
  }
  if (rs_overlay_sync_preloaded_registry() != 0) {
    return -1;
  }
  rs_overlay_progress_tick(progress, user, 6u);
  return 0;
}

int rs_overlay_boot(void) {
  return rs_overlay_boot_with_progress(0, 0);
}

int rs_overlay_prepare_parse(void) {
  rs_overlay_dbg_put('P');
  if (!g_overlay_loaded) {
    g_overlay_last_rc = RS_OVL_RC_NOT_BOOTED;
    rs_overlay_clear_phase();
    rs_overlay_dbg_put('!');
    return -1;
  }
  if (!g_overlay_cached_reu) {
    g_overlay_last_rc = RS_OVL_RC_REU_REQUIRED;
    rs_overlay_clear_phase();
    rs_overlay_dbg_put('!');
    return -1;
  }
#if !RS_C64_OVERLAY_PRELOADED
  if (rs_overlay_meta_read(RS_REU_OVL_CACHE_VALID_PARSE) != 0) {
    g_overlay_cached_reu = 0;
    g_overlay_last_rc = RS_OVL_RC_REU_REQUIRED;
    rs_overlay_clear_phase();
    rs_overlay_dbg_put('!');
    return -1;
  }
#endif
  rs_overlay_dbg_put('R');
  if (rs_overlay_fetch_slot(1u, RS_OVERLAY_PHASE_PARSE, rs_overlay_parse_off()) == 0) {
    rs_overlay_dbg_put('p');
    return 0;
  }
  g_overlay_last_rc = RS_OVL_RC_REU_PARSE;
  rs_overlay_dbg_put('!');
  return -1;
}

int rs_overlay_prepare_exec(void) {
  rs_overlay_dbg_put('E');
  if (!g_overlay_loaded) {
    g_overlay_last_rc = RS_OVL_RC_NOT_BOOTED;
    rs_overlay_clear_phase();
    rs_overlay_dbg_put('!');
    return -1;
  }
  if (!g_overlay_cached_reu) {
    g_overlay_last_rc = RS_OVL_RC_REU_REQUIRED;
    rs_overlay_clear_phase();
    rs_overlay_dbg_put('!');
    return -1;
  }
#if !RS_C64_OVERLAY_PRELOADED
  if (rs_overlay_meta_read(RS_REU_OVL_CACHE_VALID_EXEC) != 0) {
    g_overlay_cached_reu = 0;
    g_overlay_last_rc = RS_OVL_RC_REU_REQUIRED;
    rs_overlay_clear_phase();
    rs_overlay_dbg_put('!');
    return -1;
  }
#endif
  rs_overlay_dbg_put('R');
  if (rs_overlay_fetch_slot(2u, RS_OVERLAY_PHASE_EXEC, rs_overlay_exec_off()) == 0) {
    rs_overlay_dbg_put('e');
    return 0;
  }
  g_overlay_last_rc = RS_OVL_RC_REU_EXEC;
  rs_overlay_dbg_put('!');
  return -1;
}

int rs_overlay_prepare_edit(void) {
  rs_overlay_dbg_put('I');
  if (!g_overlay_loaded) {
    g_overlay_last_rc = RS_OVL_RC_NOT_BOOTED;
    rs_overlay_clear_phase();
    rs_overlay_dbg_put('!');
    return -1;
  }
  if (!g_overlay_cached_reu) {
    g_overlay_last_rc = RS_OVL_RC_REU_REQUIRED;
    rs_overlay_clear_phase();
    rs_overlay_dbg_put('!');
    return -1;
  }
#if !RS_C64_OVERLAY_PRELOADED
  if (rs_overlay_meta_read(RS_REU_OVL_CACHE_VALID_EDIT) != 0) {
    g_overlay_cached_reu = 0;
    g_overlay_last_rc = RS_OVL_RC_REU_REQUIRED;
    rs_overlay_clear_phase();
    rs_overlay_dbg_put('!');
    return -1;
  }
#endif
  rs_overlay_dbg_put('R');
  if (rs_overlay_fetch_slot(9u, RS_OVERLAY_PHASE_EDIT, rs_overlay_edit_off()) == 0) {
    rs_overlay_dbg_put('i');
    return 0;
  }
  g_overlay_last_rc = RS_OVL_RC_REU_CMD;
  rs_overlay_dbg_put('!');
  return -1;
}

int rs_overlay_read_logical_line(char* out, unsigned short max) {
  int rc;
  if (!out || max == 0u) {
    return -1;
  }
  if (rs_overlay_prepare_edit() != 0 ||
      !rs_overlay_is_phase_ready(RS_OVERLAY_PHASE_EDIT)) {
    return -1;
  }
  rs_overlay_window_enter();
  /*
   * The parser/VM overlays run with IRQs held off, but the prompt editor waits
   * on the KERNAL keyboard path. Keep RAM visible under BASIC for overlay code
   * while allowing the normal IRQ keyboard scanner to keep updating GETIN.
   */
  __asm__("cli");
  rc = rs_vmovl_overlay9(out, max);
  rs_overlay_window_leave();
  return rc;
}

static int rs_overlay_prepare_command(const RSExternalCmdDescriptor* desc) {
  RSExternalOverlayState state;
  unsigned char overlay_num;
  unsigned long cache_off;
  unsigned short valid_bit;

  rs_overlay_dbg_put('D');
  if (!g_overlay_loaded) {
    g_overlay_last_rc = RS_OVL_RC_NOT_BOOTED;
    rs_overlay_clear_phase();
    rs_overlay_dbg_put('!');
    return -1;
  }
  if (!g_overlay_cached_reu) {
    g_overlay_last_rc = RS_OVL_RC_REU_REQUIRED;
    rs_overlay_clear_phase();
    rs_overlay_dbg_put('!');
    return -1;
  }

  if (!desc ||
      rs_cmd_registry_read_overlay_state(desc->overlay_index, &state) != 0) {
    g_overlay_last_rc = RS_OVL_RC_REU_CMD;
    rs_overlay_dbg_put('!');
    return -1;
  }
  overlay_num = rs_overlay_num_from_state_index(desc->overlay_index);
  valid_bit = rs_overlay_valid_bit(overlay_num);
  cache_off = ((unsigned long)state.cache_bank << 16u) + (unsigned long)state.cache_off;

  if ((state.load_flags & RS_CMD_OVL_LOAD_F_REU_CACHE) != 0 &&
      (state.load_state & RS_CMD_OVL_STATE_CACHE_VALID) != 0 &&
      valid_bit != 0u &&
      rs_overlay_meta_read(valid_bit) == 0) {
    rs_overlay_dbg_put('R');
    if (rs_overlay_fetch_slot(overlay_num, state.overlay_phase, cache_off) == 0) {
      (void)rs_cmd_registry_update_overlay_state(desc->overlay_index,
                                                 RS_CMD_OVL_STATE_SESSION_LOADED,
                                                 0u);
      rs_overlay_dbg_put('d');
      return 0;
    }
  }
  rs_overlay_dbg_put('!');
  g_overlay_last_rc = RS_OVL_RC_REU_CMD;
  return -1;
}

int rs_overlay_command_call(RSCommandId id, unsigned char op, RSCommandFrame* frame) {
  RSExternalCmdDescriptor desc;
  int rc;
  if (!frame) {
    return -1;
  }
  frame->id = id;
  frame->op = op;
  if (rs_cmd_registry_lookup_external(id, &desc) != 0 ||
      rs_overlay_prepare_command(&desc) != 0) {
    return -1;
  }
  if (desc.overlay_index == 0u) {
    rc = rs_vmovl_overlay3(desc.handler, frame);
  } else if (desc.overlay_index == 1u) {
    rc = rs_vmovl_overlay4(desc.handler, frame);
  } else if (desc.overlay_index == 2u) {
    rc = rs_vmovl_overlay5(desc.handler, frame);
  } else if (desc.overlay_index == 3u) {
    rc = rs_vmovl_overlay6(desc.handler, frame);
  } else if (desc.overlay_index == 4u) {
    rc = rs_vmovl_overlay7(desc.handler, frame);
  } else if (desc.overlay_index == 5u) {
    rc = rs_vmovl_overlay8(desc.handler, frame);
  } else {
    rc = -1;
  }
  if (rs_overlay_prepare_exec() != 0) {
    return -1;
  }
  return rc;
}

int rs_overlay_active(void) {
  return g_overlay_loaded;
}

int rs_overlay_is_phase_ready(unsigned char phase) {
  if (!g_overlay_loaded) {
    return 0;
  }
  return g_overlay_active_phase == phase;
}

unsigned char rs_overlay_last_rc(void) {
  return g_overlay_last_rc;
}

#else

int rs_overlay_boot(void) {
  return 0;
}

int rs_overlay_boot_with_progress(RSOverlayProgressFn progress, void* user) {
  (void)progress;
  (void)user;
  return 0;
}

int rs_overlay_prepare_parse(void) {
  return 0;
}

int rs_overlay_prepare_exec(void) {
  return 0;
}

int rs_overlay_prepare_edit(void) {
  return 0;
}

int rs_overlay_read_logical_line(char* out, unsigned short max) {
  (void)out;
  (void)max;
  return -1;
}

int rs_overlay_command_call(RSCommandId id, unsigned char op, RSCommandFrame* frame) {
  (void)id;
  (void)op;
  (void)frame;
  return -1;
}

int rs_overlay_active(void) {
  return 0;
}

int rs_overlay_is_phase_ready(unsigned char phase) {
  (void)phase;
  return 1;
}

unsigned char rs_overlay_last_rc(void) {
  return 0u;
}

void rs_overlay_debug_mark(unsigned char code) {
  (void)code;
}

#endif
