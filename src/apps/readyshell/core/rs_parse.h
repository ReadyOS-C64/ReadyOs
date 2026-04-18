#ifndef RS_PARSE_H
#define RS_PARSE_H

#include "rs_errors.h"
#include "rs_lexer.h"

typedef struct RSExpr RSExpr;
typedef struct RSProgram RSProgram;

typedef enum {
  RS_EXPR_NUMBER = 0,
  RS_EXPR_STRING,
  RS_EXPR_BOOL,
  RS_EXPR_VAR,
  RS_EXPR_AT,
  RS_EXPR_ARRAY,
  RS_EXPR_RANGE,
  RS_EXPR_PROP,
  RS_EXPR_INDEX,
  RS_EXPR_BINARY,
  RS_EXPR_SCRIPT
} RSExprKind;

typedef enum {
  RS_OP_GT = 0,
  RS_OP_LT,
  RS_OP_GTE,
  RS_OP_LTE,
  RS_OP_EQ,
  RS_OP_NEQ
} RSBinaryOp;

struct RSExpr {
  RSExprKind kind;
  union {
    unsigned short number;
    int boolean;
    char* text;
    struct {
      RSExpr** items;
      unsigned short count;
    } array;
    struct {
      RSExpr* start;
      RSExpr* end;
    } range;
    struct {
      RSExpr* left;
      RSExpr* right;
      RSBinaryOp op;
    } binary;
    struct {
      RSExpr* target;
      char* name;
    } prop;
    struct {
      RSExpr* target;
      RSExpr* index;
    } index;
    RSProgram* script;
  } as;
};

typedef enum {
  RS_STAGE_CMD = 0,
  RS_STAGE_FILTER,
  RS_STAGE_FOREACH,
  RS_STAGE_EXPR
} RSStageKind;

typedef struct RSStage {
  RSStageKind kind;
  union {
    struct {
      char* name;
      RSExpr** args;
      unsigned short arg_count;
    } cmd;
    RSProgram* script;
    RSExpr* expr;
  } as;
} RSStage;

typedef struct RSPipeline {
  RSStage* stages;
  unsigned short count;
} RSPipeline;

typedef enum {
  RS_STMT_ASSIGN = 0,
  RS_STMT_PIPELINE,
  RS_STMT_EXPR
} RSStmtKind;

typedef struct RSStmt {
  RSStmtKind kind;
  union {
    struct {
      char* name;
      int rhs_is_pipeline;
      RSExpr* expr;
      RSPipeline pipeline;
    } assign;
    RSPipeline pipeline;
    RSExpr* expr;
  } as;
} RSStmt;

struct RSProgram {
  RSStmt* stmts;
  unsigned short count;
};

int rs_parse_tokens(const char* source,
                    const RSToken* tokens,
                    unsigned short token_count,
                    RSProgram* out_program,
                    RSError* err);
int rs_parse_source(const char* source, RSProgram* out_program, RSError* err);

void rs_expr_free(RSExpr* expr);
void rs_stage_free(RSStage* stage);
void rs_pipeline_free(RSPipeline* pipeline);
void rs_stmt_free(RSStmt* stmt);
void rs_program_free(RSProgram* program);

#endif
