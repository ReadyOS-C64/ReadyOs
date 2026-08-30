#ifndef UZ_CRC32_H
#define UZ_CRC32_H

typedef struct {
    unsigned char byte[4];
} UzCrc32;

void uz_crc32_init(UzCrc32 *crc);
void uz_crc32_update(UzCrc32 *crc, const unsigned char *data,
                     unsigned int length);
void uz_crc32_finish(UzCrc32 *crc);
unsigned char uz_crc32_equal(const UzCrc32 *left, const UzCrc32 *right);

#endif
