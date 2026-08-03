/* tui_readyos.h - tiny shim-ABI ReadyOS-bank byte access for TUI modules */

#ifndef TUI_READYOS_H
#define TUI_READYOS_H

unsigned char tui_readyos_read_byte(unsigned int offset);
void tui_readyos_write_byte(unsigned int offset, unsigned char value);

#endif
