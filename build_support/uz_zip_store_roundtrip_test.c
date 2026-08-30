#include <assert.h>
#include <string.h>

#include "uz_zip_read.h"

static UzDos archive_output;
static UzDos archive_input;
static UzDos extracted_output;
static unsigned char archive[1024];
static unsigned int archive_len;
static unsigned int input_pos;
static unsigned char extracted[64];
static unsigned int extracted_len;

static unsigned char archive_read_at(void *context, const UzU32 *offset,
                                     unsigned char *destination,
                                     unsigned int length) {
    unsigned long at;

    (void)context;
    at = (unsigned long)offset->lo | ((unsigned long)offset->hi << 16u);
    if (at > archive_len || length > archive_len - at) return 0u;
    memcpy(destination, archive + at, length);
    return 1u;
}

unsigned char uz_dos_write(UzDos *dos, const void *source,
                           unsigned int length) {
    if (dos == &archive_output) {
        assert(archive_len + length <= sizeof(archive));
        memcpy(archive + archive_len, source, length);
        archive_len = (unsigned int)(archive_len + length);
        return 1u;
    }
    assert(dos == &extracted_output);
    assert(extracted_len + length <= sizeof(extracted));
    memcpy(extracted + extracted_len, source, length);
    extracted_len = (unsigned int)(extracted_len + length);
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

int main(void) {
    UzZipWriter writer;
    UzZipReader reader;
    UzZipRecord written[2];
    UzZipRecord parsed;
    UzZipRecord parsed_at;
    UzU32 central_offset;
    UzU32 archive_size;
    unsigned char scratch[512];

    memset(&archive_output, 0, sizeof(archive_output));
    memset(&archive_input, 0, sizeof(archive_input));
    memset(&extracted_output, 0, sizeof(extracted_output));
    uz_zip_writer_init(&writer, &archive_output);
    assert(uz_zip_begin_store(&writer, &written[0], "A.TXT", 0u));
    assert(uz_zip_store_data(&writer, (const unsigned char *)"abc", 3u));
    assert(uz_zip_finish_store(&writer));
    assert(uz_zip_begin_store(&writer, &written[1], "EMPTY/", 1u));
    assert(uz_zip_finish_store(&writer));
    central_offset = writer.offset;
    assert(uz_zip_emit_central(&writer, &written[0]));
    assert(uz_zip_emit_central(&writer, &written[1]));
    assert(uz_zip_finish_archive(&writer, &central_offset, 2u));

    uz_zip_reader_init(&reader, &archive_input);
    assert(uz_zip_reader_begin(&reader, scratch, sizeof(scratch)));
    assert(reader.entry_count == 2u);
    assert(uz_zip_reader_next(&reader, &parsed));
    assert(strcmp(parsed.name, "A.TXT") == 0);
    assert(uz_zip_extract_store(&reader, &parsed, &extracted_output,
                                scratch, sizeof(scratch)));
    assert(extracted_len == 3u && memcmp(extracted, "abc", 3u) == 0);
    assert(uz_zip_reader_next(&reader, &parsed));
    assert(strcmp(parsed.name, "EMPTY/") == 0 && parsed.directory);
    assert(uz_zip_extract_store(&reader, &parsed, 0, scratch, sizeof(scratch)));
    assert(uz_zip_reader_finished(&reader));

    archive_size.lo = archive_len;
    archive_size.hi = 0u;
    uz_zip_reader_init_at(&reader, &archive_size, archive_read_at, 0);
    assert(uz_zip_reader_begin(&reader, scratch, sizeof(scratch)));
    assert(uz_zip_reader_next(&reader, &parsed_at));
    assert(strcmp(parsed_at.name, "A.TXT") == 0 &&
           parsed_at.method == 0u && !parsed_at.directory &&
           parsed_at.size.lo == 3u && parsed_at.size.hi == 0u);
    extracted_len = 0u;
    assert(uz_zip_extract_store(&reader, &parsed_at, &extracted_output,
                                scratch, sizeof(scratch)));
    assert(extracted_len == 3u && memcmp(extracted, "abc", 3u) == 0);
    assert(uz_zip_reader_next(&reader, &parsed_at));
    assert(strcmp(parsed_at.name, "EMPTY/") == 0 && parsed_at.directory);
    assert(uz_zip_extract_store(&reader, &parsed_at, 0,
                                scratch, sizeof(scratch)));
    assert(uz_zip_reader_finished(&reader));
    return 0;
}
