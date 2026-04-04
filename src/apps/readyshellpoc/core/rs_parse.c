#include "rs_parse.h"

#include <stdlib.h>
#include <string.h>

#if RS_PARSE_TRACE_DEBUG
#include "../platform/rs_overlay.h"
#define RS_PARSE_MARK(code) rs_overlay_debug_mark((unsigned char)(code))
const unsigned char rs_parse_entry_cookie[4] = {0xA5u, 0x5Au, 0xC3u, 0x3Cu};
#else
#define RS_PARSE_MARK(code) ((void)0)
#endif

typedef struct RSParser {
  const RSToken* tokens;
  unsigned short count;
  unsigned short pos;
  RSError* err;
} RSParser;

#ifdef __CC65__
/* Keep token storage out of function scope: cc65 emits function-local static
 * storage at the entry label, which can shift actual prologue bytes away from
 * the exported function symbol in overlay builds. */
static RSToken g_c64_parse_tokens[32];
#endif

static char* rs_dup(const char* s) {
  size_t n;
  char* p;
  if (!s) {
    s = "";
  }
  n = strlen(s);
  p = (char*)malloc(n + 1u);
  if (!p) {
    return 0;
  }
  memcpy(p, s, n + 1u);
  return p;
}

static const RSToken* rs_cur(RSParser* p) {
  if (p->pos >= p->count) {
    return &p->tokens[p->count - 1u];
  }
  return &p->tokens[p->pos];
}

static const RSToken* rs_prev(RSParser* p) {
  if (p->pos == 0) {
    return &p->tokens[0];
  }
  return &p->tokens[p->pos - 1u];
}

static void rs_parse_err_tok(RSParser* p, const char* msg, const RSToken* tok) {
  rs_error_set(p->err,
               RS_ERR_PARSE,
               msg,
               tok ? tok->offset : 0,
               tok ? tok->line : 1,
               tok ? tok->column : 1);
}

static int rs_match(RSParser* p, RSTokenType type) {
  if (rs_cur(p)->type == type) {
    p->pos++;
    return 1;
  }
  return 0;
}

static int rs_expect(RSParser* p, RSTokenType type, const char* msg) {
  if (rs_match(p, type)) {
    return 0;
  }
  rs_parse_err_tok(p, msg, rs_cur(p));
  return -1;
}

static RSExpr* rs_new_expr(RSExprKind kind) {
  RSExpr* e;
  e = (RSExpr*)malloc(sizeof(RSExpr));
  if (!e) {
    return 0;
  }
  memset(e, 0, sizeof(RSExpr));
  e->kind = kind;
  return e;
}

static int rs_program_add_stmt(RSProgram* prog, const RSStmt* stmt) {
  RSStmt* stmts;
  unsigned short n;
  n = (unsigned short)(prog->count + 1u);
  stmts = (RSStmt*)realloc(prog->stmts, sizeof(RSStmt) * n);
  if (!stmts) {
    return -1;
  }
  prog->stmts = stmts;
  prog->stmts[prog->count] = *stmt;
  prog->count = n;
  return 0;
}

static int rs_pipeline_add_stage(RSPipeline* pipeline, const RSStage* stage) {
  RSStage* stages;
  unsigned short n;
  n = (unsigned short)(pipeline->count + 1u);
  stages = (RSStage*)realloc(pipeline->stages, sizeof(RSStage) * n);
  if (!stages) {
    return -1;
  }
  pipeline->stages = stages;
  pipeline->stages[pipeline->count] = *stage;
  pipeline->count = n;
  return 0;
}

static int rs_expr_list_add(RSExpr*** arr, unsigned short* count, RSExpr* expr) {
  RSExpr** next;
  unsigned short n;
  n = (unsigned short)(*count + 1u);
  next = (RSExpr**)realloc(*arr, sizeof(RSExpr*) * n);
  if (!next) {
    return -1;
  }
  *arr = next;
  (*arr)[*count] = expr;
  *count = n;
  return 0;
}

