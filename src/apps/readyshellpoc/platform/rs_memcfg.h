#ifndef RS_MEMCFG_H
#define RS_MEMCFG_H

/* Ensure RAM is visible under BASIC ROM while calling overlay code at $A000-$BFFF. */
void rs_memcfg_push_ram_under_basic(void);
void rs_memcfg_pop(void);

#endif
