/* reu_mgr_alloc.c - authoritative ReadyOS-bank allocation helpers */

#include "reu_mgr.h"
#include "reu_control_bank.h"

static unsigned char reu_fixed_bank_type(unsigned char bank) {
    if (bank < REU_FIRST_DYNAMIC_PHYSICAL()) {
        return REU_SKIPPED;
    }
    if (bank == REU_READYOS_GLOBAL_PHYSICAL()) {
        return REU_GLOBAL;
    }
    return 0xFFu;
}

unsigned char reu_alloc_bank(unsigned char type) {
    unsigned int bank;

    /* $FF remains the legacy failure return, so the legacy allocator reserves
     * physical bank 255. Token mappings themselves still represent all bytes. */
    for (bank = REU_FIRST_DYNAMIC_PHYSICAL(); bank < 255u; ++bank) {
        if (reu_fixed_bank_type((unsigned char)bank) != 0xFFu) {
            continue;
        }
        if (readyos_bank_read_byte((unsigned int)(REUCB_BANK_TYPE_OFF + bank)) == REU_FREE) {
            readyos_bank_write_byte((unsigned int)(REUCB_BANK_TYPE_OFF + bank), type);
            return (unsigned char)bank;
        }
    }
    return 0xFFu;
}

void reu_free_bank(unsigned char bank) {
    unsigned char fixed_type;

    fixed_type = reu_fixed_bank_type(bank);
    readyos_bank_write_byte((unsigned int)(REUCB_BANK_TYPE_OFF + bank),
                            (fixed_type == 0xFFu) ? REU_FREE : fixed_type);
}
