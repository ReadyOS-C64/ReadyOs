/*
 * reu_owned_alloc.c - tiny app-side owned REU allocation micromodule
 */

#include "reu_owned_alloc.h"
#include "reu_control_bank.h"

#define SHIM_CURRENT_BANK (*(volatile unsigned char*)0xC834)

static unsigned char owned_rec[REUCB_RSRC_REC_SIZE];

static unsigned int owned_rec_off(unsigned char rec_index) {
    return (unsigned int)(REUCB_RSRC_REC_OFF +
                          ((unsigned int)rec_index * REUCB_RSRC_REC_SIZE));
}

static unsigned char owned_current_app_id(void) {
    unsigned char logical;
    unsigned char app_id;

    logical = SHIM_CURRENT_BANK;
    if (logical == 0u) {
        return REUCB_NULL_REC;
    }

    app_id = readyos_bank_read_byte((unsigned int)(REUCB_TOKEN_APP_OFF + logical));
    return (app_id < REUCB_APP_REG_COUNT) ? app_id : REUCB_NULL_REC;
}

static void owned_clear_record_for_bank(unsigned char bank) {
    unsigned char i;
    unsigned char control_bank;

    control_bank = REU_READYOS_GLOBAL_PHYSICAL();
    for (i = 0u; i < REUCB_RSRC_REC_COUNT; ++i) {
        reu_dma_fetch((unsigned int)owned_rec, control_bank,
                      owned_rec_off(i), REUCB_RSRC_REC_SIZE);
        if (owned_rec[0] != REUCB_NULL_REC &&
            owned_rec[2] == REUCB_DEP_KIND_APP_ALLOC &&
            owned_rec[3] == bank) {
            owned_rec[0] = REUCB_NULL_REC;
            reu_dma_stash((unsigned int)owned_rec, control_bank,
                          owned_rec_off(i), 1u);
            return;
        }
    }
}

static unsigned char owned_write_record(unsigned char app_id,
                                        unsigned char bank,
                                        unsigned char slot_id,
                                        const char *tag) {
    unsigned char i;
    unsigned char j;
    unsigned char control_bank;

    control_bank = REU_READYOS_GLOBAL_PHYSICAL();
    for (i = 0u; i < REUCB_RSRC_REC_COUNT; ++i) {
        reu_dma_fetch((unsigned int)owned_rec, control_bank,
                      owned_rec_off(i), REUCB_RSRC_REC_SIZE);
        if (owned_rec[0] == REUCB_NULL_REC) {
            owned_rec[0] = app_id;
            owned_rec[1] = 0u;
            owned_rec[2] = REUCB_DEP_KIND_APP_ALLOC;
            owned_rec[3] = bank;
            owned_rec[4] = 0u;
            owned_rec[5] = 0u;
            owned_rec[6] = 0xFFu;
            owned_rec[7] = 0xFFu;
            owned_rec[8] = 1u;
            owned_rec[9] = REUCB_NULL_REC;
            owned_rec[10] = slot_id;
            owned_rec[11] = 0u;
            for (j = 0u; j < 4u; ++j) {
                owned_rec[(unsigned char)(12u + j)] =
                    (tag != 0 && tag[j] != 0) ? (unsigned char)tag[j] : 0u;
            }
            reu_dma_stash((unsigned int)owned_rec, control_bank,
                          owned_rec_off(i), REUCB_RSRC_REC_SIZE);
            return 1u;
        }
    }
    return 0u;
}

unsigned char reu_alloc_owned_bank(unsigned char slot_id, const char *tag) {
    unsigned char app_id;
    unsigned char bank;

    app_id = owned_current_app_id();
    if (app_id == REUCB_NULL_REC) {
        return 0xFFu;
    }

    bank = reu_alloc_bank(REU_APP_ALLOC);
    if (bank == 0xFFu) {
        return 0xFFu;
    }

    if (!owned_write_record(app_id, bank, slot_id, tag)) {
        reu_free_bank(bank);
        return 0xFFu;
    }
    return bank;
}

void reu_free_owned_bank(unsigned char bank) {
    if (bank == 0xFFu) {
        return;
    }
    owned_clear_record_for_bank(bank);
    reu_free_bank(bank);
}
