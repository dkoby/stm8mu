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
#include "symbol.h"
#include "app_common.h"

#if 0
    #define PRINTF(...) printf(__VA_ARGS__)
#else
    #define PRINTF(...)
#endif

static void _symbol_destroy(void *p);
static struct symbol_t * _symbol_create(char *name);

/*
 *
 */
void symbols_init(struct symbols_t *sl)
{
    sl->first = NULL;
}

/*
 *
 */
struct symbol_t *symbols_add(struct symbols_t *sl, char *name)
{
    struct llist_t *head;
    struct symbol_t *s;

    s = NULL;
    if (symbol_find(sl, name))
    {
        debug_emsgf("Symbol redefined", "%s" NL, name);
        goto error;
    }

    s = _symbol_create(name);
    if (!s)
        goto error;

    s->type = SYMBOL_TYPE_NONE;

    head = llist_add(sl->first, s, _symbol_destroy, s);
    if (!head)
        goto error;
    sl->first = head;

    PRINTF("Add symbol %s, %llu" NL, name);

    return s;
error:
    debug_emsg("Can not add symbol");
    if (s)
        _symbol_destroy(s);
    app_close(APP_EXITCODE_ERROR);
    return NULL;
}

/*
 *
 */
void symbol_set_const(struct symbol_t *s, int64_t value)
{
    s->type = SYMBOL_TYPE_CONST; 
    s->val64 = value;
}

/*
 *
 */
struct symbol_t *symbol_get_const(struct symbols_t *sl, char *name, int64_t *value)
{
    struct symbol_t *s;

    s = symbol_find(sl, name);
    if (!s)
        return NULL;

    if (s->type != SYMBOL_TYPE_CONST)
    {
        /* NOTREACHED */
        debug_emsgf("Symbol not constant", "\"%s\"" NL, name);
        app_close(APP_EXITCODE_ERROR);
        return NULL;
    }

    *value = s->val64;

    return s;
}

/*
 *
 */
struct symbol_t *symbol_find(struct symbols_t *sl, char *name)
{
    struct llist_t *ll;
    struct symbol_t *s;

    if (!sl)
        return NULL;

    for (ll = sl->first; ll; ll = ll->next)
    {
        s = ll->p;
        if (strcmp(s->name, name) == 0)
            return s;
    }

    return NULL;
}

/*
 *
 */
static struct symbol_t * _symbol_create(char *name)
{
    struct symbol_t *s;

    s = malloc(sizeof(struct symbol_t));
    memset(s, 0, sizeof(struct symbol_t));

    s->section  = NULL;
    s->val64    = 0;
    s->name     = malloc(strlen(name) + 1);
    s->attr     = NULL;
    if (!s->name)
        goto error;
    strcpy(s->name, name);
    symbol_set_width(s, SYMBOL_WIDTH_SHORT);

    return s;
error:
    debug_emsg("Can not add symbol");
    if (s)
        _symbol_destroy(s);
    return NULL;
}

/*
 *
 */
void symbol_set_width(struct symbol_t *s, char *width)
{
    if (strcmp(width, SYMBOL_WIDTH_SHORT) == 0)
        s->width = 1;
    else if (strcmp(width, SYMBOL_WIDTH_LONG) == 0)
        s->width = 2;
    else if (strcmp(width, SYMBOL_WIDTH_EXT) == 0)
        s->width = 3;
    else {
        debug_emsgf("Invalid symbol width", "%s, %s" NL, s->name, width);
        app_close(APP_EXITCODE_ERROR);
    }
}

/*
 *
 */
static void _symbol_destroy(void *p)
{
    struct symbol_t *s;

    if (!p)
        return;
    s = p;

    if (s->name)
        free(s->name);
    if (s->attr)
        llist_destroy(s->attr);

    free(s);
}

/*
 *
 */
void symbols_destroy(struct symbols_t *sl)
{
    if (sl)
        llist_destroy(sl->first);
}

