#include <cbm.h>
#include <conio.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "sysinfo_uci.h"

#define MAX_ITEMS 48
#define MAX_NAME  13
#define CFG_CAP   1600
#define LINE_CAP  176
#define PATH_CAP  80

#define APP_LOAD_ADDR 0x1000u
#define APP_MAX_LEN   0xB600u
#define RS_LOAD_ADDR  0x8E00u
#define RS_SLOT_LEN   0x3800u

#define KIND_APP 1u
#define KIND_RS  2u

#define PH_OPEN  0
#define PH_INFO  1
#define PH_READ2 2
#define PH_SEEK  3
#define PH_LOAD  4
#define PH_CLOSE 5
#define PH_COUNT 6

#define UCI_DOS_TARGET 1u
#define UCI_CTRL_TARGET 4u

#define CMD_IDENTIFY  0x01u
#define CMD_OPEN      0x02u
#define CMD_CLOSE     0x03u
#define CMD_READ      0x04u
#define CMD_SEEK      0x06u
#define CMD_FILE_INFO 0x07u
#define CMD_CD        0x11u
#define CMD_LOAD_REU  0x21u
#define CMD_MOUNT     0x23u
#define CMD_DRVINFO   0x29u

#define RESULT_ADDR ((unsigned char*)0x3000)

typedef struct WorkItem {
    char name[MAX_NAME];
    unsigned char kind;
    unsigned char bank;
    unsigned int off;
    unsigned int max_len;
    unsigned int expected;
} WorkItem;

typedef struct Stamp {
    unsigned char j0;
    unsigned char j1;
    unsigned char j2;
    unsigned int ta;
} Stamp;

static unsigned char cfg_buf[CFG_CAP];
static unsigned int cfg_len;
static char line_buf[LINE_CAP];
static char image_path[PATH_CAP];
static char image_dir[PATH_CAP];
static char image_name[24];
static WorkItem items[MAX_ITEMS];
static unsigned char item_count;
static unsigned char app_count;
static unsigned char rs_count;
static unsigned char rb_meta_count;
static unsigned char failures;
static unsigned int phase_ms[PH_COUNT];
static unsigned int total_loaded_kb;
static unsigned int total_ms;
static unsigned int max_load_ms;
static unsigned int last_cmd_ms;
static char max_load_name[MAX_NAME];
static unsigned char data_buf[48];
static unsigned char stat_buf[32];
static unsigned char data_len;
static unsigned char stat_len;
static unsigned char cmd_buf[112];

static void put_u16(unsigned char *p, unsigned int v) {
    p[0] = (unsigned char)(v & 0xffu);
    p[1] = (unsigned char)(v >> 8);
}

static void write_results(unsigned char done) {
    unsigned char *p = RESULT_ADDR;
    unsigned char i;
    p[0] = 'U';
    p[1] = 'T';
    p[2] = 'I';
    p[3] = 'M';
    p[4] = 1u;
    p[5] = done;
    p[6] = item_count;
    p[7] = failures;
    put_u16(p + 8, total_ms);
    put_u16(p + 10, total_loaded_kb);
    put_u16(p + 12, max_load_ms);
    for (i = 0; i < PH_COUNT; ++i) {
        put_u16(p + 16u + (unsigned int)i * 2u, phase_ms[i]);
    }
    memset(p + 32, 0, 16);
    for (i = 0; i < MAX_NAME - 1 && max_load_name[i] != 0; ++i) {
        p[32u + i] = (unsigned char)max_load_name[i];
    }
    memset(p + 48, 0, 48);
    for (i = 0; i < 47u && image_path[i] != 0; ++i) {
        p[48u + i] = (unsigned char)image_path[i];
    }
}

static unsigned char lower_byte(unsigned char ch) {
    if (ch >= 0x41u && ch <= 0x5Au) {
        return (unsigned char)(ch + 0x20u);
    }
    return ch;
}

static unsigned char cieq(const char *a, const char *b) {
    while (*a != 0 && *b != 0) {
        if (lower_byte((unsigned char)*a) != lower_byte((unsigned char)*b)) {
            return 0u;
        }
        ++a;
        ++b;
    }
    return (unsigned char)(*a == 0 && *b == 0);
}

