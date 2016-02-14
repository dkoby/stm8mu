/*
 *     Set of utilities for programming STM8 microcontrollers.
 *
 * Copyright (c) 2015, Dmitry Kobylin
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met: 
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */
#include "debug.h"
#include "stm8chip.h"

struct stm8chip_t stm8chips[] = {
/*   NAME          RAM                   EEPROM                 OPTIONS                FLASH                 OPTBL     */
    {"STM8S207C8", {0x000000, 0x001800}, {0x004000, 0x000600},  {0x004800, 0x000100},  {0x008000, 0x010000}, 0x00487E},
    {""          , {0x000000, 0x000000}, {0x000000, 0x000000},  {0x000000, 0x000000},  {0x000000, 0x000000}, 0x000000},
};

/*
 *
 */
void stm8chip_print(struct stm8chip_t *chip)
{
    printf("%s" NL, chip->name);
    printf(TAB "RAM      %06X %06X" NL, chip->ram.offset    , chip->ram.offset     + chip->ram.length);
    printf(TAB "EEPROM   %06X %06X" NL, chip->eeprom.offset , chip->eeprom.offset  + chip->eeprom.length);
    printf(TAB "OPTIONS  %06X %06X" NL, chip->options.offset, chip->options.offset + chip->options.length);
    printf(TAB "FLASH    %06X %06X" NL, chip->flash.offset  , chip->flash.offset   + chip->flash.length);
    printf(NL);
    printf(TAB "OPTBL    %06X" NL, chip->optbl);
    printf(NL);
}

