/*
 * clipboard_copy.c - Clipboard copy path
 */

#include "clipboard.h"
#include "reu_mgr.h"
#include "reu_control_bank.h"

#define ITEM_BANK    0
#define ITEM_TYPE    1
#define ITEM_SIZE_LO 2
#define ITEM_SIZE_HI 3

unsigned char clip_copy(unsigned char type, const void *data, unsigned int size) {
    unsigned char bank;

    bank = reu_alloc_bank(REU_CLIPBOARD);
    if (bank == 0xFF) {
        return 0xFF;
    }

    if (clip_item_count() >= CLIP_MAX_ITEMS) {
        reu_free_bank(bank);
        return 0xFF;
    }

    reu_dma_stash((unsigned int)data, bank, 0, size);
    if (clip_insert_bank_item(bank, type, size) != 0u) {
        reu_free_bank(bank);
        return 0xFFu;
    }
    return 0u;
}
