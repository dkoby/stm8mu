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
#include "app.h"
#include "assembler.h"
#include "debug.h"
#include "lang.h"
#include "lang_instruction.h"

#if 1
    #define PRINTF(...) printf(__VA_ARGS__)
#else
    #define PRINTF(...)
#endif

/*
 *
 */
void assembler_init()
{
    struct asm_context_t *ctx;

    app.asmcontext = malloc(sizeof(struct asm_context_t));
    if (!app.asmcontext)
    {
        debug_emsg("Can not allocate memory");
        app_close(APP_EXITCODE_ERROR);
    }

    ctx = app.asmcontext;

    ctx->pass     = 0;
    ctx->dbendian = DB_ENDIAN_BIG;

    tokens_init(&ctx->tokens);
    symbols_init(&ctx->symbols);
    sections_init(&ctx->sections);
    relocations_init(&ctx->relocations);

    ctx->section = section_select(&ctx->sections, "text");
}

/*
 *
 */
void assembler_destroy()
{
    struct asm_context_t *ctx;

    ctx = app.asmcontext;
    if (!ctx)
        return;

    tokens_destroy(&ctx->tokens);
    symbols_destroy(&ctx->symbols);
    sections_destroy(&ctx->sections);
    relocations_destroy(&ctx->relocations);

    free(ctx);
    app.asmcontext = NULL;
}

/*
 *
 */
int assembler(struct asm_context_t *ctx, char *infile)
{
    struct token_t *token;
    int error;

    token = token_new(&ctx->tokens);
    token_prepare(token, infile);

    while (1)
    {
        token_drop(token);
        if (lang_eof(token) == 0)
            break;
        if (lang_comment(token) == 0)
            continue;
        if (lang_label(ctx, token) == 0)
            continue;
        if (ctx->pass)
        {
            if (lang_directive(ctx, token) == 0)
                continue;
            if (lang_instruction(ctx, token) == 0)
                continue;
        } else {
            if (token_get(token, TOKEN_TYPE_LINE, TOKEN_CURRENT))
                continue;
        }

        debug_emsg("Unknown program construction");
        goto error;
    }

    error = 0;
    goto noerror;
error:
    error = -1;
    token_print_rollback(token);
    debug_emsgf("Error in file", "%s" NL, infile);
noerror:
    token_remove(&ctx->tokens, token);
    return error;
}

/*
 *
 */
void assembler_print_result(struct asm_context_t *ctx)
{
    struct llist_t *loop;
    printf("================================ ASSEMBLED INFO ================================" NL);

    if (ctx->symbols.first)
    {
        struct symbol_t *s;

        printf(NL);
        printf("------------" NL);
        printf("- Symbols. -" NL);
        printf("------------" NL);

        symbols_mkloop(&ctx->symbols, &loop);
        while ((s = symbols_next(&loop)))
        {
            struct symbol_attr_t *a;
            struct llist_t *lla;

            switch (s->type)
            {
                case SYMBOL_TYPE_CONST:  printf("CONST"); break;
                case SYMBOL_TYPE_EXTERN: printf("EXTERN"); break;
                case SYMBOL_TYPE_LABEL:  printf("LABEL"); break;
                default:                 printf("-----");
            }
            printf(" \"%s\"", s->name);
            printf(", width %u", s->width);
            printf(", export %u", s->exp);
            printf(", value %06llX (%lld)", (long long int)s->val64, (long long int)s->val64);
            if (s->section)
                printf(", section \"%s\"", s->section);

            printf(NL);
            symbol_attr_mkloop(s, &lla);
            while ((a = symbol_attr_next(&lla)))
                printf("\tattr \"%s\" = \"%s\"" NL, a->name, a->value ? a->value : "NULL");
        }
    }

    if (ctx->relocations.first)
    {
        struct relocation_t *r;
        struct llist_t *ll;

        printf(NL);
        printf("----------------" NL);
        printf("- Relocations. -" NL);
        printf("----------------" NL);

        relocations_mkloop(&ctx->relocations, &ll);
        while ((r = relocations_next(&ll)))
        {
            printf("%s", r->type == RELOCATION_TYPE_ABOSULTE ? "ABS" : "REL");
            printf(", offset: 0x%06X", r->offset);
            printf(", length: 0x%02X", r->length);
            printf(", section: \"%s\"", r->section);
            printf(", symbol: \"%s\"", r->symbol);
            if (r->type == RELOCATION_TYPE_ABOSULTE)
                printf(", adjust: --");
            else
                printf(", adjust: %d", r->adjust);

            printf(NL);
        }
    }

    if (ctx->sections.first)
    {
        struct section_t *s;
        struct llist_t *ll;

        printf(NL);
        printf("-------------" NL);
        printf("- Sections. -" NL);
        printf("-------------" NL);
        sections_mkloop(&ctx->sections, &ll);
        while ((s = sections_next(&ll)))
        {
            printf(NL);
            printf("Section \"%s\" [%u bytes]", s->name, s->length);
            if (s->noload)
                printf(" NOLOAD" NL);
            else
                debug_buf((uint8_t*)s->data, s->length);
        }
    }

    printf(NL);
    printf("================================================================================" NL);
    printf(NL);
}

