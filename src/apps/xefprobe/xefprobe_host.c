#include "xefprobe_shared.h"

static unsigned char host_data[XEFPROBE_DATA_LEN];
void xefprobe_host_fail_loop(void);
void xefprobe_host_after_clear_checkpoint(void);
void xefprobe_host_after_verify_checkpoint(void);

static unsigned char host_verify_preloaded_data(void) {
    unsigned char i = 0u;

    reu_dma_fetch((unsigned int)host_data, XEFPROBE_DATA_REU_BANK, 0u, XEFPROBE_DATA_LEN);
    do {
        if (host_data[i] != xefprobe_pattern_byte(i)) {
            return 0u;
        }
        ++i;
    } while (i != 0u);
    return 1u;
}

static void host_fail(const char *line2, unsigned char code) {
    xefprobe_dbg_put('F');
    XEF_RESULT_BASE[1] = code;
    xefprobe_clear(XEF_COLOR_RED, XEF_COLOR_RED, XEF_COLOR_WHITE);
    xefprobe_puts(0u, 0u, "XEFPROBE HOST FAIL", XEF_COLOR_YELLOW);
    xefprobe_puts(0u, 2u, line2, XEF_COLOR_WHITE);
    xefprobe_host_fail_loop();
}

int main(void) {
    xefprobe_dbg_put('H');
    xefprobe_clear(XEF_COLOR_BLUE, XEF_COLOR_BLUE, XEF_COLOR_WHITE);
    xefprobe_host_after_clear_checkpoint();
    xefprobe_puts(0u, 0u, "XEFPROBE HOST", XEF_COLOR_YELLOW);
    xefprobe_puts(0u, 2u, "RUNNING FROM REU BANK 0", XEF_COLOR_WHITE);
    XEF_RESULT_BASE[0] = XEFPROBE_HOST_FLAG;

    xefprobe_dbg_put('V');
    if (!host_verify_preloaded_data()) {
        host_fail("CRT->REU VERIFY FAILED", 0xE1u);
    }
    XEF_RESULT_BASE[1] = XEFPROBE_HOST_DATA_FLAG;
    xefprobe_puts(0u, 4u, "CRT->REU DATA OK", XEF_COLOR_LIGHTGREEN);
    xefprobe_host_after_verify_checkpoint();
    xefprobe_puts(0u, 6u, "SWITCHING TO PAYLOAD", XEF_COLOR_LIGHTBLUE);
    xefprobe_delay();

    xefprobe_dbg_put('L');
    *SHIM_TARGET_BANK = XEFPROBE_PAYLOAD_REU_BANK;
    *SHIM_CURRENT_BANK = 0u;
    __asm__("jmp $C80F");
    return 0;
}
