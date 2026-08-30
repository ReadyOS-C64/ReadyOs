/*
 * reu_mgr.h - REU Memory Manager for Ready OS
 * Manages 256 REU banks (16MB) with its authoritative allocation table in the
 * ReadyOS bank at REUCB_BANK_TYPE_OFF.
 */

#ifndef REU_MGR_H
#define REU_MGR_H

/* Bank type constants */
#define REU_FREE       0
#define REU_APP_STATE  1
#define REU_CLIPBOARD  2
#define REU_APP_ALLOC  3
#define REU_RESERVED   4
#define REU_RS_CACHE   5
#define REU_SKIPPED    6
#define REU_GLOBAL     7
#define REU_LAUNCHER   9
#define REU_UNAVAIL    10
#define REU_RS_SCRATCH 13
#define REU_RB_CORE    14
#define REU_RB_CODE    15
#define REU_UZIP_PACKAGE 16

#define REU_TOTAL_BANKS  256

/* Logical app snapshot tokens start at 1. Physical Skip is the combined
 * ReadyOS global/control bank and launcher snapshot; dynamic allocation starts
 * at the immediately following physical bank. */
#define REU_FIRST_DYNAMIC 1

/* $C83B contains the direct physical ReadyOS-bank number (Skip). */
#define SHIM_READYOS_BANK ((unsigned char*)0xC83B)
#define SHIM_REU_BANK_SKIP SHIM_READYOS_BANK /* deprecated source alias */
#define REU_READYOS_GLOBAL_PHYSICAL() ((unsigned char)(*SHIM_READYOS_BANK))
#define REU_LAUNCHER_PHYSICAL()       REU_READYOS_GLOBAL_PHYSICAL()
#define REU_FIRST_DYNAMIC_PHYSICAL()  ((unsigned char)(*SHIM_READYOS_BANK + 1u))

/* Initialize REU manager (safe to call multiple times) */
void reu_mgr_init(void);

/* Allocate a free bank, mark with given type. Returns bank or 0xFF if full */
unsigned char reu_alloc_bank(unsigned char type);

/* Free a bank (marks as FREE) */
void reu_free_bank(unsigned char bank);

/* Get the type of a bank */
unsigned char reu_bank_type(unsigned char bank);

/* Count free banks */
unsigned char reu_count_free(void);

/* Count banks of a given type */
unsigned char reu_count_type(unsigned char type);

/* DMA helpers - transfer between C64 memory and REU */
void reu_dma_stash(unsigned int c64_addr, unsigned char bank,
                   unsigned int reu_offset, unsigned int length);
void reu_dma_fetch(unsigned int c64_addr, unsigned char bank,
                   unsigned int reu_offset, unsigned int length);

/* ReadyOS-bank metadata access.  These routines use the shim's resident
 * one-byte scratch for byte operations, so callers do not need a BSS mirror. */
unsigned char readyos_bank_read_byte(unsigned int offset);
void readyos_bank_write_byte(unsigned int offset, unsigned char value);
void readyos_bank_read(unsigned int offset, void *dst, unsigned int length);
void readyos_bank_write(unsigned int offset, const void *src, unsigned int length);

#endif /* REU_MGR_H */
