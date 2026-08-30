#include "uz_cold_boot.h"

#include "uz_inflate6502.h"
#include "uz_package.h"
#include "uz_create_plan_overlay.h"
#include "../../lib/reu_control_bank.h"
#include "../../lib/reu_mgr.h"

#include <string.h>

#define UZCB_CURRENT_BANK (*(volatile unsigned char *)0xC834u)
#define UZCB_HEADER ((unsigned char *)0x0800u)
#define UZCB_RECORD ((unsigned char *)0x0840u)
#define UZCB_UI_HEADER ((unsigned char *)0x0860u)
#define UZCB_INPUT ((unsigned char *)0x0900u)
#define UZCB_OUTPUT ((unsigned char *)0x0B00u)

#define UZCB_INPUT_CAP 512u
#define UZCB_OUTPUT_CAP 508u
#define UZCB_UI_HEADER_SIZE 20u
#define UZCB_CREATE_HEADER_SIZE 28u
#define UZCB_CREATE_CACHE_OFFSET 0xF000u
#define UZCB_CREATE_MAX_SIZE 0x1100u
#define UZCB_PLAN_MAX_SIZE 0x2000u
#define UZCB_UI_MAX_SIZE 0x7000u
#define UZCB_RESOURCE_MAX 0xD000u
#define UZCB_INFLATE_DESC (UZ_PACKAGE_DESC_BASE + UZ_PACKAGE_DESC_SIZE)

#pragma code-name(push, "BOOT_CODE")
#pragma rodata-name(push, "BOOT_CODE")
#pragma bss-name(push, "BOOT_BSS")

static unsigned char boot_package_bank;
static unsigned int boot_input_offset;
static unsigned int boot_input_left;
static UzU32 boot_compressed_size;
static UzU32 boot_output_size;

static unsigned int word_at(const unsigned char *data, unsigned char at) {
    return (unsigned int)data[at] |
           ((unsigned int)data[(unsigned char)(at + 1u)] << 8u);
}

static unsigned char find_package_bank(void) {
    unsigned char app_id;
    unsigned char index;
    unsigned int offset;

    if (UZCB_CURRENT_BANK == 0u) return 0xFFu;
    app_id = readyos_bank_read_byte(
        (unsigned int)(REUCB_TOKEN_APP_OFF + UZCB_CURRENT_BANK));
    if (app_id >= REUCB_APP_REG_COUNT) return 0xFFu;
    for (index = 0u; index < REUCB_RSRC_REC_COUNT; ++index) {
        offset = (unsigned int)(REUCB_RSRC_REC_OFF +
                 (unsigned int)index * REUCB_RSRC_REC_SIZE);
        reu_dma_fetch((unsigned int)UZCB_RECORD,
                      REU_READYOS_GLOBAL_PHYSICAL(), offset,
                      REUCB_RSRC_REC_SIZE);
        if (UZCB_RECORD[0] == app_id &&
            UZCB_RECORD[2] == REUCB_DEP_KIND_RS_OVL &&
            UZCB_RECORD[3] != 0u && UZCB_RECORD[10] == 1u)
            return UZCB_RECORD[3];
    }
    return 0xFFu;
}

static int boot_read(void *context, unsigned char *destination,
                     unsigned int length) {
    (void)context;
    if (length == 0u || length > boot_input_left) return -1;
    reu_dma_fetch((unsigned int)destination, boot_package_bank,
                  boot_input_offset, length);
    boot_input_offset = (unsigned int)(boot_input_offset + length);
    boot_input_left = (unsigned int)(boot_input_left - length);
    return (int)length;
}

static unsigned char boot_write(void *context, const unsigned char *source,
                                unsigned int length) {
    (void)context;
    (void)source;
    /* The frozen inflater writes every byte into its dictionary at $3000
     * before calling this boundary. For a sub-32K UI that dictionary is the
     * final UI image, so the sink only acknowledges the bounded flush. */
    return (unsigned char)(length != 0u && length <= UZCB_OUTPUT_CAP);
}