void rs_expr_free(RSExpr* expr) {
  unsigned short i;
  if (!expr) {
    return;
  }
  if (expr->kind == RS_EXPR_STRING || expr->kind == RS_EXPR_VAR) {
    free(expr->as.text);
  } else if (expr->kind == RS_EXPR_ARRAY) {
    for (i = 0; i < expr->as.array.count; ++i) {
      rs_expr_free(expr->as.array.items[i]);
    }
    free(expr->as.array.items);
  } else if (expr->kind == RS_EXPR_RANGE) {
    rs_expr_free(expr->as.range.start);
    rs_expr_free(expr->as.range.end);
  } else if (expr->kind == RS_EXPR_BINARY) {
    rs_expr_free(expr->as.binary.left);
    rs_expr_free(expr->as.binary.right);
  } else if (expr->kind == RS_EXPR_PROP) {
    rs_expr_free(expr->as.prop.target);
    free(expr->as.prop.name);
  } else if (expr->kind == RS_EXPR_INDEX) {
    rs_expr_free(expr->as.index.target);
    rs_expr_free(expr->as.index.index);
  } else if (expr->kind == RS_EXPR_SCRIPT) {
    rs_program_free(expr->as.script);
    free(expr->as.script);
  }
  free(expr);
}

void rs_stage_free(RSStage* stage) {
  unsigned short i;
  if (!stage) {
    return;
  }
  if (stage->kind == RS_STAGE_CMD) {
    free(stage->as.cmd.name);
    for (i = 0; i < stage->as.cmd.arg_count; ++i) {
      rs_expr_free(stage->as.cmd.args[i]);
    }
    free(stage->as.cmd.args);
  } else if (stage->kind == RS_STAGE_FILTER || stage->kind == RS_STAGE_FOREACH) {
    rs_program_free(stage->as.script);
    free(stage->as.script);
  } else if (stage->kind == RS_STAGE_EXPR) {
    rs_expr_free(stage->as.expr);
  }
}

void rs_pipeline_free(RSPipeline* pipeline) {
  unsigned short i;
  if (!pipeline) {
    return;
  }
  for (i = 0; i < pipeline->count; ++i) {
    rs_stage_free(&pipeline->stages[i]);
  }
  free(pipeline->stages);
  pipeline->stages = 0;
  pipeline->count = 0;
}

void rs_stmt_free(RSStmt* stmt) {
  if (!stmt) {
    return;
  }
  if (stmt->kind == RS_STMT_ASSIGN) {
    free(stmt->as.assign.name);
    if (stmt->as.assign.rhs_is_pipeline) {
      rs_pipeline_free(&stmt->as.assign.pipeline);
    } else {
      rs_expr_free(stmt->as.assign.expr);
    }
  } else if (stmt->kind == RS_STMT_PIPELINE) {
    rs_pipeline_free(&stmt->as.pipeline);
  } else if (stmt->kind == RS_STMT_EXPR) {
    rs_expr_free(stmt->as.expr);
  }
}

void rs_program_free(RSProgram* program) {
  unsigned short i;
  if (!program) {
    return;
  }
  for (i = 0; i < program->count; ++i) {
    rs_stmt_free(&program->stmts[i]);
  }
  free(program->stmts);
  program->stmts = 0;
  program->count = 0;
}

static int rs_is_expr_start(RSTokenType type) {
  return type == RS_TOK_NUMBER ||
         type == RS_TOK_STRING ||
         type == RS_TOK_TRUE ||
         type == RS_TOK_FALSE ||
         type == RS_TOK_VAR ||
         type == RS_TOK_AT ||
         type == RS_TOK_LPAREN ||
         type == RS_TOK_LBRACKET;
}

static int rs_parse_program_until(RSParser* p, RSTokenType end_type, RSProgram* out_program);
static int rs_parse_expr(RSParser* p, int allow_array, RSExpr** out_expr);

static int rs_parse_script_block(RSParser* p, RSProgram** out_prog) {
  RSProgram* prog;
  if (rs_expect(p, RS_TOK_LBRACKET, "expected '['") != 0) {
    return -1;
  }
  prog = (RSProgram*)malloc(sizeof(RSProgram));
  if (!prog) {
    rs_parse_err_tok(p, "out of memory", rs_cur(p));
    return -1;
  }
  prog->stmts = 0;
  prog->count = 0;
  if (rs_parse_program_until(p, RS_TOK_RBRACKET, prog) != 0) {
    rs_program_free(prog);
    free(prog);
    return -1;
  }
  if (rs_expect(p, RS_TOK_RBRACKET, "expected ']' to close script block") != 0) {
    rs_program_free(prog);
    free(prog);
    return -1;
  }
  *out_prog = prog;
  return 0;
}

