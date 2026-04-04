#include "rs_script_ctl_c64.h"

#include "../rs_platform.h"

#include <string.h>
#include <time.h>

static int rs_is_ws(char c) {
  return c == ' ' || c == '\t';
}

static int rs_is_digit(char c) {
  return c >= '0' && c <= '9';
}

static void rs_copy(char* out, unsigned short max, const char* in) {
  unsigned short i;
  if (!out || max == 0u) {
    return;
  }
  i = 0u;
  while (in && in[i] != '\0' && i + 1u < max) {
    out[i] = in[i];
    ++i;
  }
  out[i] = '\0';
}

static int rs_next_line(RSScriptCtl* ctl, char* out, unsigned short max, unsigned short* out_line_no) {
  unsigned short n;
  unsigned short line_no;
  unsigned short i;
  char* p;

  if (!ctl || !out || max == 0u || !out_line_no) {
    return -1;
  }

  for (;;) {
    if (ctl->pos >= ctl->len) {
      out[0] = '\0';
      return 0;
    }

    n = 0u;
    line_no = (unsigned short)(ctl->line_no + 1u);
    while (ctl->pos < ctl->len) {
      i = ctl->pos;
      if (ctl->text[i] == '\r' || ctl->text[i] == '\n') {
        break;
      }
      if (n + 1u < max) {
        out[n++] = (char)ctl->text[i];
      }
      ctl->pos = (unsigned short)(ctl->pos + 1u);
    }
    out[n] = '\0';

    while (ctl->pos < ctl->len && (ctl->text[ctl->pos] == '\r' || ctl->text[ctl->pos] == '\n')) {
      ctl->pos = (unsigned short)(ctl->pos + 1u);
    }
    ctl->line_no = line_no;

    p = out;
    while (rs_is_ws(*p)) {
      ++p;
    }
    if (*p == '\0' || *p == '#') {
      continue;
    }

    rs_copy(out, max, p);
    n = (unsigned short)strlen(out);
    while (n > 0u && rs_is_ws(out[n - 1u])) {
      out[n - 1u] = '\0';
      --n;
    }

    if (out[0] == '\0' || out[0] == '#') {
      continue;
    }

    *out_line_no = line_no;
    return 1;
  }
}

void rs_script_ctl_init(RSScriptCtl* ctl) {
  if (!ctl) {
    return;
  }
  memset(ctl, 0, sizeof(*ctl));
}

int rs_script_ctl_load(RSScriptCtl* ctl, const char* path) {
  unsigned short len;
  char line[40];
  unsigned short line_no;

  if (!ctl || !path || path[0] == '\0') {
    return -1;
  }

  rs_script_ctl_init(ctl);
  if (rs_file_read_all(path, ctl->text, RS_SCRIPT_CTL_BUF_MAX, &len) != 0) {
    return -1;
  }

  ctl->len = len;
  ctl->text[len] = '\0';
  ctl->loaded = 1;
  ctl->mode = RS_SCRIPT_MODE_INTERACTIVE;

  if (rs_next_line(ctl, line, sizeof(line), &line_no) != 1) {
    return 0;
  }

  if ((line[0] == 'M' || line[0] == 'm') && rs_is_ws(line[1])) {
    if (line[2] == 'S' || line[2] == 's') {
      ctl->mode = RS_SCRIPT_MODE_SCRIPT;
      return 0;
    }
    if (line[2] == 'I' || line[2] == 'i') {
      ctl->mode = RS_SCRIPT_MODE_INTERACTIVE;
      return 0;
    }
  }

  ctl->mode = RS_SCRIPT_MODE_INTERACTIVE;
  return 1;
}

RSScriptMode rs_script_ctl_mode(const RSScriptCtl* ctl) {
  if (!ctl || !ctl->loaded) {
    return RS_SCRIPT_MODE_NONE;
  }
  return ctl->mode;
}

int rs_script_ctl_next(RSScriptCtl* ctl, RSScriptStep* step) {
  char line[RS_SCRIPT_CMD_MAX + 1u];
  unsigned short line_no;
  char* p;
  unsigned long ticks;

  if (!ctl || !step) {
    return -1;
  }

  memset(step, 0, sizeof(*step));
  if (!ctl->loaded) {
    step->kind = RS_SCRIPT_STEP_END;
    return 0;
  }

  if (rs_next_line(ctl, line, sizeof(line), &line_no) != 1) {
    step->kind = RS_SCRIPT_STEP_END;
    return 0;
  }

  step->line_no = line_no;

  if (line[0] == 'W' || line[0] == 'w') {
    if (!rs_is_ws(line[1])) {
      step->kind = RS_SCRIPT_STEP_PARSE_ERROR;
      return 0;
    }
    p = line + 2;
    while (rs_is_ws(*p)) {
      ++p;
    }
    if (!rs_is_digit(*p)) {
      step->kind = RS_SCRIPT_STEP_PARSE_ERROR;
      return 0;
    }
    ticks = 0ul;
    while (rs_is_digit(*p)) {
      ticks = ticks * 10ul + (unsigned long)(*p - '0');
      if (ticks > 65535ul) {
        step->kind = RS_SCRIPT_STEP_PARSE_ERROR;
        return 0;
      }
      ++p;
    }
    while (rs_is_ws(*p)) {
      ++p;
    }
    if (*p != '\0') {
      step->kind = RS_SCRIPT_STEP_PARSE_ERROR;
      return 0;
    }
    step->kind = RS_SCRIPT_STEP_WAIT;
    step->wait_ticks60 = (unsigned short)ticks;
    return 0;
  }

  if (line[0] == 'C' || line[0] == 'c') {
    if (!rs_is_ws(line[1])) {
      step->kind = RS_SCRIPT_STEP_PARSE_ERROR;
      return 0;
    }
    p = line + 2;
    while (rs_is_ws(*p)) {
      ++p;
    }
    if (*p == '\0') {
      step->kind = RS_SCRIPT_STEP_PARSE_ERROR;
      return 0;
    }
    step->kind = RS_SCRIPT_STEP_COMMAND;
    rs_copy(step->command, sizeof(step->command), p);
    return 0;
  }

  step->kind = RS_SCRIPT_STEP_PARSE_ERROR;
  return 0;
}

void rs_script_wait_ticks60(unsigned short ticks60) {
  clock_t start;
  clock_t delta;
  unsigned long ticks;
  unsigned long cps;

  if (ticks60 == 0u) {
    return;
  }

  cps = (unsigned long)CLOCKS_PER_SEC;
  if (cps == 0ul) {
    return;
  }

  ticks = ((unsigned long)ticks60 * cps + 59ul) / 60ul;
  if (ticks == 0ul) {
    ticks = 1ul;
  }

  start = clock();
  delta = (clock_t)ticks;
  while ((clock_t)(clock() - start) < delta) {
    /* deterministic delay */
  }
}
