#ifndef UZ_INFLATE_H
#define UZ_INFLATE_H

#include "uz_crc32.h"
#include "uz_inflate_errors.h"
#include "uz_u32.h"

#define UZ_INFLATE_DICT_SIZE 32768u
#define UZ_INFLATE_LIT_SYMBOLS 288u
#define UZ_INFLATE_DIST_SYMBOLS 32u
#define UZ_INFLATE_CODE_SYMBOLS 19u
#define UZ_INFLATE_LENGTHS 320u

typedef int (*UzInflateRead)(void *context, unsigned char *data,
                             unsigned int length);
typedef unsigned char (*UzInflateWrite)(void *context,
                                        const unsigned char *data,
                                        unsigned int length);

typedef struct {
    UzInflateRead read;
    void *read_context;
    UzInflateWrite write;
    void *write_context;
    unsigned char *input;
    unsigned int input_cap;
    unsigned int input_pos;
    unsigned int input_len;
    unsigned char *output;
    unsigned int output_cap;
    unsigned int output_len;
    unsigned char *dictionary;
    unsigned int dictionary_pos;
    UzU32 compressed_left;
    UzU32 output_size;
    UzU32 output_left;
    UzCrc32 crc;
    unsigned int bit_buffer;
    unsigned char bit_count;
    unsigned char output_bounded;
    unsigned char error;
    unsigned int lit_count[16];
    unsigned int dist_count[16];
    unsigned int code_count[16];
    unsigned int lit_symbol[UZ_INFLATE_LIT_SYMBOLS];
    unsigned int dist_symbol[UZ_INFLATE_DIST_SYMBOLS];
    unsigned int code_symbol[UZ_INFLATE_CODE_SYMBOLS];
    unsigned char lengths[UZ_INFLATE_LENGTHS];
} UzInflate;

void uz_inflate_init(UzInflate *state,
                     UzInflateRead read, void *read_context,
                     UzInflateWrite write, void *write_context,
                     unsigned char *input, unsigned int input_cap,
                     unsigned char *output, unsigned int output_cap,
                     unsigned char *dictionary,
                     const UzU32 *compressed_size,
                     const UzU32 *expected_output_size);
unsigned char uz_inflate_run(UzInflate *state);

#endif
