/* clipboard_insert.c - Atomic ReadyOS-bank clipboard record insertion */

#include "clipboard.h"
#include "reu_mgr.h"
#include <string.h>

#define ITEM_BANK    0
#define ITEM_TYPE    1
#define ITEM_SIZE_LO 2
#define ITEM_SIZE_HI 3

unsigned char clip_insert_bank_item(unsigned char bank, unsigned char type,
                                    unsigned int size) {
    unsigned char count;
    unsigned char index;
    unsigned char entry[CLIP_ENTRY_SIZE];

    count = clip_item_count();
    if (count >= CLIP_MAX_ITEMS) return 1u;
    index = count;
    while (index > 0u) {
        readyos_bank_read((unsigned int)(CLIP_TABLE_OFF +
                          ((unsigned int)(index - 1u) * CLIP_ENTRY_SIZE)),
                          entry, CLIP_ENTRY_SIZE);
        readyos_bank_write((unsigned int)(CLIP_TABLE_OFF +
                           ((unsigned int)index * CLIP_ENTRY_SIZE)),
                           entry, CLIP_ENTRY_SIZE);
        --index;
    }
    memset(entry, 0, sizeof(entry));
    entry[ITEM_BANK] = bank;
    entry[ITEM_TYPE] = type;
    entry[ITEM_SIZE_LO] = (unsigned char)size;
    entry[ITEM_SIZE_HI] = (unsigned char)(size >> 8);
    readyos_bank_write(CLIP_TABLE_OFF, entry, CLIP_ENTRY_SIZE);
    /* Commit count last so interrupted copies never expose a partial record. */
    readyos_bank_write_byte(CLIP_COUNT_OFF, (unsigned char)(count + 1u));
    return 0u;
}
