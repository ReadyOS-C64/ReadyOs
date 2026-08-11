#include "ucitest_uci.h"

#define UCI_STAT_DATA  0x80
#define UCI_STAT_STAT  0x40
#define UCI_STATE_MASK 0x30
#define UCI_STATE_IDLE 0x00
#define UCI_STATE_LAST 0x20
#define UCI_STATE_MORE 0x30
#define UCI_STAT_BUSY  0x01
#define UCI_STAT_ACCEPT 0x02
#define UCI_STAT_ABORT 0x04
#define UCI_STAT_ERROR 0x08

#define UCI_WAIT_SYNC     60000u
#define UCI_WAIT_RESPONSE 60000u
/* Four finite passes preserve the proven 16 MHz wall-time at 64 MHz. */
#define UCI_WAIT_PASSES   4u

static unsigned int uci_base_addr;

static unsigned char id_matches(unsigned char id) {
    return (unsigned char)((id & UCITEST_UCI_ID_MASK) == UCITEST_UCI_ID_MATCH);
}

static unsigned char probe_base(unsigned int base) {
    ucitest_uci_asm_set_base(base);
    if (id_matches(ucitest_uci_asm_id())) {
        uci_base_addr = base;
        return 1u;
    }
    return 0u;
}

static unsigned char status_is_quiet_idle(unsigned char st) {
    return (unsigned char)(((st & UCI_STATE_MASK) == UCI_STATE_IDLE) &&
                           ((st & (UCI_STAT_BUSY | UCI_STAT_ACCEPT |
                                   UCI_STAT_ABORT | UCI_STAT_ERROR)) == 0u));
}

static unsigned char wait_idle(void) {
    unsigned char pass;
    unsigned int tries;
    unsigned char st;

    for (pass = 0u; pass < UCI_WAIT_PASSES; ++pass) {
        for (tries = 0u; tries < UCI_WAIT_RESPONSE; ++tries) {
            st = ucitest_uci_asm_status();
            if ((st & UCI_STAT_ERROR) != 0u) {
                return 0u;
            }
            if (status_is_quiet_idle(st)) {
                return 1u;
            }
        }
    }
    return 0u;
}

static unsigned char wait_response_state(void) {
    unsigned char pass;
    unsigned int tries;
    unsigned char st;
    unsigned char state;

    for (pass = 0u; pass < UCI_WAIT_PASSES; ++pass) {
        for (tries = 0u; tries < UCI_WAIT_RESPONSE; ++tries) {
            st = ucitest_uci_asm_status();
            if ((st & UCI_STAT_ERROR) != 0u) {
                return 0u;
            }
            state = (unsigned char)(st & UCI_STATE_MASK);
            if (state == UCI_STATE_LAST || state == UCI_STATE_MORE) {
                return 1u;
            }
        }
    }
    return 0u;
}

static unsigned char wait_accept_advance(unsigned char old_state) {
    unsigned char pass;
    unsigned int tries;
    unsigned char st;

    /* DATA_ACC is asynchronous like PUSH_CMD. Require the documented state
     * transition before reusing a LAST/MORE state value. */
    for (pass = 0u; pass < UCI_WAIT_PASSES; ++pass) {
        for (tries = 0u; tries < UCI_WAIT_RESPONSE; ++tries) {
            st = ucitest_uci_asm_status();
            if ((st & UCI_STAT_ERROR) != 0u) {
                return 0u;
            }
            if ((st & UCI_STATE_MASK) != old_state) {
                return 1u;
            }
        }
    }
    return 0u;
}

static unsigned char sync_interface(void) {
    unsigned char pass;
    unsigned int tries;
    unsigned char st;
    unsigned char state;

    for (pass = 0u; pass < UCI_WAIT_PASSES; ++pass) {
        for (tries = 0u; tries < UCI_WAIT_SYNC; ++tries) {
            st = ucitest_uci_asm_status();
            state = (unsigned char)(st & UCI_STATE_MASK);

            if ((st & UCI_STAT_ERROR) != 0u) {
                ucitest_uci_asm_clear_error();
                continue;
            }
            if ((st & UCI_STAT_ABORT) != 0u) {
                /* ABORT_P is pending already; poll/service it instead of
                 * repeatedly requesting another asynchronous abort. */
                continue;
            }
            if ((st & UCI_STAT_DATA) != 0u) {
                (void)ucitest_uci_asm_read_data();
                continue;
            }
            if ((st & UCI_STAT_STAT) != 0u) {
                (void)ucitest_uci_asm_read_stat();
                continue;
            }
            if (state == UCI_STATE_LAST || state == UCI_STATE_MORE) {
                ucitest_uci_asm_accept_data();
                if (!wait_accept_advance(state)) {
                    return 0u;
                }
                continue;
            }
            if (status_is_quiet_idle(st)) {
                return 1u;
            }
        }
    }

    return 0u;
}

static unsigned char abort_and_recover(void) {
    unsigned char st;

    /* Request once, then service queues, ERROR, and DATA_ACC until all pending
     * control bits clear at IDLE. The poll limit remains only a failure bound. */
    st = ucitest_uci_asm_status();
    if ((st & UCI_STAT_ABORT) == 0u) {
        ucitest_uci_asm_abort();
    }
    return sync_interface();
}

static void record_wait_failure(UciTestTransfer *xfer) {
    unsigned char st;

    if (xfer == 0) {
        return;
    }
    st = ucitest_uci_asm_status();
    xfer->last_status = st;
    if ((st & UCI_STAT_ERROR) != 0u) {
        xfer->flags |= UCITEST_UCI_ERROR;
    } else {
        xfer->flags |= UCITEST_UCI_TIMEOUT;
    }
}

