#ifndef UZ_BROWSER_H
#define UZ_BROWSER_H

#include "uz_dos.h"

#define UZ_BROWSER_ROWS       14u
#define UZ_BROWSER_NAME_CAP   128u
#define UZ_BROWSER_PATH_CAP   256u
#define UZ_BROWSER_SHOW_ALL   0u
#define UZ_BROWSER_FOLDERS    1u

typedef struct {
    char name[UZ_BROWSER_NAME_CAP];
    unsigned char attributes;
    unsigned char directory;
    unsigned char unusable;
} UzBrowserEntry;

typedef struct {
    UzBrowserEntry entries[UZ_BROWSER_ROWS];
    unsigned int total;
    unsigned char count;
    unsigned char page;
    unsigned char more;
    unsigned char unusable;
} UzBrowserPage;

/* READ_DIR is a fully drained UCI multiblock transaction. Only one listing
 * may be active because the transport callback does not carry a context. */
unsigned char uz_browser_list(UzDos *dos, const char *path,
                              unsigned char page,
                              unsigned char folders_only,
                              UzBrowserPage *out);

/* Position and open an absolute folder. Ultimate firmware reports a valid
 * empty directory with status 01 rather than the usual 00. */
unsigned char uz_browser_open_folder(UzDos *dos, const char *path);

/* Mutate one canonical absolute host path. Root is represented as "/" and
 * every non-root path has no trailing slash. Names are raw Ultimate ASCII. */
unsigned char uz_browser_enter(char *path, unsigned int capacity,
                               const char *name);
unsigned char uz_browser_parent(char *path);
/* Split a non-root current folder into its canonical parent and leaf.  This
 * lets Create treat an unmarked browser folder as one recursive seed without
 * retaining a second filesystem model. */
unsigned char uz_browser_split_current(const char *path,
                                       char *parent,
                                       unsigned int parent_capacity,
                                       char *leaf,
                                       unsigned int leaf_capacity);
void uz_browser_display(char *destination, unsigned int capacity,
                        const char *host_ascii);

#endif