static unsigned char inflate_resource(unsigned int input_offset,
                                      unsigned int compressed_size,
                                      unsigned int output_size,
                                      const unsigned char *expected_crc) {
    const UzCrc32 *actual_crc;
    unsigned char index;

    boot_input_offset = input_offset;
    boot_input_left = compressed_size;
    boot_compressed_size.lo = compressed_size;
    boot_compressed_size.hi = 0u;
    boot_output_size.lo = output_size;
    boot_output_size.hi = 0u;
    uz_inflate6502_init(boot_read, 0, boot_write, 0,
                       UZCB_INPUT, UZCB_INPUT_CAP,
                       UZCB_OUTPUT, UZCB_OUTPUT_CAP,
                       &boot_compressed_size, &boot_output_size);
    if (!uz_inflate6502_run() || boot_input_left != 0u) return 0u;
    actual_crc = uz_inflate6502_crc();
    for (index = 0u; index < 4u; ++index) {
        if (actual_crc->byte[index] != expected_crc[index]) return 0u;
    }
    return 1u;
}

unsigned char uz_cold_boot_run(void) {
    unsigned int v7_size;
    unsigned int bundle_compressed_size;
    unsigned int bundle_output_size;
    unsigned int create_size;
    unsigned int create_run;
    unsigned int create_entry;
    unsigned int plan_offset;
    unsigned int plan_size;
    unsigned int plan_run;
    unsigned int plan_entry;
    unsigned int ui_header_offset;
    unsigned int compressed_size;
    unsigned int output_size;
    unsigned int output_run;
    unsigned int ui_entry;
    unsigned int inflate_offset;
    unsigned int inflate_size;
    unsigned int inflate_run;
    unsigned int inflate_bss;
    unsigned int inflate_bss_run;

    boot_package_bank = find_package_bank();
    if (boot_package_bank == 0xFFu) return 1u;
    reu_dma_fetch((unsigned int)UZCB_HEADER, boot_package_bank, 0u,
                  UZ_PACKAGE_HEADER_SIZE);
    if (UZCB_HEADER[0] != 0x55u || UZCB_HEADER[1] != 0x5Au ||
        UZCB_HEADER[2] != 0x50u || UZCB_HEADER[3] != 0x4Bu ||
        UZCB_HEADER[4] != UZ_PACKAGE_VERSION ||
        UZCB_HEADER[5] != UZ_PACKAGE_PHASE_COUNT ||
        word_at(UZCB_HEADER, 6u) != UZ_PACKAGE_HEADER_SIZE) return 2u;
    v7_size = word_at(UZCB_HEADER, 8u);
    if (v7_size < UZ_PACKAGE_HEADER_SIZE ||
        v7_size > UZ_PACKAGE_MAX_SIZE) return 3u;

    reu_dma_fetch((unsigned int)UZCB_UI_HEADER, boot_package_bank, v7_size,
                  UZCB_CREATE_HEADER_SIZE);
    if (UZCB_UI_HEADER[0] != 0x55u || UZCB_UI_HEADER[1] != 0x5Au ||
        UZCB_UI_HEADER[2] != 0x43u || UZCB_UI_HEADER[3] != 0x52u ||
        UZCB_UI_HEADER[4] != 2u ||
        UZCB_UI_HEADER[5] != UZCB_CREATE_HEADER_SIZE) return 4u;
    bundle_compressed_size = word_at(UZCB_UI_HEADER, 6u);
    bundle_output_size = word_at(UZCB_UI_HEADER, 8u);
    create_size = word_at(UZCB_UI_HEADER, 10u);
    create_run = word_at(UZCB_UI_HEADER, 12u);
    create_entry = word_at(UZCB_UI_HEADER, 14u);
    plan_offset = word_at(UZCB_UI_HEADER, 16u);
    plan_size = word_at(UZCB_UI_HEADER, 18u);
    plan_run = word_at(UZCB_UI_HEADER, 20u);
    plan_entry = word_at(UZCB_UI_HEADER, 22u);
    if (bundle_compressed_size == 0u || bundle_output_size == 0u ||
        create_size == 0u || create_size > UZCB_CREATE_MAX_SIZE ||
        create_size > 0x1000u || create_run != 0xA000u ||
        create_entry < create_run || create_entry >= create_run + create_size ||
        plan_offset != create_size || plan_size == 0u ||
        plan_size > UZCB_PLAN_MAX_SIZE || plan_run != 0x9000u ||
        plan_entry < plan_run || plan_entry >= plan_run + plan_size ||
        bundle_output_size != create_size + plan_size ||
        v7_size + UZCB_CREATE_HEADER_SIZE < v7_size) return 5u;
    ui_header_offset = (unsigned int)(v7_size + UZCB_CREATE_HEADER_SIZE +
                                      bundle_compressed_size);
    if (ui_header_offset < v7_size ||
        ui_header_offset + UZCB_UI_HEADER_SIZE < ui_header_offset ||
        ui_header_offset + UZCB_UI_HEADER_SIZE > UZCB_RESOURCE_MAX) return 6u;

    inflate_offset = word_at(UZCB_HEADER,
                             (unsigned char)(UZCB_INFLATE_DESC +
                                             UZ_PACKAGE_FIELD_OFFSET));
    inflate_size = word_at(UZCB_HEADER,
                           (unsigned char)(UZCB_INFLATE_DESC +
                                           UZ_PACKAGE_FIELD_SIZE));
    inflate_run = word_at(UZCB_HEADER,
                          (unsigned char)(UZCB_INFLATE_DESC +
                                          UZ_PACKAGE_FIELD_RUN));
    inflate_bss = word_at(UZCB_HEADER,
                          (unsigned char)(UZCB_INFLATE_DESC +
                                          UZ_PACKAGE_FIELD_BSS_SIZE));
    if (inflate_offset < UZ_PACKAGE_HEADER_SIZE || inflate_size == 0u ||
        inflate_run != 0xB000u || inflate_size > 0x1400u ||
        inflate_offset + inflate_size > v7_size ||
        inflate_bss > 0x1400u) return 9u;
    inflate_bss_run = (unsigned int)(inflate_run + inflate_size);
    if (inflate_bss_run < inflate_run ||
        inflate_bss_run + inflate_bss < inflate_bss_run ||
        inflate_bss_run + inflate_bss > 0xC400u) return 9u;

    reu_dma_fetch(inflate_run, boot_package_bank, inflate_offset,
                  inflate_size);
    memset((void *)inflate_bss_run, 0, inflate_bss);
    if (!inflate_resource((unsigned int)(v7_size + UZCB_CREATE_HEADER_SIZE),
                          bundle_compressed_size, bundle_output_size,
                          UZCB_UI_HEADER + 24u)) return 10u;
    reu_dma_stash(0x3000u, boot_package_bank,
                  UZCB_CREATE_CACHE_OFFSET, create_size);
    reu_dma_stash((unsigned int)(0x3000u + plan_offset), boot_package_bank,
                  UZ_CREATE_PLAN_OVERLAY_CACHE_OFFSET, plan_size);

    reu_dma_fetch((unsigned int)UZCB_UI_HEADER, boot_package_bank,
                  ui_header_offset, UZCB_UI_HEADER_SIZE);
    if (UZCB_UI_HEADER[0] != 0x55u || UZCB_UI_HEADER[1] != 0x5Au ||
        UZCB_UI_HEADER[2] != 0x55u || UZCB_UI_HEADER[3] != 0x49u ||
        UZCB_UI_HEADER[4] != 1u ||
        UZCB_UI_HEADER[5] != UZCB_UI_HEADER_SIZE) return 7u;
    compressed_size = word_at(UZCB_UI_HEADER, 6u);
    output_size = word_at(UZCB_UI_HEADER, 8u);
    output_run = word_at(UZCB_UI_HEADER, 10u);
    ui_entry = word_at(UZCB_UI_HEADER, 12u);
    boot_input_offset = (unsigned int)(ui_header_offset +
                                       UZCB_UI_HEADER_SIZE);
    if (compressed_size == 0u || output_size == 0u ||
        output_size > UZCB_UI_MAX_SIZE || output_run != 0x3000u ||
        ui_entry < output_run || ui_entry >= output_run + output_size ||
        boot_input_offset + compressed_size < boot_input_offset ||
        boot_input_offset + compressed_size > UZCB_RESOURCE_MAX) return 8u;
    if (!inflate_resource(boot_input_offset, compressed_size, output_size,
                          UZCB_UI_HEADER + 14u)) return 11u;
    return 0u;
}

#pragma bss-name(pop)
#pragma rodata-name(pop)
#pragma code-name(pop)
