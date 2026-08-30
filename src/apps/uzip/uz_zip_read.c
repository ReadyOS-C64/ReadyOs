#include "uz_zip_read.h"

#include <string.h>

#ifdef UZIP_READYOS_APP
#ifdef UZ_ZIP_READ_PARSER_ONLY
#pragma code-name(push, "ZIP_READ_CODE")
#else
#pragma code-name(push, "JOB_CODE")
#endif
#endif

static unsigned int get16(const unsigned char *data) {
    return (unsigned int)(data[0] | ((unsigned int)data[1] << 8u));
}

static unsigned char sig_is(const unsigned char *data,
                            unsigned char a, unsigned char b) {
    return (unsigned char)(data[0] == 0x50u && data[1] == 0x4Bu &&
                          data[2] == a && data[3] == b);
}

static unsigned char read_bytes_equal(const unsigned char *left,
                                      const unsigned char *right,
                                      unsigned int length) {
    while (length != 0u) {
        if (*left++ != *right++) return 0u;
        --length;
    }
    return 1u;
}

static unsigned char read_name_equal(const unsigned char *left,
                                     const char *right) {
    while (*left != 0u && *right != 0) {
        if (*left++ != (unsigned char)*right++) return 0u;
    }
    return (unsigned char)(*left == 0u && *right == 0);
}

static unsigned char read_component_safe(const char *start,
                                         unsigned int length) {
    return (unsigned char)(length != 0u &&
        !(length == 1u && (unsigned char)start[0] == 0x2Eu) &&
        !(length == 2u && (unsigned char)start[0] == 0x2Eu &&
          (unsigned char)start[1] == 0x2Eu));
}

/* This phase-local validator prevents the parser overlay from calling the
 * Store image after `$B000` has been replaced. Keep its accepted byte set in
 * lockstep with uz_zip_name_safe and the catalog preflight. */
static unsigned char read_name_safe(const char *name,
                                    unsigned char directory) {
    const char *component;
    unsigned int component_len;
    unsigned int total;
    unsigned char value;

    if (name == 0 || name[0] == 0 || (unsigned char)name[0] == 0x2Fu ||
        (unsigned char)name[0] == 0x5Cu) return 0u;
    component = name;
    component_len = 0u;
    total = 0u;
    while (*name != 0) {
        value = (unsigned char)*name++;
        ++total;
        if (total >= UZ_ZIP_NAME_CAP || value < 0x20u || value > 0x7Eu ||
            value == 0x5Cu || value == 0x3Au) return 0u;
        if (value == 0x2Fu) {
            if (!read_component_safe(component, component_len)) return 0u;
            component = name;
            component_len = 0u;
        } else {
            ++component_len;
        }
    }
    if (directory)
        return (unsigned char)((unsigned char)name[-1] == 0x2Fu);
    return (unsigned char)((unsigned char)name[-1] != 0x2Fu &&
                           read_component_safe(component, component_len));
}

static unsigned char read_exact(UzZipReader *reader, unsigned char *data,
                                unsigned int length) {
    unsigned int chunk;
#ifndef UZ_ZIP_READ_CALLBACK_ONLY
    int got;
#endif

    while (length != 0u) {
        chunk = length;
        if (chunk > 512u) chunk = 512u;
#ifdef UZ_ZIP_READ_CALLBACK_ONLY
        if (!reader->read_at(reader->read_context,
                             &reader->read_cursor, data, chunk)) {
            reader->error = UZ_ZIP_READ_IO;
            return 0u;
        }
        uz_u32_add_u16(&reader->read_cursor, chunk);
#else
        if (reader->read_at != 0) {
            if (!reader->read_at(reader->read_context,
                                 &reader->read_cursor, data, chunk)) {
                reader->error = UZ_ZIP_READ_IO;
                return 0u;
            }
            uz_u32_add_u16(&reader->read_cursor, chunk);
        } else {
            got = uz_dos_read(reader->input, data, chunk);
            if (got <= 0 || (unsigned int)got != chunk) {
                reader->error = UZ_ZIP_READ_IO;
                return 0u;
            }
        }
#endif
        data += chunk;
        length = (unsigned int)(length - chunk);
    }
    return 1u;
}

