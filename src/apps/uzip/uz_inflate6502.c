#include "uz_inflate6502.h"

#ifdef __CC65__
/* The compact inflater is an overlay/job component. Keeping its C boundary
 * code and state with the assembly core leaves the low application window
 * available to the transport and probe/UI shell. */
#ifdef UZIP_READYOS_APP
#pragma code-name("INFLATE_CODE")
#pragma bss-name("INFLATE_BSS")
#else
#pragma code-name("JOB_CODE")
#pragma bss-name("JOB_BSS")
#endif
#endif

static UzInflate6502Read read_callback;
static void *read_context_value;
static UzInflate6502Write write_callback;
static void *write_context_value;
static unsigned char *input_buffer;
static unsigned int input_capacity;
static UzU32 compressed_left;
static UzCrc32 output_crc;

/* This implementation must execute while the inflater owns `$B000`. The
 * Store phase has a different CRC copy at the same run address, so referencing
 * the shared symbol here would jump into overwritten bytes in production. */
static const unsigned char inflate_crc_nibble[16][4] = {
    {0x00u, 0x00u, 0x00u, 0x00u}, {0x64u, 0x10u, 0xB7u, 0x1Du},
    {0xC8u, 0x20u, 0x6Eu, 0x3Bu}, {0xACu, 0x30u, 0xD9u, 0x26u},
    {0x90u, 0x41u, 0xDCu, 0x76u}, {0xF4u, 0x51u, 0x6Bu, 0x6Bu},
    {0x58u, 0x61u, 0xB2u, 0x4Du}, {0x3Cu, 0x71u, 0x05u, 0x50u},
    {0x20u, 0x83u, 0xB8u, 0xEDu}, {0x44u, 0x93u, 0x0Fu, 0xF0u},
    {0xE8u, 0xA3u, 0xD6u, 0xD6u}, {0x8Cu, 0xB3u, 0x61u, 0xCBu},
    {0xB0u, 0xC2u, 0x64u, 0x9Bu}, {0xD4u, 0xD2u, 0xD3u, 0x86u},
    {0x78u, 0xE2u, 0x0Au, 0xA0u}, {0x1Cu, 0xF2u, 0xBDu, 0xBDu}
};

static void inflate_crc_init(UzCrc32 *crc) {
    crc->byte[0] = 0xFFu;
    crc->byte[1] = 0xFFu;
    crc->byte[2] = 0xFFu;
    crc->byte[3] = 0xFFu;
}

static void inflate_crc_update(UzCrc32 *crc, const unsigned char *data,
                               unsigned int length) {
    unsigned char half;
    unsigned char index;

    while (length-- != 0u) {
        crc->byte[0] ^= *data++;
        for (half = 0u; half < 2u; ++half) {
            index = (unsigned char)(crc->byte[0] & 0x0Fu);
            crc->byte[0] = (unsigned char)(
                (crc->byte[0] >> 4u) | (crc->byte[1] << 4u));
            crc->byte[1] = (unsigned char)(
                (crc->byte[1] >> 4u) | (crc->byte[2] << 4u));
            crc->byte[2] = (unsigned char)(
                (crc->byte[2] >> 4u) | (crc->byte[3] << 4u));
            crc->byte[3] >>= 4u;
            crc->byte[0] ^= inflate_crc_nibble[index][0];
            crc->byte[1] ^= inflate_crc_nibble[index][1];
            crc->byte[2] ^= inflate_crc_nibble[index][2];
            crc->byte[3] ^= inflate_crc_nibble[index][3];
        }
    }
}

static void inflate_crc_finish(UzCrc32 *crc) {
    crc->byte[0] ^= 0xFFu;
    crc->byte[1] ^= 0xFFu;
    crc->byte[2] ^= 0xFFu;
    crc->byte[3] ^= 0xFFu;
}

/* These names form the fixed C/assembly boundary. Keep them as individual
 * objects so ca65 never depends on a compiler-specific struct layout. */
