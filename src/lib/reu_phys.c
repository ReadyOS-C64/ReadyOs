/* reu_phys.c - physical REU size policy stored in the ReadyOS bank */

#include "reu_phys.h"
#include "reu_control_bank.h"

void reu_phys_apply_to_alloc_table(unsigned char encoded_count) {
    reu_control_bank_prepare(encoded_count);
}

unsigned char reu_phys_count_from_alloc_table(void) {
    if (reu_control_bank_is_valid()) {
        return readyos_bank_read_byte((unsigned int)(REUCB_HEADER_OFF +
                                                     REUCB_HEADER_PHYS_BANKS));
    }
    return REU_PHYS_COUNT_256;
}
