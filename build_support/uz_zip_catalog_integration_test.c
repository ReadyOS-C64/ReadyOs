#include <assert.h>
#include <string.h>

#include "uz_catalog.h"

#define ENTRY_COUNT 7u

static unsigned char archive[2048];
static unsigned int archive_len;
static unsigned char bank[UZ_CATALOG_MAX_ENTRIES * sizeof(UzZipRecord)];

unsigned char uz_dos_write(UzDos *dos, const void *source,
                           unsigned int length) {
    (void)dos;
    if (length > sizeof(archive) - archive_len) return 0u;
    memcpy(archive + archive_len, source, length);
    archive_len = (unsigned int)(archive_len + length);
    return 1u;
}

static unsigned char bank_write(void *context, unsigned int offset,
                                const void *source, unsigned int length) {
    (void)context;
    if (offset > sizeof(bank) || length > sizeof(bank) - offset) return 0u;
    memcpy(bank + offset, source, length);
    return 1u;
}

static unsigned char bank_read(void *context, unsigned int offset,
                               void *destination, unsigned int length) {
    (void)context;
    if (offset > sizeof(bank) || length > sizeof(bank) - offset) return 0u;
    memcpy(destination, bank + offset, length);
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

static void append(UzCatalog *catalog, const UzZipRecord *record) {
    assert(uz_catalog_append(catalog, record));
}

int main(void) {
    static const char *names[ENTRY_COUNT] = {
        "ROOT/", "ROOT/EMPTY.BIN", "ROOT/REPEAT.BIN", "ROOT/SUB/",
        "ROOT/SUB/RANDOM.BIN", "ROOT/REPEAT.STO", "CROSS.BIN"
    };
    static const unsigned int methods[ENTRY_COUNT] = {
        0u, 8u, 8u, 0u, 8u, 0u, 8u
    };
    static const unsigned char directories[ENTRY_COUNT] = {
        1u, 0u, 0u, 1u, 0u, 0u, 0u
    };
    UzDos output;
    UzZipWriter writer;
    UzZipRecord record;
    UzCatalog catalog;
    UzU32 central_offset;
    unsigned int index;
    unsigned int cursor;
    unsigned int name_len;
    unsigned int eocd;
    unsigned long local_offsets[ENTRY_COUNT];

    memset(&output, 0, sizeof(output));
    memset(bank, 0xA5, sizeof(bank));
    uz_catalog_init(&catalog, bank_write, bank_read, 0);
    uz_zip_writer_init(&writer, &output);

    for (index = 0u; index < ENTRY_COUNT; ++index) {
        local_offsets[index] = writer.offset.lo;
        if (methods[index] == 8u) {
            assert(uz_zip_begin_deflate(&writer, &record, names[index]));
            assert(uz_dos_write(&output, "\003\000", 2u));
            record.compressed_size.lo = 2u;
            assert(uz_zip_finish_deflate(&writer));
        } else {
            assert(uz_zip_begin_store(&writer, &record, names[index],
                                      directories[index]));
            if (!directories[index]) {
                assert(uz_zip_store_data(&writer,
                    (const unsigned char *)"abc", 3u));
            }
            assert(uz_zip_finish_store(&writer));
        }
        append(&catalog, &record);
    }

    assert(catalog.count == ENTRY_COUNT);
    central_offset = writer.offset;
    for (index = 0u; index < catalog.count; ++index) {
        memset(&record, 0xCC, sizeof(record));
        assert(uz_catalog_get(&catalog, index, &record));
        assert(strcmp(record.name, names[index]) == 0);
        assert(record.method == methods[index]);
        assert(uz_zip_emit_central(&writer, &record));
    }
    assert(uz_zip_finish_archive(&writer, &central_offset, catalog.count));

    eocd = (unsigned int)(archive_len - 22u);
    assert(memcmp(archive + eocd, "PK\005\006", 4u) == 0);
    assert(get16((unsigned int)(eocd + 8u)) == ENTRY_COUNT);
    assert(get16((unsigned int)(eocd + 10u)) == ENTRY_COUNT);
    assert(get32((unsigned int)(eocd + 16u)) == central_offset.lo);
    cursor = central_offset.lo;
    for (index = 0u; index < ENTRY_COUNT; ++index) {
        name_len = strlen(names[index]);
        assert(memcmp(archive + cursor, "PK\001\002", 4u) == 0);
        assert(get16((unsigned int)(cursor + 10u)) == methods[index]);
        assert(get16((unsigned int)(cursor + 28u)) == name_len);
        assert((archive[cursor + 38u] & 0x10u) ==
               (directories[index] ? 0x10u : 0u));
        assert(get32((unsigned int)(cursor + 42u)) == local_offsets[index]);
        assert(memcmp(archive + cursor + 46u, names[index], name_len) == 0);
        cursor = (unsigned int)(cursor + 46u + name_len);
    }
    assert(cursor == eocd);
    return 0;
}
