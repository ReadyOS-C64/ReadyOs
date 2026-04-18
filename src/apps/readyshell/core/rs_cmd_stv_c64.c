#include "rs_cmd_overlay.h"
#include "rs_cmd_registry.h"

#include "rs_cmd_drive_local.h"
#include "rs_cmd_ser_local.h"

#include <cbm.h>
#include <string.h>

#if defined(__CC65__)
#pragma code-name(push, "OVERLAY5")
#pragma rodata-name(push, "OVERLAY5")
#pragma bss-name(push, "OVERLAY5")
#endif

#define STV_META_OFF   RS_CMD_SCRATCH_OFF
#define STV_DATA_OFF   (RS_CMD_SCRATCH_OFF + 0x0100ul)
#define STV_DATA_LEN   (RS_CMD_SCRATCH_LEN - 0x0100u)
#define STV_PATH_MAX   96u

static int stv_name_with_mode(const char* path,
                              unsigned char fallback_drive,
                              unsigned char* drive_out,
                              char* out,
                              unsigned short max) {
  const char* name;
  unsigned char drive;
  unsigned short n;
  if (!path || !drive_out || !out || max < 8u) {
    return -1;
  }
  if (rs_cmd_drive_parse_prefix(path, fallback_drive, &drive, &name) != 0) {
    return -1;
  }
  n = (unsigned short)strlen(name);
  if (rs_cmd_drive_has_char(name, ',')) {
    if (n + 1u > max) {
      return -1;
    }
    memcpy(out, name, n + 1u);
  } else {
    if ((unsigned long)n + 8ul > (unsigned long)max) {
      return -1;
    }
    out[0] = '0';
    out[1] = ':';
    memcpy(out + 2u, name, n);
    out[2u + n] = ',';
    out[3u + n] = 's';
    out[4u + n] = ',';
    out[5u + n] = 'w';
    out[6u + n] = '\0';
  }
  *drive_out = drive;
  return 0;
}

static int stv_parse_fallback_drive_arg(const RSCommandFrame* frame,
                                        unsigned short drive_arg_index,
                                        unsigned char* out_drive) {
  unsigned short drive16;

  if (!frame || !out_drive) {
    return -1;
  }
  if (frame->arg_count <= drive_arg_index) {
    *out_drive = 8u;
    return 0;
  }
  if (rs_cmd_value_to_u16(&frame->args[drive_arg_index], &drive16) != 0 ||
      drive16 < 8u || drive16 > 11u) {
    return -1;
  }
  *out_drive = (unsigned char)drive16;
  return 0;
}

static int stv_meta_write(unsigned short used, unsigned short count, const char* path) {
  unsigned short n;
  unsigned short i;
  for (i = 0u; i < sizeof(rs_cmd_ser_buf); ++i) {
    rs_cmd_ser_buf[i] = 0u;
  }
  if (!path) {
    path = "";
  }
  n = (unsigned short)strlen(path);
  if (n >= STV_PATH_MAX) {
    return -1;
  }
  rs_cmd_ser_buf[0] = (unsigned char)(used & 0xFFu);
  rs_cmd_ser_buf[1] = (unsigned char)((used >> 8u) & 0xFFu);
  rs_cmd_ser_buf[2] = (unsigned char)(count & 0xFFu);
  rs_cmd_ser_buf[3] = (unsigned char)((count >> 8u) & 0xFFu);
  rs_cmd_ser_buf[4] = (unsigned char)n;
  memcpy(rs_cmd_ser_buf + 5u, path, n + 1u);
  return rs_reu_write(STV_META_OFF, rs_cmd_ser_buf, sizeof(rs_cmd_ser_buf));
}

