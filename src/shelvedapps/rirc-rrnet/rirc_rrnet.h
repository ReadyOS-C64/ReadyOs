#ifndef RIRC_RRNET_H
#define RIRC_RRNET_H

extern const char rirc_rrnet_config_server[];
extern const char rirc_rrnet_config_channel[];
extern const char rirc_rrnet_config_nick[];
extern const unsigned int rirc_rrnet_config_port;

unsigned char rirc_rrnet_detect(void);
unsigned char rirc_rrnet_tcp_connect(const char *host,
                                     unsigned int port,
                                     unsigned char *socket_out);
unsigned int rirc_rrnet_socket_read(unsigned char socket_id,
                                    unsigned char *dst,
                                    unsigned int dst_len);
unsigned char rirc_rrnet_socket_write(unsigned char socket_id,
                                      const char *data,
                                      unsigned int len);
void rirc_rrnet_socket_close(unsigned char socket_id);
const char *rirc_rrnet_last_status(void);

#endif /* RIRC_RRNET_H */