static int rs_parse_primary(RSParser* p, RSExpr** out_expr) {
  const RSToken* tok;
  RSExpr* e;
  RSProgram* script;

  tok = rs_cur(p);
  if (tok->type == RS_TOK_NUMBER) {
    e = rs_new_expr(RS_EXPR_NUMBER);
    if (!e) {
      rs_parse_err_tok(p, "out of memory", tok);
      return -1;
    }
    e->as.number = tok->number;
    p->pos++;
    *out_expr = e;
    return 0;
  }

  if (tok->type == RS_TOK_STRING) {
    e = rs_new_expr(RS_EXPR_STRING);
    if (!e) {
      rs_parse_err_tok(p, "out of memory", tok);
      return -1;
    }
    e->as.text = rs_dup(tok->text);
    if (!e->as.text) {
      rs_expr_free(e);
      rs_parse_err_tok(p, "out of memory", tok);
      return -1;
    }
    p->pos++;
    *out_expr = e;
    return 0;
  }

  if (tok->type == RS_TOK_TRUE || tok->type == RS_TOK_FALSE) {
    e = rs_new_expr(RS_EXPR_BOOL);
    if (!e) {
      rs_parse_err_tok(p, "out of memory", tok);
      return -1;
    }
    e->as.boolean = (tok->type == RS_TOK_TRUE);
    p->pos++;
    *out_expr = e;
    return 0;
  }

  if (tok->type == RS_TOK_VAR) {
    e = rs_new_expr(RS_EXPR_VAR);
    if (!e) {
      rs_parse_err_tok(p, "out of memory", tok);
      return -1;
    }
    e->as.text = rs_dup(tok->text);
    if (!e->as.text) {
      rs_expr_free(e);
      rs_parse_err_tok(p, "out of memory", tok);
      return -1;
    }
    p->pos++;
    *out_expr = e;
    return 0;
  }

  if (tok->type == RS_TOK_AT) {
    e = rs_new_expr(RS_EXPR_AT);
    if (!e) {
      rs_parse_err_tok(p, "out of memory", tok);
      return -1;
    }
    p->pos++;
    *out_expr = e;
    return 0;
  }

  if (tok->type == RS_TOK_LPAREN) {
    p->pos++;
    if (rs_parse_expr(p, 1, out_expr) != 0) {
      return -1;
    }
    if (rs_expect(p, RS_TOK_RPAREN, "expected ')' after expression") != 0) {
      rs_expr_free(*out_expr);
      *out_expr = 0;
      return -1;
    }
    return 0;
  }

  if (tok->type == RS_TOK_LBRACKET) {
    if (rs_parse_script_block(p, &script) != 0) {
      return -1;
    }
    e = rs_new_expr(RS_EXPR_SCRIPT);
    if (!e) {
      rs_program_free(script);
      free(script);
      rs_parse_err_tok(p, "out of memory", tok);
      return -1;
    }
    e->as.script = script;
    *out_expr = e;
    return 0;
  }

  rs_parse_err_tok(p, "expected expression", tok);
  return -1;
}

static int rs_parse_postfix(RSParser* p, RSExpr** out_expr) {
  RSExpr* base;
  RSExpr* idx;
  RSExpr* e;
  char* name;
  const RSToken* tok;

  if (rs_parse_primary(p, &base) != 0) {
    return -1;
  }

  for (;;) {
    if (rs_match(p, RS_TOK_DOT)) {
      tok = rs_cur(p);
      if (tok->type != RS_TOK_IDENT) {
        rs_expr_free(base);
        rs_parse_err_tok(p, "expected property name after '.'", tok);
        return -1;
      }
      name = rs_dup(tok->text);
      if (!name) {
        rs_expr_free(base);
        rs_parse_err_tok(p, "out of memory", tok);
        return -1;
      }
      p->pos++;
      e = rs_new_expr(RS_EXPR_PROP);
      if (!e) {
        free(name);
        rs_expr_free(base);
        rs_parse_err_tok(p, "out of memory", tok);
        return -1;
      }
      e->as.prop.target = base;
      e->as.prop.name = name;
      base = e;
      continue;
    }

    if (rs_match(p, RS_TOK_LPAREN)) {
      if (rs_parse_expr(p, 1, &idx) != 0) {
        rs_expr_free(base);
        return -1;
      }
      if (rs_expect(p, RS_TOK_RPAREN, "expected ')' after index") != 0) {
        rs_expr_free(base);
        rs_expr_free(idx);
        return -1;
      }
      e = rs_new_expr(RS_EXPR_INDEX);
      if (!e) {
        rs_expr_free(base);
        rs_expr_free(idx);
        rs_parse_err_tok(p, "out of memory", rs_prev(p));
        return -1;
      }
      e->as.index.target = base;
      e->as.index.index = idx;
      base = e;
      continue;
    }

    break;
  }

  *out_expr = base;
  return 0;
}

