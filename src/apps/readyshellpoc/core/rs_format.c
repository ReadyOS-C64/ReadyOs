#include "rs_format.h"

#include <string.h>

static int rs_append(char* out, unsigned short max, unsigned short* len, const char* s) {
  size_t n;
  if (!s) {
    s = "";
  }
  n = strlen(s);
  if ((unsigned long)*len + (unsigned long)n + 1ul > (unsigned long)max) {
    return -1;
  }
  memcpy(out + *len, s, n);
  *len = (unsigned short)(*len + (unsigned short)n);
  out[*len] = '\0';
  return 0;
}

static int rs_format_rec(const RSValue* v, char* out, unsigned short max, unsigned short* len) {
  char buf[32];
  char rev[16];
  unsigned short i;
  unsigned short n;
  unsigned short j;
  unsigned short k;

  if (!v) {
    return rs_append(out, max, len, "<null>");
  }

  if (v->tag == RS_VAL_FALSE) {
    return rs_append(out, max, len, "FALSE");
  }
  if (v->tag == RS_VAL_TRUE) {
    return rs_append(out, max, len, "TRUE");
  }
  if (v->tag == RS_VAL_U16) {
    n = v->as.u16;
    if (n == 0) {
      buf[0] = '0';
      buf[1] = '\0';
    } else {
      j = 0;
      while (n > 0 && j + 1u < sizeof(rev)) {
        rev[j++] = (char)('0' + (n % 10u));
        n = (unsigned short)(n / 10u);
      }
      for (k = 0; k < j; ++k) {
        buf[k] = rev[j - 1u - k];
      }
      buf[j] = '\0';
    }
    return rs_append(out, max, len, buf);
  }
  if (v->tag == RS_VAL_STR) {
    if ((unsigned long)*len + (unsigned long)v->as.str.len + 1ul > (unsigned long)max) {
      return -1;
    }
    memcpy(out + *len, v->as.str.bytes, v->as.str.len);
    *len = (unsigned short)(*len + v->as.str.len);
    out[*len] = '\0';
    return 0;
  }
  if (v->tag == RS_VAL_ARRAY) {
    if (rs_append(out, max, len, "[") != 0) {
      return -1;
    }
    for (i = 0; i < v->as.array.count; ++i) {
      if (i != 0 && rs_append(out, max, len, ",") != 0) {
        return -1;
      }
      if (rs_format_rec(&v->as.array.items[i], out, max, len) != 0) {
        return -1;
      }
    }
    return rs_append(out, max, len, "]");
  }
  if (v->tag == RS_VAL_OBJECT) {
    if (rs_append(out, max, len, "{") != 0) {
      return -1;
    }
    for (i = 0; i < v->as.object.count; ++i) {
      if (i != 0 && rs_append(out, max, len, ",") != 0) {
        return -1;
      }
      if (rs_append(out, max, len, v->as.object.props[i].name) != 0) {
        return -1;
      }
      if (rs_append(out, max, len, ":") != 0) {
        return -1;
      }
      if (rs_format_rec(v->as.object.props[i].value, out, max, len) != 0) {
        return -1;
      }
    }
    return rs_append(out, max, len, "}");
  }

  return rs_append(out, max, len, "<unsupported>");
}

int rs_format_value(const RSValue* v, char* out, unsigned short max) {
  unsigned short len;
  if (!out || max == 0) {
    return -1;
  }
  out[0] = '\0';
  len = 0;
  if (rs_format_rec(v, out, max, &len) != 0) {
    return -1;
  }
  return (int)len;
}
