.print "=========================================================="
.print "File          : led.asm"
.print "Description   : Sample program for blink LED on STM8S207C8"
.print "=========================================================="

.define PC_ODR.w16    $00500A
.define PC_DDR.w16    $00500C
.define PC_CR1.w16    $00500D

; PC6
.define PIN_NUM   6

.extern STACK_TOP.w16

;********************************
;*                              *
;********************************
.section "text"
    jpf start
start.w24:
    ldw X, #STACK_TOP
    ldw SP, X

    bset PC_DDR, #PIN_NUM ; Output
    bset PC_CR1, #PIN_NUM ; Push-pull
loop:
    bset PC_ODR, #PIN_NUM
    callr delay
    bres PC_ODR, #PIN_NUM
    callr delay
    jra loop

;********************************
;*                              *
;********************************
.define DELAY_VAL_MS 500
.define FCPU_HZ      {2 * 1000 * 1000}
.define LOOP_LENGTH  4
.define LOOPS        {FCPU_HZ * DELAY_VAL_MS / 1000 / LOOP_LENGTH}

delay:
    pushw X
    pushw Y
    
    ldw Y, #{LOOPS >> 16}.w16
    ldw X, #{LOOPS & $ffff}.w16

delay_dec:
    decw X
    jrne delay_dec
    decw Y
    jrne delay_dec
    
    popw Y
    popw X
    ret
