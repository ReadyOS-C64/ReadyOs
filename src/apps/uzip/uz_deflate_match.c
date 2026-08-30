#include "uz_deflate_internal.h"

#include <string.h>

#ifdef __CC65__
#pragma code-name(push, "DEFLATE_MATCH_CODE")
#pragma rodata-name(push, "DEFLATE_MATCH_RODATA")
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

/* CRC updates happen only after MATCH has been loaded. Keeping this private
 * copy in the matcher prevents a live compressor from calling the Store
 * overlay's CRC implementation after `$B000` has been replaced. */
static const unsigned char crc_nibble[16][4] = {
    {0x00u, 0x00u, 0x00u, 0x00u}, {0x64u, 0x10u, 0xB7u, 0x1Du},
    {0xC8u, 0x20u, 0x6Eu, 0x3Bu}, {0xACu, 0x30u, 0xD9u, 0x26u},
    {0x90u, 0x41u, 0xDCu, 0x76u}, {0xF4u, 0x51u, 0x6Bu, 0x6Bu},
    {0x58u, 0x61u, 0xB2u, 0x4Du}, {0x3Cu, 0x71u, 0x05u, 0x50u},
    {0x20u, 0x83u, 0xB8u, 0xEDu}, {0x44u, 0x93u, 0x0Fu, 0xF0u},
    {0xE8u, 0xA3u, 0xD6u, 0xD6u}, {0x8Cu, 0xB3u, 0x61u, 0xCBu},
    {0xB0u, 0xC2u, 0x64u, 0x9Bu}, {0xD4u, 0xD2u, 0xD3u, 0x86u},
    {0x78u, 0xE2u, 0x0Au, 0xA0u}, {0x1Cu, 0xF2u, 0xBDu, 0xBDu}
};

void uz_deflate_match_crc_update(UzCrc32 *crc,
                                 const unsigned char *data,
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
            crc->byte[0] ^= crc_nibble[index][0];
            crc->byte[1] ^= crc_nibble[index][1];
            crc->byte[2] ^= crc_nibble[index][2];
            crc->byte[3] ^= crc_nibble[index][3];
        }
    }
}

static unsigned char fail(UzDeflate *state, unsigned char error) {
    state->error = error;
    return 0u;
}

static unsigned int get16(const unsigned char *data, unsigned int offset) {
    return (unsigned int)(data[offset] |
                          ((unsigned int)data[offset + 1u] << 8u));
}

static void set16(unsigned char *data, unsigned int offset,
                  unsigned int value) {
    data[offset] = (unsigned char)value;
    data[offset + 1u] = (unsigned char)(value >> 8u);
}

static unsigned int head_get(const UzDeflate *state, unsigned int hash) {
    return get16(state->workspace,
        (unsigned int)(UZ_DEFLATE_HEAD_OFFSET + 2u * hash));
}

static void head_set(UzDeflate *state, unsigned int hash,
                     unsigned int value) {
    set16(state->workspace,
          (unsigned int)(UZ_DEFLATE_HEAD_OFFSET + 2u * hash), value);
}

static unsigned int prev_get(const UzDeflate *state, unsigned int slot) {
    return get16(state->workspace,
        (unsigned int)(UZ_DEFLATE_PREV_OFFSET + 2u * slot));
}

static void prev_set(UzDeflate *state, unsigned int slot,
                     unsigned int value) {
    set16(state->workspace,
          (unsigned int)(UZ_DEFLATE_PREV_OFFSET + 2u * slot), value);
}

static unsigned char history_get(const UzDeflate *state, unsigned int slot) {
    return state->workspace[UZ_DEFLATE_HISTORY_OFFSET + slot];
}

static void history_set(UzDeflate *state, unsigned int slot,
                        unsigned char value) {
    state->workspace[UZ_DEFLATE_HISTORY_OFFSET + slot] = value;
}

static unsigned int hash3(const unsigned char *data) {
    unsigned int value;

    value = (unsigned int)(((unsigned int)data[0] << 5u) ^
                           ((unsigned int)data[1] << 2u) ^ data[2]);
    value ^= (unsigned int)(value >> 10u);
    return (unsigned int)(value & UZ_DEFLATE_HASH_MASK);
}

