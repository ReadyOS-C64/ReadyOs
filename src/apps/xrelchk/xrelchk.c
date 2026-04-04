/*
 * xrelchk.c - Deterministic CAL26 REL harness
 *
 * XRELCHK_CASE:
 *   0 = rebuild + verify
 *   1 = load existing + verify
 *   2 = fresh REL create/write/read probe
 */

#include <cbm.h>
#include <conio.h>
#include <string.h>

#ifndef XRELCHK_CASE
#define XRELCHK_CASE 0
#endif

#ifndef XRELCHK_STOP_AFTER
#define XRELCHK_STOP_AFTER 0
#endif

#ifndef XREL_POS_SEND_MODE
#define XREL_POS_SEND_MODE 1
#endif

#ifndef XREL_POS_CMD_MODE
#define XREL_POS_CMD_MODE 1
#endif

#ifndef XREL_POS_CH_MODE
#define XREL_POS_CH_MODE 1
#endif

#ifndef XREL_POS_REC_BASE_MODE
#define XREL_POS_REC_BASE_MODE 0
#endif

#ifndef XREL_POS_ORDER_MODE
#define XREL_POS_ORDER_MODE 0
#endif

#ifndef XREL_POS_POS_MODE
#define XREL_POS_POS_MODE 0
#endif

#ifndef XREL_POS_CR_MODE
#define XREL_POS_CR_MODE 1
#endif

#ifndef XREL_DATA_SA
#define XREL_DATA_SA 2
#endif

#define LFN_DATA 2
#define LFN_CMD  15
#define REL_SA   XREL_DATA_SA
#define LFN_SEQA 4
#define LFN_SEQZ 5

#define REC_LEN   64
#define CAL_DAYS  365

#define REC_SUPERBLOCK 1
#define REC_DAY_BASE   2
#define REC_EVENT_1    367
#define REC_EVENT_2    368
#define NEXT_RECORD    369

#define TEST_DAY_A 42
#define TEST_DAY_B 50
#define MAX_EVENT_TEXT 46

#define DBG_BASE ((volatile unsigned char*)0xC100)

#define STEP_MODE    1
#define STEP_SCRATCH 2
#define STEP_OPEN    3
#define STEP_POS     4
#define STEP_WRITE   5
#define STEP_READ    6
#define STEP_VERIFY  7
#define STEP_CASE    8
#define STEP_DUMP    9
#define STEP_SEQ     10

static unsigned char g_fail_step;
static unsigned char g_stage_len;
static unsigned char g_stop_hit;
static unsigned char g_pos_locked;
static unsigned char g_pos_send_mode;
static unsigned char g_pos_cmd_mode;
static unsigned char g_pos_ch_mode;
static unsigned char g_pos_rec_base_mode;
static unsigned char g_pos_order_mode;
static unsigned char g_pos_pos_mode;
static unsigned char g_pos_cr_mode;
static unsigned char g_seq_checked;

static void clear_dbg(void) {
    unsigned char i;
    for (i = 0; i < 0x80; ++i) DBG_BASE[i] = 0;
    DBG_BASE[0x00] = 0xA5;
    DBG_BASE[0x01] = 0x0D;
    DBG_BASE[0x05] = (unsigned char)XRELCHK_CASE;
    DBG_BASE[0x06] = 0; /* current checkpoint */
    DBG_BASE[0x07] = (unsigned char)XRELCHK_STOP_AFTER;
}

static void set_fail(unsigned char step, unsigned char detail) {
    if (g_fail_step == 0) {
        g_fail_step = step;
        DBG_BASE[0x03] = step;
        DBG_BASE[0x04] = detail;
    }
}

static void dbg_stage_append_char(char c) {
    if (g_stage_len >= 31) return;
    DBG_BASE[(unsigned char)(0x40 + g_stage_len)] = (unsigned char)c;
    ++g_stage_len;
    DBG_BASE[0x3F] = g_stage_len;
}

