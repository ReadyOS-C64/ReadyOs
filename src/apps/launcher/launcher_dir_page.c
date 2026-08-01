/*
 * launcher_dir_page.c - Launcher-local paged CBM directory reader.
 *
 * The shared reader scans to EOF to count every matching file. On the C64U D81
 * path this can leave the launcher sitting on "READING DISK..." for too long,
 * so the launcher only reads enough entries for the requested page plus one.
 */

#include "../../lib/dir_page.h"

#include <cbm.h>
#include <string.h>

#define DIR_PAGE_LFN_DIR 1
#define DIR_PAGE_READ_CHUNK 96u
#define KERNAL_ST_EOF       0x40u
#define KERNAL_ST_NO_DEVICE 0x80u

extern unsigned char launcher_resource_buf[];

static unsigned char dir_page_buf_pos;
static unsigned char dir_page_buf_len;
static unsigned char dir_page_buf_eof;

static void dir_page_cleanup_io(void) {
    cbm_k_clrch();
    cbm_k_clall();
}

static void dir_page_read_reset(void) {
    dir_page_buf_pos = 0u;
    dir_page_buf_len = 0u;
    dir_page_buf_eof = 0u;
}

static unsigned char dir_page_read_byte(unsigned char *out) {
    unsigned char raw;
    int n;

    while (dir_page_buf_pos >= dir_page_buf_len) {
        if (dir_page_buf_eof) {
            return 0u;
        }

        dir_page_buf_pos = 0u;
        dir_page_buf_len = 0u;
        n = cbm_read(DIR_PAGE_LFN_DIR, launcher_resource_buf,
                     DIR_PAGE_READ_CHUNK);
        if (n <= 0) {
            dir_page_buf_eof = 1u;
            return 0u;
        }
        dir_page_buf_len = (unsigned char)n;
        raw = cbm_k_readst();
        if ((unsigned char)n < DIR_PAGE_READ_CHUNK ||
            (raw & (KERNAL_ST_EOF | KERNAL_ST_NO_DEVICE)) != 0u) {
            dir_page_buf_eof = 1u;
        }
    }

    *out = launcher_resource_buf[dir_page_buf_pos++];
    return 1u;
}

static unsigned char dir_page_read_bytes(unsigned char *out,
                                         unsigned char count) {
    unsigned char i;

    for (i = 0u; i < count; ++i) {
        if (!dir_page_read_byte(&out[i])) {
            return 0u;
        }
    }
    return 1u;
}

static unsigned char dir_page_upchar(unsigned char ch) {
    ch &= 0x7Fu;
    if (ch >= 'a' && ch <= 'z') {
        return (unsigned char)(ch - ('a' - 'A'));
    }
    return ch;
}

static unsigned char dir_page_type_from_text(const char *type_text) {
    unsigned char a;
    unsigned char b;
    unsigned char c;

    a = dir_page_upchar((unsigned char)type_text[0]);
    b = dir_page_upchar((unsigned char)type_text[1]);
    c = dir_page_upchar((unsigned char)type_text[2]);

    if (a == 'S' && b == 'E' && c == 'Q') return CBM_T_SEQ;
    if (a == 'P' && b == 'R' && c == 'G') return CBM_T_PRG;
    if (a == 'U' && b == 'S' && c == 'R') return CBM_T_USR;
    if (a == 'R' && b == 'E' && c == 'L') return CBM_T_REL;
    if (a == 'D' && b == 'I' && c == 'R') return CBM_T_DIR;
    if (a == 'C' && b == 'B' && c == 'M') return CBM_T_CBM;
    if (a == 'D' && b == 'E' && c == 'L') return CBM_T_DEL;
    return CBM_T_DEL;
}

static unsigned char dir_page_type_matches(unsigned char filter_type,
                                           unsigned char type) {
    return (unsigned char)(filter_type == DIR_PAGE_TYPE_ANY || filter_type == type);
}

