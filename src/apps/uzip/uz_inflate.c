#include "uz_inflate.h"

#include <string.h>

static const unsigned int length_base[29] = {
    3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u, 13u, 15u, 17u, 19u, 23u,
    27u, 31u, 35u, 43u, 51u, 59u, 67u, 83u, 99u, 115u, 131u, 163u,
    195u, 227u, 258u
};

static const unsigned char length_extra[29] = {
    0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 1u, 1u, 1u, 1u, 2u, 2u, 2u,
    2u, 3u, 3u, 3u, 3u, 4u, 4u, 4u, 4u, 5u, 5u, 5u, 5u, 0u
};

static const unsigned int distance_base[30] = {
    1u, 2u, 3u, 4u, 5u, 7u, 9u, 13u, 17u, 25u, 33u, 49u, 65u, 97u,
    129u, 193u, 257u, 385u, 513u, 769u, 1025u, 1537u, 2049u, 3073u,
    4097u, 6145u, 8193u, 12289u, 16385u, 24577u
};

static const unsigned char distance_extra[30] = {
    0u, 0u, 0u, 0u, 1u, 1u, 2u, 2u, 3u, 3u, 4u, 4u, 5u, 5u, 6u,
    6u, 7u, 7u, 8u, 8u, 9u, 9u, 10u, 10u, 11u, 11u, 12u, 12u,
    13u, 13u
};

static const unsigned char code_order[19] = {
    16u, 17u, 18u, 0u, 8u, 7u, 9u, 6u, 10u, 5u,
    11u, 4u, 12u, 3u, 13u, 2u, 14u, 1u, 15u
};

static unsigned char fail(UzInflate *state, unsigned char error) {
    state->error = error;
    return 0u;
}

static unsigned int input_want(const UzU32 *left, unsigned int cap) {
    if (left->hi != 0u || left->lo > cap) return cap;
    return left->lo;
}

static unsigned char refill(UzInflate *state) {
    unsigned int want;
    int got;

    if (state->input_pos < state->input_len) return 1u;
    want = input_want(&state->compressed_left, state->input_cap);
    if (want == 0u) return fail(state, UZ_INFLATE_TRUNCATED);
    got = state->read(state->read_context, state->input, want);
    if (got <= 0 || (unsigned int)got > want) {
        return fail(state, UZ_INFLATE_IO);
    }
    state->input_pos = 0u;
    state->input_len = (unsigned int)got;
    uz_u32_sub_u16(&state->compressed_left, (unsigned int)got);
    return 1u;
}

static unsigned char source_byte(UzInflate *state, unsigned char *value) {
    if (!refill(state)) return 0u;
    *value = state->input[state->input_pos++];
    return 1u;
}

static unsigned char bits(UzInflate *state, unsigned char count,
                          unsigned int *value) {
    unsigned char next;
    unsigned int mask;

    while (state->bit_count < count) {
        if (!source_byte(state, &next)) return 0u;
        state->bit_buffer |= (unsigned int)((unsigned int)next <<
                                            state->bit_count);
        state->bit_count = (unsigned char)(state->bit_count + 8u);
    }
    if (count == 0u) {
        *value = 0u;
        return 1u;
    }
    mask = (unsigned int)((1u << count) - 1u);
    *value = (unsigned int)(state->bit_buffer & mask);
    state->bit_buffer >>= count;
    state->bit_count = (unsigned char)(state->bit_count - count);
    return 1u;
}

static unsigned char flush_output(UzInflate *state) {
    if (state->output_len == 0u) return 1u;
    uz_crc32_update(&state->crc, state->output, state->output_len);
    if (state->write != 0 &&
        !state->write(state->write_context, state->output,
                      state->output_len)) {
        return fail(state, UZ_INFLATE_IO);
    }
    state->output_len = 0u;
    return 1u;
}

