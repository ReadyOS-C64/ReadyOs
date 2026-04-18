#ifndef RS_LEXER_H
#define RS_LEXER_H

#include "rs_errors.h"

typedef enum {
  RS_TOK_EOF = 0,
  RS_TOK_NUMBER,
  RS_TOK_STRING,
  RS_TOK_IDENT,
  RS_TOK_VAR,
  RS_TOK_TRUE,
  RS_TOK_FALSE,
  RS_TOK_AT,
  RS_TOK_PIPE,
  RS_TOK_QUESTION,
  RS_TOK_PERCENT,
  RS_TOK_LBRACKET,
  RS_TOK_RBRACKET,
  RS_TOK_LPAREN,
  RS_TOK_RPAREN,
  RS_TOK_DOT,
  RS_TOK_RANGE,
  RS_TOK_COMMA,
  RS_TOK_SEMI,
  RS_TOK_NEWLINE,
  RS_TOK_ASSIGN,
  RS_TOK_GT,
  RS_TOK_LT,
  RS_TOK_GTE,
  RS_TOK_LTE,
  RS_TOK_EQ,
  RS_TOK_NEQ
} RSTokenType;

typedef struct RSToken {
  unsigned char type;
  unsigned short offset;
  /* For identifiers/vars/strings this is slice length; for numbers it is the parsed value. */
  unsigned short value;
} RSToken;

int rs_lex(const char* src,
           RSToken* out_tokens,
           unsigned short max_tokens,
           unsigned short* out_count,
           RSError* err);

#endif
