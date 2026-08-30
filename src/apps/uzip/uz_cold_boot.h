#ifndef UZ_COLD_BOOT_H
#define UZ_COLD_BOOT_H

/* Cold-only entry copied by the resident startup bridge to $A000. It expands
 * the idle UI from the launcher-owned package bank into $3000-$9FFF. */
unsigned char uz_cold_boot_run(void);

#endif
