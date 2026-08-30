#ifndef UZ_DOS_H
#define UZ_DOS_H

#include "uz_u32.h"
#include "uz_uci.h"

#define UZ_DOS_TARGET_READ  1u
#define UZ_DOS_TARGET_WRITE 2u

#define UZ_DOS_OPEN_READ       0x01u
#define UZ_DOS_OPEN_WRITE_NEW  0x06u
#define UZ_DOS_REU_LOAD        0x21u
#define UZ_DOS_REU_SAVE        0x22u

#define UZ_DOS_PATH_CAP      256u
#define UZ_DOS_COMPONENT_CAP 128u
#define UZ_DOS_QUEUE_MAX     896u
/* Firmware 3.14 corrupts DOS WRITE commands whose four-byte header plus
 * payload crosses 512 bytes, despite the larger transport command FIFO. */
#define UZ_DOS_WRITE_MAX     508u

typedef struct {
    unsigned char target;
    unsigned char *command;
    unsigned int command_cap;
    unsigned char *data;
    unsigned int data_cap;
    unsigned char *status;
    unsigned int status_cap;
    UzUciTransfer transfer;
    char message[40];
    unsigned char file_open;
} UzDos;

void uz_dos_init(UzDos *dos, unsigned char target,
                 unsigned char *command, unsigned int command_cap,
                 unsigned char *data, unsigned int data_cap,
                 unsigned char *status, unsigned int status_cap);
unsigned char uz_dos_identify(UzDos *dos);
unsigned char uz_dos_change_path(UzDos *dos, const char *path);
unsigned char uz_dos_change_absolute(UzDos *dos, const char *path);
unsigned char uz_dos_get_path(UzDos *dos);
unsigned char uz_dos_create_dir(UzDos *dos, const char *name);
unsigned char uz_dos_open_dir(UzDos *dos);
unsigned char uz_dos_read_dir(UzDos *dos, UzUciBlockHandler handler);
unsigned char uz_dos_open(UzDos *dos, const char *name, unsigned char flags);
unsigned char uz_dos_close(UzDos *dos);
/* Phase-local close for a coordinator which has intentionally erased UI code.
 * Normal UI/preflight/commit code must use uz_dos_close(). */
unsigned char uz_dos_job_close(UzDos *dos);
int uz_dos_read(UzDos *dos, void *destination, unsigned int length);
unsigned char uz_dos_write(UzDos *dos, const void *source, unsigned int length);
/* LOAD_REU reads from the open Ultimate file into a ReadyOS-owned physical
 * REU bank; SAVE_REU writes the owned bank range to the open file. Both return
 * the firmware's exact transferred count and never cross a 64K bank edge. */
unsigned char uz_dos_reu_transfer(UzDos *dos, unsigned char command,
                                  unsigned char bank, unsigned int offset,
                                  unsigned int length,
                                  unsigned int *transferred);
#define uz_dos_load_reu(dos, bank, offset, length, transferred) \
    uz_dos_reu_transfer((dos), UZ_DOS_REU_LOAD, (bank), (offset), (length), \
                        (transferred))
#define uz_dos_save_reu(dos, bank, offset, length, transferred) \
    uz_dos_reu_transfer((dos), UZ_DOS_REU_SAVE, (bank), (offset), (length), \
                        (transferred))
unsigned char uz_dos_seek(UzDos *dos, const UzU32 *offset);
unsigned char uz_dos_file_info(UzDos *dos, UzU32 *size);
unsigned char uz_dos_file_stat(UzDos *dos, const char *name, UzU32 *size);
unsigned char uz_dos_delete(UzDos *dos, const char *name);
unsigned char uz_dos_rename(UzDos *dos, const char *old_name,
                            const char *new_name);
const char *uz_dos_message(const UzDos *dos);

#endif
