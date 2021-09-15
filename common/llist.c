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
/* */
#include "debug.h"
#include "llist.h"

/*
 * RETURN
 *     pointer to head of list
 */
struct llist_t * llist_add(struct llist_t *head, void *p, void (*destroy_cb)(void *), void *context)
{
    struct llist_t *tail, *nx;

    nx = malloc(sizeof(struct llist_t));
    if (!nx)
        return NULL;

    nx->p          = p;
    nx->destroy_cb = destroy_cb;
    nx->context    = context;
    nx->next       = NULL;
    nx->prev       = NULL;

    if (!head)
    {
        tail = nx;
        head = nx;
    } else {
        for (tail = head; tail->next; tail = tail->next)
            ;

        nx->prev   = tail;
        tail->next = nx;
    }

    return head;
}

/*
 * RETURN
 *     pointer to head of list
 */
struct llist_t * llist_remove(struct llist_t *head, struct llist_t *nx)
{
    struct llist_t *next, *prev;

    if (!head || !nx)
        return head;

    if (nx->destroy_cb)
        (*nx->destroy_cb)(nx->context);

    prev = nx->prev;
    next = nx->next;

    if (prev)
        prev->next = next;
    if (next)
        next->prev = prev;

    if (nx == head)
        head = next;

    free(nx);

    return head;
}

/*
 *
 */
struct llist_t * llist_sort(struct llist_t *head, int (*f)(void *, void *))
{
    int swaped;
    struct llist_t *ll, *e0, *e1;

    while (1)
    {
        swaped = 0;
        for (ll = head; ll && ll->next; ll = ll->next)
        {
            if ((*f)(ll->p, ll->next->p))
            {
                e0 = ll;
                e1 = ll->next;

                if (e0 == head)
                    head = e0->next;

                e0->next = e1->next;
                if (e1->next)
                    e1->next->prev = e0;
                if (e0->prev)
                    e0->prev->next = e1;
                e1->prev = e0->prev;
                e0->prev = e1;
                e1->next = e0;

                swaped = 1;
                break;
            }
        }
        if (!swaped)
            break;
    }

    return head;
}

/*
 *
 */
struct llist_t * llist_find(struct llist_t *ll, void *p)
{
    for (;ll; ll = ll->next)
        if (ll->p == p)
            return ll;

    return NULL;
}

/*
 *
 */
void llist_destroy(struct llist_t *head)
{
    while (head)
        head = llist_remove(head, head);
}

