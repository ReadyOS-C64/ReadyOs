#ifndef RS_ERRORS_H
#define RS_ERRORS_H

typedef enum {
  RS_ERR_NONE = 0,
  RS_ERR_LEX = 1,
  RS_ERR_PARSE = 2,
  RS_ERR_EXEC = 3,
  RS_ERR_OOM = 4,
  RS_ERR_IO = 5,
  RS_ERR_SERIALIZE = 6
} RSErrorCode;

typedef struct RSError {
  RSErrorCode code;
  const char* message;
  unsigned short offset;
  unsigned short line;
  unsigned short column;
} RSError;

void rs_error_init(RSError* err);
void rs_error_set(RSError* err,
                  RSErrorCode code,
                  const char* message,
                  unsigned short offset,
                  unsigned short line,
                  unsigned short column);

#endif