static unsigned char fixed_symbol_bits(unsigned int symbol) {
    if (symbol <= 143u) return 8u;
    if (symbol <= 255u) return 9u;
    if (symbol <= 279u) return 7u;
    return 8u;
}

static unsigned char length_code(unsigned int length, unsigned char *extra) {
    unsigned char index;
    unsigned int maximum;

    if (length == 258u) {
        *extra = 0u;
        return 28u;
    }
    for (index = 0u; index < 28u; ++index) {
        maximum = length_base[index];
        if (length_extra[index] != 0u)
            maximum = (unsigned int)(maximum +
                ((1u << length_extra[index]) - 1u));
        if (length <= maximum) {
            *extra = length_extra[index];
            return index;
        }
    }
    return 0xFFu;
}

static unsigned char distance_code(unsigned int distance,
                                   unsigned char *extra) {
    unsigned char index;
    unsigned int maximum;

    for (index = 0u; index < 26u; ++index) {
        maximum = distance_base[index];
        if (distance_extra[index] != 0u)
            maximum = (unsigned int)(maximum +
                ((1u << distance_extra[index]) - 1u));
        if (distance <= maximum) {
            *extra = distance_extra[index];
            return index;
        }
    }
    return 0xFFu;
}

static unsigned char flush_tokens(UzDeflate *state) {
    unsigned int offset;

    if (state->token_stage_len == 0u) return 1u;
    offset = (unsigned int)(state->token_size - state->token_stage_len);
    if (!state->token_store(state->token_context, offset,
                            state->token_stage, state->token_stage_len))
        return fail(state, UZ_DEFLATE_ERR_TOKEN);
    state->token_stage_len = 0u;
    return 1u;
}

static unsigned char token_byte(UzDeflate *state, unsigned char value) {
    if (state->token_size == UZ_DEFLATE_TOKEN_MAX)
        return fail(state, UZ_DEFLATE_ERR_TOKEN);
    state->token_stage[state->token_stage_len++] = value;
    ++state->token_size;
    if (state->token_stage_len == state->token_stage_cap)
        return flush_tokens(state);
    return 1u;
}

static unsigned char token_literal(UzDeflate *state, unsigned char value) {
    return (unsigned char)(token_byte(state, value) &&
                           token_byte(state, 0u));
}

static unsigned char token_match(UzDeflate *state, unsigned int length,
                                 unsigned int distance) {
    return (unsigned char)(
        token_byte(state, (unsigned char)(length - 3u)) &&
        token_byte(state, 1u) &&
        token_byte(state, (unsigned char)distance) &&
        token_byte(state, (unsigned char)(distance >> 8u)));
}

static unsigned int match_byte(const UzDeflate *state,
                               unsigned int input_pos,
                               unsigned int distance,
                               unsigned int length) {
    unsigned int slot;

    if (length >= distance)
        return state->input[input_pos + length - distance];
    slot = (unsigned int)((state->window_pos + UZ_DEFLATE_WINDOW_SIZE -
                           distance + length) & UZ_DEFLATE_WINDOW_MASK);
    return history_get(state, slot);
}

