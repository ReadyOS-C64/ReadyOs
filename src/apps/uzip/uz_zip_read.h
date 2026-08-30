#ifndef UZ_ZIP_READ_H
#define UZ_ZIP_READ_H

#include "uz_zip_write.h"

#define UZ_ZIP_READ_OK              0u
#define UZ_ZIP_READ_IO              1u
#define UZ_ZIP_READ_NO_EOCD         2u
#define UZ_ZIP_READ_UNSUPPORTED     3u
#define UZ_ZIP_READ_BOUNDS          4u
#define UZ_ZIP_READ_CENTRAL         5u
#define UZ_ZIP_READ_LOCAL           6u
#define UZ_ZIP_READ_CRC             7u
#define UZ_ZIP_READ_NAME            8u
#define UZ_ZIP_READ_DESCRIPTOR      9u
#define UZ_ZIP_READ_STATE          10u

typedef unsigned char (*UzZipReadAt)(void *context, const UzU32 *offset,
                                     unsigned char *destination,
                                     unsigned int length);

typedef struct {
    UzDos *input;
    UzZipReadAt read_at;
    void *read_context;
    UzU32 read_cursor;
    UzU32 archive_size;
    UzU32 eocd_offset;
    UzU32 central_offset;
    UzU32 central_size;
    UzU32 central_cursor;
    UzU32 central_end;
    unsigned int entry_count;
    unsigned int entry_index;
    unsigned char error;
} UzZipReader;

void uz_zip_reader_init(UzZipReader *reader, UzDos *input);
/* Overlay-safe parser initialization. The callback must fill exactly length
 * bytes or fail; the persistent coordinator owns all Ultimate DOS/UCI work. */
void uz_zip_reader_init_at(UzZipReader *reader, const UzU32 *archive_size,
                           UzZipReadAt read_at, void *read_context);
unsigned char uz_zip_reader_begin(UzZipReader *reader,
                                  unsigned char *scratch,
                                  unsigned int scratch_size);
unsigned char uz_zip_reader_next(UzZipReader *reader, UzZipRecord *record);
unsigned char uz_zip_reader_finished(UzZipReader *reader);
/* Validate the matching local header/name/descriptor without consuming member
 * data or mutating a destination. This is the preflight boundary used before
 * an extraction root may be created. */
unsigned char uz_zip_reader_local(UzZipReader *reader,
                                  const UzZipRecord *record,
                                  UzU32 *data_offset,
                                  unsigned char *scratch,
                                  unsigned int scratch_size);
unsigned char uz_zip_extract_store(UzZipReader *reader,
                                   const UzZipRecord *record,
                                   UzDos *output,
                                   unsigned char *scratch,
                                   unsigned int scratch_size);

#endif
