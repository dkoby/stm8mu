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
#include <string.h>
/* */
#include <debug.h>
#include <btorder.h>
#include <lang_constexpr.h>
#include <lang_util.h>
#include "app.h"
#include "lang.h"
#include "assembler.h"
#include "section.h"

static int _lang_db(struct asm_context_t *ctx, struct token_t *token, int width);
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
int lang_label(struct asm_context_t *ctx, struct token_t *token)
{
    char *tname;
    char name[TOKEN_STRING_MAX];
    char attr[TOKEN_STRING_MAX];
    struct symbol_t *s;

    if (!(tname = token_get(token, TOKEN_TYPE_SYMBOL, TOKEN_CURRENT)))
        return -1;

    s = symbol_find(&ctx->symbols, tname);
    if (s)
    {
        if ((s->type == SYMBOL_TYPE_LABEL && ctx->pass == 0) ||
            s->type != SYMBOL_TYPE_LABEL)
        {
            debug_emsgf("Symbol already exists", "%s" NL, tname);
            goto error;
        }
    }
    strcpy(name, tname);

    *attr = 0;
    if (!token_get(token, TOKEN_TYPE_DOT, TOKEN_NEXT))
    {
        if (!token_get(token, TOKEN_TYPE_COLON, TOKEN_NEXT))
            return -1;
    } else {
        tname = token_get(token, TOKEN_TYPE_SYMBOL, TOKEN_NEXT);
        if (!tname)
        {
            debug_emsg("Attribute name missing in label after \".\"");
            goto error;
        }
        strcpy(attr, tname);

        if (!token_get(token, TOKEN_TYPE_COLON, TOKEN_NEXT))
        {
            debug_emsg("Missing \":\"");
            goto error;
        }
    }

    if (ctx->pass == 0)
    {
        s = symbols_add(&ctx->symbols, name);
        s->type = SYMBOL_TYPE_LABEL;
        if (*attr)
            symbol_set_width(s, attr);
    } else {
        s = symbol_find(&ctx->symbols, name);
        if (!s || s->type != SYMBOL_TYPE_LABEL)
        {
            /* NOTREACHED */
            debug_emsgf("Label not found", "%s" NL, name);
            goto error;
        }

        s->val64 = ctx->section->length;
        symbol_set_section(s, ctx->section->name);
    }

    if (lang_comment(token) < 0)
    {
        debug_emsg("Unexpected symbols after label");
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
int lang_directive(struct asm_context_t *ctx, struct token_t *token)
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

    if (strcmp(tname, "define") == 0)
    {
        char name[TOKEN_STRING_MAX];
        char attr[TOKEN_STRING_MAX];
        int64_t value;
        struct symbol_t *s;

        /*
         * Symbol name.
         */
        tname = token_get(token, TOKEN_TYPE_SYMBOL, TOKEN_NEXT);
        if (!tname)
        {
            debug_emsg("Symbol name missing in \"define\"");
            goto error;
        }
        strcpy(name, tname);

        if (symbol_find(&ctx->symbols, name))
        {
            debug_emsgf("Symbol already exists", SQ NL, name);
            goto error;
        }

        /*
         * Attribute name.
         */
        *attr = 0;
        if (token_get(token, TOKEN_TYPE_DOT, TOKEN_NEXT))
        {
            tname = token_get(token, TOKEN_TYPE_SYMBOL, TOKEN_NEXT);
            if (!tname)
            {
                debug_emsg("Attribute name missing in \"define\" after \".\"");
                goto error;
            }

            strcpy(attr, tname);
        }

        value = 0;
        s = symbols_add(&ctx->symbols, name);

        if (lang_constexpr(&ctx->symbols, token, &value) == 0)
        {

        } else if ((tname = token_get(token, TOKEN_TYPE_NUMBER, TOKEN_NEXT))) {
            if (lang_util_str2num(tname, &value) < 0)
                goto error;
        } else if ((tname = token_get(token, TOKEN_TYPE_SYMBOL, TOKEN_NEXT))) {
            struct symbol_t *sd;

            sd = symbol_find(&ctx->symbols, tname);
            if (!sd)
            {
                debug_emsgf("Symbol not exists", SQ NL, tname);
                goto error;
            }
            if (sd->type != SYMBOL_TYPE_CONST)
            {
                debug_emsgf("Symbol not constant", SQ NL, tname);
                goto error;
            }
            value = sd->val64;
        }

        symbol_set_const(s, value);
        if (*attr)
            symbol_set_width(s, attr);
    } else if (strcmp(tname, "print") == 0) {
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
                        _dot_print("$%llX", value);
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
    } else if (strcmp(tname, "extern") == 0) {
        char name[TOKEN_STRING_MAX];
        char attr[TOKEN_STRING_MAX];
        struct symbol_t *s;

        tname = token_get(token, TOKEN_TYPE_SYMBOL, TOKEN_NEXT);
        if (!tname)
        {
            debug_emsg("Symbol name missing in \".extern\" directive");
            goto error;
        }

        if (symbol_find(&ctx->symbols, tname))
        {
            debug_emsgf("Symbol already exists", "%s" NL, tname);
            goto error;
        }

        strcpy(name, tname);

        /*
         * Attribute name.
         */
        *attr = 0;
        if (token_get(token, TOKEN_TYPE_DOT, TOKEN_NEXT))
        {
            tname = token_get(token, TOKEN_TYPE_SYMBOL, TOKEN_NEXT);
            if (!tname)
            {
                debug_emsg("Attribute name missing in \"extern\" after \".\"");
                goto error;
            }

            strcpy(attr, tname);
        }

        s = symbols_add(&ctx->symbols, name);
        s->type = SYMBOL_TYPE_EXTERN;
        if (*attr)
            symbol_set_width(s, attr);
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
            debug_emsgf("Symbol not found", "%s" NL, tname);
            goto error;
        }
        if (s->type != SYMBOL_TYPE_LABEL) {
            debug_emsgf("Only label symbol can be exported", "%s" NL, tname);
            goto error;
        }
        if (s->exp) {
            debug_wmsgf("Symbol already exported", "%s" NL, tname);
        }
        s->exp = 1;
    } else if (strcmp(tname, "section") == 0) {
        int noload;
        struct section_t *s;

        tname = token_get(token, TOKEN_TYPE_STRING, TOKEN_NEXT);
        if (!tname)
        {
            debug_emsg("Section name should follow \".section\" directive");
            goto error;
        }

        s = section_find(&ctx->sections, tname);
        noload = 0;
        if (s)
            noload = s->noload;

        ctx->section = section_select(&ctx->sections, tname);

        tname = token_get(token, TOKEN_TYPE_SYMBOL, TOKEN_NEXT);
        do {
            if (!tname)
            {
                if (s && noload)
                {
                    debug_emsgf("Section redefined without NOLOAD attribute", "\"%s\"" NL, ctx->section->name);
                    goto error;
                }
                break;
            }

            if (strcmp(tname, "NOLOAD") == 0)
            {
                if (s && noload == 0)
                {
                    debug_emsgf("Section redefined with NOLOAD attribute", "\"%s\"" NL, ctx->section->name);
                    goto error;
                }
                ctx->section->noload = 1;
            } else {
                debug_emsgf("Unknown section attribute", "\"%s\"" NL, tname);
                goto error;
            }
        } while (0);
    } else if (strcmp(tname, "include") == 0) {
        tname = token_get(token, TOKEN_TYPE_STRING, TOKEN_NEXT);
        if (!tname) {
            debug_emsg("No file name given after \".include\" directive");
            goto error;
        }

        if (assembler(ctx, tname) < 0)
            goto error;
    } else if (strcmp(tname, "dbendian") == 0) {
        tname = token_get(token, TOKEN_TYPE_STRING, TOKEN_NEXT);
        if (!tname) {
            debug_emsg("No endian value given after \".dbendian\" directive");
            goto error;
        }

        if (strcmp(tname, "big") == 0)
            ctx->dbendian = DB_ENDIAN_BIG;
        else if (strcmp(tname, "little") == 0)
            ctx->dbendian = DB_ENDIAN_LITTLE;
        else {
            debug_emsg("Invalid endian value should be \"big\" or \"little\"");
            goto error;
        }
    } else if (
            strcmp(tname, "d8") == 0 ||
            strcmp(tname, "d16") == 0 ||
            strcmp(tname, "d24") == 0 ||
            strcmp(tname, "d32") == 0 ||
            strcmp(tname, "d64") == 0)
    {
        int width;

        if (strcmp(tname, "d8") == 0)
            width = 1;
        else if (strcmp(tname, "d16") == 0)
            width = 2;
        else if (strcmp(tname, "d24") == 0)
            width = 3;
        else if (strcmp(tname, "d32") == 0)
            width = 4;
        else if (strcmp(tname, "d64") == 0)
            width = 8;
        else {
            /* NOTREACHED */
            debug_emsg("Unsupported .dX");
            goto error;
        }

        if (_lang_db(ctx, token, width) < 0)
        {
            debug_emsg("Error in \".dX\" directive");
            goto error;
        }
    } else if (strcmp(tname, "fill") == 0) {
        int64_t cnt;
        int64_t value;
        uint8_t v8;

        if (lang_constexpr(&ctx->symbols, token, &value) == 0)
        {
            cnt = value;
        } else if ((tname = token_get(token, TOKEN_TYPE_NUMBER, TOKEN_NEXT))) {
            if (lang_util_str2num(tname, &cnt) < 0)
                goto error;
        } else {
            debug_emsg("Missing count of data in \".fill\" directive");
            goto error;
        }

        if (lang_constexpr(&ctx->symbols, token, &value) < 0)
        {
            if ((tname = token_get(token, TOKEN_TYPE_NUMBER, TOKEN_NEXT)))
            {
                if (lang_util_str2num(tname, &value) < 0)
                    goto error;
            } else {
                debug_emsg("Data missing in \".fill\" directive");
                goto error;
            }
        }

        v8 = value;
        while (cnt-- > 0)
            section_pushdata(ctx->section, &v8, 1);
    } else if (
            strcmp(tname, "ifdef") == 0 || strcmp(tname, "ifndef") == 0 ||
            strcmp(tname, "if")    == 0 || strcmp(tname, "ifeq")   == 0 ||  strcmp(tname, "ifneq")   == 0 )
    {
        int64_t value;
        int skipend;

        if (strcmp(tname, "ifdef") == 0 || strcmp(tname, "ifndef") == 0)
        {
            if (strcmp(tname, "ifndef") == 0)
                value = 1;
            else
                value = 0;

            tname = token_get(token, TOKEN_TYPE_SYMBOL, TOKEN_NEXT);
            if (!tname)
            {
                debug_emsg("Symbol name missing in \".if(n)def\" directive");
                goto error;
            }

            value ^= symbol_find(&ctx->symbols, tname) ? 1 : 0;
        } else if (strcmp(tname, "ifeq") == 0 || strcmp(tname, "ifneq") == 0) {
            int64_t val0;
            int64_t val1;

            value = 1;
            if (strcmp(tname, "ifneq") == 0)
                value = 0;

            if (lang_constexpr(&ctx->symbols, token, &val0) < 0)
            {
                debug_emsg("No valid first expression follows \"ifeq\" directive");
                goto error;
            }

            if (lang_constexpr(&ctx->symbols, token, &val1) < 0)
            {
                debug_emsg("No valid second expression follows \"ifeq\" directive");
                goto error;
            }

            value ^= val0 == val1 ? 0 : 1;
        } else {
            if (lang_constexpr(&ctx->symbols, token, &value) < 0)
            {
                debug_emsg("No valid expression follows \"if\" directive");
                goto error;
            }
        }

        skipend = 0;
        if (!value)
        {
            while (1)
            {
                tname = token_get(token, TOKEN_TYPE_LINE, TOKEN_NEXT);
                if (!tname)
                    return 0;

                if (strncmp(tname, ".if", 3) == 0)
                    skipend++;

                if (strncmp(tname, ".endif", 6) == 0)
                {
                    if (!skipend)
                        return 0;
                    skipend--;
                }
            }
        }
    } else if (strcmp(tname, "endif") == 0) {

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
static void _cutvalue(struct asm_context_t *ctx, uint64_t *value, int width)
{
    if (width == 2)
        *value = ctx->dbendian == DB_ENDIAN_BIG ? host_tobe16(*value) : host_tole16(*value);
    else if (width == 3)
        *value = ctx->dbendian == DB_ENDIAN_BIG ? host_tobe24(*value) : host_tole24(*value);
    else if (width == 4)
        *value = ctx->dbendian == DB_ENDIAN_BIG ? host_tobe32(*value) : host_tole32(*value);
    else if (width == 8)
        *value = ctx->dbendian == DB_ENDIAN_BIG ? host_tobe64(*value) : host_tole64(*value);
    else
        *value = host_tole64(*value);
}

/*
 *
 */
static int _lang_db(struct asm_context_t *ctx, struct token_t *token, int width)
{
    char *tname;
    int64_t value;

    while (1)
    {
        tname = token_get(token, TOKEN_TYPE_STRING, TOKEN_NEXT);
        if (tname)
        {
            if (width != 1)
            {
                debug_emsg("String supported only in \".d8\" directive");
                return -1;
            } else {
                section_pushdata(ctx->section, tname, strlen(tname) + 1);
                goto next;
            }
        }

        tname = token_get(token, TOKEN_TYPE_CHAR, TOKEN_NEXT);
        if (tname)
        {
            if (width != 1)
            {
                debug_emsg("Char supported only in \".d8\" directive");
                return -1;
            } else {
                section_pushdata(ctx->section, tname, 1);
                goto next;
            }
        }

        if (lang_constexpr(&ctx->symbols, token, &value) == 0)
        {
            _cutvalue(ctx, (uint64_t*)&value, width);

            section_pushdata(ctx->section, &value, width);
            goto next;
        }

        if ((tname = token_get(token, TOKEN_TYPE_NUMBER, TOKEN_NEXT)))
        {
            if (lang_util_str2num(tname, &value) < 0)
                return -1;

            _cutvalue(ctx, (uint64_t*)&value, width);

            section_pushdata(ctx->section, &value, width);
            goto next;
        }

        if ((tname = token_get(token, TOKEN_TYPE_SYMBOL, TOKEN_NEXT)))
        {
            struct symbol_t *s;

            s = symbol_find(&ctx->symbols, tname);
            if (!s)
            {
                debug_emsgf("Symbol not found", SQ NL, tname);
                return -1;
            }
            if (s->type == SYMBOL_TYPE_CONST)
            {
                value = s->val64;
                _cutvalue(ctx, (uint64_t*)&value, width);
            } else if (s->type == SYMBOL_TYPE_LABEL || s->type == SYMBOL_TYPE_EXTERN) {
                /*
                 * XXX
                 * Linker will be put data in BIGENDIAN. Is it necessary to specify info for linker
                 * about endianess of data?
                 */
                relocations_add(&ctx->relocations,
                        ctx->section->name,
                        s->name,
                        ctx->section->length,
                        width, 0 /* don't care */, RELOCATION_TYPE_ABOSULTE);

                value = 0;
            } else {
                debug_emsgf("Unknown symbol type", SQ NL, tname);
                return -1;
            }

            section_pushdata(ctx->section, &value, width);
            goto next;
        }

        debug_emsg("Unknown \"dX\" construction");
        return -1;
next:
        if (!token_get(token, TOKEN_TYPE_COMMA, TOKEN_NEXT))
            return 0;
    }

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

