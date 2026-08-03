/*
 * clipboard_admin.c - Clipboard query/management helpers
 */

#include "clipboard.h"
#include "reu_mgr.h"
#include "reu_control_bank.h"
#include <string.h>

/* Item field offsets */
#define ITEM_BANK    0
#define ITEM_TYPE    1
#define ITEM_SIZE_LO 2
#define ITEM_SIZE_HI 3

unsigned char clip_get_type(unsigned char index) {
    unsigned char entry[CLIP_ENTRY_SIZE];

    if (index >= clip_item_count()) return 0;
    readyos_bank_read((unsigned int)(CLIP_TABLE_OFF +
                      ((unsigned int)index * CLIP_ENTRY_SIZE)),
                      entry, CLIP_ENTRY_SIZE);
    return entry[ITEM_TYPE];
}

unsigned int clip_get_size(unsigned char index) {
    unsigned char entry[CLIP_ENTRY_SIZE];

    if (index >= clip_item_count()) return 0;
    readyos_bank_read((unsigned int)(CLIP_TABLE_OFF +
                      ((unsigned int)index * CLIP_ENTRY_SIZE)),
                      entry, CLIP_ENTRY_SIZE);
    return (unsigned int)entry[ITEM_SIZE_LO] | ((unsigned int)entry[ITEM_SIZE_HI] << 8);
}

unsigned char clip_get_bank(unsigned char index) {
    unsigned char entry[CLIP_ENTRY_SIZE];

    if (index >= clip_item_count()) return 0xFFu;
    readyos_bank_read((unsigned int)(CLIP_TABLE_OFF +
                      ((unsigned int)index * CLIP_ENTRY_SIZE)),
                      entry, CLIP_ENTRY_SIZE);
    return entry[ITEM_BANK];
}

void clip_delete(unsigned char index) {
    unsigned char entry[CLIP_ENTRY_SIZE];
    unsigned char count;

    count = clip_item_count();
    if (index >= count) return;

    readyos_bank_read((unsigned int)(CLIP_TABLE_OFF +
                      ((unsigned int)index * CLIP_ENTRY_SIZE)),
                      entry, CLIP_ENTRY_SIZE);
    reu_free_bank(entry[ITEM_BANK]);

    while ((unsigned char)(index + 1u) < count) {
        readyos_bank_read((unsigned int)(CLIP_TABLE_OFF +
                          ((unsigned int)(index + 1u) * CLIP_ENTRY_SIZE)),
                          entry, CLIP_ENTRY_SIZE);
        readyos_bank_write((unsigned int)(CLIP_TABLE_OFF +
                           ((unsigned int)index * CLIP_ENTRY_SIZE)),
                           entry, CLIP_ENTRY_SIZE);
        ++index;
    }
    memset(entry, 0, sizeof(entry));
    readyos_bank_write((unsigned int)(CLIP_TABLE_OFF +
                       ((unsigned int)(count - 1u) * CLIP_ENTRY_SIZE)),
                       entry, CLIP_ENTRY_SIZE);
    readyos_bank_write_byte(CLIP_COUNT_OFF, (unsigned char)(count - 1u));
}

void clip_clear(void) {
    unsigned char count;
    unsigned char i;
    unsigned char entry[CLIP_ENTRY_SIZE];

    count = clip_item_count();

    for (i = 0; i < count; ++i) {
        readyos_bank_read((unsigned int)(CLIP_TABLE_OFF +
                          ((unsigned int)i * CLIP_ENTRY_SIZE)),
                          entry, CLIP_ENTRY_SIZE);
        reu_free_bank(entry[ITEM_BANK]);
    }

    memset(entry, 0, sizeof(entry));
    for (i = 0u; i < CLIP_MAX_ITEMS; ++i) {
        readyos_bank_write((unsigned int)(CLIP_TABLE_OFF +
                           ((unsigned int)i * CLIP_ENTRY_SIZE)),
                           entry, CLIP_ENTRY_SIZE);
    }
    readyos_bank_write_byte(CLIP_COUNT_OFF, 0u);
}
