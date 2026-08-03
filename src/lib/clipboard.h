/*
 * clipboard.h - Multi-Item Clipboard for Ready OS
 * Clipboard backed by REU via reu_mgr
 */

#ifndef CLIPBOARD_H
#define CLIPBOARD_H

#include "reu_control_bank.h"

/* Clipboard limits */
#define CLIP_MAX_ITEMS   16

/* Clipboard data types */
#define CLIP_TYPE_TEXT    1

/* Authoritative clipboard metadata is in the ReadyOS bank. */
#define CLIP_COUNT_OFF       REUCB_CLIPBOARD_OFF
#define CLIP_TABLE_OFF       (REUCB_CLIPBOARD_OFF + 0x10u)
#define CLIP_ENTRY_SIZE      8u

/* Per-item table layout (8 bytes each):
 *   [0] bank     - REU bank holding this item's data
 *   [1] type     - CLIP_TYPE_TEXT etc.
 *   [2] size_lo  - data size low byte
 *   [3] size_hi  - data size high byte
 *   [4-7] reserved
 */

/* Copy data to clipboard (newest item at index 0).
 * Returns 0 on success, 0xFF if table full or alloc failed. */
unsigned char clip_copy(unsigned char type, const void *data, unsigned int size);

/* Get number of items on clipboard */
unsigned char clip_item_count(void);

/* Get type of item at index */
unsigned char clip_get_type(unsigned char index);

/* Get size of item at index */
unsigned int clip_get_size(unsigned char index);

/* Get the physical payload bank for an item, or $FF when out of range. */
unsigned char clip_get_bank(unsigned char index);

/* Commit an already-populated payload bank as the newest clipboard item. */
unsigned char clip_insert_bank_item(unsigned char bank, unsigned char type,
                                    unsigned int size);

/* Paste item at index into buffer. Returns actual bytes copied. */
unsigned int clip_paste(unsigned char index, void *buffer, unsigned int maxsize);

/* Delete item at index, shift remaining items, free REU bank */
void clip_delete(unsigned char index);

/* Clear all clipboard items */
void clip_clear(void);

#endif /* CLIPBOARD_H */
