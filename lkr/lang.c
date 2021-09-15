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
/* */
#include <debug.h>
#include <lang_constexpr.h>
#include <btorder.h>
#include <lang_util.h>
#include "app.h"
#include "lang.h"
#include "linker.h"

//static int _lang_db(struct asm_context_t *ctx, struct token_t *token, int width);
static void _dot_print(const char *fmt, ...);

/*
 *
 */
int lang_comment(struct token_t *token)
{
    if (token_get(token, TOKEN_TYPE_COMMENT, TOKEN_NEXT))
        return 0;

    return -1;
}

/*
 *
 */
int lang_eof(struct token_t *token)
{
    if (token_get(token, TOKEN_TYPE_EOF, TOKEN_NEXT))
        return 0;

    return -1;
}

/*
 *
 */
int lang_const_symbol(struct linker_context_t *ctx, struct token_t *token)
{
    char *tname;
    struct symbol_t *s;
    int64_t value;

    if (!(tname = token_get(token, TOKEN_TYPE_SYMBOL, TOKEN_CURRENT)))
        return -1;

    if (strcmp(tname, "NOLOAD") == 0)
    {
        debug_emsg("NOLOAD is reserved symbol name - can not define such symbol");
        goto error;
    }

    s = symbol_find(&ctx->symbols, tname);
    if (!s)
        s = symbols_add(&ctx->symbols, tname);

    if (!token_get(token, TOKEN_TYPE_EQUAL, TOKEN_NEXT))
    {
        debug_emsg("Missing \"=\"");
        goto error;
    }

    if ((tname = token_get(token, TOKEN_TYPE_SYMBOL, TOKEN_NEXT)))
    {
        struct section_t *section;

        if (strcmp(tname, "sizeof") != 0)
        {
            debug_emsg("Missing \"sizeof\"");
            goto error;
        }

        if (!token_get(token, TOKEN_TYPE_ROUND_OPEN, TOKEN_NEXT))
        {
            debug_emsg("Missing \"(\"");
            goto error;
        }

        if (!(tname = token_get(token, TOKEN_TYPE_STRING, TOKEN_NEXT)))
        {
            debug_emsg("Missing sections name in \"sizeof\" operator");
            goto error;
        }

        section = section_find(&ctx->result.sections, tname);
        if (!section)
        {
            debug_emsgf("Section not found", "\"%s\"" NL, tname);
            goto error;
        }

        value = section->length;

        if (!token_get(token, TOKEN_TYPE_ROUND_CLOSE, TOKEN_NEXT))
        {
            debug_emsg("Missing \"(\"");
            goto error;
        }
    } else if (lang_constexpr(&ctx->symbols, token, &value) == 0) {

    } else if ((tname = token_get(token, TOKEN_TYPE_NUMBER, TOKEN_NEXT))) {
        if (lang_util_str2num(tname, &value) < 0)
            goto error;
    } else {
        debug_emsg("Value missing (nor expression nor number nor \"sizeof\")");
        goto error;
    }

    symbol_set_const(s, value);

    if (lang_comment(token) < 0)
    {
        debug_emsg("Unexpected symbols");
        goto error;
    }

    return 0;
error:
    token_print_rollback(token);
    app_close(APP_EXITCODE_ERROR);
    return -1;
}

/*
 *
 */
