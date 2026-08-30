; uZIP streamed inflater, derived from Piotr Fusik's zlib6502 ca65 port.
;
; This is an altered source version, not the original software. The untouched
; cc65 snapshot and zlib license are retained in third_party/zlib6502. Keep
; changes conservative: upstream deliberately relies on register/flag state.

        .export         _uz_inflate6502_run

        .import         _uzif_refill_saved, _uzif_flush_saved
        .import         _uzif_finish_saved
        .import         _uzif_input_pointer, _uzif_input_end
        .import         _uzif_output_buffer, _uzif_output_count
        .import         _uzif_output_cap, _uzif_output_count32
        .import         _uzif_output_left, _uzif_output_bounded
        .import         _uzif_error_code, _uzif_ready
        .import         _uzif_dictionary_position
        .importzp       sreg, tmp1, tmp2, ptr1, ptr2, ptr3, ptr4

; --------------------------------------------------------------------------
;
; Constants
;

; Argument values for getBits.
GET_1_BIT           = $81
GET_2_BITS          = $82
GET_3_BITS          = $84
GET_4_BITS          = $88
GET_5_BITS          = $90
GET_6_BITS          = $a0
GET_7_BITS          = $c0

; Huffman trees.
TREE_SIZE           = 16
PRIMARY_TREE        = 0
DISTANCE_TREE       = TREE_SIZE

; Alphabet.
LENGTH_SYMBOLS      = 1+29+2    ; EOF, 29 length symbols, two unused symbols
DISTANCE_SYMBOLS    = 32        ; 30 valid plus two reserved wire symbols
CONTROL_SYMBOLS     = LENGTH_SYMBOLS+DISTANCE_SYMBOLS


; --------------------------------------------------------------------------
;
; Page zero
;

; Pointer to the compressed data.
inputPointer                :=  ptr1    ; 2 bytes

; Pointer to the uncompressed data.
outputPointer               :=  ptr2    ; 2 bytes

; Local variables.
; As far as there is no conflict, same memory locations are used
; for different variables.

inflateDynamic_symbol       :=  ptr3    ; 1 byte
inflateDynamic_lastLength   :=  ptr3+1  ; 1 byte
        .assert ptr4 = ptr3 + 2, error, "Need three bytes for inflateDynamic_tempCodes"
inflateDynamic_tempCodes    :=  ptr3+1  ; 3 bytes
inflateDynamic_allCodes     :=  inflateDynamic_tempCodes+1 ; 1 byte
inflateDynamic_primaryCodes :=  inflateDynamic_tempCodes+2 ; 1 byte
inflateCodes_sourcePointer  :=  ptr3    ; 2 bytes
inflateCodes_lengthMinus2   :=  ptr4    ; 1 byte
getBits_base                :=  sreg    ; 1 byte
getBit_buffer               :=  sreg+1  ; 1 byte
        .assert tmp2 = tmp1 + 1, error, "Need a two-byte output stage pointer"
outputStagePointer          :=  tmp1    ; 2 bytes


; --------------------------------------------------------------------------
;
; Code
;

.ifdef UZIP_READYOS_APP
        .segment        "INFLATE_CODE"
.else
        .segment        "JOB_CODE"
.endif

_uz_inflate6502_run:

        tsx
        stx     inflateEntryStack
        lda     _uzif_ready
        bne     inflateRun_ready
        lda     #9              ; UZ_INFLATE_STATE
        jmp     inflateAbortA
inflateRun_ready:

        lda     _uzif_input_pointer
        sta     inputPointer
        lda     _uzif_input_pointer+1
        sta     inputPointer+1
        lda     #<$3000
        sta     outputPointer
        lda     #>$3000
        sta     outputPointer+1
        lda     _uzif_output_buffer
        sta     outputStagePointer
        lda     _uzif_output_buffer+1
        sta     outputStagePointer+1

        ldy     #0
        sty     getBit_buffer

inflate_blockLoop:
; Get a bit of EOF and two bits of block type
;       ldy     #0
        sty     getBits_base
        lda     #GET_3_BITS
        jsr     getBits
        lsr     a
