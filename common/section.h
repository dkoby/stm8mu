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
#ifndef _SECTION_H
#define _SECTION_H

/* */
#include <llist.h>
#include <types.h>

struct section_t {
    char *name;

    uint8_t  noload; /* section has not real data */

    char *data;
    uint32_t length;  /* current section length/data pointer */
    uint32_t alength; /* allocated data length */

    /* filled by linker */
    int placed;
    uint32_t offset;
    uint32_t lma; /* load memory address */
    uint32_t vma; /* virtual memory address */
};

struct sections_t {
    struct llist_t *first;
};

void sections_init(struct sections_t *sl);
void sections_destroy(struct sections_t *sl);
void section_pushdata(struct section_t *s, void *data, uint32_t length);
struct section_t *section_find(struct sections_t *sl, char *name);
struct section_t *section_select(struct sections_t *sl, char *name);
struct section_t *section_add(struct sections_t *sl, char *name);
void section_patch(struct section_t *s, uint32_t offset, void *data, uint32_t length);

void sections_mkloop(struct sections_t *sl, struct llist_t **ll);
struct section_t *sections_next(struct llist_t **ll);

#endif

