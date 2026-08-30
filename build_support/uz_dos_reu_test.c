#include <assert.h>
#include <string.h>

#include "uz_dos.h"

static unsigned char expected_command;
static unsigned char expected_bank;
static unsigned int expected_offset;
static unsigned int expected_length;
static const char *reply_text;
static unsigned char identify_calls;

unsigned char uz_uci_detect(void) {
    return 1u;
}

unsigned int uz_uci_base(void) {
    return 0xDF1Cu;
}

unsigned char uz_uci_command(const unsigned char *command,
                             unsigned int command_len,
                             UzUciTransfer *transfer) {
    unsigned int reply_len;
    if (command_len == 2u && command[0] == UZ_DOS_TARGET_READ &&
        command[1] == 0x01u) {
        ++identify_calls;
        transfer->data_len = 0u;
        transfer->stat[0] = '0';
        transfer->stat[1] = '0';
        transfer->stat_len = 2u;
        transfer->flags = 0u;
        return 1u;
    }
    assert(command_len == 10u);
    assert(command[0] == UZ_DOS_TARGET_READ);
    assert(command[1] == expected_command);
    assert(command[2] == (unsigned char)expected_offset);
    assert(command[3] == (unsigned char)(expected_offset >> 8u));
    assert(command[4] == expected_bank && command[5] == 0u);
    assert(command[6] == (unsigned char)expected_length);
    assert(command[7] == (unsigned char)(expected_length >> 8u));
    assert(command[8] == 0u && command[9] == 0u);
    reply_len = strlen(reply_text);
    assert(reply_len <= transfer->data_cap && transfer->stat_cap >= 2u);
    memcpy(transfer->data, reply_text, reply_len);
    transfer->data_len = reply_len;
    transfer->stat[0] = '0';
    transfer->stat[1] = '0';
    transfer->stat_len = 2u;
    transfer->flags = 0u;
    return 1u;
}

int main(void) {
    UzDos dos;
    unsigned char command[32];
    unsigned char data[64];
    unsigned char status[8];
    unsigned int transferred;

    uz_dos_init(&dos, UZ_DOS_TARGET_READ,
                command, sizeof(command), data, sizeof(data),
                status, sizeof(status));
    /* Firmware 3.14 can omit the descriptive payload after ReadyOS launcher
     * already used this target; status 00 remains a valid identity reply. */
    assert(uz_dos_identify(&dos));
    assert(identify_calls == 1u);
    dos.file_open = 1u;

    expected_command = 0x21u;
    expected_bank = 0x34u;
    expected_offset = 0x1200u;
    expected_length = 0x0100u;
    reply_text = "$   100 BYTES LOADED TO REU $341200";
    assert(uz_dos_load_reu(&dos, expected_bank, expected_offset,
                           expected_length, &transferred));
    assert(transferred == expected_length);

    expected_command = 0x22u;
    expected_offset = 0xFF00u;
    expected_length = 0x0100u;
    reply_text = "$   100 BYTES SAVED FROM REU $34ff00";
    assert(uz_dos_save_reu(&dos, expected_bank, expected_offset,
                           expected_length, &transferred));
    assert(transferred == expected_length);

    assert(!uz_dos_save_reu(&dos, expected_bank, 0xFF00u, 0x0101u,
                            &transferred));
    assert(!uz_dos_load_reu(&dos, expected_bank, 0u, 0u, &transferred));
    return 0;
}