unsigned char dir_page_read(unsigned char device,
                            unsigned char start_index,
                            unsigned char filter_type,
                            DirPageEntry *entries,
                            unsigned char max_entries,
                            unsigned char *out_count,
                            unsigned char *out_total_count) {
    unsigned char ptr[2];
    unsigned char num[2];
    unsigned char ch;
    unsigned char count;
    unsigned char total_count;
    unsigned char first_line;
    unsigned char in_quotes;
    unsigned char name_pos;
    unsigned char type_pos;
    unsigned char past_space;
    unsigned char entry_type;
    char name_text[DIR_PAGE_NAME_LEN];
    char type_text[4];

    count = 0u;
    total_count = 0u;
    if (out_count != 0) {
        *out_count = 0u;
    }
    if (out_total_count != 0) {
        *out_total_count = 0u;
    }

    dir_page_cleanup_io();
    if (cbm_open(DIR_PAGE_LFN_DIR, device, 0, "$") != 0) {
        dir_page_cleanup_io();
        return DIR_PAGE_RC_IO;
    }

    dir_page_read_reset();
    (void)dir_page_read_bytes(ptr, 2u);
    first_line = 1u;

    while (1) {
        if (!dir_page_read_bytes(ptr, 2u)) {
            break;
        }
        if (!dir_page_read_bytes(num, 2u)) {
            break;
        }
        if (ptr[0] == 0u && ptr[1] == 0u) {
            break;
        }

        in_quotes = 0u;
        name_pos = 0u;
        name_text[0] = 0;
        while (1) {
            if (!dir_page_read_byte(&ch) || ch == 0u) {
                break;
            }
            if (ch == 0x22u) {
                if (in_quotes) {
                    break;
                }
                in_quotes = 1u;
                continue;
            }
            if (in_quotes && name_pos + 1u < DIR_PAGE_NAME_LEN) {
                name_text[name_pos++] = (char)ch;
            }
        }
        name_text[name_pos] = 0;

        type_pos = 0u;
        past_space = 0u;
        type_text[0] = 0;
        type_text[1] = 0;
        type_text[2] = 0;
        type_text[3] = 0;

        if (ch != 0u) {
            while (1) {
                if (!dir_page_read_byte(&ch) || ch == 0u) {
                    break;
                }
                if (!past_space) {
                    if (ch != ' ' && ch != 0xA0u) {
                        past_space = 1u;
                        if (type_pos < 3u) {
                            type_text[type_pos++] = (char)ch;
                        }
                    }
                } else if (type_pos < 3u && ch != ' ' && ch != 0xA0u) {
                    type_text[type_pos++] = (char)ch;
                }
            }
        }

        if (first_line) {
            first_line = 0u;
            continue;
        }
        if (name_pos == 0u) {
            continue;
        }

        entry_type = dir_page_type_from_text(type_text);
        if (!dir_page_type_matches(filter_type, entry_type)) {
            continue;
        }

        if (total_count >= start_index && count < max_entries && entries != 0) {
            strcpy(entries[count].name, name_text);
            entries[count].type = entry_type;
            ++count;
        }
        if (total_count != 255u) {
            ++total_count;
        }
        if (total_count > (unsigned char)(start_index + max_entries)) {
            break;
        }
    }

    cbm_close(DIR_PAGE_LFN_DIR);
    dir_page_cleanup_io();

    if (out_count != 0) {
        *out_count = count;
    }
    if (out_total_count != 0) {
        *out_total_count = total_count;
    }
    return DIR_PAGE_RC_OK;
}

const char *dir_page_type_text(unsigned char type) {
    switch (type) {
        case CBM_T_SEQ: return "SEQ";
        case CBM_T_PRG: return "PRG";
        case CBM_T_USR: return "USR";
        case CBM_T_REL: return "REL";
        case CBM_T_DIR: return "DIR";
        case CBM_T_CBM: return "CBM";
        case CBM_T_DEL: return "DEL";
        default:        return "???";
    }
}

unsigned char dir_page_type_mode(unsigned char type) {
    switch (type) {
        case CBM_T_PRG: return 'p';
        case CBM_T_USR: return 'u';
        case CBM_T_REL: return 'l';
        default:        return 's';
    }
}
