#ifndef UZ_INFLATE_ERRORS_H
#define UZ_INFLATE_ERRORS_H

/* Shared by the host semantic oracle and the fixed-scratch 6502 phase. */
#define UZ_INFLATE_OK             0u
#define UZ_INFLATE_IO             1u
#define UZ_INFLATE_TRUNCATED      2u
#define UZ_INFLATE_BLOCK_TYPE     3u
#define UZ_INFLATE_STORED_LENGTH  4u
#define UZ_INFLATE_TREE           5u
#define UZ_INFLATE_SYMBOL         6u
#define UZ_INFLATE_DISTANCE       7u
#define UZ_INFLATE_TRAILING       8u
#define UZ_INFLATE_STATE          9u
#define UZ_INFLATE_OUTPUT_SIZE   10u

#endif
