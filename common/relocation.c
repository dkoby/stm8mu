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
#include <string.h>
#include <stdlib.h>
/* */
#include <debug.h>
#include <llist.h>
#include "app_common.h"
#include "relocation.h"

#if 0
    #define PRINTF(...) printf(__VA_ARGS__)
#else
    #define PRINTF(...)
#endif

static void _relocation_destroy(void *p);

/*
 *
 */
void relocations_init(struct relocations_t *sl)
{
    sl->first = NULL;
}

/*
 *
 */
void relocations_destroy(struct relocations_t *sl)
{
    if (sl)
        llist_destroy(sl->first);
}

/*
 *
 */
void relocations_add(struct relocations_t *rl,
        char *section, char *symbol, uint32_t offset, uint32_t length, int32_t adjust, enum relocation_type_t type)
{
    struct llist_t *head;
    struct relocation_t *r;

    r = malloc(sizeof(struct relocation_t));
    if (!r)
        goto error;

    r->section = malloc(strlen(section) + 1);
    if (!r->section)
        goto error;
    strcpy(r->section, section);
    r->symbol = malloc(strlen(symbol) + 1);
    if (!r->symbol)
        goto error;
    strcpy(r->symbol, symbol);

    r->type   = type;
    r->offset = offset;
    r->length = length;
    r->adjust = adjust;

    head = llist_add(rl->first, r, _relocation_destroy, r);
    if (!head)
        goto error;
    rl->first = head;

    return;
error:
    debug_emsg("Can not add relocation");
    if (r)
        _relocation_destroy(r);
    app_close(APP_EXITCODE_ERROR);
}

/*
 *
 */
static void _relocation_destroy(void *p)
{
    struct relocation_t *r;

    if (!p)
        return;
    r = p;
    if (r->section)
        free(r->section);
    if (r->symbol)
        free(r->symbol);

    free(r);
}

/*
 *
 */
void relocations_mkloop(struct relocations_t *rl, struct llist_t **ll)
{
    *ll = rl->first;
}

/*
 *
 */
struct relocation_t *relocations_next(struct llist_t **ll)
{
    struct relocation_t *s;

    if (!*ll)
        return NULL;

    s = (*ll)->p;
    (*ll) = (*ll)->next;

    return s;
}

