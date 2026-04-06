#include "rs_lexer.h"

#include "rs_token.h"

#include <string.h>

static void rs_set_err(RSError* err,
                       const char* msg,
                       unsigned short offset,
                       unsigned short line,
                       unsigned short col) {
  rs_error_set(err, RS_ERR_LEX, msg, offset, line, col);
}

static int rs_slice_ci_equal(const char* src,
                             unsigned short start,
                             unsigned short len,
                             const char* expect) {
  unsigned short i;

  if (!src || !expect) {
    return 0;
  }

  i = 0;
  while (expect[i] != '\0') {
    if (i >= len) {
      return 0;
    }
    if (rs_ci_char((unsigned char)src[start + i]) != expect[i]) {
      return 0;
    }
    ++i;
  }
  return i == len;
}

static int rs_emit(RSToken* out,
                   unsigned short max,
                   unsigned short* count,
                   RSTokenType type,
                   unsigned short offset) {
  if (*count >= max) {
    return -1;
  }
  out[*count].type = (unsigned char)type;
  out[*count].offset = offset;
  out[*count].value = 0;
  (*count)++;
  return 0;
}

static int rs_ident_char(int ch) {
  return (ch >= 'A' && ch <= 'Z') ||
         (ch >= 'a' && ch <= 'z') ||
         (ch >= '0' && ch <= '9') ||
         ch == '_';
}

static int rs_emit_ident(RSToken* out,
                         unsigned short max,
                         unsigned short* count,
                         const char* src,
                         unsigned short start,
                         unsigned short end,
                         int is_var) {
  unsigned short len;
  RSTokenType type;
  if (*count >= max) {
    return -1;
  }
  len = (unsigned short)(end - start);
  out[*count].offset = start;
  out[*count].value = len;

  if (is_var) {
    type = RS_TOK_VAR;
  } else if (rs_slice_ci_equal(src, start, len, "TRUE")) {
    type = RS_TOK_TRUE;
  } else if (rs_slice_ci_equal(src, start, len, "FALSE")) {
    type = RS_TOK_FALSE;
  } else {
    type = RS_TOK_IDENT;
  }
  out[*count].type = (unsigned char)type;
  (*count)++;
  return 0;
}

static int rs_emit_number(RSToken* out,
                          unsigned short max,
                          unsigned short* count,
                          unsigned short n,
                          unsigned short start) {
  if (*count >= max) {
    return -1;
  }
  out[*count].type = RS_TOK_NUMBER;
  out[*count].offset = start;
  out[*count].value = n;
  (*count)++;
  return 0;
}

static int rs_emit_string(RSToken* out,
                          unsigned short max,
                          unsigned short* count,
                          const char* src,
                          unsigned short* io,
                          unsigned short* line,
                          unsigned short* col,
                          RSError* err) {
  unsigned short i;
  unsigned short p;
  char ch;

  if (*count >= max) {
    return -1;
  }

  i = *io;
  p = *col;
  i++;
  p++;

  out[*count].type = RS_TOK_STRING;
  out[*count].offset = *io;
  out[*count].value = 0;
  while (src[i] != '\0') {
    ch = src[i];
    if (ch == '"') {
      out[*count].value = (unsigned short)(i - (unsigned short)(*io + 1u));
      i++;
      p++;
      *io = i;
      *col = p;
      (*count)++;
      return 0;
    }
    if (ch == '\\') {
      i++;
      p++;
      ch = src[i];
      if (ch == '\0') {
        break;
      }
    }
    i++;
    p++;
  }

  rs_set_err(err, "unterminated string", *io, *line, *col);
  return -1;
}

