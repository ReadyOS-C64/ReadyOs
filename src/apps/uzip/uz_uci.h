#ifndef UZ_UCI_H
#define UZ_UCI_H

#if !defined(__CC65__) && !defined(__fastcall__)
#define __fastcall__
#endif

#define UZ_UCI_ID_MASK    0x7Fu
#define UZ_UCI_ID_MATCH   0x49u

#define UZ_UCI_TRUNC_DATA 0x01u
#define UZ_UCI_TRUNC_STAT 0x02u
#define UZ_UCI_TIMEOUT    0x04u
#define UZ_UCI_ERROR      0x08u

typedef void (*UzUciBlockHandler)(const unsigned char *data,
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
    UzUciBlockHandler on_block;
    unsigned char flags;
    unsigned char last_status;
} UzUciTransfer;

unsigned char uz_uci_detect(void);
unsigned int uz_uci_base(void);
unsigned char uz_uci_command(const unsigned char *command,
                             unsigned int command_len,
                             UzUciTransfer *transfer);

/* uZIP's sole UCI transaction gateway. It owns quiet-IDLE synchronization,
 * asynchronous PUSH/ABORT handling, complete data/status queue draining,
 * DATA_ACC transitions, and the final quiet-IDLE wait. Callers never pace it
 * or treat Ultimate DOS target status as UCI transport state. */

void __fastcall__ uz_uci_asm_set_base(unsigned int base);
unsigned char __fastcall__ uz_uci_asm_write_cmd(unsigned char value);
unsigned char uz_uci_asm_id(void);
unsigned char uz_uci_asm_status(void);
unsigned char uz_uci_asm_read_data(void);
unsigned char uz_uci_asm_read_stat(void);
void uz_uci_asm_push_cmd(void);
void uz_uci_asm_accept_data(void);
unsigned char __fastcall__ uz_uci_asm_accept_more_transition(
    unsigned char old_state);
void uz_uci_asm_abort(void);
void uz_uci_asm_clear_error(void);

#endif
