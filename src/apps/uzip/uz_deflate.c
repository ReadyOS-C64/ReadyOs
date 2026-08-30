#include "uz_deflate_internal.h"

#include <string.h>

#ifdef __CC65__
#ifdef UZ_CREATE_COORD_BUILD
#pragma code-name(push, "CREATE_COORD_CODE")
#pragma rodata-name(push, "CREATE_COORD_RODATA")
#else
#pragma code-name(push, "DEFLATE_COORD_CODE")
#pragma rodata-name(push, "DEFLATE_COORD_RODATA")
#endif
#endif

/* Keeps the packed coordinator descriptor well-defined even in production
 * builds whose diagnostic layer contributes no additional constants. */
static volatile const unsigned char coord_package_marker = 0xD3u;

static unsigned char fail(UzDeflate *state, unsigned char error) {
    state->error = error;
    return 0u;
}

static unsigned int input_want(const UzU32 *left, unsigned int cap) {
    if (left->hi != 0u || left->lo > cap) return cap;
    return left->lo;
}

static unsigned char read_exact(UzDeflate *state, unsigned int length) {
    unsigned int done;
    int got;

    done = 0u;
    while (done < length) {
        got = state->read(state->read_context, state->input + done,
                          (unsigned int)(length - done));
        if (got <= 0 || (unsigned int)got > length - done)
            return fail(state, UZ_DEFLATE_ERR_INPUT);
        done = (unsigned int)(done + (unsigned int)got);
    }
    return 1u;
}

static unsigned char load_phase(UzDeflate *state, unsigned char phase) {
    if (state->phase_load != 0 &&
        !state->phase_load(state->phase_context, phase))
        return fail(state, UZ_DEFLATE_ERR_STATE);
    return 1u;
}

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
                     const UzU32 *input_size) {
    (void)coord_package_marker;
    memset(state, 0, sizeof(*state));
    state->read = read;
    state->read_context = read_context;
    state->write = write;
    state->write_context = write_context;
    state->token_store = token_store;
    state->token_load = token_load;
    state->token_context = token_context;
    state->phase_load = phase_load;
    state->phase_context = phase_context;
    state->workspace = workspace;
    state->input = input;
    state->input_cap = input_cap;
    state->output = output;
    state->output_cap = output_cap;
    state->token_stage = token_stage;
    state->token_stage_cap = token_stage_cap;
    if (input_size != 0) {
        state->input_left = *input_size;
        state->input_size = *input_size;
    }
    state->crc.byte[0] = 0xFFu;
    state->crc.byte[1] = 0xFFu;
    state->crc.byte[2] = 0xFFu;
    state->crc.byte[3] = 0xFFu;
    state->ready = (unsigned char)(
        read != 0 && write != 0 && token_store != 0 && token_load != 0 &&
        workspace != 0 && input != 0 && input_cap == UZ_DEFLATE_BLOCK_SIZE &&
        output != 0 && output_cap != 0u && output_cap <= 508u &&
        token_stage != 0 && token_stage_cap >= 4u &&
        token_stage_cap <= UZ_DEFLATE_TOKEN_MAX && input_size != 0);
    if (!state->ready) state->error = UZ_DEFLATE_ERR_STATE;
}

unsigned char uz_deflate_run(UzDeflate *state) {
    unsigned int want;
    unsigned int fixed_bits;
    unsigned char final;
    unsigned char reset;

    if (state == 0 || !state->ready)
        return state == 0 ? 0u : fail(state, UZ_DEFLATE_ERR_STATE);
    reset = 1u;
    if (state->input_left.hi == 0u && state->input_left.lo == 0u) {
        state->token_size = 0u;
        state->token_stage_len = 0u;
        if (!load_phase(state, UZ_DEFLATE_PHASE_EMIT) ||
            !uz_deflate_emit_block(state, 0u, 10u, 1u)) return 0u;
    }
    while (state->input_left.hi != 0u || state->input_left.lo != 0u) {
        want = input_want(&state->input_left, state->input_cap);
        if (!read_exact(state, want)) return 0u;
        uz_u32_sub_u16(&state->input_left, want);
        final = (unsigned char)(state->input_left.hi == 0u &&
                                state->input_left.lo == 0u);
        if (!load_phase(state, UZ_DEFLATE_PHASE_MATCH)) return 0u;
        uz_deflate_match_crc_update(&state->crc, state->input, want);
        if (
            !uz_deflate_tokenize_block(state, want, reset, &fixed_bits))
            return 0u;
        reset = 0u;
        if (!load_phase(state, UZ_DEFLATE_PHASE_EMIT) ||
            !uz_deflate_emit_block(state, want, fixed_bits, final)) return 0u;
    }
    state->crc.byte[0] ^= 0xFFu;
    state->crc.byte[1] ^= 0xFFu;
    state->crc.byte[2] ^= 0xFFu;
    state->crc.byte[3] ^= 0xFFu;
    state->error = UZ_DEFLATE_OK;
    return 1u;
}

#ifdef __CC65__
#pragma rodata-name(pop)
#pragma code-name(pop)
#endif
