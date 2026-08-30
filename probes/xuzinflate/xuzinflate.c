#include <cbm.h>
#include <string.h>

#include "xuzinflate_config.h"
#include "uz_dos.h"
#include "uz_inflate6502.h"

/* The unused KERNAL cassette buffer survives a warm BASIC error/reset, so a
 * mid-codec crash still leaves the machine-readable progress record intact. */
#define RESULT ((volatile unsigned char *)0x033Cu)
#define INPUT_CHUNK 512u
#define INPUT_BUFFER ((unsigned char *)0x0400u)
/* WRITE's four-byte command header precedes the output payload in one screen-
 * RAM window. uz_dos_write therefore copies payload to its identical address. */
#define COMMAND_CAP 512u
#define COMMAND_BUFFER ((unsigned char *)0x0600u)
#define OUTPUT_CHUNK 508u
#define OUTPUT_BUFFER ((unsigned char *)0x0604u)
#define POSITIVE_COUNT 4u
#define NEGATIVE_COUNT 11u

#define STAGE_IDENTIFY 1u
#define STAGE_OWNER    2u
#define STAGE_PATHS    3u
#define STAGE_POSITIVE 4u
#define STAGE_NEGATIVE 5u
#define STAGE_DONE     6u

typedef struct {
    const char *packed_name;
    const char *output_name;
    unsigned int packed_size;
    unsigned int output_size;
    unsigned char crc[4];
} PositiveCase;

typedef struct {
    const char *packed_name;
    unsigned int packed_size;
    unsigned char expected_error;
    unsigned int expected_size;
    unsigned char output_bounded;
} NegativeCase;

/* The assembly phase refills at most 512 bytes and flushes at most 508. The
 * shared UCI transport still drains the complete hardware queues. */
static unsigned char output_response[28u];
static unsigned char status_queue[8];
static UzDos input;
static UzDos output;
static unsigned char stage;
static unsigned char failure_code;
static unsigned char case_index;
static unsigned char negative_index;

#define DICTIONARY_GUARD_LOW (*(volatile unsigned char *)0x2FFFu)
#define DICTIONARY_GUARD_HIGH (*(volatile unsigned char *)0xB000u)

void xuzinflate_stack_watermark_init(void);
unsigned int xuzinflate_stack_watermark_low(void);
extern unsigned int xuzinflate_stack_initial;

static const PositiveCase positive[POSITIVE_COUNT] = {
    {"empty.raw", "empty.bin", XUZ_EMPTY_PACKED, XUZ_EMPTY_SIZE,
     {XUZ_EMPTY_CRC0, XUZ_EMPTY_CRC1, XUZ_EMPTY_CRC2, XUZ_EMPTY_CRC3}},
    {"stored.raw", "stored.bin", XUZ_STORED_PACKED, XUZ_STORED_SIZE,
     {XUZ_STORED_CRC0, XUZ_STORED_CRC1, XUZ_STORED_CRC2, XUZ_STORED_CRC3}},
    {"fixed.raw", "fixed.bin", XUZ_FIXED_PACKED, XUZ_FIXED_SIZE,
     {XUZ_FIXED_CRC0, XUZ_FIXED_CRC1, XUZ_FIXED_CRC2, XUZ_FIXED_CRC3}},
    {"dynamic.raw", "dynamic.bin", XUZ_DYNAMIC_PACKED, XUZ_DYNAMIC_SIZE,
     {XUZ_DYNAMIC_CRC0, XUZ_DYNAMIC_CRC1, XUZ_DYNAMIC_CRC2, XUZ_DYNAMIC_CRC3}}
};

