/* Host regression for the production DMA system-record lifecycle.
 * cc -std=c99 -Wall -Wextra build_support/reu_dma_record_test.c -o /tmp/reu_dma_record_test
 * /tmp/reu_dma_record_test
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/lib/reu_control_bank.h"

static unsigned char system_bank[65536];
static unsigned char host_readyos_bank = 32;
#undef SHIM_READYOS_BANK
#define SHIM_READYOS_BANK (&host_readyos_bank)

unsigned char readyos_bank_read_byte(unsigned int offset) {
    assert(offset < sizeof(system_bank));
    return system_bank[offset];
}
void readyos_bank_write_byte(unsigned int offset, unsigned char value) {
    assert(offset < sizeof(system_bank));
    system_bank[offset] = value;
}
void readyos_bank_read(unsigned int offset, void *dst, unsigned int length) {
    assert(offset + length <= sizeof(system_bank));
    memcpy(dst, system_bank + offset, length);
}
void readyos_bank_write(unsigned int offset, const void *src, unsigned int length) {
    assert(offset + length <= sizeof(system_bank));
    memcpy(system_bank + offset, src, length);
}

/* Exercise the actual implementation, substituting only REU I/O and shim RAM. */
#include "../src/lib/reu_control_bank.c"

int main(void) {
    unsigned int i;
    unsigned char saved[REUCB_DMA_SIZE];
    unsigned char *record = system_bank + REUCB_DMA_OFF;
    unsigned char *header = system_bank + REUCB_HEADER_OFF;

    memset(system_bank, 0xA5, sizeof(system_bank));
    assert(!reu_control_bank_dma_is_valid());
    reu_control_bank_reset_dma();
    assert(reu_control_bank_dma_is_valid());
    for (i = 0; i < sizeof(system_bank); ++i) {
        if (i < REUCB_DMA_OFF || i >= REUCB_DMA_OFF + REUCB_DMA_SIZE)
            assert(system_bank[i] == 0xA5);
    }
    for (i = 3; i < REUCB_DMA_SIZE; ++i) assert(record[i] == 0);
    for (i = 0; i < 3; ++i) {
        record[i] ^= 0xFF;
        assert(!reu_control_bank_dma_is_valid());
        record[i] ^= 0xFF;
    }

    record[REUCB_DMA_OFF_FLAGS] = REUCB_DMA_COMPILED | REUCB_DMA_ENABLED |
        REUCB_DMA_CHECKED | REUCB_DMA_AVAILABLE | REUCB_DMA_USED;
    memcpy(record + REUCB_DMA_OFF_PATH, "/usb1/test.d81", 15);
    memcpy(saved, record, sizeof(saved));
    header[REUCB_HEADER_PHYS_BANKS] = 0;
    reu_control_bank_sync_and_mirror(REUCB_WRITER_LAUNCHER);
    assert(reu_control_bank_is_valid());
    assert(header[REUCB_HEADER_DMA_OFF_LO] == (REUCB_DMA_OFF & 255));
    assert(header[REUCB_HEADER_DMA_OFF_HI] == (REUCB_DMA_OFF >> 8));
    assert(header[REUCB_HEADER_DMA_SIZE] == REUCB_DMA_SIZE);
    assert(header[REUCB_HEADER_DMA_VERSION] == REUCB_DMA_VERSION);
    assert(memcmp(saved, record, sizeof(saved)) == 0);
    /* Valid warm prepare avoids cc65's 16-bit cold-clear wrap on this host. */
    reu_control_bank_prepare(0);
    assert(memcmp(saved, record, sizeof(saved)) == 0);
    reu_control_bank_reset_dma();
    assert(reu_control_bank_dma_is_valid());
    for (i = 3; i < REUCB_DMA_SIZE; ++i) assert(record[i] == 0);
    puts("PASS DMA record: isolated reset, signature/version rejection, public descriptor, warm preservation");
    return 0;
}
