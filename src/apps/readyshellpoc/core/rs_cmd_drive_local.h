#ifndef RS_CMD_DRIVE_LOCAL_H
#define RS_CMD_DRIVE_LOCAL_H

#include <string.h>

static int rs_cmd_drive_parse_prefix(const char* src,
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

  if (digits != 0u && src[i] == ':' && drive >= 8u && drive <= 11u) {
    *out_drive = (unsigned char)drive;
    *out_name = src + i + 1u;
    return 0;
  }

  *out_drive = fallback_drive;
  *out_name = src;
  return 0;
}

static int rs_cmd_drive_has_char(const char* src, char ch) {
  if (!src) {
    return 0;
  }
  while (*src != '\0') {
    if (*src == ch) {
      return 1;
    }
    ++src;
  }
  return 0;
}

static int rs_cmd_drive_canonicalize_path(const char* src,
                                          unsigned char fallback_drive,
                                          char* out,
                                          unsigned short max) {
  const char* name;
  unsigned char drive;
  unsigned short n;

  if (!src || !out || max == 0u) {
    return -1;
  }
  if (rs_cmd_drive_parse_prefix(src, fallback_drive, &drive, &name) != 0) {
    return -1;
  }

  if (name != src) {
    n = (unsigned short)strlen(src);
    if (n + 1u > max) {
      return -1;
    }
    memcpy(out, src, n + 1u);
    return 0;
  }

  if (drive == 8u) {
    n = (unsigned short)strlen(src);
    if (n + 1u > max) {
      return -1;
    }
    memcpy(out, src, n + 1u);
    return 0;
  }

  n = (unsigned short)strlen(src);
  if ((unsigned long)n + 4ul > (unsigned long)max) {
    return -1;
  }
  if (drive >= 10u) {
    out[0] = '1';
    out[1] = (char)('0' + (drive - 10u));
    out[2] = ':';
    memcpy(out + 3u, src, n + 1u);
  } else {
    out[0] = (char)('0' + drive);
    out[1] = ':';
    memcpy(out + 2u, src, n + 1u);
  }
  return 0;
}

#endif