static int stv_meta_read(unsigned short* used, unsigned short* count, char* path, unsigned short max) {
  unsigned short n;
  if (!used || !count || !path || max == 0u) {
    return -1;
  }
  if (rs_reu_read(STV_META_OFF, rs_cmd_ser_buf, sizeof(rs_cmd_ser_buf)) != 0) {
    return -1;
  }
  *used = (unsigned short)(rs_cmd_ser_buf[0] | ((unsigned short)rs_cmd_ser_buf[1] << 8u));
  *count = (unsigned short)(rs_cmd_ser_buf[2] | ((unsigned short)rs_cmd_ser_buf[3] << 8u));
  n = rs_cmd_ser_buf[4];
  if (n + 1u > max || n >= STV_PATH_MAX) {
    return -1;
  }
  memcpy(path, rs_cmd_ser_buf + 5u, n);
  path[n] = '\0';
  return 0;
}

static int stv_write_reu_file(const char* path,
                              unsigned char fallback_drive,
                              unsigned short len) {
  char namebuf[96];
  unsigned char drive;
  unsigned short written;
  unsigned short chunk;
  int n;
  if (!path ||
      stv_name_with_mode(path, fallback_drive, &drive, namebuf, sizeof(namebuf)) != 0) {
    return -1;
  }
  if (cbm_open(2, drive, CBM_WRITE, namebuf) != 0) {
    cbm_k_clrch();
    return -1;
  }
  written = 0u;
  while (written < len) {
    chunk = (unsigned short)(len - written);
    if (chunk > sizeof(rs_cmd_ser_buf)) {
      chunk = sizeof(rs_cmd_ser_buf);
    }
    if (rs_reu_read(STV_DATA_OFF + (unsigned long)written, rs_cmd_ser_buf, chunk) != 0) {
      cbm_close(2);
      cbm_k_clrch();
      return -1;
    }
    n = cbm_write(2, rs_cmd_ser_buf, chunk);
    if (n < 0 || (unsigned short)n != chunk) {
      cbm_close(2);
      cbm_k_clrch();
      return -1;
    }
    written = (unsigned short)(written + chunk);
  }
  cbm_close(2);
  cbm_k_clrch();
  return 0;
}

static int stv_patch_header(unsigned short used, unsigned short count) {
  unsigned short payload_len;
  unsigned char b[2];
  if (used < 9u) {
    return -1;
  }
  payload_len = (unsigned short)(used - 6u);
  b[0] = (unsigned char)(payload_len & 0xFFu);
  b[1] = (unsigned char)((payload_len >> 8u) & 0xFFu);
  if (rs_reu_write(STV_DATA_OFF + 4ul, b, 2u) != 0) {
    return -1;
  }
  b[0] = (unsigned char)(count & 0xFFu);
  b[1] = (unsigned char)((count >> 8u) & 0xFFu);
  return rs_reu_write(STV_DATA_OFF + 7ul, b, 2u);
}

static int stv_begin(RSCommandFrame* frame) {
  const char* path;
  char stored_path[STV_PATH_MAX];
  unsigned char header[9];
  unsigned char fallback_drive;
  if (!frame || !frame->args || frame->arg_count < 1u) {
    return -1;
  }
  path = rs_cmd_value_cstr(&frame->args[0]);
  if (!path || frame->arg_count > 2u ||
      stv_parse_fallback_drive_arg(frame, 1u, &fallback_drive) != 0 ||
      rs_cmd_drive_canonicalize_path(path,
                                     fallback_drive,
                                     stored_path,
                                     sizeof(stored_path)) != 0) {
    return -2;
  }
  header[0] = 'R';
  header[1] = 'S';
  header[2] = 'V';
  header[3] = '1';
  header[4] = 0u;
  header[5] = 0u;
  header[6] = (unsigned char)RS_VAL_ARRAY;
  header[7] = 0u;
  header[8] = 0u;
  if (rs_reu_write(STV_DATA_OFF, header, sizeof(header)) != 0) {
    return -1;
  }
  frame->used = sizeof(header);
  frame->count = 0u;
  return stv_meta_write(frame->used, frame->count, stored_path);
}

