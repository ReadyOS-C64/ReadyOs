/*
 * clipboard_paste.c - Clipboard paste path
 */

#include "clipboard.h"
#include "reu_mgr.h"
#include "reu_control_bank.h"

#define ITEM_BANK    0
#define ITEM_SIZE_LO 2
#define ITEM_SIZE_HI 3

unsigned int clip_paste(unsigned char index, void *buffer, unsigned int maxsize) {
    unsigned char entry[CLIP_ENTRY_SIZE];
    unsigned int size;
    unsigned int copy_size;

    if (index >= clip_item_count()) {
        return 0;
    }

    readyos_bank_read((unsigned int)(CLIP_TABLE_OFF +
                      ((unsigned int)index * CLIP_ENTRY_SIZE)),
                      entry, CLIP_ENTRY_SIZE);
    size = (unsigned int)entry[ITEM_SIZE_LO] | ((unsigned int)entry[ITEM_SIZE_HI] << 8);
    copy_size = (size < maxsize) ? size : maxsize;

    reu_dma_fetch((unsigned int)buffer, entry[ITEM_BANK], 0, copy_size);
    return copy_size;
}
