#include "rs_cmd_overlay.h"

#include "rs_cmd_dir_local.h"
#include "rs_cmd_file_local.h"
#include "rs_token.h"
#include "rs_cmd_value_local.h"
#include "../platform/rs_platform.h"

#include <cbm.h>
#include <string.h>

#if defined(__CC65__)
#pragma code-name(push, "OVERLAY3")
#pragma rodata-name(push, "OVERLAY3")
#pragma bss-name(push, "OVERLAY3")
#endif

#define LST_RECORD_SIZE 28u
#define LST_MAX_RECORDS (RS_CMD_SCRATCH_LEN / LST_RECORD_SIZE)
#define LST_NAME_OFF 2u
#define LST_NAME_LEN 17u
#define LST_TYPE_OFF 19u
#define LST_TYPE_LEN 4u
#define LST_PATTERN_LEN 17u
#define LST_SPEC_LEN    18u

#define LST_TYPE_MASK_SEQ 0x01u
#define LST_TYPE_MASK_PRG 0x02u
#define LST_TYPE_MASK_USR 0x04u
#define LST_TYPE_MASK_REL 0x08u
#define LST_TYPE_MASK_DIR 0x10u
#define LST_TYPE_MASK_CBM 0x20u
#define LST_TYPE_MASK_DEL 0x40u

static unsigned char g_lst_buf[LST_RECORD_SIZE];
static char g_lst_name[20];
static char g_lst_type[4];
static char g_lst_pattern[LST_PATTERN_LEN];
static char g_lst_spec[LST_SPEC_LEN];

static unsigned char lst_type_mask(unsigned char type) {
  switch (type) {
    case CBM_T_SEQ: return LST_TYPE_MASK_SEQ;
    case CBM_T_PRG: return LST_TYPE_MASK_PRG;
    case CBM_T_USR: return LST_TYPE_MASK_USR;
    case CBM_T_REL: return LST_TYPE_MASK_REL;
    case CBM_T_DIR: return LST_TYPE_MASK_DIR;
    case CBM_T_CBM: return LST_TYPE_MASK_CBM;
    case CBM_T_DEL: return LST_TYPE_MASK_DEL;
    default: break;
  }
  return 0u;
}

static unsigned char lst_type_token_mask(const char* token) {
  if (!token || token[0] == '\0') {
    return 0u;
  }
  if (rs_ci_equal(token, "SEQ")) {
    return LST_TYPE_MASK_SEQ;
  }
  if (rs_ci_equal(token, "PRG")) {
    return LST_TYPE_MASK_PRG;
  }
  if (rs_ci_equal(token, "USR")) {
    return LST_TYPE_MASK_USR;
  }
  if (rs_ci_equal(token, "REL")) {
    return LST_TYPE_MASK_REL;
  }
  if (rs_ci_equal(token, "DIR")) {
    return LST_TYPE_MASK_DIR;
  }
  if (rs_ci_equal(token, "CBM")) {
    return LST_TYPE_MASK_CBM;
  }
  if (rs_ci_equal(token, "DEL")) {
    return LST_TYPE_MASK_DEL;
  }
  return 0u;
}

static int lst_parse_type_filter(const char* src, unsigned char* out_mask) {
  unsigned char mask;
  char token[4];
  unsigned char tok_len;
  unsigned char type_mask;
  char ch;

  if (!src || !out_mask) {
    return -1;
  }

  mask = 0u;
  tok_len = 0u;
  while (*src != '\0') {
    while (*src == ' ' || *src == '\t') {
      ++src;
    }
    tok_len = 0u;
    while ((ch = *src) != '\0' && ch != ',') {
      if (ch != ' ' && ch != '\t') {
        if (tok_len >= (unsigned char)(sizeof(token) - 1u)) {
          return -1;
        }
        token[tok_len++] = (char)rs_ci_char((unsigned char)ch);
      }
      ++src;
    }
    token[tok_len] = '\0';
    type_mask = lst_type_token_mask(token);
    if (type_mask == 0u) {
      return -1;
    }
    mask = (unsigned char)(mask | type_mask);
    if (*src == ',') {
      ++src;
      if (*src == '\0') {
        return -1;
      }
    }
  }

  if (mask == 0u) {
    return -1;
  }
  *out_mask = mask;
  return 0;
}

static int lst_copy_pattern(char* out, unsigned short max, const char* src) {
  unsigned short n;

  if (!out || max == 0u || !src) {
    return -1;
  }
  n = (unsigned short)strlen(src);
  if (n + 1u > max) {
    return -1;
  }
  memcpy(out, src, n + 1u);
  return 0;
}

