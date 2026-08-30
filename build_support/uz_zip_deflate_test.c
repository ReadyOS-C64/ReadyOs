#include <assert.h>
#include <string.h>
#include <zlib.h>

#include "uz_deflate.h"
#include "uz_zip_write.h"

typedef struct {
    unsigned char data[20000];
    unsigned int size;
    unsigned int limit;
} Sink;

typedef struct {
    const unsigned char *data;
    unsigned int size;
    unsigned int position;
} Input;

typedef struct {
    unsigned char data[UZ_DEFLATE_TOKEN_MAX];
    unsigned int size;
} Tokens;

static unsigned char workspace[UZ_DEFLATE_WORKSPACE_SIZE];
static unsigned char input_stage[UZ_DEFLATE_BLOCK_SIZE];
static unsigned char output_stage[508];
static unsigned char token_stage[251];
static Sink *dos_sink;

static unsigned int get16(const unsigned char *data) {
    return (unsigned int)(data[0] | ((unsigned int)data[1] << 8u));
}

static unsigned long get32(const unsigned char *data) {
    return (unsigned long)data[0] | ((unsigned long)data[1] << 8u) |
           ((unsigned long)data[2] << 16u) | ((unsigned long)data[3] << 24u);
}

static unsigned char sink_write(void *context, const unsigned char *data,
                                unsigned int length) {
    Sink *sink;

    sink = (Sink *)context;
    if (length > sink->limit - sink->size) return 0u;
    memcpy(sink->data + sink->size, data, length);
    sink->size = (unsigned int)(sink->size + length);
    return 1u;
}

unsigned char uz_dos_write(UzDos *dos, const void *source,
                           unsigned int length) {
    (void)dos;
    return sink_write(dos_sink, (const unsigned char *)source, length);
}

static unsigned char raw_write(void *context, const unsigned char *data,
                               unsigned int length) {
    return uz_dos_write((UzDos *)context, data, length);
}

static int read_input(void *context, unsigned char *data,
                      unsigned int length) {
    Input *input;
    unsigned int available;

    input = (Input *)context;
    available = (unsigned int)(input->size - input->position);
    if (length > available) length = available;
    if (length > 137u) length = 137u;
    if (length == 0u) return 0;
    memcpy(data, input->data + input->position, length);
    input->position = (unsigned int)(input->position + length);
    return (int)length;
}

static unsigned char token_store(void *context, unsigned int offset,
                                 const unsigned char *data,
                                 unsigned int length) {
    Tokens *tokens;

    tokens = (Tokens *)context;
    if (offset > sizeof(tokens->data) ||
        length > sizeof(tokens->data) - offset) return 0u;
    memcpy(tokens->data + offset, data, length);
    if (tokens->size < offset + length)
        tokens->size = (unsigned int)(offset + length);
    return 1u;
}

static unsigned char token_load(void *context, unsigned int offset,
                                unsigned char *data,
                                unsigned int length) {
    Tokens *tokens;

    tokens = (Tokens *)context;
    if (offset > tokens->size || length > tokens->size - offset) return 0u;
    memcpy(data, tokens->data + offset, length);
    return 1u;
}

