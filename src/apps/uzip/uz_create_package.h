#ifndef UZ_CREATE_PACKAGE_H
#define UZ_CREATE_PACKAGE_H

#define UZ_CREATE_PACKAGE_VERSION 2u
#define UZ_CREATE_PACKAGE_HEADER_SIZE 28u
#define UZ_CREATE_PACKAGE_MAX_IMAGE 0x1100u
#define UZ_CREATE_PACKAGE_MAX_RESOURCE 0xD000u
#define UZ_CREATE_PACKAGE_CACHE_OFFSET 0xF000u

/* The create coordinator is a compact extension immediately after the
 * canonical uZPK v7 payload. It is deliberately outside the six extraction
 * descriptors so the physically proven v7 prefix remains byte-for-byte
 * frozen. */
unsigned char uz_create_package_open(unsigned char package_bank);
unsigned int uz_create_package_offset(void);
unsigned int uz_create_package_size(void);
unsigned int uz_create_package_run(void);
unsigned int uz_create_package_entry(void);

#endif
