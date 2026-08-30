#include <assert.h>
#include <string.h>

#include "uz_crc32.h"

int main(void) {
    static const unsigned char expected[4] = {0x26u, 0x39u, 0xF4u, 0xCBu};
    UzCrc32 crc;

    uz_crc32_init(&crc);
    uz_crc32_update(&crc, (const unsigned char *)"123456789", 4u);
    uz_crc32_update(&crc, (const unsigned char *)"123456789" + 4u, 5u);
    uz_crc32_finish(&crc);
    assert(memcmp(crc.byte, expected, sizeof(expected)) == 0);
    return 0;
}
