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
#ifndef _LINKER_H
#define _LINKER_H

/* */
#include <types.h>
#include <symbol.h>
#include <section.h>
#include <relocation.h>

struct linker_file_data_t {
    char *fname;

    struct symbols_t symbols;
    struct sections_t sections;
    struct relocations_t relocations;
};

struct linker_context_t {
    struct llist_t *flist;

    struct symbols_t symbols;

    struct {
        struct symbols_t symbols;
        struct sections_t sections;
        struct relocations_t relocations;
    } result;

    struct tokens_t tokens;
};

void linker_init();
void linker_run();
void linker_destroy();

struct symbol_t * linker_add_symbol(struct linker_context_t *ctx, char *name, int64_t value);

extern struct linker_context_t lcontext;

#endif

