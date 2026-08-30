#ifndef UZ_PACK_H
#define UZ_PACK_H

/* Linker-generated cold-load metadata, returned as ordinary 16-bit values so
 * C never guesses packed offsets or the $B000 phase run address. */
unsigned int uz_pack_job_load(void);
unsigned int uz_pack_job_size(void);
unsigned int uz_pack_job_run(void);
unsigned int uz_pack_inflate_load(void);
unsigned int uz_pack_inflate_size(void);
unsigned int uz_pack_inflate_run(void);
unsigned int uz_pack_inflate_bss_run(void);
unsigned int uz_pack_inflate_bss_size(void);
unsigned int uz_pack_zip_read_load(void);
unsigned int uz_pack_zip_read_size(void);
unsigned int uz_pack_zip_read_run(void);
unsigned int uz_pack_deflate_match_load(void);
unsigned int uz_pack_deflate_match_size(void);
unsigned int uz_pack_deflate_match_run(void);
unsigned int uz_pack_deflate_emit_load(void);
unsigned int uz_pack_deflate_emit_size(void);
unsigned int uz_pack_deflate_emit_run(void);
unsigned int uz_pack_deflate_coord_load(void);
unsigned int uz_pack_deflate_coord_size(void);
unsigned int uz_pack_deflate_coord_run(void);
unsigned int uz_pack_deflate_coord_bss_run(void);
unsigned int uz_pack_deflate_coord_bss_size(void);
#ifdef UZIP_CREATE_COORD
unsigned int uz_pack_create_coord_load(void);
unsigned int uz_pack_create_coord_size(void);
unsigned int uz_pack_create_coord_run(void);
unsigned int uz_pack_create_coord_entry(void);
#endif

#endif
