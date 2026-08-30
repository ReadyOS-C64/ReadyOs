#include "uz_deflate_internal.h"

#ifdef __CC65__
#pragma code-name(push, "DEFLATE_EMIT_CODE")
#pragma rodata-name(push, "DEFLATE_EMIT_RODATA")
#endif

static const unsigned int length_base[29] = {
    3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 13u, 15u, 17u, 19u, 23u,
    27u, 31u, 35u, 43u, 51u, 59u, 67u, 83u, 99u, 115u, 131u, 163u,
    195u, 227u, 258u
};

static const unsigned char length_extra[29] = {
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 2u, 2u, 2u,
    2u, 3u, 3u, 3u, 3u, 4u, 4u, 4u, 4u, 5u, 5u, 5u, 5u, 0u
};

static const unsigned int distance_base[26] = {
    1u, 2u, 3u, 4u, 5u, 7u, 9u, 13u, 17u, 25u, 33u, 49u, 65u, 97u,
    129u, 193u, 257u, 385u, 513u, 769u, 1025u, 1537u, 2049u, 3073u,
    4097u, 6145u
};

static const unsigned char distance_extra[26] = {
    0u, 0u, 0u, 0u, 1u, 1u, 2u, 2u, 3u, 3u, 4u, 4u, 5u, 5u, 6u,
    6u, 7u, 7u, 8u, 8u, 9u, 9u, 10u, 10u, 11u, 11u
};

static unsigned char fail(UzDeflate *state, unsigned char error) {
    state->error = error;
    return 0u;
}

static unsigned char flush_output(UzDeflate *state) {
    if (state->output_len == 0u) return 1u;
    if (!state->write(state->write_context, state->output,
                      state->output_len))
        return fail(state, UZ_DEFLATE_ERR_OUTPUT);
    uz_u32_add_u16(&state->output_size, state->output_len);
    state->output_len = 0u;
    return 1u;
}

static unsigned char output_byte(UzDeflate *state, unsigned char value) {
    state->output[state->output_len++] = value;
    if (state->output_len == state->output_cap) return flush_output(state);
    return 1u;
}

static unsigned char output_data(UzDeflate *state,
                                 const unsigned char *data,
                                 unsigned int length) {
    while (length != 0u) {
        if (!output_byte(state, *data++)) return 0u;
        --length;
    }
    return 1u;
}

static unsigned char put_bits(UzDeflate *state, unsigned int value,
                              unsigned char count) {
    unsigned char take;
    unsigned char room;
    unsigned int mask;

    while (count != 0u) {
        room = (unsigned char)(8u - state->bit_count);
        take = count < room ? count : room;
        mask = (unsigned int)((1u << take) - 1u);
        state->bit_byte |= (unsigned char)((value & mask) << state->bit_count);
        value >>= take;
        state->bit_count = (unsigned char)(state->bit_count + take);
        count = (unsigned char)(count - take);
        if (state->bit_count == 8u) {
            if (!output_byte(state, state->bit_byte)) return 0u;
            state->bit_byte = 0u;
            state->bit_count = 0u;
        }
    }
    return 1u;
}

static unsigned char align_bits(UzDeflate *state) {
    if (state->bit_count != 0u) {
        if (!output_byte(state, state->bit_byte)) return 0u;
        state->bit_byte = 0u;
        state->bit_count = 0u;
    }
    return 1u;
}

static unsigned int reverse_bits(unsigned int value, unsigned char count) {
    unsigned int result;

    result = 0u;
    while (count-- != 0u) {
        result = (unsigned int)((result << 1u) | (value & 1u));
        value >>= 1u;
    }
    return result;
}

static unsigned char fixed_symbol_bits(unsigned int symbol) {
    if (symbol <= 143u) return 8u;
    if (symbol <= 255u) return 9u;
    if (symbol <= 279u) return 7u;
    return 8u;
}

static unsigned char emit_fixed_symbol(UzDeflate *state,
                                       unsigned int symbol) {
    unsigned int code;
    unsigned char count;

    count = fixed_symbol_bits(symbol);
    if (symbol <= 143u) code = (unsigned int)(symbol + 0x30u);
    else if (symbol <= 255u) code = (unsigned int)(symbol - 144u + 0x190u);
    else if (symbol <= 279u) code = (unsigned int)(symbol - 256u);
    else code = (unsigned int)(symbol - 280u + 0xC0u);
    return put_bits(state, reverse_bits(code, count), count);
}

static unsigned char length_code(unsigned int length, unsigned char *extra,
                                 unsigned int *extra_value) {
    unsigned char index;
    unsigned int maximum;

    if (length == 258u) {
        *extra = 0u;
        *extra_value = 0u;
        return 28u;
    }
    for (index = 0u; index < 28u; ++index) {
        maximum = length_base[index];
        if (length_extra[index] != 0u)
            maximum = (unsigned int)(maximum +
                ((1u << length_extra[index]) - 1u));
        if (length <= maximum) {
            *extra = length_extra[index];
            *extra_value = (unsigned int)(length - length_base[index]);
            return index;
        }
    }
    return 0xFFu;
}

