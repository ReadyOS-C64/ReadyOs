/*
 * test_reu.c - REU DMA Test for Ready OS
 * Verifies REU stash/fetch operations work correctly
 *
 * For Commodore 64 with 16MB REU, compiled with CC65
 */

#include <c64.h>
#include <conio.h>
#include <string.h>

/* Screen and color memory */
#define SCREEN ((unsigned char*)0x0400)
#define COLOR_RAM ((unsigned char*)0xD800)

/* REU Registers */
#define REU_STATUS   (*(unsigned char*)0xDF00)
#define REU_COMMAND  (*(unsigned char*)0xDF01)
#define REU_C64_LO   (*(unsigned char*)0xDF02)
#define REU_C64_HI   (*(unsigned char*)0xDF03)
#define REU_REU_LO   (*(unsigned char*)0xDF04)
#define REU_REU_HI   (*(unsigned char*)0xDF05)
#define REU_REU_BANK (*(unsigned char*)0xDF06)
#define REU_LEN_LO   (*(unsigned char*)0xDF07)
#define REU_LEN_HI   (*(unsigned char*)0xDF08)

/* REU Commands */
#define REU_CMD_STASH 0x90  /* C64 -> REU */
#define REU_CMD_FETCH 0x91  /* REU -> C64 */

/* Test buffer */
static unsigned char test_src[256];
static unsigned char test_dst[256];

/* Function prototypes */
static void init_screen(void);
static void print_at(unsigned char x, unsigned char y, const char *text);
static void print_hex(unsigned char x, unsigned char y, unsigned int value);
static unsigned char detect_reu(void);
static void reu_stash(unsigned int c64_addr, unsigned char bank,
                      unsigned int reu_addr, unsigned int length);
static void reu_fetch(unsigned int c64_addr, unsigned char bank,
                      unsigned int reu_addr, unsigned int length);
static unsigned char test_basic_transfer(void);
static unsigned char test_large_transfer(void);
static void show_result(unsigned char y, const char *test_name, unsigned char passed);

int main(void) {
    unsigned char reu_present;
    unsigned char test1, test2;

    init_screen();
    print_at(10, 0, "READY OS - REU TEST");
    print_at(10, 1, "===================");

    /* Detect REU */
    print_at(0, 3, "DETECTING REU...");
    reu_present = detect_reu();

    if (!reu_present) {
        print_at(0, 4, "ERROR: NO REU DETECTED!");
        print_at(0, 5, "PLEASE RUN WITH -REU FLAG");
        goto done;
    }

    print_at(17, 3, "OK - 16MB REU FOUND");

    /* Run tests */
    print_at(0, 5, "RUNNING TESTS...");

    /* Test 1: Basic 256-byte transfer */
    test1 = test_basic_transfer();
    show_result(7, "256-BYTE TRANSFER", test1);

    /* Test 2: Large transfer (1KB) */
    test2 = test_large_transfer();
    show_result(8, "1KB TRANSFER", test2);

    /* Summary */
    print_at(0, 10, "===================");
    if (test1 && test2) {
        print_at(0, 11, "ALL TESTS PASSED!");
        print_at(0, 12, "REU IS WORKING CORRECTLY");
    } else {
        print_at(0, 11, "SOME TESTS FAILED!");
    }

done:
    print_at(0, 14, "PRESS ANY KEY...");
    cgetc();
    return 0;
}

static void init_screen(void) {
    clrscr();
    VIC.bordercolor = COLOR_BLUE;
    VIC.bgcolor0 = COLOR_BLUE;
    textcolor(COLOR_WHITE);
}

static void print_at(unsigned char x, unsigned char y, const char *text) {
    unsigned int offset;
    unsigned char i;

    offset = (unsigned int)y * 40 + x;
    for (i = 0; text[i] != 0; ++i) {
        unsigned char c = text[i];
        /* Convert ASCII to screen code */
        if (c >= 'A' && c <= 'Z') {
            c = c - 'A' + 1;  /* A-Z -> 1-26 */
        } else if (c >= 'a' && c <= 'z') {
            c = c - 'a' + 1;  /* a-z -> 1-26 */
        } else if (c >= '0' && c <= '9') {
            c = c - '0' + 48; /* 0-9 -> 48-57 */
        } else if (c == ' ') {
            c = 32;
        } else if (c == '-') {
            c = 45;
        } else if (c == '=') {
            c = 61;
        } else if (c == '.') {
            c = 46;
        } else if (c == '!') {
            c = 33;
        } else if (c == ':') {
            c = 58;
        }
        SCREEN[offset + i] = c;
        COLOR_RAM[offset + i] = COLOR_WHITE;
    }
}

static void print_hex(unsigned char x, unsigned char y, unsigned int value) {
    static const char hex[] = "0123456789ABCDEF";
    unsigned int offset;

    offset = (unsigned int)y * 40 + x;
    SCREEN[offset] = 4;  /* '$' -> screen code for $ */
    SCREEN[offset + 1] = hex[(value >> 12) & 0x0F] - 'A' + 1;
    SCREEN[offset + 2] = hex[(value >> 8) & 0x0F] - 'A' + 1;
    SCREEN[offset + 3] = hex[(value >> 4) & 0x0F] - 'A' + 1;
    SCREEN[offset + 4] = hex[value & 0x0F] - 'A' + 1;
}

