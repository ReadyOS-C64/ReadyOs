/*
 * resume_state.c - Warm resume payload storage for Ready OS apps
 */

#include "resume_state.h"
#include "reu_mgr.h"

#define RESUME_MAGIC_0 'R'
#define RESUME_MAGIC_1 'S'
#define RESUME_MAGIC_2 'M'
#define RESUME_MAGIC_3 '1'

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

#if (REU_RESUME_OFF != REU_APP_SNAPSHOT_SIZE)
#error "REU resume offset must match snapshot end"
#endif

#if ((REU_RESUME_OFF + REU_RESUME_TAIL_SIZE) != 0x10000)
#error "REU resume tail must end at 64KB bank boundary"
#endif

static unsigned char s_bank = 0;
static unsigned char s_app_id = 0;
static unsigned char s_schema = 0;
static unsigned int s_last_seq = 0;

static unsigned char s_hdr[RESUME_HDR_SIZE];
static unsigned char s_zero_hdr[RESUME_HDR_SIZE];

static unsigned int get_u16(const unsigned char *buf, unsigned char off) {
    return (unsigned int)buf[off] | ((unsigned int)buf[(unsigned char)(off + 1)] << 8);
}

static void put_u16(unsigned char *buf, unsigned char off, unsigned int value) {
    buf[off] = (unsigned char)(value & 0xFF);
    buf[(unsigned char)(off + 1)] = (unsigned char)(value >> 8);
}

static unsigned char header_is_valid(unsigned int max_len, unsigned int *out_len,
                                     unsigned int *out_seq) {
    unsigned int len;
    unsigned int seq;

    if (s_hdr[HDR_OFF_MAGIC0] != RESUME_MAGIC_0 ||
        s_hdr[HDR_OFF_MAGIC1] != RESUME_MAGIC_1 ||
        s_hdr[HDR_OFF_MAGIC2] != RESUME_MAGIC_2 ||
        s_hdr[HDR_OFF_MAGIC3] != RESUME_MAGIC_3) {
        return 0;
    }
    if (s_hdr[HDR_OFF_APP_ID] != s_app_id || s_hdr[HDR_OFF_SCHEMA] != s_schema) {
        return 0;
    }

    len = get_u16(s_hdr, HDR_OFF_LEN_LO);
    seq = get_u16(s_hdr, HDR_OFF_SEQ_LO);

    if (len == 0) {
        return 0;
    }
    if (len > max_len) {
        return 0;
    }
    if (len > (REU_RESUME_TAIL_SIZE - RESUME_HDR_SIZE)) {
        return 0;
    }

    *out_len = len;
    *out_seq = seq;
    return 1;
}

static void build_header(unsigned int payload_len) {
    s_last_seq = (unsigned int)(s_last_seq + 1);

    s_hdr[HDR_OFF_MAGIC0] = RESUME_MAGIC_0;
    s_hdr[HDR_OFF_MAGIC1] = RESUME_MAGIC_1;
    s_hdr[HDR_OFF_MAGIC2] = RESUME_MAGIC_2;
    s_hdr[HDR_OFF_MAGIC3] = RESUME_MAGIC_3;
    s_hdr[HDR_OFF_APP_ID] = s_app_id;
    s_hdr[HDR_OFF_SCHEMA] = s_schema;
    s_hdr[HDR_OFF_FLAGS] = 0;
    s_hdr[HDR_OFF_RSVD0] = 0;
    put_u16(s_hdr, HDR_OFF_SEQ_LO, s_last_seq);
    put_u16(s_hdr, HDR_OFF_LEN_LO, payload_len);
    put_u16(s_hdr, HDR_OFF_CRC_LO, 0);
    s_hdr[HDR_OFF_RSVD1] = 0;
    s_hdr[HDR_OFF_RSVD2] = 0;
}

static unsigned char write_segments_len(const ResumeWriteSegment *segments,
                                        unsigned char segment_count,
                                        unsigned int *out_total) {
    unsigned char i;
    unsigned int total = 0;
    unsigned int prev;

    for (i = 0; i < segment_count; ++i) {
        if (segments[i].len == 0) {
            continue;
        }
        if (segments[i].ptr == 0) {
            return 0;
        }
        prev = total;
        total = (unsigned int)(total + segments[i].len);
        if (total < prev) {
            return 0;
        }
    }
    *out_total = total;
    return 1;
}