/*
 *
 */
static void _attr_destroy(void *p)
{
    struct symbol_attr_t *attr;

    if (!p)
        return;

    attr = p;
    if (attr->name)
        free(attr->name);
    if (attr->value)
        free(attr->value);
    free(attr);
}

/*
 *
 */
void symbol_set_section(struct symbol_t *s, char *section)
{
    if (s->section)
    {
        debug_emsg("Symbol already assigned to section");
        app_close(APP_EXITCODE_ERROR);
        return;
    }

    s->section = malloc(strlen(section) + 1);
    if (!s->section)
    {
        debug_emsg("Failed to allocate memory for section name");
        app_close(APP_EXITCODE_ERROR);
        return;
    }
    strcpy(s->section, section);
}

/*
 *
 */
void symbol_set_attr(struct symbol_t *s, char *name, char *value)
{
    struct llist_t *head;
    struct symbol_attr_t *attr;

    if (!name)
    {
        /* NOTREACHED */
        debug_emsg("No attribute name given");
        app_close(APP_EXITCODE_ERROR);
        return;
    }

    attr = malloc(sizeof(struct symbol_attr_t));
    if (!attr)
        goto error;
    memset(attr, 0, sizeof(struct symbol_attr_t));

    attr->name = malloc(strlen(name) + 1);
    if (!attr->name)
        goto error;
    strcpy(attr->name, name);
    if (value)
    {
        attr->value = malloc(strlen(value) + 1);
        if (!attr->value)
            goto error;
        strcpy(attr->value, value);
    }

    /*
     *
     */
    if (strcmp(name, "width") == 0)
    {
        if (strcmp(value, SYMBOL_WIDTH_SHORT) == 0)
            ;
        else if (strcmp(value, SYMBOL_WIDTH_LONG) == 0)
            ;
        else if (strcmp(value, SYMBOL_WIDTH_EXT) == 0)
            ;
        else {
            debug_emsg("Invalid value of width attribute");
            goto error;
        }
    }

    {
        struct llist_t *ll;
        struct symbol_attr_t *attr;

        for (ll = s->attr; ll; ll = ll->next)
        {
            attr = ll->p;

            if (attr->name && strcmp(attr->name, name) == 0)
            {
                s->attr = llist_remove(s->attr, ll);
                break;
            }
        }
    }

    head = llist_add(s->attr, attr, _attr_destroy, attr);
    if (!head)
        goto error;
    s->attr = head;

    return;
error:
    debug_emsgf("Can not set attribute of symbol", "%s" NL, s->name);
    if (attr)
        _attr_destroy(attr);
    app_close(APP_EXITCODE_ERROR);
}

/*
 *
 */
char *symbol_get_attr(struct symbol_t *s, char *name)
{
    struct llist_t *ll;
    struct symbol_attr_t *attr;

    for (ll = s->attr; ll; ll = ll->next)
    {
        attr = ll->p;

        if (attr->name && strcmp(attr->name, name) == 0)
            return attr->value;
    }

    return NULL;
}

/*
 *
 */
void symbol_attr_mkloop(struct symbol_t *s, struct llist_t **ll)
{
    *ll = s->attr;
}

/*
 *
 */
struct symbol_attr_t *symbol_attr_next(struct llist_t **ll)
{
    struct symbol_attr_t *a;

    if (!*ll)
        return NULL;

    a = (*ll)->p;
    (*ll) = (*ll)->next;

    return a;
}


/*
 *
 */
void symbols_mkloop(struct symbols_t *sl, struct llist_t **ll)
{
    *ll = sl->first;
}

/*
 *
 */
struct symbol_t *symbols_next(struct llist_t **ll)
{
    struct symbol_t *s;

    if (!*ll)
        return NULL;

    s = (*ll)->p;
    (*ll) = (*ll)->next;

    return s;
}

