
.define PC_ODR.w16    $00500A
.define PC_IDR.w16    $00500B
.define PC_DDR.w16    $00500C
.define PC_CR1.w16    $00500D
.define PC_CR2.w16    $00500E

;******************************************
;**  Options                             **
;******************************************
.define OPT0_ROP_ENABLE    $AA
.define OPT0_ROP_DISABLE   $00

.define OPT3_WWDG_HALT_NO_RESET    {0 << 0}       ; No reset generated on halt if WWDG active
.define OPT3_WWDG_HALT_RESET       {1 << 0}       ; Reset generated on halt if WWDG active
.define OPT3_WWDG_HW_SOFTWARE      {0 << 1}       ; WWDG window watchdog activated by software
.define OPT3_WWDG_HW_HARDWARE      {1 << 1}       ; WWDG window watchdog activated by hardware
.define OPT3_WWDG_IWDG_HW_SOFTWARE {0 << 2}            ; IWDG independent watchdog activated by software
.define OPT3_WWDG_IWDG_HW_HARDWARE {1 << 2}       ; IWDG independent watchdog activated by hardware

.define OPT4_PRSCS_24MHZ_TO128KHZ  %00            ; 24 MHz to 128 kHz prescaler
.define OPT4_PRSCS_16MHZ_TO128KHZ  %01            ; 16 MHz to 128 kHz prescaler
.define OPT4_PRSCS_8MHZ_TO128KHZ   %10            ; 8 MHz to 128 kHz prescaler
.define OPT4_PRSCS_4MHZ_TO128KHZ   %11            ; 4 MHz to 128 kHz prescaler
.define OPT4_CKAWUSEL_LSI          {0 << 2}       ; LSI clock source selected for AWU
.define OPT4_CKAWUSEL_HSE          {1 << 2}       ; HSE clock with prescaler selected as clock source for AWU
.define OPT4_EXTCLK_CRYSTAL        {0 << 3}       ; External crystal connected to OSCIN/OSCOUT
.define OPT4_EXTCLK_GEN            {1 << 3}       ; External clock signal on OSCIN

.define OPT5_HSECNT_2048_CYCLES    $00            ; HSE crystal oscillator stabilization time 2048 HSE cycles
.define OPT5_HSECNT_128_CYCLES     $B4            ; HSE crystal oscillator stabilization time 128 HSE cycles
.define OPT5_HSECNT_8_CYCLES       $D2            ; HSE crystal oscillator stabilization time 8 HSE cycles
.define OPT5_HSECNT_05_CYCLES      $E1            ; HSE crystal oscillator stabilization time 0.5 HSE cycles

.define OPT7_WAITSTATE_NO_WAIT_STATE  {0 << 0}    ; No wait state
.define OPT7_WAITSTATE_1_WAIT_STATE   {1 << 0}    ; No wait state, for Fcpu > 16 MHz

