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
