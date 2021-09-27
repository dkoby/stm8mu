;======================================
;
;======================================

.print "======================================="
.print "File          : syntax.asm"
.print "Author        : Dmitry Kobylin"
.print "Date          : 26/11/2015"
.print "Version       : 0.0.1"
.print "Description   : Sample assembler file"
.print "Asm version   : 0.0.1"
.print "======================================="

.print "
Multiline print also supported.

Test escape characters:

Escape \"  : \"
Escape LF  : \n
Escape CR  : \reSCAPE
Escape \\   : \\
Escape 0   : \0
This string will not printed, because 0 character escaped
"

.print "\n"

.define   DEF1      {1}                     ; decimal number
.define   DEF1_     {123456789}             ; decimal number
.define   DEF2      {$ff}                   ; hex number
.define   DEF3      {%11111110}             ; binary number
.define   DEF4      {@17}                   ; octal number
.define   DEF5      {%0001_0110}            ; underscore separator
.define   DEF6      {9 + 4 - 2}             ; simple expression
.define   DEF7      {8 + 2 * 2}             ; test of operator priority
.define   DEF8      {0 - 4 - DEF7}          ; symbol in expression
.define   DEF9      {~9 & $ff}              ; negate
.define   DEF10     {(8 + 2) * 2 - (4 - 2)} ; complex expression with brackets
.define   DEF11.w8  {$ffffffffff}           ; constant symbol with attribute
.define   DEF12.w16 {$ffffffffff}           ; constant symbol with attribute
.define   DEF13.w24 {$ffffffffff}           ; constant symbol with attribute

.print "1 << 4 = " {1 << 4}
.print "8 >> 3 = " {8 >> 3}
.print ""
.print "DEF1  = "      {DEF1}
.print "DEF1_ = "      {DEF1_}
.print "DEF2  = "      {DEF2}
.print "DEF3  = "      {DEF3}
.print "DEF4  = "      {DEF4}  ", DEF4 = " "%~" {DEF4} ", DEF5 = " "%%" {DEF5}
.print "DEF6  = "      {DEF6}
.print "DEF7  = "      {DEF7}
.print "DEF8  = "      {DEF8}
.print "DEF9  = "      {DEF9}
.print "DEF10 = "      {DEF10}
.print "DEF11 = " "%$" {DEF11}
.print "DEF12 = " "%$" {DEF12}
.print "DEF13 = " "%$" {DEF13}

.extern data0     ; external label (shortmem)
.extern data1.w8  ; external label (shortmem)
.extern data2.w16 ; external label (longmem)
.extern data3.w24 ; external label (extmem)

.extern function0     ; external label (shortmem)
.extern function1.w8  ; external label (shortmem)
.extern function2.w16 ; external label (longmem)
.extern function3.w24 ; external label (extmem)

.print ""

.ifeq {TEST0} {256}
    .print "TEST0 symbol from command line is equal to 256"
.endif

.define TESTIF1 {2}
.define TESTIF2 {0}
.define TESTIF3 {0}

.if {TESTIF1 - 1}
    .print "test .if directive"
    .if {TESTIF2}
        .if {TESTIF1}
            .print "test .if directive, sub1"
        .endif
        .print "test .if directive, sub2"
    .endif
    .ifdef TESTIF2
        .print "test .ifdef directive"
    .endif
    .ifdef TESTIF3
        .print "test .define directive"
    .endif
    .ifndef TESTIF4
        .print "test .ifndef directive"
    .endif
    .ifeq {2 + 2} {4}
        .print "2 + 2 = 4"
    .endif
    .ifneq {2 + 2} {5}
        .print "2 + 2 != 5"
    .endif
    .ifneq {2 + 2} {4}
        .print "equal, no print"
    .endif
    .ifeq {2 + 2} {5}
        .print "not equal equal, no print"
    .endif
.endif

.include "constant.inc"
.include "constant.inc"

.print "CONSTANT1 = " "%$" {CONSTANT1}

;
; Test of little-endian .dX directive.
; Following data/instructions will be placed in "somedata_le" section.
;
.section "somedata_le"
.dbendian "little"
    .d8   $0123456789ABCDEF  ; output: EF
    .d8   $00, $00
    .d16  $0123456789ABCDEF  ; output: EF CD
    .d8   $00, $00
    .d24  $0123456789ABCDEF  ; output: EF CD AB
    .d8   $00, $00
    .d32  $0123456789ABCDEF  ; output: EF CD AB 89
    .d8   $00, $00
    .d64  $0123456789ABCDEF  ; output: EF CD AB 89 67 45 23 01

