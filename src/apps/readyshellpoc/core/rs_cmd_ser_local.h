#ifndef RS_CMD_SER_LOCAL_H
#define RS_CMD_SER_LOCAL_H

#include "rs_cmd_overlay.h"
#include "rs_cmd_value_local.h"
#include "../platform/rs_platform.h"

static unsigned char rs_cmd_ser_buf[128];

static int rs_cmd_reu_put(unsigned long base, unsigned short* pos, unsigned short max, unsigned char b) {
  if (!pos || *pos >= max) {
    return -1;
  }
  if (rs_reu_write(base + (unsigned long)*pos, &b, 1u) != 0) {
    return -1;
  }
  *pos = (unsigned short)(*pos + 1u);
  return 0;
}

static int rs_cmd_reu_get(unsigned long base, unsigned short* pos, unsigned short max, unsigned char* out) {
  if (!pos || !out || *pos >= max) {
    return -1;
  }
  if (rs_reu_read(base + (unsigned long)*pos, out, 1u) != 0) {
    return -1;
  }
  *pos = (unsigned short)(*pos + 1u);
  return 0;
}

static int rs_cmd_reu_put_u16(unsigned long base,
                              unsigned short* pos,
                              unsigned short max,
                              unsigned short v) {
  if (rs_cmd_reu_put(base, pos, max, (unsigned char)(v & 0xFFu)) != 0) {
    return -1;
  }
  return rs_cmd_reu_put(base, pos, max, (unsigned char)((v >> 8u) & 0xFFu));
}

static int rs_cmd_reu_get_u16(unsigned long base,
                              unsigned short* pos,
                              unsigned short max,
                              unsigned short* out) {
  unsigned char lo;
  unsigned char hi;
  if (rs_cmd_reu_get(base, pos, max, &lo) != 0 ||
      rs_cmd_reu_get(base, pos, max, &hi) != 0) {
    return -1;
  }
  *out = (unsigned short)(lo | ((unsigned short)hi << 8u));
  return 0;
}

static int rs_cmd_skip_value_from_reu(unsigned long base,
                                      unsigned short* pos,
                                      unsigned short max) {
  unsigned char tag;
  unsigned short n;
  unsigned short i;
  if (!pos || rs_cmd_reu_get(base, pos, max, &tag) != 0) {
    return -1;
  }
  if (tag == RS_VAL_FALSE || tag == RS_VAL_TRUE) {
    return 0;
  }
  if (tag == RS_VAL_U16) {
    if ((unsigned short)(*pos + 2u) > max) {
      return -1;
    }
    *pos = (unsigned short)(*pos + 2u);
    return 0;
  }
  if (tag == RS_VAL_STR) {
    unsigned char len;
    if (rs_cmd_reu_get(base, pos, max, &len) != 0 ||
        (unsigned short)(*pos + (unsigned short)len) > max) {
      return -1;
    }
    *pos = (unsigned short)(*pos + (unsigned short)len);
    return 0;
  }
  if (tag == RS_VAL_ARRAY) {
    if (rs_cmd_reu_get_u16(base, pos, max, &n) != 0) {
      return -1;
    }
    for (i = 0u; i < n; ++i) {
      if (rs_cmd_skip_value_from_reu(base, pos, max) != 0) {
        return -1;
      }
    }
    return 0;
  }
  if (tag == RS_VAL_OBJECT) {
    unsigned char count;
    if (rs_cmd_reu_get(base, pos, max, &count) != 0) {
      return -1;
    }
    for (i = 0u; i < (unsigned short)count; ++i) {
      if (rs_cmd_skip_value_from_reu(base, pos, max) != 0 ||
          rs_cmd_skip_value_from_reu(base, pos, max) != 0) {
        return -1;
      }
    }
    return 0;
  }
  return -1;
}

