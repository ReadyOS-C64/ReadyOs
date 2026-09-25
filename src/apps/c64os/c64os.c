/* Experimental whole-machine transfer gate. Cold C64 OS boot and native return
 * utility remain to be integrated; this gate must pass before that handoff. */
#include <conio.h>
#include "../../lib/reu_control_bank.h"

extern void bridge_selftest(void);
static unsigned char header[64];
static unsigned char shim[1024];
static unsigned int cycles;
static unsigned int i;
static unsigned char key;

static unsigned char supported(void) {
    if (*SHIM_READYOS_BANK != 39u) return 0;
    readyos_bank_read(REUCB_HEADER_OFF, header, sizeof(header));
    return header[0] == REUCB_MAGIC0 && header[1] == REUCB_MAGIC1 &&
        header[2] == REUCB_MAGIC2 && header[3] == REUCB_MAGIC3 &&
        header[REUCB_HEADER_CONTROL_BANK] == 39u &&
        (header[REUCB_HEADER_FLAGS] & REUCB_HEADER_FLAG_PHYS_SIZE) &&
        header[REUCB_HEADER_PHYS_BANKS] == 0u;
}

int main(void) {
    clrscr();
    cputs("c64 os bridge - experimental\r\n\r\n");
    cputs("whole-machine transfer proof\r\n");
    cputs("s: save, damage zp/stack/shim, restore\r\n");
    cputs("f1: readyos launcher\r\n\r\n");
    if (!supported()) {
        cputs("requires skip 39 and verified 16mb reu");
        cgetc();
        __asm__("jmp $c80c");
    }
    for (;;) {
        key = cgetc();
        if (key == 133u) __asm__("jmp $c80c");
        if (key != 's') continue;
        for (i = 0; i < sizeof(shim); ++i)
            shim[i] = ((volatile unsigned char*)0xc600)[i];
        bridge_selftest();
        for (i = 0; i < sizeof(shim); ++i) {
            if (shim[i] != ((volatile unsigned char*)0xc600)[i]) {
                cputs("\r\nshim mismatch - stop");
                for (;;) {}
            }
        }
        ++cycles;
        cprintf("\r\nrestored zp/stack/shim: %u", cycles);
    }
    return 0;
}
