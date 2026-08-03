/*
 * resume_state.h - Warm resume payload storage in per-app REU bank tail
 *
 * Stores app-defined state payload at REU offset $B600..$FFFF.
 * This area is outside the shim's app snapshot transfer window.
 */

#ifndef RESUME_STATE_H
#define RESUME_STATE_H

#define REU_APP_SNAPSHOT_SIZE 0xB600
#define REU_RESUME_OFF        0xB600
#define REU_RESUME_TAIL_SIZE  0x4A00

#define RESUME_SCHEMA_V1      1

typedef struct {
    const void *ptr;
    unsigned int len;
} ResumeWriteSegment;

typedef struct {
    void *ptr;
    unsigned int len;
} ResumeReadSegment;

/* Initialize module context for current app/bank. */
void resume_init_for_app(unsigned char bank, unsigned char app_id,
                         unsigned char schema_version);

/*
 * Attempt to load persisted payload.
 * Returns 1 on success, 0 on missing/invalid payload.
 */
unsigned char resume_try_load(void *dst, unsigned int dst_len,
                              unsigned int *out_len);

/*
 * Save payload for current app.
 * Returns 1 on success, 0 on invalid size/context.
 */
unsigned char resume_save(const void *src, unsigned int src_len);

/* Segment-based save/load helpers to avoid large temporary buffers. */
unsigned char resume_save_segments(const ResumeWriteSegment *segments,
                                   unsigned char segment_count);
unsigned char resume_load_segments(const ResumeReadSegment *segments,
                                   unsigned char segment_count,
                                   unsigned int *out_len);

/* Invalidate persisted payload for current app. */
void resume_invalidate(void);

#endif /* RESUME_STATE_H */
