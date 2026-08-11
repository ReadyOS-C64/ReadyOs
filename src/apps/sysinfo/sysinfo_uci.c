#include "sysinfo_uci.h"

#define UCI_ID_MASK    0x7F
#define UCI_ID_MATCH   0x49
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

static unsigned char uci_id_matches(unsigned char id) {
    return (unsigned char)((id & UCI_ID_MASK) == UCI_ID_MATCH);
}

static unsigned char uci_probe_base(unsigned int base) {
    sysinfo_uci_asm_set_base(base);
    if (uci_id_matches(sysinfo_uci_asm_id())) {
        uci_base_addr = base;
        return 1u;
    }
    return 0u;
}

/* IDLE is reusable only after every asynchronous control request has cleared.
 * This is deliberately stricter than testing the state nibble or CMD_BUSY. */
static unsigned char uci_status_is_quiet_idle(unsigned char st) {
    return (unsigned char)(((st & UCI_STATE_MASK) == UCI_STATE_IDLE) &&
                           ((st & (UCI_STAT_BUSY | UCI_STAT_ACCEPT |
                                   UCI_STAT_ABORT | UCI_STAT_ERROR)) == 0u));
}

static unsigned char uci_wait_idle(void) {
    unsigned char pass;
    unsigned int tries;
    unsigned char st;

    for (pass = 0u; pass < UCI_WAIT_PASSES; ++pass) {
        for (tries = 0u; tries < UCI_WAIT_RESPONSE; ++tries) {
            st = sysinfo_uci_asm_status();
            if ((st & UCI_STAT_ERROR) != 0u) {
                return 0u;
            }
            if (uci_status_is_quiet_idle(st)) {
                return 1u;
            }
        }
    }
    return 0u;
}

static unsigned char uci_wait_response_state(void) {
    unsigned char pass;
    unsigned int tries;
    unsigned char st;

    for (pass = 0u; pass < UCI_WAIT_PASSES; ++pass) {
        for (tries = 0u; tries < UCI_WAIT_RESPONSE; ++tries) {
            st = sysinfo_uci_asm_status();
            if ((st & UCI_STAT_ERROR) != 0u) {
                return 0u;
            }
            if ((st & UCI_STATE_MASK) == UCI_STATE_LAST ||
                (st & UCI_STATE_MASK) == UCI_STATE_MORE) {
                return 1u;
            }
        }
    }
    return 0u;
}

