/*
 * reu_mgr_stats.c - REU manager query helpers
 * Split from core so apps that only use clipboard can avoid this code.
 */

#include "reu_mgr.h"

unsigned char reu_bank_type(unsigned char bank) {
    return REU_ALLOC_TABLE[bank];
}

unsigned char reu_count_free(void) {
    unsigned int i;
    unsigned char count = 0;

    for (i = 0; i < REU_TOTAL_BANKS; ++i) {
        if (REU_ALLOC_TABLE[i] == REU_FREE) {
            ++count;
            if (count == 255) break;
        }
    }

    return count;
}

unsigned char reu_count_type(unsigned char type) {
    unsigned int i;
    unsigned char count = 0;

    for (i = 0; i < REU_TOTAL_BANKS; ++i) {
        if (REU_ALLOC_TABLE[i] == type) {
            ++count;
            if (count == 255) break;
        }
    }

    return count;
}
