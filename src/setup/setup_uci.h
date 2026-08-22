#ifndef SETUP_UCI_H
#define SETUP_UCI_H

#define SETUP_UCI_TRUNC_DATA 0x01u
#define SETUP_UCI_TRUNC_STAT 0x02u
#define SETUP_UCI_TIMEOUT    0x04u
#define SETUP_UCI_ERROR      0x08u

typedef void (*SetupUciBlockHandler)(const unsigned char *data,
                                     unsigned int data_len,
                                     const unsigned char *stat,
                                     unsigned int stat_len);

typedef struct {
    unsigned char *data;
    unsigned int data_cap;
    unsigned int data_len;
    unsigned char *stat;
    unsigned int stat_cap;
    unsigned int stat_len;
    unsigned int data_seen;
    unsigned int stat_seen;
    unsigned int block_count;
    SetupUciBlockHandler on_block;
    unsigned char flags;
    unsigned char last_status;
} SetupUciTransfer;

unsigned char setup_uci_detect(void);
unsigned int setup_uci_base(void);
unsigned char setup_uci_command(const unsigned char *cmd,
                                unsigned int cmd_len,
                                SetupUciTransfer *xfer);

/* SETUP's only UCI transaction gateway. It owns quiet-IDLE synchronization,
 * asynchronous PUSH/ABORT handling, complete data/status queue draining,
 * DATA_ACC transitions, and the final quiet-IDLE wait. Callers never pace it. */

void __fastcall__ setup_uci_asm_set_base(unsigned int base);
unsigned char __fastcall__ setup_uci_asm_write_cmd(unsigned char value);
unsigned char setup_uci_asm_id(void);
unsigned char setup_uci_asm_status(void);
unsigned char setup_uci_asm_read_data(void);
unsigned char setup_uci_asm_read_stat(void);
void setup_uci_asm_push_cmd(void);
void setup_uci_asm_accept_data(void);
unsigned char __fastcall__ setup_uci_asm_accept_more_transition(unsigned char old_state);
void setup_uci_asm_abort(void);
void setup_uci_asm_clear_error(void);

#endif
