#include "setup_config.h"

#include <string.h>

static const unsigned char key_dma[] = {
    0x64u, 0x6Du, 0x61u, 0x5Fu, 0x6Cu, 0x6Fu,
    0x61u, 0x64u, 0x69u, 0x6Eu, 0x67u, 0u
};
static const unsigned char key_path[] = {
    0x63u, 0x36u, 0x34u, 0x75u, 0x5Fu, 0x69u, 0x6Du, 0x61u,
    0x67u, 0x65u, 0x5Fu, 0x70u, 0x61u, 0x74u, 0x68u, 0u
};

static unsigned char lower(unsigned char ch) {
    if (ch >= 0x41u && ch <= 0x5Au) return (unsigned char)(ch + 0x20u);
    return ch;
}

static unsigned char key_at(const unsigned char *data, unsigned int pos,
                            unsigned int end, const unsigned char *key) {
    unsigned int i = 0u;
    while (key[i] != 0) {
        if (pos + i >= end || lower(data[pos + i]) != (unsigned char)key[i])
            return 0u;
        ++i;
    }
    return (unsigned char)(pos + i < end && data[pos + i] == '=');
}

static unsigned int key_length(const unsigned char *key) {
    unsigned int length = 0u;
    while (key[length] != 0u) ++length;
    return length;
}

unsigned char setup_config_find_path(const unsigned char *data,
                                     unsigned int length,
                                     char *path,
                                     unsigned int path_cap) {
    unsigned int line;
    unsigned int end;
    unsigned int value;
    unsigned int n;
    if (data == 0 || path == 0 || path_cap == 0u) return 0u;
    path[0] = 0;
    line = 0u;
    while (line < length) {
        end = line;
        while (end < length && data[end] != 13u && data[end] != 10u) ++end;
        if (key_at(data, line, end, key_path)) {
            value = (unsigned int)(line + key_length(key_path) + 1u);
            n = 0u;
            while (value < end && n + 1u < path_cap)
                path[n++] = (char)data[value++];
            path[n] = 0;
            return (unsigned char)(value == end);
        }
        line = end;
        while (line < length && (data[line] == 13u || data[line] == 10u)) ++line;
    }
    return 0u;
}

static unsigned char replace_value(unsigned char *data,
                                   unsigned int *length,
                                   unsigned int capacity,
                                   const unsigned char *key,
                                   const char *value) {
    unsigned int line;
    unsigned int end;
    unsigned int value_pos;
    unsigned int old_len;
    unsigned int new_len;
    unsigned int tail;

    line = 0u;
    while (line < *length) {
        end = line;
        while (end < *length && data[end] != 13u && data[end] != 10u) ++end;
        if (key_at(data, line, end, key)) {
            value_pos = (unsigned int)(line + key_length(key) + 1u);
            old_len = (unsigned int)(end - value_pos);
            new_len = strlen(value);
            if (new_len > old_len &&
                *length > (unsigned int)(capacity - (new_len - old_len))) return 0u;
            tail = (unsigned int)(*length - end);
            memmove(data + value_pos + new_len, data + end, tail);
            memcpy(data + value_pos, value, new_len);
            *length = (unsigned int)(*length + new_len - old_len);
            return 1u;
        }
        line = end;
        while (line < *length && (data[line] == 13u || data[line] == 10u)) ++line;
    }
    return 0u;
}

unsigned char setup_config_prepare(unsigned char *data,
                                   unsigned int *length,
                                   unsigned int capacity,
                                   const char *path) {
    if (data == 0 || length == 0 || path == 0 || path[0] != '/' ||
        strlen(path) >= SETUP_PATH_CAP) return 0u;
    if (!replace_value(data, length, capacity, key_dma, "1")) return 0u;
    return replace_value(data, length, capacity, key_path, path);
}