int rs_lex(const char* src,
           RSToken* out_tokens,
           unsigned short max_tokens,
           unsigned short* out_count,
           RSError* err) {
  unsigned short i;
  unsigned short line;
  unsigned short col;
  unsigned short start;
  unsigned short start_col;
  unsigned short n;
  int ch;

  if (!src || !out_tokens || !out_count || max_tokens == 0) {
    rs_set_err(err, "invalid lexer args", 0, 1, 1);
    return -1;
  }

  rs_error_init(err);

  i = 0;
  line = 1;
  col = 1;
  *out_count = 0;

  while (src[i] != '\0') {
    ch = (unsigned char)src[i];

    if (ch == ' ' || ch == '\t') {
      i++;
      col++;
      continue;
    }

    if (ch == '\r' || ch == '\n') {
      if (rs_emit(out_tokens, max_tokens, out_count, RS_TOK_NEWLINE, i) != 0) {
        rs_set_err(err, "too many tokens", i, line, col);
        return -1;
      }
      if (ch == '\r' && src[i + 1] == '\n') {
        i++;
      }
      i++;
      line++;
      col = 1;
      continue;
    }

    if (ch == '"') {
      if (rs_emit_string(out_tokens, max_tokens, out_count, src, &i, &line, &col, err) != 0) {
        return -1;
      }
      continue;
    }

    if (ch >= '0' && ch <= '9') {
      start = i;
      start_col = col;
      n = 0;
      while (src[i] >= '0' && src[i] <= '9') {
        n = (unsigned short)(n * 10u + (unsigned short)(src[i] - '0'));
        i++;
        col++;
      }
      if (rs_emit_number(out_tokens, max_tokens, out_count, n, start) != 0) {
        rs_set_err(err, "too many tokens", start, line, start_col);
        return -1;
      }
      continue;
    }

    if (ch == '$') {
      start = i + 1u;
      start_col = (unsigned short)(col + 1u);
      i++;
      col++;
      while (rs_ident_char((unsigned char)src[i])) {
        i++;
        col++;
      }
      if (start == i) {
        rs_set_err(err, "expected variable name", i, line, col);
        return -1;
      }
      if (rs_emit_ident(out_tokens,
                        max_tokens,
                        out_count,
                        src,
                        start,
                        i,
                        1) != 0) {
        rs_set_err(err, "too many tokens", start, line, start_col);
        return -1;
      }
      continue;
    }

    if (rs_ident_char(ch)) {
      start = i;
      start_col = col;
      while (rs_ident_char((unsigned char)src[i])) {
        i++;
        col++;
      }
      if (rs_emit_ident(out_tokens,
                        max_tokens,
                        out_count,
                        src,
                        start,
                        i,
                        0) != 0) {
        rs_set_err(err, "too many tokens", start, line, start_col);
        return -1;
      }
      continue;
    }

    if (ch == '.' && src[i + 1] == '.') {
      if (rs_emit(out_tokens, max_tokens, out_count, RS_TOK_RANGE, i) != 0) {
        rs_set_err(err, "too many tokens", i, line, col);
        return -1;
      }
      i += 2;
      col += 2;
      continue;
    }

    if (ch == '=' && src[i + 1] == '=') {
      if (rs_emit(out_tokens, max_tokens, out_count, RS_TOK_EQ, i) != 0) {
        rs_set_err(err, "too many tokens", i, line, col);
        return -1;
      }
      i += 2;
      col += 2;
      continue;
    }

    if (ch == '!' && src[i + 1] == '=') {
      if (rs_emit(out_tokens, max_tokens, out_count, RS_TOK_NEQ, i) != 0) {
        rs_set_err(err, "too many tokens", i, line, col);
        return -1;
      }
      i += 2;
      col += 2;
      continue;
    }

    if (ch == '<' && src[i + 1] == '>') {
      if (rs_emit(out_tokens, max_tokens, out_count, RS_TOK_NEQ, i) != 0) {
        rs_set_err(err, "too many tokens", i, line, col);
        return -1;
      }
      i += 2;
      col += 2;
      continue;
    }

    if (ch == '>' && src[i + 1] == '=') {
      if (rs_emit(out_tokens, max_tokens, out_count, RS_TOK_GTE, i) != 0) {
        rs_set_err(err, "too many tokens", i, line, col);
        return -1;
      }
      i += 2;
      col += 2;
      continue;
    }

    if (ch == '<' && src[i + 1] == '=') {
      if (rs_emit(out_tokens, max_tokens, out_count, RS_TOK_LTE, i) != 0) {
        rs_set_err(err, "too many tokens", i, line, col);
        return -1;
      }
      i += 2;
      col += 2;
      continue;
    }

    if (ch == '@') {
      if (rs_emit(out_tokens, max_tokens, out_count, RS_TOK_AT, i) != 0) {
        rs_set_err(err, "too many tokens", i, line, col);
        return -1;
      }
      i++;
      col++;
      continue;
    }

    if (ch == '|') {
      if (rs_emit(out_tokens, max_tokens, out_count, RS_TOK_PIPE, i) != 0) {
        rs_set_err(err, "too many tokens", i, line, col);
        return -1;
      }
      i++;
      col++;
      continue;
    }

    if (ch == '?') {
      if (rs_emit(out_tokens, max_tokens, out_count, RS_TOK_QUESTION, i) != 0) {
        rs_set_err(err, "too many tokens", i, line, col);
        return -1;
      }
      i++;
      col++;
      continue;
    }

    if (ch == '%') {
      if (rs_emit(out_tokens, max_tokens, out_count, RS_TOK_PERCENT, i) != 0) {
        rs_set_err(err, "too many tokens", i, line, col);
        return -1;
      }
      i++;
      col++;
      continue;
    }

    if (ch == '[') {
      if (rs_emit(out_tokens, max_tokens, out_count, RS_TOK_LBRACKET, i) != 0) {
        rs_set_err(err, "too many tokens", i, line, col);
        return -1;
      }
      i++;
      col++;
      continue;
    }

    if (ch == ']') {
      if (rs_emit(out_tokens, max_tokens, out_count, RS_TOK_RBRACKET, i) != 0) {
        rs_set_err(err, "too many tokens", i, line, col);
        return -1;
      }
      i++;
      col++;
      continue;
    }

    if (ch == '(') {
      if (rs_emit(out_tokens, max_tokens, out_count, RS_TOK_LPAREN, i) != 0) {
        rs_set_err(err, "too many tokens", i, line, col);
        return -1;
      }
      i++;
      col++;
      continue;
    }

    if (ch == ')') {
      if (rs_emit(out_tokens, max_tokens, out_count, RS_TOK_RPAREN, i) != 0) {
        rs_set_err(err, "too many tokens", i, line, col);
        return -1;
      }
      i++;
      col++;
      continue;
    }

    if (ch == '.') {
      if (rs_emit(out_tokens, max_tokens, out_count, RS_TOK_DOT, i) != 0) {
        rs_set_err(err, "too many tokens", i, line, col);
        return -1;
      }
      i++;
      col++;
      continue;
    }

    if (ch == ',') {
      if (rs_emit(out_tokens, max_tokens, out_count, RS_TOK_COMMA, i) != 0) {
        rs_set_err(err, "too many tokens", i, line, col);
        return -1;
      }
      i++;
      col++;
      continue;
    }

    if (ch == ';') {
      if (rs_emit(out_tokens, max_tokens, out_count, RS_TOK_SEMI, i) != 0) {
        rs_set_err(err, "too many tokens", i, line, col);
        return -1;
      }
      i++;
      col++;
      continue;
    }

    if (ch == '=') {
      if (rs_emit(out_tokens, max_tokens, out_count, RS_TOK_ASSIGN, i) != 0) {
        rs_set_err(err, "too many tokens", i, line, col);
        return -1;
      }
      i++;
      col++;
      continue;
    }

    if (ch == '>') {
      if (rs_emit(out_tokens, max_tokens, out_count, RS_TOK_GT, i) != 0) {
        rs_set_err(err, "too many tokens", i, line, col);
        return -1;
      }
      i++;
      col++;
      continue;
    }

    if (ch == '<') {
      if (rs_emit(out_tokens, max_tokens, out_count, RS_TOK_LT, i) != 0) {
        rs_set_err(err, "too many tokens", i, line, col);
        return -1;
      }
      i++;
      col++;
      continue;
    }

    rs_set_err(err, "unexpected character", i, line, col);
    return -1;
  }

  if (rs_emit(out_tokens, max_tokens, out_count, RS_TOK_EOF, i) != 0) {
    rs_set_err(err, "too many tokens", i, line, col);
    return -1;
  }

  return 0;
}
