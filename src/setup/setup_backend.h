#ifndef SETUP_BACKEND_H
#define SETUP_BACKEND_H

#include "setup_config.h"

#define SETUP_PAGE_ROWS 14u
#define SETUP_NAME_CAP  96u

typedef struct {
    char name[SETUP_NAME_CAP];
    unsigned char directory;
} SetupEntry;

typedef struct {
    SetupEntry entries[SETUP_PAGE_ROWS];
    unsigned int total;
    unsigned char count;
    unsigned char page;
    unsigned char more;
} SetupPage;

unsigned char setup_backend_identify(void);
unsigned char setup_backend_list(const char *path, unsigned char page,
                                 SetupPage *out);
unsigned char setup_backend_validate_image(const char *full_path,
                                           unsigned char *config,
                                           unsigned int *config_len);
unsigned char setup_backend_configure_image(const char *full_path,
                                            unsigned char *config,
                                            unsigned int *config_len);
const char *setup_backend_status(void);

#endif
