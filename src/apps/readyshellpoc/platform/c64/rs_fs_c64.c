#include "../rs_platform.h"

#include <cbm.h>
#include <string.h>

static const char* rs_fs_name_with_mode(const char* path, char mode, char* out, unsigned short max) {
  unsigned short n;
  unsigned short i;
  int has_meta;

  if (!path || !out || max < 8u) {
    return 0;
  }

  has_meta = 0;
  n = (unsigned short)strlen(path);
  for (i = 0; i < n; ++i) {
    if (path[i] == ':' || path[i] == ',') {
      has_meta = 1;
      break;
    }
  }

  if (has_meta) {
    if (n + 1u > max) {
      return 0;
    }
    memcpy(out, path, n + 1u);
    return out;
  }

  if ((unsigned long)n + 8ul > (unsigned long)max) {
    return 0;
  }

  out[0] = '0';
  out[1] = ':';
  memcpy(out + 2u, path, n);
  out[2u + n] = ',';
  out[3u + n] = 's';
  out[4u + n] = ',';
  out[5u + n] = mode;
  out[6u + n] = '\0';
  return out;
}

int rs_file_read_all(const char* path, unsigned char* dst, unsigned short max,
                     unsigned short* out_len) {
  char namebuf[96];
  const char* name;
  int n;
  unsigned short total;

  if (!path || !dst || !out_len) {
    return -1;
  }

  name = rs_fs_name_with_mode(path, 'r', namebuf, sizeof(namebuf));
  if (!name) {
    return -1;
  }

  if (cbm_open(2, 8, CBM_READ, name) != 0) {
    cbm_k_clrch();
    return -1;
  }

  total = 0;
  while (total < max) {
    n = cbm_read(2, dst + total, (unsigned int)(max - total));
    if (n < 0) {
      cbm_close(2);
      cbm_k_clrch();
      return -1;
    }
    if (n == 0) {
      break;
    }
    total = (unsigned short)(total + (unsigned short)n);
  }

  cbm_close(2);
  cbm_k_clrch();
  *out_len = total;
  return 0;
}

int rs_file_write_all(const char* path, const unsigned char* src, unsigned short len) {
  char namebuf[96];
  const char* name;
  int n;
  unsigned short written;

  if (!path || (!src && len != 0)) {
    return -1;
  }

  name = rs_fs_name_with_mode(path, 'w', namebuf, sizeof(namebuf));
  if (!name) {
    return -1;
  }

  if (cbm_open(2, 8, CBM_WRITE, name) != 0) {
    cbm_k_clrch();
    return -1;
  }

  written = 0;
  while (written < len) {
    n = cbm_write(2, src + written, (unsigned int)(len - written));
    if (n < 0) {
      cbm_close(2);
      cbm_k_clrch();
      return -1;
    }
    if (n == 0) {
      break;
    }
    written = (unsigned short)(written + (unsigned short)n);
  }

  cbm_close(2);
  cbm_k_clrch();
  return written == len ? 0 : -1;
}
