/*
 * tui_readyos.c - no-BSS ReadyOS-bank byte DMA for lightweight TUI apps.
 *
 * Normal apps already link reu_mgr_dma.c and use the zero-frame aliases in
 * tui_readyos_alias.s to avoid duplicating this register setup. Keep this
 * standalone implementation for deliberately small apps that do not otherwise
 * need the REU manager. tui_readyos_reu_mgr.c is the retained C reference.
 */

#include "tui_readyos.h"

#define REU_COMMAND  (*(unsigned char*)0xDF01)
#define REU_C64_LO   (*(unsigned char*)0xDF02)
#define REU_C64_HI   (*(unsigned char*)0xDF03)
#define REU_REU_LO   (*(unsigned char*)0xDF04)
#define REU_REU_HI   (*(unsigned char*)0xDF05)
#define REU_REU_BANK (*(unsigned char*)0xDF06)
#define REU_LEN_LO   (*(unsigned char*)0xDF07)
#define REU_LEN_HI   (*(unsigned char*)0xDF08)
#define READYOS_BANK (*(unsigned char*)0xC83B)
#define READYOS_BYTE_SCRATCH (*(unsigned char*)0xC83D)

static void tui_readyos_transfer(unsigned int offset, unsigned char command) {
    REU_C64_LO = 0x3Du;
    REU_C64_HI = 0xC8u;
    REU_REU_LO = (unsigned char)offset;
    REU_REU_HI = (unsigned char)(offset >> 8);
    REU_REU_BANK = READYOS_BANK;
    REU_LEN_LO = 1u;
    REU_LEN_HI = 0u;
    REU_COMMAND = command;
}

unsigned char tui_readyos_read_byte(unsigned int offset) {
    tui_readyos_transfer(offset, 0x91u);
    return READYOS_BYTE_SCRATCH;
}

void tui_readyos_write_byte(unsigned int offset, unsigned char value) {
    READYOS_BYTE_SCRATCH = value;
    tui_readyos_transfer(offset, 0x90u);
}
