#ifndef SYSINFO_UCI_H
#define SYSINFO_UCI_H

#define SYSINFO_UCI_DATA_MAX 48
#define SYSINFO_UCI_STAT_MAX 32

unsigned char sysinfo_uci_detect(void);
unsigned int sysinfo_uci_base(void);
/* Owns the full asynchronous transaction and always drains overflow before
 * DATA_ACC. Callers consume captured prefixes; they must not pace this call. */
unsigned char sysinfo_uci_command(const unsigned char *cmd,
                                  unsigned char cmd_len,
                                  unsigned char *data,
                                  unsigned char data_cap,
                                  unsigned char *data_len,
                                  unsigned char *stat,
                                  unsigned char stat_cap,
                                  unsigned char *stat_len);

void __fastcall__ sysinfo_uci_asm_set_base(unsigned int base);
unsigned char __fastcall__ sysinfo_uci_asm_write_cmd(unsigned char value);
unsigned char sysinfo_uci_asm_id(void);
unsigned char sysinfo_uci_asm_status(void);
unsigned char sysinfo_uci_asm_read_data(void);
unsigned char sysinfo_uci_asm_read_stat(void);
void sysinfo_uci_asm_push_cmd(void);
void sysinfo_uci_asm_accept_data(void);
void sysinfo_uci_asm_abort(void);
void sysinfo_uci_asm_clear_error(void);
unsigned char sysinfo_uci_asm_read_u64_turbo(void);
unsigned char sysinfo_uci_asm_read_u64_turbo_enable(void);
unsigned char sysinfo_uci_asm_read_softiec_bus(void);

unsigned char __fastcall__ sysinfo_rom_asm_read_chargen(unsigned int off);
unsigned char __fastcall__ sysinfo_rom_asm_read_kernal(unsigned int off);

#endif /* SYSINFO_UCI_H */
