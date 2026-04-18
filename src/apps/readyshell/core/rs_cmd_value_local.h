#ifndef RS_CMD_VALUE_LOCAL_H
#define RS_CMD_VALUE_LOCAL_H

#include "rs_value.h"

#include <stdlib.h>
#include <string.h>

static void rs_cmd_value_init_false(RSValue* v) {
  if (v) {
    v->tag = RS_VAL_FALSE;
    v->as.u16 = 0u;
  }
}

static void rs_cmd_value_init_bool(RSValue* v, int truthy) {
  if (v) {
    v->tag = truthy ? RS_VAL_TRUE : RS_VAL_FALSE;
    v->as.u16 = truthy ? 1u : 0u;
  }
}

static void rs_cmd_value_init_u16(RSValue* v, unsigned short n) {
  if (v) {
    v->tag = RS_VAL_U16;
    v->as.u16 = n;
  }
}

static char* rs_cmd_strdup(const char* s) {
  unsigned short n;
  char* p;
  if (!s) {
    s = "";
  }
  n = (unsigned short)strlen(s);
  p = (char*)malloc((size_t)n + 1u);
  if (!p) {
    return 0;
  }
  memcpy(p, s, (size_t)n + 1u);
  return p;
}

static int rs_cmd_value_init_string(RSValue* v, const char* s) {
  unsigned short n;
  char* p;
  if (!v) {
    return -1;
  }
  if (!s) {
    s = "";
  }
  n = (unsigned short)strlen(s);
  if (n > 255u) {
    return -1;
  }
  p = rs_cmd_strdup(s);
  if (!p) {
    return -1;
  }
  v->tag = RS_VAL_STR;
  v->as.str.len = (unsigned char)n;
  v->as.str.bytes = p;
  return 0;
}

static void rs_cmd_value_free(RSValue* v) {
  unsigned short i;
  if (!v) {
    return;
  }
  if (v->tag == RS_VAL_STR) {
    free(v->as.str.bytes);
  } else if (v->tag == RS_VAL_ARRAY) {
    for (i = 0u; i < v->as.array.count; ++i) {
      rs_cmd_value_free(&v->as.array.items[i]);
    }
    free(v->as.array.items);
  } else if (v->tag == RS_VAL_OBJECT) {
    for (i = 0u; i < v->as.object.count; ++i) {
      free(v->as.object.props[i].name);
      if (v->as.object.props[i].value) {
        rs_cmd_value_free(v->as.object.props[i].value);
        free(v->as.object.props[i].value);
      }
    }
    free(v->as.object.props);
  }
  rs_cmd_value_init_false(v);
}

static int rs_cmd_value_array_new(RSValue* v, unsigned short count) {
  unsigned short i;
  RSValue* items;
  if (!v) {
    return -1;
  }
  items = 0;
  if (count) {
    items = (RSValue*)malloc(sizeof(RSValue) * count);
    if (!items) {
      return -1;
    }
  }
  v->tag = RS_VAL_ARRAY;
  v->as.array.count = count;
  v->as.array.items = items;
  for (i = 0u; i < count; ++i) {
    rs_cmd_value_init_false(&items[i]);
  }
  return 0;
}

static int rs_cmd_value_object_new(RSValue* v) {
  if (!v) {
    return -1;
  }
  v->tag = RS_VAL_OBJECT;
  v->as.object.count = 0u;
  v->as.object.props = 0;
  return 0;
}

static int rs_cmd_value_clone(RSValue* out, const RSValue* in);

static int rs_cmd_object_set(RSValue* obj, const char* name, const RSValue* value) {
  RSObjectProp* props;
  RSValue* pv;
  char* nm;
  unsigned short i;
  if (!obj || obj->tag != RS_VAL_OBJECT || !name || !value || obj->as.object.count == 255u) {
    return -1;
  }
  props = (RSObjectProp*)malloc(sizeof(RSObjectProp) * (obj->as.object.count + 1u));
  if (!props) {
    return -1;
  }
  for (i = 0u; i < obj->as.object.count; ++i) {
    props[i] = obj->as.object.props[i];
  }
  nm = rs_cmd_strdup(name);
  if (!nm) {
    free(props);
    return -1;
  }
  pv = (RSValue*)malloc(sizeof(RSValue));
  if (!pv) {
    free(nm);
    free(props);
    return -1;
  }
  rs_cmd_value_init_false(pv);
  if (rs_cmd_value_clone(pv, value) != 0) {
    free(pv);
    free(nm);
    free(props);
    return -1;
  }
  props[obj->as.object.count].name = nm;
  props[obj->as.object.count].value = pv;
  free(obj->as.object.props);
  obj->as.object.props = props;
  obj->as.object.count = (unsigned char)(obj->as.object.count + 1u);
  return 0;
}

static int rs_cmd_value_clone(RSValue* out, const RSValue* in) {
  unsigned short i;
  if (!out || !in) {
    return -1;
  }
  rs_cmd_value_free(out);
  if (in->tag == RS_VAL_FALSE) {
    rs_cmd_value_init_false(out);
    return 0;
  }
  if (in->tag == RS_VAL_TRUE) {
    out->tag = RS_VAL_TRUE;
    out->as.u16 = 1u;
    return 0;
  }
  if (in->tag == RS_VAL_U16) {
    rs_cmd_value_init_u16(out, in->as.u16);
    return 0;
  }
  if (in->tag == RS_VAL_STR) {
    return rs_cmd_value_init_string(out, in->as.str.bytes);
  }
  if (in->tag == RS_VAL_ARRAY) {
    if (rs_cmd_value_array_new(out, in->as.array.count) != 0) {
      return -1;
    }
    for (i = 0u; i < in->as.array.count; ++i) {
      if (rs_cmd_value_clone(&out->as.array.items[i], &in->as.array.items[i]) != 0) {
        return -1;
      }
    }
    return 0;
  }
  if (in->tag == RS_VAL_OBJECT) {
    if (rs_cmd_value_object_new(out) != 0) {
      return -1;
    }
    for (i = 0u; i < in->as.object.count; ++i) {
      if (rs_cmd_object_set(out,
                            in->as.object.props[i].name,
                            in->as.object.props[i].value) != 0) {
        return -1;
      }
    }
    return 0;
  }
  return -1;
}

static int rs_cmd_value_to_u16(const RSValue* v, unsigned short* out) {
  if (!v || !out) {
    return -1;
  }
  if (v->tag == RS_VAL_U16) {
    *out = v->as.u16;
    return 0;
  }
  if (v->tag == RS_VAL_TRUE) {
    *out = 1u;
    return 0;
  }
  if (v->tag == RS_VAL_FALSE) {
    *out = 0u;
    return 0;
  }
  return -1;
}

static const char* rs_cmd_value_cstr(const RSValue* v) {
  if (!v || v->tag != RS_VAL_STR) {
    return 0;
  }
  return v->as.str.bytes;
}

#endif
