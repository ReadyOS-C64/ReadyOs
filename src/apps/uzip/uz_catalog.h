#ifndef UZ_CATALOG_H
#define UZ_CATALOG_H

#include "uz_zip_write.h"

#define UZ_CATALOG_MAX_ENTRIES 400u

#define UZ_CATALOG_OK        0u
#define UZ_CATALOG_ERR_STATE 1u
#define UZ_CATALOG_ERR_FULL  2u
#define UZ_CATALOG_ERR_IO    3u
#define UZ_CATALOG_ERR_INDEX 4u
#define UZ_CATALOG_ERR_RECORD 5u
#define UZ_CATALOG_ERR_CONFLICT 6u

typedef unsigned char (*UzCatalogWrite)(void *context, unsigned int offset,
                                        const void *source,
                                        unsigned int length);
typedef unsigned char (*UzCatalogRead)(void *context, unsigned int offset,
                                       void *destination,
                                       unsigned int length);

typedef struct {
    UzCatalogWrite write;
    UzCatalogRead read;
    void *context;
    unsigned int count;
    unsigned char error;
} UzCatalog;

void uz_catalog_init(UzCatalog *catalog, UzCatalogWrite write,
                     UzCatalogRead read, void *context);
unsigned char uz_catalog_append(UzCatalog *catalog,
                                const UzZipRecord *record);
/* Preflight append rejects case-folded duplicate destination names and a file
 * whose normalized name would have to become another entry's parent folder.
 * Caller-owned scratch avoids a 149-byte cc65 software-stack local. */
unsigned char uz_catalog_append_unique(UzCatalog *catalog,
                                       const UzZipRecord *record,
                                       UzZipRecord *scratch);
unsigned char uz_catalog_get(UzCatalog *catalog, unsigned int index,
                             UzZipRecord *record);
unsigned char uz_catalog_update(UzCatalog *catalog, unsigned int index,
                                const UzZipRecord *record);

#endif
