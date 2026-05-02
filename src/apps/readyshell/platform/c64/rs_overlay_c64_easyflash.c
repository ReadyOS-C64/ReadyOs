#include "../rs_overlay.h"
#include "../rs_platform.h"
#include "../rs_memcfg.h"
#include "../../core/rs_cmd_overlay.h"
#include "../../core/rs_cmd_registry.h"
#include "../../core/rs_ui_state.h"

#include <string.h>

#define RS_OVERLAY_COUNT 8u
#define RS_REU_OVL_CACHE_BASE 0x400000ul
#define RS_REU_OVL_CACHE_BASE2 0x410000ul
#define RS_REU_OVL_CACHE_PARSE_OFF (RS_REU_OVL_CACHE_BASE + (unsigned long)RS_REU_OVL_CACHE_PARSE_REL)
#define RS_REU_OVL_CACHE_EXEC_OFF  (RS_REU_OVL_CACHE_BASE + (unsigned long)RS_REU_OVL_CACHE_EXEC_REL)
#define RS_OVL_RC_NOT_BOOTED     0xE3u
#define RS_OVL_RC_REU_PARSE      0xE4u
#define RS_OVL_RC_REU_EXEC       0xE5u
#define RS_OVL_RC_REU_REQUIRED   0xE9u
#define RS_OVL_RC_REU_CACHE      0xEAu
#define RS_OVL_RC_REU_CMD        0xEBu
#define RS_OVL_RC_REU_REG        0xECu

static int g_overlay_loaded = 0;
static int g_overlay_cached_reu = 0;
static unsigned char g_overlay_last_rc = 0u;
static unsigned char g_overlay_active_phase = RS_OVERLAY_PHASE_NONE;
static unsigned char g_overlay_meta_buf[RS_REU_OVL_CACHE_META_LEN];

extern unsigned char _OVERLAY1_LOAD__[];
#define RS_OVERLAY_LOAD_RAM _OVERLAY1_LOAD__

extern int rs_vmovl_overlay3(unsigned char handler, RSCommandFrame* frame);
extern int rs_vmovl_overlay4(unsigned char handler, RSCommandFrame* frame);
extern int rs_vmovl_overlay5(unsigned char handler, RSCommandFrame* frame);
extern int rs_vmovl_overlay6(unsigned char handler, RSCommandFrame* frame);
extern int rs_vmovl_overlay7(unsigned char handler, RSCommandFrame* frame);
extern int rs_vmovl_overlay8(unsigned char handler, RSCommandFrame* frame);

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

static void rs_overlay_window_enter(void) {
  rs_memcfg_push_ram_under_basic();
}

static void rs_overlay_window_leave(void) {
  rs_memcfg_pop();
}

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

static int rs_overlay_meta_read(unsigned char needed_mask) {
  unsigned short slot_len;

  memset(g_overlay_meta_buf, 0, sizeof(g_overlay_meta_buf));
  if (rs_reu_read(RS_REU_OVL_CACHE_META_OFF, g_overlay_meta_buf, sizeof(g_overlay_meta_buf)) != 0) {
    return -1;
  }
  if (g_overlay_meta_buf[0] != 'O' ||
      g_overlay_meta_buf[1] != 'V' ||
      g_overlay_meta_buf[2] != RS_REU_OVL_CACHE_META_VERSION ||
      g_overlay_meta_buf[4] != RS_REU_OVL_CACHE_BANK ||
      g_overlay_meta_buf[5] != RS_REU_OVL_CACHE_BANK2) {
    return -1;
  }
  if ((g_overlay_meta_buf[3] & needed_mask) != needed_mask) {
    return -1;
  }

  slot_len = (unsigned short)g_overlay_meta_buf[6] |
             ((unsigned short)g_overlay_meta_buf[7] << 8u);
  if (slot_len != RS_REU_OVL_CACHE_SLOT_LEN) {
    return -1;
  }
  return 0;
}

static unsigned char rs_overlay_valid_bit(unsigned char overlay_num) {
  if (overlay_num == 0u || overlay_num > RS_OVERLAY_COUNT) {
    return 0u;
  }
  return (unsigned char)(1u << (overlay_num - 1u));
}

