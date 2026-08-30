#include "uz_uci.h"

#define UCI_STAT_DATA   0x80u
#define UCI_STAT_STAT   0x40u
#define UCI_STATE_MASK  0x30u
#define UCI_STATE_IDLE  0x00u
#define UCI_STATE_LAST  0x20u
#define UCI_STATE_MORE  0x30u
#define UCI_STAT_BUSY   0x01u
#define UCI_STAT_ACCEPT 0x02u
#define UCI_STAT_ABORT  0x04u
#define UCI_STAT_ERROR  0x08u

#define UCI_WAIT_SYNC     60000u
#define UCI_WAIT_RESPONSE 60000u
/* Four finite passes preserve the proven 16 MHz wall-time at 64 MHz. */
#define UCI_WAIT_PASSES   4u

static unsigned int uci_base_addr;

static unsigned char id_matches(unsigned char id) {
    return (unsigned char)((id & UZ_UCI_ID_MASK) == UZ_UCI_ID_MATCH);
}

static unsigned char probe_base(unsigned int base) {
    uz_uci_asm_set_base(base);
    if (id_matches(uz_uci_asm_id())) {
        uci_base_addr = base;
        return 1u;
    }
    return 0u;
}

static unsigned char quiet_idle(unsigned char status) {
    return (unsigned char)(
        ((status & UCI_STATE_MASK) == UCI_STATE_IDLE) &&
        ((status & (UCI_STAT_BUSY | UCI_STAT_ACCEPT |
                    UCI_STAT_ABORT | UCI_STAT_ERROR)) == 0u));
}

static unsigned char wait_idle(void) {
    unsigned char pass;
    unsigned int tries;
    unsigned char status;

    for (pass = 0u; pass < UCI_WAIT_PASSES; ++pass) {
        for (tries = 0u; tries < UCI_WAIT_RESPONSE; ++tries) {
            status = uz_uci_asm_status();
            if ((status & UCI_STAT_ERROR) != 0u) return 0u;
            if (quiet_idle(status)) return 1u;
        }
    }
    return 0u;
}

static unsigned char wait_response_state(void) {
    unsigned char pass;
    unsigned int tries;
    unsigned char status;
    unsigned char state;

    for (pass = 0u; pass < UCI_WAIT_PASSES; ++pass) {
        for (tries = 0u; tries < UCI_WAIT_RESPONSE; ++tries) {
            status = uz_uci_asm_status();
            if ((status & UCI_STAT_ERROR) != 0u) return 0u;
            state = (unsigned char)(status & UCI_STATE_MASK);
            if (state == UCI_STATE_LAST || state == UCI_STATE_MORE) return 1u;
        }
    }
    return 0u;
}

static unsigned char wait_accept_advance(unsigned char old_state) {
    unsigned char pass;
    unsigned int tries;
    unsigned char status;

    /* DATA_ACC is asynchronous. An unchanged state still belongs to the
     * acknowledged block and is never evidence that a new block is ready. */
    for (pass = 0u; pass < UCI_WAIT_PASSES; ++pass) {
        for (tries = 0u; tries < UCI_WAIT_RESPONSE; ++tries) {
            status = uz_uci_asm_status();
            if ((status & UCI_STAT_ERROR) != 0u) return 0u;
            if ((status & UCI_STATE_MASK) != old_state) return 1u;
        }
    }
    return 0u;
}

static unsigned char sync_interface(void) {
    unsigned char pass;
    unsigned int tries;
    unsigned char status;
    unsigned char state;

    for (pass = 0u; pass < UCI_WAIT_PASSES; ++pass) {
        for (tries = 0u; tries < UCI_WAIT_SYNC; ++tries) {
            status = uz_uci_asm_status();
            state = (unsigned char)(status & UCI_STATE_MASK);
            if ((status & UCI_STAT_ERROR) != 0u) {
                uz_uci_asm_clear_error();
                continue;
            }
            if ((status & UCI_STAT_ABORT) != 0u) continue;
            if ((status & UCI_STAT_DATA) != 0u) {
                (void)uz_uci_asm_read_data();
                continue;
            }
            if ((status & UCI_STAT_STAT) != 0u) {
                (void)uz_uci_asm_read_stat();
                continue;
            }
            if (state == UCI_STATE_MORE) {
                if (!uz_uci_asm_accept_more_transition(state)) return 0u;
                continue;
            }
            if (state == UCI_STATE_LAST) {
                uz_uci_asm_accept_data();
                if (!wait_accept_advance(state)) return 0u;
                continue;
            }
            if (quiet_idle(status)) return 1u;
        }
    }
    return 0u;
}

static unsigned char abort_and_recover(void) {
    unsigned char status;

    status = uz_uci_asm_status();
    if ((status & UCI_STAT_ABORT) == 0u) uz_uci_asm_abort();
    return sync_interface();
}

static void record_wait_failure(UzUciTransfer *transfer) {
    unsigned char status;

    if (transfer == 0) return;
    status = uz_uci_asm_status();
    transfer->last_status = status;
    if ((status & UCI_STAT_ERROR) != 0u) transfer->flags |= UZ_UCI_ERROR;
    else transfer->flags |= UZ_UCI_TIMEOUT;
}

