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
#include <stdlib.h>
#include <string.h>
/* */
#include <debug.h>
#include "app_common.h"
#include "section.h"

#if 0
    #define PRINTF(...) printf(__VA_ARGS__)
#else
    #define PRINTF(...)
#endif

#define SECTION_PREALLOC_SIZE    (64 * 1024)

static struct section_t * _section_create(char *name);
static void _section_destroy(void *p);

/*
 *
 */
void sections_init(struct sections_t *sl)
{
    sl->first = NULL;
}

/*
 *
 */
void sections_destroy(struct sections_t *sl)
{
    if (sl)
        llist_destroy(sl->first);
}

/*
 *
 */
struct section_t *section_find(struct sections_t *sl, char *name)
{
    struct llist_t *ll;
    struct section_t *s;

    for (ll = sl->first; ll; ll = ll->next)
    {
        s = ll->p;
        if (strcmp(s->name, name) == 0)
            return s;
    }

    return NULL;
}

/*
 * Select section by name. If section does not exists yet - create it.
 */
struct section_t *section_select(struct sections_t *sl, char *name)
{
    struct section_t *s;

    if (!sl)
        return NULL;

    s = section_find(sl, name);

    if (!s)
    {
        s = section_add(sl, name);
        if (!s)
            goto error;
    }

    return s;
error:
    debug_emsg("Can not select section");
    app_close(APP_EXITCODE_ERROR);
    return NULL;
}

/*
 *
 */
struct section_t *section_add(struct sections_t *sl, char *name)
{
    struct section_t *s;
    struct llist_t *head;

    s = _section_create(name);

    if (!s)
        goto error;

    head = llist_add(sl->first, s, _section_destroy, s);
    if (!head)
        goto error;
    sl->first = head;

    return s;
error:
    debug_emsg("Can not add section");
    app_close(APP_EXITCODE_ERROR);
    return NULL;
}

/*
 *
 */
static struct section_t * _section_create(char *name)
{
    struct section_t *s;

    s = malloc(sizeof(struct section_t));
    if (!s)
        goto error;

    s->data = malloc(SECTION_PREALLOC_SIZE);
    if (!s->data)
        goto error;
    s->name = malloc(strlen(name) + 1);
    if (!s->name)
        goto error;
    strcpy(s->name, name);

    s->length = 0;
    s->noload  = 0;
    s->placed  = 0;
    s->lma     = 0;
    s->vma     = 0;
    s->alength = SECTION_PREALLOC_SIZE;
    strcpy(s->name, name);

    return s;
error:
    _section_destroy(s);
    return NULL;
}

/*
 *
 */
static void _section_destroy(void *p)
{
    struct section_t *s;

    if (!p)
        return;

    s = p;
    if (s->data)
        free(s->data);
    if (s->name)
        free(s->name);
    free(s);
}

/*
 *
 */
void section_pushdata(struct section_t *s, void *data, uint32_t length)
{
    uint32_t needspace;

    if (!s || !data)
    {
        /* NOTREACHED */
        debug_emsg("NULL");
        app_close(APP_EXITCODE_ERROR);
        return;
    }

    if (!s->noload && length > 0)
    {
        /* reallocate memory if necessary */
        needspace = s->length + length;
        if (needspace > s->alength) 
        {
            void *p;

            s->alength += SECTION_PREALLOC_SIZE;
            if (s->alength < needspace)
                s->alength = (needspace / SECTION_PREALLOC_SIZE + 1) * SECTION_PREALLOC_SIZE;

            p = realloc(s->data, s->alength);
            if (!p)
            {
                debug_emsg("Realloc failed");
                app_close(APP_EXITCODE_ERROR);
                return;
            }
            s->data = p;
        }

        memcpy(&s->data[s->length], data, length);
    }
    s->length += length;
}

/*
 *
 */
void section_patch(struct section_t *s, uint32_t offset, void *data, uint32_t length)
{
    if (s->noload)
        return;

    if (offset + length > s->length)
    {
        debug_emsg("Failed to patch section");
        printf("offset %08X length %08X section length %08X" NL, offset, length, s->length);
        app_close(APP_EXITCODE_ERROR);
    }

    memcpy(&s->data[offset], data, length);
    return;
}

/*
 *
 */
void sections_mkloop(struct sections_t *sl, struct llist_t **ll)
{
    *ll = sl->first;
}

/*
 *
 */
struct section_t *sections_next(struct llist_t **ll)
{
    struct section_t *s;

    if (!(*ll))
        return NULL;

    s = (*ll)->p;
    (*ll) = (*ll)->next;

    return s;
}

