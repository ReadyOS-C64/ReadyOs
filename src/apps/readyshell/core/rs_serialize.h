#ifndef RS_SERIALIZE_H
#define RS_SERIALIZE_H

#include "rs_value.h"

int rs_serialize_value(const RSValue* value,
                       unsigned char* dst,
                       unsigned short max,
                       unsigned short* out_len);

int rs_deserialize_value(const unsigned char* src,
                         unsigned short len,
                         RSValue* out_value,
                         unsigned short* out_used);

int rs_serialize_file_payload(const RSValue* value,
                              unsigned char* dst,
                              unsigned short max,
                              unsigned short* out_len);

int rs_deserialize_file_payload(const unsigned char* src,
                                unsigned short len,
                                RSValue* out_value);

#endif