static int lst_parse_args(RSCommandFrame* frame,
                          unsigned char* out_drive,
                          char* out_pattern,
                          unsigned short pattern_max,
                          unsigned char* out_type_filter) {
  unsigned short drive16;
  const char* src0;
  const char* src1;
  const char* src2;
  const char* pattern;
  unsigned char drive;

  if (!frame || !out_drive || !out_pattern || pattern_max == 0u || !out_type_filter) {
    return -1;
  }

  drive = rs_cmd_file_default_drive();
  out_pattern[0] = '\0';
  *out_type_filter = 0u;

  if (frame->arg_count == 0u) {
    *out_drive = drive;
    return 0;
  }

  src0 = rs_cmd_value_cstr(&frame->args[0]);
  src1 = (frame->arg_count >= 2u) ? rs_cmd_value_cstr(&frame->args[1]) : 0;
  src2 = (frame->arg_count >= 3u) ? rs_cmd_value_cstr(&frame->args[2]) : 0;

  if (src0) {
    if (rs_cmd_file_parse_drive_prefix(src0, drive, &drive, &pattern) != 0 ||
        lst_copy_pattern(out_pattern, pattern_max, pattern) != 0) {
      return -1;
    }
    if (frame->arg_count == 2u && src1) {
      if (lst_parse_type_filter(src1, out_type_filter) != 0) {
        return -1;
      }
    } else if (frame->arg_count >= 2u) {
      if (rs_cmd_value_to_u16(&frame->args[1], &drive16) != 0 || drive16 > 255u) {
        return -1;
      }
      if (src0[0] < '0' || src0[0] > '9') {
        drive = (unsigned char)drive16;
      }
      if (frame->arg_count == 3u) {
        if (!src2 || lst_parse_type_filter(src2, out_type_filter) != 0) {
          return -1;
        }
      } else if (frame->arg_count > 3u) {
        return -1;
      }
    } else if (frame->arg_count > 3u) {
      return -1;
    }
    *out_drive = drive;
    return 0;
  }

  if (rs_cmd_value_to_u16(&frame->args[0], &drive16) != 0 || drive16 > 255u) {
    return -1;
  }
  *out_drive = (unsigned char)drive16;
  if (frame->arg_count == 1u) {
    return 0;
  }
  if (!src1 || lst_parse_type_filter(src1, out_type_filter) != 0) {
    return -1;
  }
  if (frame->arg_count != 2u) {
    return -1;
  }
  return 0;
}

static int lst_build_dir_spec(const char* pattern,
                              char* out,
                              unsigned short max) {
  unsigned short n;

  if (!out || max < 2u) {
    return -1;
  }

  out[0] = '$';
  if (!pattern || pattern[0] == '\0') {
    out[1] = '\0';
    return 0;
  }

  n = (unsigned short)strlen(pattern);
  if (n + 2u > max) {
    return -1;
  }
  memcpy(out + 1u, pattern, n + 1u);
  return 0;
}

static int lst_open_header(unsigned char drive,
                           const char* pattern,
                           struct cbm_dirent* ent) {
  unsigned char mode;
  unsigned char st;

  if (!ent || lst_build_dir_spec(pattern, g_lst_spec, sizeof(g_lst_spec)) != 0) {
    return -1;
  }

  for (mode = 0u; mode < 4u; ++mode) {
    rs_cmd_dir_prepare(drive, mode);
    if (cbm_opendir(RS_CMD_DIR_LFN, drive, g_lst_spec) != 0) {
      continue;
    }

    st = cbm_readdir(RS_CMD_DIR_LFN, ent);
    if (st == 0u) {
      return 0;
    }

    cbm_closedir(RS_CMD_DIR_LFN);
    rs_cmd_dir_cleanup_io();
  }

  return -1;
}

static void lst_name_from_dirent(const char* in, char* out, unsigned short max) {
  unsigned short i;
  unsigned short j;
  char c;
  i = 0u;
  j = 0u;
  while (i < 16u && in[i] != '\0' && j + 1u < max) {
    c = in[i++];
    if (c == '"') {
      continue;
    }
    out[j++] = c;
  }
  while (j > 0u && out[j - 1u] == ' ') {
    --j;
  }
  out[j] = '\0';
}

static void lst_type_to_text(unsigned char type, char* out, unsigned short max) {
  const char* text;
  unsigned short i;
  if (!out || max == 0u) {
    return;
  }
  switch (type) {
    case CBM_T_SEQ: text = "SEQ"; break;
    case CBM_T_PRG: text = "PRG"; break;
    case CBM_T_USR: text = "USR"; break;
    case CBM_T_REL: text = "REL"; break;
    case CBM_T_DIR: text = "DIR"; break;
    case CBM_T_CBM: text = "CBM"; break;
    case CBM_T_DEL: text = "DEL"; break;
    default: text = "DEL"; break;
  }
  i = 0u;
  while (text[i] != '\0' && i + 1u < max) {
    out[i] = text[i];
    ++i;
  }
  out[i] = '\0';
}

static void lst_clear_record(void) {
  unsigned char i;
  for (i = 0u; i < LST_RECORD_SIZE; ++i) {
    g_lst_buf[i] = 0u;
  }
}

