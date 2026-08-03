/*
 * clipboard_count.c - Clipboard count helper
 */

#include "clipboard.h"
#include "reu_control_bank.h"

unsigned char clip_item_count(void) {
    unsigned char count;

    count = readyos_bank_read_byte(CLIP_COUNT_OFF);
    return (count <= CLIP_MAX_ITEMS) ? count : 0u;
}
