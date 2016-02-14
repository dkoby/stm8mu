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
#include <string.h>
/* */
#include "debug.h"
#include "memdata.h"

/*
 *
 */
struct memdata_t *memdata_create()
{
    struct memdata_t *md;

    md = malloc(sizeof(struct memdata_t));
    if (!md)
        return NULL;

    md->rows = NULL;

    return md;
}

/*
 *
 */
static void memdata_row_destroy(void *context)
{
    struct memdata_row_t *row;

    row = context;
    if (row->data)
        free(row->data);
    free(row);
}

/*
 *
 */
int memdata_add(struct memdata_t *md, uint32_t offset, uint8_t *buf, uint32_t length)
{
    struct memdata_row_t *row;
    struct llist_t *head;

    if (!md)
    {
        /* NOTREACHED */
        debug_emsg("NULL md");
        return -1;
    }

    row = malloc(sizeof(struct memdata_row_t));
    if (!row)
        goto error;
    row->data = malloc(length);
    if (!row->data)
        goto error;
    memcpy(row->data, buf, length);
    row->offset = offset;
    row->length = length;
    row->mark   = 0;

    head = llist_add(md->rows, row, memdata_row_destroy, row);
    if (!head)
        goto error;
    md->rows = head;

    return 0;
error:
    if (row)
        memdata_row_destroy(row);

    return -1;
}

/*
 *
 */
void memdata_destroy(struct memdata_t *md)
{
    if (!md)
        return;
    if (md->rows)
        llist_destroy(md->rows);
    free(md);
}

/*
 *
 */
void memdata_print(struct memdata_t *md)
{
    struct llist_t *ll;
    struct memdata_row_t *row;

    if (!md)
    {
        /* NOTREACHED */
        debug_emsg("NULL md");
        return;
    }

    for (ll = md->rows; ll; ll = ll->next)
    {
        int i;
        row = ll->p;

        printf("DATA %06X %06X: ", row->offset, row->length);
        for (i = 0; i < row->length; i++)
        {
            if ((i % 16) == 0)
                printf(NL "                    ");
            printf("%02X ", row->data[i]);
        }
        printf(NL);
    }
}

/*
 *
 */
int _compare_row(void *first, void *second)
{
    if (!first || !second)
        return 0;

//    printf("%06X %06X"NL, ((struct memdata_row_t*)first)->offset, ((struct memdata_row_t*)second)->offset);

    if (((struct memdata_row_t*)first)->offset > ((struct memdata_row_t*)second)->offset)
        return 1;
    else
        return 0;
}

/*
 * Pack chunks of data in continues memory if possible. Also sort entries
 * by memory offset.
 *
 * RETURN
 *    0 on success, -1 on error
 */
int memdata_pack(struct memdata_t **md)
{
    struct memdata_t *newmd;
    struct llist_t *ll;
    struct memdata_row_t *row;
    struct {
        uint8_t *data;
        uint32_t offset;
        uint32_t length;
    } chunk;

    newmd = NULL;
    chunk.data  = NULL;
    if (!md)
    {
        /* NOTREACHED */
        debug_emsg("NULL md");
        goto error;
    }

    newmd = memdata_create();
    if (!newmd)
        goto error;

    /* clear marks on old memory data */
    for (ll = (*md)->rows; ll; ll = ll->next)
    {
        row = ll->p;
        row->mark = 0;
    }

    while (1)
    {
        /* find first entry for new data block */
        for (ll = (*md)->rows; ll; ll = ll->next)
        {
            row = ll->p;
            if (!row->mark)
            {
                chunk.data = malloc(row->length);
                if (!chunk.data)
                {
                    debug_emsg("Failed to allocate memory for chunk");
                    goto error;
                }
                memcpy(chunk.data, row->data, row->length);
                chunk.offset = row->offset;
                chunk.length = row->length;
                row->mark = 1;
                break;
            }
        }
        if (!chunk.data)
            break;

        /* find contiguous memory chunks */
        while (1)
        {
            int marked;

            marked = 0;
            for (ll = (*md)->rows; ll; ll = ll->next)
            {
                row = ll->p;

                /* drop zero length rows */
                if (row->length == 0)
                {
                    /* NOTREACHED */
                    debug_emsg("Zero length");
                    row->mark = 1;
                    marked = 1;
                } else if (!row->mark) {
                    /* check overlap */
                    if ((row->offset <  chunk.offset && row->offset + row->length > chunk.offset) ||
                        (row->offset >= chunk.offset && row->offset < chunk.offset + chunk.length))
                    {
                        debug_emsgf("Memory overlap at", "%06X %06X %06X" NL,
                                chunk.offset, chunk.length, row->offset);
                        goto error;
                    }

                    if (row->offset == chunk.offset + chunk.length)
                    {
                        void *p;

                        p = realloc(chunk.data, chunk.length + row->length);
                        if (!p)
                        {
                            debug_emsg("Failed to realloc memory");
                            goto error;
                        }
                        chunk.data = p;
                        memcpy(chunk.data + chunk.length, row->data, row->length);
                        chunk.length += row->length;

                        row->mark = 1;
                        marked    = 1;
                    } else if (row->offset + row->length == chunk.offset) {
                        void *p;

                        p = realloc(chunk.data, chunk.length + row->length);
                        if (!p)
                        {
                            debug_emsg("Failed to realloc memory");
                            goto error;
                        }
                        chunk.data = p;
                        memmove(chunk.data + row->length, chunk.data, chunk.length);
                        memcpy(chunk.data, row->data, row->length);
                        chunk.offset = row->offset;
                        chunk.length += row->length;

                        row->mark = 1;
                        marked    = 1;
                    }
                }
            }
            if (!marked)
                break;
        }

        if (memdata_add(newmd, chunk.offset, chunk.data, chunk.length))
        {
            debug_emsg("Failed to append memory data");
            goto error;
        }
        free(chunk.data);
        chunk.data = NULL;
    }

    if (chunk.data)
        free(chunk.data);

    /* sort new list in ascend order */
    newmd->rows = llist_sort(newmd->rows, _compare_row);

    memdata_destroy(*md);
    *md = newmd;

    return 0;
error:
    if (newmd)
        memdata_destroy(newmd);
    if (chunk.data)
        free(chunk.data);
    return -1;
}

/*
 *
 */
void memdata_mkloop(struct memdata_t *md, struct llist_t **loop)
{
    *loop = md->rows;
}

/*
 *
 */
struct memdata_row_t *memdata_next(struct llist_t **loop)
{
    struct memdata_row_t *row;

    if (!(*loop))
        return NULL;

    row = (*loop)->p;
    (*loop) = (*loop)->next;

    return row;
}

