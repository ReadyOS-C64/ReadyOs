#ifndef UZ_DEFLATE_H
#define UZ_DEFLATE_H

#include "uz_crc32.h"
#include "uz_u32.h"

#define UZ_DEFLATE_WINDOW_SIZE   8192u
#define UZ_DEFLATE_HASH_SIZE     1024u
#define UZ_DEFLATE_BLOCK_SIZE    2048u
#define UZ_DEFLATE_TOKEN_MAX     4096u
#define UZ_DEFLATE_WORKSPACE_SIZE 26624u

#define UZ_DEFLATE_OK             0u
#define UZ_DEFLATE_ERR_STATE      1u
#define UZ_DEFLATE_ERR_INPUT      2u
#define UZ_DEFLATE_ERR_OUTPUT     3u
#define UZ_DEFLATE_ERR_TOKEN      4u
#define UZ_DEFLATE_ERR_INTERNAL   5u

#define UZ_DEFLATE_PHASE_MATCH    1u
#define UZ_DEFLATE_PHASE_EMIT     2u

typedef int (*UzDeflateRead)(void *context, unsigned char *data,
                             unsigned int length);
typedef unsigned char (*UzDeflateWrite)(void *context,
                                        const unsigned char *data,
                                        unsigned int length);
typedef unsigned char (*UzDeflateTokenStore)(void *context,
                                             unsigned int offset,
                                             const unsigned char *data,
                                             unsigned int length);
typedef unsigned char (*UzDeflateTokenLoad)(void *context,
                                            unsigned int offset,
                                            unsigned char *data,
                                            unsigned int length);
typedef unsigned char (*UzDeflatePhaseLoad)(void *context,
                                            unsigned char phase);

/* The phase callback may replace the $B000 job window. uz_deflate_run, its
 * state, and all stream/token callbacks must therefore execute/live outside
 * that window on the C64. A null callback is the host single-image form. */

typedef struct {
    UzDeflateRead read;
    void *read_context;
    UzDeflateWrite write;
    void *write_context;
    UzDeflateTokenStore token_store;
    UzDeflateTokenLoad token_load;
    void *token_context;
    UzDeflatePhaseLoad phase_load;
    void *phase_context;
    unsigned char *workspace;
    unsigned char *input;
    unsigned int input_cap;
    unsigned char *output;
    unsigned int output_cap;
    unsigned char *token_stage;
    unsigned int token_stage_cap;
    UzU32 input_left;
    UzU32 input_size;
    UzU32 output_size;
    UzCrc32 crc;
    unsigned int window_pos;
    unsigned int window_filled;
    unsigned int token_size;
    unsigned int token_stage_len;
    unsigned int token_read_offset;
    unsigned int token_read_pos;
    unsigned int output_len;
    unsigned int fixed_blocks;
    unsigned int stored_blocks;
    unsigned char bit_byte;
    unsigned char bit_count;
    unsigned char error;
    unsigned char ready;
} UzDeflate;

void uz_deflate_init(UzDeflate *state,
                     UzDeflateRead read, void *read_context,
                     UzDeflateWrite write, void *write_context,
                     UzDeflateTokenStore token_store,
                     UzDeflateTokenLoad token_load, void *token_context,
                     UzDeflatePhaseLoad phase_load, void *phase_context,
                     unsigned char *workspace,
                     unsigned char *input, unsigned int input_cap,
                     unsigned char *output, unsigned int output_cap,
                     unsigned char *token_stage,
                     unsigned int token_stage_cap,
                     const UzU32 *input_size);
unsigned char uz_deflate_run(UzDeflate *state);

#endif