static void dbg_stage_reset(const char *label) {
    unsigned char i;
    g_stage_len = 0;
    DBG_BASE[0x3F] = 0;
    for (i = 0; i < 32; ++i) DBG_BASE[(unsigned char)(0x40 + i)] = 0;
    while (*label) dbg_stage_append_char(*label++);
}

static void dbg_stage_mark(char c) {
    if (g_stage_len > 0 && DBG_BASE[(unsigned char)(0x40 + g_stage_len - 1)] != ' ') {
        dbg_stage_append_char(' ');
    }
    dbg_stage_append_char(c);
}

static void dbg_stage_dot(void) {
    dbg_stage_append_char('.');
}

static void dbg_copy(unsigned char off, const unsigned char *src, unsigned char n) {
    unsigned char i;
    for (i = 0; i < n; ++i) DBG_BASE[(unsigned char)(off + i)] = src[i];
}

static void dbg_set_day(unsigned int doy) {
    DBG_BASE[0x18] = (unsigned char)(doy & 0xFF);
    DBG_BASE[0x19] = (unsigned char)(doy >> 8);
}

static unsigned char checkpoint(unsigned char id) {
    DBG_BASE[0x06] = id;
#if XRELCHK_STOP_AFTER > 0
    if (id >= XRELCHK_STOP_AFTER) {
        g_stop_hit = 1;
        return 1;
    }
#endif
    return 0;
}

static unsigned char cmd_status_code(void) {
    char st[40];
    int n;

    n = cbm_read(LFN_CMD, st, sizeof(st) - 1);
    if (n < 2) return 0xFF;
    st[n] = 0;
    if (st[0] < '0' || st[0] > '9' || st[1] < '0' || st[1] > '9') return 0xFE;
    return (unsigned char)((st[0] - '0') * 10 + (st[1] - '0'));
}

static unsigned char pos_channel_byte(unsigned char mode) {
    switch (mode) {
        case 0: return REL_SA;
        case 1: return (unsigned char)(0x60 + REL_SA);
        case 2: return LFN_DATA;
        case 4: return (unsigned char)('0' + REL_SA);
        case 5: return (unsigned char)('0' + LFN_DATA);
        default: return (unsigned char)(0x60 + LFN_DATA);
    }
}

static unsigned char pos_try(unsigned int rec1,
                             unsigned char send_mode,
                             unsigned char cmd_mode,
                             unsigned char ch_mode,
                             unsigned char rec_base_mode,
                             unsigned char order_mode,
                             unsigned char pos_mode,
                             unsigned char cr_mode) {
    unsigned char p[6];
    unsigned char nbytes;
    unsigned int rec;
    int n;
    unsigned char rc;
    unsigned char i;
    unsigned char st;

    rec = rec1;
    if (rec_base_mode && rec > 0) rec = (unsigned int)(rec - 1);

    p[0] = cmd_mode ? 0xD0 : 0x50;
    p[1] = pos_channel_byte(ch_mode);
    if (order_mode == 0) {
        p[2] = (unsigned char)(rec & 0xFF);      /* lo */
        p[3] = (unsigned char)(rec >> 8);        /* hi */
    } else {
        p[2] = (unsigned char)(rec >> 8);        /* hi */
        p[3] = (unsigned char)(rec & 0xFF);      /* lo */
    }
    p[4] = pos_mode ? 0 : 1; /* 0->1, 1->0 */
    nbytes = cr_mode ? 6 : 5;
    p[5] = 0x0D;

    if (send_mode == 0) {
        n = cbm_write(LFN_CMD, p, nbytes);
        DBG_BASE[0x14] = (unsigned char)((n < 0) ? 0xFF : n);
        if (n != nbytes) return 0xFD;
    } else {
        rc = cbm_k_ckout(LFN_CMD);
        if (rc != 0) {
            cbm_k_clrch();
            DBG_BASE[0x14] = rc;
            return 0xFC;
        }
        for (i = 0; i < nbytes; ++i) cbm_k_bsout(p[i]);
        cbm_k_clrch();
        DBG_BASE[0x14] = nbytes;
    }

    DBG_BASE[0x38] = p[1];
    DBG_BASE[0x39] = p[2];
    DBG_BASE[0x3A] = p[3];
    DBG_BASE[0x3B] = p[4];
    DBG_BASE[0x3C] = nbytes;

    st = cmd_status_code();
    DBG_BASE[0x17] = st;
    return st;
}

