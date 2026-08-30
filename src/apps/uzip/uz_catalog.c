#include "uz_catalog.h"

#include <string.h>

#ifdef __CC65__
#if defined(UZIP_CREATE_PLAN_OVERLAY)
#pragma code-name(push, "CREATE_PLAN_CODE")
#pragma rodata-name(push, "CREATE_PLAN_RODATA")
#elif defined(UZIP_CATALOG_UI)
#pragma code-name(push, "UI_CODE")
#pragma rodata-name(push, "UI_RODATA")
#elif defined(UZIP_READYOS_APP)
#pragma code-name(push, "DEFLATE_COORD_CODE")
#pragma rodata-name(push, "DEFLATE_COORD_RODATA")
#endif
#endif

static unsigned int record_offset(unsigned int index) {
    return (unsigned int)(index * (unsigned int)sizeof(UzZipRecord));
}

static unsigned char component_valid(const char *start, unsigned int length) {
    return (unsigned char)(length != 0u &&
        !(length == 1u && start[0] == '.') &&
        !(length == 2u && start[0] == '.' && start[1] == '.'));
}

static unsigned char name_valid(const char *name, unsigned char directory) {
    const char *component;
    unsigned int component_len;
    unsigned int total;
    unsigned char value;

    if (name == 0 || name[0] == 0 || name[0] == '/' || name[0] == '\\')
        return 0u;
    component = name;
    component_len = 0u;
    total = 0u;
    while (*name != 0) {
        value = (unsigned char)*name++;
        ++total;
        if (total >= UZ_ZIP_NAME_CAP || value < 0x20u || value > 0x7Eu ||
            value == '\\' || value == ':') return 0u;
        if (value == '/') {
            if (!component_valid(component, component_len)) return 0u;
            component = name;
            component_len = 0u;
        } else {
            ++component_len;
        }
    }
    if (directory) return (unsigned char)(name[-1] == '/');
    return (unsigned char)(name[-1] != '/' &&
                           component_valid(component, component_len));
}

static unsigned char record_valid(const UzZipRecord *record) {
    return (unsigned char)(record != 0 &&
        name_valid(record->name, record->directory) &&
        (record->method == 0u || record->method == 8u) &&
        (!record->directory ||
         (record->method == 0u && record->size.lo == 0u &&
          record->size.hi == 0u && record->compressed_size.lo == 0u &&
          record->compressed_size.hi == 0u && record->crc.byte[0] == 0u &&
          record->crc.byte[1] == 0u && record->crc.byte[2] == 0u &&
          record->crc.byte[3] == 0u)));
}

void uz_catalog_init(UzCatalog *catalog, UzCatalogWrite write,
                     UzCatalogRead read, void *context) {
    catalog->write = write;
    catalog->read = read;
    catalog->context = context;
    catalog->count = 0u;
    catalog->error = (write == 0 || read == 0) ?
        UZ_CATALOG_ERR_STATE : UZ_CATALOG_OK;
}

unsigned char uz_catalog_append(UzCatalog *catalog,
                                const UzZipRecord *record) {
    if (catalog == 0 || catalog->write == 0 || record == 0) {
        if (catalog != 0) catalog->error = UZ_CATALOG_ERR_STATE;
        return 0u;
    }
    if (!record_valid(record)) {
        catalog->error = UZ_CATALOG_ERR_RECORD;
        return 0u;
    }
    if (catalog->count >= UZ_CATALOG_MAX_ENTRIES) {
        catalog->error = UZ_CATALOG_ERR_FULL;
        return 0u;
    }
    if (!catalog->write(catalog->context, record_offset(catalog->count),
                        record, sizeof(*record))) {
        catalog->error = UZ_CATALOG_ERR_IO;
        return 0u;
    }
    ++catalog->count;
    catalog->error = UZ_CATALOG_OK;
    return 1u;
}

