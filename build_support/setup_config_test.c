#include "src/setup/setup_config.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void expect_path(const unsigned char *data, unsigned int length,
                        const char *expected) {
    char path[SETUP_PATH_CAP];
    assert(setup_config_find_path(data, length, path, sizeof(path)));
    assert(strcmp(path, expected) == 0);
}

static int contains(const unsigned char *data, unsigned int length,
                    const char *needle) {
    unsigned int i;
    unsigned int n = (unsigned int)strlen(needle);
    for (i = 0u; i + n <= length; ++i)
        if (memcmp(data + i, needle, n) == 0) return 1;
    return 0;
}

int main(void) {
    static unsigned char config[SETUP_CONFIG_CAP];
    static const char source[] =
        "[SYSTEM]\r"
        "VARIANT_NAME=PRECOG ULTIMATE\r"
        "[LAUNCHER]\r"
        "DMA_LOADING=0\r"
        "C64U_IMAGE_PATH=/USB1/OLD.D81\r"
        "[APPS]\r8:EDITOR:EDITOR\rTEXT EDITOR\r";
    unsigned int length = (unsigned int)strlen(source);

    memcpy(config, source, length);
    expect_path(config, length, "/USB1/OLD.D81");
    assert(setup_config_prepare(config, &length, sizeof(config),
                                "/USB1/READYOS/READYOS.D81"));
    expect_path(config, length, "/USB1/READYOS/READYOS.D81");
    assert(contains(config, length, "DMA_LOADING=1"));
    assert(contains(config, length, "8:EDITOR:EDITOR"));

    assert(setup_config_prepare(config, &length, sizeof(config), "/SD/X.D81"));
    expect_path(config, length, "/SD/X.D81");

    memcpy(config, "DMA_LOADING=1\r", 14u);
    length = 14u;
    assert(!setup_config_prepare(config, &length, sizeof(config), "/USB1/X.D81"));
    assert(!setup_config_prepare(config, &length, sizeof(config), "USB1/X.D81"));

    puts("SETUP config parse/rewrite/preserve tests passed");
    return 0;
}
