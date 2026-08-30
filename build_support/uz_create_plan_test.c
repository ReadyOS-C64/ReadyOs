#include "uz_create_plan.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned char bank[UZ_CATALOG_MAX_ENTRIES * sizeof(UzZipRecord)];
static unsigned int list_calls;
static unsigned char listing_mode;
static unsigned int generated_count;

static unsigned char write_bank(void *context, unsigned int offset,
                                const void *source, unsigned int length) {
    (void)context;
    if (offset > sizeof(bank) || length > sizeof(bank) - offset) return 0u;
    memcpy(bank + offset, source, length);
    return 1u;
}

static unsigned char read_bank(void *context, unsigned int offset,
                               void *destination, unsigned int length) {
    (void)context;
    if (offset > sizeof(bank) || length > sizeof(bank) - offset) return 0u;
    memcpy(destination, bank + offset, length);
    return 1u;
}

static void set_entry(UzBrowserPage *page, unsigned char slot,
                      const char *name, unsigned char directory) {
    UzBrowserEntry *entry;

    entry = &page->entries[slot];
    memset(entry, 0, sizeof(*entry));
    strcpy(entry->name, name);
    entry->directory = directory;
    entry->attributes = directory ? 0x10u : 0u;
}

static unsigned char fake_list(void *context, const char *path,
                               unsigned char number, UzBrowserPage *page) {
    unsigned int first;
    unsigned int remaining;
    unsigned int count;
    unsigned int index;
    char name[16];

    (void)context;
    ++list_calls;
    memset(page, 0, sizeof(*page));
    page->page = number;
    if (listing_mode == 0u) {
        if (strcmp(path, "/USB1/SRC/TOP") == 0) {
            assert(number == 0u);
            set_entry(page, 0u, "A.BIN", 0u);
            set_entry(page, 1u, "SUB", 1u);
            set_entry(page, 2u, "EMPTY", 1u);
            page->count = 3u;
            page->total = 3u;
        } else if (strcmp(path, "/USB1/SRC/TOP/SUB") == 0) {
            assert(number == 0u);
            set_entry(page, 0u, "B.BIN", 0u);
            page->count = 1u;
            page->total = 1u;
        } else if (strcmp(path, "/USB1/SRC/TOP/EMPTY") != 0) {
            return 0u;
        }
        return 1u;
    }
    if (listing_mode == 1u || listing_mode == 4u) {
        first = (unsigned int)number * UZ_BROWSER_ROWS;
        if (first >= generated_count) return 0u;
        remaining = generated_count - first;
        count = remaining > UZ_BROWSER_ROWS ? UZ_BROWSER_ROWS : remaining;
        for (index = 0u; index < count; ++index) {
            sprintf(name, "F%03u", first + index);
            set_entry(page, (unsigned char)index, name, 0u);
        }
        page->count = (unsigned char)count;
        page->total = generated_count;
        page->more = (unsigned char)(first + count < generated_count);
        return 1u;
    }
    if (listing_mode == 2u) {
        set_entry(page, 0u, "FILE", 0u);
        set_entry(page, 1u, "file", 0u);
        page->count = 2u;
        page->total = 2u;
        return 1u;
    }
    if (listing_mode == 3u) {
        set_entry(page, 0u, "BAD", 0u);
        page->entries[0].unusable = 1u;
        page->count = 1u;
        page->unusable = 1u;
        return 1u;
    }
    return 0u;
}

static void init_plan(UzCreatePlan *plan, UzCatalog *catalog,
                      UzBrowserPage *page, UzZipRecord *record,
                      UzZipRecord *scratch, char *path,
                      const char *base, unsigned int method) {
    memset(bank, 0, sizeof(bank));
    list_calls = 0u;
    uz_catalog_init(catalog, write_bank, read_bank, 0);
    uz_create_plan_init(plan, catalog, fake_list, 0, base, path,
                        UZ_BROWSER_PATH_CAP, page, record, scratch, method);
}

static void expect_record(UzCatalog *catalog, unsigned int index,
                          UzZipRecord *record, const char *name,
                          unsigned char directory, unsigned int method) {
    assert(uz_catalog_get(catalog, index, record));
    assert(strcmp(record->name, name) == 0);
    assert(record->directory == directory);
    assert(record->method == method);
    assert(record->size.lo == 0u && record->size.hi == 0u);
}

