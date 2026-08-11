#ifndef READYIRC_UCI_H
#define READYIRC_UCI_H

#define READYIRC_UCI_DATA_MAX 82
#define READYIRC_UCI_STAT_MAX 16

extern const char readyirc_config_server[];
extern const char readyirc_config_channel[];
extern const char readyirc_config_nick[];
extern const unsigned int readyirc_config_port;

/* These high-level calls own the complete asynchronous UCI transaction:
 * quiet-idle sync, PUSH -> LAST/MORE wait, both queue drains, DATA_ACC, and
 * final quiet-idle wait. App code must not add timing delays around them. */
unsigned char readyirc_uci_detect(void);
unsigned int readyirc_uci_base(void);
unsigned char readyirc_uci_tcp_connect(const char *host,
                                       unsigned int port,
                                       unsigned char *socket_out);
unsigned int readyirc_uci_socket_read(unsigned char socket_id,
                                      unsigned char *dst,
                                      unsigned int dst_len);
unsigned char readyirc_uci_socket_write(unsigned char socket_id,
                                        const char *data,
                                        unsigned int len);
void readyirc_uci_socket_close(unsigned char socket_id);
const char *readyirc_uci_last_status(void);
unsigned char readyirc_uci_last_status_code(void);

void __fastcall__ readyirc_uci_asm_set_base(unsigned int base);
unsigned char __fastcall__ readyirc_uci_asm_write_cmd(unsigned char value);
unsigned char readyirc_uci_asm_id(void);
unsigned char readyirc_uci_asm_status(void);
unsigned char readyirc_uci_asm_read_data(void);
unsigned char readyirc_uci_asm_read_stat(void);
void readyirc_uci_asm_push_cmd(void);
void readyirc_uci_asm_accept_data(void);
void readyirc_uci_asm_abort(void);
void readyirc_uci_asm_clear_error(void);

#endif /* READYIRC_UCI_H */