int main(void) {
    static unsigned char plain[12000];
    unsigned char decoded[12000];
    unsigned int packed_len;
    unsigned int index;
    unsigned int data_at;
    unsigned int descriptor_at;
    unsigned long crc;
    unsigned int random_value;
    z_stream inflater;
    Sink sink;
    Input input;
    Tokens tokens;
    UzU32 plain_size;
    UzU32 central_offset;
    UzDos output;
    UzZipRecord record;
    UzZipWriter writer;
    UzDeflate codec;

    random_value = 0xACE1u;
    for (index = 0u; index < sizeof(plain); ++index) {
        if (index < 6000u) {
            plain[index] = (unsigned char)
                "READYOS-ULTIMATE-ZIP"[index % 20u];
        } else {
            random_value = (unsigned int)(random_value * 25173u + 13849u);
            plain[index] = (unsigned char)(random_value >> 8u);
        }
    }
    crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, plain, sizeof(plain));

    memset(&sink, 0, sizeof(sink));
    sink.limit = sizeof(sink.data);
    dos_sink = &sink;
    memset(&output, 0, sizeof(output));
    uz_zip_writer_init(&writer, &output);
    assert(!uz_zip_begin_deflate(&writer, &record, "../BAD"));
    assert(writer.error == UZ_ZIP_ERR_NAME);
    uz_zip_writer_init(&writer, &output);
    assert(uz_zip_begin_deflate(&writer, &record, "NESTED/DATA.BIN"));
    assert(record.local_offset.lo == 0u && record.local_offset.hi == 0u);
    assert(record.method == 8u && record.flags == 8u && !record.directory);
    data_at = sink.size;

    input.data = plain;
    input.size = sizeof(plain);
    input.position = 0u;
    memset(&tokens, 0, sizeof(tokens));
    memset(&codec, 0, sizeof(codec));
    plain_size.lo = sizeof(plain);
    plain_size.hi = 0u;
    uz_deflate_init(&codec, read_input, &input,
                    raw_write, &output,
                    token_store, token_load, &tokens,
                    0, 0, workspace,
                    input_stage, sizeof(input_stage),
                    output_stage, sizeof(output_stage),
                    token_stage, sizeof(token_stage), &plain_size);
    assert(uz_deflate_run(&codec));
    packed_len = codec.output_size.lo;
    assert(codec.output_size.hi == 0u && input.position == sizeof(plain));
    assert(codec.fixed_blocks != 0u && codec.stored_blocks != 0u);
    record.size = codec.input_size;
    record.compressed_size = codec.output_size;
    record.crc = codec.crc;
    assert(uz_zip_finish_deflate(&writer));
    central_offset = writer.offset;
    assert(uz_zip_emit_central(&writer, &record));
    assert(uz_zip_finish_archive(&writer, &central_offset, 1u));

    assert(memcmp(sink.data, "PK\003\004", 4u) == 0);
    assert(get16(sink.data + 6u) == 8u && get16(sink.data + 8u) == 8u);
    assert(get16(sink.data + 26u) == 15u);
    assert(memcmp(sink.data + 30u, "NESTED/DATA.BIN", 15u) == 0);
    assert(data_at == 45u);
    descriptor_at = (unsigned int)(data_at + packed_len);
    assert(memcmp(sink.data + descriptor_at, "PK\007\010", 4u) == 0);
    assert(get32(sink.data + descriptor_at + 4u) == crc);
    assert(get32(sink.data + descriptor_at + 8u) == packed_len);
    assert(get32(sink.data + descriptor_at + 12u) == sizeof(plain));
    assert(record.compressed_size.lo == packed_len &&
           record.compressed_size.hi == 0u);
    assert(memcmp(sink.data + descriptor_at + 16u, "PK\001\002", 4u) == 0);
    assert(get16(sink.data + descriptor_at + 24u) == 8u);
    assert(get16(sink.data + descriptor_at + 26u) == 8u);
    assert(get32(sink.data + descriptor_at + 36u) == packed_len);
    assert(get32(sink.data + descriptor_at + 40u) == sizeof(plain));
    assert(memcmp(sink.data + sink.size - 22u, "PK\005\006", 4u) == 0);
    assert(writer.offset.lo == sink.size && writer.offset.hi == 0u);

    memset(&inflater, 0, sizeof(inflater));
    inflater.next_in = sink.data + data_at;
    inflater.avail_in = packed_len;
    inflater.next_out = decoded;
    inflater.avail_out = sizeof(decoded);
    assert(inflateInit2(&inflater, -15) == Z_OK);
    assert(inflate(&inflater, Z_FINISH) == Z_STREAM_END);
    assert(inflateEnd(&inflater) == Z_OK);
    assert(inflater.total_out == sizeof(plain));
    assert(memcmp(decoded, plain, sizeof(plain)) == 0);

    memset(&sink, 0, sizeof(sink));
    sink.limit = 8u;
    dos_sink = &sink;
    uz_zip_writer_init(&writer, &output);
    assert(!uz_zip_begin_deflate(&writer, &record, "FAIL.BIN"));
    assert(writer.error == UZ_ZIP_ERR_WRITE);
    return 0;
}
