#include "uz_package.h"

#include "../../lib/reu_mgr.h"

#include <string.h>

#ifdef UZIP_READYOS_APP
#pragma code-name(push, "UI_CODE")
#pragma rodata-name(push, "UI_RODATA")
#endif

/* Package descriptors are persistent control state.  Every phase can replace
 * $A000-$C3FF, so the cached header must remain in resident BSS below $3000;
 * placing it in idle UI_BSS makes the first $B000 job destroy the offsets
 * needed to load the next phase. */
static unsigned char package_header[UZ_PACKAGE_HEADER_SIZE];
static unsigned char package_valid;

static unsigned int package_word(const unsigned char *header,
                                 unsigned char offset) {
    return (unsigned int)header[offset] |
           ((unsigned int)header[(unsigned char)(offset + 1u)] << 8u);
}

static unsigned char descriptor_at(unsigned char phase,
                                   unsigned char field) {
    return (unsigned char)(UZ_PACKAGE_DESC_BASE +
                           phase * UZ_PACKAGE_DESC_SIZE + field);
}

unsigned char uz_package_parse(const unsigned char *header) {
    unsigned char phase;
    unsigned char at;
    unsigned int payload_size;
    unsigned int cursor;
    unsigned int offset;
    unsigned int size;

    package_valid = 0u;
    if (header == 0 ||
        header[0] != 0x55u || header[1] != 0x5Au ||
        header[2] != 0x50u || header[3] != 0x4Bu ||
        header[4] != UZ_PACKAGE_VERSION ||
        header[5] != UZ_PACKAGE_PHASE_COUNT ||
        package_word(header, 6u) != UZ_PACKAGE_HEADER_SIZE ||
        header[10] != 0u || header[11] != 0u ||
        header[60] != 0u || header[61] != 0u ||
        header[62] != 0u || header[63] != 0u) return 0u;

    payload_size = package_word(header, 8u);
    if (payload_size < UZ_PACKAGE_HEADER_SIZE ||
        payload_size > UZ_PACKAGE_MAX_SIZE) return 0u;

    cursor = UZ_PACKAGE_HEADER_SIZE;
    for (phase = 0u; phase < UZ_PACKAGE_PHASE_COUNT; ++phase) {
        at = descriptor_at(phase, UZ_PACKAGE_FIELD_OFFSET);
        offset = package_word(header, at);
        size = package_word(header,
                            descriptor_at(phase, UZ_PACKAGE_FIELD_SIZE));
        if (offset != cursor || size == 0u || cursor > payload_size ||
            size > payload_size - cursor ||
            package_word(header,
                         descriptor_at(phase, UZ_PACKAGE_FIELD_RUN)) == 0u)
            return 0u;
        cursor = (unsigned int)(cursor + size);
    }
    if (cursor != payload_size) return 0u;

    if (header != package_header)
        memcpy(package_header, header, UZ_PACKAGE_HEADER_SIZE);
    package_valid = 1u;
    return 1u;
}

unsigned char uz_package_open(unsigned char package_bank) {
    package_valid = 0u;
    if (package_bank == 0xFFu) return 0u;
#ifdef __CC65__
    reu_dma_fetch((unsigned int)package_header, package_bank, 0u,
                  UZ_PACKAGE_HEADER_SIZE);
    return uz_package_parse(package_header);
#else
    (void)package_bank;
    return 0u;
#endif
}

unsigned char uz_package_version(void) {
    return package_valid ? package_header[4] : 0u;
}

unsigned char uz_package_phase_count(void) {
    return package_valid ? package_header[5] : 0u;
}

unsigned int uz_package_payload_size(void) {
    return package_valid ? package_word(package_header, 8u) : 0u;
}

unsigned int uz_package_phase_offset(unsigned char phase) {
    if (!package_valid || phase >= UZ_PACKAGE_PHASE_COUNT) return 0u;
    return package_word(package_header,
                        descriptor_at(phase, UZ_PACKAGE_FIELD_OFFSET));
}

unsigned int uz_package_phase_size(unsigned char phase) {
    if (!package_valid || phase >= UZ_PACKAGE_PHASE_COUNT) return 0u;
    return package_word(package_header,
                        descriptor_at(phase, UZ_PACKAGE_FIELD_SIZE));
}

unsigned int uz_package_phase_run(unsigned char phase) {
    if (!package_valid || phase >= UZ_PACKAGE_PHASE_COUNT) return 0u;
    return package_word(package_header,
                        descriptor_at(phase, UZ_PACKAGE_FIELD_RUN));
}

unsigned int uz_package_phase_bss_size(unsigned char phase) {
    if (!package_valid || phase >= UZ_PACKAGE_PHASE_COUNT) return 0u;
    return package_word(package_header,
                        descriptor_at(phase, UZ_PACKAGE_FIELD_BSS_SIZE));
}

#ifdef UZIP_READYOS_APP
#pragma rodata-name(pop)
#pragma code-name(pop)
#endif