;
; Test of big-endian .dX directive.
; Following data/instructions will be placed in "somedata_be" section.
;
.section "somedata_be"
.dbendian "big"
    .d8   $0123456789ABCDEF  ; output: EF
    .d8   $00, $00
    .d16  $0123456789ABCDEF  ; output: CD EF
    .d8   $00, $00
    .d24  $0123456789ABCDEF  ; output: AB CD EF
    .d8   $00, $00
    .d32  $0123456789ABCDEF  ; output: 89 AB CD EF
    .d8   $00, $00
    .d64  $0123456789ABCDEF  ; output: 01 23 45 67 89 AB CD EF

;
; Test of .fill directive.
; Following data/instructions will be placed in "filled_section" section.
;
.section "filled_section"
    .fill $80 $12         ; fill with number
    .fill $80 {$A0 | $0B} ; fill with value of expression

;
; Following data/instructions will be placed in "dataX" section.
;
.section "dataX0"

data_samples_8bit:
    .d8 '0', '1', '2', '3'
    .d8  "0123456789", "ABCD", $BE, $EF, $BEEF
    .d8  0, 1, 2, 3, 4, 5, $FF, {4 + 2}, {DEF3}, $AB, $CD


data_samples_16bit.w16:
    .d16  $DEAD, {$0000 + 1}, $BEEF

data_samples_24bit:
    .d24 label_short
    .d24 label_long

;
; Following data/instructions will be placed in "data_markup" section.
; Data of this section does not appear in output file.
;
.section "data_markup" NOLOAD
    .d8 0, 1, 2, 3, 4

;
; Following data/instructions will be placed in "text1" section.
;
.section "text1"

    ld  A, {$10     >> 1}        ; shortmem expression
    ld  A, {$20     >> 1}.w8     ; shortmem expression
    ld  A, {$4000   >> 2}.w16    ; longmem expression
    ldf A, {$800000 >> 3}.w24    ; extmem expression
label_short.w8:
label_long.w16:
label_ext.w24:
    ld  A, label_short           ; shortmem
    ld  A, label_long            ; longmem
    ld  A, #label_short          ; byte

    ld  A, $20                   ; shortmem
    ld  A, $2000                 ; longmem
    ldf A, $200000               ; extmem
    ld  A, #$55                  ; byte
    ldw X, #$4455                ; word
    ld  A, (X)                   ; (X)
    ld  A, (Y)                   ; (Y)
    ld  (Y), A                   ; (Y)
    ld  A, ($20, X)              ; shortoff X
    ld  A, ($2000, X)            ; longoff X
    ldf A, ($200000, X)          ; extoff X
    ld  A, ($20, Y)              ; shortoff Y
    ld  A, ($2000, Y)            ; longoff Y
    ldf A, ($200000, Y)          ; extoff Y
    ld  A, ($20, SP)             ; shortoff SP
    ld  A, ([$20], X)            ; shortptr X
    ld  A, ([$2000], X)          ; longptr X

    ld  A, ([$20], Y)             ; shortptr Y
    ld  A, ([label_short], Y)     ; shortptr Y
    ld  A, ([{$20 + 1}], Y)       ; shortptr Y
    ld  A, ([{$20 + 1}.w16], X)   ; longptr X
    ld  A, ([{$20 + DEF1}.w16], X); longptr X

    ld A, [$30]                   ; shortptr
    ld A, [$3000]                 ; longptr
    ld A, [label_long]            ; longptr


    nop | nop | nop       ; instructions can be placed in one line, separated by "|" character
    ld  A, #$33
    ld  A, #label_short
    ld  A, label_short
    ld  A, $5002
    ld  A, label_long
    ld  A, (X)
    ld  A, ($50, X)
    ld  A, (label_short, X)
    ld  A, ($5002, X)
    ld  A, (label_long, X)
    ld  A, (Y)
    ld  A, ($50, Y)
    ld  A, (label_short, Y)
    ld  A, ($5002, Y)
    ld  A, (label_long, Y)
    ld  A, ($50, SP)
    ld  A, (label_short, SP)
    ld  A, [$50]
    ld  A, [label_short]
    ld  A, [$5002]
    ld  A, [label_long]
    ld  A, ([$50], X)
    ld  A, ([label_short], X)
    ld  A, ([$5002], X)
    ld  A, ([label_long], X)
    ld  A, ([$50], Y)
    ld  A, ([label_short], Y)

    ld  $50, A
    ld  label_short, A
    ld  $5002, A
    ld  label_long, A
    ld  (X), A
    ld  ($50, X), A
    ld  (label_short, X), A
    ld  ($5002, X), A
    ld  (label_long, X), A
    ld  (Y), A
    ld  ($50, Y), A
    ld  (label_short, Y), A
    ld  ($5002, Y), A
    ld  (label_long, Y), A
    ld  ($50, SP), A
    ld  (label_short, SP), A
    ld  [$50], A
    ld  [$5002], A
    ld  ([$50], X), A
    ld  ([$5002], X), A
    ld  ([$50], Y), A

    adc A, #$55
    adc A, $55
    adc A, $1011
    adc A, (X)
    adc A, ($10, X)
    adc A, ($5002, X)
    adc A, (Y)
    adc A, ($10, Y)
    adc A, ($5002, Y)
    adc A, ($10, SP)
    adc A, [$10]
    adc A, [$5002]
    adc A, ([$10], X)
    adc A, ([$5002], X)

