#include <assert.h>
#include <string.h>

#include "uz_catalog.h"

static unsigned char bank[UZ_CATALOG_MAX_ENTRIES * sizeof(UzZipRecord)];
static unsigned char fail_io;

static unsigned char write_bank(void *context, unsigned int offset,
                                const void *source, unsigned int length) {
    (void)context;
    if (fail_io || offset > sizeof(bank) || length > sizeof(bank) - offset)
        return 0u;
    memcpy(bank + offset, source, length);
    return 1u;
}

static unsigned char read_bank(void *context, unsigned int offset,
                               void *destination, unsigned int length) {
    (void)context;
    if (fail_io || offset > sizeof(bank) || length > sizeof(bank) - offset)
        return 0u;
    memcpy(destination, bank + offset, length);
    return 1u;
}

int main(void) {
    UzCatalog catalog;
    UzZipRecord record;
    UzZipRecord fetched;
    unsigned int index;
    UzCatalog unique;

    memset(bank, 0xA5, sizeof(bank));
    memset(&record, 0, sizeof(record));
    strcpy(record.name, "NESTED/A.BIN");
    record.method = 8u;
    record.size.lo = 1234u;
    uz_catalog_init(&catalog, write_bank, read_bank, 0);
    assert(catalog.error == UZ_CATALOG_OK && catalog.count == 0u);
    assert(uz_catalog_append(&catalog, &record));
    assert(catalog.count == 1u);
    memset(&fetched, 0, sizeof(fetched));
    assert(uz_catalog_get(&catalog, 0u, &fetched));
    assert(memcmp(&record, &fetched, sizeof(record)) == 0);

    fetched.compressed_size.lo = 321u;
    assert(uz_catalog_update(&catalog, 0u, &fetched));
    memset(&record, 0, sizeof(record));
    assert(uz_catalog_get(&catalog, 0u, &record));
    assert(record.compressed_size.lo == 321u);
    assert(!uz_catalog_get(&catalog, 1u, &record));
    assert(catalog.error == UZ_CATALOG_ERR_INDEX);

    memset(&record, 0, sizeof(record));
    strcpy(record.name, "BAD/../NAME");
    record.method = 8u;
    assert(!uz_catalog_append(&catalog, &record));
    assert(catalog.error == UZ_CATALOG_ERR_RECORD && catalog.count == 1u);
    memset(&record, 0, sizeof(record));
    strcpy(record.name, "NESTED/A.BIN");
    record.method = 8u;

    for (index = 1u; index < UZ_CATALOG_MAX_ENTRIES; ++index)
        assert(uz_catalog_append(&catalog, &record));
    assert(catalog.count == UZ_CATALOG_MAX_ENTRIES);
    assert(!uz_catalog_append(&catalog, &record));
    assert(catalog.error == UZ_CATALOG_ERR_FULL);

    fail_io = 1u;
    assert(!uz_catalog_get(&catalog, 0u, &record));
    assert(catalog.error == UZ_CATALOG_ERR_IO);
    fail_io = 0u;

    memset(bank, 0, sizeof(bank));
    uz_catalog_init(&unique, write_bank, read_bank, 0);
    memset(&record, 0, sizeof(record));
    strcpy(record.name, "ROOT/");
    record.directory = 1u;
    assert(uz_catalog_append_unique(&unique, &record, &fetched));
    memset(&record, 0, sizeof(record));
    strcpy(record.name, "root/FILE.BIN");
    record.method = 8u;
    assert(uz_catalog_append_unique(&unique, &record, &fetched));
    strcpy(record.name, "ROOT/file.bin");
    assert(!uz_catalog_append_unique(&unique, &record, &fetched));
    assert(unique.error == UZ_CATALOG_ERR_CONFLICT && unique.count == 2u);
    strcpy(record.name, "ROOT");
    assert(!uz_catalog_append_unique(&unique, &record, &fetched));
    assert(unique.error == UZ_CATALOG_ERR_CONFLICT && unique.count == 2u);
    strcpy(record.name, "TOP");
    assert(uz_catalog_append_unique(&unique, &record, &fetched));
    strcpy(record.name, "top/CHILD.BIN");
    assert(!uz_catalog_append_unique(&unique, &record, &fetched));
    assert(unique.error == UZ_CATALOG_ERR_CONFLICT && unique.count == 3u);
    fail_io = 1u;
    strcpy(record.name, "OTHER.BIN");
    assert(!uz_catalog_append_unique(&unique, &record, &fetched));
    assert(unique.error == UZ_CATALOG_ERR_IO && unique.count == 3u);
    fail_io = 0u;

    uz_catalog_init(&catalog, 0, read_bank, 0);
    assert(catalog.error == UZ_CATALOG_ERR_STATE);
    assert(!uz_catalog_append(&catalog, &record));
    return 0;
}