unsigned char uz_uci_detect(void) {
    static const unsigned int bases[] = {0xDF1Cu, 0xDE1Cu, 0xDFFCu};
    unsigned char index;

    if (uci_base_addr != 0u) {
        if (probe_base(uci_base_addr)) return 1u;
        uci_base_addr = 0u;
    }
    for (index = 0u; index < sizeof(bases) / sizeof(bases[0]); ++index) {
        if (probe_base(bases[index])) return 1u;
    }
    return 0u;
}

unsigned int uz_uci_base(void) {
    return uz_uci_detect() ? uci_base_addr : 0u;
}

unsigned char uz_uci_command(const unsigned char *command,
                             unsigned int command_len,
                             UzUciTransfer *transfer) {
    unsigned int index;
    unsigned int drain_tries;
    unsigned char status;
    unsigned char state;

    if (transfer != 0) {
        transfer->data_len = 0u;
        transfer->stat_len = 0u;
        transfer->data_seen = 0u;
        transfer->stat_seen = 0u;
        transfer->block_count = 0u;
        transfer->flags = 0u;
        transfer->last_status = 0u;
    }
    if (!uz_uci_detect() || command == 0 || command_len == 0u ||
        command_len > 896u) return 0u;
    if (!sync_interface() && !abort_and_recover()) {
        if (transfer != 0) transfer->flags |= UZ_UCI_TIMEOUT;
        return 0u;
    }
    for (index = 0u; index < command_len; ++index) {
        uz_uci_asm_write_cmd(command[index]);
    }
    uz_uci_asm_push_cmd();

    /* PUSH_CMD is asynchronous. Immediate IDLE is not completion; only a
     * DATA_LAST or DATA_MORE state begins response processing. */
    if (!wait_response_state()) {
        record_wait_failure(transfer);
        (void)abort_and_recover();
        return 0u;
    }

    for (;;) {
        status = uz_uci_asm_status();
        if (transfer != 0) transfer->last_status = status;
        if ((status & UCI_STAT_ERROR) != 0u) {
            if (transfer != 0) transfer->flags |= UZ_UCI_ERROR;
            (void)abort_and_recover();
            return 0u;
        }
        state = (unsigned char)(status & UCI_STATE_MASK);
        if (state != UCI_STATE_LAST && state != UCI_STATE_MORE) {
            if (!wait_response_state()) {
                record_wait_failure(transfer);
                (void)abort_and_recover();
                return 0u;
            }
            continue;
        }

        /* Availability flags are authoritative. This bound only detects a
         * wedged bit and exceeds the documented 896+256-byte queues. */
        for (drain_tries = 0u; drain_tries < UCI_WAIT_RESPONSE; ++drain_tries) {
            status = uz_uci_asm_status();
            if (transfer != 0) transfer->last_status = status;
            if ((status & UCI_STAT_ERROR) != 0u) {
                if (transfer != 0) transfer->flags |= UZ_UCI_ERROR;
                (void)abort_and_recover();
                return 0u;
            }
            if ((status & UCI_STAT_DATA) != 0u) {
                if (transfer != 0) ++transfer->data_seen;
                if (transfer != 0 && transfer->data != 0 &&
                    transfer->data_len < transfer->data_cap) {
                    transfer->data[transfer->data_len++] = uz_uci_asm_read_data();
                } else {
                    (void)uz_uci_asm_read_data();
                    if (transfer != 0) transfer->flags |= UZ_UCI_TRUNC_DATA;
                }
                continue;
            }
            if ((status & UCI_STAT_STAT) != 0u) {
                if (transfer != 0) ++transfer->stat_seen;
                if (transfer != 0 && transfer->stat != 0 &&
                    transfer->stat_len < transfer->stat_cap) {
                    transfer->stat[transfer->stat_len++] = uz_uci_asm_read_stat();
                } else {
                    (void)uz_uci_asm_read_stat();
                    if (transfer != 0) transfer->flags |= UZ_UCI_TRUNC_STAT;
                }
                continue;
            }
            break;
        }
        if (drain_tries == UCI_WAIT_RESPONSE) {
            if (transfer != 0) transfer->flags |= UZ_UCI_TIMEOUT;
            (void)abort_and_recover();
            return 0u;
        }
        if (transfer != 0 && transfer->on_block != 0) {
            transfer->on_block(transfer->data, transfer->data_len,
                               transfer->stat, transfer->stat_len);
            ++transfer->block_count;
            transfer->data_len = 0u;
            transfer->stat_len = 0u;
        }
        if (state == UCI_STATE_LAST) {
            uz_uci_asm_accept_data();
            if (!wait_idle()) {
                record_wait_failure(transfer);
                (void)abort_and_recover();
                return 0u;
            }
            return 1u;
        }

        /* DATA_ACC is asynchronous. Observe MORE->BUSY adjacent to the write
         * so a 64 MHz C call boundary cannot miss the required transition. */
        if (!uz_uci_asm_accept_more_transition(state) ||
            !wait_response_state()) {
            record_wait_failure(transfer);
            (void)abort_and_recover();
            return 0u;
        }
    }
}
