/*
 * clipboard_admin.c - Clipboard query/management helpers
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

unsigned char clip_get_type(unsigned char index) {
    unsigned char *entry;

    if (index >= *CLIP_COUNT) return 0;
    entry = ITEM_ENTRY(index);
    return entry[ITEM_TYPE];
}

unsigned int clip_get_size(unsigned char index) {
    unsigned char *entry;

    if (index >= *CLIP_COUNT) return 0;
    entry = ITEM_ENTRY(index);
    return (unsigned int)entry[ITEM_SIZE_LO] | ((unsigned int)entry[ITEM_SIZE_HI] << 8);
}

void clip_delete(unsigned char index) {
    unsigned char *entry;
    unsigned char count;

    count = *CLIP_COUNT;
    if (index >= count) return;

    entry = ITEM_ENTRY(index);
    reu_free_bank(entry[ITEM_BANK]);

    if (index < count - 1) {
        memmove(CLIP_TABLE + (unsigned int)index * 8,
                CLIP_TABLE + (unsigned int)(index + 1) * 8,
                (unsigned int)(count - index - 1) * 8);
    }

    {
        unsigned char *last = ITEM_ENTRY(count - 1);
        memset(last, 0, 8);
    }

    *CLIP_COUNT = count - 1;
}

void clip_clear(void) {
    unsigned char count;
    unsigned char i;
    unsigned char *entry;

    count = *CLIP_COUNT;

    for (i = 0; i < count; ++i) {
        entry = ITEM_ENTRY(i);
        reu_free_bank(entry[ITEM_BANK]);
    }

    memset(CLIP_TABLE, 0, CLIP_MAX_ITEMS * 8);
    *CLIP_COUNT = 0;
}
