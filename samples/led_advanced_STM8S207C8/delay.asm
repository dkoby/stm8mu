
.print "=========================================================="
.print "File          : delay.asm"
.print "Description   : Simple delay routines"
.print "=========================================================="

;********************************
.section "text"

;
;
;
delay.w24:
    push  A
    pushw X

    ld A, (7, SP)
    ldw X, #$ffff
delay_dec:
    decw X
    jrne delay_dec
    dec A
    jrne delay_dec
    
    popw X
    pop  A

    retf
        
.export delay

