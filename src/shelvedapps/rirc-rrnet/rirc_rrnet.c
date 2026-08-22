#include "rirc_rrnet.h"

#include "ip65.h"

#include <stdint.h>
#include <string.h>

#define RX_RING_SIZE 512u
#define WRITE_CHUNK 96u

static unsigned char initialized;
static unsigned char connected;
static unsigned char closed_by_peer;
static unsigned char rx_overflow;
static char last_status[32];

static unsigned char rx_ring[RX_RING_SIZE];
static unsigned int rx_head;
static unsigned int rx_tail;

static void set_status(const char *msg) {
    unsigned char i;

    i = 0u;
    while (msg[i] != 0 && i + 1u < sizeof(last_status)) {
        last_status[i] = msg[i];
        ++i;
    }
    last_status[i] = 0;
}

static unsigned int ring_next(unsigned int pos) {
    ++pos;
    if (pos >= RX_RING_SIZE) {
        pos = 0u;
    }
    return pos;
}

static void ring_push(unsigned char ch) {
    unsigned int next;

    next = ring_next(rx_head);
    if (next == rx_tail) {
        rx_overflow = 1u;
        return;
    }
    rx_ring[rx_head] = ch;
    rx_head = next;
}

static unsigned char ring_pop(unsigned char *ch) {
    if (rx_tail == rx_head) {
        return 0u;
    }
    *ch = rx_ring[rx_tail];
    rx_tail = ring_next(rx_tail);
    return 1u;
}

static void __fastcall__ tcp_rx_callback(const uint8_t *buf, int16_t len) {
    int16_t i;

    if (len < 0) {
        connected = 0u;
        closed_by_peer = 1u;
        set_status("closed by peer");
        return;
    }
    for (i = 0; i < len; ++i) {
        ring_push(buf[i]);
    }
}

unsigned char rirc_rrnet_detect(void) {
    if (initialized) {
        return 1u;
    }
    set_status("initializing rr-net");
    if (ip65_init(ETH_INIT_DEFAULT)) {
        set_status(ip65_strerror(ip65_error));
        return 0u;
    }
    initialized = 1u;
    return 1u;
}

unsigned char rirc_rrnet_tcp_connect(const char *host,
                                     unsigned int port,
                                     unsigned char *socket_out) {
    uint32_t ip;

    if (socket_out != 0) {
        *socket_out = 1u;
    }
    rx_head = 0u;
    rx_tail = 0u;
    rx_overflow = 0u;
    closed_by_peer = 0u;
    connected = 0u;

    if (!rirc_rrnet_detect()) {
        return 0u;
    }

    set_status("dhcp");
    if (dhcp_init()) {
        set_status(ip65_strerror(ip65_error));
        return 0u;
    }

    set_status("dns");
    ip = dns_resolve(host);
    if (ip == 0u) {
        set_status(ip65_strerror(ip65_error));
        return 0u;
    }

    set_status("tcp connect");
    if (tcp_connect(ip, port, tcp_rx_callback)) {
        set_status(ip65_strerror(ip65_error));
        return 0u;
    }

    connected = 1u;
    set_status("connected");
    return 1u;
}

unsigned int rirc_rrnet_socket_read(unsigned char socket_id,
                                    unsigned char *dst,
                                    unsigned int dst_len) {
    unsigned int copied;
    unsigned char ch;
    unsigned char polls;

    (void)socket_id;
    if (dst == 0 || dst_len == 0u) {
        return 0u;
    }

    for (polls = 0u; polls < 2u; ++polls) {
        (void)ip65_process();
    }

    copied = 0u;
    while (copied < dst_len && ring_pop(&ch)) {
        dst[copied] = ch;
        ++copied;
    }
    if (rx_overflow) {
        rx_overflow = 0u;
        set_status("rx overflow");
    }
    return copied;
}

unsigned char rirc_rrnet_socket_write(unsigned char socket_id,
                                      const char *data,
                                      unsigned int len) {
    unsigned int pos;
    unsigned int chunk;

    (void)socket_id;
    if (!connected || data == 0) {
        return 0u;
    }

    pos = 0u;
    while (pos < len) {
        chunk = (unsigned int)(len - pos);
        if (chunk > WRITE_CHUNK) {
            chunk = WRITE_CHUNK;
        }
        if (tcp_send((const uint8_t *)(data + pos), chunk)) {
            set_status(ip65_strerror(ip65_error));
            return 0u;
        }
        (void)ip65_process();
        pos = (unsigned int)(pos + chunk);
    }
    return 1u;
}

void rirc_rrnet_socket_close(unsigned char socket_id) {
    (void)socket_id;
    if (connected) {
        (void)tcp_close();
    }
    connected = 0u;
}

const char *rirc_rrnet_last_status(void) {
    if (closed_by_peer) {
        return "closed by peer";
    }
    return last_status;
}
