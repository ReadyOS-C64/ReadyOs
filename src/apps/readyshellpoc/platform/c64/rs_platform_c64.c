#include "../rs_platform.h"

#include "../../core/rs_value.h"
#include "reu_mgr.h"

#include <cbm.h>
#include <conio.h>
#include <stdlib.h>
#include <string.h>

#define RS_REU_PROBE_BANK 0x43u
#define RS_REU_PROBE_OFF  0x0000u

static void rs_name_from_dirent(const char* in, char* out, unsigned short max) {
  unsigned short i;
  unsigned short j;
  char c;
  i = 0;
  j = 0;
  while (i < 16u && in[i] != '\0' && j + 1u < max) {
    c = in[i++];
    if (c == '"') {
      continue;
    }
    out[j++] = c;
  }
  while (j > 0 && out[j - 1u] == ' ') {
    --j;
  }
  out[j] = '\0';
}

static void rs_ext_from_name(const char* name, char* ext, unsigned short max) {
  unsigned short i;
  unsigned short dot;
  unsigned short j;
  i = 0;
  dot = 0xFFFFu;
  while (name[i] != '\0') {
    if (name[i] == '.') {
      dot = i;
    }
    ++i;
  }
  if (dot == 0xFFFFu || name[dot + 1u] == '\0') {
    ext[0] = '\0';
    return;
  }
  j = 0;
  i = (unsigned short)(dot + 1u);
  while (name[i] != '\0' && j + 1u < max) {
    ext[j++] = name[i++];
  }
  ext[j] = '\0';
}

static int rs_array_append_owned(RSValue* arr, RSValue* item) {
  RSValue* next;
  if (!arr || arr->tag != RS_VAL_ARRAY || !item) {
    return -1;
  }
  next = (RSValue*)realloc(arr->as.array.items, sizeof(RSValue) * (arr->as.array.count + 1u));
  if (!next) {
    return -1;
  }
  arr->as.array.items = next;
  arr->as.array.items[arr->as.array.count] = *item;
  arr->as.array.count = (unsigned short)(arr->as.array.count + 1u);
  rs_value_init_false(item);
  return 0;
}

int rs_getline_40(char* out, unsigned max40) {
  unsigned short limit;
  unsigned short len;
  unsigned short cursor_pos;
  unsigned short i;
  unsigned char screen_w;
  unsigned char screen_h;
  unsigned char start_x;
  unsigned char start_y;
  unsigned char ch;
  int insert_mode;

  if (!out || max40 == 0) {
    return -1;
  }

  screensize(&screen_w, &screen_h);
  (void)screen_h;
  start_x = wherex();
  start_y = wherey();

  limit = (unsigned short)max40;
  if (start_x < screen_w) {
    unsigned short avail;
    avail = (unsigned short)(screen_w - start_x);
    if (avail < limit) {
      limit = avail;
    }
  } else {
    limit = 0;
  }
  if (limit == 0) {
    limit = 1;
  }

  len = 0;
  cursor_pos = 0;
  insert_mode = 1;
  out[0] = '\0';

  for (;;) {
    ch = (unsigned char)cgetc();
    if (ch == 0) {
      continue;
    }

    if (ch == CH_STOP) {
      return -1;
    }

    if (ch == CH_ENTER || ch == '\r' || ch == '\n') {
      break;
    }

    if (ch == CH_CURS_LEFT || ch == CH_CURS_UP) {
      if (cursor_pos > 0) {
        cursor_pos--;
      }
      gotoxy((unsigned char)(start_x + cursor_pos), start_y);
      continue;
    }

    if (ch == CH_CURS_RIGHT || ch == CH_CURS_DOWN) {
      if (cursor_pos < len) {
        cursor_pos++;
      }
      gotoxy((unsigned char)(start_x + cursor_pos), start_y);
      continue;
    }

    if (ch == CH_INS) {
      insert_mode = !insert_mode;
      continue;
    }

    if (ch == CH_DEL || ch == '\b' || ch == 127u) {
      if (cursor_pos > 0 && len > 0) {
        unsigned short from;
        cursor_pos--;
        from = cursor_pos;
        while (from < len) {
          out[from] = out[from + 1u];
          ++from;
        }
        len--;
      }
    } else if (ch >= 32 && ch <= 126) {
      if (ch == '!') {
        ch = '|';
      }
      if (len < limit) {
        if (insert_mode && cursor_pos < len) {
          unsigned short from2;
          from2 = len;
          while (from2 > cursor_pos) {
            out[from2] = out[from2 - 1u];
            --from2;
          }
          out[cursor_pos] = (char)ch;
          cursor_pos++;
          len++;
        } else {
          out[cursor_pos] = (char)ch;
          if (cursor_pos == len) {
            len++;
          }
          cursor_pos++;
        }
        out[len] = '\0';
      }
    }

    gotoxy(start_x, start_y);
    for (i = 0; i < len; ++i) {
      rs_putc(out[i]);
    }
    rs_putc(' ');
    gotoxy((unsigned char)(start_x + cursor_pos), start_y);
  }

  out[len] = '\0';

  rs_newline();
  return (int)len;
}