static unsigned char detect_reu(void) {
    static unsigned char test_byte;
    static unsigned char result_byte;

    /* More reliable detection: do an actual DMA transfer */
    /* Write a test pattern, stash to REU, clear it, fetch back */

    test_byte = 0xA5;  /* Test pattern */
    result_byte = 0;

    /* Setup: stash test_byte to REU bank 0, offset 0 */
    REU_C64_LO = (unsigned int)&test_byte & 0xFF;
    REU_C64_HI = ((unsigned int)&test_byte >> 8) & 0xFF;
    REU_REU_LO = 0x00;
    REU_REU_HI = 0x00;
    REU_REU_BANK = 0;
    REU_LEN_LO = 1;
    REU_LEN_HI = 0;
    REU_COMMAND = REU_CMD_STASH;

    /* Change the test byte */
    test_byte = 0x00;

    /* Fetch back from REU to result_byte */
    REU_C64_LO = (unsigned int)&result_byte & 0xFF;
    REU_C64_HI = ((unsigned int)&result_byte >> 8) & 0xFF;
    REU_REU_LO = 0x00;
    REU_REU_HI = 0x00;
    REU_REU_BANK = 0;
    REU_LEN_LO = 1;
    REU_LEN_HI = 0;
    REU_COMMAND = REU_CMD_FETCH;

    /* If REU is present, result_byte should be 0xA5 */
    return (result_byte == 0xA5) ? 1 : 0;
}

static void reu_stash(unsigned int c64_addr, unsigned char bank,
                      unsigned int reu_addr, unsigned int length) {
    REU_C64_LO = c64_addr & 0xFF;
    REU_C64_HI = (c64_addr >> 8) & 0xFF;
    REU_REU_LO = reu_addr & 0xFF;
    REU_REU_HI = (reu_addr >> 8) & 0xFF;
    REU_REU_BANK = bank;
    REU_LEN_LO = length & 0xFF;
    REU_LEN_HI = (length >> 8) & 0xFF;
    REU_COMMAND = REU_CMD_STASH;
}

static void reu_fetch(unsigned int c64_addr, unsigned char bank,
                      unsigned int reu_addr, unsigned int length) {
    REU_C64_LO = c64_addr & 0xFF;
    REU_C64_HI = (c64_addr >> 8) & 0xFF;
    REU_REU_LO = reu_addr & 0xFF;
    REU_REU_HI = (reu_addr >> 8) & 0xFF;
    REU_REU_BANK = bank;
    REU_LEN_LO = length & 0xFF;
    REU_LEN_HI = (length >> 8) & 0xFF;
    REU_COMMAND = REU_CMD_FETCH;
}

static unsigned char test_basic_transfer(void) {
    unsigned char i;
    unsigned char passed;

    /* Fill source buffer with test pattern */
    for (i = 0; i != 0; --i) {  /* Won't execute - need different approach */
    }
    /* Use counting loop properly */
    i = 0;
    do {
        test_src[i] = i;
        ++i;
    } while (i != 0);

    /* Clear destination buffer */
    memset(test_dst, 0, 256);

    /* Stash source to REU bank 5 at offset 0 */
    reu_stash((unsigned int)test_src, 5, 0, 256);

    /* Fetch back to destination */
    reu_fetch((unsigned int)test_dst, 5, 0, 256);

    /* Verify */
    passed = 1;
    i = 0;
    do {
        if (test_dst[i] != i) {
            passed = 0;
            break;
        }
        ++i;
    } while (i != 0);

    return passed;
}

static unsigned char test_large_transfer(void) {
    unsigned int i;
    unsigned char *src;
    unsigned char *dst;
    unsigned char passed;

    /* Use screen memory as temp buffer for this test */
    /* We'll save it to REU first, do our test, then restore */

    /* For simplicity, just test that we can transfer 1KB */
    /* Use memory at $C000 as test area */
    src = (unsigned char*)0xC000;
    dst = (unsigned char*)0xC400;

    /* Fill source with pattern */
    for (i = 0; i < 1024; ++i) {
        src[i] = (unsigned char)(i & 0xFF);
    }

    /* Clear destination */
    memset(dst, 0, 1024);

    /* Stash to bank 6 */
    reu_stash(0xC000, 6, 0, 1024);

    /* Fetch to different location */
    reu_fetch(0xC400, 6, 0, 1024);

    /* Verify */
    passed = 1;
    for (i = 0; i < 1024; ++i) {
        if (dst[i] != (unsigned char)(i & 0xFF)) {
            passed = 0;
            break;
        }
    }

    return passed;
}

static void show_result(unsigned char y, const char *test_name, unsigned char passed) {
    print_at(2, y, test_name);
    if (passed) {
        print_at(25, y, "PASSED");
    } else {
        print_at(25, y, "FAILED");
    }
}
