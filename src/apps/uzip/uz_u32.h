#ifndef UZ_U32_H
#define UZ_U32_H

#ifndef __CC65__
#include <limits.h>
#if USHRT_MAX != 0xFFFFu
#error uZIP host tests require a 16-bit unsigned short
#endif
typedef unsigned short UzWord16;
#else
typedef unsigned int UzWord16;
#endif

/* Explicit two-word arithmetic keeps cc65's software long helpers out of hot
 * stream paths while retaining classic ZIP's full unsigned 32-bit range.
 * Host fields are explicitly 16-bit so carry/borrow tests match cc65. */
typedef struct {
    UzWord16 lo;
    UzWord16 hi;
} UzU32;

void uz_u32_zero(UzU32 *value);
void uz_u32_from_le(UzU32 *value, const unsigned char *source);
void uz_u32_to_le(unsigned char *destination, const UzU32 *value);
void uz_u32_add_u16(UzU32 *value, unsigned int addend);
void uz_u32_sub_u16(UzU32 *value, unsigned int subtrahend);
void uz_u32_add(UzU32 *value, const UzU32 *addend);
void uz_u32_sub(UzU32 *value, const UzU32 *subtrahend);
unsigned char uz_u32_equal(const UzU32 *left, const UzU32 *right);
unsigned char uz_u32_less(const UzU32 *left, const UzU32 *right);

#endif
