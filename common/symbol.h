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
#ifndef _SYMBOL_H
#define _SYMBOL_H

/* */
#include <llist.h>
#include <types.h>

struct symbol_attr_t {
    char *name;
    char *value;
};

struct symbol_t {
    enum {
        SYMBOL_TYPE_NONE = 0,
        SYMBOL_TYPE_CONST,
        SYMBOL_TYPE_EXTERN,
        SYMBOL_TYPE_LABEL,
    } type;

    char *name;    /* name of symbol */
    char *section; /* section */
    int exp;       /* export symbol */

    union {
        int64_t val64;
        int64_t offset;
    };

    uint8_t width; /* width of symbol in bytes */

    struct llist_t *attr;
};

struct symbols_t {
    struct llist_t *first;
};

#define SYMBOL_WIDTH_SHORT  "w8"
#define SYMBOL_WIDTH_LONG   "w16"
#define SYMBOL_WIDTH_EXT    "w24"

//#define SYMBOL_WIDTH_DEFAULT   SYMBOL_WIDTH_SHORT

#define SYMBOL_CURRENT_LABEL    "##current_label##"

void symbols_init(struct symbols_t *sl);
void symbols_destroy(struct symbols_t *sl);
struct symbol_t *symbols_add(struct symbols_t *sl, char *name);
struct symbol_t *symbol_find(struct symbols_t *sl, char *name);
void symbol_drop(struct symbols_t *sl, char *name);

void symbol_set_const(struct symbol_t *s, int64_t value);
struct symbol_t *symbol_get_const(struct symbols_t *sl, char *name, int64_t *value);

void symbol_set_section(struct symbol_t *s, char *section);
void symbol_set_width(struct symbol_t *s, char *width);
void symbol_set_attr(struct symbol_t *s, char *name, char *value);
char *symbol_get_attr(struct symbol_t *s, char *name);

void symbols_mkloop(struct symbols_t *sl, struct llist_t **ll);
struct symbol_t *symbols_next(struct llist_t **ll);

void symbol_attr_mkloop(struct symbol_t *s, struct llist_t **ll);
struct symbol_attr_t *symbol_attr_next(struct llist_t **ll);


#endif