static unsigned int find_match(UzDeflate *state, unsigned int input_pos,
                               unsigned int input_len,
                               unsigned int *best_distance) {
    unsigned int hash;
    unsigned int candidate;
    unsigned int next;
    unsigned int slot;
    unsigned int distance;
    unsigned int length;
    unsigned int maximum;
    unsigned int best;
    unsigned char links;

    if (input_len - input_pos < 3u || state->window_filled < 3u) return 0u;
    hash = hash3(state->input + input_pos);
    candidate = head_get(state, hash);
    maximum = (unsigned int)(input_len - input_pos);
    if (maximum > UZ_DEFLATE_MAX_MATCH) maximum = UZ_DEFLATE_MAX_MATCH;
    best = 2u;
    links = 0u;
    while (candidate != 0u && links < UZ_DEFLATE_MAX_CHAIN) {
        slot = (unsigned int)(candidate - 1u);
        if (slot >= UZ_DEFLATE_WINDOW_SIZE) break;
        distance = (unsigned int)((state->window_pos +
                    UZ_DEFLATE_WINDOW_SIZE - slot) &
                    UZ_DEFLATE_WINDOW_MASK);
        if (distance == 0u) distance = UZ_DEFLATE_WINDOW_SIZE;
        if (distance <= state->window_filled &&
            history_get(state, slot) == state->input[input_pos]) {
            length = 1u;
            while (length < maximum &&
                   match_byte(state, input_pos, distance, length) ==
                       state->input[input_pos + length])
                ++length;
            if (length > best) {
                best = length;
                *best_distance = distance;
                if (length == maximum) break;
            }
        }
        next = prev_get(state, slot);
        if (next == candidate) break;
        candidate = next;
        ++links;
    }
    return best >= 3u ? best : 0u;
}

static void insert_byte(UzDeflate *state, unsigned int input_pos,
                        unsigned int input_len) {
    unsigned int hash;
    unsigned int previous;
    unsigned int encoded_slot;

    encoded_slot = (unsigned int)(state->window_pos + 1u);
    if (input_len - input_pos >= 3u) {
        hash = hash3(state->input + input_pos);
        previous = head_get(state, hash);
        if (previous == encoded_slot) previous = 0u;
        prev_set(state, state->window_pos, previous);
        head_set(state, hash, encoded_slot);
    } else {
        prev_set(state, state->window_pos, 0u);
    }
    history_set(state, state->window_pos, state->input[input_pos]);
    state->window_pos = (unsigned int)((state->window_pos + 1u) &
                                       UZ_DEFLATE_WINDOW_MASK);
    if (state->window_filled < UZ_DEFLATE_WINDOW_SIZE)
        ++state->window_filled;
}

static unsigned int match_fixed_bits(unsigned int length,
                                     unsigned int distance) {
    unsigned char length_index;
    unsigned char distance_index;
    unsigned char extra;
    unsigned int bits;

    length_index = length_code(length, &extra);
    if (length_index == 0xFFu) return 0xFFFFu;
    bits = (unsigned int)(fixed_symbol_bits(
        (unsigned int)(257u + length_index)) + extra);
    distance_index = distance_code(distance, &extra);
    if (distance_index == 0xFFu) return 0xFFFFu;
    return (unsigned int)(bits + 5u + extra);
}

unsigned char uz_deflate_tokenize_block(UzDeflate *state,
                                        unsigned int input_len,
                                        unsigned char reset,
                                        unsigned int *fixed_bits) {
    unsigned int position;
    unsigned int length;
    unsigned int distance;
    unsigned int consume;
    unsigned int bits;

    if (reset) {
        memset(state->workspace + UZ_DEFLATE_HEAD_OFFSET, 0,
               2u * UZ_DEFLATE_HASH_SIZE);
        state->window_pos = 0u;
        state->window_filled = 0u;
    }
    state->token_size = 0u;
    state->token_stage_len = 0u;
    *fixed_bits = 10u;
    position = 0u;
    while (position < input_len) {
        distance = 0u;
        length = find_match(state, position, input_len, &distance);
        if (length >= 3u) {
            bits = match_fixed_bits(length, distance);
            if (bits == 0xFFFFu)
                return fail(state, UZ_DEFLATE_ERR_INTERNAL);
            if (!token_match(state, length, distance)) return 0u;
            *fixed_bits = (unsigned int)(*fixed_bits + bits);
            consume = length;
        } else {
            if (!token_literal(state, state->input[position])) return 0u;
            *fixed_bits = (unsigned int)(*fixed_bits +
                fixed_symbol_bits(state->input[position]));
            consume = 1u;
        }
        while (consume-- != 0u) {
            insert_byte(state, position, input_len);
            ++position;
        }
    }
    return flush_tokens(state);
}

#ifdef __CC65__
#pragma rodata-name(pop)
#pragma code-name(pop)
#endif
