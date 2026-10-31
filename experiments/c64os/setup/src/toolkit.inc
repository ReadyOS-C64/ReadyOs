; Offsets from the installed C64 OS S/ and TK/H, TK/S headers.
TK_THIS = $2B
TK_CLASS = $2D
TK_JUMP = $54
TK_COLORS = $0387
TK_INIT = 4
TK_SETFIRST = 10
TK_DRAW = 55
TK_OFFTOP = 11
TK_OFFBOT = 13
TK_OFFLEFT = 19
TK_OFFRIGHT = 21
TK_WIDTH = 25
TK_DFLAGS = 32
TK_MFLAGS = 33
TK_BCOLOR = 34
TK_SPLITPOS = 40
TK_LIST_CONTENT = 39
TK_LIST_SIZE = 41
TK_LIST_KEY = 43
TK_LIST_CLICK = 45
TK_LIST_DCLICK = 47
TK_SETCONTENT = 61
TK_SETBARS = 64
TK_INDEXCHANGE = 61
TK_RSMASK = 27
TK_HEIGHT = 17
TK_SETSTR = 76
TK_INPUT_LENGTH = 54
TK_INPUT_MAX = 55
TK_INPUT_INDEX = 56
TK_INPUT_ANCHOR = 57
TK_INPUT_DELEGATES = 58

.macro tkptr target
        ldx #<target
        ldy #>target
.endmacro
.macro tkobject target
        ldx target
        ldy target+1
        jsr ptrthis
.endmacro
.macro tkcall method
        ldy #method
        jsr getmethod
        jsr TK_JUMP
.endmacro
.macro tkprop8 prop, value
        ldy #prop
        lda #value
        sta (TK_THIS),y
.endmacro
.macro tkcallback prop, routine
        ldy #prop
        lda #<routine
        sta (TK_THIS),y
        iny
        lda #>routine
        sta (TK_THIS),y
.endmacro