static unsigned char set_1571_mode(void) {
    int rc;
    rc = cbm_open(14, 8, 15, "u0>m1");
    DBG_BASE[0x10] = (unsigned char)((rc < 0) ? 0xFF : rc);
    cbm_close(14);
    return (rc == 0) ? 0 : 1;
}

static unsigned char scratch_file(const char *name) {
    char cmd[24];
    int rc;

    strcpy(cmd, "s0:");
    strcat(cmd, name);
    rc = cbm_open(14, 8, 15, cmd);
    DBG_BASE[0x11] = (unsigned char)((rc < 0) ? 0xFF : rc);
    cbm_close(14);
    return 0;
}

static unsigned char rel_open(const char *spec, const char *cmd_spec) {
    int rc_cmd;
    int rc_data;

    rc_cmd = cbm_open(LFN_CMD, 8, 15, cmd_spec);
    DBG_BASE[0x13] = (unsigned char)((rc_cmd < 0) ? 0xFF : rc_cmd);
    if (rc_cmd != 0) {
        return 1;
    }

    rc_data = cbm_open(LFN_DATA, 8, REL_SA, spec);
    DBG_BASE[0x12] = (unsigned char)((rc_data < 0) ? 0xFF : rc_data);
    if (rc_data != 0) {
        cbm_close(LFN_CMD);
        return 1;
    }

    /* Drain initial command status (e.g. 73 after I0) so rel_pos sees fresh
     * status for each P command. */
    DBG_BASE[0x1A] = cmd_status_code();

    return 0;
}

static unsigned char rel_open_create(const char *name, unsigned char rec_len, const char *cmd_spec) {
    char spec[24];
    unsigned char n;

    strcpy(spec, "0:");
    strcat(spec, name);
    strcat(spec, ",l,");
    n = (unsigned char)strlen(spec);
    spec[n] = rec_len;
    spec[(unsigned char)(n + 1)] = 0;
    return rel_open(spec, cmd_spec);
}

static unsigned char rel_open_rel(const char *name, unsigned char rec_len, const char *cmd_spec) {
    return rel_open_create(name, rec_len, cmd_spec);
}

static void rel_close(void) {
    cbm_close(LFN_DATA);
    cbm_close(LFN_CMD);
}

static unsigned char seq_read_one(unsigned char lfn,
                                  const char *spec,
                                  unsigned char dbg_off) {
    static unsigned char buf[1];
    unsigned char rc;
    int open_rc;

    open_rc = cbm_open(lfn, 8, 2, spec);
    if (open_rc != 0) {
        DBG_BASE[dbg_off] = (unsigned char)0xFF;
        return 1;
    }

    rc = cbm_k_chkin(lfn);
    if (rc != 0) {
        cbm_k_clrch();
        cbm_close(lfn);
        DBG_BASE[dbg_off] = (unsigned char)0xFE;
        return 1;
    }
    buf[0] = cbm_k_basin();
    cbm_k_clrch();
    cbm_close(lfn);

    DBG_BASE[dbg_off] = 1;
    DBG_BASE[(unsigned char)(dbg_off + 2)] = buf[0];
    return 0;
}

static unsigned char seq_probe_general(void) {
    if (g_seq_checked) return 0;

    dbg_stage_mark('Q');
    if (seq_read_one(LFN_SEQA, "0:XSEQA", 0x1B) != 0) {
        set_fail(STEP_SEQ, 1);
        return 1;
    }
    dbg_stage_dot();

    if (seq_read_one(LFN_SEQZ, "0:XSEQZ", 0x1C) != 0) {
        set_fail(STEP_SEQ, 2);
        return 1;
    }
    dbg_stage_dot();

    g_seq_checked = 0;
    return 0;
}

