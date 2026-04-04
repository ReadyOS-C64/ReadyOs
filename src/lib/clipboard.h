/*
 * clipboard.h - Multi-Item Clipboard for Ready OS
 * Clipboard backed by REU via reu_mgr
 */

#ifndef CLIPBOARD_H
#define CLIPBOARD_H

/* Clipboard limits */
#define CLIP_MAX_ITEMS   16

/* Clipboard data types */
#define CLIP_TYPE_TEXT    1

/* Memory-mapped clipboard state at $C700 area (persists across app switches) */
#define CLIP_COUNT       ((unsigned char*)0xC702)
#define CLIP_TABLE       ((unsigned char*)0xC710)  /* 16 items x 8 bytes = 128 bytes */

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

/* Paste item at index into buffer. Returns actual bytes copied. */
unsigned int clip_paste(unsigned char index, void *buffer, unsigned int maxsize);

/* Delete item at index, shift remaining items, free REU bank */
void clip_delete(unsigned char index);

/* Clear all clipboard items */
void clip_clear(void);

#endif /* CLIPBOARD_H */