static void test_recursive_and_marked(void) {
    UzCreatePlan plan;
    UzCatalog catalog;
    UzBrowserPage page;
    UzZipRecord record;
    UzZipRecord scratch;
    char path[UZ_BROWSER_PATH_CAP];

    listing_mode = 0u;
    init_plan(&plan, &catalog, &page, &record, &scratch, path,
              "/USB1/SRC", 8u);
    assert(plan.error == UZ_CREATE_PLAN_OK);
    assert(uz_create_plan_seed(&plan, "LOOSE.PRG", 0u));
    assert(uz_create_plan_seed(&plan, "TOP", 1u));
    assert(uz_create_plan_build(&plan, "/USB1/DEST/ARCHIVE.ZIP"));
    assert(plan.files == 3u && plan.directories == 3u);
    assert(plan.seed_count == 2u && catalog.count == 6u);
    assert(list_calls == 3u);
    expect_record(&catalog, 0u, &record, "LOOSE.PRG", 0u, 8u);
    expect_record(&catalog, 1u, &record, "TOP/", 1u, 0u);
    expect_record(&catalog, 2u, &record, "TOP/A.BIN", 0u, 8u);
    expect_record(&catalog, 3u, &record, "TOP/SUB/", 1u, 0u);
    expect_record(&catalog, 4u, &record, "TOP/EMPTY/", 1u, 0u);
    expect_record(&catalog, 5u, &record, "TOP/SUB/B.BIN", 0u, 8u);
    assert(!uz_create_plan_build(&plan, "/USB1/DEST/OTHER.ZIP"));
    assert(plan.error == UZ_CREATE_PLAN_ERR_STATE);
}

static void test_paging_and_store(void) {
    UzCreatePlan plan;
    UzCatalog catalog;
    UzBrowserPage page;
    UzZipRecord record;
    UzZipRecord scratch;
    char path[UZ_BROWSER_PATH_CAP];

    listing_mode = 1u;
    generated_count = 30u;
    init_plan(&plan, &catalog, &page, &record, &scratch, path,
              "/USB1", 0u);
    assert(uz_create_plan_seed(&plan, "BIG", 1u));
    assert(uz_create_plan_build(&plan, "/USB1/BIG.ZIP"));
    assert(catalog.count == 31u && plan.files == 30u &&
           plan.directories == 1u && list_calls == 3u);
    expect_record(&catalog, 30u, &record, "BIG/F029", 0u, 0u);
}

static void test_output_safety(void) {
    UzCreatePlan plan;
    UzCatalog catalog;
    UzBrowserPage page;
    UzZipRecord record;
    UzZipRecord scratch;
    char path[UZ_BROWSER_PATH_CAP];

    listing_mode = 0u;
    init_plan(&plan, &catalog, &page, &record, &scratch, path,
              "/USB1/SRC", 8u);
    assert(uz_create_plan_seed(&plan, "TOP", 1u));
    assert(!uz_create_plan_build(&plan, "/usb1/src/top/OUT.ZIP"));
    assert(plan.error == UZ_CREATE_PLAN_ERR_OUTPUT_INSIDE && list_calls == 0u);

    init_plan(&plan, &catalog, &page, &record, &scratch, path,
              "/USB1/SRC", 8u);
    assert(uz_create_plan_seed(&plan, "OUT.ZIP", 0u));
    assert(!uz_create_plan_build(&plan, "/USB1/SRC/out.zip"));
    assert(plan.error == UZ_CREATE_PLAN_ERR_OUTPUT_INSIDE && list_calls == 0u);

    init_plan(&plan, &catalog, &page, &record, &scratch, path,
              "/USB1/SRC", 8u);
    assert(uz_create_plan_seed(&plan, "OUT.ZIP", 0u));
    assert(uz_create_plan_build(&plan, "/USB1/SRC/ARCHIVE.ZIP"));
    assert(catalog.count == 1u && list_calls == 0u);
}