unsigned char *uzif_input_pointer;
unsigned char *uzif_input_end;
unsigned char *uzif_output_buffer;
unsigned int uzif_output_count;
unsigned int uzif_output_cap;
UzU32 uzif_output_count32;
UzU32 uzif_output_left;
unsigned char uzif_output_bounded;
unsigned char uzif_error_code;
unsigned char uzif_ready;
unsigned int uzif_dictionary_position;

static unsigned int input_want(void) {
    if (compressed_left.hi != 0u || compressed_left.lo > input_capacity)
        return input_capacity;
    return compressed_left.lo;
}

void uz_inflate6502_init(UzInflate6502Read read, void *read_context,
                        UzInflate6502Write write, void *write_context,
                        unsigned char *input, unsigned int input_cap,
                        unsigned char *output, unsigned int output_cap,
                        const UzU32 *compressed_size,
                        const UzU32 *expected_output_size) {
    read_callback = read;
    read_context_value = read_context;
    write_callback = write;
    write_context_value = write_context;
    input_buffer = input;
    input_capacity = input_cap;
    uzif_output_buffer = output;
    uzif_output_cap = output_cap;
    uzif_input_pointer = input;
    uzif_input_end = input;
    uzif_output_count = 0u;
    uz_u32_zero(&uzif_output_count32);
    uz_u32_zero(&uzif_output_left);
    uz_u32_zero(&compressed_left);
    if (compressed_size != 0) compressed_left = *compressed_size;
    uzif_output_bounded = (unsigned char)(expected_output_size != 0);
    if (expected_output_size != 0) uzif_output_left = *expected_output_size;
    uzif_error_code = UZ_INFLATE_OK;
    uzif_ready = (unsigned char)(read != 0 && input != 0 && input_cap != 0u &&
                                 output != 0 && output_cap != 0u &&
                                 output_cap <= 508u &&
                                 compressed_size != 0);
    if (!uzif_ready) uzif_error_code = UZ_INFLATE_STATE;
    uzif_dictionary_position = 0u;
    inflate_crc_init(&output_crc);
}

unsigned char uz_inflate6502_refill_boundary(void) {
    unsigned int want;
    int got;

    want = input_want();
    if (want == 0u) {
        uzif_error_code = UZ_INFLATE_TRUNCATED;
        return 0u;
    }
    got = read_callback(read_context_value, input_buffer, want);
    if (got <= 0 || (unsigned int)got > want) {
        uzif_error_code = UZ_INFLATE_IO;
        return 0u;
    }
    uz_u32_sub_u16(&compressed_left, (unsigned int)got);
    uzif_input_pointer = input_buffer;
    uzif_input_end = input_buffer + (unsigned int)got;
    return 1u;
}

unsigned char uz_inflate6502_flush_boundary(void) {
    if (uzif_output_count == 0u) return 1u;
    inflate_crc_update(&output_crc, uzif_output_buffer, uzif_output_count);
    if (write_callback != 0 &&
        !write_callback(write_context_value, uzif_output_buffer,
                        uzif_output_count)) {
        uzif_error_code = UZ_INFLATE_IO;
        return 0u;
    }
    uzif_output_count = 0u;
    return 1u;
}

unsigned char uz_inflate6502_finish_boundary(void) {
    if (!uz_inflate6502_flush_boundary()) return 0u;
    if (compressed_left.hi != 0u || compressed_left.lo != 0u ||
        uzif_input_pointer != uzif_input_end) {
        uzif_error_code = UZ_INFLATE_TRAILING;
        return 0u;
    }
    if (uzif_output_bounded &&
        (uzif_output_left.hi != 0u || uzif_output_left.lo != 0u)) {
        uzif_error_code = UZ_INFLATE_OUTPUT_SIZE;
        return 0u;
    }
    inflate_crc_finish(&output_crc);
    uzif_error_code = UZ_INFLATE_OK;
    return 1u;
}

unsigned char uz_inflate6502_error(void) {
    return uzif_error_code;
}

const UzU32 *uz_inflate6502_output_size(void) {
    return &uzif_output_count32;
}

const UzCrc32 *uz_inflate6502_crc(void) {
    return &output_crc;
}

unsigned int uz_inflate6502_dictionary_pos(void) {
    return uzif_dictionary_position;
}