static int rs_cmd_ser_value_to_reu(const RSValue* value,
                                   unsigned long base,
                                   unsigned short* pos,
                                   unsigned short max) {
  unsigned short i;
  unsigned short n;
  RSValue name_val;
  if (!value || !pos) {
    return -1;
  }
  if (rs_cmd_reu_put(base, pos, max, (unsigned char)value->tag) != 0) {
    return -1;
  }
  if (value->tag == RS_VAL_FALSE || value->tag == RS_VAL_TRUE) {
    return 0;
  }
  if (value->tag == RS_VAL_U16) {
    return rs_cmd_reu_put_u16(base, pos, max, value->as.u16);
  }
  if (value->tag == RS_VAL_STR) {
    n = value->as.str.len;
    if (rs_cmd_reu_put(base, pos, max, (unsigned char)n) != 0) {
      return -1;
    }
    for (i = 0u; i < n; ++i) {
      if (rs_cmd_reu_put(base, pos, max, (unsigned char)value->as.str.bytes[i]) != 0) {
        return -1;
      }
    }
    return 0;
  }
  if (value->tag == RS_VAL_ARRAY) {
    if (rs_cmd_reu_put_u16(base, pos, max, value->as.array.count) != 0) {
      return -1;
    }
    for (i = 0u; i < value->as.array.count; ++i) {
      if (rs_cmd_ser_value_to_reu(&value->as.array.items[i], base, pos, max) != 0) {
        return -1;
      }
    }
    return 0;
  }
  if (value->tag == RS_VAL_OBJECT) {
    if (rs_cmd_reu_put(base, pos, max, value->as.object.count) != 0) {
      return -1;
    }
    for (i = 0u; i < value->as.object.count; ++i) {
      rs_cmd_value_init_false(&name_val);
      if (rs_cmd_value_init_string(&name_val, value->as.object.props[i].name) != 0) {
        return -1;
      }
      if (rs_cmd_ser_value_to_reu(&name_val, base, pos, max) != 0) {
        rs_cmd_value_free(&name_val);
        return -1;
      }
      rs_cmd_value_free(&name_val);
      if (rs_cmd_ser_value_to_reu(value->as.object.props[i].value, base, pos, max) != 0) {
        return -1;
      }
    }
    return 0;
  }
  return -1;
}

static int rs_cmd_deser_value_from_reu(unsigned long base,
                                       unsigned short* pos,
                                       unsigned short max,
                                       RSValue* out) {
  unsigned char tag;
  unsigned short n;
  unsigned short i;
  RSValue name_val;
  RSValue tmp;
  char* s;

  if (!pos || !out || rs_cmd_reu_get(base, pos, max, &tag) != 0) {
    return -1;
  }

  rs_cmd_value_free(out);
  if (tag == RS_VAL_FALSE) {
    rs_cmd_value_init_false(out);
    return 0;
  }
  if (tag == RS_VAL_TRUE) {
    out->tag = RS_VAL_TRUE;
    out->as.u16 = 1u;
    return 0;
  }
  if (tag == RS_VAL_U16) {
    if (rs_cmd_reu_get_u16(base, pos, max, &n) != 0) {
      return -1;
    }
    rs_cmd_value_init_u16(out, n);
    return 0;
  }
  if (tag == RS_VAL_STR) {
    unsigned char len;
    if (rs_cmd_reu_get(base, pos, max, &len) != 0) {
      return -1;
    }
    s = (char*)malloc((size_t)len + 1u);
    if (!s) {
      return -1;
    }
    for (i = 0u; i < (unsigned short)len; ++i) {
      unsigned char ch;
      if (rs_cmd_reu_get(base, pos, max, &ch) != 0) {
        free(s);
        return -1;
      }
      s[i] = (char)ch;
    }
    s[len] = '\0';
    if (rs_cmd_value_init_string(out, s) != 0) {
      free(s);
      return -1;
    }
    free(s);
    return 0;
  }
  if (tag == RS_VAL_ARRAY) {
    if (rs_cmd_reu_get_u16(base, pos, max, &n) != 0) {
      return -1;
    }
    if (rs_cmd_value_array_new(out, n) != 0) {
      return -1;
    }
    for (i = 0u; i < n; ++i) {
      if (rs_cmd_deser_value_from_reu(base, pos, max, &out->as.array.items[i]) != 0) {
        return -1;
      }
    }
    return 0;
  }
  if (tag == RS_VAL_OBJECT) {
    unsigned char count;
    if (rs_cmd_reu_get(base, pos, max, &count) != 0) {
      return -1;
    }
    if (rs_cmd_value_object_new(out) != 0) {
      return -1;
    }
    for (i = 0u; i < (unsigned short)count; ++i) {
      rs_cmd_value_init_false(&name_val);
      rs_cmd_value_init_false(&tmp);
      if (rs_cmd_deser_value_from_reu(base, pos, max, &name_val) != 0 ||
          name_val.tag != RS_VAL_STR ||
          rs_cmd_deser_value_from_reu(base, pos, max, &tmp) != 0 ||
          rs_cmd_object_set(out, name_val.as.str.bytes, &tmp) != 0) {
        rs_cmd_value_free(&name_val);
        rs_cmd_value_free(&tmp);
        return -1;
      }
      rs_cmd_value_free(&name_val);
      rs_cmd_value_free(&tmp);
    }
    return 0;
  }
  return -1;
}

#endif
