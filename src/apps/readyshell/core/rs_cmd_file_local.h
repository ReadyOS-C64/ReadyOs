#ifndef RS_CMD_FILE_LOCAL_H
#define RS_CMD_FILE_LOCAL_H

#include "rs_cmd_dir_local.h"
#include "rs_cmd_value_local.h"
#include "rs_errors.h"
#include "../platform/rs_platform.h"
#include "storage_device.h"

#include <cbm.h>
#include <cbm_filetype.h>
#include <string.h>

#define RS_CMD_FILE_LFN_DATA 2u
#define RS_CMD_FILE_LFN_CMD  15u

static const char rs_cmd_file_err_text[] = "FILE ERROR";
static char rs_cmd_file_status_msg[24];

static int rs_cmd_file_str_eq(const char* a, const char* b) {
  unsigned char ca;
  unsigned char cb;
  do {
    ca = a ? (unsigned char)*a++ : 0u;
    cb = b ? (unsigned char)*b++ : 0u;
    if (ca != cb) {
      return 0;
    }
  } while (ca != '\0');
  return 1;
}

static int rs_cmd_file_has_char(const char* s, char ch) {
  if (!s) {
    return 0;
  }
  while (*s != '\0') {
    if (*s == ch) {
      return 1;
    }
    ++s;
  }
  return 0;
}

static void rs_cmd_file_cleanup_io(void) {
  cbm_k_clrch();
  cbm_k_clall();
}

static unsigned char rs_cmd_file_default_drive(void) {
  unsigned char drive;
  drive = *SHIM_STORAGE_DRIVE;
  if (drive >= 8u && drive <= 11u) {
    return drive;
  }
  if (drive == 9u) {
    return 9u;
  }
  return 8u;
}

static void rs_cmd_file_set_error(RSError* err,
                                  unsigned char code,
                                  const char* message) {
  if (!err) {
    return;
  }
  rs_error_set(err,
               RS_ERR_IO,
               (message && *message) ? message : rs_cmd_file_err_text,
               (unsigned short)code,
               1u,
               1u);
}

static void rs_cmd_file_parse_status_line(const char* line,
                                          unsigned char* code_out,
                                          char* msg_out,
                                          unsigned char msg_cap) {
  unsigned int code;
  unsigned char i;
  const char* p;

  code = 0u;
  p = line;
  while (*p >= '0' && *p <= '9') {
    code = (unsigned int)(code * 10u + (unsigned int)(*p - '0'));
    ++p;
  }
  if (code_out) {
    *code_out = (unsigned char)(code > 255u ? 255u : code);
  }
  if (!msg_out || msg_cap == 0u) {
    return;
  }
  if (*p == ',') {
    ++p;
  }
  while (*p == ' ') {
    ++p;
  }
  i = 0u;
  while (*p != '\0' && *p != ',' && *p != '\r' && *p != '\n' && i + 1u < msg_cap) {
    msg_out[i++] = *p++;
  }
  msg_out[i] = '\0';
  if (msg_out[0] == '\0') {
    strncpy(msg_out, "STATUS", msg_cap - 1u);
    msg_out[msg_cap - 1u] = '\0';
  }
}

static int rs_cmd_file_read_status_open(unsigned char* code_out,
                                        char* msg_out,
                                        unsigned char msg_cap) {
  char line[40];
  int n;

  n = cbm_read(RS_CMD_FILE_LFN_CMD, line, sizeof(line) - 1u);
  if (n < 0) {
    n = 0;
  }
  line[n] = '\0';
  while (n > 0 && (line[n - 1] == '\r' || line[n - 1] == '\n')) {
    line[n - 1] = '\0';
    --n;
  }
  rs_cmd_file_parse_status_line(line, code_out, msg_out, msg_cap);
  return n > 0 ? 0 : -1;
}

static int rs_cmd_file_fetch_status(unsigned char drive,
                                    unsigned char* code_out,
                                    char* msg_out,
                                    unsigned char msg_cap) {
  int rc;

  rs_cmd_file_cleanup_io();
  if (cbm_open(RS_CMD_FILE_LFN_CMD, drive, 15, "") != 0) {
    rs_cmd_file_cleanup_io();
    return -1;
  }
  rc = rs_cmd_file_read_status_open(code_out, msg_out, msg_cap);
  cbm_close(RS_CMD_FILE_LFN_CMD);
  rs_cmd_file_cleanup_io();
  return rc;
}

static void rs_cmd_file_note_status(RSError* err,
                                    unsigned char drive,
                                    unsigned char fallback_code) {
  unsigned char code;
  rs_cmd_file_status_msg[0] = '\0';

  if (rs_cmd_file_fetch_status(drive,
                               &code,
                               rs_cmd_file_status_msg,
                               sizeof(rs_cmd_file_status_msg)) == 0) {
    rs_cmd_file_set_error(err, code, rs_cmd_file_status_msg);
    return;
  }
  rs_cmd_file_set_error(err, fallback_code, rs_cmd_file_err_text);
}

static int rs_cmd_file_parse_drive_prefix(const char* src,
                                          unsigned char fallback_drive,
                                          unsigned char* out_drive,
                                          const char** out_name) {
  unsigned int drive;
  unsigned char digits;
  unsigned char i;

  if (!src || !out_drive || !out_name) {
    return -1;
  }

  drive = 0u;
  digits = 0u;
  i = 0u;
  while (src[i] >= '0' && src[i] <= '9') {
    drive = (drive * 10u) + (unsigned int)(src[i] - '0');
    ++i;
    ++digits;
  }
  if (digits != 0u && src[i] == ':') {
    if (drive < 8u || drive > 11u) {
      return -1;
    }
    *out_drive = (unsigned char)drive;
    *out_name = src + i + 1u;
    return 0;
  }

  *out_drive = fallback_drive;
  *out_name = src;
  return 0;
}