static unsigned char ascii_fold(unsigned char value) {
    if (value >= 0x61u && value <= 0x7Au)
        return (unsigned char)(value - 0x20u);
    return value;
}

static unsigned int normalized_length(const UzZipRecord *record) {
    unsigned int length;

    length = strlen(record->name);
    if (record->directory && length != 0u &&
        (unsigned char)record->name[length - 1u] == 0x2Fu) --length;
    return length;
}

static unsigned char prefix_equal(const char *left, const char *right,
                                  unsigned int length) {
    while (length-- != 0u) {
        if (ascii_fold((unsigned char)*left++) !=
            ascii_fold((unsigned char)*right++)) return 0u;
    }
    return 1u;
}

static unsigned char names_conflict(const UzZipRecord *left,
                                    const UzZipRecord *right) {
    unsigned int left_len;
    unsigned int right_len;

    left_len = normalized_length(left);
    right_len = normalized_length(right);
    if (left_len == right_len)
        return prefix_equal(left->name, right->name, left_len);
    if (left_len < right_len)
        return (unsigned char)(!left->directory &&
            (unsigned char)right->name[left_len] == 0x2Fu &&
            prefix_equal(left->name, right->name, left_len));
    return (unsigned char)(!right->directory &&
        (unsigned char)left->name[right_len] == 0x2Fu &&
        prefix_equal(left->name, right->name, right_len));
}

unsigned char uz_catalog_append_unique(UzCatalog *catalog,
                                       const UzZipRecord *record,
                                       UzZipRecord *scratch) {
    unsigned int index;

    if (catalog == 0 || catalog->read == 0 || record == 0 || scratch == 0) {
        if (catalog != 0) catalog->error = UZ_CATALOG_ERR_STATE;
        return 0u;
    }
    if (!record_valid(record)) {
        catalog->error = UZ_CATALOG_ERR_RECORD;
        return 0u;
    }
    for (index = 0u; index < catalog->count; ++index) {
        if (!uz_catalog_get(catalog, index, scratch)) return 0u;
        if (names_conflict(scratch, record)) {
            catalog->error = UZ_CATALOG_ERR_CONFLICT;
            return 0u;
        }
    }
    return uz_catalog_append(catalog, record);
}

unsigned char uz_catalog_get(UzCatalog *catalog, unsigned int index,
                             UzZipRecord *record) {
    if (catalog == 0 || catalog->read == 0 || record == 0) {
        if (catalog != 0) catalog->error = UZ_CATALOG_ERR_STATE;
        return 0u;
    }
    if (index >= catalog->count) {
        catalog->error = UZ_CATALOG_ERR_INDEX;
        return 0u;
    }
    if (!catalog->read(catalog->context, record_offset(index),
                       record, sizeof(*record))) {
        catalog->error = UZ_CATALOG_ERR_IO;
        return 0u;
    }
    catalog->error = UZ_CATALOG_OK;
    return 1u;
}

unsigned char uz_catalog_update(UzCatalog *catalog, unsigned int index,
                                const UzZipRecord *record) {
    if (catalog == 0 || catalog->write == 0 || record == 0) {
        if (catalog != 0) catalog->error = UZ_CATALOG_ERR_STATE;
        return 0u;
    }
    if (!record_valid(record)) {
        catalog->error = UZ_CATALOG_ERR_RECORD;
        return 0u;
    }
    if (index >= catalog->count) {
        catalog->error = UZ_CATALOG_ERR_INDEX;
        return 0u;
    }
    if (!catalog->write(catalog->context, record_offset(index),
                        record, sizeof(*record))) {
        catalog->error = UZ_CATALOG_ERR_IO;
        return 0u;
    }
    catalog->error = UZ_CATALOG_OK;
    return 1u;
}

#if defined(__CC65__) && (defined(UZIP_READYOS_APP) || \
    defined(UZIP_CATALOG_UI) || defined(UZIP_CREATE_PLAN_OVERLAY))
#pragma rodata-name(pop)
#pragma code-name(pop)
#endif
