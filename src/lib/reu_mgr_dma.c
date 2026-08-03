/*
 * reu_mgr_dma.c - REU DMA transfer helpers
 */

#include "reu_mgr.h"

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
#define READYOS_BYTE_SCRATCH (*(volatile unsigned char*)0xC83D)

void reu_dma_stash(unsigned int c64_addr, unsigned char bank,
                   unsigned int reu_offset, unsigned int length) {
    REU_C64_LO   = (unsigned char)(c64_addr & 0xFF);
    REU_C64_HI   = (unsigned char)(c64_addr >> 8);
    REU_REU_LO   = (unsigned char)(reu_offset & 0xFF);
    REU_REU_HI   = (unsigned char)(reu_offset >> 8);
    REU_REU_BANK = bank;
    REU_LEN_LO   = (unsigned char)(length & 0xFF);
    REU_LEN_HI   = (unsigned char)(length >> 8);
    REU_COMMAND  = REU_CMD_STASH;
}

void reu_dma_fetch(unsigned int c64_addr, unsigned char bank,
                   unsigned int reu_offset, unsigned int length) {
    REU_C64_LO   = (unsigned char)(c64_addr & 0xFF);
    REU_C64_HI   = (unsigned char)(c64_addr >> 8);
    REU_REU_LO   = (unsigned char)(reu_offset & 0xFF);
    REU_REU_HI   = (unsigned char)(reu_offset >> 8);
    REU_REU_BANK = bank;
    REU_LEN_LO   = (unsigned char)(length & 0xFF);
    REU_LEN_HI   = (unsigned char)(length >> 8);
    REU_COMMAND  = REU_CMD_FETCH;
}

unsigned char readyos_bank_read_byte(unsigned int offset) {
    reu_dma_fetch(0xC83Du, REU_READYOS_GLOBAL_PHYSICAL(), offset, 1u);
    return READYOS_BYTE_SCRATCH;
}

void readyos_bank_write_byte(unsigned int offset, unsigned char value) {
    READYOS_BYTE_SCRATCH = value;
    reu_dma_stash(0xC83Du, REU_READYOS_GLOBAL_PHYSICAL(), offset, 1u);
}

void readyos_bank_read(unsigned int offset, void *dst, unsigned int length) {
    reu_dma_fetch((unsigned int)dst, REU_READYOS_GLOBAL_PHYSICAL(), offset, length);
}

void readyos_bank_write(unsigned int offset, const void *src, unsigned int length) {
    reu_dma_stash((unsigned int)src, REU_READYOS_GLOBAL_PHYSICAL(), offset, length);
}
