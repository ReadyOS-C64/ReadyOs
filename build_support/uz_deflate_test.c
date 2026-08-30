#include "uz_deflate.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

typedef struct {
    const unsigned char *data;
    unsigned int size;
    unsigned int position;
    unsigned int max_chunk;
} Input;

typedef struct {
    unsigned char *data;
    unsigned int capacity;
    unsigned int size;
} Buffer;

typedef struct {
    unsigned int calls;
    unsigned char next;
    unsigned char failed;
} PhaseTrace;

static const char *dump_case;
static const char *dump_path;

static unsigned char load_phase(void *context, unsigned char phase) {
    PhaseTrace *trace;

    trace = (PhaseTrace *)context;
    if (phase != trace->next) {
        trace->failed = 1u;
        return 0u;
    }
    ++trace->calls;
    trace->next = phase == UZ_DEFLATE_PHASE_MATCH
        ? UZ_DEFLATE_PHASE_EMIT : UZ_DEFLATE_PHASE_MATCH;
    return 1u;
}

static int read_input(void *context, unsigned char *data,
                      unsigned int length) {
    Input *input;
    unsigned int available;

    input = (Input *)context;
    available = (unsigned int)(input->size - input->position);
    if (length > available) length = available;
    if (length > input->max_chunk) length = input->max_chunk;
    if (length == 0u) return 0;
    memcpy(data, input->data + input->position, length);
    input->position = (unsigned int)(input->position + length);
    return (int)length;
}

static unsigned char write_buffer(void *context, const unsigned char *data,
                                  unsigned int length) {
    Buffer *buffer;

    buffer = (Buffer *)context;
    if (length > buffer->capacity - buffer->size) return 0u;
    memcpy(buffer->data + buffer->size, data, length);
    buffer->size = (unsigned int)(buffer->size + length);
    return 1u;
}

static unsigned char token_store(void *context, unsigned int offset,
                                 const unsigned char *data,
                                 unsigned int length) {
    Buffer *buffer;

    buffer = (Buffer *)context;
    if (offset > buffer->capacity || length > buffer->capacity - offset)
        return 0u;
    memcpy(buffer->data + offset, data, length);
    if (buffer->size < offset + length)
        buffer->size = (unsigned int)(offset + length);
    return 1u;
}

static unsigned char token_load(void *context, unsigned int offset,
                                unsigned char *data,
                                unsigned int length) {
    Buffer *buffer;

    buffer = (Buffer *)context;
    if (offset > buffer->size || length > buffer->size - offset) return 0u;
    memcpy(data, buffer->data + offset, length);
    return 1u;
}

static int inflate_exact(const unsigned char *packed, unsigned int packed_size,
                         unsigned char *plain, unsigned int plain_size) {
    z_stream stream;
    int result;

    memset(&stream, 0, sizeof(stream));
    stream.next_in = (Bytef *)packed;
    stream.avail_in = packed_size;
    stream.next_out = plain;
    stream.avail_out = plain_size == 0u ? 1u : plain_size;
    if (inflateInit2(&stream, -15) != Z_OK) return 0;
    result = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);
    return result == Z_STREAM_END && stream.total_in == packed_size &&
           stream.total_out == plain_size;
}

static unsigned int crc_value(const UzCrc32 *crc) {
    return (unsigned int)(crc->byte[0] | ((unsigned int)crc->byte[1] << 8u) |
        ((unsigned int)crc->byte[2] << 16u) |
        ((unsigned int)crc->byte[3] << 24u));
}

