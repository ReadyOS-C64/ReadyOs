/*
 * resume_state_ctx.c - Shared resume state context for split modules
 */

#include "resume_state_priv.h"
#include "reu_mgr.h"

unsigned char rs_resume_bank = 0;
unsigned char rs_resume_app_id = 0;
unsigned char rs_resume_schema = 0;
unsigned int rs_resume_last_seq = 0;

unsigned char rs_resume_hdr[RESUME_HDR_SIZE];
unsigned char rs_resume_zero_hdr[RESUME_HDR_SIZE];

void resume_init_for_app(unsigned char bank, unsigned char app_id,
                         unsigned char schema_version) {
    unsigned char status;

    if (bank == 0u) {
        rs_resume_bank = REU_READYOS_GLOBAL_PHYSICAL();
    } else {
        status = readyos_bank_read_byte((unsigned int)(REUCB_TOKEN_STATUS_OFF + bank));
        if (!(status & REUCB_TOKEN_VALID)) {
            rs_resume_bank = 0xFFu;
            schema_version = 0u;
        } else {
            rs_resume_bank = readyos_bank_read_byte(
                (unsigned int)(REUCB_SHIM_LOOKUP_OFF + bank));
        }
    }
    rs_resume_app_id = app_id;
    rs_resume_schema = schema_version;
    rs_resume_last_seq = 0;
}
