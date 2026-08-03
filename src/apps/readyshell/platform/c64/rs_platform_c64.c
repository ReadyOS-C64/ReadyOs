#include "../rs_platform.h"
#include "../../core/rs_ui_state.h"
#include "reu_mgr.h"
#include "reu_control_bank.h"

#define RS_REU_STATE_BANK_CACHE (*(unsigned char*)0xCFF2)

int rs_reu_available(void) {
  unsigned char probe;
  unsigned char check;
  unsigned char bank;

  bank = rs_reu_state_bank();
  if (bank == 0u) {
    return 0;
  }

  probe = 0xA5u;
  check = 0u;
  reu_dma_stash((unsigned int)&probe, bank, RS_REU_PROBE_REL, 1u);
  reu_dma_fetch((unsigned int)&check, bank, RS_REU_PROBE_REL, 1u);
  return check == probe;
}

unsigned char rs_reu_state_bank(void) {
  unsigned int bank;

  if (RS_REU_STATE_BANK_CACHE != 0u &&
      readyos_bank_read_byte((unsigned int)(REUCB_BANK_TYPE_OFF +
                                            RS_REU_STATE_BANK_CACHE)) ==
          REU_RS_SCRATCH) {
    return RS_REU_STATE_BANK_CACHE;
  }

  for (bank = 0u; bank < REU_TOTAL_BANKS; ++bank) {
    if (readyos_bank_read_byte((unsigned int)(REUCB_BANK_TYPE_OFF + bank)) ==
        REU_RS_SCRATCH) {
      RS_REU_STATE_BANK_CACHE = (unsigned char)bank;
      return (unsigned char)bank;
    }
  }

  return 0u;
}

unsigned long rs_reu_state_abs(unsigned short rel_off) {
  return ((unsigned long)rs_reu_state_bank() << 16u) + (unsigned long)rel_off;
}

int rs_reu_read(unsigned long reu_off, void* ram_dst, unsigned short len) {
  unsigned char bank;
  unsigned int off;
  if (len == 0u) {
    return 0;
  }
  if (!ram_dst) {
    return -1;
  }
  bank = (unsigned char)((reu_off >> 16u) & 0xFFul);
  off = (unsigned int)(reu_off & 0xFFFFul);
  reu_dma_fetch((unsigned int)ram_dst, bank, off, (unsigned int)len);
  return 0;
}

int rs_reu_write(unsigned long reu_off, const void* ram_src, unsigned short len) {
  unsigned char bank;
  unsigned int off;
  if (len == 0u) {
    return 0;
  }
  if (!ram_src) {
    return -1;
  }
  bank = (unsigned char)((reu_off >> 16u) & 0xFFul);
  off = (unsigned int)(reu_off & 0xFFFFul);
  reu_dma_stash((unsigned int)ram_src, bank, off, (unsigned int)len);
  return 0;
}