static unsigned char rs_overlay_num_from_state_index(unsigned char index) {
  return (unsigned char)(index + 3u);
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

int rs_overlay_boot_with_progress(RSOverlayProgressFn progress, void* user) {
  unsigned char overlay_index;

  rs_overlay_progress_tick(progress, user, 1u);
  g_overlay_loaded = 0;
  rs_overlay_clear_phase();
  g_overlay_cached_reu = 0;

  if (!rs_reu_available()) {
    g_overlay_last_rc = RS_OVL_RC_REU_REQUIRED;
    return -1;
  }
  if (rs_cmd_registry_seed() != 0) {
    g_overlay_last_rc = RS_OVL_RC_REU_REG;
    return -1;
  }
  if (rs_overlay_meta_read(RS_REU_OVL_CACHE_VALID_PARSE |
                           RS_REU_OVL_CACHE_VALID_EXEC |
                           RS_REU_OVL_CACHE_VALID_CMD3 |
                           RS_REU_OVL_CACHE_VALID_CMD4 |
                           RS_REU_OVL_CACHE_VALID_CMD5 |
                           RS_REU_OVL_CACHE_VALID_CMD6 |
                           RS_REU_OVL_CACHE_VALID_CMD7 |
                           RS_REU_OVL_CACHE_VALID_CMD8) != 0) {
    g_overlay_last_rc = RS_OVL_RC_REU_CACHE;
    return -1;
  }

  for (overlay_index = 0u; overlay_index < 6u; ++overlay_index) {
    if (rs_cmd_registry_update_overlay_state(
            overlay_index,
            RS_CMD_OVL_STATE_CACHE_VALID,
            0u) != 0) {
      g_overlay_last_rc = RS_OVL_RC_REU_REG;
      return -1;
    }
  }

  g_overlay_loaded = 1;
  g_overlay_cached_reu = 1;
  rs_overlay_set_phase(RS_OVERLAY_PHASE_EXEC);
  rs_overlay_progress_tick(progress, user, 2u);
  return 0;
}

int rs_overlay_boot(void) {
  return rs_overlay_boot_with_progress(0, 0);
}

int rs_overlay_prepare_parse(void) {
  if (!g_overlay_loaded) {
    g_overlay_last_rc = RS_OVL_RC_NOT_BOOTED;
    rs_overlay_clear_phase();
    return -1;
  }
  if (!g_overlay_cached_reu || rs_overlay_meta_read(RS_REU_OVL_CACHE_VALID_PARSE) != 0) {
    g_overlay_cached_reu = 0;
    g_overlay_last_rc = RS_OVL_RC_REU_REQUIRED;
    rs_overlay_clear_phase();
    return -1;
  }
  if (rs_overlay_fetch_slot(1u, RS_OVERLAY_PHASE_PARSE, RS_REU_OVL_CACHE_PARSE_OFF) == 0) {
    return 0;
  }
  g_overlay_last_rc = RS_OVL_RC_REU_PARSE;
  return -1;
}

int rs_overlay_prepare_exec(void) {
  if (!g_overlay_loaded) {
    g_overlay_last_rc = RS_OVL_RC_NOT_BOOTED;
    rs_overlay_clear_phase();
    return -1;
  }
  if (!g_overlay_cached_reu || rs_overlay_meta_read(RS_REU_OVL_CACHE_VALID_EXEC) != 0) {
    g_overlay_cached_reu = 0;
    g_overlay_last_rc = RS_OVL_RC_REU_REQUIRED;
    rs_overlay_clear_phase();
    return -1;
  }
  if (rs_overlay_fetch_slot(2u, RS_OVERLAY_PHASE_EXEC, RS_REU_OVL_CACHE_EXEC_OFF) == 0) {
    return 0;
  }
  g_overlay_last_rc = RS_OVL_RC_REU_EXEC;
  return -1;
}

static int rs_overlay_prepare_command(const RSExternalCmdDescriptor* desc) {
  RSExternalOverlayState state;
  unsigned char overlay_num;
  unsigned long cache_off;
  unsigned char valid_bit;

  if (!g_overlay_loaded) {
    g_overlay_last_rc = RS_OVL_RC_NOT_BOOTED;
    rs_overlay_clear_phase();
    return -1;
  }
  if (!g_overlay_cached_reu) {
    g_overlay_last_rc = RS_OVL_RC_REU_REQUIRED;
    rs_overlay_clear_phase();
    return -1;
  }
  if (!desc || rs_cmd_registry_read_overlay_state(desc->overlay_index, &state) != 0) {
    g_overlay_last_rc = RS_OVL_RC_REU_CMD;
    return -1;
  }

  overlay_num = rs_overlay_num_from_state_index(desc->overlay_index);
  valid_bit = rs_overlay_valid_bit(overlay_num);
  cache_off = ((unsigned long)state.cache_bank << 16u) + (unsigned long)state.cache_off;
  if (valid_bit == 0u ||
      (state.load_flags & RS_CMD_OVL_LOAD_F_REU_CACHE) == 0u ||
      rs_overlay_meta_read(valid_bit) != 0) {
    g_overlay_last_rc = RS_OVL_RC_REU_CACHE;
    return -1;
  }
  if (rs_overlay_fetch_slot(overlay_num, state.overlay_phase, cache_off) != 0) {
    g_overlay_last_rc = RS_OVL_RC_REU_CACHE;
    return -1;
  }
  if (rs_cmd_registry_update_overlay_state(desc->overlay_index,
                                           RS_CMD_OVL_STATE_SESSION_LOADED,
                                           0u) != 0) {
    g_overlay_last_rc = RS_OVL_RC_REU_REG;
    return -1;
  }
  return 0;
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

void rs_overlay_debug_mark(unsigned char code) {
  (void)code;
}
