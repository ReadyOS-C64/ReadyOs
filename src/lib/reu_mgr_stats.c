/* reu_mgr_stats.c - authoritative ReadyOS-bank allocation queries */

#include "reu_mgr.h"
#include "reu_control_bank.h"

unsigned char reu_bank_type(unsigned char bank) {
    return readyos_bank_read_byte((unsigned int)(REUCB_BANK_TYPE_OFF + bank));
}

unsigned char reu_count_free(void) {
    unsigned int i;
    unsigned char count;

    count = 0u;
    for (i = 0u; i < REU_TOTAL_BANKS; ++i) {
        if (readyos_bank_read_byte((unsigned int)(REUCB_BANK_TYPE_OFF + i)) == REU_FREE) {
            if (++count == 255u) {
                break;
            }
        }
    }
    return count;
}

unsigned char reu_count_type(unsigned char type) {
    unsigned int i;
    unsigned char count;

    count = 0u;
    for (i = 0u; i < REU_TOTAL_BANKS; ++i) {
        if (readyos_bank_read_byte((unsigned int)(REUCB_BANK_TYPE_OFF + i)) == type) {
            if (++count == 255u) {
                break;
            }
        }
    }
    return count;
}
