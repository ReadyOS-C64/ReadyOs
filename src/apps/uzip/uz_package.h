#ifndef UZ_PACKAGE_H
#define UZ_PACKAGE_H

/* uZPK v7 is one launcher-preloaded REU payload. The header is followed by
 * six canonical, consecutive phase images; no phase alignment is required in
 * REU because every image is DMA-expanded into its fixed C64 run window. */
#define UZ_PACKAGE_VERSION       7u
#define UZ_PACKAGE_PHASE_COUNT   6u
#define UZ_PACKAGE_HEADER_SIZE   64u
#define UZ_PACKAGE_MAX_SIZE      0x5F00u
#define UZ_PACKAGE_DESC_BASE     12u
#define UZ_PACKAGE_DESC_SIZE     8u

#define UZ_PACKAGE_PHASE_JOB      0u
#define UZ_PACKAGE_PHASE_INFLATE  1u
#define UZ_PACKAGE_PHASE_MATCH    2u
#define UZ_PACKAGE_PHASE_EMIT     3u
#define UZ_PACKAGE_PHASE_COORD    4u
#define UZ_PACKAGE_PHASE_READER   5u

#define UZ_PACKAGE_FIELD_OFFSET   0u
#define UZ_PACKAGE_FIELD_SIZE     2u
#define UZ_PACKAGE_FIELD_RUN      4u
#define UZ_PACKAGE_FIELD_BSS_SIZE 6u

/* Parse a 64-byte header already in C64 RAM, or fetch and parse it from the
 * start of an owned REU bank. A failed parse invalidates all accessors. */
unsigned char uz_package_parse(const unsigned char *header);
unsigned char uz_package_open(unsigned char package_bank);

unsigned char uz_package_version(void);
unsigned char uz_package_phase_count(void);
unsigned int uz_package_payload_size(void);
unsigned int uz_package_phase_offset(unsigned char phase);
unsigned int uz_package_phase_size(unsigned char phase);
unsigned int uz_package_phase_run(unsigned char phase);
unsigned int uz_package_phase_bss_size(unsigned char phase);

#endif
