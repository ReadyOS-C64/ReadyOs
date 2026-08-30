#include <assert.h>
#include <string.h>

#include "uz_zip_write.h"

static unsigned char archive[512];
static unsigned int archive_len;

unsigned char uz_dos_write(UzDos *dos, const void *source,
                           unsigned int length) {
    (void)dos;
    if (archive_len + length > sizeof(archive)) return 0u;
    memcpy(archive + archive_len, source, length);
    archive_len = (unsigned int)(archive_len + length);
    return 1u;
}

static unsigned int get16(unsigned int offset) {
    return (unsigned int)(archive[offset] |
                          ((unsigned int)archive[offset + 1u] << 8u));
}

static unsigned long get32(unsigned int offset) {
    return (unsigned long)archive[offset] |
           ((unsigned long)archive[offset + 1u] << 8u) |
           ((unsigned long)archive[offset + 2u] << 16u) |
           ((unsigned long)archive[offset + 3u] << 24u);
}

int main(void) {
    UzDos output;
    UzZipWriter writer;
    UzZipRecord records[2];
    UzU32 central_offset;

    memset(&output, 0, sizeof(output));
    assert(uz_zip_name_safe("A.TXT", 0u));
    assert(uz_zip_name_safe("EMPTY/", 1u));
    assert(!uz_zip_name_safe("../BAD", 0u));
    assert(!uz_zip_name_safe("A//B", 0u));
    assert(!uz_zip_name_safe("/ABS", 0u));
    assert(!uz_zip_name_safe("A:B", 0u));

    uz_zip_writer_init(&writer, &output);
    assert(uz_zip_begin_store(&writer, &records[0], "A.TXT", 0u));
    assert(uz_zip_store_data(&writer, (const unsigned char *)"abc", 3u));
    assert(uz_zip_finish_store(&writer));
    assert(uz_zip_begin_store(&writer, &records[1], "EMPTY/", 1u));
    assert(uz_zip_finish_store(&writer));
    central_offset = writer.offset;
    assert(uz_zip_emit_central(&writer, &records[0]));
    assert(uz_zip_emit_central(&writer, &records[1]));
    assert(uz_zip_finish_archive(&writer, &central_offset, 2u));

    assert(archive_len == 231u);
    assert(memcmp(archive, "PK\003\004", 4u) == 0);
    assert(memcmp(archive + 54u, "PK\003\004", 4u) == 0);
    assert(memcmp(archive + 106u, "PK\001\002", 4u) == 0);
    assert(memcmp(archive + 209u, "PK\005\006", 4u) == 0);
    assert(archive[42] == 0xC2u && archive[43] == 0x41u &&
           archive[44] == 0x24u && archive[45] == 0x35u);
    assert(get16(209u + 8u) == 2u && get16(209u + 10u) == 2u);

    archive_len = 0u;
    uz_zip_writer_init(&writer, &output);
    assert(uz_zip_begin_deflate(&writer, &records[0], "B.BIN"));
    assert(!uz_zip_finish_store(&writer));
    assert(writer.error == UZ_ZIP_ERR_STATE && writer.active == &records[0]);
    writer.offset.lo = 0xFFF0u;
    writer.offset.hi = 1u;
    records[0].compressed_size.lo = 0x0020u;
    records[0].compressed_size.hi = 0u;
    records[0].size = records[0].compressed_size;
    memset(records[0].crc.byte, 0, sizeof(records[0].crc.byte));
    assert(uz_zip_finish_deflate(&writer));
    /* Raw extent crosses the low word, then the 16-byte descriptor advances
     * from $00010 to $00020 in high word two. */
    assert(writer.offset.lo == 0x0020u && writer.offset.hi == 2u);

    archive_len = 0u;
    uz_zip_writer_init(&writer, &output);
    assert(uz_zip_begin_deflate(&writer, &records[0], "EMPTY.BIN"));
    assert(uz_dos_write(&output, "\003\000", 2u));
    records[0].compressed_size.lo = 2u;
    records[0].compressed_size.hi = 0u;
    records[0].size.lo = records[0].size.hi = 0u;
    memset(records[0].crc.byte, 0, sizeof(records[0].crc.byte));
    assert(uz_zip_finish_deflate(&writer));
    assert(uz_zip_begin_store(&writer, &records[1], "TAIL.TXT", 0u));
    assert(uz_zip_store_data(&writer, (const unsigned char *)"abc", 3u));
    assert(uz_zip_finish_store(&writer));
    central_offset = writer.offset;
    assert(uz_zip_emit_central(&writer, &records[0]));
    assert(uz_zip_emit_central(&writer, &records[1]));
    assert(uz_zip_finish_archive(&writer, &central_offset, 2u));
    assert(archive_len == 245u);
    assert(records[0].local_offset.lo == 0u);
    assert(records[1].local_offset.lo == 57u);
    assert(memcmp(archive + 57u, "PK\003\004", 4u) == 0);
    assert(get16(8u) == 8u && get16(57u + 8u) == 0u);
    assert(memcmp(archive + 114u, "PK\001\002", 4u) == 0);
    assert(get16(114u + 10u) == 8u);
    assert(get32(114u + 42u) == 0u);
    assert(memcmp(archive + 169u, "PK\001\002", 4u) == 0);
    assert(get16(169u + 10u) == 0u);
    assert(get32(169u + 42u) == 57u);
    assert(memcmp(archive + 223u, "PK\005\006", 4u) == 0);
    assert(get16(223u + 8u) == 2u && get16(223u + 10u) == 2u);
    return 0;
}
