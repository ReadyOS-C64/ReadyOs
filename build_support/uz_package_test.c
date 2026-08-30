#include "uz_package.h"

#include <assert.h>
#include <string.h>

void reu_dma_fetch(unsigned int c64_addr, unsigned char bank,
                   unsigned int reu_offset, unsigned int length) {
    (void)c64_addr;
    (void)bank;
    (void)reu_offset;
    (void)length;
}

static void put16(unsigned char *header, unsigned char at,
                  unsigned int value) {
    header[at] = (unsigned char)value;
    header[(unsigned char)(at + 1u)] = (unsigned char)(value >> 8u);
}

static unsigned char descriptor_at(unsigned char phase,
                                   unsigned char field) {
    return (unsigned char)(UZ_PACKAGE_DESC_BASE +
                           phase * UZ_PACKAGE_DESC_SIZE + field);
}

static void make_header(unsigned char *header) {
    unsigned char phase;
    unsigned int cursor;

    memset(header, 0, UZ_PACKAGE_HEADER_SIZE);
    header[0] = 0x55u;
    header[1] = 0x5Au;
    header[2] = 0x50u;
    header[3] = 0x4Bu;
    header[4] = UZ_PACKAGE_VERSION;
    header[5] = UZ_PACKAGE_PHASE_COUNT;
    put16(header, 6u, UZ_PACKAGE_HEADER_SIZE);
    cursor = UZ_PACKAGE_HEADER_SIZE;
    for (phase = 0u; phase < UZ_PACKAGE_PHASE_COUNT; ++phase) {
        put16(header, descriptor_at(phase, UZ_PACKAGE_FIELD_OFFSET), cursor);
        put16(header, descriptor_at(phase, UZ_PACKAGE_FIELD_SIZE),
              (unsigned int)(phase + 1u));
        put16(header, descriptor_at(phase, UZ_PACKAGE_FIELD_RUN),
              phase == UZ_PACKAGE_PHASE_COORD ? 0xA000u : 0xB000u);
        put16(header, descriptor_at(phase, UZ_PACKAGE_FIELD_BSS_SIZE),
              phase == UZ_PACKAGE_PHASE_INFLATE ? 123u : 0u);
        cursor = (unsigned int)(cursor + phase + 1u);
    }
    put16(header, 8u, cursor);
}

int main(void) {
    unsigned char header[UZ_PACKAGE_HEADER_SIZE];
    unsigned char saved;

    make_header(header);
    assert(uz_package_parse(header));
    assert(uz_package_version() == UZ_PACKAGE_VERSION);
    assert(uz_package_phase_count() == UZ_PACKAGE_PHASE_COUNT);
    assert(uz_package_payload_size() == UZ_PACKAGE_HEADER_SIZE + 21u);
    assert(uz_package_phase_offset(UZ_PACKAGE_PHASE_JOB) ==
           UZ_PACKAGE_HEADER_SIZE);
    assert(uz_package_phase_size(UZ_PACKAGE_PHASE_READER) == 6u);
    assert(uz_package_phase_run(UZ_PACKAGE_PHASE_COORD) == 0xA000u);
    assert(uz_package_phase_bss_size(UZ_PACKAGE_PHASE_INFLATE) == 123u);
    assert(uz_package_phase_size(UZ_PACKAGE_PHASE_COUNT) == 0u);

    saved = header[4];
    header[4] = 6u;
    assert(!uz_package_parse(header));
    assert(uz_package_payload_size() == 0u);
    header[4] = saved;

    make_header(header);
    header[10] = 1u;
    assert(!uz_package_parse(header));

    make_header(header);
    put16(header, descriptor_at(1u, UZ_PACKAGE_FIELD_OFFSET),
          UZ_PACKAGE_HEADER_SIZE + 2u);
    assert(!uz_package_parse(header));

    make_header(header);
    put16(header, descriptor_at(2u, UZ_PACKAGE_FIELD_SIZE), 0u);
    assert(!uz_package_parse(header));

    make_header(header);
    put16(header, 8u, UZ_PACKAGE_HEADER_SIZE + 20u);
    assert(!uz_package_parse(header));

    make_header(header);
    put16(header, 8u, UZ_PACKAGE_MAX_SIZE + 1u);
    assert(!uz_package_parse(header));
    return 0;
}
