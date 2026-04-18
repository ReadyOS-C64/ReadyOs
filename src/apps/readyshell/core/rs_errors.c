#include "rs_errors.h"

void rs_error_init(RSError* err) {
  if (!err) {
    return;
  }
  err->code = RS_ERR_NONE;
  err->message = "";
  err->offset = 0;
  err->line = 1;
  err->column = 1;
}

void rs_error_set(RSError* err,
                  RSErrorCode code,
                  const char* message,
                  unsigned short offset,
                  unsigned short line,
                  unsigned short column) {
  if (!err) {
    return;
  }
  err->code = code;
  if (message) {
    err->message = message;
  } else {
    err->message = "";
  }
  err->offset = offset;
  err->line = line;
  err->column = column;
}