static int stv_process(RSCommandFrame* frame) {
  unsigned short used;
  unsigned short count;
  char path[STV_PATH_MAX];
  if (!frame || !frame->item) {
    return -1;
  }
  if (stv_meta_read(&used, &count, path, sizeof(path)) != 0) {
    return -1;
  }
  if (rs_cmd_ser_value_to_reu(frame->item, STV_DATA_OFF, &used, STV_DATA_LEN) != 0) {
    return -3;
  }
  ++count;
  frame->used = used;
  frame->count = count;
  return stv_meta_write(used, count, path);
}

static int stv_end(RSCommandFrame* frame) {
  unsigned short used;
  unsigned short count;
  char path[STV_PATH_MAX];
  unsigned char fallback_drive;
  int ok;
  if (!frame || !frame->out) {
    return -1;
  }
  if (stv_meta_read(&used, &count, path, sizeof(path)) != 0 ||
      stv_parse_fallback_drive_arg(frame, 1u, &fallback_drive) != 0 ||
      stv_patch_header(used, count) != 0) {
    return -1;
  }
  ok = (stv_write_reu_file(path, fallback_drive, used) == 0);
  rs_cmd_value_free(frame->out);
  rs_cmd_value_init_bool(frame->out, ok);
  frame->used = used;
  frame->count = count;
  return 0;
}

static int stv_run_direct(RSCommandFrame* frame) {
  const char* path;
  const RSValue* value;
  unsigned char fallback_drive;
  unsigned short used;
  unsigned short payload_len;
  unsigned char h[6];
  int ok;
  if (!frame || !frame->args || !frame->out) {
    return -1;
  }
  if (frame->item) {
    if (frame->arg_count < 1u || frame->arg_count > 2u) {
      return -2;
    }
    value = frame->item;
    path = rs_cmd_value_cstr(&frame->args[0]);
  } else {
    if (frame->arg_count < 2u || frame->arg_count > 3u) {
      return -2;
    }
    value = &frame->args[0];
    path = rs_cmd_value_cstr(&frame->args[1]);
  }
  if (!path ||
      stv_parse_fallback_drive_arg(frame, frame->item ? 1u : 2u, &fallback_drive) != 0) {
    return -2;
  }
  h[0] = 'R';
  h[1] = 'S';
  h[2] = 'V';
  h[3] = '1';
  h[4] = 0u;
  h[5] = 0u;
  if (rs_reu_write(STV_DATA_OFF, h, sizeof(h)) != 0) {
    return -1;
  }
  used = 6u;
  if (rs_cmd_ser_value_to_reu(value, STV_DATA_OFF, &used, STV_DATA_LEN) != 0) {
    return -3;
  }
  payload_len = (unsigned short)(used - 6u);
  h[0] = (unsigned char)(payload_len & 0xFFu);
  h[1] = (unsigned char)((payload_len >> 8u) & 0xFFu);
  if (rs_reu_write(STV_DATA_OFF + 4ul, h, 2u) != 0) {
    return -1;
  }
  ok = (stv_write_reu_file(path, fallback_drive, used) == 0);
  rs_cmd_value_free(frame->out);
  rs_cmd_value_init_bool(frame->out, ok);
  frame->used = used;
  return 0;
}

int rs_vmovl_overlay5(unsigned char handler, RSCommandFrame* frame) {
  if (!frame || handler != RS_CMD_HANDLER_OVL5_STV) {
    return -1;
  }
  if (frame->op == RS_CMD_OVL_OP_BEGIN) {
    return stv_begin(frame);
  }
  if (frame->op == RS_CMD_OVL_OP_PROCESS) {
    return stv_process(frame);
  }
  if (frame->op == RS_CMD_OVL_OP_END) {
    return stv_end(frame);
  }
  if (frame->op == RS_CMD_OVL_OP_RUN) {
    return stv_run_direct(frame);
  }
  return -1;
}

#if defined(__CC65__)
#pragma bss-name(pop)
#pragma rodata-name(pop)
#pragma code-name(pop)
#endif
