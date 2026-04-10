#include "rs_cmd_overlay.h"

#include "rs_cmd_value_local.h"

#include <cbm.h>

#if defined(__CC65__)
#pragma code-name(push, "OVERLAY4")
#pragma rodata-name(push, "OVERLAY4")
#pragma bss-name(push, "OVERLAY4")
#endif

static int drvi_make_object(unsigned char drive, RSValue* out) {
  RSValue vdrive;
  RSValue vdisk;
  RSValue vid;
  RSValue vfree;
  RSValue vtype;
  unsigned short free_blocks;
  unsigned char st;
  struct cbm_dirent ent;

  if (!out) {
    return -1;
  }

  free_blocks = 0u;
  if (cbm_opendir(1, drive) == 0) {
    for (;;) {
      st = cbm_readdir(1, &ent);
      if (st != 0u) {
        if (st == 2u) {
          free_blocks = ent.size;
        }
        break;
      }
    }
    cbm_closedir(1);
  }

  rs_cmd_value_free(out);
  if (rs_cmd_value_object_new(out) != 0) {
    return -1;
  }

  rs_cmd_value_init_u16(&vdrive, drive);
  rs_cmd_value_init_u16(&vfree, free_blocks);
  rs_cmd_value_init_false(&vdisk);
  rs_cmd_value_init_false(&vid);
  rs_cmd_value_init_false(&vtype);

  if (rs_cmd_value_init_string(&vdisk, "DISK") != 0 ||
      rs_cmd_value_init_string(&vid, "") != 0 ||
      rs_cmd_value_init_string(&vtype, "1541") != 0) {
    rs_cmd_value_free(out);
    return -1;
  }

  if (rs_cmd_object_set(out, "DRIVE", &vdrive) != 0 ||
      rs_cmd_object_set(out, "DISKNAME", &vdisk) != 0 ||
      rs_cmd_object_set(out, "ID", &vid) != 0 ||
      rs_cmd_object_set(out, "BLOCKSFREE", &vfree) != 0 ||
      rs_cmd_object_set(out, "TYPE", &vtype) != 0) {
    rs_cmd_value_free(&vdisk);
    rs_cmd_value_free(&vid);
    rs_cmd_value_free(&vtype);
    rs_cmd_value_free(out);
    return -1;
  }

  rs_cmd_value_free(&vdisk);
  rs_cmd_value_free(&vid);
  rs_cmd_value_free(&vtype);
  return 0;
}

int rs_vmovl_cmd_drvi(RSCommandFrame* frame) {
  unsigned short drive16;
  unsigned char drive;

  if (!frame || !frame->out) {
    return -1;
  }

  if (frame->arg_count == 0u) {
    drive = 8u;
  } else if (rs_cmd_value_to_u16(&frame->args[0], &drive16) == 0 && drive16 <= 255u) {
    drive = (unsigned char)drive16;
  } else {
    return -2;
  }

  frame->drive = drive;
  return drvi_make_object(drive, frame->out);
}

#if defined(__CC65__)
#pragma bss-name(pop)
#pragma rodata-name(pop)
#pragma code-name(pop)
#endif
