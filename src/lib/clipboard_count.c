/*
 * clipboard_count.c - Clipboard count helper
 */

#include "clipboard.h"

unsigned char clip_item_count(void) {
    return *CLIP_COUNT;
}