; A and Z contain block type, C contains EOF flag
; Save EOF flag
        php
        bne     inflateCompressed

; Decompress a stored block and validate LEN/NLEN explicitly.
        sty     getBit_buffer   ; ignore bits until byte boundary
        jsr     getWord
        stx     inflateStoredLength
        sta     inflateStoredLength+1
        jsr     getWord
        stx     inflateStoredInverse
        sta     inflateStoredInverse+1
        lda     inflateStoredLength
        eor     #$ff
        cmp     inflateStoredInverse
        bne     inflateStored_badLength
        lda     inflateStoredLength+1
        eor     #$ff
        cmp     inflateStoredInverse+1
        bne     inflateStored_badLength
inflateStored_copyLoop:
        lda     inflateStoredLength
        ora     inflateStoredLength+1
        beq     inflate_nextBlock
        jsr     getByte
        jsr     storeByte
        lda     inflateStoredLength
        bne     inflateStored_decLow
        dec     inflateStoredLength+1
inflateStored_decLow:
        dec     inflateStoredLength
        jmp     inflateStored_copyLoop
inflateStored_badLength:
        lda     #4              ; UZ_INFLATE_STORED_LENGTH
        jmp     inflateAbortA

; Block decompressed.
inflate_nextBlock:
        plp
        bcc     inflate_blockLoop

; Publish exact consumed input and dictionary position, then flush/validate.
        lda     inputPointer
        sta     _uzif_input_pointer
        lda     inputPointer+1
        sta     _uzif_input_pointer+1
        sec
        lda     outputPointer
        sbc     #<$3000
        sta     _uzif_dictionary_position
        lda     outputPointer+1
        sbc     #>$3000
        sta     _uzif_dictionary_position+1
        jsr     _uzif_finish_saved
        rts

inflateAbortA:
        sta     _uzif_error_code
inflateAbortExisting:
        ldx     inflateEntryStack
        txs
        lda     #0
        rts

inflateCompressed:
; Decompress a Huffman-coded data block
; A=1: fixed block, initialize with fixed codes
; A=2: dynamic block, start by clearing all code lengths
; A=3: invalid compressed data
        cmp     #3
        bne     inflateCompressed_validType
        lda     #3              ; UZ_INFLATE_BLOCK_TYPE
        jmp     inflateAbortA
inflateCompressed_validType:
        eor     #2

;       ldy     #0
inflateCompressed_setCodeLengths:
        tax
        beq     inflateCompressed_setLiteralCodeLength
; fixed Huffman literal codes:
; 144 8-bit codes
; 112 9-bit codes
        lda     #4
        cpy     #144
        rol     a
inflateCompressed_setLiteralCodeLength:
        sta     literalSymbolCodeLength,y
        beq     inflateCompressed_setControlCodeLength
; fixed Huffman control codes:
; 24 7-bit codes
;  6 8-bit codes
;  2 meaningless 8-bit codes
; 32 5-bit distance codes (the decoder rejects reserved symbols 30 and 31)
        lda     #5+DISTANCE_TREE
        cpy     #LENGTH_SYMBOLS
        bcs     inflateCompressed_setControlCodeLength
        cpy     #24
        adc     #$100+2-DISTANCE_TREE
inflateCompressed_setControlCodeLength:
        cpy     #CONTROL_SYMBOLS
        bcs     inflateCompressed_noControlSymbol
        sta     controlSymbolCodeLength,y
inflateCompressed_noControlSymbol:
        iny
        bne     inflateCompressed_setCodeLengths

        tax
        bne     inflateCodes
        jmp     inflateDynamic

; Decompress a block
inflateCodes:
        lda     controlSymbolCodeLength
        and     #$0f
        bne     inflateCodes_haveEnd
        lda     #5              ; UZ_INFLATE_TREE
        jmp     inflateAbortA
inflateCodes_haveEnd:
        jsr     buildHuffmanTree
inflateCodes_loop:
        jsr     fetchPrimaryCode
        bcs     inflateCodes_control
        jsr     storeByte
        jmp     inflateCodes_loop