static unsigned char rel_pos(unsigned int rec1) {
    unsigned char p[5];
    unsigned char rc;
    unsigned char i;
    unsigned char st;

    p[0] = 'p';
    p[1] = (unsigned char)(REL_SA + 96);
    p[2] = (unsigned char)(rec1 & 0xFF); /* lo */
    p[3] = (unsigned char)(rec1 >> 8);   /* hi */
    p[4] = 1;                            /* first byte in record */

    rc = cbm_k_ckout(LFN_CMD);
    if (rc != 0) {
        cbm_k_clrch();
        DBG_BASE[0x14] = rc;
        return 1;
    }
    for (i = 0; i < 5; ++i) cbm_k_bsout(p[i]);
    cbm_k_clrch();
    DBG_BASE[0x14] = 5;
    DBG_BASE[0x38] = p[1];
    DBG_BASE[0x39] = p[2];
    DBG_BASE[0x3A] = p[3];
    DBG_BASE[0x3B] = p[4];
    DBG_BASE[0x3C] = 5;

    st = cmd_status_code();
    DBG_BASE[0x17] = st;
    return 0;
}

static unsigned char rel_write_rec(unsigned int rec1, const unsigned char *buf) {
    unsigned char i;
    unsigned char rc;

    if (rel_pos(rec1) != 0) return 1;

    rc = cbm_k_ckout(LFN_DATA);
    if (rc != 0) {
        cbm_k_clrch();
        DBG_BASE[0x15] = rc;
        return 1;
    }
    for (i = 0; i < REC_LEN; ++i) cbm_k_bsout(buf[i]);
    cbm_k_clrch();
    DBG_BASE[0x15] = REC_LEN;
    return 0;
}

static unsigned char rel_read_rec(unsigned int rec1, unsigned char *buf) {
    int n;

    if (rel_pos(rec1) != 0) return 1;
    n = cbm_read(LFN_DATA, buf, REC_LEN);
    DBG_BASE[0x16] = (unsigned char)((n < 0) ? 0xFF : n);
    if (n != REC_LEN) return 1;
    return 0;
}

static unsigned int rd_dec(const unsigned char *buf, unsigned char off, unsigned char digits) {
    unsigned char i;
    unsigned int out = 0;
    unsigned char c;

    for (i = 0; i < digits; ++i) {
        c = buf[off + i];
        if (c < '0' || c > '9') return 0xFFFF;
        out = (unsigned int)(out * 10 + (unsigned int)(c - '0'));
    }
    return out;
}

static void wr_dec(unsigned char *buf, unsigned char off, unsigned char digits, unsigned int v) {
    unsigned char i;
    for (i = 0; i < digits; ++i) {
        buf[off + digits - 1 - i] = (unsigned char)('0' + (v % 10));
        v /= 10;
    }
}

static unsigned int sb_checksum(const unsigned char *sb) {
    unsigned int s = 0;
    unsigned char i;
    for (i = 0; i < 15; ++i) s += sb[i];
    return s;
}

static void build_super(unsigned char *rec) {
    memset(rec, ' ', REC_LEN);
    rec[0] = 'c';
    rec[1] = '2';
    rec[2] = '6';
    rec[3] = 'e';
    rec[4] = '1';
    wr_dec(rec, 5, 5, 0);
    wr_dec(rec, 10, 5, NEXT_RECORD);
    wr_dec(rec, 15, 5, sb_checksum(rec));
}

static void build_index(unsigned char *rec, unsigned int doy, unsigned int head, unsigned int tail, unsigned int cnt) {
    memset(rec, ' ', REC_LEN);
    rec[0] = 'i';
    wr_dec(rec, 1, 3, doy);
    wr_dec(rec, 6, 5, head);
    wr_dec(rec, 11, 5, tail);
    wr_dec(rec, 16, 5, cnt);
}