static int rs_parse_range(RSParser* p, RSExpr** out_expr) {
  RSExpr* left;
  RSExpr* right;
  RSExpr* e;

  if (rs_parse_postfix(p, &left) != 0) {
    return -1;
  }
  if (!rs_match(p, RS_TOK_RANGE)) {
    *out_expr = left;
    return 0;
  }
  if (rs_parse_postfix(p, &right) != 0) {
    rs_expr_free(left);
    return -1;
  }
  e = rs_new_expr(RS_EXPR_RANGE);
  if (!e) {
    rs_expr_free(left);
    rs_expr_free(right);
    rs_parse_err_tok(p, "out of memory", rs_prev(p));
    return -1;
  }
  e->as.range.start = left;
  e->as.range.end = right;
  *out_expr = e;
  return 0;
}

static int rs_parse_comparison(RSParser* p, RSExpr** out_expr) {
  RSExpr* left;
  RSExpr* right;
  RSExpr* e;
  RSBinaryOp op;
  RSTokenType t;

  if (rs_parse_range(p, &left) != 0) {
    return -1;
  }

  for (;;) {
    t = rs_cur(p)->type;
    if (t != RS_TOK_GT && t != RS_TOK_LT && t != RS_TOK_GTE && t != RS_TOK_LTE &&
        t != RS_TOK_EQ && t != RS_TOK_NEQ) {
      break;
    }
    p->pos++;
    if (rs_parse_range(p, &right) != 0) {
      rs_expr_free(left);
      return -1;
    }
    if (t == RS_TOK_GT) {
      op = RS_OP_GT;
    } else if (t == RS_TOK_LT) {
      op = RS_OP_LT;
    } else if (t == RS_TOK_GTE) {
      op = RS_OP_GTE;
    } else if (t == RS_TOK_LTE) {
      op = RS_OP_LTE;
    } else if (t == RS_TOK_EQ) {
      op = RS_OP_EQ;
    } else {
      op = RS_OP_NEQ;
    }

    e = rs_new_expr(RS_EXPR_BINARY);
    if (!e) {
      rs_expr_free(left);
      rs_expr_free(right);
      rs_parse_err_tok(p, "out of memory", rs_prev(p));
      return -1;
    }
    e->as.binary.left = left;
    e->as.binary.right = right;
    e->as.binary.op = op;
    left = e;
  }

  *out_expr = left;
  return 0;
}

static int rs_parse_expr(RSParser* p, int allow_array, RSExpr** out_expr) {
  RSExpr* first;
  RSExpr* e;
  RSExpr** items;
  unsigned short count;

  if (rs_parse_comparison(p, &first) != 0) {
    return -1;
  }

  if (!allow_array || rs_cur(p)->type != RS_TOK_COMMA) {
    *out_expr = first;
    return 0;
  }

  items = 0;
  count = 0;
  if (rs_expr_list_add(&items, &count, first) != 0) {
    rs_expr_free(first);
    rs_parse_err_tok(p, "out of memory", rs_cur(p));
    return -1;
  }

  while (rs_match(p, RS_TOK_COMMA)) {
    RSExpr* item;
    if (rs_parse_comparison(p, &item) != 0) {
      unsigned short i;
      for (i = 0; i < count; ++i) {
        rs_expr_free(items[i]);
      }
      free(items);
      return -1;
    }
    if (rs_expr_list_add(&items, &count, item) != 0) {
      unsigned short i2;
      rs_expr_free(item);
      for (i2 = 0; i2 < count; ++i2) {
        rs_expr_free(items[i2]);
      }
      free(items);
      rs_parse_err_tok(p, "out of memory", rs_cur(p));
      return -1;
    }
  }

  e = rs_new_expr(RS_EXPR_ARRAY);
  if (!e) {
    unsigned short i3;
    for (i3 = 0; i3 < count; ++i3) {
      rs_expr_free(items[i3]);
    }
    free(items);
    rs_parse_err_tok(p, "out of memory", rs_cur(p));
    return -1;
  }
  e->as.array.items = items;
  e->as.array.count = count;
  *out_expr = e;
  return 0;
}