static const NegativeCase negative[NEGATIVE_COUNT] = {
    {"trunc.raw", XUZ_TRUNC_PACKED, UZ_INFLATE_TRUNCATED, 0u, 0u},
    {"trail.raw", XUZ_TRAIL_PACKED, UZ_INFLATE_TRAILING, 0u, 0u},
    {"badtype.raw", XUZ_BADTYPE_PACKED, UZ_INFLATE_BLOCK_TYPE, 0u, 0u},
    {"badstored.raw", XUZ_BADSTORED_PACKED, UZ_INFLATE_STORED_LENGTH, 0u, 0u},
    {"baddist.raw", XUZ_BADDIST_PACKED, UZ_INFLATE_DISTANCE, 0u, 0u},
    {"badlength.raw", XUZ_BADLENGTH_PACKED, UZ_INFLATE_SYMBOL, 0u, 0u},
    {"badrsvdist.raw", XUZ_BADRSVDIST_PACKED, UZ_INFLATE_DISTANCE, 0u, 0u},
    {"badrepeat.raw", XUZ_BADREPEAT_PACKED, UZ_INFLATE_TREE, 0u, 0u},
    {"badtree.raw", XUZ_BADTREE_PACKED, UZ_INFLATE_TREE, 0u, 0u},
    /* The 40K fixed stream already proves long bounded output in the positive
     * corpus. Use the 1,025-byte stored stream for the two error edges so the
     * stock-speed matrix does not repeat seven minutes of identical decoding. */
    {"stored.raw", XUZ_STORED_PACKED, UZ_INFLATE_OUTPUT_SIZE,
     XUZ_STORED_SIZE - 1u, 1u},
    {"stored.raw", XUZ_STORED_PACKED, UZ_INFLATE_OUTPUT_SIZE,
     XUZ_STORED_SIZE + 1u, 1u}
};

static unsigned char screen_code(unsigned char value) {
    if (value >= 0x41u && value <= 0x5Au) return (unsigned char)(value - 0x40u);
    if (value >= 0xC1u && value <= 0xDAu) return (unsigned char)(value - 0xC0u);
    return value;
}

static void screen_text(unsigned char row, const char *text) {
    volatile unsigned char *screen;
    unsigned char column;

    screen = (volatile unsigned char *)(0x0400u + (unsigned int)row * 40u);
    for (column = 0u; column < 40u; ++column) {
        screen[column] = (*text != 0) ? screen_code((unsigned char)*text++) : 0x20u;
    }
}

static void stamp(unsigned char value[3]) {
    /* KERNAL jiffy clock: sampled for physical elapsed-time estimates only. */
    value[0] = *(volatile unsigned char *)0x00A0u;
    value[1] = *(volatile unsigned char *)0x00A1u;
    value[2] = *(volatile unsigned char *)0x00A2u;
}

static void write_result(unsigned char done) {
    unsigned int stack_low;

    stack_low = xuzinflate_stack_watermark_low();
    RESULT[0] = 0x58u; /* XZI1 */
    RESULT[1] = 0x5Au;
    RESULT[2] = 0x49u;
    RESULT[3] = 0x31u;
    RESULT[4] = 1u;
    RESULT[5] = done;
    RESULT[6] = stage;
    RESULT[7] = failure_code;
    RESULT[8] = case_index;
    RESULT[9] = negative_index;
    RESULT[10] = (unsigned char)uz_uci_base();
    RESULT[11] = (unsigned char)(uz_uci_base() >> 8u);
    RESULT[12] = input.transfer.flags;
    RESULT[13] = input.transfer.last_status;
    RESULT[14] = output.transfer.flags;
    RESULT[15] = output.transfer.last_status;
    RESULT[16] = uz_inflate6502_error();
    RESULT[17] = POSITIVE_COUNT;
    RESULT[18] = NEGATIVE_COUNT;
    RESULT[19] = 0u;
    memcpy((void *)(RESULT + 56u), uz_inflate6502_crc()->byte, 4u);
    uz_u32_to_le((unsigned char *)(RESULT + 60u),
                 uz_inflate6502_output_size());
    RESULT[80] = (unsigned char)xuzinflate_stack_initial;
    RESULT[81] = (unsigned char)(xuzinflate_stack_initial >> 8u);
    RESULT[82] = (unsigned char)stack_low;
    RESULT[83] = (unsigned char)(stack_low >> 8u);
}

static void show_stage(const char *text) {
    screen_text(5u, text);
    write_result(0u);
}