static void build_event(unsigned char *rec, unsigned int doy, const char *txt) {
    unsigned char n = (unsigned char)strlen(txt);
    if (n > MAX_EVENT_TEXT) n = MAX_EVENT_TEXT;
    memset(rec, ' ', REC_LEN);
    rec[0] = 'e';
    rec[1] = '0';
    rec[2] = '0';
    wr_dec(rec, 3, 3, doy);
    wr_dec(rec, 6, 5, 0);
    wr_dec(rec, 11, 5, 0);
    wr_dec(rec, 16, 2, n);
    memcpy(&rec[18], txt, n);
}

static unsigned char verify_super(const unsigned char *rec) {
    unsigned int v;

    if (rec[0] != 'c' || rec[1] != '2' || rec[2] != '6' || rec[3] != 'e' || rec[4] != '1') return 1;
    v = rd_dec(rec, 15, 5);
    if (v == 0xFFFF) return 1;
    if (v != sb_checksum(rec)) return 1;
    return 0;
}

static unsigned char verify_index(const unsigned char *rec, unsigned int doy) {
    unsigned int v;

    if (rec[0] != 'i') return 1;
    v = rd_dec(rec, 1, 3);
    if (v == 0xFFFF || v != doy) return 1;
    v = rd_dec(rec, 6, 5);
    if (v == 0xFFFF) return 1;
    v = rd_dec(rec, 11, 5);
    if (v == 0xFFFF) return 1;
    v = rd_dec(rec, 16, 5);
    if (v == 0xFFFF) return 1;
    return 0;
}

static unsigned char verify_event(const unsigned char *rec, unsigned int doy) {
    unsigned int v;

    if (rec[0] != 'e') return 1;
    v = rd_dec(rec, 3, 3);
    if (v == 0xFFFF || v != doy) return 1;
    v = rd_dec(rec, 16, 2);
    if (v == 0xFFFF || v > MAX_EVENT_TEXT) return 1;
    return 0;
}

static unsigned char rebuild_fixture(void) {
    unsigned char rec[REC_LEN];
    unsigned int doy;

    if (seq_probe_general() != 0) return 1;

    dbg_stage_mark('K');
    if (rel_open_rel("cal26.rel", REC_LEN, "") != 0) {
        set_fail(STEP_OPEN, 1);
        return 1;
    }
    if (checkpoint(2)) {
        rel_close();
        return 0;
    }

    dbg_stage_mark('L');

    build_super(rec);
    if (rel_write_rec(REC_SUPERBLOCK, rec) != 0) {
        rel_close();
        set_fail(STEP_WRITE, 1);
        return 1;
    }
    if (checkpoint(3)) {
        rel_close();
        return 0;
    }

    for (doy = 1; doy <= CAL_DAYS; ++doy) {
        dbg_set_day(doy);
        if (doy == TEST_DAY_A) {
            build_index(rec, doy, REC_EVENT_1, REC_EVENT_1, 1);
        } else if (doy == TEST_DAY_B) {
            build_index(rec, doy, REC_EVENT_2, REC_EVENT_2, 1);
        } else {
            build_index(rec, doy, 0, 0, 0);
        }
        if (rel_write_rec((unsigned int)(REC_DAY_BASE + (doy - 1)), rec) != 0) {
            rel_close();
            set_fail(STEP_WRITE, 2);
            return 1;
        }
        if ((doy & 0x1F) == 0 || doy == CAL_DAYS) dbg_stage_dot();
    }
    if (checkpoint(4)) {
        rel_close();
        return 0;
    }

    build_event(rec, TEST_DAY_A, "HARNESS EVENT A");
    if (rel_write_rec(REC_EVENT_1, rec) != 0) {
        rel_close();
        set_fail(STEP_WRITE, 3);
        return 1;
    }

    build_event(rec, TEST_DAY_B, "HARNESS EVENT B");
    if (rel_write_rec(REC_EVENT_2, rec) != 0) {
        rel_close();
        set_fail(STEP_WRITE, 4);
        return 1;
    }
    if (checkpoint(5)) {
        rel_close();
        return 0;
    }

    rel_close();
    return 0;
}

