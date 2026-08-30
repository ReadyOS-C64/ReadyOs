#include "uz_zip_write.h"

#include <string.h>

#ifdef UZIP_READYOS_APP
#pragma code-name(push, "JOB_CODE")
#endif

static void put16(unsigned char *out, unsigned int value) {
    out[0] = (unsigned char)value;
    out[1] = (unsigned char)(value >> 8u);
}

static void put32_bytes(unsigned char *out, const unsigned char value[4]) {
    out[0] = value[0];
    out[1] = value[1];
    out[2] = value[2];
    out[3] = value[3];
}

static void put32_constant(unsigned char *out,
                           unsigned int low, unsigned int high) {
    out[0] = (unsigned char)low;
    out[1] = (unsigned char)(low >> 8u);
    out[2] = (unsigned char)high;
    out[3] = (unsigned char)(high >> 8u);
}

static unsigned char emit(UzZipWriter *writer, const unsigned char *data,
                          unsigned int length) {
    unsigned int chunk;

    while (length != 0u) {
        chunk = length;
        if (chunk > UZ_DOS_WRITE_MAX) chunk = UZ_DOS_WRITE_MAX;
        if (!uz_dos_write(writer->output, data, chunk)) {
            writer->error = UZ_ZIP_ERR_WRITE;
            return 0u;
        }
        uz_u32_add_u16(&writer->offset, chunk);
        data += chunk;
        length = (unsigned int)(length - chunk);
    }
    return 1u;
}

static unsigned char safe_component(const char *start, unsigned int length) {
    return (unsigned char)(length != 0u &&
        !(length == 1u && (unsigned char)start[0] == 0x2Eu) &&
        !(length == 2u && (unsigned char)start[0] == 0x2Eu &&
          (unsigned char)start[1] == 0x2Eu));
}

unsigned char uz_zip_name_safe(const char *name, unsigned char directory) {
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
            if (!safe_component(component, component_len)) return 0u;
            component = name;
            component_len = 0u;
        } else {
            ++component_len;
        }
    }
    if (directory) return (unsigned char)(total != 0u &&
                                         (unsigned char)name[-1] == 0x2Fu);
    return (unsigned char)((unsigned char)name[-1] != 0x2Fu &&
                          safe_component(component, component_len));
}

void uz_zip_writer_init(UzZipWriter *writer, UzDos *output) {
    memset(writer, 0, sizeof(*writer));
    writer->output = output;
}

unsigned char uz_zip_begin_streamed(UzZipWriter *writer, UzZipRecord *record,
                                    const char *name, unsigned char directory,
                                    unsigned int method) {
    unsigned char header[30];
    unsigned int name_len;

    if (writer == 0 || writer->output == 0 || record == 0 ||
        writer->active != 0) {
        if (writer != 0) writer->error = UZ_ZIP_ERR_STATE;
        return 0u;
    }
    if (!uz_zip_name_safe(name, directory)) {
        writer->error = UZ_ZIP_ERR_NAME;
        return 0u;
    }
    name_len = strlen(name);
    memset(record, 0, sizeof(*record));
    strcpy(record->name, name);
    record->local_offset = writer->offset;
    record->flags = 0x0008u;
    record->directory = directory;
    record->method = method;
    uz_crc32_init(&record->crc);

    memset(header, 0, sizeof(header));
    put32_constant(header, 0x4B50u, 0x0403u);
    put16(header + 4u, 20u);
    put16(header + 6u, 0x0008u); /* descriptor follows streamed data */
    put16(header + 8u, method);
    put16(header + 26u, name_len);
    if (!emit(writer, header, sizeof(header)) ||
        !emit(writer, (const unsigned char *)name, name_len)) return 0u;
    writer->active = record;
    return 1u;
}

unsigned char uz_zip_store_data(UzZipWriter *writer,
                                const unsigned char *data,
                                unsigned int length) {
    if (writer == 0 || writer->active == 0 || writer->active->directory) {
        if (writer != 0) writer->error = UZ_ZIP_ERR_STATE;
        return 0u;
    }
    if (!emit(writer, data, length)) return 0u;
    uz_crc32_update(&writer->active->crc, data, length);
    uz_u32_add_u16(&writer->active->size, length);
    uz_u32_add_u16(&writer->active->compressed_size, length);
    return 1u;
}

static unsigned char finish_streamed(UzZipWriter *writer) {
    unsigned char descriptor[16];
    UzZipRecord *record;

    /* Public finish functions validate the writer before entering this shared
     * descriptor emitter; do not duplicate the saturated Store-phase guard. */
    record = writer->active;
    put32_constant(descriptor, 0x4B50u, 0x0807u);
    put32_bytes(descriptor + 4u, record->crc.byte);
    uz_u32_to_le(descriptor + 8u, &record->compressed_size);
    uz_u32_to_le(descriptor + 12u, &record->size);
    writer->active = 0;
    return emit(writer, descriptor, sizeof(descriptor));
}

unsigned char uz_zip_finish_store(UzZipWriter *writer) {
    if (writer == 0 || writer->active == 0 ||
        writer->active->method != 0u) {
        if (writer != 0) writer->error = UZ_ZIP_ERR_STATE;
        return 0u;
    }
    uz_crc32_finish(&writer->active->crc);
    return finish_streamed(writer);
}

unsigned char uz_zip_finish_deflate(UzZipWriter *writer) {
    if (writer == 0 || writer->active == 0) {
        if (writer != 0) writer->error = UZ_ZIP_ERR_STATE;
        return 0u;
    }
    uz_u32_add(&writer->offset, &writer->active->compressed_size);
    return finish_streamed(writer);
}

unsigned char uz_zip_emit_central(UzZipWriter *writer,
                                  const UzZipRecord *record) {
    unsigned char header[46];
    unsigned int name_len;

    /* Records cross this overlay only after uz_catalog accepted them. Keeping
     * validation at that boundary preserves Store's measured 5K contract. */
    name_len = strlen(record->name);
    memset(header, 0, sizeof(header));
    put32_constant(header, 0x4B50u, 0x0201u);
    put16(header + 4u, 20u);
    put16(header + 6u, 20u);
    put16(header + 8u, record->flags);
    put16(header + 10u, record->method);
    put32_bytes(header + 16u, record->crc.byte);
    uz_u32_to_le(header + 20u, &record->compressed_size);
    uz_u32_to_le(header + 24u, &record->size);
    put16(header + 28u, name_len);
    if (record->directory) header[38] = 0x10u;
    uz_u32_to_le(header + 42u, &record->local_offset);
    return (unsigned char)(emit(writer, header, sizeof(header)) &&
        emit(writer, (const unsigned char *)record->name, name_len));
}

unsigned char uz_zip_finish_archive(UzZipWriter *writer,
                                    const UzU32 *central_offset,
                                    unsigned int count) {
    unsigned char eocd[22];
    UzU32 central_size;

    if (writer == 0 || central_offset == 0 || writer->active != 0) {
        if (writer != 0) writer->error = UZ_ZIP_ERR_STATE;
        return 0u;
    }
    central_size = writer->offset;
    uz_u32_sub(&central_size, central_offset);
    memset(eocd, 0, sizeof(eocd));
    put32_constant(eocd, 0x4B50u, 0x0605u);
    put16(eocd + 8u, count);
    put16(eocd + 10u, count);
    uz_u32_to_le(eocd + 12u, &central_size);
    uz_u32_to_le(eocd + 16u, central_offset);
    return emit(writer, eocd, sizeof(eocd));
}

#ifdef UZIP_READYOS_APP
#pragma code-name(pop)
#endif
