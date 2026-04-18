#ifndef RS_CMD_DIR_LOCAL_H
#define RS_CMD_DIR_LOCAL_H

#include <cbm.h>

#define RS_CMD_DIR_LFN 1u
#define RS_CMD_DIR_CMD_LFN 15u

static void rs_cmd_dir_cleanup_io(void) {
  cbm_k_clrch();
  cbm_k_clall();
}

static void rs_cmd_dir_drain_status(unsigned char drive) {
  rs_cmd_dir_cleanup_io();
  if (cbm_open(RS_CMD_DIR_CMD_LFN, drive, 15, "") != 0) {
    rs_cmd_dir_cleanup_io();
    return;
  }
  cbm_close(RS_CMD_DIR_CMD_LFN);
  rs_cmd_dir_cleanup_io();
}

static void rs_cmd_dir_initialize(unsigned char drive) {
  rs_cmd_dir_cleanup_io();
  if (cbm_open(RS_CMD_DIR_CMD_LFN, drive, 15, "I0") != 0) {
    rs_cmd_dir_cleanup_io();
    return;
  }
  cbm_close(RS_CMD_DIR_CMD_LFN);
  rs_cmd_dir_cleanup_io();
}

static void rs_cmd_dir_prepare(unsigned char drive,
                               unsigned char mode) {
  if (mode == 0u) {
    return;
  }
  rs_cmd_dir_cleanup_io();
  if (mode >= 2u) {
    rs_cmd_dir_drain_status(drive);
  }
  if (mode >= 3u) {
    rs_cmd_dir_initialize(drive);
  }
  rs_cmd_dir_cleanup_io();
}

static int rs_cmd_dir_open_header(unsigned char drive,
                                  struct cbm_dirent* ent) {
  unsigned char mode;
  unsigned char st;

  if (!ent) {
    return -1;
  }

  for (mode = 0u; mode < 4u; ++mode) {
    rs_cmd_dir_prepare(drive, mode);
    if (cbm_opendir(RS_CMD_DIR_LFN, drive) != 0) {
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

#endif