inflateCodes_control:
        beq     inflate_nextBlock
        cpx     #30
        bcc     inflateCodes_validLength
        lda     #6              ; UZ_INFLATE_SYMBOL
        jmp     inflateAbortA
inflateCodes_validLength:
; Copy sequence from look-behind buffer
;       ldy     #0
        sty     getBits_base
        cmp     #9
        bcc     inflateCodes_setSequenceLength
        tya
;       lda     #0
        cpx     #1+28
        bcs     inflateCodes_setSequenceLength
        dex
        txa
        lsr     a
        ror     getBits_base
        inc     getBits_base
        lsr     a
        rol     getBits_base
        jsr     getAMinus1BitsMax8
;       sec
        adc     #0
inflateCodes_setSequenceLength:
        sta     inflateCodes_lengthMinus2
        ldx     #DISTANCE_TREE
        jsr     fetchCode
        cpx     #30
        bcc     inflateCodes_validDistanceSymbol
        lda     #7              ; UZ_INFLATE_DISTANCE
        jmp     inflateAbortA
inflateCodes_validDistanceSymbol:
        cmp     #4
        bcc     inflateCodes_setOffsetLowByte
        inc     getBits_base
        lsr     a
        jsr     getAMinus1BitsMax8
inflateCodes_setOffsetLowByte:
        eor     #$ff
        sta     inflateCodes_sourcePointer
        lda     getBits_base
        cpx     #10
        bcc     inflateCodes_setOffsetHighByte
        lda     getNPlus1Bits_mask-10,x
        jsr     getBits
        clc
inflateCodes_setOffsetHighByte:
        eor     #$ff
;       clc
        adc     outputPointer+1
        sta     inflateCodes_sourcePointer+1
        lda     inflateCodes_sourcePointer
        clc
        adc     outputPointer
        sta     inflateCodes_sourcePointer
        bcc     inflateCodes_sourceNoCarry
        inc     inflateCodes_sourcePointer+1
inflateCodes_sourceNoCarry:
        lda     inflateCodes_sourcePointer+1
        sec
        sbc     #>$3000
        and     #$7f
        clc
        adc     #>$3000
        sta     inflateCodes_sourcePointer+1
        sec
        lda     outputPointer
        sbc     inflateCodes_sourcePointer
        sta     inflateCodesDistance
        lda     outputPointer+1
        sbc     inflateCodes_sourcePointer+1
        and     #$7f
        sta     inflateCodesDistance+1
        ora     inflateCodesDistance
        bne     inflateCodes_distanceNonzero
        lda     #$80
        sta     inflateCodesDistance+1
inflateCodes_distanceNonzero:
        lda     _uzif_output_count32+2
        ora     _uzif_output_count32+3
        bne     inflateCodes_distanceReady
        lda     _uzif_output_count32+1
        cmp     inflateCodesDistance+1
        bcc     inflateCodes_badDistance
        bne     inflateCodes_distanceReady
        lda     _uzif_output_count32
        cmp     inflateCodesDistance
        bcs     inflateCodes_distanceReady
inflateCodes_badDistance:
        lda     #7              ; UZ_INFLATE_DISTANCE
        jmp     inflateAbortA
inflateCodes_distanceReady:
        jsr     copyByte
        jsr     copyByte
inflateCodes_copyByte:
        jsr     copyByte
        dec     inflateCodes_lengthMinus2
        bne     inflateCodes_copyByte
        jmp     inflateCodes_loop

inflateDynamic:
; Decompress a block reading Huffman trees first
;       ldy     #0
; numberOfPrimaryCodes = 257 + getBits(5)
; numberOfDistanceCodes = 1 + getBits(5)
; numberOfTemporaryCodes = 4 + getBits(4)
        ldx     #3
inflateDynamic_getHeader:
        lda     inflateDynamic_headerBits-1,x
        jsr     getBits
;       sec
        adc     inflateDynamic_headerBase-1,x
        sta     inflateDynamic_tempCodes-1,x
        dex
        bne     inflateDynamic_getHeader
        lda     inflateDynamic_primaryCodes
        cmp     #31
        bcc     inflateDynamic_primaryBounded
        jmp     inflateDynamic_badTree
