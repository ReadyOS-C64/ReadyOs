#include "uz_create_package.h"

#include "uz_package.h"
#include "../../lib/reu_mgr.h"

#ifdef UZIP_READYOS_APP
#pragma code-name(push, "UI_CODE")
#pragma rodata-name(push, "UI_RODATA")
#pragma bss-name(push, "UI_BSS")
#endif

static unsigned char create_header[UZ_CREATE_PACKAGE_HEADER_SIZE];
static unsigned int create_offset;
static unsigned int create_size;
static unsigned int create_run;
static unsigned int create_entry;

static unsigned int word_at(unsigned char offset) {
    return (unsigned int)(create_header[offset] |
        ((unsigned int)create_header[(unsigned char)(offset + 1u)] << 8u));
}

unsigned char uz_create_package_open(unsigned char package_bank) {
    unsigned int payload_size;
    unsigned int compressed_size;
    unsigned int output_size;
    unsigned int plan_offset;
    unsigned int plan_size;

    create_offset = 0u;
    create_size = 0u;
    create_run = 0u;
    create_entry = 0u;
    payload_size = uz_package_payload_size();
    if (package_bank == 0xFFu || payload_size < UZ_PACKAGE_HEADER_SIZE ||
        payload_size > UZ_PACKAGE_MAX_SIZE ||
        payload_size + UZ_CREATE_PACKAGE_HEADER_SIZE < payload_size ||
        payload_size + UZ_CREATE_PACKAGE_HEADER_SIZE >
            UZ_CREATE_PACKAGE_MAX_RESOURCE) return 0u;
    reu_dma_fetch((unsigned int)create_header, package_bank, payload_size,
                  sizeof(create_header));
    if (create_header[0] != 0x55u || create_header[1] != 0x5Au ||
        create_header[2] != 0x43u || create_header[3] != 0x52u ||
        create_header[4] != UZ_CREATE_PACKAGE_VERSION ||
        create_header[5] != UZ_CREATE_PACKAGE_HEADER_SIZE) return 0u;
    compressed_size = word_at(6u);
    output_size = word_at(8u);
    create_size = word_at(10u);
    create_run = word_at(12u);
    create_entry = word_at(14u);
    plan_offset = word_at(16u);
    plan_size = word_at(18u);
    create_offset = UZ_CREATE_PACKAGE_CACHE_OFFSET;
    if (compressed_size == 0u ||
        payload_size + UZ_CREATE_PACKAGE_HEADER_SIZE + compressed_size <
            payload_size ||
        payload_size + UZ_CREATE_PACKAGE_HEADER_SIZE + compressed_size >
            UZ_CREATE_PACKAGE_MAX_RESOURCE ||
        create_size == 0u || create_size > UZ_CREATE_PACKAGE_MAX_IMAGE ||
        create_size > 0x1000u ||
        plan_offset != create_size || plan_size == 0u ||
        output_size != create_size + plan_size ||
        create_run != 0xA000u || create_entry < create_run ||
        create_entry >= create_run + create_size) {
        create_offset = 0u;
        create_size = 0u;
        create_run = 0u;
        create_entry = 0u;
        return 0u;
    }
    return 1u;
}

unsigned int uz_create_package_offset(void) { return create_offset; }
unsigned int uz_create_package_size(void) { return create_size; }
unsigned int uz_create_package_run(void) { return create_run; }
unsigned int uz_create_package_entry(void) { return create_entry; }

#ifdef UZIP_READYOS_APP
#pragma bss-name(pop)
#pragma rodata-name(pop)
#pragma code-name(pop)
#endif
