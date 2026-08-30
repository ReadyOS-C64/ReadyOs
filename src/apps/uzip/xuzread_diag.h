#ifndef XUZREAD_DIAG_H
#define XUZREAD_DIAG_H

/* Focused physical parser/catalog diagnostic. It performs no extraction
 * mutation and uses only ReadyOS-owned REU banks. */
void xuzread_diag_run(unsigned char package_bank);

#endif
