/* reu_control_registry.c - launcher publication into schema-v5 app records */

#include "reu_control_bank.h"

static unsigned char registry_record[REUCB_APP_REG_SIZE];
static unsigned char registry_phys[REUCB_APP_REG_COUNT];
static unsigned char registry_status[REUCB_APP_REG_COUNT];
static unsigned char registry_fill[64];

static void registry_fill_buf(unsigned char value) {
    unsigned char i;
    for (i = 0u; i < 64u; ++i) {
        registry_fill[i] = value;
    }
}

void reu_control_bank_write_launcher_registry(
    unsigned char first_app_index,
    unsigned char app_count,
    const unsigned char *app_banks,
    const unsigned char *app_drives,
    const unsigned char *app_default_slots,
    const unsigned char *app_resource_sets,
    const unsigned char *app_resource_loaded,
    const unsigned char *app_rs_bank1,
    const unsigned char *app_rs_bank2,
    const unsigned char *app_rs_bank3,
    const unsigned char *app_rs_bank4,
    const unsigned char *apps_loaded,
    const unsigned int *app_sizes) {
    unsigned char i;
    unsigned char j;
    unsigned char app_id;
    unsigned char token;
    unsigned char status;

    for (i = 0u; i < REUCB_APP_REG_COUNT; ++i) {
        registry_phys[i] = 0u;
        registry_status[i] = 0u;
    }
    for (i = first_app_index; i < app_count; ++i) {
        app_id = (unsigned char)(i - first_app_index);
        token = app_banks[i];
        if (token != 0u && app_id < REUCB_APP_REG_COUNT) {
            registry_phys[app_id] = readyos_bank_read_byte(
                (unsigned int)(REUCB_SHIM_LOOKUP_OFF + token));
            registry_status[app_id] = readyos_bank_read_byte(
                (unsigned int)(REUCB_TOKEN_STATUS_OFF + token));
        }
    }

    registry_fill_buf(0u);
    for (j = 0u; j < 4u; ++j) {
        readyos_bank_write((unsigned int)(REUCB_TOKEN_STATUS_OFF +
                                          ((unsigned int)j * 64u)),
                           registry_fill, 64u);
    }
    registry_fill_buf(REUCB_NULL_REC);
    for (j = 0u; j < 4u; ++j) {
        readyos_bank_write((unsigned int)(REUCB_TOKEN_APP_OFF +
                                          ((unsigned int)j * 64u)),
                           registry_fill, 64u);
    }

    for (i = first_app_index; i < app_count; ++i) {
        app_id = (unsigned char)(i - first_app_index);
        if (app_id >= REUCB_APP_REG_COUNT) {
            break;
        }
        token = app_banks[i];
        for (j = 0u; j < REUCB_APP_REG_SIZE; ++j) {
            registry_record[j] = 0u;
        }
        registry_record[REUCB_APP_REC_TOKEN] = token;
        registry_record[REUCB_APP_REC_PHYSICAL] = registry_phys[app_id];
        registry_record[REUCB_APP_REC_FLAGS] = apps_loaded[i] ? REUCB_APP_FLAG_LOADED : 0u;
        registry_record[REUCB_APP_REC_DRIVE] = app_drives[i];
        registry_record[REUCB_APP_REC_HOTKEY] = app_default_slots[i];
        registry_record[REUCB_APP_REC_RSRC_SET] = app_resource_sets[i];
        registry_record[REUCB_APP_REC_RSRC_READY] = app_resource_loaded[i];
        registry_record[REUCB_APP_REC_FIRST_RSRC] = REUCB_NULL_REC;
        registry_record[REUCB_APP_REC_RSRC1] = app_rs_bank1[i];
        registry_record[REUCB_APP_REC_RSRC2] = app_rs_bank2[i];
        registry_record[REUCB_APP_REC_RSRC3] = app_rs_bank3[i];
        registry_record[REUCB_APP_REC_RSRC4] = app_rs_bank4[i];
        registry_record[REUCB_APP_REC_SIZE_LO] = (unsigned char)app_sizes[i];
        registry_record[REUCB_APP_REC_SIZE_HI] = (unsigned char)(app_sizes[i] >> 8);
        readyos_bank_write((unsigned int)(REUCB_APP_REG_OFF +
                                          ((unsigned int)app_id * REUCB_APP_REG_SIZE)),
                           registry_record, REUCB_APP_REG_SIZE);

        if (token != 0u && (registry_status[app_id] & REUCB_TOKEN_VALID) != 0u) {
            status = REUCB_TOKEN_VALID;
            if (apps_loaded[i]) {
                status = (unsigned char)(status | REUCB_TOKEN_LOADED |
                                         REUCB_TOKEN_RESUMABLE);
            }
            readyos_bank_write_byte((unsigned int)(REUCB_SHIM_LOOKUP_OFF + token),
                                    registry_phys[app_id]);
            readyos_bank_write_byte((unsigned int)(REUCB_TOKEN_STATUS_OFF + token), status);
            readyos_bank_write_byte((unsigned int)(REUCB_TOKEN_APP_OFF + token), app_id);
        }
    }
}