static int run_case(const char *name, const unsigned char *plain,
                    unsigned int plain_size, unsigned char expect_fixed,
                    unsigned char expect_stored,
                    unsigned char expected_first_type) {
    unsigned char *workspace;
    unsigned char *input_stage;
    unsigned char *output_stage;
    unsigned char *token_stage;
    unsigned char *token_memory;
    unsigned char *packed;
    unsigned char *decoded;
    unsigned int packed_capacity;
    unsigned long expected_crc;
    Input input;
    Buffer output;
    Buffer tokens;
    PhaseTrace phases;
    UzDeflate state;
    UzU32 size;
    FILE *dump;
    int ok;

    workspace = (unsigned char *)malloc(UZ_DEFLATE_WORKSPACE_SIZE);
    input_stage = (unsigned char *)malloc(UZ_DEFLATE_BLOCK_SIZE);
    output_stage = (unsigned char *)malloc(508u);
    token_stage = (unsigned char *)malloc(251u);
    token_memory = (unsigned char *)malloc(UZ_DEFLATE_TOKEN_MAX);
    packed_capacity = (unsigned int)(plain_size + plain_size / 100u + 256u);
    packed = (unsigned char *)malloc(packed_capacity);
    decoded = (unsigned char *)malloc(plain_size == 0u ? 1u : plain_size);
    if (workspace == NULL || input_stage == NULL || output_stage == NULL ||
        token_stage == NULL || token_memory == NULL || packed == NULL ||
        decoded == NULL) return 0;

    input.data = plain;
    input.size = plain_size;
    input.position = 0u;
    input.max_chunk = 137u;
    output.data = packed;
    output.capacity = packed_capacity;
    output.size = 0u;
    tokens.data = token_memory;
    tokens.capacity = UZ_DEFLATE_TOKEN_MAX;
    tokens.size = 0u;
    phases.calls = 0u;
    phases.next = plain_size == 0u ? UZ_DEFLATE_PHASE_EMIT
                                   : UZ_DEFLATE_PHASE_MATCH;
    phases.failed = 0u;
    size.lo = plain_size;
    size.hi = 0u;
    uz_deflate_init(&state, read_input, &input, write_buffer, &output,
                    token_store, token_load, &tokens,
                    load_phase, &phases, workspace,
                    input_stage, UZ_DEFLATE_BLOCK_SIZE,
                    output_stage, 508u, token_stage, 251u, &size);
    ok = uz_deflate_run(&state);
    if (ok && dump_case != NULL && dump_path != NULL &&
        strcmp(name, dump_case) == 0) {
        dump = fopen(dump_path, "wb");
        if (dump == NULL || fwrite(packed, 1u, output.size, dump) != output.size)
            ok = 0;
        if (dump != NULL) fclose(dump);
    }
    ok = ok && input.position == plain_size &&
         inflate_exact(packed, output.size, decoded, plain_size) &&
         memcmp(decoded, plain, plain_size) == 0;
    expected_crc = crc32(0L, Z_NULL, 0);
    expected_crc = crc32(expected_crc, plain, plain_size);
    ok = ok && crc_value(&state.crc) == (unsigned int)expected_crc &&
         state.output_size.lo == output.size && state.output_size.hi == 0u &&
         output.size != 0u && ((output.data[0] >> 1u) & 3u) == expected_first_type &&
         state.fixed_blocks + state.stored_blocks ==
             (plain_size == 0u ? 1u :
              (unsigned int)((plain_size + UZ_DEFLATE_BLOCK_SIZE - 1u) /
                             UZ_DEFLATE_BLOCK_SIZE)) &&
         !phases.failed && phases.calls == (plain_size == 0u ? 1u :
             2u * (unsigned int)((plain_size + UZ_DEFLATE_BLOCK_SIZE - 1u) /
                                 UZ_DEFLATE_BLOCK_SIZE)) &&
         (!expect_fixed || state.fixed_blocks != 0u) &&
         (expect_stored ? state.stored_blocks != 0u :
                          state.stored_blocks == 0u);
    if (!ok) {
        fprintf(stderr,
                "%s failed: error=%u packed=%u fixed=%u stored=%u input=%u\n",
                name, state.error, output.size, state.fixed_blocks,
                state.stored_blocks, input.position);
    }
    free(decoded);
    free(packed);
    free(token_memory);
    free(token_stage);
    free(output_stage);
    free(input_stage);
    free(workspace);
    return ok;
}

int main(void) {
    unsigned char repetitive[40000];
    unsigned char random_data[40000];
    unsigned char mixed[50000];
    unsigned char cross_window[16384];
    unsigned char workflow_deep[2305];
    unsigned int index;
    unsigned int value;

    dump_case = getenv("UZ_DEFLATE_TEST_DUMP_CASE");
    dump_path = getenv("UZ_DEFLATE_TEST_DUMP_PATH");

    for (index = 0u; index < sizeof(repetitive); ++index)
        repetitive[index] = (unsigned char)("READYOS-ULTIMATE-ZIP/"[
            index % 21u]);
    value = 0xACE1u;
    for (index = 0u; index < sizeof(random_data); ++index) {
        value = (unsigned int)(value * 25173u + 13849u);
        random_data[index] = (unsigned char)(value >> 8u);
    }
    for (index = 0u; index < sizeof(mixed); ++index) {
        if ((index / 4096u) & 1u) mixed[index] = random_data[index % 40000u];
        else mixed[index] = (unsigned char)(index & 7u);
    }
    memcpy(cross_window, random_data, 8192u);
    memcpy(cross_window + 8192u, random_data, 8192u);
    for (index = 0u; index < sizeof(workflow_deep); ++index)
        workflow_deep[index] = (unsigned char)
            ((index * 29u) ^ (index >> 1u) ^ 0xA5u);
    if (!run_case("empty", repetitive, 0u, 1u, 0u, 1u) ||
        !run_case("repetitive", repetitive, sizeof(repetitive), 1u, 0u, 1u) ||
        !run_case("random", random_data, sizeof(random_data), 0u, 1u, 0u) ||
        !run_case("mixed", mixed, sizeof(mixed), 1u, 1u, 1u) ||
        !run_case("cross-window", cross_window, sizeof(cross_window),
                  1u, 1u, 0u) ||
        !run_case("workflow-deep", workflow_deep, sizeof(workflow_deep),
                  1u, 0u, 1u)) return 1;
    return 0;
}