loop16.w16:
loop8:
loop.w24:
    jpf loop
    jpf [$2000]

    rim
    sim

    halt
    rvf
    rcf
    scf
    wfi
    wfe
    ret
    retf

    mul X, A
    mul Y, A

    div X, A
    div Y, A

    exgw X, Y
    divw X, Y

    decw X
    decw Y

    cplw X
    cplw Y

    clrw X
    clrw Y

    ccf

    callr loop8

    callf loop
    callf [$8010]

    break

    sllw X
    slaw X
    sllw Y

    sraw X
    sraw Y

    srlw X
    srlw Y

    swapw X
    swapw Y

    tnzw X
    tnzw Y

    trap

    rrcw X
    rrcw Y

    rrwa X
    rrwa Y

    rlwa X
    rlwa Y

    rlcw X
    rlcw Y

    iret

    btjt data_samples_16bit, #7, loop8
    btjf $8033, #1, $02
    bset $8034, #5
    bres $8035, #4

    call testcall16
    call $0002
    call $1002
testcall16.w16:
testcall8:
    call (X)
    call ($10, X)
    call (testcall8, X)
    call (testcall16, X)
    call ($1020, X)
    
    call (Y)
    call ($10, Y)
    call (testcall8, Y)
    call (testcall16, Y)
    call ($1020, Y)

    call [$10]
    call [$1020]

    call [testcall8]
    call [testcall16]

    call ([$10], X)
    call ([$1020], X)

    call ([$10], Y)

    add A, ($1ff, X)
    add A, ($1ff, Y)

