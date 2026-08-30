#ifndef UZ_INFLATE6502_H
#define UZ_INFLATE6502_H

#include "uz_crc32.h"
#include "uz_inflate_errors.h"
#include "uz_u32.h"

#define UZ_INFLATE6502_DICT_START 0x3000u
#define UZ_INFLATE6502_DICT_END   0xB000u

typedef int (*UzInflate6502Read)(void *context, unsigned char *data,
                                 unsigned int length);
typedef unsigned char (*UzInflate6502Write)(void *context,
                                            const unsigned char *data,
                                            unsigned int length);

/* This codec is deliberately single-instance. Its assembly phase has fixed
 * scratch and crosses into C only at input/output block boundaries. */
void uz_inflate6502_init(UzInflate6502Read read, void *read_context,
                        UzInflate6502Write write, void *write_context,
                        unsigned char *input, unsigned int input_cap,
                        unsigned char *output, unsigned int output_cap,
                        const UzU32 *compressed_size,
                        const UzU32 *expected_output_size);
unsigned char uz_inflate6502_run(void);

unsigned char uz_inflate6502_error(void);
const UzU32 *uz_inflate6502_output_size(void);
const UzCrc32 *uz_inflate6502_crc(void);
unsigned int uz_inflate6502_dictionary_pos(void);

/* Assembly-only boundary ABI. The assembly phase saves all cc65 runtime ZP
 * values it owns before either function and restores them afterward. */
unsigned char uz_inflate6502_refill_boundary(void);
unsigned char uz_inflate6502_flush_boundary(void);
unsigned char uz_inflate6502_finish_boundary(void);

extern unsigned char *uzif_input_pointer;
extern unsigned char *uzif_input_end;
extern unsigned char *uzif_output_buffer;
extern unsigned int uzif_output_count;
extern unsigned int uzif_output_cap;
extern UzU32 uzif_output_count32;
extern UzU32 uzif_output_left;
extern unsigned char uzif_output_bounded;
extern unsigned char uzif_error_code;
extern unsigned char uzif_ready;
extern unsigned int uzif_dictionary_position;

#endif