static unsigned char emit_byte(UzInflate *state, unsigned char value) {
    if (state->output_bounded && state->output_left.hi == 0u &&
        state->output_left.lo == 0u)
        return fail(state, UZ_INFLATE_OUTPUT_SIZE);
    state->dictionary[state->dictionary_pos++] = value;
    if (state->dictionary_pos == UZ_INFLATE_DICT_SIZE)
        state->dictionary_pos = 0u;
    state->output[state->output_len++] = value;
    uz_u32_add_u16(&state->output_size, 1u);
    if (state->output_bounded) uz_u32_sub_u16(&state->output_left, 1u);
    if (state->output_len == state->output_cap) return flush_output(state);
    return 1u;
}

static unsigned char build_tree(UzInflate *state,
                                const unsigned char *lengths,
                                unsigned int symbol_count,
                                unsigned int *counts,
                                unsigned int *symbols,
                                unsigned int symbol_cap) {
    unsigned int offsets[16];
    unsigned int symbol;
    unsigned int length;
    unsigned int left;
    unsigned int used;

    memset(counts, 0, 16u * sizeof(counts[0]));
    for (symbol = 0u; symbol < symbol_count; ++symbol) {
        length = lengths[symbol];
        if (length > 15u) return fail(state, UZ_INFLATE_TREE);
        ++counts[length];
    }
    used = (unsigned int)(symbol_count - counts[0]);
    if (used > symbol_cap) return fail(state, UZ_INFLATE_TREE);
    left = 1u;
    for (length = 1u; length <= 15u; ++length) {
        left <<= 1u;
        if (counts[length] > left) return fail(state, UZ_INFLATE_TREE);
        left = (unsigned int)(left - counts[length]);
    }
    offsets[1] = 0u;
    for (length = 1u; length < 15u; ++length)
        offsets[length + 1u] = (unsigned int)(offsets[length] + counts[length]);
    for (symbol = 0u; symbol < symbol_count; ++symbol) {
        length = lengths[symbol];
        if (length != 0u) symbols[offsets[length]++] = symbol;
    }
    return 1u;
}

static unsigned char decode_symbol(UzInflate *state,
                                   const unsigned int *counts,
                                   const unsigned int *symbols,
                                   unsigned int *symbol) {
    unsigned int code;
    unsigned int first;
    unsigned int index;
    unsigned int count;
    unsigned int bit;
    unsigned char length;

    code = first = index = 0u;
    for (length = 1u; length <= 15u; ++length) {
        if (!bits(state, 1u, &bit)) return 0u;
        code |= bit;
        count = counts[length];
        if (code >= first && (unsigned int)(code - first) < count) {
            *symbol = symbols[index + (unsigned int)(code - first)];
            return 1u;
        }
        index = (unsigned int)(index + count);
        first = (unsigned int)((first + count) << 1u);
        code <<= 1u;
    }
    return fail(state, UZ_INFLATE_SYMBOL);
}

static unsigned char fixed_trees(UzInflate *state) {
    unsigned int index;

    for (index = 0u; index <= 143u; ++index) state->lengths[index] = 8u;
    for (; index <= 255u; ++index) state->lengths[index] = 9u;
    for (; index <= 279u; ++index) state->lengths[index] = 7u;
    for (; index <= 287u; ++index) state->lengths[index] = 8u;
    if (!build_tree(state, state->lengths, 288u, state->lit_count,
                    state->lit_symbol, UZ_INFLATE_LIT_SYMBOLS)) return 0u;
    for (index = 0u; index < 32u; ++index) state->lengths[index] = 5u;
    return build_tree(state, state->lengths, 32u, state->dist_count,
                      state->dist_symbol, UZ_INFLATE_DIST_SYMBOLS);
}