static void test_rejections(void) {
    UzCreatePlan plan;
    UzCatalog catalog;
    UzBrowserPage page;
    UzZipRecord record;
    UzZipRecord scratch;
    char path[UZ_BROWSER_PATH_CAP];
    char long_name[127];
    char long_base[UZ_BROWSER_PATH_CAP];

    listing_mode = 0u;
    init_plan(&plan, &catalog, &page, &record, &scratch, path,
              "/USB1", 8u);
    assert(!uz_create_plan_build(&plan, "/USB1/A.ZIP"));
    assert(plan.error == UZ_CREATE_PLAN_ERR_EMPTY);

    init_plan(&plan, &catalog, &page, &record, &scratch, path,
              "/USB1", 8u);
    assert(!uz_create_plan_seed(&plan, "../BAD", 0u));
    assert(plan.error == UZ_CREATE_PLAN_ERR_NAME);

    init_plan(&plan, &catalog, &page, &record, &scratch, path,
              "/USB1", 8u);
    assert(uz_create_plan_seed(&plan, "A", 0u));
    assert(!uz_create_plan_seed(&plan, "a", 0u));
    assert(plan.error == UZ_CREATE_PLAN_ERR_CONFLICT);

    listing_mode = 2u;
    init_plan(&plan, &catalog, &page, &record, &scratch, path,
              "/USB1", 8u);
    assert(uz_create_plan_seed(&plan, "DUP", 1u));
    assert(!uz_create_plan_build(&plan, "/USB1/DUP.ZIP"));
    assert(plan.error == UZ_CREATE_PLAN_ERR_CONFLICT);

    listing_mode = 3u;
    init_plan(&plan, &catalog, &page, &record, &scratch, path,
              "/USB1", 8u);
    assert(uz_create_plan_seed(&plan, "BADDIR", 1u));
    assert(!uz_create_plan_build(&plan, "/USB1/BAD.ZIP"));
    assert(plan.error == UZ_CREATE_PLAN_ERR_NAME);

    memset(long_name, 'N', sizeof(long_name) - 1u);
    long_name[sizeof(long_name) - 1u] = 0;
    listing_mode = 4u;
    generated_count = 1u;
    init_plan(&plan, &catalog, &page, &record, &scratch, path,
              "/USB1", 8u);
    assert(uz_create_plan_seed(&plan, long_name, 1u));
    assert(!uz_create_plan_build(&plan, "/USB1/LONG.ZIP"));
    assert(plan.error == UZ_CREATE_PLAN_ERR_NAME);

    long_base[0] = '/';
    memset(long_base + 1u, 'A', 125u);
    long_base[126] = '/';
    memset(long_base + 127u, 'B', 125u);
    long_base[252] = 0;
    listing_mode = 0u;
    init_plan(&plan, &catalog, &page, &record, &scratch, path,
              long_base, 8u);
    assert(uz_create_plan_seed(&plan, "TOP", 1u));
    assert(!uz_create_plan_build(&plan, "/USB1/OUT.ZIP"));
    assert(plan.error == UZ_CREATE_PLAN_ERR_PATH);

    init_plan(&plan, &catalog, &page, &record, &scratch, path,
              "/USB1", 8u);
    assert(uz_create_plan_seed(&plan, "A", 0u));
    assert(!uz_create_plan_build(&plan, "/USB1/DEST/"));
    assert(plan.error == UZ_CREATE_PLAN_ERR_OUTPUT_PATH);
}

static void test_catalog_limit(void) {
    UzCreatePlan plan;
    UzCatalog catalog;
    UzBrowserPage page;
    UzZipRecord record;
    UzZipRecord scratch;
    char path[UZ_BROWSER_PATH_CAP];

    listing_mode = 1u;
    generated_count = 400u;
    init_plan(&plan, &catalog, &page, &record, &scratch, path,
              "/USB1", 8u);
    assert(uz_create_plan_seed(&plan, "FULL", 1u));
    assert(!uz_create_plan_build(&plan, "/USB1/FULL.ZIP"));
    assert(plan.error == UZ_CREATE_PLAN_ERR_FULL);
    assert(catalog.count == UZ_CATALOG_MAX_ENTRIES);
}

int main(void) {
    test_recursive_and_marked();
    test_paging_and_store();
    test_output_safety();
    test_rejections();
    test_catalog_limit();
    puts("uZIP create-plan tests passed");
    return 0;
}
