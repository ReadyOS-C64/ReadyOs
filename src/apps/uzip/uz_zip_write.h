#ifndef UZ_ZIP_WRITE_H
#define UZ_ZIP_WRITE_H

#include "uz_crc32.h"
#include "uz_dos.h"
#include "uz_u32.h"

#define UZ_ZIP_NAME_CAP 128u

#define UZ_ZIP_OK               0u
#define UZ_ZIP_ERR_NAME         1u
#define UZ_ZIP_ERR_STATE        2u
#define UZ_ZIP_ERR_WRITE        3u
#define UZ_ZIP_ERR_ENTRY_COUNT  4u

typedef struct {
    char name[UZ_ZIP_NAME_CAP];
    UzU32 local_offset;
    UzU32 size;
    UzU32 compressed_size;
    UzCrc32 crc;
    unsigned int flags;
    unsigned int method;
    unsigned char directory;
} UzZipRecord;

typedef struct {
    UzDos *output;
    UzU32 offset;
    UzZipRecord *active;
    unsigned char error;
} UzZipWriter;

unsigned char uz_zip_name_safe(const char *name, unsigned char directory);
void uz_zip_writer_init(UzZipWriter *writer, UzDos *output);
unsigned char uz_zip_begin_streamed(UzZipWriter *writer, UzZipRecord *record,
                                    const char *name, unsigned char directory,
                                    unsigned int method);
#define uz_zip_begin_store(writer, record, name, directory) \
    uz_zip_begin_streamed((writer), (record), (name), (directory), 0u)
#define uz_zip_begin_deflate(writer, record, name) \
    uz_zip_begin_streamed((writer), (record), (name), 0u, 8u)
unsigned char uz_zip_store_data(UzZipWriter *writer,
                                const unsigned char *data,
                                unsigned int length);
unsigned char uz_zip_finish_store(UzZipWriter *writer);
/* The raw method-8 bytes were written directly by the compressor. This adds
 * record->compressed_size to the archive offset and emits its descriptor. */
unsigned char uz_zip_finish_deflate(UzZipWriter *writer);
/* Central records are deliberately streamed one at a time. The caller saves
 * writer->offset before the first call, loads each record from its bounded
 * catalog, then passes that saved offset to the EOCD finisher. */
unsigned char uz_zip_emit_central(UzZipWriter *writer,
                                  const UzZipRecord *record);
unsigned char uz_zip_finish_archive(UzZipWriter *writer,
                                    const UzU32 *central_offset,
                                    unsigned int count);

#endif
