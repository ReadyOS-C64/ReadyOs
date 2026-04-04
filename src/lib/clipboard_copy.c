/*
 * clipboard_copy.c - Clipboard copy path
 */

#include "clipboard.h"
#include "reu_mgr.h"
#include <string.h>

#define ITEM_ENTRY(idx) (CLIP_TABLE + ((unsigned char)(idx) * 8))
#define ITEM_BANK    0
#define ITEM_TYPE    1
#define ITEM_SIZE_LO 2
#define ITEM_SIZE_HI 3

unsigned char clip_copy(unsigned char type, const void *data, unsigned int size) {
    unsigned char bank;
    unsigned char count;
    unsigned char *entry;

    bank = reu_alloc_bank(REU_CLIPBOARD);
    if (bank == 0xFF) {
        return 0xFF;
    }

    count = *CLIP_COUNT;
    if (count >= CLIP_MAX_ITEMS) {
        reu_free_bank(bank);
        return 0xFF;
    }

    if (count > 0) {
        memmove(CLIP_TABLE + 8, CLIP_TABLE, (unsigned int)count * 8);
    }

    entry = ITEM_ENTRY(0);
    entry[ITEM_BANK]    = bank;
    entry[ITEM_TYPE]    = type;
    entry[ITEM_SIZE_LO] = (unsigned char)(size & 0xFF);
    entry[ITEM_SIZE_HI] = (unsigned char)(size >> 8);
    entry[4] = 0;
    entry[5] = 0;
    entry[6] = 0;
    entry[7] = 0;

    reu_dma_stash((unsigned int)data, bank, 0, size);
    *CLIP_COUNT = count + 1;
    return 0;
}
