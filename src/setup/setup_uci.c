#include "setup_uci.h"

#define UCI_DATA       0x80u
#define UCI_STAT       0x40u
#define UCI_STATE_MASK 0x30u
#define UCI_IDLE       0x00u
#define UCI_LAST       0x20u
#define UCI_MORE       0x30u
#define UCI_BUSY       0x01u
#define UCI_ACCEPT     0x02u
#define UCI_ABORT      0x04u
#define UCI_ERROR      0x08u
#define UCI_WAIT       60000u
#define UCI_PASSES     4u

static unsigned int setup_uci_base_addr;

static unsigned char quiet_idle(unsigned char st) {
    return (unsigned char)(((st & UCI_STATE_MASK) == UCI_IDLE) &&
        ((st & (UCI_BUSY | UCI_ACCEPT | UCI_ABORT | UCI_ERROR)) == 0u));
}

static unsigned char probe(unsigned int base) {
    setup_uci_asm_set_base(base);
    if ((setup_uci_asm_id() & 0x7Fu) == 0x49u) {
        setup_uci_base_addr = base;
        return 1u;
    }
    return 0u;
}

unsigned char setup_uci_detect(void) {
    static const unsigned int bases[] = {0xDF1Cu, 0xDE1Cu, 0xDFFCu};
    unsigned char i;
    if (setup_uci_base_addr != 0u && probe(setup_uci_base_addr)) return 1u;
    setup_uci_base_addr = 0u;
    for (i = 0u; i < sizeof(bases) / sizeof(bases[0]); ++i)
        if (probe(bases[i])) return 1u;
    return 0u;
}

unsigned int setup_uci_base(void) {
    return setup_uci_detect() ? setup_uci_base_addr : 0u;
}

static unsigned char wait_idle(void) {
    unsigned char pass;
    unsigned int tries;
    unsigned char st;
    for (pass = 0u; pass < UCI_PASSES; ++pass)
        for (tries = 0u; tries < UCI_WAIT; ++tries) {
            st = setup_uci_asm_status();
            if (st & UCI_ERROR) return 0u;
            if (quiet_idle(st)) return 1u;
        }
    return 0u;
}

static unsigned char wait_response(void) {
    unsigned char pass;
    unsigned int tries;
    unsigned char st;
    for (pass = 0u; pass < UCI_PASSES; ++pass)
        for (tries = 0u; tries < UCI_WAIT; ++tries) {
            st = setup_uci_asm_status();
            if (st & UCI_ERROR) return 0u;
            st &= UCI_STATE_MASK;
            if (st == UCI_LAST || st == UCI_MORE) return 1u;
        }
    return 0u;
}

static unsigned char wait_advance(unsigned char old_state) {
    unsigned char pass;
    unsigned int tries;
    unsigned char st;
    for (pass = 0u; pass < UCI_PASSES; ++pass)
        for (tries = 0u; tries < UCI_WAIT; ++tries) {
            st = setup_uci_asm_status();
            if (st & UCI_ERROR) return 0u;
            if ((st & UCI_STATE_MASK) != old_state) return 1u;
        }
    return 0u;
}

static unsigned char sync_interface(void) {
    unsigned char pass;
    unsigned int tries;
    unsigned char st;
    unsigned char state;
    for (pass = 0u; pass < UCI_PASSES; ++pass)
        for (tries = 0u; tries < UCI_WAIT; ++tries) {
            st = setup_uci_asm_status();
            state = (unsigned char)(st & UCI_STATE_MASK);
            if (st & UCI_ERROR) { setup_uci_asm_clear_error(); continue; }
            if (st & UCI_ABORT) continue;
            if (st & UCI_DATA) { (void)setup_uci_asm_read_data(); continue; }
            if (st & UCI_STAT) { (void)setup_uci_asm_read_stat(); continue; }
            if (state == UCI_MORE) {
                if (!setup_uci_asm_accept_more_transition(state)) return 0u;
                continue;
            }
            if (state == UCI_LAST) {
                setup_uci_asm_accept_data();
                if (!wait_advance(state)) return 0u;
                continue;
            }
            if (quiet_idle(st)) return 1u;
        }
    return 0u;
}