static unsigned char seek_read(UzZipReader *reader, const UzU32 *offset,
                               unsigned char *data, unsigned int length) {
    reader->read_cursor = *offset;
#ifndef UZ_ZIP_READ_CALLBACK_ONLY
    if (reader->read_at == 0 && !uz_dos_seek(reader->input, offset)) {
        reader->error = UZ_ZIP_READ_IO;
        return 0u;
    }
#endif
    return read_exact(reader, data, length);
}

static unsigned char u32_at_most(const UzU32 *left, const UzU32 *right) {
    return (unsigned char)(uz_u32_equal(left, right) || uz_u32_less(left, right));
}

static unsigned int low_chunk(const UzU32 *value, unsigned int maximum) {
    if (value->hi != 0u || value->lo > maximum) return maximum;
    return value->lo;
}

static unsigned char validate_eocd(UzZipReader *reader,
                                   const UzU32 *candidate,
                                   unsigned char header[22]) {
    UzU32 end;
    UzU32 central_end;
    unsigned int comment_len;
    unsigned int entries;

    if (!seek_read(reader, candidate, header, 22u)) return 0u;
    if (!sig_is(header, 0x05u, 0x06u)) return 0u;
    comment_len = get16(header + 20u);
    end = *candidate;
    uz_u32_add_u16(&end, (unsigned int)(22u + comment_len));
    if (!uz_u32_equal(&end, &reader->archive_size)) return 0u;
    if (get16(header + 4u) != 0u || get16(header + 6u) != 0u ||
        get16(header + 8u) != get16(header + 10u)) {
        reader->error = UZ_ZIP_READ_UNSUPPORTED;
        return 0u;
    }
    entries = get16(header + 10u);
    uz_u32_from_le(&reader->central_size, header + 12u);
    uz_u32_from_le(&reader->central_offset, header + 16u);
    if (entries == 0xFFFFu ||
        (reader->central_size.lo == 0xFFFFu && reader->central_size.hi == 0xFFFFu) ||
        (reader->central_offset.lo == 0xFFFFu && reader->central_offset.hi == 0xFFFFu)) {
        reader->error = UZ_ZIP_READ_UNSUPPORTED;
        return 0u;
    }
    central_end = reader->central_offset;
    uz_u32_add(&central_end, &reader->central_size);
    if (!u32_at_most(&central_end, candidate)) {
        reader->error = UZ_ZIP_READ_BOUNDS;
        return 0u;
    }
    reader->eocd_offset = *candidate;
    reader->central_end = central_end;
    reader->central_cursor = reader->central_offset;
    reader->entry_count = entries;
    reader->entry_index = 0u;
    reader->error = UZ_ZIP_READ_OK;
    return 1u;
}

#ifndef UZ_ZIP_READ_CALLBACK_ONLY
void uz_zip_reader_init(UzZipReader *reader, UzDos *input) {
    memset(reader, 0, sizeof(*reader));
    reader->input = input;
}
#endif

void uz_zip_reader_init_at(UzZipReader *reader, const UzU32 *archive_size,
                           UzZipReadAt read_at, void *read_context) {
    memset(reader, 0, sizeof(*reader));
    if (archive_size != 0) reader->archive_size = *archive_size;
    reader->read_at = read_at;
    reader->read_context = read_context;
}