static void stop_failed(unsigned char code) {
    failure_code = code;
    (void)uz_dos_close(&input);
    (void)uz_dos_close(&output);
    write_result(1u);
    screen_text(8u, "XUZINFLATE FINISHED FAIL");
    for (;;) { }
}

static unsigned char verify_owner(void) {
    unsigned int expected;
    unsigned int offset;
    unsigned int chunk;
    int got;

    expected = strlen(XUZINFLATE_OWNER_TEXT);
    if (!uz_dos_change_path(&input, XUZINFLATE_OWNED_ROOT) ||
        !uz_dos_open(&input, ".readyos-uzip-owner", UZ_DOS_OPEN_READ)) return 0u;
    offset = 0u;
    while (offset < expected) {
        chunk = (unsigned int)(expected - offset);
        if (chunk > INPUT_CHUNK) chunk = INPUT_CHUNK;
        got = uz_dos_read(&input, INPUT_BUFFER, chunk);
        if (got < 0 || (unsigned int)got != chunk ||
            memcmp(INPUT_BUFFER, XUZINFLATE_OWNER_TEXT + offset, chunk) != 0) {
            (void)uz_dos_close(&input);
            return 0u;
        }
        offset = (unsigned int)(offset + chunk);
    }
    got = uz_dos_read(&input, INPUT_BUFFER, 1u);
    if (!uz_dos_close(&input) || got != 0) return 0u;
    return 1u;
}

static int inflate_read(void *context, unsigned char *data,
                        unsigned int length) {
    return uz_dos_read((UzDos *)context, data, length);
}

static unsigned char inflate_write(void *context,
                                   const unsigned char *data,
                                   unsigned int length) {
    unsigned int dictionary_pos;

    uz_u32_to_le((unsigned char *)(RESULT + 72u),
                 uz_inflate6502_output_size());
    dictionary_pos = uz_inflate6502_dictionary_pos();
    RESULT[76] = (unsigned char)dictionary_pos;
    RESULT[77] = (unsigned char)(dictionary_pos >> 8u);
    RESULT[78] = DICTIONARY_GUARD_LOW;
    RESULT[79] = DICTIONARY_GUARD_HIGH;
    return uz_dos_write((UzDos *)context, data, length);
}

static unsigned char size_is(const UzU32 *value, unsigned int expected) {
    return (unsigned char)(value->hi == 0u && value->lo == expected);
}

static unsigned char positive_case(unsigned char index) {
    UzU32 packed_size;
    UzU32 expected_size;
    UzU32 actual_size;
    const PositiveCase *item;

    item = &positive[index];
    packed_size.lo = item->packed_size;
    packed_size.hi = 0u;
    expected_size.lo = item->output_size;
    expected_size.hi = 0u;
    if (!uz_dos_open(&input, item->packed_name, UZ_DOS_OPEN_READ) ||
        !uz_dos_file_info(&input, &actual_size) ||
        !size_is(&actual_size, item->packed_size) ||
        !uz_dos_open(&output, item->output_name, UZ_DOS_OPEN_WRITE_NEW)) {
        return 0u;
    }
    uz_inflate6502_init(inflate_read, &input, inflate_write, &output,
                        INPUT_BUFFER, INPUT_CHUNK,
                        OUTPUT_BUFFER, OUTPUT_CHUNK,
                        &packed_size, &expected_size);
    stamp((unsigned char *)(RESULT + 24u + (unsigned int)index * 3u));
    if (!uz_inflate6502_run()) return 0u;
    stamp((unsigned char *)(RESULT + 40u + (unsigned int)index * 3u));
    if (!size_is(uz_inflate6502_output_size(), item->output_size) ||
        memcmp(uz_inflate6502_crc()->byte, item->crc, 4u) != 0) return 0u;
    if (!uz_dos_close(&input) || !uz_dos_close(&output)) return 0u;
    return 1u;
}

