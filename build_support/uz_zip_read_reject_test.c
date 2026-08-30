#include <assert.h>
#include <string.h>

#include "uz_zip_read.h"

static UzDos archive_output;
static UzDos archive_input;
static unsigned char archive[1024];
static unsigned int archive_len;
static unsigned int input_pos;

unsigned char uz_dos_write(UzDos *dos, const void *source,
                           unsigned int length) {
    assert(dos == &archive_output);
    assert(archive_len + length <= sizeof(archive));
    memcpy(archive + archive_len, source, length);
    archive_len = (unsigned int)(archive_len + length);
    return 1u;
}

unsigned char uz_dos_seek(UzDos *dos, const UzU32 *offset) {
    assert(dos == &archive_input && offset->hi == 0u);
    input_pos = offset->lo;
    return (unsigned char)(input_pos <= archive_len);
}

int uz_dos_read(UzDos *dos, void *destination, unsigned int length) {
    unsigned int available;
    assert(dos == &archive_input);
    available = (unsigned int)(archive_len - input_pos);
    if (length > available) length = available;
    memcpy(destination, archive + input_pos, length);
    input_pos = (unsigned int)(input_pos + length);
    return (int)length;
}

unsigned char uz_dos_file_info(UzDos *dos, UzU32 *size) {
    assert(dos == &archive_input);
    size->lo = archive_len;
    size->hi = 0u;
    return 1u;
}

static unsigned int find_signature(unsigned char third, unsigned char fourth) {
    unsigned int index;
    for (index = 0u; index + 4u <= archive_len; ++index) {
        if (archive[index] == 0x50u && archive[index + 1u] == 0x4Bu &&
            archive[index + 2u] == third && archive[index + 3u] == fourth) {
            return index;
        }
    }
    assert(0);
    return 0u;
}

static void put32(unsigned int offset, unsigned int value) {
    archive[offset] = (unsigned char)value;
    archive[offset + 1u] = (unsigned char)(value >> 8u);
    archive[offset + 2u] = 0u;
    archive[offset + 3u] = 0u;
}

static void build_valid(void) {
    UzZipWriter writer;
    UzZipRecord record;
    UzU32 central_offset;
    archive_len = 0u;
    input_pos = 0u;
    memset(archive, 0, sizeof(archive));
    uz_zip_writer_init(&writer, &archive_output);
    assert(uz_zip_begin_store(&writer, &record, "A.TXT", 0u));
    assert(uz_zip_store_data(&writer, (const unsigned char *)"abc", 3u));
    assert(uz_zip_finish_store(&writer));
    central_offset = writer.offset;
    assert(uz_zip_emit_central(&writer, &record));
    assert(uz_zip_finish_archive(&writer, &central_offset, 1u));
}

static void begin_and_next(UzZipReader *reader, UzZipRecord *record,
                           unsigned char scratch[512]) {
    uz_zip_reader_init(reader, &archive_input);
    assert(uz_zip_reader_begin(reader, scratch, 512u));
    assert(uz_zip_reader_next(reader, record));
}