static unsigned char ciprefix(const char *s, const char *prefix) {
    while (*prefix != 0) {
        if (*s == 0 ||
            lower_byte((unsigned char)*s) != lower_byte((unsigned char)*prefix)) {
            return 0u;
        }
        ++s;
        ++prefix;
    }
    return 1u;
}

static unsigned char c64u_path_key_match(const char *s) {
    static const unsigned char key[] = {
        0x43, 0x36, 0x34, 0x55, 0x5f, 0x49, 0x4d, 0x41,
        0x47, 0x45, 0x5f, 0x50, 0x41, 0x54, 0x48, 0x3d
    };
    unsigned char i;

    for (i = 0u; i < sizeof(key); ++i) {
        if (lower_byte((unsigned char)s[i]) != lower_byte(key[i])) {
            return 0u;
        }
    }
    return 1u;
}

static unsigned char starts_section(const char *s, const char *name) {
    unsigned char i;
    if (s[0] != '[') {
        return 0u;
    }
    for (i = 1u; s[i] != 0 && s[i] != ']'; ++i) {
        if (!ciprefix(s + i, name)) {
            continue;
        }
        return 1u;
    }
    return 0u;
}

static void trim(char *s) {
    char *p = s;
    unsigned char len;
    while (*p == ' ') {
        ++p;
    }
    if (p != s) {
        memmove(s, p, strlen(p) + 1);
    }
    len = (unsigned char)strlen(s);
    while (len != 0u && (s[(unsigned char)(len - 1u)] == ' ' ||
                         s[(unsigned char)(len - 1u)] == 13)) {
        s[(unsigned char)(len - 1u)] = 0;
        --len;
    }
}

