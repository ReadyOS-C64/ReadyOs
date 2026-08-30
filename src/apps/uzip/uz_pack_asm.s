; Bridge linker-generated packed-segment symbols to simple cc65 C functions.

        .export _uz_pack_job_load, _uz_pack_job_size, _uz_pack_job_run
        .export _uz_pack_inflate_load, _uz_pack_inflate_size
        .export _uz_pack_inflate_run, _uz_pack_inflate_bss_run
        .export _uz_pack_inflate_bss_size
        .export _uz_pack_zip_read_load, _uz_pack_zip_read_size
        .export _uz_pack_zip_read_run
        .export _uz_pack_deflate_match_load, _uz_pack_deflate_match_size
        .export _uz_pack_deflate_match_run
        .export _uz_pack_deflate_emit_load, _uz_pack_deflate_emit_size
        .export _uz_pack_deflate_emit_run
        .export _uz_pack_deflate_coord_load, _uz_pack_deflate_coord_size
        .export _uz_pack_deflate_coord_run
        .export _uz_pack_deflate_coord_bss_run
        .export _uz_pack_deflate_coord_bss_size
.ifdef UZIP_CREATE_COORD
        .export _uz_pack_create_coord_load, _uz_pack_create_coord_size
        .export _uz_pack_create_coord_run, _uz_pack_create_coord_entry
.endif
        .import __JOB_CODE_LOAD__, __JOB_CODE_RUN__
        .import __JOB_RODATA_LOAD__, __JOB_RODATA_SIZE__
        .import __DEFLATE_MATCH_CODE_LOAD__, __DEFLATE_MATCH_CODE_RUN__
        .import __DEFLATE_MATCH_RODATA_LOAD__, __DEFLATE_MATCH_RODATA_SIZE__
        .import __DEFLATE_EMIT_CODE_LOAD__, __DEFLATE_EMIT_CODE_RUN__
        .import __DEFLATE_EMIT_RODATA_LOAD__, __DEFLATE_EMIT_RODATA_SIZE__
        .import __DEFLATE_COORD_CODE_LOAD__, __DEFLATE_COORD_CODE_RUN__
        .import __DEFLATE_COORD_RODATA_LOAD__, __DEFLATE_COORD_RODATA_SIZE__
        .import __DEFLATE_COORD_BSS_RUN__, __DEFLATE_COORD_BSS_SIZE__
        .import __INFLATE_CODE_LOAD__, __INFLATE_CODE_RUN__
        .import __INFLATE_RODATA_LOAD__, __INFLATE_RODATA_SIZE__
        .import __INFLATE_BSS_RUN__, __INFLATE_BSS_SIZE__
        .import __ZIP_READ_CODE_LOAD__, __ZIP_READ_CODE_SIZE__
        .import __ZIP_READ_CODE_RUN__
.ifdef UZIP_CREATE_COORD
        .import __CREATE_COORD_CODE_LOAD__, __CREATE_COORD_CODE_RUN__
        .import __CREATE_COORD_RODATA_LOAD__, __CREATE_COORD_RODATA_SIZE__
        .import _uz_create_job_entry
.endif

; Cold package seeding is an idle-only operation; keep this bridge out of the
; saturated job-safe resident core.
        .segment "UI_CODE"

_uz_pack_job_load:
        lda #<__JOB_CODE_LOAD__
        ldx #>__JOB_CODE_LOAD__
        rts

_uz_pack_job_size:
        lda #<(__JOB_RODATA_LOAD__ + __JOB_RODATA_SIZE__ - __JOB_CODE_LOAD__)
        ldx #>(__JOB_RODATA_LOAD__ + __JOB_RODATA_SIZE__ - __JOB_CODE_LOAD__)
        rts

_uz_pack_job_run:
        lda #<__JOB_CODE_RUN__
        ldx #>__JOB_CODE_RUN__
        rts

_uz_pack_inflate_load:
        lda #<__INFLATE_CODE_LOAD__
        ldx #>__INFLATE_CODE_LOAD__
        rts

_uz_pack_inflate_size:
        lda #<(__INFLATE_RODATA_LOAD__ + __INFLATE_RODATA_SIZE__ - __INFLATE_CODE_LOAD__)
        ldx #>(__INFLATE_RODATA_LOAD__ + __INFLATE_RODATA_SIZE__ - __INFLATE_CODE_LOAD__)
        rts

_uz_pack_inflate_run:
        lda #<__INFLATE_CODE_RUN__
        ldx #>__INFLATE_CODE_RUN__
        rts

