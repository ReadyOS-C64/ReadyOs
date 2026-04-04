/*
 * clipboard_paste.c - Clipboard paste path
 */

#include "clipboard.h"
#include "reu_mgr.h"

#define ITEM_ENTRY(idx) (CLIP_TABLE + ((unsigned char)(idx) * 8))
#define ITEM_BANK    0
#define ITEM_SIZE_LO 2
#define ITEM_SIZE_HI 3

unsigned int clip_paste(unsigned char index, void *buffer, unsigned int maxsize) {
    unsigned char *entry;
    unsigned int size;
    unsigned int copy_size;

    if (index >= *CLIP_COUNT) {
        return 0;
    }

    entry = ITEM_ENTRY(index);
    size = (unsigned int)entry[ITEM_SIZE_LO] | ((unsigned int)entry[ITEM_SIZE_HI] << 8);
    copy_size = (size < maxsize) ? size : maxsize;

    reu_dma_fetch((unsigned int)buffer, entry[ITEM_BANK], 0, copy_size);
    return copy_size;
}