static unsigned char negative_case(unsigned char index) {
    UzU32 packed_size;
    UzU32 actual_size;
    UzU32 expected_size;
    const UzU32 *expected_size_pointer;
    const NegativeCase *item;
    unsigned char accepted;

    item = &negative[index];
    packed_size.lo = item->packed_size;
    packed_size.hi = 0u;
    expected_size_pointer = 0;
    if (item->output_bounded) {
        expected_size.lo = item->expected_size;
        expected_size.hi = 0u;
        expected_size_pointer = &expected_size;
    }
    if (!uz_dos_open(&input, item->packed_name, UZ_DOS_OPEN_READ) ||
        !uz_dos_file_info(&input, &actual_size) ||
        !size_is(&actual_size, item->packed_size)) return 0u;
    uz_inflate6502_init(inflate_read, &input, 0, 0,
                        INPUT_BUFFER, INPUT_CHUNK,
                        OUTPUT_BUFFER, OUTPUT_CHUNK,
                        &packed_size, expected_size_pointer);
    accepted = uz_inflate6502_run();
    if (!uz_dos_close(&input)) return 0u;
    return (unsigned char)(!accepted &&
                           uz_inflate6502_error() == item->expected_error);
}

int main(void) {
    unsigned char index;

    *(volatile unsigned char *)0xD020u = 6u;
    *(volatile unsigned char *)0xD021u = 6u;
    memset((void *)0x0400u, 0x20, 1000u);
    screen_text(0u, "XUZINFLATE PHYSICAL C64 ULTIMATE PROBE");
    screen_text(1u, "STREAMED RAW RFC1951 + 32K DICTIONARY");
    screen_text(3u, "PRESS SPACE TO START");
    while (cbm_k_getin() != 0x20u) { }
    xuzinflate_stack_watermark_init();
    memset((void *)RESULT, 0, 128u);
    DICTIONARY_GUARD_LOW = 0xA5u;
    DICTIONARY_GUARD_HIGH = 0x5Au;

    /* Expose RAM under BASIC ROM for the middle of the 32K dictionary while
     * retaining KERNAL ROM, IRQ jiffies, and I/O/UCI visibility. */
    *(volatile unsigned char *)0x0001u = 0x36u;

    uz_dos_init(&input, UZ_DOS_TARGET_READ, COMMAND_BUFFER, COMMAND_CAP,
                INPUT_BUFFER, INPUT_CHUNK,
                status_queue, sizeof(status_queue));
    uz_dos_init(&output, UZ_DOS_TARGET_WRITE, COMMAND_BUFFER, COMMAND_CAP,
                output_response, sizeof(output_response),
                status_queue, sizeof(status_queue));

    stage = STAGE_IDENTIFY;
    show_stage("IDENTIFY ULTIMATE DOS TARGETS");
    if (!uz_dos_identify(&input) || !uz_dos_identify(&output)) stop_failed(0x11u);

    stage = STAGE_OWNER;
    show_stage("VERIFY UNIQUE OWNED ROOT");
    if (!verify_owner()) stop_failed(0x21u);

    stage = STAGE_PATHS;
    show_stage("CREATE OUTPUT PATH");
    if (!uz_dos_change_path(&input, "source") ||
        !uz_dos_change_path(&output, XUZINFLATE_OWNED_ROOT) ||
        !uz_dos_create_dir(&output, "output") ||
        !uz_dos_change_path(&output, "output")) stop_failed(0x31u);

    stage = STAGE_POSITIVE;
    for (index = 0u; index < POSITIVE_COUNT; ++index) {
        case_index = index;
        show_stage("INFLATE POSITIVE STREAM");
        if (!positive_case(index)) stop_failed((unsigned char)(0x40u + index));
        case_index = (unsigned char)(index + 1u);
        write_result(0u);
    }

    stage = STAGE_NEGATIVE;
    for (index = 0u; index < NEGATIVE_COUNT; ++index) {
        negative_index = index;
        show_stage("REJECT CORRUPT STREAM");
        if (!negative_case(index)) stop_failed((unsigned char)(0x50u + index));
        negative_index = (unsigned char)(index + 1u);
        write_result(0u);
    }

    stage = STAGE_DONE;
    failure_code = 0u;
    write_result(1u);
    screen_text(8u, "XUZINFLATE FINISHED PASS");
    for (;;) { }
    return 0;
}