static unsigned char uci_wait_accept_advance(unsigned char old_state) {
    unsigned char pass;
    unsigned int tries;
    unsigned char st;

    /* DATA_ACC is asynchronous too. The documented transition must leave the
     * just-drained data state before that state value can be reused. */
    for (pass = 0u; pass < UCI_WAIT_PASSES; ++pass) {
        for (tries = 0u; tries < UCI_WAIT_RESPONSE; ++tries) {
            st = sysinfo_uci_asm_status();
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

static unsigned char uci_sync_interface(void) {
    unsigned char pass;
    unsigned int tries;
    unsigned char st;
    unsigned char state;

    for (pass = 0u; pass < UCI_WAIT_PASSES; ++pass) {
        for (tries = 0u; tries < UCI_WAIT_SYNC; ++tries) {
            st = sysinfo_uci_asm_status();
            state = (unsigned char)(st & UCI_STATE_MASK);

            if ((st & UCI_STAT_ERROR) != 0u) {
                sysinfo_uci_asm_clear_error();
                continue;
            }
            if ((st & UCI_STAT_ABORT) != 0u) {
                /* ABORT_P is already an outstanding asynchronous request. Do not
                 * re-issue it; keep servicing until the pending bit clears. */
                continue;
            }
            if ((st & UCI_STAT_DATA) != 0u) {
                (void)sysinfo_uci_asm_read_data();
                continue;
            }
            if ((st & UCI_STAT_STAT) != 0u) {
                (void)sysinfo_uci_asm_read_stat();
                continue;
            }
            if (state == UCI_STATE_LAST || state == UCI_STATE_MORE) {
                sysinfo_uci_asm_accept_data();
                if (!uci_wait_accept_advance(state)) {
                    return 0u;
                }
                continue;
            }
            if (uci_status_is_quiet_idle(st)) {
                return 1u;
            }
        }
    }

    return 0u;
}

static unsigned char uci_abort_and_recover(void) {
    unsigned char st;

    /* ABORT is asynchronous. Recovery owns queue draining, ERROR clearing,
     * DATA_ACC for orphaned blocks, and the final fully quiet IDLE wait. Read
     * ABORT_P first: a set bit is already a pending request and must never be
     * treated as permission to issue ABORT again. */
    st = sysinfo_uci_asm_status();
    if ((st & UCI_STAT_ABORT) == 0u) {
        sysinfo_uci_asm_abort();
    }
    return uci_sync_interface();
}

unsigned char sysinfo_uci_detect(void) {
    static const unsigned int bases[] = {
        0xDF1Cu,
        0xDE1Cu,
        0xDFFCu
    };
    unsigned char i;

    if (uci_base_addr != 0u) {
        if (uci_probe_base(uci_base_addr)) {
            return 1u;
        }
        uci_base_addr = 0u;
    }

    for (i = 0u; i < sizeof(bases) / sizeof(bases[0]); ++i) {
        if (uci_probe_base(bases[i])) {
            return 1u;
        }
    }
    return 0u;
}

unsigned int sysinfo_uci_base(void) {
    if (!sysinfo_uci_detect()) {
        return 0u;
    }
    return uci_base_addr;
}

unsigned char sysinfo_uci_command(const unsigned char *cmd,
                                  unsigned char cmd_len,
                                  unsigned char *data,
                                  unsigned char data_cap,
                                  unsigned char *data_len,
                                  unsigned char *stat,
                                  unsigned char stat_cap,
                                  unsigned char *stat_len) {
    unsigned char i;
    unsigned char st;
    unsigned char state;
    unsigned char data_pos;
    unsigned char stat_pos;
    unsigned int drain_tries;

    data_pos = 0u;
    stat_pos = 0u;
    if (data_len != 0) {
        *data_len = 0u;
    }
    if (stat_len != 0) {
        *stat_len = 0u;
    }
    if (!sysinfo_uci_detect() || cmd == 0 || cmd_len == 0u) {
        return 0u;
    }

    if (!uci_sync_interface()) {
        if (!uci_abort_and_recover()) {
            return 0u;
        }
    }

    for (i = 0u; i < cmd_len; ++i) {
        sysinfo_uci_asm_write_cmd(cmd[i]);
    }
    sysinfo_uci_asm_push_cmd();

    /* PUSH_CMD is asynchronous.  An immediate IDLE observation means the
     * Ultimate has not consumed the push yet; completion requires LAST/MORE. */
    if (!uci_wait_response_state()) {
        (void)uci_abort_and_recover();
        return 0u;
    }

    for (;;) {
        st = sysinfo_uci_asm_status();
        if ((st & UCI_STAT_ERROR) != 0u) {
            (void)uci_abort_and_recover();
            return 0u;
        }
        state = (unsigned char)(st & UCI_STATE_MASK);
        if (state != UCI_STATE_LAST && state != UCI_STATE_MORE) {
            if (!uci_wait_response_state()) {
                (void)uci_abort_and_recover();
                return 0u;
            }
            continue;
        }

        /* Availability flags, not a CPU-speed-dependent quiet delay, delimit
         * each response block.  Always drain overflow before DATA_ACC. */
        /* The queue capacities are finite, but a wedged DATA_AV/STAT_AV bit
         * must not turn recovery into an infinite loop. This counter is only
         * a failure bound; availability flags remain the sole pacing source. */
        for (drain_tries = 0u;
             drain_tries < UCI_WAIT_RESPONSE;
             ++drain_tries) {
            st = sysinfo_uci_asm_status();
            if ((st & UCI_STAT_ERROR) != 0u) {
                (void)uci_abort_and_recover();
                return 0u;
            }
            if ((st & UCI_STAT_DATA) != 0u) {
                if (data_pos < data_cap && data != 0) {
                    data[data_pos] = sysinfo_uci_asm_read_data();
                    ++data_pos;
                } else {
                    (void)sysinfo_uci_asm_read_data();
                }
                continue;
            }
            if ((st & UCI_STAT_STAT) != 0u) {
                if (stat_pos < stat_cap && stat != 0) {
                    stat[stat_pos] = sysinfo_uci_asm_read_stat();
                    ++stat_pos;
                } else {
                    (void)sysinfo_uci_asm_read_stat();
                }
                continue;
            }
            break;
        }
        if (drain_tries == UCI_WAIT_RESPONSE) {
            (void)uci_abort_and_recover();
            return 0u;
        }

        sysinfo_uci_asm_accept_data();
        if (state == UCI_STATE_LAST) {
            if (!uci_wait_idle()) {
                (void)uci_abort_and_recover();
                return 0u;
            }
            break;
        }
        if (!uci_wait_accept_advance(state)) {
            (void)uci_abort_and_recover();
            return 0u;
        }
        if (!uci_wait_response_state()) {
            (void)uci_abort_and_recover();
            return 0u;
        }
    }

    if (data_len != 0) {
        *data_len = data_pos;
    }
    if (stat_len != 0) {
        *stat_len = stat_pos;
    }
    return 1u;
}
