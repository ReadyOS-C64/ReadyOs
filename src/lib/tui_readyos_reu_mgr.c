/*
 * tui_readyos_reu_mgr.c - safe C reference adapter retained for provenance.
 *
 * Production builds use tui_readyos_alias.s: its zero-frame tail jumps avoid
 * duplicating DMA code without perturbing cc65's software-stack arguments.
 * This C form remains a correct fallback/reference but is deliberately not
 * linked because ordinary C calls cost more resident bytes.
 */

#include "tui_readyos.h"
#include "reu_mgr.h"

unsigned char tui_readyos_read_byte(unsigned int offset) {
    return readyos_bank_read_byte(offset);
}

void tui_readyos_write_byte(unsigned int offset, unsigned char value) {
    readyos_bank_write_byte(offset, value);
}
