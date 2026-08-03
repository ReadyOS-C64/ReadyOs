/*
 * resume_state_priv.h - Internal shared state for split resume_state modules
 */

#ifndef RESUME_STATE_PRIV_H
#define RESUME_STATE_PRIV_H

#include "resume_state.h"
#include "reu_control_bank.h"

#define RESUME_MAGIC_0 0x52u /* ASCII R */
#define RESUME_MAGIC_1 0x53u /* ASCII S */
#define RESUME_MAGIC_2 0x4Du /* ASCII M */
#define RESUME_MAGIC_3 0x31u /* ASCII 1 */

#define RESUME_HDR_SIZE 16

#define HDR_OFF_MAGIC0      0
#define HDR_OFF_MAGIC1      1
#define HDR_OFF_MAGIC2      2
#define HDR_OFF_MAGIC3      3
#define HDR_OFF_APP_ID      4
#define HDR_OFF_SCHEMA      5
#define HDR_OFF_FLAGS       6
#define HDR_OFF_RSVD0       7
#define HDR_OFF_SEQ_LO      8
#define HDR_OFF_SEQ_HI      9
#define HDR_OFF_LEN_LO      10
#define HDR_OFF_LEN_HI      11
#define HDR_OFF_CRC_LO      12
#define HDR_OFF_CRC_HI      13
#define HDR_OFF_RSVD1       14
#define HDR_OFF_RSVD2       15

extern unsigned char rs_resume_bank;
extern unsigned char rs_resume_app_id;
extern unsigned char rs_resume_schema;
extern unsigned int rs_resume_last_seq;

extern unsigned char rs_resume_hdr[RESUME_HDR_SIZE];
extern unsigned char rs_resume_zero_hdr[RESUME_HDR_SIZE];

/* Token 0 is the launcher snapshot in the ReadyOS bank, whose normal app tail
 * is occupied by schema-v5 metadata.  Its tiny UI resume record lives in the
 * schema runtime block; all application tokens retain the full per-bank tail. */
#define RS_RESUME_OFF() \
    ((rs_resume_app_id == 0u) ? REUCB_RUNTIME_OFF : REU_RESUME_OFF)
#define RS_RESUME_TAIL_SIZE() \
    ((rs_resume_app_id == 0u) ? REUCB_RUNTIME_SIZE : REU_RESUME_TAIL_SIZE)

#endif /* RESUME_STATE_PRIV_H */
