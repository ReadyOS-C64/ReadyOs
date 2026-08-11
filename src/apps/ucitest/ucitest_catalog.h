/*
 * ucitest_catalog.h - UCI tester command descriptors
 */

#ifndef UCITEST_CATALOG_H
#define UCITEST_CATALOG_H

#define UC_KIND_TRANSPORT 0
#define UC_KIND_DOS       1
#define UC_KIND_NETWORK   2
#define UC_KIND_CONTROL   3
#define UC_KIND_SOFTIEC   4
#define UC_KIND_HTTP      5

#define UC_FIELD_TEXT   1
#define UC_FIELD_TEXTZ  2
#define UC_FIELD_BYTE   3
#define UC_FIELD_WORD   4
#define UC_FIELD_DWORD  5
#define UC_FIELD_BOOL   6
#define UC_FIELD_CONST  7
#define UC_FIELD_RAW    8
#define UC_FIELD_KEY    9
#define UC_FIELD_LTEXT  10

#define UC_FLAG_MUTATING 0x01
#define UC_FLAG_NETWORK  0x02
#define UC_FLAG_LONG     0x04
#define UC_FLAG_REBOOTS  0x08
#define UC_FLAG_HANDLE   0x10
#define UC_FLAG_SPECIAL  0x20

#define UC_DEC_TEXT       1
#define UC_DEC_HEX        2
#define UC_DEC_HANDLE     3
#define UC_DEC_WORD       4
#define UC_DEC_MAC        5
#define UC_DEC_IP         6
#define UC_DEC_DIR        7
#define UC_DEC_FILE_INFO  8
#define UC_DEC_HTTP_VALUE 9
#define UC_DEC_DWORD      10
#define UC_DEC_SOCKET_READ 11
#define UC_DEC_HTTP_HANDLES 12
#define UC_DEC_IEC_NAME   13

#define UC_SPECIAL_DETECT 1
#define UC_SPECIAL_ID     2
#define UC_SPECIAL_STATUS 3
#define UC_SPECIAL_ABORT  4
#define UC_SPECIAL_CLEAR  5
#define UC_SPECIAL_RAW    6
#define UC_SPECIAL_NORMS  7

typedef struct {
    const char *name;
    unsigned char kind;
    unsigned char target;
} UciTestTargetSpec;

typedef struct {
    const char *label;
    unsigned char kind;
    unsigned int def_lo;
    unsigned int def_hi;
    const char *def_text;
    const char **choices;
    unsigned char choice_count;
} UciTestFieldSpec;

typedef struct {
    const char *name;
    unsigned char kind;
    unsigned char cmd;
    unsigned char flags;
    unsigned char decoder;
    const UciTestFieldSpec *fields;
    unsigned char field_count;
} UciTestCommandSpec;

typedef struct {
    const char *name;
    const char *hint1;
    const char *hint2;
    unsigned char kind;
    unsigned char cmd;
    unsigned char value_mask;
    unsigned int value0;
    unsigned int value1;
    const char *text0;
    const char *text1;
} UciTestExampleSpec;

extern const UciTestTargetSpec ucitest_targets[];
extern const unsigned char ucitest_target_count;
extern const UciTestCommandSpec ucitest_commands[];
extern const unsigned char ucitest_command_count;
extern const UciTestExampleSpec ucitest_examples[];
extern const unsigned char ucitest_example_count;

unsigned char ucitest_command_count_for_kind(unsigned char kind);
unsigned char ucitest_command_index_for_kind(unsigned char kind,
                                             unsigned char rel_index);
unsigned char ucitest_command_rel_for_kind_cmd(unsigned char kind,
                                               unsigned char cmd);
const char *ucitest_flag_text(unsigned char flags);

#endif /* UCITEST_CATALOG_H */
