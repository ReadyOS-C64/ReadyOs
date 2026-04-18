#ifndef RS_PIPE_H
#define RS_PIPE_H

#include "rs_value.h"

int rs_pipe_expand_value(const RSValue* value, RSValue** out_items, unsigned short* out_count);
int rs_pipe_append_item(RSValue** items, unsigned short* count, const RSValue* item);
void rs_pipe_free_items(RSValue* items, unsigned short count);

#endif
