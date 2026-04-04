/*
 * clipboard.c - Multi-Item Clipboard Implementation
 * Uses REU banks for storage via reu_mgr
 */

#include "clipboard.h"
#include "reu_mgr.h"
#include <string.h>

/* Access item entry in the table (8 bytes per item) */
#define ITEM_ENTRY(idx) (CLIP_TABLE + ((unsigned char)(idx) * 8))

/* Item field offsets */
#define ITEM_BANK    0
#define ITEM_TYPE    1
#define ITEM_SIZE_LO 2
#define ITEM_SIZE_HI 3

/*---------------------------------------------------------------------------
 * Copy data to clipboard (newest at index 0)
 *---------------------------------------------------------------------------*/
unsigned char clip_copy(unsigned char type, const void *data, unsigned int size) {
    unsigned char bank;
    unsigned char count;
    unsigned char *entry;

    /* Allocate a REU bank for this item */
    bank = reu_alloc_bank(REU_CLIPBOARD);
    if (bank == 0xFF) {
        return 0xFF;  /* No free banks */
    }

    count = *CLIP_COUNT;

    /* If table is full, reject (could auto-evict in future) */
    if (count >= CLIP_MAX_ITEMS) {
        reu_free_bank(bank);
        return 0xFF;
    }

    /* Shift existing items down to make room at index 0 */
    if (count > 0) {
        memmove(CLIP_TABLE + 8, CLIP_TABLE, (unsigned int)count * 8);
    }

    /* Write new item at index 0 */
    entry = ITEM_ENTRY(0);
    entry[ITEM_BANK]    = bank;
    entry[ITEM_TYPE]    = type;
    entry[ITEM_SIZE_LO] = (unsigned char)(size & 0xFF);
    entry[ITEM_SIZE_HI] = (unsigned char)(size >> 8);
    entry[4] = 0;
    entry[5] = 0;
    entry[6] = 0;
    entry[7] = 0;

    /* DMA stash data to the allocated bank at offset 0 */
    reu_dma_stash((unsigned int)data, bank, 0, size);

    /* Increment count */
    *CLIP_COUNT = count + 1;

    return 0;
}

/*---------------------------------------------------------------------------
 * Get item count
 *---------------------------------------------------------------------------*/
unsigned char clip_item_count(void) {
    return *CLIP_COUNT;
}

/*---------------------------------------------------------------------------
 * Get item type
 *---------------------------------------------------------------------------*/
unsigned char clip_get_type(unsigned char index) {
    unsigned char *entry;

    if (index >= *CLIP_COUNT) return 0;

    entry = ITEM_ENTRY(index);
    return entry[ITEM_TYPE];
}

/*---------------------------------------------------------------------------
 * Get item size
 *---------------------------------------------------------------------------*/
unsigned int clip_get_size(unsigned char index) {
    unsigned char *entry;

    if (index >= *CLIP_COUNT) return 0;

    entry = ITEM_ENTRY(index);
    return (unsigned int)entry[ITEM_SIZE_LO] | ((unsigned int)entry[ITEM_SIZE_HI] << 8);
}

/*---------------------------------------------------------------------------
 * Paste item at index into buffer
 *---------------------------------------------------------------------------*/
unsigned int clip_paste(unsigned char index, void *buffer, unsigned int maxsize) {
    unsigned char *entry;
    unsigned int size;
    unsigned int copy_size;

    if (index >= *CLIP_COUNT) return 0;

    entry = ITEM_ENTRY(index);
    size = (unsigned int)entry[ITEM_SIZE_LO] | ((unsigned int)entry[ITEM_SIZE_HI] << 8);

    copy_size = (size < maxsize) ? size : maxsize;

    /* DMA fetch from REU bank to buffer */
    reu_dma_fetch((unsigned int)buffer, entry[ITEM_BANK], 0, copy_size);

    return copy_size;
}

/*---------------------------------------------------------------------------
 * Delete item at index
 *---------------------------------------------------------------------------*/
void clip_delete(unsigned char index) {
    unsigned char *entry;
    unsigned char count;

    count = *CLIP_COUNT;
    if (index >= count) return;

    /* Free the REU bank */
    entry = ITEM_ENTRY(index);
    reu_free_bank(entry[ITEM_BANK]);

    /* Shift remaining items up */
    if (index < count - 1) {
        memmove(CLIP_TABLE + (unsigned int)index * 8,
                CLIP_TABLE + (unsigned int)(index + 1) * 8,
                (unsigned int)(count - index - 1) * 8);
    }

    /* Clear the last slot */
    {
        unsigned char *last = ITEM_ENTRY(count - 1);
        memset(last, 0, 8);
    }

    *CLIP_COUNT = count - 1;
}

/*---------------------------------------------------------------------------
 * Clear all clipboard items
 *---------------------------------------------------------------------------*/
void clip_clear(void) {
    unsigned char count;
    unsigned char i;
    unsigned char *entry;

    count = *CLIP_COUNT;

    /* Free all banks */
    for (i = 0; i < count; ++i) {
        entry = ITEM_ENTRY(i);
        reu_free_bank(entry[ITEM_BANK]);
    }

    /* Zero the table and count */
    memset(CLIP_TABLE, 0, CLIP_MAX_ITEMS * 8);
    *CLIP_COUNT = 0;
}