static unsigned char dynamic_trees(UzInflate *state) {
    unsigned int value;
    unsigned int hlit;
    unsigned int hdist;
    unsigned int hclen;
    unsigned int total;
    unsigned int index;
    unsigned int symbol;
    unsigned int repeat;
    unsigned char previous;
    unsigned char code_lengths[19];

    if (!bits(state, 5u, &value)) return 0u;
    hlit = (unsigned int)(value + 257u);
    if (!bits(state, 5u, &value)) return 0u;
    hdist = (unsigned int)(value + 1u);
    if (!bits(state, 4u, &value)) return 0u;
    hclen = (unsigned int)(value + 4u);
    if (hlit > 286u || hdist > 32u) return fail(state, UZ_INFLATE_TREE);
    memset(code_lengths, 0, sizeof(code_lengths));
    for (index = 0u; index < hclen; ++index) {
        if (!bits(state, 3u, &value)) return 0u;
        code_lengths[code_order[index]] = (unsigned char)value;
    }
    if (!build_tree(state, code_lengths, 19u, state->code_count,
                    state->code_symbol, UZ_INFLATE_CODE_SYMBOLS)) return 0u;
    total = (unsigned int)(hlit + hdist);
    index = 0u;
    previous = 0u;
    while (index < total) {
        if (!decode_symbol(state, state->code_count,
                           state->code_symbol, &symbol)) return 0u;
        if (symbol <= 15u) {
            previous = (unsigned char)symbol;
            state->lengths[index++] = previous;
        } else if (symbol == 16u) {
            if (index == 0u || !bits(state, 2u, &repeat))
                return fail(state, UZ_INFLATE_TREE);
            repeat = (unsigned int)(repeat + 3u);
            if (repeat > total - index) return fail(state, UZ_INFLATE_TREE);
            while (repeat-- != 0u) state->lengths[index++] = previous;
        } else if (symbol == 17u) {
            if (!bits(state, 3u, &repeat)) return 0u;
            repeat = (unsigned int)(repeat + 3u);
            if (repeat > total - index) return fail(state, UZ_INFLATE_TREE);
            previous = 0u;
            while (repeat-- != 0u) state->lengths[index++] = 0u;
        } else if (symbol == 18u) {
            if (!bits(state, 7u, &repeat)) return 0u;
            repeat = (unsigned int)(repeat + 11u);
            if (repeat > total - index) return fail(state, UZ_INFLATE_TREE);
            previous = 0u;
            while (repeat-- != 0u) state->lengths[index++] = 0u;
        } else {
            return fail(state, UZ_INFLATE_TREE);
        }
    }
    if (state->lengths[256] == 0u) return fail(state, UZ_INFLATE_TREE);
    if (!build_tree(state, state->lengths, hlit, state->lit_count,
                    state->lit_symbol, UZ_INFLATE_LIT_SYMBOLS)) return 0u;
    return build_tree(state, state->lengths + hlit, hdist,
                      state->dist_count, state->dist_symbol,
                      UZ_INFLATE_DIST_SYMBOLS);
}

static unsigned char have_distance(const UzInflate *state,
                                   unsigned int distance) {
    if (distance == 0u || distance > UZ_INFLATE_DICT_SIZE) return 0u;
    if (state->output_size.hi != 0u) return 1u;
    return (unsigned char)(state->output_size.lo >= distance);
}

static unsigned char compressed_block(UzInflate *state) {
    unsigned int symbol;
    unsigned int length;
    unsigned int distance;
    unsigned int extra;
    unsigned int source;
    unsigned char extra_count;

    for (;;) {
        if (!decode_symbol(state, state->lit_count,
                           state->lit_symbol, &symbol)) return 0u;
        if (symbol < 256u) {
            if (!emit_byte(state, (unsigned char)symbol)) return 0u;
            continue;
        }
        if (symbol == 256u) return 1u;
        if (symbol < 257u || symbol > 285u)
            return fail(state, UZ_INFLATE_SYMBOL);
        symbol = (unsigned int)(symbol - 257u);
        length = length_base[symbol];
        extra_count = length_extra[symbol];
        if (!bits(state, extra_count, &extra)) return 0u;
        length = (unsigned int)(length + extra);
        if (!decode_symbol(state, state->dist_count,
                           state->dist_symbol, &symbol)) return 0u;
        if (symbol >= 30u) return fail(state, UZ_INFLATE_DISTANCE);
        distance = distance_base[symbol];
        extra_count = distance_extra[symbol];
        if (!bits(state, extra_count, &extra)) return 0u;
        distance = (unsigned int)(distance + extra);
        if (!have_distance(state, distance))
            return fail(state, UZ_INFLATE_DISTANCE);
        source = (state->dictionary_pos >= distance)
            ? (unsigned int)(state->dictionary_pos - distance)
            : (unsigned int)(UZ_INFLATE_DICT_SIZE + state->dictionary_pos -
                             distance);
        while (length-- != 0u) {
            unsigned char value;
            value = state->dictionary[source++];
            if (source == UZ_INFLATE_DICT_SIZE) source = 0u;
            if (!emit_byte(state, value)) return 0u;
        }
    }
}

