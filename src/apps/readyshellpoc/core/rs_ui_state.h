#ifndef RS_UI_STATE_H
#define RS_UI_STATE_H

/*
 * Shared ReadyShell UI flags live in REU bank 0x48 metadata space instead of
 * overlay or resident heap RAM. This keeps the pause state visible across the
 * resident/output boundary without colliding with command scratch payloads.
 */
#define RS_REU_UI_FLAGS_OFF 0x4880F0ul
#define RS_UI_FLAG_PAUSED 0x01u

#endif
