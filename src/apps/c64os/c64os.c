/* Experimental bridge for the fixed 16MB REU + IDE64 v4.1 environment. */
#include <conio.h>
#include "../../lib/reu_control_bank.h"

extern void bridge_enter(void);
extern unsigned char bridge_target, bridge_proof, bridge_failprobe;
static unsigned char header[64];
static unsigned char record[8];
static const unsigned char signature[5] = {0x52,0x42,0x47,0x33,3};
static unsigned char shim[1024];
static unsigned int cycles;
static unsigned int i;
static unsigned char key;

static unsigned char supported(void) {
    if (*SHIM_READYOS_BANK != 39u) return 0;
    /* v4.1, STD cartridge mapping: OPEN would hide the app itself. */
    if ((*(volatile unsigned char*)0xde32 & 0xe3u) != 0x23u) return 0;
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
    cputs("c: start or resume c64 os\r\n");
    cputs("s: save, damage zp/stack/shim, restore\r\n");
    cputs("x: test missing boot file recovery\r\n");
    cputs("f1: readyos launcher\r\n\r\n");
    if (!supported()) {
        cputs("requires skip 39, 16mb reu, ide64 v4.1");
        cgetc();
        __asm__("jmp $c80c");
    }
    readyos_bank_read(REUCB_RESERVED_OFF, record, sizeof(record));
    for (i = 0; i < sizeof(signature); ++i) {
        if (record[i] != signature[i]) break;
    }
    if (i != sizeof(signature)) {
        for (i = 0; i < sizeof(record); ++i) record[i] = 0;
        for (i = 0; i < sizeof(signature); ++i) record[i] = signature[i];
        readyos_bank_write(REUCB_RESERVED_OFF, record, sizeof(record));
    }
    for (;;) {
        key = cgetc();
        if (key == 133u) __asm__("jmp $c80c");
        if (key != 's' && key != 'c' && key != 'x') continue;
        bridge_proof = key == 's';
        bridge_failprobe = key == 'x';
        bridge_target = 32;
        if (!bridge_proof) {
            readyos_bank_read(REUCB_RESERVED_OFF, record, sizeof(record));
            bridge_target = record[6] == 1 ? 34 : 255;
            if (bridge_failprobe) bridge_target = 255;
        }
        record[7] = 0;
        readyos_bank_write(REUCB_RESERVED_OFF + 7, &record[7], 1);
        /* The REU API legitimately updates shim DMA parameters. Compare the
         * boundary state after those calls, immediately before suspension. */
        for (i = 0; i < sizeof(shim); ++i)
            shim[i] = ((volatile unsigned char*)0xc600)[i];
        bridge_enter();
        for (i = 0; i < sizeof(shim); ++i) {
            if (shim[i] != ((volatile unsigned char*)0xc600)[i]) {
                cputs("\r\nshim mismatch - stop");
                for (;;) {}
            }
        }
        readyos_bank_read(REUCB_RESERVED_OFF + 7, &record[7], 1);
        key = record[7];
        if (key) {
            cprintf("\r\ncold boot failed: %u; readyos restored", key);
            continue;
        }
        ++cycles;
        cprintf("\r\nrestored zp/stack/shim: %u", cycles);
    }
    return 0;
}