static unsigned char stored_block(UzInflate *state) {
    unsigned char lo;
    unsigned char hi;
    unsigned int length;
    unsigned int inverted;
    unsigned int chunk;
    unsigned int index;

    state->bit_buffer = 0u;
    state->bit_count = 0u;
    if (!source_byte(state, &lo) || !source_byte(state, &hi)) return 0u;
    length = (unsigned int)(lo | ((unsigned int)hi << 8u));
    if (!source_byte(state, &lo) || !source_byte(state, &hi)) return 0u;
    inverted = (unsigned int)(lo | ((unsigned int)hi << 8u));
    if ((unsigned int)(length ^ 0xFFFFu) != inverted)
        return fail(state, UZ_INFLATE_STORED_LENGTH);
    while (length != 0u) {
        if (!refill(state)) return 0u;
        chunk = (unsigned int)(state->input_len - state->input_pos);
        if (chunk > length) chunk = length;
        for (index = 0u; index < chunk; ++index) {
            if (!emit_byte(state, state->input[state->input_pos++])) return 0u;
        }
        length = (unsigned int)(length - chunk);
    }
    return 1u;
}

void uz_inflate_init(UzInflate *state,
                     UzInflateRead read, void *read_context,
                     UzInflateWrite write, void *write_context,
                     unsigned char *input, unsigned int input_cap,
                     unsigned char *output, unsigned int output_cap,
                     unsigned char *dictionary,
                     const UzU32 *compressed_size,
                     const UzU32 *expected_output_size) {
    memset(state, 0, sizeof(*state));
    state->read = read;
    state->read_context = read_context;
    state->write = write;
    state->write_context = write_context;
    state->input = input;
    state->input_cap = input_cap;
    state->output = output;
    state->output_cap = output_cap;
    state->dictionary = dictionary;
    if (compressed_size != 0) state->compressed_left = *compressed_size;
    if (expected_output_size != 0) {
        state->output_left = *expected_output_size;
        state->output_bounded = 1u;
    }
    uz_crc32_init(&state->crc);
}

unsigned char uz_inflate_run(UzInflate *state) {
    unsigned int final;
    unsigned int type;

    if (state == 0 || state->read == 0 || state->input == 0 ||
        state->input_cap == 0u || state->output == 0 ||
        state->output_cap == 0u || state->dictionary == 0) {
        if (state != 0) state->error = UZ_INFLATE_STATE;
        return 0u;
    }
    do {
        if (!bits(state, 1u, &final) || !bits(state, 2u, &type)) return 0u;
        if (type == 0u) {
            if (!stored_block(state)) return 0u;
        } else if (type == 1u) {
            if (!fixed_trees(state) || !compressed_block(state)) return 0u;
        } else if (type == 2u) {
            if (!dynamic_trees(state) || !compressed_block(state)) return 0u;
        } else {
            return fail(state, UZ_INFLATE_BLOCK_TYPE);
        }
    } while (final == 0u);
    if (state->compressed_left.hi != 0u || state->compressed_left.lo != 0u ||
        state->input_pos != state->input_len)
        return fail(state, UZ_INFLATE_TRAILING);
    if (state->output_bounded &&
        (state->output_left.hi != 0u || state->output_left.lo != 0u))
        return fail(state, UZ_INFLATE_OUTPUT_SIZE);
    if (!flush_output(state)) return 0u;
    uz_crc32_finish(&state->crc);
    state->error = UZ_INFLATE_OK;
    return 1u;
}
