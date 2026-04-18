#include "rs_token.h"

int rs_ci_char(int c) {
  if (c >= 'a' && c <= 'z') {
    return c - ('a' - 'A');
  }
  return c;
}

int rs_ci_equal(const char* a, const char* b) {
  int ca;
  int cb;
  if (!a || !b) {
    return 0;
  }
  while (*a && *b) {
    ca = rs_ci_char((unsigned char)*a);
    cb = rs_ci_char((unsigned char)*b);
    if (ca != cb) {
      return 0;
    }
    ++a;
    ++b;
  }
  return *a == '\0' && *b == '\0';
}

void rs_upper_copy(char* dst, const char* src, unsigned short max) {
  unsigned short i;
  if (!dst || max == 0) {
    return;
  }
  if (!src) {
    dst[0] = '\0';
    return;
  }
  for (i = 0; i + 1 < max && src[i] != '\0'; ++i) {
    dst[i] = (char)rs_ci_char((unsigned char)src[i]);
  }
  dst[i] = '\0';
}