static unsigned char distance_code(unsigned int distance,
                                   unsigned char *extra,
                                   unsigned int *extra_value) {
    unsigned char index;
    unsigned int maximum;

    for (index = 0u; index < 26u; ++index) {
        maximum = distance_base[index];
        if (distance_extra[index] != 0u)
            maximum = (unsigned int)(maximum +
                ((1u << distance_extra[index]) - 1u));
        if (distance <= maximum) {
            *extra = distance_extra[index];
            *extra_value = (unsigned int)(distance - distance_base[index]);
            return index;
        }
    }
    return 0xFFu;
}

static unsigned char token_read_byte(UzDeflate *state,
                                     unsigned char *value) {
    unsigned int want;

    if (state->token_read_pos == state->token_stage_len) {
        if (state->token_read_offset == state->token_size)
            return fail(state, UZ_DEFLATE_ERR_TOKEN);
        want = (unsigned int)(state->token_size - state->token_read_offset);
        if (want > state->token_stage_cap) want = state->token_stage_cap;
        if (!state->token_load(state->token_context,
                               state->token_read_offset,
                               state->token_stage, want))
            return fail(state, UZ_DEFLATE_ERR_TOKEN);
        state->token_read_offset = (unsigned int)(state->token_read_offset +
                                                  want);
        state->token_stage_len = want;
        state->token_read_pos = 0u;
    }
    *value = state->token_stage[state->token_read_pos++];
    return 1u;
}

static unsigned int stored_cost(const UzDeflate *state,
                                unsigned int input_len) {
    unsigned int bits;
    unsigned char after_header;
    unsigned char padding;

    after_header = (unsigned char)((state->bit_count + 3u) & 7u);
    padding = after_header == 0u ? 0u : (unsigned char)(8u - after_header);
    bits = (unsigned int)(3u + padding + 32u);
    bits = (unsigned int)(bits + 8u * input_len);
    return bits;
}

static unsigned char emit_match(UzDeflate *state, unsigned int length,
                                unsigned int distance) {
    unsigned char index;
    unsigned char extra;
    unsigned int extra_value;

    index = length_code(length, &extra, &extra_value);
    if (index == 0xFFu)
        return fail(state, UZ_DEFLATE_ERR_INTERNAL);
    if (!emit_fixed_symbol(state, (unsigned int)(257u + index)) ||
        !put_bits(state, extra_value, extra)) return 0u;
    index = distance_code(distance, &extra, &extra_value);
    if (index == 0xFFu)
        return fail(state, UZ_DEFLATE_ERR_INTERNAL);
    if (!put_bits(state, reverse_bits(index, 5u), 5u) ||
        !put_bits(state, extra_value, extra)) return 0u;
    return 1u;
}

static unsigned char emit_fixed(UzDeflate *state, unsigned char final) {
    unsigned char low;
    unsigned char high;
    unsigned char distance_low;
    unsigned char distance_high;
    unsigned int length;
    unsigned int distance;

    if (!put_bits(state, (unsigned int)(2u | final), 3u)) return 0u;
    state->token_read_offset = 0u;
    state->token_read_pos = 0u;
    state->token_stage_len = 0u;
    while (state->token_read_offset < state->token_size ||
           state->token_read_pos < state->token_stage_len) {
        if (!token_read_byte(state, &low) ||
            !token_read_byte(state, &high)) return 0u;
        if (high == 0u) {
            if (!emit_fixed_symbol(state, low)) return 0u;
        } else if (high == 1u) {
            if (!token_read_byte(state, &distance_low) ||
                !token_read_byte(state, &distance_high)) return 0u;
            length = (unsigned int)(low + 3u);
            distance = (unsigned int)(distance_low |
                       ((unsigned int)distance_high << 8u));
            if (!emit_match(state, length, distance)) return 0u;
        } else {
            return fail(state, UZ_DEFLATE_ERR_TOKEN);
        }
    }
    if (!emit_fixed_symbol(state, 256u)) return 0u;
    ++state->fixed_blocks;
    return 1u;
}

static unsigned char emit_stored(UzDeflate *state, unsigned char final,
                                 unsigned int input_len) {
    unsigned int inverse;

    if (!put_bits(state, final, 3u) || !align_bits(state)) return 0u;
    inverse = (unsigned int)(input_len ^ 0xFFFFu);
    if (!output_byte(state, (unsigned char)input_len) ||
        !output_byte(state, (unsigned char)(input_len >> 8u)) ||
        !output_byte(state, (unsigned char)inverse) ||
        !output_byte(state, (unsigned char)(inverse >> 8u)) ||
        !output_data(state, state->input, input_len)) return 0u;
    ++state->stored_blocks;
    return 1u;
}

unsigned char uz_deflate_emit_block(UzDeflate *state,
                                    unsigned int input_len,
                                    unsigned int fixed_bits,
                                    unsigned char final) {
    if (fixed_bits <= stored_cost(state, input_len)) {
        if (!emit_fixed(state, final)) return 0u;
    } else if (!emit_stored(state, final, input_len)) {
        return 0u;
    }
    if (final && (!align_bits(state) || !flush_output(state))) return 0u;
    return 1u;
}

#ifdef __CC65__
#pragma rodata-name(pop)
#pragma code-name(pop)
#endif
