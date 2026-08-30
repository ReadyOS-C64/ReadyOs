#include "uz_crc32.h"

#ifdef UZIP_READYOS_APP
#pragma code-name(push, "JOB_CODE")
#pragma rodata-name(push, "JOB_RODATA")
#endif

static const unsigned char crc_nibble[16][4] = {
    {0x00u, 0x00u, 0x00u, 0x00u},
    {0x64u, 0x10u, 0xB7u, 0x1Du},
    {0xC8u, 0x20u, 0x6Eu, 0x3Bu},
    {0xACu, 0x30u, 0xD9u, 0x26u},
    {0x90u, 0x41u, 0xDCu, 0x76u},
    {0xF4u, 0x51u, 0x6Bu, 0x6Bu},
    {0x58u, 0x61u, 0xB2u, 0x4Du},
    {0x3Cu, 0x71u, 0x05u, 0x50u},
    {0x20u, 0x83u, 0xB8u, 0xEDu},
    {0x44u, 0x93u, 0x0Fu, 0xF0u},
    {0xE8u, 0xA3u, 0xD6u, 0xD6u},
    {0x8Cu, 0xB3u, 0x61u, 0xCBu},
    {0xB0u, 0xC2u, 0x64u, 0x9Bu},
    {0xD4u, 0xD2u, 0xD3u, 0x86u},
    {0x78u, 0xE2u, 0x0Au, 0xA0u},
    {0x1Cu, 0xF2u, 0xBDu, 0xBDu}
};

void uz_crc32_init(UzCrc32 *crc) {
    crc->byte[0] = 0xFFu;
    crc->byte[1] = 0xFFu;
    crc->byte[2] = 0xFFu;
    crc->byte[3] = 0xFFu;
}

void uz_crc32_update(UzCrc32 *crc, const unsigned char *data,
                     unsigned int length) {
    unsigned char half;
    unsigned char index;

    while (length-- != 0u) {
        crc->byte[0] ^= *data++;
        for (half = 0u; half < 2u; ++half) {
            index = (unsigned char)(crc->byte[0] & 0x0Fu);
            crc->byte[0] = (unsigned char)(
                (crc->byte[0] >> 4u) | (crc->byte[1] << 4u));
            crc->byte[1] = (unsigned char)(
                (crc->byte[1] >> 4u) | (crc->byte[2] << 4u));
            crc->byte[2] = (unsigned char)(
                (crc->byte[2] >> 4u) | (crc->byte[3] << 4u));
            crc->byte[3] >>= 4u;
            crc->byte[0] ^= crc_nibble[index][0];
            crc->byte[1] ^= crc_nibble[index][1];
            crc->byte[2] ^= crc_nibble[index][2];
            crc->byte[3] ^= crc_nibble[index][3];
        }
    }
}

void uz_crc32_finish(UzCrc32 *crc) {
    crc->byte[0] ^= 0xFFu;
    crc->byte[1] ^= 0xFFu;
    crc->byte[2] ^= 0xFFu;
    crc->byte[3] ^= 0xFFu;
}

unsigned char uz_crc32_equal(const UzCrc32 *left, const UzCrc32 *right) {
    return (unsigned char)(left->byte[0] == right->byte[0] &&
                           left->byte[1] == right->byte[1] &&
                           left->byte[2] == right->byte[2] &&
                           left->byte[3] == right->byte[3]);
}

#ifdef UZIP_READYOS_APP
#pragma rodata-name(pop)
#pragma code-name(pop)
#endif