int rs_list_dir(unsigned char drive, struct RSValue* out_array) {
  unsigned char st;
  struct cbm_dirent ent;
  RSValue* arr;
  unsigned short free_blocks;

  if (!out_array) {
    return -1;
  }

  arr = (RSValue*)out_array;
  rs_value_free(arr);
  arr->tag = RS_VAL_ARRAY;
  arr->as.array.count = 0;
  arr->as.array.items = 0;

  if (cbm_opendir(1, drive) != 0) {
    return -1;
  }

  free_blocks = 0;
  for (;;) {
    RSValue obj;
    RSValue vname;
    RSValue vblocks;
    RSValue vext;
    char name[20];
    char ext[8];

    st = cbm_readdir(1, &ent);
    if (st != 0) {
      if (st == 2) {
        free_blocks = ent.size;
      }
      break;
    }

    rs_name_from_dirent(ent.name, name, sizeof(name));
    rs_ext_from_name(name, ext, sizeof(ext));

    rs_value_init_false(&obj);
    rs_value_init_false(&vname);
    rs_value_init_false(&vblocks);
    rs_value_init_false(&vext);

    if (rs_value_object_new(&obj) != 0) {
      cbm_closedir(1);
      return -1;
    }
    if (rs_value_init_string(&vname, name) != 0 ||
        rs_value_init_string(&vext, ext) != 0) {
      rs_value_free(&obj);
      cbm_closedir(1);
      return -1;
    }
    rs_value_init_u16(&vblocks, ent.size);

    if (rs_value_object_set(&obj, "NAME", &vname) != 0 ||
        rs_value_object_set(&obj, "BLOCKS", &vblocks) != 0 ||
        rs_value_object_set(&obj, "EXT", &vext) != 0) {
      rs_value_free(&obj);
      rs_value_free(&vname);
      rs_value_free(&vblocks);
      rs_value_free(&vext);
      cbm_closedir(1);
      return -1;
    }

    rs_value_free(&vname);
    rs_value_free(&vblocks);
    rs_value_free(&vext);

    if (rs_array_append_owned(arr, &obj) != 0) {
      rs_value_free(&obj);
      cbm_closedir(1);
      return -1;
    }
  }

  cbm_closedir(1);
  (void)free_blocks;
  return 0;
}

int rs_drive_info(unsigned char drive, struct RSValue* out_obj) {
  RSValue* obj;
  RSValue vdrive;
  RSValue vdisk;
  RSValue vid;
  RSValue vfree;
  RSValue vtype;
  unsigned short free_blocks;
  unsigned char st;
  struct cbm_dirent ent;

  if (!out_obj) {
    return -1;
  }

  free_blocks = 0;
  if (cbm_opendir(1, drive) == 0) {
    for (;;) {
      st = cbm_readdir(1, &ent);
      if (st != 0) {
        if (st == 2) {
          free_blocks = ent.size;
        }
        break;
      }
    }
    cbm_closedir(1);
  }

  obj = (RSValue*)out_obj;
  rs_value_free(obj);
  if (rs_value_object_new(obj) != 0) {
    return -1;
  }

  rs_value_init_false(&vdrive);
  rs_value_init_false(&vdisk);
  rs_value_init_false(&vid);
  rs_value_init_false(&vfree);
  rs_value_init_false(&vtype);

  rs_value_init_u16(&vdrive, drive);
  rs_value_init_u16(&vfree, free_blocks);
  if (rs_value_init_string(&vdisk, "DISK") != 0 ||
      rs_value_init_string(&vid, "") != 0 ||
      rs_value_init_string(&vtype, "1541") != 0) {
    rs_value_free(obj);
    return -1;
  }

  if (rs_value_object_set(obj, "DRIVE", &vdrive) != 0 ||
      rs_value_object_set(obj, "DISKNAME", &vdisk) != 0 ||
      rs_value_object_set(obj, "ID", &vid) != 0 ||
      rs_value_object_set(obj, "BLOCKSFREE", &vfree) != 0 ||
      rs_value_object_set(obj, "TYPE", &vtype) != 0) {
    rs_value_free(obj);
    rs_value_free(&vdisk);
    rs_value_free(&vid);
    rs_value_free(&vtype);
    return -1;
  }

  rs_value_free(&vdisk);
  rs_value_free(&vid);
  rs_value_free(&vtype);
  return 0;
}

int rs_reu_available(void) {
  unsigned char probe;
  unsigned char check;
  probe = 0xA5u;
  check = 0u;
  reu_dma_stash((unsigned int)&probe, RS_REU_PROBE_BANK, RS_REU_PROBE_OFF, 1u);
  reu_dma_fetch((unsigned int)&check, RS_REU_PROBE_BANK, RS_REU_PROBE_OFF, 1u);
  return check == probe;
}

int rs_reu_read(unsigned long reu_off, void* ram_dst, unsigned short len) {
  unsigned char bank;
  unsigned int off;
  if (!ram_dst || len == 0u) {
    return -1;
  }
  bank = (unsigned char)((reu_off >> 16u) & 0xFFul);
  off = (unsigned int)(reu_off & 0xFFFFul);
  reu_dma_fetch((unsigned int)ram_dst, bank, off, (unsigned int)len);
  return 0;
}

int rs_reu_write(unsigned long reu_off, const void* ram_src, unsigned short len) {
  unsigned char bank;
  unsigned int off;
  if (!ram_src || len == 0u) {
    return -1;
  }
  bank = (unsigned char)((reu_off >> 16u) & 0xFFul);
  off = (unsigned int)(reu_off & 0xFFFFul);
  reu_dma_stash((unsigned int)ram_src, bank, off, (unsigned int)len);
  return 0;
}

void* rs_alloc(unsigned short size) {
  return malloc(size);
}

void rs_free(void* p) {
  free(p);
}

unsigned short rs_mem_avail(void) {
  return 0;
}
