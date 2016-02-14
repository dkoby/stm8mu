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
#include <stdlib.h>
/* */
#include <debug.h>
#include "bmem.h"

#define BMEM_STATIC_BUF_SIZE    (128 * 1024)
static uint8_t bmem_static_buf[BMEM_STATIC_BUF_SIZE];

/*
 *
 */
void bmem_init(struct bmem_t *bm)
{
    bm->buf  = bmem_static_buf;
    bm->size = BMEM_STATIC_BUF_SIZE;
}

/*
 *
 */
void bmem_destroy(struct bmem_t *bm)
{
    if (bm->buf && bm->buf != bmem_static_buf)
        free(bm->buf);
}

/*
 *
 */
int bmem_alloc(struct bmem_t *bm, uint32_t size)
{
    void *p;

    if (size > bm->size)
    {
        p = realloc(bm->buf == bmem_static_buf ? NULL : bm->buf, size);
        if (!p)
        {
            debug_emsg("Failed to allocate bmem");
            return -1;
        }
        bm->buf  = p;
        bm->size = size;
    }

    return 0;
}
