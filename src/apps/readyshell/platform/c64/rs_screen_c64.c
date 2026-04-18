#include "../rs_platform.h"

#include <cbm.h>
#include <c64.h>
#include <conio.h>

#define RS_SCREEN_BASE ((unsigned char*)0x0400)

static void rs_scroll_up_one(void) {
  unsigned char w;
  unsigned char h;
  unsigned int row;
  unsigned int col;
  unsigned int from;
  unsigned int to;
  unsigned int last_row;
  unsigned char color;

  screensize(&w, &h);
  color = COLOR_WHITE;

  if (w == 0 || h < 2) {
    return;
  }

  for (row = 1u; row < (unsigned int)h; ++row) {
    from = row * (unsigned int)w;
    to = (row - 1u) * (unsigned int)w;
    for (col = 0u; col < (unsigned int)w; ++col) {
      RS_SCREEN_BASE[to + col] = RS_SCREEN_BASE[from + col];
      COLOR_RAM[to + col] = COLOR_RAM[from + col];
    }
  }

  last_row = ((unsigned int)h - 1u) * (unsigned int)w;
  for (col = 0u; col < (unsigned int)w; ++col) {
    RS_SCREEN_BASE[last_row + col] = ' ';
    COLOR_RAM[last_row + col] = color;
  }
}

void rs_putc(char c) {
  cputc((unsigned char)c);
}

void rs_puts(const char* s) {
  char* p;
  if (s) {
    p = (char*)s;
  } else {
    p = "";
  }
  while (*p) {
    cputc((unsigned char)*p);
    ++p;
  }
}

void rs_newline(void) {
  unsigned char w;
  unsigned char h;
  unsigned char y;

  screensize(&w, &h);
  y = wherey();

  if (y + 1u >= h) {
    rs_scroll_up_one();
    gotoxy(0, (unsigned char)(h - 1u));
  } else {
    gotoxy(0, (unsigned char)(y + 1u));
  }
}

void rs_prompt(void) {
  textcolor(COLOR_YELLOW);
  cputs("RS> ");
  textcolor(COLOR_WHITE);
}