static void copy_token(char *dst, unsigned char cap, const char *src) {
    unsigned char i = 0;
    if (cap == 0u) {
        return;
    }
    while (i < (unsigned char)(cap - 1u) && src[i] != 0) {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = 0;
}

static unsigned char parse_hex_word(const char *s, unsigned int *out) {
    unsigned int v = 0;
    unsigned char i;
    unsigned char ch;
    for (i = 0; s[i] != 0; ++i) {
        ch = (unsigned char)s[i];
        v = (unsigned int)(v << 4);
        if (ch >= '0' && ch <= '9') {
            v = (unsigned int)(v + (unsigned int)(ch - '0'));
        } else if (lower_byte(ch) >= 'a' && lower_byte(ch) <= 'f') {
            ch = lower_byte(ch);
            v = (unsigned int)(v + (unsigned int)(10u + ch - 'a'));
        } else {
            return 0u;
        }
    }
    *out = v;
    return (unsigned char)(i != 0u);
}

static unsigned char add_item(const char *name,
                              unsigned char kind,
                              unsigned char bank,
                              unsigned int off,
                              unsigned int max_len,
                              unsigned int expected) {
    WorkItem *it;
    if (item_count >= MAX_ITEMS || name[0] == 0) {
        return 0u;
    }
    it = &items[item_count++];
    memset(it, 0, sizeof(*it));
    copy_token(it->name, sizeof(it->name), name);
    it->kind = kind;
    it->bank = bank;
    it->off = off;
    it->max_len = max_len;
    it->expected = expected;
    return 1u;
}

static unsigned char load_cfg(void) {
    int n;
    cfg_len = 0;
    if (cbm_open(2, 8, 2, "apps.cfg,s,r") != 0) {
        return 0u;
    }
    for (;;) {
        n = cbm_read(2, cfg_buf + cfg_len, (unsigned int)(CFG_CAP - cfg_len));
        if (n <= 0) {
            break;
        }
        cfg_len = (unsigned int)(cfg_len + (unsigned int)n);
        if (cfg_len >= CFG_CAP) {
            break;
        }
    }
    cbm_close(2);
    return (unsigned char)(cfg_len != 0u && cfg_len < CFG_CAP);
}

static unsigned char next_line(unsigned int *pos) {
    unsigned char len = 0;
    while (*pos < cfg_len && cfg_buf[*pos] != 13) {
        if (len < LINE_CAP - 1) {
            line_buf[len++] = (char)cfg_buf[*pos];
        }
        *pos = (unsigned int)(*pos + 1u);
    }
    if (*pos < cfg_len && cfg_buf[*pos] == 13) {
        *pos = (unsigned int)(*pos + 1u);
    }
    line_buf[len] = 0;
    trim(line_buf);
    return (unsigned char)(len != 0u || *pos < cfg_len);
}

static void parse_rs_deps(char *line) {
    char *cursor = line;
    char *comma;
    char *at;
    char *colon;
    char *name;
    unsigned int off;
    unsigned char ordinal;

    while (cursor != 0 && cursor[0] != 0) {
        comma = strchr(cursor, ',');
        if (comma != 0) {
            *comma = 0;
        }
        trim(cursor);
        at = strchr(cursor, '@');
        colon = at ? strchr(at, ':') : 0;
        if (at != 0 && colon != 0) {
            *at = 0;
            *colon = 0;
            name = cursor;
            trim(name);
            ordinal = (unsigned char)((at[1] >= '0' && at[1] <= '2') ? at[1] - '0' : 0);
            if (parse_hex_word(colon + 1, &off)) {
                add_item(name, KIND_RS, (unsigned char)(0x80u + ordinal),
                         off, RS_SLOT_LEN, RS_LOAD_ADDR);
                ++rs_count;
            }
        }
        if (comma == 0) {
            break;
        }
        cursor = comma + 1;
    }
}

static void parse_app_entry(char *line,
                            char *out_resource,
                            unsigned char *dep_required) {
    char *p0;
    char *p1;
    char *p2;
    char *p3;
    char *p4;
    unsigned char bank;

    *dep_required = 0u;
    out_resource[0] = 0;
    p0 = line;
    p1 = strchr(p0, ':');
    if (p1 == 0) return;
    *p1++ = 0;
    p2 = strchr(p1, ':');
    if (p2 == 0) return;
    *p2++ = 0;
    p3 = strchr(p2, ':');
    if (p3 != 0) {
        *p3++ = 0;
        p4 = strchr(p3, ':');
        if (p4 != 0) {
            *p4++ = 0;
            copy_token(out_resource, 16, p4);
        } else if (p3[0] < '0' || p3[0] > '9') {
            copy_token(out_resource, 16, p3);
        }
    }
    if (out_resource[0] != 0) {
        unsigned char len = (unsigned char)strlen(out_resource);
        if (len != 0u && out_resource[(unsigned char)(len - 1u)] == '+') {
            out_resource[(unsigned char)(len - 1u)] = 0;
            *dep_required = 1u;
        }
    }
    bank = (unsigned char)(0x40u + app_count);
    add_item(p1, KIND_APP, bank, 0u, APP_MAX_LEN, APP_LOAD_ADDR);
    ++app_count;
}

static unsigned char parse_cfg(void) {
    unsigned int pos = 0;
    unsigned char in_launcher = 0;
    unsigned char in_apps = 0;
    unsigned char pending_dep = 0;
    unsigned char pending_desc = 0;
    char pending_resource[16];
    char resource[16];
    unsigned char dep_required;

    image_path[0] = 0;
    while (pos < cfg_len) {
        if (!next_line(&pos) || line_buf[0] == 0) {
            continue;
        }
        if (starts_section(line_buf, "launcher")) {
            in_launcher = 1u;
            in_apps = 0u;
            continue;
        }
        if (starts_section(line_buf, "apps")) {
            in_launcher = 0u;
            in_apps = 1u;
            continue;
        }
        if (line_buf[0] == '[') {
            in_launcher = 0u;
            in_apps = 0u;
            continue;
        }
        if (c64u_path_key_match(line_buf)) {
            copy_token(image_path, sizeof(image_path), line_buf + 16);
            continue;
        }
        if (!in_apps) {
            continue;
        }
        if (pending_dep && pending_desc) {
            parse_rs_deps(line_buf);
            if (cieq(pending_resource, "rbcore")) {
                rb_meta_count = (unsigned char)(rb_meta_count + 2u);
            }
            pending_dep = 0u;
            pending_desc = 0u;
            pending_resource[0] = 0;
            continue;
        }
        if (pending_dep) {
            pending_desc = 1u;
            continue;
        }
        if (line_buf[0] >= '0' && line_buf[0] <= '9' && line_buf[1] == ':') {
            parse_app_entry(line_buf, resource, &dep_required);
            if (dep_required) {
                copy_token(pending_resource, sizeof(pending_resource), resource);
                pending_dep = 1u;
                pending_desc = 0u;
            }
        }
    }
    return (unsigned char)(item_count != 0u && image_path[0] != 0);
}

static unsigned char split_image_path(void) {
    char *slash = 0;
    char *p = image_path;
    while (*p != 0) {
        if (*p == '/') {
            slash = p;
        }
        ++p;
    }
    if (slash == 0 || slash[1] == 0) {
        return 0u;
    }
    copy_token(image_name, sizeof(image_name), slash + 1);
    if (slash == image_path) {
        copy_token(image_dir, sizeof(image_dir), "/");
    } else {
        *slash = 0;
        copy_token(image_dir, sizeof(image_dir), image_path);
        *slash = '/';
    }
    return 1u;
}

static unsigned char path_byte(unsigned char ch) {
    if (ch >= 'a' && ch <= 'z') {
        ch &= 0xdfu;
    }
    return ch;
}

static unsigned char cmd_add_path(unsigned char pos, const char *s) {
    while (*s != 0 && pos < sizeof(cmd_buf)) {
        cmd_buf[pos++] = path_byte((unsigned char)*s++);
    }
    return pos;
}

static unsigned char cmd_add_raw(unsigned char pos, const char *s) {
    while (*s != 0 && pos < sizeof(cmd_buf)) {
        cmd_buf[pos++] = (unsigned char)*s++;
    }
    return pos;
}

static unsigned char uci_cmd(unsigned char len) {
    /* Probe the production System Info transport itself. This call owns the
     * async PUSH/LAST-or-MORE lifecycle and cannot be surrounded by pacing. */
    return sysinfo_uci_command(cmd_buf, len, data_buf, sizeof(data_buf),
                               &data_len, stat_buf, sizeof(stat_buf), &stat_len);
}

static unsigned char status_ok(void) {
    return (unsigned char)(stat_len >= 2u && stat_buf[0] == '0' && stat_buf[1] == '0');
}

static unsigned char data_or_status_ok(void) {
    if (stat_len != 0u) {
        return status_ok();
    }
    return (unsigned char)(data_len != 0u);
}

static unsigned char dos_simple(unsigned char op) {
    cmd_buf[0] = UCI_DOS_TARGET;
    cmd_buf[1] = op;
    return uci_cmd(2);
}

static unsigned char dos_cd(const char *name) {
    unsigned char pos = 0;
    cmd_buf[pos++] = UCI_DOS_TARGET;
    cmd_buf[pos++] = CMD_CD;
    pos = cmd_add_path(pos, name);
    return uci_cmd(pos);
}

static unsigned char dos_mount(void) {
    unsigned char pos = 0;
    cmd_buf[pos++] = UCI_DOS_TARGET;
    cmd_buf[pos++] = CMD_MOUNT;
    cmd_buf[pos++] = 8u;
    pos = cmd_add_path(pos, image_name);
    return uci_cmd(pos);
}

static unsigned char dos_open(const char *name) {
    unsigned char pos = 0;
    cmd_buf[pos++] = UCI_DOS_TARGET;
    cmd_buf[pos++] = CMD_OPEN;
    cmd_buf[pos++] = 1u;
    pos = cmd_add_raw(pos, name);
    return uci_cmd(pos);
}

static unsigned char dos_read2(void) {
    cmd_buf[0] = UCI_DOS_TARGET;
    cmd_buf[1] = CMD_READ;
    cmd_buf[2] = 2u;
    cmd_buf[3] = 0u;
    return uci_cmd(4);
}

static unsigned char dos_seek2(void) {
    cmd_buf[0] = UCI_DOS_TARGET;
    cmd_buf[1] = CMD_SEEK;
    cmd_buf[2] = 2u;
    cmd_buf[3] = 0u;
    cmd_buf[4] = 0u;
    cmd_buf[5] = 0u;
    return uci_cmd(6);
}

static unsigned char dos_load_reu(const WorkItem *it, unsigned int len) {
    cmd_buf[0] = UCI_DOS_TARGET;
    cmd_buf[1] = CMD_LOAD_REU;
    cmd_buf[2] = (unsigned char)(it->off & 0xffu);
    cmd_buf[3] = (unsigned char)(it->off >> 8);
    cmd_buf[4] = it->bank;
    cmd_buf[5] = 0u;
    cmd_buf[6] = (unsigned char)(len & 0xffu);
    cmd_buf[7] = (unsigned char)(len >> 8);
    cmd_buf[8] = 0u;
    cmd_buf[9] = 0u;
    return uci_cmd(10);
}

static void read_stamp(Stamp *s) {
    unsigned char a;
    unsigned char b;
    do {
        a = *(volatile unsigned char*)0xA2;
        s->j1 = *(volatile unsigned char*)0xA1;
        s->j0 = *(volatile unsigned char*)0xA0;
        b = *(volatile unsigned char*)0xA2;
    } while (a != b);
    s->j2 = b;
    s->ta = (unsigned int)(*(volatile unsigned char*)0xDC04) |
            ((unsigned int)(*(volatile unsigned char*)0xDC05) << 8);
}

static unsigned int elapsed_ms(const Stamp *start, const Stamp *end) {
    unsigned long sj = (unsigned long)start->j0 |
                       ((unsigned long)start->j1 << 8) |
                       ((unsigned long)start->j2 << 16);
    unsigned long ej = (unsigned long)end->j0 |
                       ((unsigned long)end->j1 << 8) |
                       ((unsigned long)end->j2 << 16);
    unsigned long dj = (ej - sj) & 0x00ffffffUL;
    long sub = (long)start->ta - (long)end->ta;
    long cycles = (long)(dj * 17095UL) + sub;
    if (cycles < 0) {
        cycles = (long)(dj * 17095UL);
    }
    if (cycles > 65535000L) {
        return 65535u;
    }
    return (unsigned int)((cycles + 500L) / 1000L);
}

static unsigned char timed_cmd(unsigned char phase,
                               unsigned char (*fn)(void),
                               unsigned char need_status) {
    Stamp a;
    Stamp b;
    unsigned int ms;
    unsigned char ok;
    read_stamp(&a);
    ok = fn();
    read_stamp(&b);
    ms = elapsed_ms(&a, &b);
    last_cmd_ms = ms;
    phase_ms[phase] = (unsigned int)(phase_ms[phase] + ms);
    total_ms = (unsigned int)(total_ms + ms);
    if (!ok) {
        return 0u;
    }
    if (need_status && !status_ok()) {
        return 0u;
    }
    return 1u;
}

static const WorkItem *current_item;
static unsigned int current_len;

static unsigned char wrap_open(void) { return dos_open(current_item->name); }
static unsigned char wrap_info(void) { return dos_simple(CMD_FILE_INFO); }
static unsigned char wrap_read2(void) { return dos_read2(); }
static unsigned char wrap_seek2(void) { return dos_seek2(); }
static unsigned char wrap_load(void) { return dos_load_reu(current_item, current_len); }
static unsigned char wrap_close(void) { return dos_simple(CMD_CLOSE); }

static unsigned char setup_ultimate_path(void) {
    cmd_buf[0] = UCI_DOS_TARGET;
    cmd_buf[1] = CMD_IDENTIFY;
    if (!uci_cmd(2)) return 0u;
    cmd_buf[0] = UCI_CTRL_TARGET;
    cmd_buf[1] = CMD_DRVINFO;
    cmd_buf[2] = 0u;
    if (!uci_cmd(3)) return 0u;
    if (!dos_cd("/") || !status_ok()) return 0u;
    if (!dos_cd(image_dir) || !status_ok()) {
        if (image_dir[0] != '/' || image_dir[1] == 0 ||
            !dos_cd(image_dir + 1) || !status_ok()) {
            return 0u;
        }
    }
    if (!dos_mount() || !status_ok()) return 0u;
    if (!dos_cd(image_name) || !status_ok()) {
        if (!dos_cd(image_name) || !status_ok()) {
            return 0u;
        }
    }
    return 1u;
}

static unsigned char load_one(const WorkItem *it) {
    unsigned int file_size;
    unsigned int payload_len;
    unsigned int hdr;
    gotoxy(0, 8);
    cprintf("file %-12s %02u/%02u    ", it->name,
            (unsigned char)((it - items) + 1), item_count);
    current_item = it;
    if (!timed_cmd(PH_OPEN, wrap_open, 1u)) goto fail;
    if (!timed_cmd(PH_INFO, wrap_info, 0u) || !data_or_status_ok()) goto fail;
    if (data_len < 4u || data_buf[2] != 0u || data_buf[3] != 0u) goto fail;
    file_size = (unsigned int)data_buf[0] | ((unsigned int)data_buf[1] << 8);
    if (file_size <= 2u) goto fail;
    payload_len = (unsigned int)(file_size - 2u);
    if (payload_len > it->max_len) {
        if (it->kind == KIND_RS && payload_len <= 0x3A00u) {
            payload_len = it->max_len;
        } else {
            goto fail;
        }
    }
    if (!timed_cmd(PH_READ2, wrap_read2, 0u) || !data_or_status_ok()) goto fail;
    if (data_len < 2u) goto fail;
    hdr = (unsigned int)data_buf[0] | ((unsigned int)data_buf[1] << 8);
    if (hdr != it->expected) goto fail;
    if (!timed_cmd(PH_SEEK, wrap_seek2, 1u)) goto fail;
    current_len = payload_len;
    if (!timed_cmd(PH_LOAD, wrap_load, 1u)) goto fail;
    {
        unsigned int load_ms = last_cmd_ms;
        if (load_ms > max_load_ms) {
            max_load_ms = load_ms;
            copy_token(max_load_name, sizeof(max_load_name), it->name);
        }
    }
    if (!timed_cmd(PH_CLOSE, wrap_close, 0u)) goto fail_close_counted;
    total_loaded_kb = (unsigned int)(total_loaded_kb + ((payload_len + 1023u) >> 10));
    return 1u;
fail:
    ++failures;
    (void)timed_cmd(PH_CLOSE, wrap_close, 0u);
fail_close_counted:
    gotoxy(0, 10);
    cprintf("failed %-12s st:%02x%02x dl:%02u",
            it->name, stat_buf[0], stat_buf[1], data_len);
    return 0u;
}

static void show_summary(void) {
    unsigned int header_ms = (unsigned int)(phase_ms[PH_READ2] + phase_ms[PH_SEEK]);
    clrscr();
    cprintf("uci timing load-all probe\r\n");
    cprintf("items:%u apps:%u rs:%u rbmeta:%u fail:%u\r\n",
            item_count, app_count, rs_count, rb_meta_count, failures);
    cprintf("total:%u ms loaded:%u kb\r\n", total_ms, total_loaded_kb);
    cprintf("open:%u info:%u read2:%u\r\n",
            phase_ms[PH_OPEN], phase_ms[PH_INFO], phase_ms[PH_READ2]);
    cprintf("seek:%u load:%u close:%u\r\n",
            phase_ms[PH_SEEK], phase_ms[PH_LOAD], phase_ms[PH_CLOSE]);
    cprintf("header tax:%u ms (%u files)\r\n", header_ms, item_count);
    cprintf("avg tax/file:%u ms\r\n",
            item_count ? (unsigned int)(header_ms / item_count) : 0u);
    cprintf("max load phase:%u ms\r\n", max_load_ms);
    cprintf("config path:%s\r\n", image_path);
    cprintf("PROBE DONE\r\n");
}

void main(void) {
    unsigned char i;

    clrscr();
    textcolor(COLOR_LIGHTGREEN);
    cprintf("uci timing probe\r\n");
    textcolor(COLOR_WHITE);
    write_results(0u);
    cprintf("reading apps.cfg...\r\n");
    if (!load_cfg()) {
        failures = 1u;
        write_results(2u);
        cprintf("config load failed len:%u\r\n", cfg_len);
        return;
    }
    if (!parse_cfg()) {
        failures = 1u;
        write_results(3u);
        cprintf("config parse failed len:%u path:%s\r\n", cfg_len, image_path);
        return;
    }
    if (!split_image_path()) {
        failures = 1u;
        write_results(4u);
        cprintf("config path failed len:%u path:%s\r\n", cfg_len, image_path);
        return;
    }
    cprintf("cfg ok apps:%u items:%u\r\n", app_count, item_count);
    if (!sysinfo_uci_detect()) {
        cprintf("no uci detected\r\n");
        failures = 1u;
        write_results(3u);
        return;
    }
    cprintf("uci base:%04x path:%s\r\n", sysinfo_uci_base(), image_path);
    cprintf("mounting via ultimate dos...\r\n");
    if (!setup_ultimate_path()) {
        cprintf("ultimate path setup failed st:%02x%02x\r\n", stat_buf[0], stat_buf[1]);
        failures = 1u;
        write_results(4u);
        return;
    }
    for (i = 0; i < item_count; ++i) {
        (void)load_one(&items[i]);
        write_results(0u);
    }
    write_results(1u);
    show_summary();
}
