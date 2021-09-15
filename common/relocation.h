/*
 *     Set of utilities for programming STM8 microcontrollers.
 *
 * Copyright (c) 2015-2021, Dmitry Kobylin
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
#ifndef _RELOCATION_H
#define _RELOCATION_H

/* */
#include <types.h>
#include <llist.h>

struct relocation_t {
    enum relocation_type_t {
        RELOCATION_TYPE_ABOSULTE = 0,
        RELOCATION_TYPE_RELATIVE = 1,
    } type;

    char *section;
    char *symbol;

    uint32_t offset; /* offset of fixup from start of section */
    uint32_t length; /* length of fixup */
    int32_t  adjust; /* adjust offset of relative fixup */
};

struct relocations_t {
    struct llist_t *first;
};

void relocations_init(struct relocations_t *rl);
void relocations_destroy(struct relocations_t *rl);
void relocations_add(struct relocations_t *rl,
        char *section, char *symbol, uint32_t offset, uint32_t length, int32_t adjust, enum relocation_type_t type);

void relocations_mkloop(struct relocations_t *rl, struct llist_t **ll);
struct relocation_t *relocations_next(struct llist_t **ll);

#endif