static unsigned char load_verify(void) {
    unsigned char rec[REC_LEN];
    unsigned int doy;

    if (seq_probe_general() != 0) return 1;

    dbg_stage_mark('A');
    if (rel_open_rel("cal26.rel", REC_LEN, "") != 0) {
        set_fail(STEP_OPEN, 2);
        return 1;
    }
    if (checkpoint(6)) {
        rel_close();
        return 0;
    }

    dbg_stage_mark('C');
    if (rel_read_rec(REC_SUPERBLOCK, rec) != 0) {
        rel_close();
        set_fail(STEP_READ, 1);
        return 1;
    }
    if (verify_super(rec) != 0) {
        dbg_copy(0x20, rec, 16);
        rel_close();
        set_fail(STEP_VERIFY, 1);
        return 1;
    }
    if (checkpoint(7)) {
        rel_close();
        return 0;
    }

    dbg_stage_mark('D');
    for (doy = 1; doy <= CAL_DAYS; ++doy) {
        dbg_set_day(doy);
        if (rel_read_rec((unsigned int)(REC_DAY_BASE + (doy - 1)), rec) != 0) {
            rel_close();
            set_fail(STEP_READ, 2);
            return 1;
        }
        if (verify_index(rec, doy) != 0) {
            dbg_copy(0x20, rec, 16);
            rel_close();
            set_fail(STEP_VERIFY, 2);
            return 1;
        }
        if ((doy & 0x1F) == 0 || doy == CAL_DAYS) dbg_stage_dot();
    }
    if (checkpoint(8)) {
        rel_close();
        return 0;
    }

    if (rel_read_rec(REC_EVENT_2, rec) != 0) {
        rel_close();
        set_fail(STEP_READ, 3);
        return 1;
    }
    if (verify_event(rec, TEST_DAY_B) != 0) {
        dbg_copy(0x20, rec, 16);
        rel_close();
        set_fail(STEP_VERIFY, 3);
        return 1;
    }

    if (rel_read_rec(REC_EVENT_1, rec) != 0) {
        rel_close();
        set_fail(STEP_READ, 4);
        return 1;
    }
    if (verify_event(rec, TEST_DAY_A) != 0) {
        dbg_copy(0x20, rec, 16);
        rel_close();
        set_fail(STEP_VERIFY, 4);
        return 1;
    }
    if (checkpoint(9)) {
        rel_close();
        return 0;
    }

    rel_close();
    dbg_stage_mark('J');
    return 0;
}