int main(void) {
    UzZipReader reader;
    UzZipRecord record;
    unsigned char scratch[512];
    unsigned int central;
    unsigned int descriptor;
    unsigned int eocd;
    unsigned char saved_descriptor[16];

    build_valid();
    descriptor = find_signature(0x07u, 0x08u);
    central = find_signature(0x01u, 0x02u);
    eocd = find_signature(0x05u, 0x06u);
    memcpy(saved_descriptor, archive + descriptor, sizeof(saved_descriptor));
    memmove(archive + descriptor, archive + central,
            (unsigned int)(archive_len - central));
    archive_len = (unsigned int)(archive_len - sizeof(saved_descriptor));
    central = (unsigned int)(central - sizeof(saved_descriptor));
    eocd = (unsigned int)(eocd - sizeof(saved_descriptor));
    archive[6u] = 0u;
    archive[7u] = 0u;
    memcpy(archive + 14u, saved_descriptor + 4u, 4u);
    memcpy(archive + 18u, saved_descriptor + 8u, 8u);
    archive[central + 8u] = 0u;
    archive[central + 9u] = 0u;
    put32((unsigned int)(eocd + 16u), central);
    begin_and_next(&reader, &record, scratch);
    assert(!record.flags);
    assert(uz_zip_extract_store(&reader, &record, 0,
                                scratch, sizeof(scratch)));
    assert(uz_zip_reader_finished(&reader));

    build_valid();
    central = find_signature(0x01u, 0x02u);
    archive[7u] = 0x08u;
    archive[central + 9u] = 0x08u;
    begin_and_next(&reader, &record, scratch);
    assert(record.flags == 0x0808u);
    assert(uz_zip_extract_store(&reader, &record, 0,
                                scratch, sizeof(scratch)));
    assert(uz_zip_reader_finished(&reader));

    build_valid();
    --archive_len;
    uz_zip_reader_init(&reader, &archive_input);
    assert(!uz_zip_reader_begin(&reader, scratch, sizeof(scratch)));
    assert(reader.error == UZ_ZIP_READ_IO ||
           reader.error == UZ_ZIP_READ_NO_EOCD);

    build_valid();
    eocd = find_signature(0x05u, 0x06u);
    archive[eocd + 4u] = 1u;
    uz_zip_reader_init(&reader, &archive_input);
    assert(!uz_zip_reader_begin(&reader, scratch, sizeof(scratch)));
    assert(reader.error == UZ_ZIP_READ_UNSUPPORTED);

    build_valid();
    eocd = find_signature(0x05u, 0x06u);
    put32((unsigned int)(eocd + 16u), eocd);
    uz_zip_reader_init(&reader, &archive_input);
    assert(!uz_zip_reader_begin(&reader, scratch, sizeof(scratch)));
    assert(reader.error == UZ_ZIP_READ_BOUNDS);

    build_valid();
    central = find_signature(0x01u, 0x02u);
    archive[central + 10u] = 8u;
    uz_zip_reader_init(&reader, &archive_input);
    assert(uz_zip_reader_begin(&reader, scratch, sizeof(scratch)));
    assert(uz_zip_reader_next(&reader, &record));
    assert(record.method == 8u);
    assert(!uz_zip_extract_store(&reader, &record, 0,
                                 scratch, sizeof(scratch)));
    assert(reader.error == UZ_ZIP_READ_UNSUPPORTED);

    build_valid();
    central = find_signature(0x01u, 0x02u);
    archive[central + 10u] = 9u;
    uz_zip_reader_init(&reader, &archive_input);
    assert(uz_zip_reader_begin(&reader, scratch, sizeof(scratch)));
    assert(!uz_zip_reader_next(&reader, &record));
    assert(reader.error == UZ_ZIP_READ_UNSUPPORTED);

    build_valid();
    central = find_signature(0x01u, 0x02u);
    archive[central + 5u] = 3u;
    archive[central + 41u] = 0xA0u;
    uz_zip_reader_init(&reader, &archive_input);
    assert(uz_zip_reader_begin(&reader, scratch, sizeof(scratch)));
    assert(!uz_zip_reader_next(&reader, &record));
    assert(reader.error == UZ_ZIP_READ_UNSUPPORTED);

    build_valid();
    central = find_signature(0x01u, 0x02u);
    memcpy(archive + central + 46u, "../A/", 5u);
    uz_zip_reader_init(&reader, &archive_input);
    assert(uz_zip_reader_begin(&reader, scratch, sizeof(scratch)));
    assert(!uz_zip_reader_next(&reader, &record));
    assert(reader.error == UZ_ZIP_READ_NAME);

    build_valid();
    central = find_signature(0x01u, 0x02u);
    put32((unsigned int)(central + 20u), 1000u);
    put32((unsigned int)(central + 24u), 1000u);
    begin_and_next(&reader, &record, scratch);
    assert(!uz_zip_extract_store(&reader, &record, 0,
                                 scratch, sizeof(scratch)));
    assert(reader.error == UZ_ZIP_READ_BOUNDS);

    build_valid();
    begin_and_next(&reader, &record, scratch);
    archive[8u] = 8u;
    assert(!uz_zip_extract_store(&reader, &record, 0,
                                 scratch, sizeof(scratch)));
    assert(reader.error == UZ_ZIP_READ_LOCAL);

    build_valid();
    begin_and_next(&reader, &record, scratch);
    archive[35u] ^= 0x80u;
    assert(!uz_zip_extract_store(&reader, &record, 0,
                                 scratch, sizeof(scratch)));
    assert(reader.error == UZ_ZIP_READ_CRC);

    build_valid();
    descriptor = find_signature(0x07u, 0x08u);
    begin_and_next(&reader, &record, scratch);
    archive[descriptor + 4u] ^= 0x01u;
    assert(!uz_zip_extract_store(&reader, &record, 0,
                                 scratch, sizeof(scratch)));
    assert(reader.error == UZ_ZIP_READ_DESCRIPTOR);
    return 0;
}