_uz_pack_inflate_bss_run:
        lda #<__INFLATE_BSS_RUN__
        ldx #>__INFLATE_BSS_RUN__
        rts

_uz_pack_inflate_bss_size:
        lda #<__INFLATE_BSS_SIZE__
        ldx #>__INFLATE_BSS_SIZE__
        rts

_uz_pack_zip_read_load:
        lda #<__ZIP_READ_CODE_LOAD__
        ldx #>__ZIP_READ_CODE_LOAD__
        rts

_uz_pack_zip_read_size:
        lda #<__ZIP_READ_CODE_SIZE__
        ldx #>__ZIP_READ_CODE_SIZE__
        rts

_uz_pack_zip_read_run:
        lda #<__ZIP_READ_CODE_RUN__
        ldx #>__ZIP_READ_CODE_RUN__
        rts

_uz_pack_deflate_match_load:
        lda #<__DEFLATE_MATCH_CODE_LOAD__
        ldx #>__DEFLATE_MATCH_CODE_LOAD__
        rts

_uz_pack_deflate_match_size:
        lda #<(__DEFLATE_MATCH_RODATA_LOAD__ + __DEFLATE_MATCH_RODATA_SIZE__ - __DEFLATE_MATCH_CODE_LOAD__)
        ldx #>(__DEFLATE_MATCH_RODATA_LOAD__ + __DEFLATE_MATCH_RODATA_SIZE__ - __DEFLATE_MATCH_CODE_LOAD__)
        rts

_uz_pack_deflate_match_run:
        lda #<__DEFLATE_MATCH_CODE_RUN__
        ldx #>__DEFLATE_MATCH_CODE_RUN__
        rts

_uz_pack_deflate_emit_load:
        lda #<__DEFLATE_EMIT_CODE_LOAD__
        ldx #>__DEFLATE_EMIT_CODE_LOAD__
        rts

_uz_pack_deflate_emit_size:
        lda #<(__DEFLATE_EMIT_RODATA_LOAD__ + __DEFLATE_EMIT_RODATA_SIZE__ - __DEFLATE_EMIT_CODE_LOAD__)
        ldx #>(__DEFLATE_EMIT_RODATA_LOAD__ + __DEFLATE_EMIT_RODATA_SIZE__ - __DEFLATE_EMIT_CODE_LOAD__)
        rts

_uz_pack_deflate_emit_run:
        lda #<__DEFLATE_EMIT_CODE_RUN__
        ldx #>__DEFLATE_EMIT_CODE_RUN__
        rts

_uz_pack_deflate_coord_load:
        lda #<__DEFLATE_COORD_CODE_LOAD__
        ldx #>__DEFLATE_COORD_CODE_LOAD__
        rts

_uz_pack_deflate_coord_size:
        lda #<(__DEFLATE_COORD_RODATA_LOAD__ + __DEFLATE_COORD_RODATA_SIZE__ - __DEFLATE_COORD_CODE_LOAD__)
        ldx #>(__DEFLATE_COORD_RODATA_LOAD__ + __DEFLATE_COORD_RODATA_SIZE__ - __DEFLATE_COORD_CODE_LOAD__)
        rts

_uz_pack_deflate_coord_run:
        lda #<__DEFLATE_COORD_CODE_RUN__
        ldx #>__DEFLATE_COORD_CODE_RUN__
        rts

_uz_pack_deflate_coord_bss_run:
        lda #<__DEFLATE_COORD_BSS_RUN__
        ldx #>__DEFLATE_COORD_BSS_RUN__
        rts

_uz_pack_deflate_coord_bss_size:
        lda #<__DEFLATE_COORD_BSS_SIZE__
        ldx #>__DEFLATE_COORD_BSS_SIZE__
        rts

.ifdef UZIP_CREATE_COORD
_uz_pack_create_coord_load:
        lda #<__CREATE_COORD_CODE_LOAD__
        ldx #>__CREATE_COORD_CODE_LOAD__
        rts

_uz_pack_create_coord_size:
        lda #<(__CREATE_COORD_RODATA_LOAD__ + __CREATE_COORD_RODATA_SIZE__ - __CREATE_COORD_CODE_LOAD__)
        ldx #>(__CREATE_COORD_RODATA_LOAD__ + __CREATE_COORD_RODATA_SIZE__ - __CREATE_COORD_CODE_LOAD__)
        rts

_uz_pack_create_coord_run:
        lda #<__CREATE_COORD_CODE_RUN__
        ldx #>__CREATE_COORD_CODE_RUN__
        rts

_uz_pack_create_coord_entry:
        lda #<_uz_create_job_entry
        ldx #>_uz_create_job_entry
        rts
.endif