somefunction:

    addw X, #$22
    addw X, #$2233
    addw X, data_samples_16bit

    addw X, ($10, SP)

    addw Y, #$10
    addw Y, #$1020
    addw Y, $10
    addw Y, $1020

    addw Y, ($10, SP)

    addw SP, #$33

    mov $8010, #$55
    mov data_samples_16bit, #DEF1
    mov $8020, $33
    mov $44, $5040
    mov data_samples_8bit, $33
    mov $34, data_samples_8bit
    mov $10, #$55

    jrc  somefunction | jrc  $35
    jreq somefunction | jreq $35
    jrf  somefunction | jrf  $35
    
    jrh  somefunction | jrh  $35
    jrih somefunction | jrih $35
    jril somefunction | jril $35
    jrm  $33 | jrm  $35

    jrmi  $33 | jrmi  $35
    jrnc  $33 | jrnc  $35
    jrne  $33 | jrne  $35
    jrnh  $33 | jrnh  $35
    jrnm  $33 | jrnm  $35
    jrnv  $33 | jrnv  $35
    jrpl  $33 | jrpl  $35
    jrsge $33 | jrsge $35
    jrsgt $33 | jrsgt $35
    jrsle $33 | jrsle $35
    jrslt $33 | jrslt $35
    jrt   $33 | jrt   $35
    jruge $33 | jruge $35
    jrugt $33 | jrugt $35
    jrule $33 | jrule $35
    jrc   $33 | jrc   $35
    jrult $33 | jrult $35
    jrv   $33 | jrv   $35

    and A, $10
    and A, ($10, X)
    and A, ([data_samples_16bit], X)

    bccm data_samples_16bit, #7
    bccm $3344, #7

    bcp A, #$44
    bcp A, ($10, SP)
    bcp A, ($1ff, Y)

    bcpl $0044, #5
    bcpl data_samples_16bit, #5

    clr A
    clr $10
    clr $1020
    clr (X)
    clr ($10, X)
    clr ($1020, X)
    clr (Y)
    clr ($10, Y)
    clr ($1020, Y)
    clr ($10, SP)
    clr [$10]
    clr [$1020]
    clr ([$10], X)
    clr ([$1020], X)
    clr ([$10], Y)

    cp A, #$55
    cp A, $55
    cp A, $1011
    cp A, (X)
    cp A, ($10, X)
    cp A, ($5002, X)
    cp A, (Y)
    cp A, ($10, Y)
    cp A, ($5002, Y)
    cp A, ($10, SP)
    cp A, [$10]
    cp A, [$5002]
    cp A, ([$10], X)
    cp A, ([$5002], X)
    cp A, ([$10], Y)

    adc A, ([$10], Y)

    cpw X, #$10
    cpw X, #$2010
    cpw X, #data_samples_16bit

    cpw X, $20
    cpw X, $2030
    cpw X, (Y)
    cpw X, ($10, Y)
    cpw X, ($1020, Y)
    cpw X, ($10, SP)
    cpw X, [$10]
    cpw X, [$1020]
    cpw X, ([$10], Y)

    cpw Y, #$20
    cpw Y, #$2030
    cpw Y, $20
    cpw Y, $2030
    cpw Y, (X)
    cpw Y, ($10, X)
    cpw Y, ($1020, X)
    cpw Y, [$10]
    cpw Y, ([$10], X)
    cpw Y, ([$1020], X)

    cpl A
    cpl $10
    cpl $1020
    cpl (X)
    cpl ($10, X)
    cpl ($1020, X)
    cpl (Y)
    cpl ($10, Y)
    cpl ($1020, Y)
    cpl ($10, SP)
    cpl [$10]
    cpl [$1020]
    cpl ([$10], X)
    cpl ([$1020], X)
    cpl ([$10], Y)

    dec A
    dec $10
    dec $1020
    dec (X)
    dec ($10, X)
    dec ($1020, X)
    dec (Y)
    dec ($10, Y)
    dec ($1020, Y)
    dec ($10, SP)
    dec [$10]
    dec [$1020]
    dec ([$10], X)
    dec ([$1020], X)
    dec ([$10], Y)

    exg A, XL
    exg A, YL
    exg A, $10
    exg A, $1020
    exg A, data_samples_16bit

    inc A
    inc $10
    inc $1020
    inc (X)
    inc ($10, X)
    inc ($1020, X)
    inc (Y)
    inc ($10, Y)
    inc ($1020, Y)
    inc ($10, SP)
    inc [$10]
    inc [$1020]
    inc ([$10], X)
    inc ([$1020], X)
    inc ([$10], Y)

    incw X
    incw Y

    jp $10
    jp $1020
    jp (X)
    jp ($10, X)
    jp ($1020, X)
    jp (Y)
    jp ($10, Y)
    jp ($1020, Y)
    jp [$10]
    jp [$1020]
    jp ([$10], X)
    jp ([$1020], X)
    jp ([$10], Y)

    jpf $10
    jpf $1020
    jpf $102030
    jpf label_ext
    jpf [$10]
    jpf [$1020]
    jpf [label_long]

    ldf A, $10
    ldf A, $1020
    ldf A, $102030
    ldf A, label_ext
    ldf A, ($10, X)
    ldf A, ($1020, X)
    ldf A, ($102030, X)
    ldf A, (label_ext, X)
    ldf A, ($10, Y)
    ldf A, ($1020, Y)
    ldf A, ($102030, Y)
    ldf A, (label_ext, Y)
    ldf A, ([$10], X)
    ldf A, ([$1020], X)
    ldf A, ([label_long], X)
    ldf A, ([$10], Y)
    ldf A, ([$1020], Y)
    ldf A, ([label_long], Y)
    ldf A, [$10]
    ldf A, [$1020]
    ldf A, [label_long]

    ldf $10, A
    ldf $1020, A
    ldf $102030, A
    ldf label_ext, A
    ldf ($10, X), A
    ldf ($1020, X), A
    ldf ($102030, X), A
    ldf (label_ext, X), A
    ldf ($10, Y), A
    ldf ($1020, Y), A
    ldf ($102030, Y), A
    ldf (label_ext, Y), A
    ldf ([$10], X), A
    ldf ([$1020], X), A
    ldf ([label_long], X), A
    ldf ([$10], Y), A
    ldf ([$1020], Y), A
    ldf ([label_long], Y), A
    ldf [$10], A
    ldf [$1020], A
    ldf [label_long], A

    ldw X, #$10
    ldw X, #$1020
    ldw X, #label_long

    ldw X, $10
    ldw X, $1020
    ldw X, (X)
    ldw X, ($10, X)
    ldw X, ($1020, X)
    ldw X, ($10, SP)
    ldw X, [$10]
    ldw X, [$1020]
    ldw X, ([$10], X)
    ldw X, ([$1020], X)

    ldw $10, X
    ldw $1020, X
    ldw (X), Y
    ldw ($10, X), Y
    ldw ($1020, X), Y
    ldw ($10, SP), X
    ldw [$10], X
    ldw [$1020], X
    ldw ([$10], X), Y
    ldw ([$1020], X), Y

    ldw Y, #$10
    ldw Y, #$1020
    ldw Y, $10
    ldw Y, $1020
    ldw Y, (Y)
    ldw Y, ($10, Y)
    ldw Y, ($1020, Y)
    ldw Y, ($10, SP)
    ldw Y, [$10]
    ldw Y, ([$10], Y)

    ldw $10, Y
    ldw $1020, Y
    ldw (Y), X
    ldw ($10, Y), X
    ldw ($1020, Y), X
    ldw ($10, SP), Y
    ldw [$10], Y
    ldw ([$10], Y), X

    ldw Y, X
    ldw X, Y
    ldw X, SP
    ldw SP, X
    ldw Y, SP
    ldw SP, Y

    neg A
    neg $10
    neg $1020
    neg (X)
    neg ($10, X)
    neg ($1020, X)
    neg (Y)
    neg ($10, Y)
    neg ($1020, Y)
    neg ($10, SP)
    neg [$10]
    neg [$1020]
    neg ([$10], X)
    neg ([$1020], X)
    neg ([$10], Y)

    negw X
    negw Y

    and A, #$10
    and A, $10
    and A, $1020
    and A, (X)
    and A, ($10, X)
    and A, ($1020, X)
    and A, (Y)
    and A, ($10, Y)
    and A, ($1020, Y)
    and A, ($10, SP)
    and A, [$10]
    and A, [$1020]
    and A, ([$10], X)
    and A, ([$1020], X)
    and A, ([$10], Y)

    or A, #$10
    or A, $10
    or A, $1020
    or A, (X)
    or A, ($10, X)
    or A, ($1020, X)
    or A, (Y)
    or A, ($10, Y)
    or A, ($1020, Y)
    or A, ($10, SP)
    or A, [$10]
    or A, [$1020]
    or A, ([$10], X)
    or A, ([$1020], X)
    or A, ([$10], Y)

    pop A
    pop CC
    pop $10
    pop $1020
    pop label_long

    popw X
    popw Y

    push A
    push CC
    push #$10
    push $10
    push $1020
    push label_long

    pushw X
    pushw Y

    rlc A
    rlc $10
    rlc $1020
    rlc (X)
    rlc ($10, X)
    rlc ($1020, X)
    rlc (Y)
    rlc ($10, Y)
    rlc ($1020, Y)
    rlc ($10, SP)
    rlc [$10]
    rlc [$1020]
    rlc ([$10], X)
    rlc ([$1020], X)
    rlc ([$10], Y)

    rrc A
    rrc $10
    rrc $1020
    rrc (X)
    rrc ($10, X)
    rrc ($1020, X)
    rrc (Y)
    rrc ($10, Y)
    rrc ($1020, Y)
    rrc ($10, SP)
    rrc [$10]
    rrc [$1020]
    rrc ([$10], X)
    rrc ([$1020], X)
    rrc ([$10], Y)

    sbc A, #$10
    sbc A, $10
    sbc A, $1020
    sbc A, (X)
    sbc A, ($10, X)
    sbc A, ($1020, X)
    sbc A, (Y)
    sbc A, ($10, Y)
    sbc A, ($1020, Y)
    sbc A, ($10, SP)
    sbc A, [$10]
    sbc A, [$1020]
    sbc A, ([$10], X)
    sbc A, ([$1020], X)
    sbc A, ([$10], Y)

    sll A
    sll $10
    sll $1020
    sll (X)
    sll ($10, X)
    sll ($1020, X)
    sll (Y)
    sll ($10, Y)
    sll ($1020, Y)
    sll ($10, SP)
    sll [$10]
    sll [$1020]
    sll ([$10], X)
    sll ([$1020], X)
    sll ([$10], Y)

    sra A
    sra $10
    sra $1020
    sra (X)
    sra ($10, X)
    sra ($1020, X)
    sra (Y)
    sra ($10, Y)
    sra ($1020, Y)
    sra ($10, SP)
    sra [$10]
    sra [$1020]
    sra ([$10], X)
    sra ([$1020], X)
    sra ([$10], Y)

    srl A
    srl $10
    srl $1020
    srl (X)
    srl ($10, X)
    srl ($1020, X)
    srl (Y)
    srl ($10, Y)
    srl ($1020, Y)
    srl ($10, SP)
    srl [$10]
    srl [$1020]
    srl ([$10], X)
    srl ([$1020], X)
    srl ([$10], Y)

    sub A, #$10
    sub A, $10
    sub A, $1020
    sub A, (X)
    sub A, ($10, X)
    sub A, ($1020, X)
    sub A, (Y)
    sub A, ($10, Y)
    sub A, ($1020, Y)
    sub A, ($10, SP)
    sub A, [$10]
    sub A, [$1020]
    sub A, ([$10], X)
    sub A, ([$1020], X)
    sub A, ([$10], Y)
    sub SP, #$10

    subw X, #$10
    subw X, #$1020
    subw X, #label_long
    subw X, $10
    subw X, $1020
    subw X, ($10, SP)
    subw Y, #$10
    subw Y, #$1020
    subw Y, #label_long
    subw Y, $10
    subw Y, $1020
    subw Y, ($10, SP)

    swap A
    swap $10
    swap $1020
    swap (X)
    swap ($10, X)
    swap ($1020, X)
    swap (Y)
    swap ($10, Y)
    swap ($1020, Y)
    swap ($10, SP)
    swap [$10]
    swap [$1020]
    swap ([$10], X)
    swap ([$1020], X)
    swap ([$10], Y)

    tnz A
    tnz $10
    tnz $1020
    tnz (X)
    tnz ($10, X)
    tnz ($1020, X)
    tnz (Y)
    tnz ($10, Y)
    tnz ($1020, Y)
    tnz ($10, SP)
    tnz [$10]
    tnz [$1020]
    tnz ([$10], X)
    tnz ([$1020], X)
    tnz ([$10], Y)

    xor A, #$10
    xor A, $10
    xor A, $1020
    xor A, (X)
    xor A, ($10, X)
    xor A, ($1020, X)
    xor A, (Y)
    xor A, ($10, Y)
    xor A, ($1020, Y)
    xor A, ($10, SP)
    xor A, [$10]
    xor A, [$1020]
    xor A, ([$10], X)
    xor A, ([$1020], X)
    xor A, ([$10], Y)

;
; Exported symbols
;
.export somefunction
.export data_samples_24bit

;
; Local symbols.
;
.print "==== Test local symbols ===="

delay:
    dec A
    ret

test_local1:
.define ?DELAY1 {50}
.define ?DELAY2 {?DELAY1 + 50}

.print "?DELAY1 = " {?DELAY1}
.print "?DELAY2 = " {?DELAY2}

    ld A, #{?DELAY1}
?d1:
    tnz A | jrne ?d1

    ld A, #?DELAY2
?d2:
    tnz A | jrne ?d2
    
test_local2:
.define ?DELAY1 {100}
.define ?DELAY2 {?DELAY1 + 100}

.print "?DELAY1 = " {?DELAY1}
.print "?DELAY2 = " {?DELAY2}

    ld A, #{?DELAY1}
?d1:
    tnz A | jrne ?d1

    ld A, #?DELAY2
?d2:
    tnz A | jrne ?d2

