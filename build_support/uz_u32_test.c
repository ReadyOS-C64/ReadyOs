#include <assert.h>
#include <string.h>

#include "uz_u32.h"

int main(void) {
    static const unsigned char encoded[4] = {0xFEu, 0xFFu, 0x34u, 0x12u};
    unsigned char output[4];
    UzU32 value;
    UzU32 other;

    assert(sizeof(UzU32) == 4u);
    uz_u32_zero(&value);
    assert(value.lo == 0u && value.hi == 0u);
    uz_u32_from_le(&value, encoded);
    assert(value.lo == 0xFFFEu && value.hi == 0x1234u);
    memset(output, 0, sizeof(output));
    uz_u32_to_le(output, &value);
    assert(memcmp(output, encoded, sizeof(output)) == 0);

    uz_u32_add_u16(&value, 3u);
    assert(value.lo == 1u && value.hi == 0x1235u);
    uz_u32_sub_u16(&value, 3u);
    assert(value.lo == 0xFFFEu && value.hi == 0x1234u);

    value.lo = 0xFFF0u;
    value.hi = 0xFFFFu;
    other.lo = 0x0020u;
    other.hi = 0u;
    uz_u32_add(&value, &other);
    assert(value.lo == 0x0010u && value.hi == 0u);
    uz_u32_sub(&value, &other);
    assert(value.lo == 0xFFF0u && value.hi == 0xFFFFu);

    other = value;
    assert(uz_u32_equal(&value, &other));
    other.lo = 0xFFF1u;
    assert(uz_u32_less(&value, &other));
    other.lo = 0u;
    other.hi = 0u;
    assert(!uz_u32_less(&value, &other));
    return 0;
}