inflateDynamic_primaryBounded:
        lda     inflateDynamic_allCodes
        cmp     #65             ; 32 primary controls + 1..32 distances
        bcc     inflateDynamic_headerBounded
        jmp     inflateDynamic_badTree
inflateDynamic_headerBounded:

; Get lengths of temporary codes in the order stored in inflateDynamic_tempSymbols
;       ldx     #0
inflateDynamic_getTempCodeLengths:
        lda     #GET_3_BITS
        jsr     getBits
        ldy     inflateDynamic_tempSymbols,x
        sta     literalSymbolCodeLength,y
        ldy     #0
        inx
        cpx     inflateDynamic_tempCodes
        bcc     inflateDynamic_getTempCodeLengths

; Build the tree for temporary codes
        jsr     buildHuffmanTree
        lda     #0
        sta     inflateDynamicHavePrevious

; Use temporary codes to get lengths of literal/length and distance codes
;       ldx     #0
;       sec
inflateDynamic_decodeLength:
; C=1: literal codes
; C=0: control codes
        stx     inflateDynamic_symbol
        php
; Fetch a temporary code
        jsr     fetchPrimaryCode
; Temporary code 0..15: put this length
        bmi     inflateDynamic_repeatLength
        ldy     #1
        sty     inflateDynamicHavePrevious
        dey
        jmp     inflateDynamic_storeLengths
inflateDynamic_repeatLength:
; Temporary code 16: repeat last length 3 + getBits(2) times
; Temporary code 17: put zero length 3 + getBits(3) times
; Temporary code 18: put zero length 11 + getBits(7) times
        tax
        cpx     #GET_2_BITS
        bne     inflateDynamic_repeatReady
        lda     inflateDynamicHavePrevious
        bne     inflateDynamic_repeatReady
        lda     #5
        jmp     inflateAbortA
inflateDynamic_repeatReady:
        txa                     ; previous-length validation may clobber A
        jsr     getBits
        cpx     #GET_3_BITS
        bcc     inflateDynamic_code16
        beq     inflateDynamic_code17
;       sec
        adc     #7
inflateDynamic_code17:
;       ldy     #0
        sty     inflateDynamic_lastLength
        pha
        lda     #1
        sta     inflateDynamicHavePrevious
        pla
inflateDynamic_code16:
        tay
        lda     inflateDynamic_lastLength
        iny
        iny
inflateDynamic_storeLengths:
        iny
        plp
        ldx     inflateDynamic_symbol
inflateDynamic_storeLength:
        bcc     inflateDynamic_controlSymbolCodeLength
        sta     literalSymbolCodeLength,x
        inx
        cpx     #1
inflateDynamic_storeNext:
        dey
        bne     inflateDynamic_storeLength
        sta     inflateDynamic_lastLength
        beq     inflateDynamic_decodeLength ; jmp
inflateDynamic_controlSymbolCodeLength:
        cpx     inflateDynamic_primaryCodes
        bcc     inflateDynamic_storeControl
; the code lengths we skip here were zero-initialized
; in inflateCompressed_setControlCodeLength
        bne     inflateDynamic_noStartDistanceTree
        ldx     #LENGTH_SYMBOLS
inflateDynamic_noStartDistanceTree:
        ora     #DISTANCE_TREE
inflateDynamic_storeControl:
        sta     controlSymbolCodeLength,x
        inx
        cpx     inflateDynamic_allCodes
        bcc     inflateDynamic_storeNext
        dey
        bne     inflateDynamic_badTree
        jmp     inflateCodes
inflateDynamic_badTree:
        lda     #5              ; UZ_INFLATE_TREE
        jmp     inflateAbortA

; Build Huffman trees basing on code lengths (in bits)
; stored in the *SymbolCodeLength arrays
buildHuffmanTree:
; Clear nBitCode_literalCount, nBitCode_controlCount
        tya
;       lda     #0
buildHuffmanTree_clear:
        sta     nBitCode_clearFrom,y
        iny
        bne     buildHuffmanTree_clear