static int lst_write_record(unsigned short index,
                            const char* name,
                            const char* type,
                            unsigned short blocks) {
  unsigned short i;
  unsigned long off;
  lst_clear_record();
  g_lst_buf[0] = (unsigned char)(blocks & 0xFFu);
  g_lst_buf[1] = (unsigned char)((blocks >> 8u) & 0xFFu);
  for (i = 0u; i + 1u < LST_NAME_LEN && name[i] != '\0'; ++i) {
    g_lst_buf[LST_NAME_OFF + i] = (unsigned char)name[i];
  }
  g_lst_buf[LST_NAME_OFF + LST_NAME_LEN - 1u] = '\0';
  for (i = 0u; i + 1u < LST_TYPE_LEN && type[i] != '\0'; ++i) {
    g_lst_buf[LST_TYPE_OFF + i] = (unsigned char)type[i];
  }
  g_lst_buf[LST_TYPE_OFF + LST_TYPE_LEN - 1u] = '\0';
  off = RS_CMD_SCRATCH_OFF + ((unsigned long)index * (unsigned long)LST_RECORD_SIZE);
  return rs_reu_write(off, g_lst_buf, LST_RECORD_SIZE);
}

static int lst_read_record(unsigned short index) {
  unsigned long off;
  off = RS_CMD_SCRATCH_OFF + ((unsigned long)index * (unsigned long)LST_RECORD_SIZE);
  return rs_reu_read(off, g_lst_buf, LST_RECORD_SIZE);
}

static int lst_begin(RSCommandFrame* frame) {
  unsigned char drive;
  unsigned char type_filter;
  unsigned char type_mask;
  unsigned char st;
  unsigned short count;
  struct cbm_dirent ent;

  if (lst_parse_args(frame,
                     &drive,
                     g_lst_pattern,
                     sizeof(g_lst_pattern),
                     &type_filter) != 0) {
    return -2;
  }
  if (lst_open_header(drive, g_lst_pattern, &ent) != 0) {
    return -1;
  }

  count = 0u;
  for (;;) {
    st = cbm_readdir(RS_CMD_DIR_LFN, &ent);
    if (st != 0u) {
      break;
    }
    if (count >= (unsigned short)LST_MAX_RECORDS) {
      cbm_closedir(RS_CMD_DIR_LFN);
      return -3;
    }
    type_mask = lst_type_mask(ent.type);
    if (type_filter != 0u && (type_mask & type_filter) == 0u) {
      continue;
    }
    lst_name_from_dirent(ent.name, g_lst_name, sizeof(g_lst_name));
    lst_type_to_text(ent.type, g_lst_type, sizeof(g_lst_type));
    if (lst_write_record(count, g_lst_name, g_lst_type, ent.size) != 0) {
      cbm_closedir(RS_CMD_DIR_LFN);
      return -1;
    }
    ++count;
  }

  cbm_closedir(RS_CMD_DIR_LFN);
  frame->drive = drive;
  frame->count = count;
  frame->index = 0u;
  return 0;
}

static int lst_item(RSCommandFrame* frame) {
  RSValue vname;
  RSValue vblocks;
  RSValue vtype;
  unsigned short blocks;

  if (!frame || !frame->out || frame->index >= frame->count) {
    return -1;
  }
  if (lst_read_record(frame->index) != 0) {
    return -1;
  }

  blocks = (unsigned short)(g_lst_buf[0] | ((unsigned short)g_lst_buf[1] << 8u));
  rs_cmd_value_free(frame->out);
  if (rs_cmd_value_object_new(frame->out) != 0) {
    return -1;
  }

  rs_cmd_value_init_false(&vname);
  rs_cmd_value_init_false(&vblocks);
  rs_cmd_value_init_false(&vtype);
  if (rs_cmd_value_init_string(&vname, (const char*)(g_lst_buf + LST_NAME_OFF)) != 0 ||
      rs_cmd_value_init_string(&vtype, (const char*)(g_lst_buf + LST_TYPE_OFF)) != 0) {
    rs_cmd_value_free(frame->out);
    return -1;
  }
  rs_cmd_value_init_u16(&vblocks, blocks);

  if (rs_cmd_object_set(frame->out, "NAME", &vname) != 0 ||
      rs_cmd_object_set(frame->out, "BLOCKS", &vblocks) != 0 ||
      rs_cmd_object_set(frame->out, "TYPE", &vtype) != 0) {
    rs_cmd_value_free(&vname);
    rs_cmd_value_free(&vtype);
    rs_cmd_value_free(frame->out);
    return -1;
  }

  rs_cmd_value_free(&vname);
  rs_cmd_value_free(&vtype);
  return 0;
}

int rs_vmovl_overlay3_lst(RSCommandFrame* frame) {
  if (!frame) {
    return -1;
  }
  if (frame->op == RS_CMD_OVL_OP_BEGIN) {
    return lst_begin(frame);
  }
  if (frame->op == RS_CMD_OVL_OP_ITEM) {
    return lst_item(frame);
  }
  return -1;
}

#if defined(__CC65__)
#pragma bss-name(pop)
#pragma rodata-name(pop)
#pragma code-name(pop)
#endif