static int rs_parse_stage(RSParser* p, RSStage* out_stage) {
  const RSToken* tok;
  RSExpr* expr;
  RSProgram* script;
  RSExpr* arg;

  memset(out_stage, 0, sizeof(RSStage));

  if (rs_match(p, RS_TOK_QUESTION)) {
    if (rs_parse_script_block(p, &script) != 0) {
      return -1;
    }
    out_stage->kind = RS_STAGE_FILTER;
    out_stage->as.script = script;
    return 0;
  }

  if (rs_match(p, RS_TOK_PERCENT)) {
    if (rs_parse_script_block(p, &script) != 0) {
      return -1;
    }
    out_stage->kind = RS_STAGE_FOREACH;
    out_stage->as.script = script;
    return 0;
  }

  tok = rs_cur(p);
  if (tok->type == RS_TOK_IDENT) {
    out_stage->kind = RS_STAGE_CMD;
    out_stage->as.cmd.name = rs_dup(tok->text);
    if (!out_stage->as.cmd.name) {
      rs_parse_err_tok(p, "out of memory", tok);
      return -1;
    }
    p->pos++;

    while (rs_is_expr_start(rs_cur(p)->type)) {
      if (rs_parse_expr(p, 0, &arg) != 0) {
        rs_stage_free(out_stage);
        return -1;
      }
      if (rs_expr_list_add(&out_stage->as.cmd.args, &out_stage->as.cmd.arg_count, arg) != 0) {
        rs_expr_free(arg);
        rs_stage_free(out_stage);
        rs_parse_err_tok(p, "out of memory", rs_cur(p));
        return -1;
      }
      if (!rs_match(p, RS_TOK_COMMA)) {
        break;
      }
    }

    return 0;
  }

  if (rs_parse_expr(p, 1, &expr) != 0) {
    return -1;
  }
  out_stage->kind = RS_STAGE_EXPR;
  out_stage->as.expr = expr;
  return 0;
}

static void rs_skip_separators(RSParser* p) {
  while (rs_cur(p)->type == RS_TOK_NEWLINE || rs_cur(p)->type == RS_TOK_SEMI) {
    p->pos++;
  }
}

static int rs_parse_pipeline_or_expr_stmt(RSParser* p, RSStmt* out_stmt) {
  RSStage first;
  RSPipeline pipeline;
  int has_pipe;

  memset(&first, 0, sizeof(first));
  pipeline.stages = 0;
  pipeline.count = 0;

  if (rs_parse_stage(p, &first) != 0) {
    return -1;
  }

  has_pipe = rs_match(p, RS_TOK_PIPE);
  if (!has_pipe && first.kind == RS_STAGE_EXPR) {
    out_stmt->kind = RS_STMT_EXPR;
    out_stmt->as.expr = first.as.expr;
    return 0;
  }

  if (rs_pipeline_add_stage(&pipeline, &first) != 0) {
    rs_stage_free(&first);
    rs_parse_err_tok(p, "out of memory", rs_cur(p));
    return -1;
  }

  while (has_pipe) {
    RSStage next;
    memset(&next, 0, sizeof(next));
    if (rs_parse_stage(p, &next) != 0) {
      rs_pipeline_free(&pipeline);
      return -1;
    }
    if (rs_pipeline_add_stage(&pipeline, &next) != 0) {
      rs_stage_free(&next);
      rs_pipeline_free(&pipeline);
      rs_parse_err_tok(p, "out of memory", rs_cur(p));
      return -1;
    }
    has_pipe = rs_match(p, RS_TOK_PIPE);
  }

  out_stmt->kind = RS_STMT_PIPELINE;
  out_stmt->as.pipeline = pipeline;
  return 0;
}