; Count number of codes of each length
;       ldy     #0
buildHuffmanTree_countCodeLengths:
        ldx     literalSymbolCodeLength,y
        inc     nBitCode_literalCount,x
        bne     buildHuffmanTree_notAllLiterals
        stx     allLiteralsCodeLength
buildHuffmanTree_notAllLiterals:
        cpy     #CONTROL_SYMBOLS
        bcs     buildHuffmanTree_noControlSymbol
        ldx     controlSymbolCodeLength,y
        inc     nBitCode_controlCount,x
buildHuffmanTree_noControlSymbol:
        iny
        bne     buildHuffmanTree_countCodeLengths
        lda     ptr3
        sta     treeSavedPtr
        lda     ptr3+1
        sta     treeSavedPtr+1
        lda     ptr4
        sta     treeSavedPtr+2
        lda     ptr4+1
        sta     treeSavedPtr+3
        jsr     validatePrimaryHuffmanCounts
        lda     #<(nBitCode_controlCount+DISTANCE_TREE)
        sta     ptr3
        lda     #>(nBitCode_controlCount+DISTANCE_TREE)
        sta     ptr3+1
        jsr     validateHuffmanCounts
        lda     treeSavedPtr
        sta     ptr3
        lda     treeSavedPtr+1
        sta     ptr3+1
        lda     treeSavedPtr+2
        sta     ptr4
        lda     treeSavedPtr+3
        sta     ptr4+1
; Calculate offsets of symbols sorted by code length
        ldy     #0              ; validator exits at TREE_SIZE, builder needs 0
        lda     #0
        ldx     #$100-4*TREE_SIZE
buildHuffmanTree_calculateOffsets:
        sta     nBitCode_literalOffset+4*TREE_SIZE-$100,x
        clc
        adc     nBitCode_literalCount+4*TREE_SIZE-$100,x
        inx
        bne     buildHuffmanTree_calculateOffsets
; Put symbols in their place in the sorted array
;       ldy     #0
buildHuffmanTree_assignCode:
        tya
        ldx     literalSymbolCodeLength,y
        ldy     nBitCode_literalOffset,x
        inc     nBitCode_literalOffset,x
        sta     codeToLiteralSymbol,y
        tay
        cpy     #CONTROL_SYMBOLS
        bcs     buildHuffmanTree_noControlSymbol2
        ldx     controlSymbolCodeLength,y
        ldy     nBitCode_controlOffset,x
        inc     nBitCode_controlOffset,x
        sta     codeToControlSymbol,y
        tay
buildHuffmanTree_noControlSymbol2:
        iny
        bne     buildHuffmanTree_assignCode
        rts

; Literal bytes and length/EOB symbols share one canonical tree even though
; the compact decoder keeps their counts in adjacent arrays. Validate their
; combined code space; checking either half alone cannot detect every
; oversubscribed primary tree.
validatePrimaryHuffmanCounts:
        lda     #1
        sta     treeCodeSpace
        lda     #0
        sta     treeCodeSpace+1
        ldy     #1
validatePrimaryHuffmanCounts_loop:
        asl     treeCodeSpace
        rol     treeCodeSpace+1
        sec
        lda     treeCodeSpace
        sbc     nBitCode_literalCount,y
        sta     treeCodeSpace
        lda     treeCodeSpace+1
        sbc     #0
        bcc     validateHuffmanCounts_bad
        sta     treeCodeSpace+1
        sec
        lda     treeCodeSpace
        sbc     nBitCode_controlCount,y
        sta     treeCodeSpace
        lda     treeCodeSpace+1
        sbc     #0
        bcc     validateHuffmanCounts_bad
        sta     treeCodeSpace+1
        iny
        cpy     #TREE_SIZE
        bne     validatePrimaryHuffmanCounts_loop
        rts

validateHuffmanCounts:
        lda     #1
        sta     treeCodeSpace
        lda     #0
        sta     treeCodeSpace+1
        ldy     #1
