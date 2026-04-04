#ifndef RS_SCRIPT_CTL_C64_H
#define RS_SCRIPT_CTL_C64_H

#define RS_SCRIPT_CTL_BUF_MAX 384u
#define RS_SCRIPT_CMD_MAX 159u

typedef enum RSScriptMode {
  RS_SCRIPT_MODE_NONE = 0,
  RS_SCRIPT_MODE_INTERACTIVE = 1,
  RS_SCRIPT_MODE_SCRIPT = 2
} RSScriptMode;

typedef enum RSScriptStepKind {
  RS_SCRIPT_STEP_END = 0,
  RS_SCRIPT_STEP_WAIT = 1,
  RS_SCRIPT_STEP_COMMAND = 2,
  RS_SCRIPT_STEP_PARSE_ERROR = 3
} RSScriptStepKind;

typedef struct RSScriptStep {
  RSScriptStepKind kind;
  unsigned short line_no;
  unsigned short wait_ticks60;
  char command[RS_SCRIPT_CMD_MAX + 1u];
} RSScriptStep;

typedef struct RSScriptCtl {
  int loaded;
  RSScriptMode mode;
  unsigned short len;
  unsigned short pos;
  unsigned short line_no;
  unsigned char text[RS_SCRIPT_CTL_BUF_MAX + 1u];
} RSScriptCtl;

void rs_script_ctl_init(RSScriptCtl* ctl);
int rs_script_ctl_load(RSScriptCtl* ctl, const char* path);
RSScriptMode rs_script_ctl_mode(const RSScriptCtl* ctl);
int rs_script_ctl_next(RSScriptCtl* ctl, RSScriptStep* step);
void rs_script_wait_ticks60(unsigned short ticks60);

#endif