static unsigned char read_segments_len(const ResumeReadSegment *segments,
                                       unsigned char segment_count,
                                       unsigned int *out_total) {
    unsigned char i;
    unsigned int total = 0;
    unsigned int prev;

    for (i = 0; i < segment_count; ++i) {
        if (segments[i].len == 0) {
            continue;
        }
        if (segments[i].ptr == 0) {
            return 0;
        }
        prev = total;
        total = (unsigned int)(total + segments[i].len);
        if (total < prev) {
            return 0;
        }
    }
    *out_total = total;
    return 1;
}

void resume_init_for_app(unsigned char bank, unsigned char app_id,
                         unsigned char schema_version) {
    s_bank = bank;
    s_app_id = app_id;
    s_schema = schema_version;
    s_last_seq = 0;
}

unsigned char resume_save_segments(const ResumeWriteSegment *segments,
                                   unsigned char segment_count) {
    unsigned int payload_len;
    unsigned int payload_off;
    unsigned char i;

    if (s_schema == 0) {
        return 0;
    }
    if (segments == 0 || segment_count == 0) {
        return 0;
    }
    if (!write_segments_len(segments, segment_count, &payload_len)) {
        return 0;
    }
    if (payload_len == 0 || payload_len > (REU_RESUME_TAIL_SIZE - RESUME_HDR_SIZE)) {
        return 0;
    }

    build_header(payload_len);

    payload_off = (unsigned int)(REU_RESUME_OFF + RESUME_HDR_SIZE);
    for (i = 0; i < segment_count; ++i) {
        if (segments[i].len == 0) {
            continue;
        }
        reu_dma_stash((unsigned int)segments[i].ptr, s_bank, payload_off, segments[i].len);
        payload_off = (unsigned int)(payload_off + segments[i].len);
    }
    reu_dma_stash((unsigned int)s_hdr, s_bank, REU_RESUME_OFF, RESUME_HDR_SIZE);
    return 1;
}

unsigned char resume_load_segments(const ResumeReadSegment *segments,
                                   unsigned char segment_count,
                                   unsigned int *out_len) {
    unsigned int stored_len;
    unsigned int stored_seq;
    unsigned int expected_len;
    unsigned int payload_off;
    unsigned char i;

    if (out_len != 0) {
        *out_len = 0;
    }
    if (s_schema == 0) {
        return 0;
    }
    if (segments == 0 || segment_count == 0) {
        return 0;
    }
    if (!read_segments_len(segments, segment_count, &expected_len)) {
        return 0;
    }
    if (expected_len == 0 || expected_len > (REU_RESUME_TAIL_SIZE - RESUME_HDR_SIZE)) {
        return 0;
    }

    reu_dma_fetch((unsigned int)s_hdr, s_bank, REU_RESUME_OFF, RESUME_HDR_SIZE);
    if (!header_is_valid(expected_len, &stored_len, &stored_seq)) {
        return 0;
    }
    if (stored_len != expected_len) {
        return 0;
    }

    payload_off = (unsigned int)(REU_RESUME_OFF + RESUME_HDR_SIZE);
    for (i = 0; i < segment_count; ++i) {
        if (segments[i].len == 0) {
            continue;
        }
        reu_dma_fetch((unsigned int)segments[i].ptr, s_bank, payload_off, segments[i].len);
        payload_off = (unsigned int)(payload_off + segments[i].len);
    }

    s_last_seq = stored_seq;
    if (out_len != 0) {
        *out_len = stored_len;
    }
    return 1;
}

unsigned char resume_try_load(void *dst, unsigned int dst_len,
                              unsigned int *out_len) {
    ResumeReadSegment seg;
    seg.ptr = dst;
    seg.len = dst_len;
    return resume_load_segments(&seg, 1, out_len);
}

unsigned char resume_save(const void *src, unsigned int src_len) {
    ResumeWriteSegment seg;
    seg.ptr = src;
    seg.len = src_len;
    return resume_save_segments(&seg, 1);
}

void resume_invalidate(void) {
    unsigned char i;

    if (s_bank == 0) {
        return;
    }

    for (i = 0; i < RESUME_HDR_SIZE; ++i) {
        s_zero_hdr[i] = 0;
    }
    reu_dma_stash((unsigned int)s_zero_hdr, s_bank, REU_RESUME_OFF, RESUME_HDR_SIZE);
    s_last_seq = 0;
}
