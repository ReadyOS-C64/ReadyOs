#ifndef XUZREU_DIAG_H
#define XUZREU_DIAG_H

/* Diagnostic-only entry points. The physical probe is a normal ReadyOS uZIP
 * build so every REU bank comes from the launcher-owned allocation schema. */
unsigned char xuzreu_diag_result_present(void);
void xuzreu_diag_run(unsigned char package_bank);

#endif
