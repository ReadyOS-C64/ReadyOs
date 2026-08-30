#include "uz_u32.h"

void uz_u32_zero(UzU32 *value) {
    value->lo = 0u;
    value->hi = 0u;
}

void uz_u32_from_le(UzU32 *value, const unsigned char *source) {
    value->lo = (unsigned int)(source[0] | ((unsigned int)source[1] << 8u));
    value->hi = (unsigned int)(source[2] | ((unsigned int)source[3] << 8u));
}

void uz_u32_to_le(unsigned char *destination, const UzU32 *value) {
    destination[0] = (unsigned char)value->lo;
    destination[1] = (unsigned char)(value->lo >> 8u);
    destination[2] = (unsigned char)value->hi;
    destination[3] = (unsigned char)(value->hi >> 8u);
}

void uz_u32_add_u16(UzU32 *value, unsigned int addend) {
    unsigned int before;

    before = value->lo;
    value->lo = (unsigned int)(value->lo + addend);
    if (value->lo < before) ++value->hi;
}

void uz_u32_sub_u16(UzU32 *value, unsigned int subtrahend) {
    unsigned int before;

    before = value->lo;
    value->lo = (unsigned int)(value->lo - subtrahend);
    if (before < subtrahend) --value->hi;
}

/* Full addition is shared by method-8 creation and central-directory reading.
 * Keep its single small copy resident rather than duplicate it across mutually
 * exclusive Store and ZIP-read images. */
void uz_u32_add(UzU32 *value, const UzU32 *addend) {
    unsigned int before;

    before = value->lo;
    value->lo = (unsigned int)(value->lo + addend->lo);
    value->hi = (unsigned int)(value->hi + addend->hi);
    if (value->lo < before) ++value->hi;
}

#ifdef UZIP_READYOS_APP
/* Full-width subtraction is currently a Store/container phase operation.
 * Keep it with that overlay instead of consuming the saturated resident core. */
#pragma code-name(push, "JOB_CODE")
#endif
void uz_u32_sub(UzU32 *value, const UzU32 *subtrahend) {
    unsigned int before;

    before = value->lo;
    value->lo = (unsigned int)(value->lo - subtrahend->lo);
    value->hi = (unsigned int)(value->hi - subtrahend->hi);
    if (before < subtrahend->lo) --value->hi;
}
#ifdef UZIP_READYOS_APP
#pragma code-name(pop)
#endif

#ifdef UZIP_READYOS_APP
#pragma code-name(push, "ZIP_READ_CODE")
#endif
unsigned char uz_u32_equal(const UzU32 *left, const UzU32 *right) {
    return (unsigned char)(left->lo == right->lo && left->hi == right->hi);
}

unsigned char uz_u32_less(const UzU32 *left, const UzU32 *right) {
    if (left->hi != right->hi) return (unsigned char)(left->hi < right->hi);
    return (unsigned char)(left->lo < right->lo);
}
#ifdef UZIP_READYOS_APP
#pragma code-name(pop)
#endif