static int rs_parse_stmt(RSParser* p, RSStmt* out_stmt) {
  const RSToken* tok;
  memset(out_stmt, 0, sizeof(RSStmt));

  tok = rs_cur(p);
  if (tok->type == RS_TOK_VAR && p->pos + 1u < p->count && p->tokens[p->pos + 1u].type == RS_TOK_ASSIGN) {
    RSStmt rhs;
    memset(&rhs, 0, sizeof(rhs));

    out_stmt->kind = RS_STMT_ASSIGN;
    out_stmt->as.assign.name = rs_dup(tok->text);
    if (!out_stmt->as.assign.name) {
      rs_parse_err_tok(p, "out of memory", tok);
      return -1;
    }

    p->pos += 2;

    if (rs_parse_pipeline_or_expr_stmt(p, &rhs) != 0) {
      rs_stmt_free(out_stmt);
      return -1;
    }

    if (rhs.kind == RS_STMT_PIPELINE) {
      out_stmt->as.assign.rhs_is_pipeline = 1;
      out_stmt->as.assign.pipeline = rhs.as.pipeline;
    } else if (rhs.kind == RS_STMT_EXPR) {
      out_stmt->as.assign.rhs_is_pipeline = 0;
      out_stmt->as.assign.expr = rhs.as.expr;
    } else {
      rs_stmt_free(&rhs);
      rs_stmt_free(out_stmt);
      rs_parse_err_tok(p, "invalid assignment rhs", tok);
      return -1;
    }

    return 0;
  }

  return rs_parse_pipeline_or_expr_stmt(p, out_stmt);
}

static int rs_parse_program_until(RSParser* p, RSTokenType end_type, RSProgram* out_program) {
  RSStmt stmt;

  rs_skip_separators(p);

  while (rs_cur(p)->type != RS_TOK_EOF && rs_cur(p)->type != end_type) {
    memset(&stmt, 0, sizeof(stmt));
    if (rs_parse_stmt(p, &stmt) != 0) {
      return -1;
    }
    if (rs_program_add_stmt(out_program, &stmt) != 0) {
      rs_stmt_free(&stmt);
      rs_parse_err_tok(p, "out of memory", rs_cur(p));
      return -1;
    }
    rs_skip_separators(p);
  }

  return 0;
}

int rs_parse_tokens(const RSToken* tokens,
                    unsigned short token_count,
                    RSProgram* out_program,
                    RSError* err) {
  RSParser p;

  if (!tokens || token_count == 0 || !out_program) {
    rs_error_set(err, RS_ERR_PARSE, "invalid parser args", 0, 1, 1);
    return -1;
  }

  rs_error_init(err);

  out_program->stmts = 0;
  out_program->count = 0;

  p.tokens = tokens;
  p.count = token_count;
  p.pos = 0;
  p.err = err;

  if (rs_parse_program_until(&p, RS_TOK_EOF, out_program) != 0) {
    rs_program_free(out_program);
    return -1;
  }

  if (rs_cur(&p)->type != RS_TOK_EOF) {
    rs_parse_err_tok(&p, "unexpected trailing token", rs_cur(&p));
    rs_program_free(out_program);
    return -1;
  }

  return 0;
}

int rs_parse_source(const char* source, RSProgram* out_program, RSError* err) {
  RSToken* tokens;
  unsigned short max_tokens;
  unsigned short count;
  int rc;

  RS_PARSE_MARK('p');
  if (!source || !out_program) {
    RS_PARSE_MARK('F');
    rs_error_set(err, RS_ERR_PARSE, "invalid parser args", 0, 1, 1);
    return -1;
  }

#ifdef __CC65__
  {
    /* Keep C64 token storage small enough to preserve VM heap headroom. */
    (void)source;
    tokens = g_c64_parse_tokens;
    max_tokens = (unsigned short)(sizeof(g_c64_parse_tokens) / sizeof(g_c64_parse_tokens[0]));
    RS_PARSE_MARK('q');
  }
#else
  max_tokens = 2048u;
  tokens = (RSToken*)malloc(sizeof(RSToken) * max_tokens);
  if (!tokens) {
    RS_PARSE_MARK('F');
    rs_error_set(err, RS_ERR_OOM, "out of memory", 0, 1, 1);
    return -1;
  }
  RS_PARSE_MARK('q');
  #endif

  if (rs_lex(source, tokens, max_tokens, &count, err) != 0) {
    RS_PARSE_MARK('F');
    #ifndef __CC65__
    free(tokens);
    #endif
    return -1;
  }

  RS_PARSE_MARK('r');
  RS_PARSE_MARK('s');
  rc = rs_parse_tokens(tokens, count, out_program, err);
  RS_PARSE_MARK('t');
  #ifndef __CC65__
  free(tokens);
  #endif
  if (rc != 0) {
    RS_PARSE_MARK('F');
    return rc;
  }
  RS_PARSE_MARK('P');
  return rc;
}