validateHuffmanCounts_loop:
        asl     treeCodeSpace
        rol     treeCodeSpace+1
        sec
        lda     treeCodeSpace
        sbc     (ptr3),y
        sta     treeCodeSpace
        lda     treeCodeSpace+1
        sbc     #0
        bcc     validateHuffmanCounts_bad
        sta     treeCodeSpace+1
        iny
        cpy     #TREE_SIZE
        bne     validateHuffmanCounts_loop
        rts
validateHuffmanCounts_bad:
        lda     #5
        jmp     inflateAbortA

; Read Huffman code using the primary tree
fetchPrimaryCode:
        ldx     #PRIMARY_TREE
; Read a code from input using the tree specified in X.
; Return low byte of this code in A.
; Return C flag reset for literal code, set for length code.
fetchCode:
;       ldy     #0
        tya
fetchCode_nextBit:
        jsr     getBit
        rol     a
        inx
        cpx     #DISTANCE_TREE
        beq     fetchCode_tooLong
        cpx     #2*TREE_SIZE
        beq     fetchCode_tooLong
        bcs     fetchCode_ge256
; are all 256 literal codes of this length?
        cpx     allLiteralsCodeLength
        beq     fetchCode_allLiterals
; is it literal code of length X?
        sec
        sbc     nBitCode_literalCount,x
        bcs     fetchCode_notLiteral
; literal code
;       clc
        adc     nBitCode_literalOffset,x
        tax
        lda     codeToLiteralSymbol,x
fetchCode_allLiterals:
        clc
        rts
; code >= 256, must be control
fetchCode_ge256:
;       sec
        sbc     nBitCode_literalCount,x
        sec
; is it control code of length X?
fetchCode_notLiteral:
;       sec
        sbc     nBitCode_controlCount,x
        bcs     fetchCode_nextBit
; control code
;       clc
        adc     nBitCode_controlOffset,x
        tax
        lda     codeToControlSymbol,x
        and     #$1f    ; make distance symbols zero-based
        tax
;       sec
        rts
fetchCode_tooLong:
        lda     #6
        jmp     inflateAbortA

; Read A minus 1 bits, but no more than 8
getAMinus1BitsMax8:
        rol     getBits_base
        tax
        cmp     #9
        bcs     getByte
        lda     getNPlus1Bits_mask-2,x
getBits:
        jsr     getBits_loop
getBits_normalizeLoop:
        lsr     getBits_base
        ror     a
        bcc     getBits_normalizeLoop
        rts

; Read 16 bits
getWord:
        jsr     getByte
        tax
; Read 8 bits
getByte:
        lda     #$80
getBits_loop:
        jsr     getBit
        ror     a
        bcc     getBits_loop
        rts

; Read one bit, return in the C flag
getBit:
        lsr     getBit_buffer
        bne     getBit_return
        pha
        lda     inputPointer
        cmp     _uzif_input_end
        bne     getBit_haveByte
        lda     inputPointer+1
        cmp     _uzif_input_end+1
        bne     getBit_haveByte
        jsr     _uzif_refill_saved
        bne     getBit_refilled
        pla
        jmp     inflateAbortExisting
getBit_refilled:
        lda     _uzif_input_pointer
        sta     inputPointer
        lda     _uzif_input_pointer+1
        sta     inputPointer+1
getBit_haveByte:
        ldy     #0
        lda     (inputPointer),y
        inc     inputPointer
        bne     getBit_samePage
        inc     inputPointer+1
getBit_samePage:
        sec
        ror     a
        sta     getBit_buffer
        pla
getBit_return:
        rts

; Copy a previously written byte
copyByte:
        ldy     #0
        lda     (inflateCodes_sourcePointer),y
        pha
        inc     inflateCodes_sourcePointer
        bne     copyByte_ready
        inc     inflateCodes_sourcePointer+1
        lda     inflateCodes_sourcePointer+1
        cmp     #>$B000
        bne     copyByte_ready
        lda     #>$3000
        sta     inflateCodes_sourcePointer+1
copyByte_ready:
        pla
