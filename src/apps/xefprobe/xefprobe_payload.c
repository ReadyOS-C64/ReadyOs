#include "xefprobe_shared.h"

static unsigned char source_data[XEFPROBE_DATA_LEN];
static unsigned char verify_data[XEFPROBE_DATA_LEN];

static unsigned char payload_verify_preloaded_data(void) {
    unsigned char i = 0u;

    reu_dma_fetch((unsigned int)source_data, XEFPROBE_DATA_REU_BANK, 0u, XEFPROBE_DATA_LEN);
    do {
        if (source_data[i] != xefprobe_pattern_byte(i)) {
            return 0u;
        }
        ++i;
    } while (i != 0u);
    return 1u;
}

static unsigned char payload_verify_stash_fetch(void) {
    unsigned char i = 0u;

    do {
        source_data[i] = (unsigned char)(xefprobe_pattern_byte(i) ^ 0x5Au);
        verify_data[i] = 0u;
        ++i;
    } while (i != 0u);

    reu_dma_stash((unsigned int)source_data, XEFPROBE_SCRATCH_REU_BANK, 0u, XEFPROBE_DATA_LEN);
    reu_dma_fetch((unsigned int)verify_data, XEFPROBE_SCRATCH_REU_BANK, 0u, XEFPROBE_DATA_LEN);

    i = 0u;
    do {
        if (verify_data[i] != (unsigned char)(xefprobe_pattern_byte(i) ^ 0x5Au)) {
            return 0u;
        }
        ++i;
    } while (i != 0u);
    return 1u;
}

void xefprobe_payload_done(void);
void xefprobe_payload_fail_loop(void);
void xefprobe_payload_after_fetch_checkpoint(void);

static void payload_fail(const char *line2, unsigned char code) {
    xefprobe_dbg_put('F');
    XEF_RESULT_BASE[3] = code;
    xefprobe_clear(XEF_COLOR_RED, XEF_COLOR_RED, XEF_COLOR_WHITE);
    xefprobe_puts(0u, 0u, "XEFPROBE PAYLOAD FAIL", XEF_COLOR_YELLOW);
    xefprobe_puts(0u, 2u, line2, XEF_COLOR_WHITE);
    xefprobe_payload_fail_loop();
}

int main(void) {
    xefprobe_dbg_put('P');
    xefprobe_clear(XEF_COLOR_GREEN, XEF_COLOR_GREEN, XEF_COLOR_WHITE);
    xefprobe_puts(0u, 0u, "XEFPROBE PAYLOAD", XEF_COLOR_YELLOW);

    if (XEF_RESULT_BASE[0] != XEFPROBE_HOST_FLAG ||
        XEF_RESULT_BASE[1] != XEFPROBE_HOST_DATA_FLAG) {
        payload_fail("HOST FLAGS NOT SET", 0xE2u);
    }

    xefprobe_dbg_put('D');
    if (!payload_verify_preloaded_data()) {
        payload_fail("CRT->REU FETCH FAIL", 0xE3u);
    }
    XEF_RESULT_BASE[2] = XEFPROBE_PAYLOAD_FLAG;
    xefprobe_puts(0u, 2u, "HOST->PAYLOAD SWITCH OK", XEF_COLOR_LIGHTGREEN);
    xefprobe_puts(0u, 4u, "CRT->REU FETCH PASS", XEF_COLOR_LIGHTGREEN);
    xefprobe_payload_after_fetch_checkpoint();

    xefprobe_dbg_put('S');
    if (!payload_verify_stash_fetch()) {
        payload_fail("REU STASH/FETCH FAIL", 0xE4u);
    }
    XEF_RESULT_BASE[3] = XEFPROBE_STASH_FLAG;
    xefprobe_puts(0u, 6u, "REU STASH/FETCH PASS", XEF_COLOR_LIGHTGREEN);
    xefprobe_puts(0u, 8u, "TYPE1 CRT + REU OK", XEF_COLOR_LIGHTBLUE);
    xefprobe_payload_done();
    return 0;
}