unsigned char uz_zip_reader_begin(UzZipReader *reader,
                                  unsigned char *scratch,
                                  unsigned int scratch_size) {
    UzU32 end;
    UzU32 start;
    UzU32 scan_left;
    UzU32 candidate;
    unsigned char header[22];
    unsigned int block;
    unsigned int index;
    unsigned int advance;

#ifdef UZ_ZIP_READ_CALLBACK_ONLY
    if (reader == 0 || reader->read_at == 0 ||
#else
    if (reader == 0 ||
        (reader->input == 0 && reader->read_at == 0) ||
#endif
        scratch == 0 || scratch_size < 64u) {
        if (reader != 0) reader->error = UZ_ZIP_READ_STATE;
        return 0u;
    }
#ifndef UZ_ZIP_READ_CALLBACK_ONLY
    if (reader->read_at == 0) {
        if (!uz_dos_file_info(reader->input, &reader->archive_size)) {
            reader->error = UZ_ZIP_READ_IO;
            return 0u;
        }
    }
#endif
    if (reader->archive_size.hi == 0u && reader->archive_size.lo < 22u) {
        reader->error = UZ_ZIP_READ_NO_EOCD;
        return 0u;
    }
    end = reader->archive_size;
    scan_left = reader->archive_size;
    if (scan_left.hi > 1u || (scan_left.hi == 1u && scan_left.lo > 0x0015u)) {
        scan_left.hi = 1u;
        scan_left.lo = 0x0015u; /* 65,535-byte comment plus 22-byte EOCD */
    }
    if (scratch_size > 512u) scratch_size = 512u;
    while (scan_left.hi != 0u || scan_left.lo != 0u) {
        block = low_chunk(&scan_left, scratch_size);
        if (block < 4u) break;
        start = end;
        uz_u32_sub_u16(&start, block);
        if (!seek_read(reader, &start, scratch, block)) return 0u;
        index = (unsigned int)(block - 4u);
        for (;;) {
            if (sig_is(scratch + index, 0x05u, 0x06u)) {
                candidate = start;
                uz_u32_add_u16(&candidate, index);
                reader->error = UZ_ZIP_READ_OK;
                if (validate_eocd(reader, &candidate, header)) return 1u;
                if (reader->error != UZ_ZIP_READ_OK) return 0u;
            }
            if (index == 0u) break;
            --index;
        }
        if ((scan_left.hi == 0u && scan_left.lo <= block) || block <= 4u) break;
        advance = (unsigned int)(block - 3u);
        uz_u32_sub_u16(&scan_left, advance);
        end = start;
        uz_u32_add_u16(&end, 3u);
    }
    reader->error = UZ_ZIP_READ_NO_EOCD;
    return 0u;
}

unsigned char uz_zip_reader_next(UzZipReader *reader, UzZipRecord *record) {
    unsigned char header[46];
    UzU32 next;
    unsigned int name_len;
    unsigned int extra_len;
    unsigned int comment_len;
    unsigned char directory;
    unsigned int method;

    if (reader->entry_index >= reader->entry_count) {
        reader->error = UZ_ZIP_READ_STATE;
        return 0u;
    }
    if (!seek_read(reader, &reader->central_cursor, header, sizeof(header))) return 0u;
    method = get16(header + 10u);
    if (!sig_is(header, 0x01u, 0x02u) ||
        (method != 0u && method != 8u) ||
        (get16(header + 8u) & (unsigned int)~0x0808u) != 0u ||
        (header[5] == 3u && (header[41] & 0xF0u) == 0xA0u)) {
        reader->error = UZ_ZIP_READ_UNSUPPORTED;
        return 0u;
    }
    name_len = get16(header + 28u);
    extra_len = get16(header + 30u);
    comment_len = get16(header + 32u);
    if (name_len == 0u || name_len >= UZ_ZIP_NAME_CAP) {
        reader->error = UZ_ZIP_READ_NAME;
        return 0u;
    }
    memset(record, 0, sizeof(*record));
    if (!read_exact(reader, (unsigned char *)record->name, name_len)) return 0u;
    record->name[name_len] = 0;
    directory = (unsigned char)((unsigned char)record->name[name_len - 1u] == 0x2Fu);
    if (!read_name_safe(record->name, directory)) {
        reader->error = UZ_ZIP_READ_NAME;
        return 0u;
    }
    record->directory = directory;
    record->flags = get16(header + 8u);
    record->method = method;
    memcpy(record->crc.byte, header + 16u, 4u);
    uz_u32_from_le(&record->compressed_size, header + 20u);
    uz_u32_from_le(&record->size, header + 24u);
    uz_u32_from_le(&record->local_offset, header + 42u);
    if ((method == 0u && !uz_u32_equal(&record->size,
                                       &record->compressed_size)) ||
        !uz_u32_less(&record->local_offset, &reader->central_offset) ||
        (record->size.lo == 0xFFFFu && record->size.hi == 0xFFFFu) ||
        (record->compressed_size.lo == 0xFFFFu &&
         record->compressed_size.hi == 0xFFFFu)) {
        reader->error = UZ_ZIP_READ_UNSUPPORTED;
        return 0u;
    }
    if (record->directory &&
        (record->method != 0u || record->size.lo != 0u ||
         record->size.hi != 0u ||
         record->crc.byte[0] != 0u || record->crc.byte[1] != 0u ||
         record->crc.byte[2] != 0u || record->crc.byte[3] != 0u)) {
        reader->error = UZ_ZIP_READ_UNSUPPORTED;
        return 0u;
    }
    next = reader->central_cursor;
    uz_u32_add_u16(&next, (unsigned int)(46u + name_len));
    uz_u32_add_u16(&next, extra_len);
    uz_u32_add_u16(&next, comment_len);
    if (!u32_at_most(&next, &reader->central_end)) {
        reader->error = UZ_ZIP_READ_BOUNDS;
        return 0u;
    }
    reader->central_cursor = next;
    ++reader->entry_index;
    reader->error = UZ_ZIP_READ_OK;
    return 1u;
}

unsigned char uz_zip_reader_finished(UzZipReader *reader) {
    if (reader->entry_index != reader->entry_count ||
        !uz_u32_equal(&reader->central_cursor, &reader->central_end)) {
        reader->error = UZ_ZIP_READ_CENTRAL;
        return 0u;
    }
    return 1u;
}

static unsigned char descriptor_matches(const unsigned char *descriptor,
                                        unsigned int start,
                                        const UzZipRecord *record) {
    UzU32 compressed;
    UzU32 size;

    if (!read_bytes_equal(descriptor + start, record->crc.byte, 4u)) return 0u;
    uz_u32_from_le(&compressed, descriptor + start + 4u);
    uz_u32_from_le(&size, descriptor + start + 8u);
    return (unsigned char)(uz_u32_equal(&compressed, &record->compressed_size) &&
                           uz_u32_equal(&size, &record->size));
}

unsigned char uz_zip_reader_local(UzZipReader *reader,
                                  const UzZipRecord *record,
                                  UzU32 *data_offset,
                                  unsigned char *scratch,
                                  unsigned int scratch_size) {
    unsigned char local[30];
    unsigned char descriptor[16];
    UzU32 descriptor_offset;
    UzU32 end;
    UzU32 local_compressed;
    UzU32 local_size;
    unsigned int name_len;
    unsigned int extra_len;
    unsigned int descriptor_start;

    if (reader == 0 || record == 0 || data_offset == 0 || scratch == 0 ||
        scratch_size < UZ_ZIP_NAME_CAP) {
        if (reader != 0) reader->error = UZ_ZIP_READ_STATE;
        return 0u;
    }
    if (!seek_read(reader, &record->local_offset, local, sizeof(local))) return 0u;
    if (!sig_is(local, 0x03u, 0x04u) ||
        get16(local + 8u) != record->method ||
        get16(local + 6u) != record->flags) {
        reader->error = UZ_ZIP_READ_LOCAL;
        return 0u;
    }
    name_len = get16(local + 26u);
    extra_len = get16(local + 28u);
    if (name_len == 0u || name_len >= UZ_ZIP_NAME_CAP ||
        !read_exact(reader, scratch, name_len)) {
        reader->error = UZ_ZIP_READ_LOCAL;
        return 0u;
    }
    scratch[name_len] = 0u;
    if (!read_name_equal(scratch, record->name)) {
        reader->error = UZ_ZIP_READ_NAME;
        return 0u;
    }
    *data_offset = record->local_offset;
    uz_u32_add_u16(data_offset, (unsigned int)(30u + name_len));
    uz_u32_add_u16(data_offset, extra_len);
    if (uz_u32_less(data_offset, &record->local_offset)) {
        reader->error = UZ_ZIP_READ_BOUNDS;
        return 0u;
    }
    descriptor_offset = *data_offset;
    uz_u32_add(&descriptor_offset, &record->compressed_size);
    if (uz_u32_less(&descriptor_offset, data_offset) ||
        !u32_at_most(&descriptor_offset, &reader->central_offset)) {
        reader->error = UZ_ZIP_READ_BOUNDS;
        return 0u;
    }
    if ((record->flags & 0x0008u) == 0u) {
        uz_u32_from_le(&local_compressed, local + 18u);
        uz_u32_from_le(&local_size, local + 22u);
        if (!read_bytes_equal(local + 14u, record->crc.byte, 4u) ||
            !uz_u32_equal(&local_compressed, &record->compressed_size) ||
            !uz_u32_equal(&local_size, &record->size)) {
            reader->error = UZ_ZIP_READ_LOCAL;
            return 0u;
        }
        reader->error = UZ_ZIP_READ_OK;
        return 1u;
    }
    end = descriptor_offset;
    uz_u32_add_u16(&end, 12u);
    if (uz_u32_less(&end, &descriptor_offset) ||
        !u32_at_most(&end, &reader->central_offset)) {
        reader->error = UZ_ZIP_READ_BOUNDS;
        return 0u;
    }
    if (!seek_read(reader, &descriptor_offset, descriptor, 16u)) return 0u;
    descriptor_start = 0xFFFFu;
    if (sig_is(descriptor, 0x07u, 0x08u) &&
        descriptor_matches(descriptor, 4u, record)) descriptor_start = 4u;
    if (descriptor_start == 0xFFFFu &&
        descriptor_matches(descriptor, 0u, record)) descriptor_start = 0u;
    if (descriptor_start == 0xFFFFu) {
        reader->error = UZ_ZIP_READ_DESCRIPTOR;
        return 0u;
    }
    end = descriptor_offset;
    uz_u32_add_u16(&end, (unsigned int)(descriptor_start + 12u));
    if (uz_u32_less(&end, &descriptor_offset) ||
        !u32_at_most(&end, &reader->central_offset)) {
        reader->error = UZ_ZIP_READ_BOUNDS;
        return 0u;
    }
    reader->error = UZ_ZIP_READ_OK;
    return 1u;
}

#ifndef UZ_ZIP_READ_PARSER_ONLY
static unsigned char write_output(UzZipReader *reader, UzDos *output,
                                  const unsigned char *data,
                                  unsigned int length) {
    unsigned int chunk;

    if (output == 0) return 1u;
    while (length != 0u) {
        chunk = length;
        if (chunk > UZ_DOS_WRITE_MAX) chunk = UZ_DOS_WRITE_MAX;
        if (!uz_dos_write(output, data, chunk)) {
            reader->error = UZ_ZIP_READ_IO;
            return 0u;
        }
        data += chunk;
        length = (unsigned int)(length - chunk);
    }
    return 1u;
}

unsigned char uz_zip_extract_store(UzZipReader *reader,
                                   const UzZipRecord *record,
                                   UzDos *output,
                                   unsigned char *scratch,
                                   unsigned int scratch_size) {
    UzU32 data_offset;
    UzU32 remaining;
    UzCrc32 crc;
    unsigned int chunk;

    if (record->method != 0u) {
        reader->error = UZ_ZIP_READ_UNSUPPORTED;
        return 0u;
    }
    if (!uz_zip_reader_local(reader, record, &data_offset,
                             scratch, scratch_size)) return 0u;
    reader->read_cursor = data_offset;
#ifndef UZ_ZIP_READ_CALLBACK_ONLY
    if (reader->read_at == 0 && !uz_dos_seek(reader->input, &data_offset)) {
        reader->error = UZ_ZIP_READ_IO;
        return 0u;
    }
#endif
    remaining = record->compressed_size;
    uz_crc32_init(&crc);
    if (scratch_size > 512u) scratch_size = 512u;
    while (remaining.hi != 0u || remaining.lo != 0u) {
        chunk = low_chunk(&remaining, scratch_size);
        if (!read_exact(reader, scratch, chunk) ||
            !write_output(reader, output, scratch, chunk)) return 0u;
        uz_crc32_update(&crc, scratch, chunk);
        uz_u32_sub_u16(&remaining, chunk);
    }
    uz_crc32_finish(&crc);
    if (!uz_crc32_equal(&crc, &record->crc)) {
        reader->error = UZ_ZIP_READ_CRC;
        return 0u;
    }
    reader->error = UZ_ZIP_READ_OK;
    return 1u;
}
#endif

#ifdef UZIP_READYOS_APP
#pragma code-name(pop)
#endif