static unsigned char fresh_rel_probe(void) {
    static unsigned char wbuf[REC_LEN];
    static unsigned char rbuf[REC_LEN];
    unsigned char i;
    int n;

    if (seq_probe_general() != 0) return 1;

    dbg_stage_mark('M');

    scratch_file("xrelrt.rel");
    if (rel_open_create("xrelrt.rel", REC_LEN, "") != 0) {
        set_fail(STEP_OPEN, 3);
        return 1;
    }

    for (i = 0; i < REC_LEN; ++i) wbuf[i] = (unsigned char)(0x40 + (i & 0x1F));

    /* Probe A: write/read first record with explicit P on a freshly-created
     * REL file to isolate C64-native REL creation vs positioning semantics. */
    dbg_stage_mark('N');
    if (rel_pos(1) != 0) {
        rel_close();
        set_fail(STEP_POS, 7);
        return 1;
    }
    n = cbm_write(LFN_DATA, wbuf, REC_LEN);
    DBG_BASE[0x15] = (unsigned char)((n < 0) ? 0xFF : n);
    if (n != REC_LEN) {
        rel_close();
        set_fail(STEP_WRITE, 6);
        return 1;
    }
    rel_close();

    if (rel_open_rel("xrelrt.rel", REC_LEN, "") != 0) {
        set_fail(STEP_OPEN, 4);
        return 1;
    }
    if (rel_pos(1) != 0) {
        rel_close();
        set_fail(STEP_POS, 8);
        return 1;
    }
    n = cbm_read(LFN_DATA, rbuf, REC_LEN);
    DBG_BASE[0x16] = (unsigned char)((n < 0) ? 0xFF : n);
    rel_close();
    if (n != REC_LEN) {
        set_fail(STEP_READ, 6);
        return 1;
    }
    if (memcmp(wbuf, rbuf, REC_LEN) != 0) {
        dbg_copy(0x20, rbuf, 16);
        set_fail(STEP_VERIFY, 6);
        return 1;
    }

    /* Probe B: same file, explicit P then read record 1. */
    dbg_stage_mark('O');
    if (rel_open_rel("xrelrt.rel", REC_LEN, "") != 0) {
        set_fail(STEP_OPEN, 5);
        return 1;
    }
    if (rel_pos(1) != 0) {
        rel_close();
        set_fail(STEP_POS, 6);
        return 1;
    }
    n = cbm_read(LFN_DATA, rbuf, REC_LEN);
    DBG_BASE[0x16] = (unsigned char)((n < 0) ? 0xFF : n);
    rel_close();
    if (n != REC_LEN) {
        set_fail(STEP_READ, 7);
        return 1;
    }
    if (memcmp(wbuf, rbuf, REC_LEN) != 0) {
        dbg_copy(0x20, rbuf, 16);
        set_fail(STEP_VERIFY, 7);
        return 1;
    }

    dbg_stage_mark('P');
    return 0;
}

static void write_debug_dump_file(void) {
    int rc;
    int n;

    scratch_file("xrelstat");
    rc = cbm_open(7, 8, 2, "xrelstat,s,w");
    DBG_BASE[0x0E] = (unsigned char)((rc < 0) ? 0xFF : rc);
    if (rc != 0) {
        set_fail(STEP_DUMP, 1);
        return;
    }

    n = cbm_write(7, (const void*)DBG_BASE, 0x80);
    DBG_BASE[0x0F] = (unsigned char)((n < 0) ? 0xFF : n);
    cbm_close(7);
    if (n != 0x80) set_fail(STEP_DUMP, 2);
}

int main(void) {
    unsigned char ok = 1;

    clrscr();
    clear_dbg();
    g_fail_step = 0;
    g_stop_hit = 0;
    g_pos_locked = 0;
    g_pos_send_mode = 0;
    g_pos_cmd_mode = 0;
    g_pos_ch_mode = 0;
    g_pos_rec_base_mode = 0;
    g_pos_order_mode = 0;
    g_pos_pos_mode = 0;
    g_pos_cr_mode = 0;
    g_seq_checked = 0;

    if (set_1571_mode() != 0) {
        set_fail(STEP_MODE, DBG_BASE[0x10]);
        ok = 0;
    }
    if (ok && checkpoint(1)) {
        DBG_BASE[0x02] = 1;
        DBG_BASE[0x1F] = 0x5A;
        write_debug_dump_file();
        return 0;
    }

    if (ok) {
        if (XRELCHK_CASE == 0) {
            dbg_stage_reset("REBLD");
            if (rebuild_fixture() != 0) ok = 0;
            if (ok && !g_stop_hit && load_verify() != 0) ok = 0;
        } else if (XRELCHK_CASE == 1) {
            dbg_stage_reset("LOAD");
            if (!g_stop_hit && load_verify() != 0) ok = 0;
        } else if (XRELCHK_CASE == 2) {
            dbg_stage_reset("FRESH");
            if (!g_stop_hit && fresh_rel_probe() != 0) ok = 0;
        } else {
            set_fail(STEP_CASE, 0xEE);
            ok = 0;
        }
    }

    DBG_BASE[0x02] = ok ? 1 : 0;
    if (!g_stop_hit) checkpoint(10);
    DBG_BASE[0x1F] = 0x5A;
    write_debug_dump_file();

    return 0;
}