int lang_directive(struct linker_context_t *ctx, struct token_t *token)
{
    char *tname;

    if (!token_get(token, TOKEN_TYPE_DOT, TOKEN_CURRENT))
        return -1;

    tname = token_get(token, TOKEN_TYPE_SYMBOL, TOKEN_NEXT);
    if (!tname)
    {
        debug_emsg("Symbol name missing in directive name");
        goto error;
    }

    if (strcmp(tname, "print") == 0) {
        int arg;
        int64_t value;
        enum token_number_format_t format;
        char svalue[128];

        arg = 0;

        format = TOKEN_NUMBER_FORMAT_DECIMAL;
        while (1)
        {
            if (lang_constexpr(&ctx->symbols, token, &value) == 0)
            {
                arg = 1;
                switch (format)
                {
                    case TOKEN_NUMBER_FORMAT_DECIMAL:
                        _dot_print("%lld", value);
                        break;
                    case TOKEN_NUMBER_FORMAT_HEX:
                        _dot_print("$%06llX", value);
                        break;
                    case TOKEN_NUMBER_FORMAT_BINARY:
                        lang_util_num2str(value, TOKEN_NUMBER_FORMAT_BINARY, svalue);
                        _dot_print("%s", svalue);
                        break;
                    case TOKEN_NUMBER_FORMAT_OCTAL:
                        lang_util_num2str(value, TOKEN_NUMBER_FORMAT_OCTAL, svalue);
                        _dot_print("%s", svalue);
                        break;
                }
            } else if (token_get(token, TOKEN_TYPE_STRING, TOKEN_NEXT)) {
                arg = 1;
                if (strcmp(token->name, "%") == 0)
                    format = TOKEN_NUMBER_FORMAT_DECIMAL;
                else if (strcmp(token->name, "%$") == 0)
                    format = TOKEN_NUMBER_FORMAT_HEX;
                else if (strcmp(token->name, "%%") == 0)
                    format = TOKEN_NUMBER_FORMAT_BINARY;
                else if (strcmp(token->name, "%~") == 0)
                    format = TOKEN_NUMBER_FORMAT_OCTAL;
                else
                    _dot_print("%s", token->name);
            } else {
                if (!arg)
                {
                    debug_emsg("String or expression should follow \".print\"");
                    goto error;
                } else {
                    _dot_print(NL);
                    break;
                }
            }
        }

    } else if (strcmp(tname, "export") == 0) {
        struct symbol_t *s;

        tname = token_get(token, TOKEN_TYPE_SYMBOL, TOKEN_NEXT);
        if (!tname)
        {
            debug_emsg("Symbol name missing in \".export\" directive");
            goto error;
        }

        s = symbol_find(&ctx->symbols, tname);
        if (!s)
        {
            debug_emsgf("Symbol not found", "\"%s\"" NL, tname);
            goto error;
        }
        if (s->exp) {
            debug_wmsgf("Symbol already exported", "\"%s\"" NL, tname);
            goto error;
        }
        s->exp = 1;
    } else if (strcmp(tname, "place") == 0) {
        struct section_t *s;
        int64_t value;

        if (!(tname = token_get(token, TOKEN_TYPE_STRING, TOKEN_NEXT)))
        {
            debug_emsg("Missing sections name in \"sizeof\" operator");
            goto error;
        }

        s = section_find(&ctx->result.sections, tname);
        if (!s)
        {
            debug_emsgf("Section not found", "\"%s\"" NL, tname);
            goto error;
        }

        if (s->placed)
        {
            debug_emsgf("Section already placed", "\"%s\"" NL, tname);
            goto error;
        }

        if ((tname = token_get(token, TOKEN_TYPE_SYMBOL, TOKEN_NEXT)))
        {
            if (strcmp(tname, "NOLOAD") == 0)
            {
                s->noload = 1;
            } else {
                struct symbol_t *symbol;

                symbol = symbol_find(&ctx->symbols, tname);
                if (!symbol)
                {
                    debug_emsgf("Symbol not defined", SQ NL, tname);
                    goto error;
                }
                s->lma = symbol->val64;
            }
        } else if ((tname = token_get(token, TOKEN_TYPE_NUMBER, TOKEN_NEXT))) {
            if (lang_util_str2num(tname, &value) < 0)
                goto error;
            s->lma = value;
        } else {
            if (lang_constexpr(&ctx->symbols, token, &value) < 0)
            {
                debug_emsg("No valid expression for LMA");
                goto error;
            }
            s->lma = value;
        }

        if ((tname = token_get(token, TOKEN_TYPE_SYMBOL, TOKEN_NEXT)))
        {
            if (strcmp(tname, "NOLOAD") == 0)
            {
                debug_emsg("NOLOAD not permitted for VMA");
                goto error;
            } else {
                struct symbol_t *symbol;

                symbol = symbol_find(&ctx->symbols, tname);
                if (!symbol)
                {
                    debug_emsgf("Symbol not defined", SQ NL, tname);
                    goto error;
                }
                s->vma = symbol->val64;
            }
        } else if ((tname = token_get(token, TOKEN_TYPE_NUMBER, TOKEN_NEXT))) {
            if (lang_util_str2num(tname, &value) < 0)
                goto error;
            s->vma = value;
        } else {

            if (lang_constexpr(&ctx->symbols, token, &value) < 0)
            {
                debug_emsg("No valid expression for VMA");
                goto error;
            }
            s->vma = value;
        }

        s->placed = 1;
    } else if (strcmp(tname, "fill") == 0) {
        struct section_t *s;
        int64_t cnt;
        int64_t fill;
        int8_t v8;


        if (!(tname = token_get(token, TOKEN_TYPE_STRING, TOKEN_NEXT)))
        {
            debug_emsg("Missing sections name in \"sizeof\" operator");
            goto error;
        }

        s = section_find(&ctx->result.sections, tname);
        if (!s)
        {
            debug_emsgf("Section not found", "\"%s\"" NL, tname);
            goto error;
        }

        if ((tname = token_get(token, TOKEN_TYPE_NUMBER, TOKEN_NEXT)))
        {
            if (lang_util_str2num(tname, &cnt) < 0)
                goto error;
        } else if (lang_constexpr(&ctx->symbols, token, &cnt) < 0) {
            debug_emsg("Missing valid number or expression for counter of \".fill\" directive");
            goto error;
        }

        if ((tname = token_get(token, TOKEN_TYPE_NUMBER, TOKEN_NEXT)))
        {
            if (lang_util_str2num(tname, &fill) < 0)
                goto error;
        } else if (lang_constexpr(&ctx->symbols, token, &fill) < 0) {
            debug_emsg("Missing valid number or expression for fill value of \".fill\" directive");
            goto error;
        }

        v8 = fill;
        while (cnt-- > 0)
            section_pushdata(s, &v8, 1);
    } else {
        debug_emsgf("Unknown directive", "\"%s\"" NL, tname);
        goto error;
    }

    if (lang_comment(token) < 0)
    {
        debug_emsg("Unexpected symbols after directive");
        goto error;
    }

    return 0;
error:
    token_print_rollback(token);
    app_close(APP_EXITCODE_ERROR);
    return -1;
}

/*
 *
 */
static void _dot_print(const char *fmt, ...)
{
    va_list va;

    va_start(va, fmt);
    if (!app.noprint)
        vprintf(fmt, va);
    va_end(va);
}

