/*
 * reu_phys_probe.c - launcher-owned physical REU size probe
 */

#include "reu_phys.h"

#define REU_TEST_OFF 0xFFF0u

static unsigned char reu_phys_probe_value;

static void reu_phys_stash_probe_byte(unsigned char bank, unsigned char value) {
    reu_phys_probe_value = value;
    reu_dma_stash((unsigned int)&reu_phys_probe_value, bank, REU_TEST_OFF, 1u);
}

static unsigned char reu_phys_fetch_probe_byte(unsigned char bank) {
    reu_phys_probe_value = 0u;
    reu_dma_fetch((unsigned int)&reu_phys_probe_value, bank, REU_TEST_OFF, 1u);
    return reu_phys_probe_value;
}

unsigned char reu_phys_detect_bank_count(void) {
    unsigned char base_bank;
    unsigned char base_orig;
    unsigned char cand_orig;
    unsigned char got;
    unsigned int bank;

    base_bank = REU_FIRST_DYNAMIC_PHYSICAL();
    base_orig = reu_phys_fetch_probe_byte(base_bank);
    reu_phys_stash_probe_byte(base_bank, 0x5Au);

    for (bank = (unsigned int)base_bank + 1u; bank < 256u; ++bank) {
        cand_orig = reu_phys_fetch_probe_byte((unsigned char)bank);
        reu_phys_stash_probe_byte((unsigned char)bank, 0xC3u);
        got = reu_phys_fetch_probe_byte(base_bank);
        reu_phys_stash_probe_byte((unsigned char)bank, cand_orig);
        if (got == 0xC3u) {
            reu_phys_stash_probe_byte(base_bank, base_orig);
            return (unsigned char)(bank - (unsigned int)base_bank);
        }
    }

    reu_phys_stash_probe_byte(base_bank, base_orig);
    return REU_PHYS_COUNT_256;
}
