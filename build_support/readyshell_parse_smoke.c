#include <stdio.h>

#include "src/apps/readyshellpoc/core/rs_errors.h"
#include "src/apps/readyshellpoc/core/rs_parse.h"

static int run_case(const char* source, int expect_ok) {
  RSProgram program;
  RSError err;
  int rc = rs_parse_source(source, &program, &err);
  int pass = expect_ok ? (rc == 0) : (rc != 0);

  if (rc == 0) {
    printf("OK   src='%s' stmts=%u\n", source, (unsigned)program.count);
    rs_program_free(&program);
  } else {
    printf("ERR  src='%s' code=%d msg=%s line=%u col=%u\n",
           source,
           (int)err.code,
           err.message ? err.message : "<null>",
           (unsigned)err.line,
           (unsigned)err.column);
  }

  return pass ? 0 : 1;
}

int main(void) {
  int fail = 0;

  fail |= run_case("1", 1);
  fail |= run_case("$A=1", 1);
  fail |= run_case("GEN 3|PRT @", 1);
  fail |= run_case("?[ @ > 1 ]", 1);
  fail |= run_case("$A = [1,2", 0);
  fail |= run_case("$A = 1..", 0);

  return fail;
}
