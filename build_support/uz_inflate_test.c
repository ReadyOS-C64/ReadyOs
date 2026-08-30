#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "uz_inflate.h"

typedef struct {
    const unsigned char *data;
    unsigned int length;
    unsigned int pos;
    unsigned int max_chunk;
} Reader;

typedef struct {
    unsigned char *data;
    unsigned int cap;
    unsigned int length;
} Writer;

static int read_chunk(void *context, unsigned char *data,
                      unsigned int length) {
    Reader *reader;
    unsigned int left;

    reader = (Reader *)context;
    left = (unsigned int)(reader->length - reader->pos);
    if (length > reader->max_chunk) length = reader->max_chunk;
    if (length > left) length = left;
    if (length == 0u) return 0;
    memcpy(data, reader->data + reader->pos, length);
    reader->pos = (unsigned int)(reader->pos + length);
    return (int)length;
}

static unsigned char write_chunk(void *context, const unsigned char *data,
                                 unsigned int length) {
    Writer *writer;

    writer = (Writer *)context;
    if (length > writer->cap - writer->length) return 0u;
    memcpy(writer->data + writer->length, data, length);
    writer->length = (unsigned int)(writer->length + length);
    return 1u;
}

static unsigned char *raw_deflate(const unsigned char *source,
                                  unsigned int source_len,
                                  int level, int strategy,
                                  unsigned int *packed_len) {
    z_stream stream;
    unsigned char *packed;
    unsigned int cap;
    int rc;

    cap = (unsigned int)(source_len + source_len / 8u + 1024u);
    packed = (unsigned char *)malloc(cap);
    assert(packed != 0);
    memset(&stream, 0, sizeof(stream));
    rc = deflateInit2(&stream, level, Z_DEFLATED, -15, 8, strategy);
    assert(rc == Z_OK);
    stream.next_in = (Bytef *)source;
    stream.avail_in = source_len;
    stream.next_out = packed;
    stream.avail_out = cap;
    rc = deflate(&stream, Z_FINISH);
    assert(rc == Z_STREAM_END);
    *packed_len = (unsigned int)stream.total_out;
    assert(deflateEnd(&stream) == Z_OK);
    return packed;
}

static void set_u32(UzU32 *value, unsigned int size) {
    value->lo = size;
    value->hi = 0u;
}

static void decode_ok(const unsigned char *source, unsigned int source_len,
                      int level, int strategy) {
    unsigned char *packed;
    unsigned int packed_len;
    unsigned char *decoded;
    unsigned char *dictionary;
    unsigned char input[37];
    unsigned char output[29];
    UzInflate *inflate;
    UzU32 packed_size;
    UzU32 expected_size;
    Reader reader;
    Writer writer;

    packed = raw_deflate(source, source_len, level, strategy, &packed_len);
    decoded = (unsigned char *)malloc(source_len == 0u ? 1u : source_len);
    dictionary = (unsigned char *)malloc(UZ_INFLATE_DICT_SIZE);
    inflate = (UzInflate *)malloc(sizeof(*inflate));
    assert(decoded != 0 && dictionary != 0 && inflate != 0);
    reader.data = packed;
    reader.length = packed_len;
    reader.pos = 0u;
    reader.max_chunk = 7u;
    writer.data = decoded;
    writer.cap = source_len;
    writer.length = 0u;
    set_u32(&packed_size, packed_len);
    set_u32(&expected_size, source_len);
    uz_inflate_init(inflate, read_chunk, &reader, write_chunk, &writer,
                    input, sizeof(input), output, sizeof(output), dictionary,
                    &packed_size, &expected_size);
    assert(uz_inflate_run(inflate));
    assert(inflate->error == UZ_INFLATE_OK);
    assert(inflate->output_size.hi == 0u &&
           inflate->output_size.lo == source_len);
    assert(writer.length == source_len);
    assert(memcmp(decoded, source, source_len) == 0);
    free(inflate);
    free(dictionary);
    free(decoded);
    free(packed);
}

static void decode_bad(const unsigned char *packed, unsigned int packed_len,
                       int expected_len, unsigned char expected_error) {
    unsigned char decoded[128];
    unsigned char *dictionary;
    unsigned char input[11];
    unsigned char output[13];
    UzInflate *inflate;
    UzU32 packed_size;
    UzU32 expected_size;
    const UzU32 *expected_ptr;
    Reader reader;
    Writer writer;

    dictionary = (unsigned char *)malloc(UZ_INFLATE_DICT_SIZE);
    inflate = (UzInflate *)malloc(sizeof(*inflate));
    assert(dictionary != 0 && inflate != 0);
    reader.data = packed;
    reader.length = packed_len;
    reader.pos = 0u;
    reader.max_chunk = 3u;
    writer.data = decoded;
    writer.cap = sizeof(decoded);
    writer.length = 0u;
    set_u32(&packed_size, packed_len);
    expected_ptr = 0;
    if (expected_len >= 0) {
        set_u32(&expected_size, (unsigned int)expected_len);
        expected_ptr = &expected_size;
    }
    uz_inflate_init(inflate, read_chunk, &reader, write_chunk, &writer,
                    input, sizeof(input), output, sizeof(output), dictionary,
                    &packed_size, expected_ptr);
    assert(!uz_inflate_run(inflate));
    if (expected_error != 0u) assert(inflate->error == expected_error);
    free(inflate);
    free(dictionary);
}

int main(void) {
    static const unsigned char hello[] = "hello deflate hello deflate";
    unsigned char *large;
    unsigned char *packed;
    unsigned char *trailing;
    unsigned int packed_len;
    unsigned int index;
    unsigned char invalid[1];

    decode_ok((const unsigned char *)"", 0u, 0, Z_DEFAULT_STRATEGY);
    decode_ok(hello, sizeof(hello) - 1u, 0, Z_DEFAULT_STRATEGY);
    decode_ok(hello, sizeof(hello) - 1u, 6, Z_FIXED);
    decode_ok(hello, sizeof(hello) - 1u, 6, Z_DEFAULT_STRATEGY);

    large = (unsigned char *)malloc(65535u);
    assert(large != 0);
    for (index = 0u; index < 65535u; ++index)
        large[index] = (unsigned char)((index * 29u + (index >> 7u)) & 0xFFu);
    decode_ok(large, 65535u, 6, Z_FIXED);
    decode_ok(large, 65535u, 9, Z_DEFAULT_STRATEGY);

    packed = raw_deflate(hello, sizeof(hello) - 1u, 6,
                         Z_DEFAULT_STRATEGY, &packed_len);
    assert(packed_len > 1u);
    decode_bad(packed, (unsigned int)(packed_len - 1u), -1, 0u);
    trailing = (unsigned char *)malloc((unsigned int)(packed_len + 1u));
    assert(trailing != 0);
    memcpy(trailing, packed, packed_len);
    trailing[packed_len] = 0xA5u;
    decode_bad(trailing, (unsigned int)(packed_len + 1u), -1,
               UZ_INFLATE_TRAILING);
    invalid[0] = 0x07u;
    decode_bad(invalid, 1u, -1, UZ_INFLATE_BLOCK_TYPE);
    decode_bad(packed, packed_len, (int)(sizeof(hello) - 2u),
               UZ_INFLATE_OUTPUT_SIZE);
    decode_bad(packed, packed_len, (int)sizeof(hello),
               UZ_INFLATE_OUTPUT_SIZE);

    free(trailing);
    free(packed);
    free(large);
    return 0;
}
