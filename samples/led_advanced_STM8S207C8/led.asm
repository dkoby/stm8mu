
.print "=========================================================="
.print "File          : led.asm"
.print "Description   : Sample program for blink LED on STM8S207C8"
.print "=========================================================="

.include "hw.inc"

;********************************
.section "vectors"
    jpf start

;********************************
; PC6
.define PIN_NUM   6

.extern STACK_TOP.w16
.extern delay.w24


;********************************
;*                              *
;********************************
.section "text"
start.w24:
    ldw X, #STACK_TOP
    ldw SP, X

    bset PC_DDR, #PIN_NUM ; Output
    bset PC_CR1, #PIN_NUM ; Push-pull

    ldw X, #interval
loop:

    bset PC_ODR, #PIN_NUM

    ld A, (X)
    push A
    callf delay
    pop A

    bres PC_ODR, #PIN_NUM

    ld A, (X)
    push A
    callf delay
    pop A

    incw X
    ld A, (X)
    jrne loop
    ldw X, #interval

    jra loop
;********************************
.section "data"
interval.w16:
    .d8 1
    .d8 2
    .d8 3
    .d8 4
    .d8 5
    .d8 0

;********************************
.section "bss"
    nop

.section "options"

.define OPT0_VAL OPT0_ROP_DISABLE
.define OPT1_VAL 0
.define OPT2_VAL 0
.define OPT3_VAL 0
.define OPT4_VAL {OPT4_EXTCLK_CRYSTAL | OPT4_CKAWUSEL_HSE | OPT4_PRSCS_24MHZ_TO128KHZ}
.define OPT5_VAL 0
.define OPT6_VAL 0
.define OPT7_VAL OPT7_WAITSTATE_1_WAIT_STATE

    .d8 { OPT0_VAL}
    .d8 { OPT1_VAL}
    .d8 {~OPT1_VAL}
    .d8 { OPT2_VAL}
    .d8 {~OPT2_VAL}
    .d8 { OPT3_VAL}
    .d8 {~OPT3_VAL}
    .d8 { OPT4_VAL}
    .d8 {~OPT4_VAL}
    .d8 { OPT5_VAL}
    .d8 {~OPT5_VAL}
    .d8 { OPT6_VAL}
    .d8 {~OPT6_VAL}
    .d8 { OPT7_VAL}
    .d8 {~OPT7_VAL}