static int rs_cmd_file_parse_embedded_name(const RSValue* arg,
                                           unsigned char* out_drive,
                                           char* out_name,
                                           unsigned short max) {
  const char* src;
  const char* name;
  unsigned char drive;
  unsigned short n;

  src = rs_cmd_value_cstr(arg);
  if (!src || !out_drive || !out_name || max == 0u) {
    return -1;
  }
  if (rs_cmd_file_parse_drive_prefix(src,
                                     rs_cmd_file_default_drive(),
                                     &drive,
                                     &name) != 0) {
    return -1;
  }
  n = (unsigned short)strlen(name);
  if (n == 0u || n + 1u > max) {
    return -1;
  }
  memcpy(out_name, name, n + 1u);
  *out_drive = drive;
  return 0;
}

static int rs_cmd_file_parse_plain_name(const RSValue* arg,
                                        char* out_name,
                                        unsigned short max) {
  const char* src;
  unsigned short n;

  src = rs_cmd_value_cstr(arg);
  if (!src || !out_name || max == 0u || rs_cmd_file_has_char(src, ':')) {
    return -1;
  }
  n = (unsigned short)strlen(src);
  if (n == 0u || n + 1u > max) {
    return -1;
  }
  memcpy(out_name, src, n + 1u);
  return 0;
}

static int rs_cmd_file_parse_optional_drive(const RSValue* arg,
                                            unsigned char fallback_drive,
                                            unsigned char* out_drive) {
  unsigned short drive16;
  if (!out_drive) {
    return -1;
  }
  if (!arg) {
    *out_drive = fallback_drive;
    return 0;
  }
  if (rs_cmd_value_to_u16(arg, &drive16) != 0 || drive16 < 8u || drive16 > 11u) {
    return -1;
  }
  *out_drive = (unsigned char)drive16;
  return 0;
}

static unsigned char rs_cmd_file_type_mode(unsigned char type) {
  switch (type) {
    case CBM_T_PRG:
      return 'p';
    case CBM_T_USR:
      return 'u';
    case CBM_T_REL:
      return 'l';
    default:
      return 's';
  }
}

static int rs_cmd_file_open_name(unsigned char lfn,
                                 unsigned char drive,
                                 const char* name,
                                 unsigned char type,
                                 char mode) {
  char open_name[28];
  unsigned short n;

  if (!name) {
    return -1;
  }
  n = (unsigned short)strlen(name);
  if ((unsigned long)n + 5ul >= (unsigned long)sizeof(open_name)) {
    return -1;
  }
  memcpy(open_name, name, n);
  open_name[n] = ',';
  open_name[n + 1u] = (char)rs_cmd_file_type_mode(type);
  open_name[n + 2u] = ',';
  open_name[n + 3u] = mode;
  open_name[n + 4u] = '\0';
  rs_cmd_file_cleanup_io();
  return cbm_open(lfn, drive, 2, open_name);
}

static void rs_cmd_file_trim_name(const char* src, char* out, unsigned short max) {
  unsigned short i;
  unsigned short j;
  if (!out || max == 0u) {
    return;
  }
  j = 0u;
  if (!src) {
    out[0] = '\0';
    return;
  }
  for (i = 0u; i < 16u && src[i] != '\0' && j + 1u < max; ++i) {
    if (src[i] == '"') {
      continue;
    }
    out[j++] = src[i];
  }
  while (j > 0u && out[j - 1u] == ' ') {
    --j;
  }
  out[j] = '\0';
}

static int rs_cmd_file_find_entry(unsigned char drive,
                                  const char* name,
                                  unsigned char* out_type,
                                  unsigned short* out_size) {
  struct cbm_dirent ent;
  char trimmed[20];

  if (!name) {
    return -1;
  }
  if (rs_cmd_dir_open_header(drive, &ent) != 0) {
    return -1;
  }
  for (;;) {
    rs_cmd_file_trim_name(ent.name, trimmed, sizeof(trimmed));
    if (rs_cmd_file_str_eq(trimmed, name)) {
      cbm_closedir(RS_CMD_DIR_LFN);
      rs_cmd_file_cleanup_io();
      if (out_type) {
        *out_type = ent.type;
      }
      if (out_size) {
        *out_size = ent.size;
      }
      return 0;
    }
    if (cbm_readdir(RS_CMD_DIR_LFN, &ent) != 0u) {
      break;
    }
  }
  cbm_closedir(RS_CMD_DIR_LFN);
  rs_cmd_file_cleanup_io();
  return -1;
}

static int rs_cmd_file_run_command(unsigned char drive,
                                   const char* cmd,
                                   RSError* err) {
  unsigned char code;
  rs_cmd_file_cleanup_io();
  if (cbm_open(RS_CMD_FILE_LFN_CMD, drive, 15, cmd) != 0) {
    rs_cmd_file_note_status(err, drive, 255u);
    return -1;
  }
  rs_cmd_file_status_msg[0] = '\0';
  if (rs_cmd_file_read_status_open(&code,
                                   rs_cmd_file_status_msg,
                                   sizeof(rs_cmd_file_status_msg)) != 0) {
    cbm_close(RS_CMD_FILE_LFN_CMD);
    rs_cmd_file_cleanup_io();
    rs_cmd_file_set_error(err, 255u, rs_cmd_file_err_text);
    return -1;
  }
  cbm_close(RS_CMD_FILE_LFN_CMD);
  rs_cmd_file_cleanup_io();
  if (code <= 1u) {
    return 0;
  }
  rs_cmd_file_set_error(err, code, rs_cmd_file_status_msg);
  return -1;
}

#endif
