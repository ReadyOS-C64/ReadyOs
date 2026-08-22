#ifndef SETUP_CONFIG_H
#define SETUP_CONFIG_H

#define SETUP_CONFIG_CAP 4096u
#define SETUP_PATH_CAP   192u

unsigned char setup_config_find_path(const unsigned char *data,
                                     unsigned int length,
                                     char *path,
                                     unsigned int path_cap);
unsigned char setup_config_prepare(unsigned char *data,
                                   unsigned int *length,
                                   unsigned int capacity,
                                   const char *path);

#endif
