#ifndef UZ_DEFLATE_INTERNAL_H
#define UZ_DEFLATE_INTERNAL_H

#include "uz_deflate.h"

#define UZ_DEFLATE_HISTORY_OFFSET 0u
#define UZ_DEFLATE_HEAD_OFFSET UZ_DEFLATE_WINDOW_SIZE
#define UZ_DEFLATE_PREV_OFFSET \
    (UZ_DEFLATE_HEAD_OFFSET + 2u * UZ_DEFLATE_HASH_SIZE)
#define UZ_DEFLATE_WINDOW_MASK (UZ_DEFLATE_WINDOW_SIZE - 1u)
#define UZ_DEFLATE_HASH_MASK (UZ_DEFLATE_HASH_SIZE - 1u)
#define UZ_DEFLATE_MAX_CHAIN 64u
#define UZ_DEFLATE_MAX_MATCH 258u

unsigned char uz_deflate_tokenize_block(UzDeflate *state,
                                        unsigned int input_len,
                                        unsigned char reset,
                                        unsigned int *fixed_bits);
void uz_deflate_match_crc_update(UzCrc32 *crc,
                                 const unsigned char *data,
                                 unsigned int length);
unsigned char uz_deflate_emit_block(UzDeflate *state,
                                    unsigned int input_len,
                                    unsigned int fixed_bits,
                                    unsigned char final);

#endif