; Write a byte
storeByte:
        sta     emittedByte
        lda     _uzif_output_bounded
        beq     storeByte_haveRoom
        lda     _uzif_output_left
        ora     _uzif_output_left+1
        ora     _uzif_output_left+2
        ora     _uzif_output_left+3
        bne     storeByte_decrementLeft
        lda     #10
        jmp     inflateAbortA
storeByte_decrementLeft:
        sec
        lda     _uzif_output_left
        sbc     #1
        sta     _uzif_output_left
        lda     _uzif_output_left+1
        sbc     #0
        sta     _uzif_output_left+1
        lda     _uzif_output_left+2
        sbc     #0
        sta     _uzif_output_left+2
        lda     _uzif_output_left+3
        sbc     #0
        sta     _uzif_output_left+3
storeByte_haveRoom:
        ldy     #0
        lda     emittedByte
        sta     (outputPointer),y
        sta     (outputStagePointer),y
        inc     outputPointer
        bne     storeByte_stage
        inc     outputPointer+1
        lda     outputPointer+1
        cmp     #>$B000
        bne     storeByte_stage
        lda     #>$3000
        sta     outputPointer+1
storeByte_stage:
        inc     outputStagePointer
        bne     storeByte_counts
        inc     outputStagePointer+1
storeByte_counts:
        inc     _uzif_output_count
        bne     storeByte_count32
        inc     _uzif_output_count+1
storeByte_count32:
        inc     _uzif_output_count32
        bne     storeByte_checkFlush
        inc     _uzif_output_count32+1
        bne     storeByte_checkFlush
        inc     _uzif_output_count32+2
        bne     storeByte_checkFlush
        inc     _uzif_output_count32+3
storeByte_checkFlush:
        lda     _uzif_output_count
        cmp     _uzif_output_cap
        bne     storeByte_return
        lda     _uzif_output_count+1
        cmp     _uzif_output_cap+1
        bne     storeByte_return
        jsr     _uzif_flush_saved
        bne     storeByte_resetStage
        jmp     inflateAbortExisting
storeByte_resetStage:
        lda     _uzif_output_buffer
        sta     outputStagePointer
        lda     _uzif_output_buffer+1
        sta     outputStagePointer+1
storeByte_return:
        clc
        rts


; --------------------------------------------------------------------------
;
; Constant data
;

.ifdef UZIP_READYOS_APP
        .segment        "INFLATE_RODATA"
.else
        .segment        "JOB_RODATA"
.endif

getNPlus1Bits_mask:
        .byte   GET_1_BIT,GET_2_BITS,GET_3_BITS,GET_4_BITS,GET_5_BITS,GET_6_BITS,GET_7_BITS

inflateDynamic_tempSymbols:
        .byte   GET_2_BITS,GET_3_BITS,GET_7_BITS,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15

inflateDynamic_headerBits:
        .byte   GET_4_BITS,GET_5_BITS,GET_5_BITS
inflateDynamic_headerBase:
        .byte   3,LENGTH_SYMBOLS,0


; --------------------------------------------------------------------------
;
; Uninitialised data
;

.ifdef UZIP_READYOS_APP
        .segment        "INFLATE_BSS"
.else
        .segment        "JOB_BSS"
.endif

inflateEntryStack:
        .res    1
emittedByte:
        .res    1
inflateStoredLength:
        .res    2
inflateStoredInverse:
        .res    2
inflateCodesDistance:
        .res    2
inflateDynamicHavePrevious:
        .res    1
treeSavedPtr:
        .res    4
treeCodeSpace:
        .res    2

; Data for building trees.

literalSymbolCodeLength:
        .res    256
controlSymbolCodeLength:
        .res    CONTROL_SYMBOLS

; Huffman trees.

nBitCode_clearFrom:
nBitCode_literalCount:
        .res    2*TREE_SIZE
nBitCode_controlCount:
        .res    2*TREE_SIZE
nBitCode_literalOffset:
        .res    2*TREE_SIZE
nBitCode_controlOffset:
        .res    2*TREE_SIZE
allLiteralsCodeLength:
        .res    1

codeToLiteralSymbol:
        .res    256
codeToControlSymbol:
        .res    CONTROL_SYMBOLS
