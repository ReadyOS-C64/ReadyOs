#ifndef XUZDEFLATE_DIAG_H
#define XUZDEFLATE_DIAG_H

/* Diagnostic UI entry. The actual compressor coordinator is invoked through
 * the resident snapshot/restore trampoline at its packed run address. */
void xuzdeflate_diag_run(unsigned char package_bank);

#endif
