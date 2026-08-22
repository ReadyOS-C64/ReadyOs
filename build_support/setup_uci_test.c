#include "src/setup/setup_uci.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define ST_DATA  0x80u
#define ST_STAT  0x40u
#define ST_LAST  0x20u
#define ST_MORE  0x30u
#define ST_BUSY  0x10u
#define ST_ABORT 0x04u
#define ST_ERROR 0x08u

static unsigned int selected_base;
static unsigned char phase;
static unsigned char scenario;
static unsigned char block_index;
static unsigned char pushed_samples;
static unsigned char data_index;
static unsigned char stat_index;
static unsigned char command_bytes[32];
static unsigned char command_count;
static unsigned char push_count;
static unsigned char abort_count;
static unsigned char accept_count;
static unsigned char accept_more_count;
static unsigned char handled_data[8];
static unsigned char handled_length;
static unsigned char handled_blocks;

static const unsigned char reply_data[] = {'U','L','T','I','M','A','T','E'};
static const unsigned char reply_stat[] = {'0','0'};
static const unsigned char more_data_0[] = {'A','B','C'};
static const unsigned char more_data_1[] = {'D','E'};
static const unsigned char more_stat[] = {'0'};

static unsigned int active_data_length(void) {
    if (scenario == 1u)
        return block_index == 0u ? sizeof(more_data_0) : sizeof(more_data_1);
    return sizeof(reply_data);
}

static unsigned int active_stat_length(void) {
    return scenario == 1u ? sizeof(more_stat) : sizeof(reply_stat);
}

void setup_uci_asm_set_base(unsigned int base) { selected_base = base; }
unsigned char setup_uci_asm_id(void) {
    return selected_base == 0xDF1Cu ? 0x49u : 0u;
}
unsigned char setup_uci_asm_write_cmd(unsigned char value) {
    assert(command_count < sizeof(command_bytes));
    command_bytes[command_count++] = value;
    return value;
}
void setup_uci_asm_push_cmd(void) {
    ++push_count;
    pushed_samples = 0u;
    data_index = stat_index = block_index = 0u;
    phase = 1u;
}
unsigned char setup_uci_asm_status(void) {
    unsigned char st;
    if (phase == 0u || phase == 5u) return 0u;
    if (phase == 1u) {
        if (pushed_samples++ == 0u) return 0u; /* asynchronous PUSH */
        phase = scenario == 3u ? 7u : 4u;
    }
    if (phase == 2u) return ST_BUSY;
    if (phase == 3u) { phase = 5u; return ST_ABORT; }
    if (phase == 4u) {
        st = (scenario == 1u && block_index == 0u) ? ST_MORE : ST_LAST;
        if (data_index < active_data_length()) st |= ST_DATA;
        if (stat_index < active_stat_length()) st |= ST_STAT;
        return st;
    }
    if (phase == 6u) {
        phase = 4u;
        block_index = 1u;
        data_index = stat_index = 0u;
        return ST_BUSY;
    }
    if (phase == 7u) return ST_ERROR;
    return 0u;
}
unsigned char setup_uci_asm_read_data(void) {
    if (scenario == 1u)
        return block_index == 0u ? more_data_0[data_index++] :
                                  more_data_1[data_index++];
    return reply_data[data_index++];
}
unsigned char setup_uci_asm_read_stat(void) {
    if (scenario == 1u) return more_stat[stat_index++];
    return reply_stat[stat_index++];
}
void setup_uci_asm_accept_data(void) { ++accept_count; phase = 5u; }
unsigned char setup_uci_asm_accept_more_transition(unsigned char old_state) {
    assert(old_state == ST_MORE && scenario == 1u && block_index == 0u);
    ++accept_more_count;
    phase = 6u;
    return 1u;
}
void setup_uci_asm_abort(void) { ++abort_count; phase = 3u; }
void setup_uci_asm_clear_error(void) { phase = 5u; }

static void collect_blocks(const unsigned char *data, unsigned int data_len,
                           const unsigned char *stat, unsigned int stat_len) {
    assert(stat_len == 1u && stat[0] == '0');
    assert(handled_length + data_len <= sizeof(handled_data));
    memcpy(handled_data + handled_length, data, data_len);
    handled_length = (unsigned char)(handled_length + data_len);
    ++handled_blocks;
}

static void init_xfer(SetupUciTransfer *xfer, unsigned char *data,
                      unsigned int data_cap, unsigned char *stat,
                      unsigned int stat_cap) {
    memset(xfer, 0, sizeof(*xfer));
    xfer->data = data;
    xfer->data_cap = data_cap;
    xfer->stat = stat;
    xfer->stat_cap = stat_cap;
}

int main(void) {
    SetupUciTransfer xfer;
    unsigned char data[12];
    unsigned char stat[4];
    const unsigned char identify[] = {1u, 1u};

    init_xfer(&xfer, data, sizeof(data), stat, sizeof(stat));
    assert(setup_uci_detect() && setup_uci_base() == 0xDF1Cu);
    assert(setup_uci_command(identify, sizeof(identify), &xfer));
    assert(push_count == 1u && accept_count == 1u && abort_count == 0u);
    assert(command_count == 2u && command_bytes[0] == 1u && command_bytes[1] == 1u);
    assert(xfer.flags == 0u && xfer.data_len == sizeof(reply_data));
    assert(xfer.stat_len == sizeof(reply_stat));

    scenario = 1u;
    handled_length = handled_blocks = 0u;
    init_xfer(&xfer, data, sizeof(data), stat, sizeof(stat));
    xfer.on_block = collect_blocks;
    assert(setup_uci_command(identify, sizeof(identify), &xfer));
    assert(accept_more_count == 1u);
    assert(xfer.block_count == 2u && handled_blocks == 2u);
    assert(xfer.data_seen == 5u && xfer.stat_seen == 2u);
    assert(handled_length == 5u && memcmp(handled_data, "ABCDE", 5u) == 0);

    scenario = 2u;
    init_xfer(&xfer, data, 2u, stat, 1u);
    assert(setup_uci_command(identify, sizeof(identify), &xfer));
    assert(xfer.data_seen == sizeof(reply_data));
    assert(xfer.stat_seen == sizeof(reply_stat));
    assert((xfer.flags & (SETUP_UCI_TRUNC_DATA | SETUP_UCI_TRUNC_STAT)) ==
           (SETUP_UCI_TRUNC_DATA | SETUP_UCI_TRUNC_STAT));

    scenario = 3u;
    init_xfer(&xfer, data, sizeof(data), stat, sizeof(stat));
    assert(!setup_uci_command(identify, sizeof(identify), &xfer));
    assert((xfer.flags & SETUP_UCI_ERROR) != 0u && abort_count == 1u);
    assert(!setup_uci_command(identify, 897u, &xfer));
    assert(push_count == 4u);

    puts("SETUP UCI async/multiblock/truncation/error tests passed");
    return 0;
}
