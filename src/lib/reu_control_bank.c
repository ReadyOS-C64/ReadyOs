/* reu_control_bank.c - ReadyOS-bank schema-v5 lifecycle */

#include "reu_control_bank.h"
#include "reu_phys.h"

static unsigned char reucb_header[REUCB_HEADER_SIZE];
static unsigned char reucb_zero[REUCB_HEADER_SIZE];
static unsigned char reucb_generation;

static void reucb_fill(unsigned char *buf, unsigned char value, unsigned char len) {
    unsigned char i;
    for (i = 0u; i < len; ++i) {
        buf[i] = value;
    }
}

unsigned char reu_control_bank_is_valid(void) {
    readyos_bank_read(REUCB_HEADER_OFF, reucb_header, 5u);
    return (unsigned char)(reucb_header[0] == REUCB_MAGIC0 &&
                           reucb_header[1] == REUCB_MAGIC1 &&
                           reucb_header[2] == REUCB_MAGIC2 &&
                           reucb_header[3] == REUCB_MAGIC3 &&
                           reucb_header[4] == REUCB_SCHEMA_VERSION);
}

static void reucb_write_header(unsigned char writer_id,
                               unsigned char physical_banks) {
    unsigned char readyos_bank;

    readyos_bank = REU_READYOS_GLOBAL_PHYSICAL();
    reucb_fill(reucb_header, 0u, REUCB_HEADER_SIZE);
    reucb_header[0] = REUCB_MAGIC0;
    reucb_header[1] = REUCB_MAGIC1;
    reucb_header[2] = REUCB_MAGIC2;
    reucb_header[3] = REUCB_MAGIC3;
    reucb_header[4] = REUCB_SCHEMA_VERSION;
    reucb_header[5] = REUCB_HEADER_SIZE;
    reucb_header[6] = ++reucb_generation;
    reucb_header[7] = writer_id;
    reucb_header[REUCB_HEADER_REU_SKIP] = REU_FIRST_DYNAMIC_PHYSICAL();
    reucb_header[REUCB_HEADER_CONTROL_BANK] = readyos_bank;
    reucb_header[REUCB_HEADER_LAUNCHER_BANK] = readyos_bank;
    reucb_header[REUCB_HEADER_LAUNCHER_OVL] = 0u;
    reucb_header[REUCB_HEADER_FIRST_DYNAMIC] = REU_FIRST_DYNAMIC_PHYSICAL();
    reucb_header[REUCB_HEADER_LOGICAL_BANKS] = 0u; /* 0 encodes 256 tokens. */
    reucb_header[14] = (unsigned char)REUCB_BANK_TYPE_OFF;
    reucb_header[15] = (unsigned char)(REUCB_BANK_TYPE_OFF >> 8);
    reucb_header[16] = 0u; /* 0 encodes 256 bytes. */
    reucb_header[17] = 1u;
    reucb_header[18] = (unsigned char)REUCB_SHIM_LOOKUP_OFF;
    reucb_header[19] = (unsigned char)(REUCB_SHIM_LOOKUP_OFF >> 8);
    reucb_header[20] = (unsigned char)REUCB_TOKEN_STATUS_OFF;
    reucb_header[21] = (unsigned char)(REUCB_TOKEN_STATUS_OFF >> 8);
    reucb_header[22] = (unsigned char)REUCB_APP_REG_OFF;
    reucb_header[23] = (unsigned char)(REUCB_APP_REG_OFF >> 8);
    reucb_header[24] = REUCB_APP_REG_COUNT;
    reucb_header[25] = REUCB_APP_REG_SIZE;
    reucb_header[26] = (unsigned char)REUCB_APP_META_OFF;
    reucb_header[27] = (unsigned char)(REUCB_APP_META_OFF >> 8);
    reucb_header[28] = REUCB_APP_META_COUNT;
    reucb_header[29] = REUCB_APP_META_SIZE;
    reucb_header[30] = (unsigned char)REUCB_RSRC_REC_OFF;
    reucb_header[31] = (unsigned char)(REUCB_RSRC_REC_OFF >> 8);
    reucb_header[32] = REUCB_RSRC_REC_COUNT;
    reucb_header[33] = REUCB_RSRC_REC_SIZE;
    reucb_header[34] = (unsigned char)REUCB_DEP_LINE_OFF;
    reucb_header[35] = (unsigned char)(REUCB_DEP_LINE_OFF >> 8);
    reucb_header[36] = REUCB_DEP_LINE_COUNT;
    reucb_header[37] = REUCB_DEP_LINE_SIZE;
    reucb_header[38] = (unsigned char)REUCB_CLIPBOARD_OFF;
    reucb_header[39] = (unsigned char)(REUCB_CLIPBOARD_OFF >> 8);
    reucb_header[40] = (unsigned char)REUCB_HOTKEY_OFF;
    reucb_header[41] = (unsigned char)(REUCB_HOTKEY_OFF >> 8);
    reucb_header[42] = (unsigned char)REUCB_RUNTIME_OFF;
    reucb_header[43] = (unsigned char)(REUCB_RUNTIME_OFF >> 8);
    reucb_header[REUCB_HEADER_PHYS_BANKS] = physical_banks;
    reucb_header[REUCB_HEADER_FIRST_UNAVAIL] = physical_banks;
    reucb_header[REUCB_HEADER_FLAGS] = REUCB_HEADER_FLAG_PHYS_SIZE;
    readyos_bank_write(REUCB_HEADER_OFF, reucb_header, REUCB_HEADER_SIZE);
}

void reu_control_bank_prepare(unsigned char physical_banks) {
    unsigned char valid;
    unsigned char bank;
    unsigned char type;
    unsigned int off;

    valid = reu_control_bank_is_valid();
    if (!valid) {
        reucb_fill(reucb_zero, 0u, REUCB_HEADER_SIZE);
        off = REUCB_HEADER_OFF;
        do {
            readyos_bank_write(off, reucb_zero, REUCB_HEADER_SIZE);
            off = (unsigned int)(off + REUCB_HEADER_SIZE);
        } while (off != 0u);
    }

    bank = 0u;
    do {
        if (bank < REU_FIRST_DYNAMIC_PHYSICAL()) {
            type = REU_SKIPPED;
        } else if (bank == REU_READYOS_GLOBAL_PHYSICAL()) {
            type = REU_GLOBAL;
        } else if (reu_phys_is_unavailable(physical_banks, bank)) {
            type = REU_UNAVAIL;
        } else if (!valid) {
            type = REU_FREE;
        } else {
            type = readyos_bank_read_byte((unsigned int)(REUCB_BANK_TYPE_OFF + bank));
            if (type == REU_SKIPPED || type == REU_GLOBAL ||
                type == REU_LAUNCHER || type == REU_UNAVAIL) {
                type = REU_FREE;
            }
        }
        readyos_bank_write_byte((unsigned int)(REUCB_BANK_TYPE_OFF + bank), type);
        ++bank;
    } while (bank != 0u);

    reucb_write_header(REUCB_WRITER_LAUNCHER, physical_banks);
}

void reu_control_bank_sync_and_mirror(unsigned char writer_id) {
    unsigned char physical_banks;

    physical_banks = readyos_bank_read_byte((unsigned int)(REUCB_HEADER_OFF +
                                                           REUCB_HEADER_PHYS_BANKS));
    readyos_bank_write_byte((unsigned int)(REUCB_BANK_TYPE_OFF +
                                           REU_READYOS_GLOBAL_PHYSICAL()),
                            REU_GLOBAL);
    reucb_write_header(writer_id, physical_banks);
}