static unsigned char recover(void) {
    unsigned char st = setup_uci_asm_status();
    if ((st & UCI_ABORT) == 0u) setup_uci_asm_abort();
    return sync_interface();
}

static void fail_wait(SetupUciTransfer *xfer) {
    unsigned char st;
    if (xfer == 0) return;
    st = setup_uci_asm_status();
    xfer->last_status = st;
    xfer->flags |= (st & UCI_ERROR) ? SETUP_UCI_ERROR : SETUP_UCI_TIMEOUT;
}

unsigned char setup_uci_command(const unsigned char *cmd,
                                unsigned int cmd_len,
                                SetupUciTransfer *xfer) {
    unsigned int i;
    unsigned int drain;
    unsigned char st;
    unsigned char state;

    if (xfer != 0) {
        xfer->data_len = xfer->stat_len = 0u;
        xfer->data_seen = xfer->stat_seen = 0u;
        xfer->block_count = 0u;
        xfer->flags = xfer->last_status = 0u;
    }
    if (!setup_uci_detect() || cmd == 0 || cmd_len == 0u || cmd_len > 896u)
        return 0u;
    if (!sync_interface() && !recover()) {
        if (xfer != 0) xfer->flags |= SETUP_UCI_TIMEOUT;
        return 0u;
    }
    for (i = 0u; i < cmd_len; ++i) setup_uci_asm_write_cmd(cmd[i]);
    setup_uci_asm_push_cmd();
    /* PUSH is asynchronous: immediate IDLE is not completion. */
    if (!wait_response()) { fail_wait(xfer); (void)recover(); return 0u; }

    for (;;) {
        st = setup_uci_asm_status();
        if (xfer != 0) xfer->last_status = st;
        if (st & UCI_ERROR) {
            if (xfer != 0) xfer->flags |= SETUP_UCI_ERROR;
            (void)recover(); return 0u;
        }
        state = (unsigned char)(st & UCI_STATE_MASK);
        if (state != UCI_LAST && state != UCI_MORE) {
            if (!wait_response()) { fail_wait(xfer); (void)recover(); return 0u; }
            continue;
        }
        for (drain = 0u; drain < UCI_WAIT; ++drain) {
            st = setup_uci_asm_status();
            if (st & UCI_ERROR) {
                if (xfer != 0) xfer->flags |= SETUP_UCI_ERROR;
                (void)recover(); return 0u;
            }
            if (st & UCI_DATA) {
                if (xfer != 0) ++xfer->data_seen;
                if (xfer != 0 && xfer->data != 0 &&
                    xfer->data_len < xfer->data_cap)
                    xfer->data[xfer->data_len++] = setup_uci_asm_read_data();
                else {
                    (void)setup_uci_asm_read_data();
                    if (xfer != 0) xfer->flags |= SETUP_UCI_TRUNC_DATA;
                }
                continue;
            }
            if (st & UCI_STAT) {
                if (xfer != 0) ++xfer->stat_seen;
                if (xfer != 0 && xfer->stat != 0 &&
                    xfer->stat_len < xfer->stat_cap)
                    xfer->stat[xfer->stat_len++] = setup_uci_asm_read_stat();
                else {
                    (void)setup_uci_asm_read_stat();
                    if (xfer != 0) xfer->flags |= SETUP_UCI_TRUNC_STAT;
                }
                continue;
            }
            break;
        }
        if (drain == UCI_WAIT) {
            if (xfer != 0) xfer->flags |= SETUP_UCI_TIMEOUT;
            (void)recover(); return 0u;
        }
        if (xfer != 0 && xfer->on_block != 0) {
            xfer->on_block(xfer->data, xfer->data_len,
                           xfer->stat, xfer->stat_len);
            ++xfer->block_count;
            xfer->data_len = xfer->stat_len = 0u;
        }
        if (state == UCI_LAST) {
            setup_uci_asm_accept_data();
            if (!wait_idle()) { fail_wait(xfer); (void)recover(); return 0u; }
            return 1u;
        }
        /* The ReadyFS/ReadyIRC high-MHz fix keeps DATA_ACC and observation of
         * MORE->BUSY adjacent in assembly so 64 MHz cannot miss the edge. */
        if (!setup_uci_asm_accept_more_transition(state) || !wait_response()) {
            fail_wait(xfer); (void)recover(); return 0u;
        }
    }
}
