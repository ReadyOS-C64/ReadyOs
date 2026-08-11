/*
 * ucitest_uci.h - Generic Ultimate Command Interface transport for ucitest
 */

#ifndef UCITEST_UCI_H
#define UCITEST_UCI_H

#define UCITEST_UCI_ID_MASK   0x7F
#define UCITEST_UCI_ID_MATCH  0x49

#define UCITEST_UCI_TRUNC_DATA 0x01
#define UCITEST_UCI_TRUNC_STAT 0x02
#define UCITEST_UCI_TIMEOUT    0x04
#define UCITEST_UCI_ERROR      0x08

typedef struct {
    unsigned char *data;
    unsigned int data_cap;
    unsigned int data_len;
    unsigned char *stat;
    unsigned int stat_cap;
    unsigned int stat_len;
    unsigned char flags;
    unsigned char last_status;
} UciTestTransfer;

unsigned char ucitest_uci_detect(void);
unsigned int ucitest_uci_base(void);
unsigned char ucitest_uci_id(void);
unsigned char ucitest_uci_status(void);
void ucitest_uci_abort(void);
void ucitest_uci_clear_error(void);
unsigned char ucitest_uci_command(const unsigned char *cmd,
                                  unsigned int cmd_len,
                                  UciTestTransfer *xfer);
/* ucitest_uci_command owns sync, asynchronous PUSH/LAST-or-MORE, full queue
 * drains, DATA_ACC, and final quiet IDLE. Callers must not add pacing delays. */

void __fastcall__ ucitest_uci_asm_set_base(unsigned int base);
unsigned char __fastcall__ ucitest_uci_asm_write_cmd(unsigned char value);
unsigned char ucitest_uci_asm_id(void);
unsigned char ucitest_uci_asm_status(void);
unsigned char ucitest_uci_asm_read_data(void);
unsigned char ucitest_uci_asm_read_stat(void);
void ucitest_uci_asm_push_cmd(void);
void ucitest_uci_asm_accept_data(void);
void ucitest_uci_asm_abort(void);
void ucitest_uci_asm_clear_error(void);
unsigned char ucitest_uci_asm_softiec_bus(void);

#endif /* UCITEST_UCI_H */