unsigned char ucitest_uci_detect(void) {
    static const unsigned int bases[] = {
        0xDF1Cu,
        0xDE1Cu,
        0xDFFCu
    };
    unsigned char i;

    if (uci_base_addr != 0u) {
        if (probe_base(uci_base_addr)) {
            return 1u;
        }
        uci_base_addr = 0u;
    }
    for (i = 0u; i < sizeof(bases) / sizeof(bases[0]); ++i) {
        if (probe_base(bases[i])) {
            return 1u;
        }
    }
    return 0u;
}

unsigned int ucitest_uci_base(void) {
    if (!ucitest_uci_detect()) {
        return 0u;
    }
    return uci_base_addr;
}

unsigned char ucitest_uci_id(void) {
    if (!ucitest_uci_detect()) {
        return 0u;
    }
    return ucitest_uci_asm_id();
}

unsigned char ucitest_uci_status(void) {
    if (!ucitest_uci_detect()) {
        return 0u;
    }
    return ucitest_uci_asm_status();
}

void ucitest_uci_abort(void) {
    if (ucitest_uci_detect()) {
        (void)abort_and_recover();
    }
}

void ucitest_uci_clear_error(void) {
    if (ucitest_uci_detect()) {
        ucitest_uci_asm_clear_error();
    }
}

unsigned char ucitest_uci_command(const unsigned char *cmd,
                                  unsigned int cmd_len,
                                  UciTestTransfer *xfer) {
    unsigned int i;
    unsigned int drain_tries;
    unsigned char st;
    unsigned char state;

    if (xfer != 0) {
        xfer->data_len = 0u;
        xfer->stat_len = 0u;
        xfer->flags = 0u;
        xfer->last_status = 0u;
    }
    if (!ucitest_uci_detect() || cmd == 0 || cmd_len == 0u) {
        return 0u;
    }
    if (!sync_interface()) {
        if (!abort_and_recover()) {
            if (xfer != 0) {
                xfer->flags |= UCITEST_UCI_TIMEOUT;
            }
            return 0u;
        }
    }

    for (i = 0u; i < cmd_len; ++i) {
        ucitest_uci_asm_write_cmd(cmd[i]);
    }
    ucitest_uci_asm_push_cmd();

    /* PUSH_CMD is asynchronous.  An immediate IDLE sample only means that
     * the Ultimate has not observed the push yet; wait for a response state. */
    if (!wait_response_state()) {
        record_wait_failure(xfer);
        (void)abort_and_recover();
        return 0u;
    }

    for (;;) {
        st = ucitest_uci_asm_status();
        if (xfer != 0) {
            xfer->last_status = st;
        }
        if ((st & UCI_STAT_ERROR) != 0u) {
            if (xfer != 0) {
                xfer->flags |= UCITEST_UCI_ERROR;
            }
            (void)abort_and_recover();
            return 0u;
        }
        state = (unsigned char)(st & UCI_STATE_MASK);
        if (state != UCI_STATE_LAST && state != UCI_STATE_MORE) {
            if (!wait_response_state()) {
                record_wait_failure(xfer);
                (void)abort_and_recover();
                return 0u;
            }
            continue;
        }

        /* A queue-availability bit is authoritative, but it cannot be allowed
         * to spin forever if hardware is wedged. The count is a failure bound
         * large enough for the documented 896+256 byte queues. */
        for (drain_tries = 0u;
             drain_tries < UCI_WAIT_RESPONSE;
             ++drain_tries) {
            st = ucitest_uci_asm_status();
            if (xfer != 0) {
                xfer->last_status = st;
            }
            if ((st & UCI_STAT_ERROR) != 0u) {
                if (xfer != 0) {
                    xfer->flags |= UCITEST_UCI_ERROR;
                }
                (void)abort_and_recover();
                return 0u;
            }
            if ((st & UCI_STAT_DATA) != 0u) {
                if (xfer != 0 && xfer->data != 0 &&
                    xfer->data_len < xfer->data_cap) {
                    xfer->data[xfer->data_len] = ucitest_uci_asm_read_data();
                    ++xfer->data_len;
                } else {
                    (void)ucitest_uci_asm_read_data();
                    if (xfer != 0) {
                        xfer->flags |= UCITEST_UCI_TRUNC_DATA;
                    }
                }
                continue;
            }
            if ((st & UCI_STAT_STAT) != 0u) {
                if (xfer != 0 && xfer->stat != 0 &&
                    xfer->stat_len < xfer->stat_cap) {
                    xfer->stat[xfer->stat_len] = ucitest_uci_asm_read_stat();
                    ++xfer->stat_len;
                } else {
                    (void)ucitest_uci_asm_read_stat();
                    if (xfer != 0) {
                        xfer->flags |= UCITEST_UCI_TRUNC_STAT;
                    }
                }
                continue;
            }
            break;
        }
        if (drain_tries == UCI_WAIT_RESPONSE) {
            if (xfer != 0) {
                xfer->flags |= UCITEST_UCI_TIMEOUT;
            }
            (void)abort_and_recover();
            return 0u;
        }

        ucitest_uci_asm_accept_data();
        if (state == UCI_STATE_LAST) {
            if (!wait_idle()) {
                record_wait_failure(xfer);
                (void)abort_and_recover();
                return 0u;
            }
            break;
        }
        if (!wait_accept_advance(state)) {
            record_wait_failure(xfer);
            (void)abort_and_recover();
            return 0u;
        }
        if (!wait_response_state()) {
            record_wait_failure(xfer);
            (void)abort_and_recover();
            return 0u;
        }
    }
    return 1u;
}
