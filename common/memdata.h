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
#ifndef _MEMDATA_H
#define _MEMDATA_H

#include "types.h"
#include "llist.h"

struct memdata_row_t {
    uint32_t offset;
    uint8_t *data;
    uint32_t length;
    int mark;
};

struct memdata_t {
    struct llist_t *rows;
};

struct memdata_t *memdata_create();
int memdata_add(struct memdata_t *md, uint32_t offset, uint8_t *buf, uint32_t len);
void memdata_destroy(struct memdata_t *md);
void memdata_print(struct memdata_t *md);
int memdata_pack(struct memdata_t **md);

void memdata_mkloop(struct memdata_t *md, struct llist_t **loop);
struct memdata_row_t *memdata_next(struct llist_t **loop);

#endif

