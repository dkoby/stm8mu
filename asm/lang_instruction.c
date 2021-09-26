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
#include <btorder.h>
#include <lang_constexpr.h>
#include <lang_util.h>
#include "symbol.h"
#include "types.h"
#include "app.h"
#include "lang.h"
#include "assembler.h"
#include "section.h"
#include "symbol.h"

//#define DEBUG_THIS

#ifdef DEBUG_THIS
    #define PRINTF(...) printf(__VA_ARGS__)
#else
    #define PRINTF(...)
#endif

struct arg_t {
    enum arg_type_t {
        ARG_TYPE_NONE = 0,    /*   */
        ARG_TYPE_A,           /* A */
        ARG_TYPE_X,           /* X */
        ARG_TYPE_Y,           /* Y */
        ARG_TYPE_SP,          /* SP */
        ARG_TYPE_XL,          /* XL */
        ARG_TYPE_YL,          /* YL */
        ARG_TYPE_XH,          /* XH */
        ARG_TYPE_YH,          /* YH */
        ARG_TYPE_CC,          /* CC */
        ARG_TYPE_SHORTMEM,    /* CONST.w8 */
        ARG_TYPE_LONGMEM,     /* CONST.w16 */
        ARG_TYPE_EXTMEM,      /* CONST.w24 */
        ARG_TYPE_BYTE,        /* #CONST.w8 */
        ARG_TYPE_WORD,        /* #CONST.w16 */
        ARG_TYPE_OFF_X,       /* (X) */
        ARG_TYPE_OFF_Y,       /* (Y) */
        ARG_TYPE_SHORTOFF_X,  /* (CONST.w8, X) */
        ARG_TYPE_LONGOFF_X,   /* (CONST.w16, X) */
        ARG_TYPE_EXTOFF_X,    /* (CONST.w24, X) */
        ARG_TYPE_SHORTOFF_Y,  /* (CONST.w8, Y) */
        ARG_TYPE_LONGOFF_Y,   /* (CONST.w16, Y) */
        ARG_TYPE_EXTOFF_Y,    /* (CONST.w24, Y) */
        ARG_TYPE_SHORTOFF_SP, /* (CONST.w8, SP) */
        ARG_TYPE_SHORTPTR_X,  /* ([CONST.w8], X) */
        ARG_TYPE_LONGPTR_X,   /* ([CONST.w16], X) */
        ARG_TYPE_SHORTPTR_Y,  /* ([CONST.w8], Y) */
        ARG_TYPE_LONGPTR_Y,   /* ([CONST.w16], Y) */
        ARG_TYPE_SHORTPTR,    /* [CONST.w8] */
        ARG_TYPE_LONGPTR,     /* [CONST.w16] */
    } type;

    int64_t value;
    struct symbol_t *symbol;
};

#define PREBYTE_NONE  0x00 /* not use prebyte */
#define PREBYTE_PDY   0x90
#define PREBYTE_PIX   0x92
#define PREBYTE_PIY   0x91
#define PREBYTE_PWSP  0x72

static char *noarg[] = {
    "break", 
    "ccf", 
    "halt", 
    "iret", 
    "nop", 
    "rcf", 
    "ret", 
    "retf", 
    "rim", 
    "rvf", 
    "sim", 
    "scf", 
    "trap", 
    "wfe", 
    "wfi", 
    "",
};

static int _get_args(struct asm_context_t *ctx, struct arg_t *args, struct token_t *token, int nmax);
static int _assemble(struct asm_context_t *ctx, char *name, struct arg_t *args);

/*
 *
 */
int lang_instruction(struct asm_context_t *ctx, struct token_t *token)
{
#define ARGS_MAX   4
    struct arg_t args[ARGS_MAX];
    char name[TOKEN_STRING_MAX];
    char *tname;
    int first;
    char **narg;

    first = 1;
    while (1)
    {
        tname = token_get(token, TOKEN_TYPE_SYMBOL, first ? TOKEN_CURRENT : TOKEN_NEXT);
        if (!tname)
        {
            if (!first)
            {
                debug_emsg("No instruction follows \"|\"");
                goto error;
            }
            return -1;
        }
        strcpy(name, tname);
        PRINTF("INSTRUCTION %s (line %u)" NL, name, token->file.line);

        /* set all arguments to ARG_TYPE_NONE */
        memset(args, 0, sizeof(struct arg_t) * ARGS_MAX);

        /* check if instruction have arguemnts */
        narg = noarg;
        while (*narg[0])
        {
            if (strcmp(*narg, name) == 0)
                break;
            narg++;
        }

        /* get arguments if necessary */
        if (*narg[0] == 0)
        {
            if (_get_args(ctx, args, token, ARGS_MAX) < 0)
            {
                debug_emsg("Failed to get instruction arguments");
                goto error;
            }
        }

        if (_assemble(ctx, name, args))
            goto error;

        token_drop(token);

        if (!token_get(token, TOKEN_TYPE_OR, TOKEN_NEXT))
        {
            if (lang_comment(token) < 0)
            {
                debug_emsg("Unexpected symbols after instruction");
                goto error;
            } else {
                break;
            }
        }

        first = 0;
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
enum {
    GETARG_RESULT_NOTOKEN,
    GETARG_RESULT_ERROR,
    GETARG_RESULT_OK,
};

static int _get_arg(struct asm_context_t *ctx, struct arg_t *arg, struct token_t *token)
{
    char *tname;
    char name[TOKEN_STRING_MAX];
    struct symbol_t *symbol;
    int64_t value;

    tname = token_get(token, TOKEN_TYPE_SYMBOL, TOKEN_NEXT);
    if (tname)
    {
        if (strcmp(tname, "A") == 0)
            arg->type = ARG_TYPE_A;
        else if (strcmp(tname, "X") == 0)
            arg->type = ARG_TYPE_X;
        else if (strcmp(tname, "Y") == 0)
            arg->type = ARG_TYPE_Y;
        else if (strcmp(tname, "XL") == 0)
            arg->type = ARG_TYPE_XL;
        else if (strcmp(tname, "YL") == 0)
            arg->type = ARG_TYPE_YL;
        else if (strcmp(tname, "XH") == 0)
            arg->type = ARG_TYPE_XH;
        else if (strcmp(tname, "YH") == 0)
            arg->type = ARG_TYPE_YH;
        else if (strcmp(tname, "SP") == 0)
            arg->type = ARG_TYPE_SP;
        else if (strcmp(tname, "CC") == 0)
            arg->type = ARG_TYPE_CC;
        else {
            strcpy(name, tname);
            if (lang_util_question_expand(&ctx->symbols, name) < 0)
                return GETARG_RESULT_ERROR;

            symbol = symbol_find(&ctx->symbols, name);
            if (symbol)
            {
                if (symbol->type == SYMBOL_TYPE_CONST) {
                    arg->value = symbol->val64;
                } else if (symbol->type == SYMBOL_TYPE_EXTERN) {
                    arg->symbol = symbol;
                } else if (symbol->type == SYMBOL_TYPE_LABEL) {
                    arg->symbol = symbol;
                } else {
                    debug_emsgf("Symbol should be extern or label", "%s" NL, name);
                    return GETARG_RESULT_ERROR;
                }
                /* 
                 * NOTE
                 * For constant argument, type selected by symbol width, not by value of constant.
                 */
                if (symbol->width == 1)
                {
                    arg->type = ARG_TYPE_SHORTMEM;
                } else if (symbol->width == 2) {
                    arg->type = ARG_TYPE_LONGMEM;
                } else if (symbol->width == 3) {
                    arg->type = ARG_TYPE_EXTMEM;
                } else {
                    /* NOTREACHED */
                    debug_emsgf("Unknown symbol width", "%s %u" NL, name, symbol->width);
                    return GETARG_RESULT_ERROR;
                }
            } else {
                debug_emsgf("Symbol not found", "\"%s\"" NL, name);
                return GETARG_RESULT_ERROR;
            }
        }
        return GETARG_RESULT_OK;
    }

    tname = token_get(token, TOKEN_TYPE_NUMBER, TOKEN_NEXT);
    if (tname)
    {
        if (lang_util_str2num(tname, &arg->value) < 0)
            return GETARG_RESULT_ERROR;

        if (arg->value < 0x100)
            arg->type = ARG_TYPE_SHORTMEM;
        else if (arg->value < 0x10000)
            arg->type = ARG_TYPE_LONGMEM;
        else if (arg->value >= 0x10000)
            arg->type = ARG_TYPE_EXTMEM;
        return GETARG_RESULT_OK;
    }

    if (lang_constexpr(&ctx->symbols, token, &value) == 0)
    {
        arg->type  = ARG_TYPE_SHORTMEM;
        arg->value = value;
        if (token_get(token, TOKEN_TYPE_DOT, TOKEN_NEXT))
        {
            tname = token_get(token, TOKEN_TYPE_SYMBOL, TOKEN_NEXT);
            if (!tname)
            {
                debug_emsg("Attribute name missing in \"constexpr\" after \".\"");
                return GETARG_RESULT_ERROR;
            }

            if (strcmp(tname, SYMBOL_WIDTH_SHORT) == 0)
                arg->type = ARG_TYPE_SHORTMEM;
            else if (strcmp(tname, SYMBOL_WIDTH_LONG) == 0)
                arg->type = ARG_TYPE_LONGMEM;
            else if (strcmp(tname, SYMBOL_WIDTH_EXT) == 0)
                arg->type = ARG_TYPE_EXTMEM;
            else {
                /* NOTREACHED */
                debug_emsgf("Unknown symbol width", "%s" NL, tname);
                return GETARG_RESULT_ERROR;
            }
        }
        return GETARG_RESULT_OK;
    }

    return GETARG_RESULT_NOTOKEN;
}

/*
 *
 */
static int _get_args(struct asm_context_t *ctx, struct arg_t *args, struct token_t *token, int nmax)
{
    struct arg_t *arg;
    int res;

    arg = args;

    while (1)
    {
        /*
         * ARG_TYPE_A,           A
         * ARG_TYPE_X,           X
         * ARG_TYPE_Y,           Y
         * ARG_TYPE_XL,          XL
         * ARG_TYPE_YL,          YL
         * ARG_TYPE_XH,          XH
         * ARG_TYPE_YH,          YH
         */
        res = _get_arg(ctx, arg, token);
        if (res == GETARG_RESULT_OK)
            goto next;
        if (res == GETARG_RESULT_ERROR)
            return -1;

        /* BYTE, WORD */
        if (token_get(token, TOKEN_TYPE_HASH, TOKEN_NEXT))
        {
            res = _get_arg(ctx, arg, token);
            if (res == GETARG_RESULT_NOTOKEN)
            {
               debug_emsg("Argument missing after \"#\"");
               return -1;
            }
            if (res == GETARG_RESULT_ERROR)
                return -1;
            switch (arg->type)
            {
                case ARG_TYPE_SHORTMEM: arg->type = ARG_TYPE_BYTE; break;
                case ARG_TYPE_LONGMEM:  arg->type = ARG_TYPE_WORD; break;
                default:
                    debug_emsg("Invalid argument type");
                    return -1;
            }
            goto next;
        }

        /*
         * ARG_TYPE_OFF_X,        (X)
         * ARG_TYPE_OFF_Y,        (Y)
         * ARG_TYPE_SHORTOFF_X,   (CONST.w8, X)
         * ARG_TYPE_LONGOFF_X,    (CONST.w16, X)
         * ARG_TYPE_EXTOFF_X,     (CONST.w24, X)
         * ARG_TYPE_SHORTOFF_Y,   (CONST.w8, Y)
         * ARG_TYPE_LONGOFF_Y,    (CONST.w16, Y)
         * ARG_TYPE_EXTOFF_Y,     (CONST.w24, Y)
         * ARG_TYPE_SHORTOFF_SP,  (CONST.w8, SP)
         * ARG_TYPE_SHORTPTR_X,   ([CONST.w8], X)
         * ARG_TYPE_LONGPTR_X,    ([CONST.w16], X)
         * ARG_TYPE_SHORTPTR_Y,   ([CONST.w8], Y)
         */
        if (token_get(token, TOKEN_TYPE_ROUND_OPEN, TOKEN_NEXT))
        {
            int bracket;
            int pretype;

            /* [ */
            bracket = 0;
            if (token_get(token, TOKEN_TYPE_BRACKET_OPEN, TOKEN_NEXT))
                bracket = 1;

            res = _get_arg(ctx, arg, token);
            if (res == GETARG_RESULT_NOTOKEN)
            {
                debug_emsgf("Argument missing after \"%c\"", bracket ? "[" : "(");
                return -1;
            }
            if (res == GETARG_RESULT_ERROR)
                return -1;

            switch (arg->type)
            {
                case ARG_TYPE_X:
                case ARG_TYPE_Y:
                    if (bracket)
                    {
                        debug_emsg("Invalid argument type, extra \"[\" before \"X\" or \"Y\"");
                        return -1;
                    }
                    if (arg->type == ARG_TYPE_X)
                        arg->type = ARG_TYPE_OFF_X;
                    else
                        arg->type = ARG_TYPE_OFF_Y;

                    if (!token_get(token, TOKEN_TYPE_ROUND_CLOSE, TOKEN_NEXT))
                    {
                        debug_emsg("Missing \")\"");
                        return -1;
                    }
                    goto next;
                case ARG_TYPE_SHORTMEM: arg->type = bracket ? ARG_TYPE_SHORTPTR_X : ARG_TYPE_SHORTOFF_X; break;
                case ARG_TYPE_LONGMEM:  arg->type = bracket ? ARG_TYPE_LONGPTR_X  : ARG_TYPE_LONGOFF_X; break;
                case ARG_TYPE_EXTMEM:
                    if (bracket)
                    {
                        debug_emsg("Invalid argument type (extended address on pointer)");
                        return -1;
                    }
                    arg->type = ARG_TYPE_EXTOFF_X;
                    break;
                default:
                    debug_emsg("Invalid argument type");
                    return -1;
            }
            pretype = arg->type;
#if 0
            PRINTF("PRETYPE %u" NL, pretype);
#endif

            /* ] */
            if (bracket && !token_get(token, TOKEN_TYPE_BRACKET_CLOSE, TOKEN_NEXT))
            {
                debug_emsg("Missing \"]\"");
                return -1;
            }

            /* , */
            if (!token_get(token, TOKEN_TYPE_COMMA, TOKEN_NEXT))
            {
                debug_emsg("Missing \",\"");
                return -1;
            }

            /* -, X, Y, SP */
            res = _get_arg(ctx, arg, token);
            if (res == GETARG_RESULT_NOTOKEN)
            {
                debug_emsgf("Argument missing after \"%c\"", bracket ? "[" : "(");
                return -1;
            }
            if (res == GETARG_RESULT_ERROR)
                return -1;
            switch (arg->type)
            {
                case ARG_TYPE_X:
                    if (pretype == ARG_TYPE_SHORTOFF_X)
                        arg->type = ARG_TYPE_SHORTOFF_X;
                    else if (pretype == ARG_TYPE_LONGOFF_X)
                        arg->type = ARG_TYPE_LONGOFF_X;
                    else if (pretype == ARG_TYPE_EXTOFF_X)
                        arg->type = ARG_TYPE_EXTOFF_X;

                    else if (pretype == ARG_TYPE_SHORTPTR_X)
                        arg->type = ARG_TYPE_SHORTPTR_X;
                    else if (pretype == ARG_TYPE_LONGPTR_X)
                        arg->type = ARG_TYPE_LONGPTR_X;
                    else {
                        debug_emsg("Invalid argument after \",\"");
                        printf("%u" NL, pretype);
                        return -1;
                    }
                    break;
                case ARG_TYPE_Y:
                    if (pretype == ARG_TYPE_SHORTOFF_X)
                        arg->type = ARG_TYPE_SHORTOFF_Y;
                    else if (pretype == ARG_TYPE_LONGOFF_X)
                        arg->type = ARG_TYPE_LONGOFF_Y;
                    else if (pretype == ARG_TYPE_EXTOFF_X)
                        arg->type = ARG_TYPE_EXTOFF_Y;
                    else if (pretype == ARG_TYPE_SHORTPTR_X)
                        arg->type = ARG_TYPE_SHORTPTR_Y;
                    else if (pretype == ARG_TYPE_LONGPTR_X)
                        arg->type = ARG_TYPE_LONGPTR_Y;
                    else {
                        debug_emsg("Invalid argument after \",\"");
                        return -1;
                    }
                    break;
                case ARG_TYPE_SP:
                    if (pretype == ARG_TYPE_SHORTOFF_X)
                        arg->type = ARG_TYPE_SHORTOFF_SP;
                    else {
                        debug_emsg("Invalid argument after \",\"");
                        return -1;
                    }
                    break;
                default:
                    debug_emsg("Invalid argument after \",\"");
                    return -1;
            }

            if (!token_get(token, TOKEN_TYPE_ROUND_CLOSE, TOKEN_NEXT))
            {
                debug_emsg("Missing \")\"");
                return -1;
            }

            goto next;
        }

        /* 
         * ARG_TYPE_SHORTPTR,     [CONST.w8]
         * ARG_TYPE_LONGPTR,      [CONST.w16]
         */
        if (token_get(token, TOKEN_TYPE_BRACKET_OPEN, TOKEN_NEXT))
        {
            res = _get_arg(ctx, arg, token);
            if (res == GETARG_RESULT_NOTOKEN)
            {
                debug_emsg("Argument missing after \"[\"");
                return -1;
            }
            if (res == GETARG_RESULT_ERROR)
                return -1;
            switch (arg->type)
            {
                case ARG_TYPE_SHORTMEM: arg->type = ARG_TYPE_SHORTPTR; break;
                case ARG_TYPE_LONGMEM:  arg->type = ARG_TYPE_LONGPTR; break;
                default:
                    debug_emsg("Invalid argument in \"[\" \"]\"");
                    return -1;
            }

            if (!token_get(token, TOKEN_TYPE_BRACKET_CLOSE, TOKEN_NEXT))
            {
                debug_emsg("Missing \"]\"");
                return -1;
            }
            goto next;
        }

        debug_emsg("Unknown argument for instruction");
        return -1;
next:
#ifdef DEBUG_THIS 
        PRINTF("\t");
        switch (arg->type)
        {
            case ARG_TYPE_NONE          : PRINTF("ARG_TYPE_NONE        "); break;
            case ARG_TYPE_A             : PRINTF("ARG_TYPE_A           "); break;
            case ARG_TYPE_X             : PRINTF("ARG_TYPE_X           "); break;
            case ARG_TYPE_Y             : PRINTF("ARG_TYPE_Y           "); break;
            case ARG_TYPE_SP            : PRINTF("ARG_TYPE_SP          "); break;
            case ARG_TYPE_XL            : PRINTF("ARG_TYPE_XL          "); break;
            case ARG_TYPE_YL            : PRINTF("ARG_TYPE_YL          "); break;
            case ARG_TYPE_XH            : PRINTF("ARG_TYPE_XH          "); break;
            case ARG_TYPE_YH            : PRINTF("ARG_TYPE_YH          "); break;
            case ARG_TYPE_CC            : PRINTF("ARG_TYPE_CC          "); break;
            case ARG_TYPE_SHORTMEM      : PRINTF("ARG_TYPE_SHORTMEM    "); break;
            case ARG_TYPE_LONGMEM       : PRINTF("ARG_TYPE_LONGMEM     "); break;
            case ARG_TYPE_EXTMEM        : PRINTF("ARG_TYPE_EXTMEM      "); break;
            case ARG_TYPE_BYTE          : PRINTF("ARG_TYPE_BYTE        "); break;
            case ARG_TYPE_WORD          : PRINTF("ARG_TYPE_WORD        "); break;
            case ARG_TYPE_OFF_X         : PRINTF("ARG_TYPE_OFF_X       "); break;
            case ARG_TYPE_OFF_Y         : PRINTF("ARG_TYPE_OFF_Y       "); break;
            case ARG_TYPE_SHORTOFF_X    : PRINTF("ARG_TYPE_SHORTOFF_X  "); break;
            case ARG_TYPE_LONGOFF_X     : PRINTF("ARG_TYPE_LONGOFF_X   "); break;
            case ARG_TYPE_EXTOFF_X      : PRINTF("ARG_TYPE_EXTOFF_X    "); break;
            case ARG_TYPE_SHORTOFF_Y    : PRINTF("ARG_TYPE_SHORTOFF_Y  "); break;
            case ARG_TYPE_LONGOFF_Y     : PRINTF("ARG_TYPE_LONGOFF_Y   "); break;
            case ARG_TYPE_EXTOFF_Y      : PRINTF("ARG_TYPE_EXTOFF_Y    "); break;
            case ARG_TYPE_SHORTOFF_SP   : PRINTF("ARG_TYPE_SHORTOFF_SP "); break;
            case ARG_TYPE_SHORTPTR_X    : PRINTF("ARG_TYPE_SHORTPTR_X  "); break;
            case ARG_TYPE_LONGPTR_X     : PRINTF("ARG_TYPE_LONGPTR_X   "); break;
            case ARG_TYPE_SHORTPTR_Y    : PRINTF("ARG_TYPE_SHORTPTR_Y  "); break;
            case ARG_TYPE_LONGPTR_Y     : PRINTF("ARG_TYPE_LONGPTR_Y   "); break;
            case ARG_TYPE_SHORTPTR      : PRINTF("ARG_TYPE_SHORTPTR    "); break;
            case ARG_TYPE_LONGPTR       : PRINTF("ARG_TYPE_LONGPTR     "); break;
            default:
                  /* UNREACHED */
                  debug_emsg("Unknown arg type");
                  app_close(APP_EXITCODE_ERROR);
        }
        PRINTF(", VALUE %lld, SYMBOL \"%s\"" NL, arg->value, arg->symbol ? arg->symbol->name : "-");
#endif
        if (!token_get(token, TOKEN_TYPE_COMMA, TOKEN_NEXT))
            return 0;
        arg++;
        nmax--;
        if (nmax <= 1)
        {
            debug_emsg("Too much args for instruction");
            app_close(APP_EXITCODE_ERROR);
            return -1;
        }
    }

    return -1;
}

/*
 *
 */
struct gen_info_t {
    enum arg_type_t arg0;
    enum arg_type_t arg1;
    enum arg_type_t arg2;
    enum arg_type_t arg3;

    uint8_t prebyte;
    uint8_t opcode;
    uint8_t arglen;

#define GEN_FLAG_NONE       0
#define GEN_FLAG_ODD        (1 << 0)
#define GEN_FLAG_EVEN       (0 << 0)
#define GEN_FLAG_CHECK_LONG (1 << 1)
#define GEN_FLAG_ARG_DST    (1 << 2)
#define GEN_FLAG_CHECK_EXT  (1 << 3)
#define GEN_FLAG_END        (1 << 31)    
    uint32_t flag;
};

static const struct gen_info_t _gen_info_adc[] = {
    {ARG_TYPE_A, ARG_TYPE_BYTE       , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xA9, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xB9, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xC9, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_OFF_X      , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xF9, 0, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTOFF_X , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xE9, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGOFF_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xD9, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_OFF_Y      , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xF9, 0, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTOFF_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xE9, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGOFF_Y  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xD9, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTOFF_SP, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x19, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTPTR   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xC9, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGPTR    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xC9, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTPTR_X , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xD9, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGPTR_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xD9, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTPTR_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0xD9, 1, GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_add[] = {
    {ARG_TYPE_A, ARG_TYPE_BYTE       , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xAB, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xBB, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xCB, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_OFF_X      , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xFB, 0, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTOFF_X , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xEB, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGOFF_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xDB, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_OFF_Y      , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xFB, 0, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTOFF_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xEB, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGOFF_Y  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xDB, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTOFF_SP, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x1B, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTPTR   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xCB, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGPTR    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xCB, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTPTR_X , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xDB, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGPTR_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xDB, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTPTR_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0xDB, 1, GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_and[] = {
    {ARG_TYPE_A, ARG_TYPE_BYTE       , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xA4, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xB4, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xC4, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_OFF_X      , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xF4, 0, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTOFF_X , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xE4, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGOFF_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xD4, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_OFF_Y      , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xF4, 0, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTOFF_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xE4, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGOFF_Y  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xD4, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTOFF_SP, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x14, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTPTR   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xC4, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGPTR    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xC4, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTPTR_X , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xD4, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGPTR_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xD4, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTPTR_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0xD4, 1, GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_bcp[] = {
    {ARG_TYPE_A, ARG_TYPE_BYTE       , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xA5, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xB5, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xC5, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_OFF_X      , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xF5, 0, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTOFF_X , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xE5, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGOFF_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xD5, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_OFF_Y      , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xF5, 0, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTOFF_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xE5, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGOFF_Y  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xD5, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTOFF_SP, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x15, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTPTR   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xC5, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGPTR    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xC5, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTPTR_X , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xD5, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGPTR_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xD5, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTPTR_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0xD5, 1, GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_int[] = {
    {ARG_TYPE_EXTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x82, 3, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_ld[] = {
    {ARG_TYPE_A, ARG_TYPE_BYTE       , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xA6, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xB6, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xC6, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_OFF_X      , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xF6, 0, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTOFF_X , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xE6, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGOFF_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xD6, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_OFF_Y      , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xF6, 0, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTOFF_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xE6, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGOFF_Y  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xD6, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTOFF_SP, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x7B, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTPTR   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xC6, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGPTR    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xC6, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTPTR_X , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xD6, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGPTR_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xD6, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTPTR_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0xD6, 1, GEN_FLAG_NONE},

    {ARG_TYPE_SHORTMEM   , ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xB7, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGMEM    , ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xC7, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_OFF_X      , ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xF7, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_X , ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xE7, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_X  , ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xD7, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_OFF_Y      , ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xF7, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_Y , ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xE7, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_Y  , ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xD7, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_SP, ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x6B, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR   , ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xC7, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR    , ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xC7, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},

    {ARG_TYPE_SHORTPTR_X , ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xD7, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR_X  , ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xD7, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_Y , ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0xD7, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},

    {ARG_TYPE_XL, ARG_TYPE_A , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x97, 0, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_XL, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x9F, 0, GEN_FLAG_NONE},
    {ARG_TYPE_YL, ARG_TYPE_A , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x97, 0, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_YL, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x9F, 0, GEN_FLAG_NONE},

    {ARG_TYPE_XH, ARG_TYPE_A , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x95, 0, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_XH, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x9E, 0, GEN_FLAG_NONE},
    {ARG_TYPE_YH, ARG_TYPE_A , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x95, 0, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_YH, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x9E, 0, GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_nop[] = {
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x9D, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_sim[] = {
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x9B, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_rim[] = {
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x9A, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_halt[] = {
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x8E, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_rvf[] = {
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x9C, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_rcf[] = {
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x98, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_scf[] = {
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x99, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_wfi[] = {
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x8F, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_wfe[] = {
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x8F, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_ret[] = {
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x81, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_retf[] = {
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x87, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_mul[] = {
    {ARG_TYPE_X, ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x42, 0, GEN_FLAG_NONE},
    {ARG_TYPE_Y, ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x42, 0, GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_div[] = {
    {ARG_TYPE_X, ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x62, 0, GEN_FLAG_NONE},
    {ARG_TYPE_Y, ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x62, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_exgw[] = {
    {ARG_TYPE_X, ARG_TYPE_Y, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x51, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_divw[] = {
    {ARG_TYPE_X, ARG_TYPE_Y, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x65, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_decw[] = {
    {ARG_TYPE_X, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x5A, 0, GEN_FLAG_NONE},
    {ARG_TYPE_Y, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x5A, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_cplw[] = {
    {ARG_TYPE_X, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x53, 0, GEN_FLAG_NONE},
    {ARG_TYPE_Y, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x53, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_clrw[] = {
    {ARG_TYPE_X, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x5F, 0, GEN_FLAG_NONE},
    {ARG_TYPE_Y, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x5F, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_ccf[] = {
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x8C, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_callf[] = {
    {ARG_TYPE_EXTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x8D, 3, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX, 0x8D, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_break[] = {
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x8B, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_sllw[] = {
    {ARG_TYPE_X, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x58, 0, GEN_FLAG_NONE},
    {ARG_TYPE_Y, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x58, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_sraw[] = {
    {ARG_TYPE_X, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x57, 0, GEN_FLAG_NONE},
    {ARG_TYPE_Y, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x57, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_srlw[] = {
    {ARG_TYPE_X, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x54, 0, GEN_FLAG_NONE},
    {ARG_TYPE_Y, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x54, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_swapw[] = {
    {ARG_TYPE_X, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x5E, 0, GEN_FLAG_NONE},
    {ARG_TYPE_Y, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x5E, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_tnzw[] = {
    {ARG_TYPE_X, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x5D, 0, GEN_FLAG_NONE},
    {ARG_TYPE_Y, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x5D, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_trap[] = {
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x83, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_rrcw[] = {
    {ARG_TYPE_X, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x56, 0, GEN_FLAG_NONE},
    {ARG_TYPE_Y, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x56, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_rrwa[] = {
    {ARG_TYPE_X, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x01, 0, GEN_FLAG_NONE},
    {ARG_TYPE_Y, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x01, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_rlwa[] = {
    {ARG_TYPE_X, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x02, 0, GEN_FLAG_NONE},
    {ARG_TYPE_Y, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x02, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_rlcw[] = {
    {ARG_TYPE_X, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x59, 0, GEN_FLAG_NONE},
    {ARG_TYPE_Y, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x59, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_iret[] = {
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x80, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_call[] = {
    {ARG_TYPE_SHORTMEM  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xCD, 2, GEN_FLAG_ARG_DST | GEN_FLAG_CHECK_LONG},
    {ARG_TYPE_LONGMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xCD, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_OFF_X     , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xFD, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_X, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xED, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_X , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xDD, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_OFF_Y     , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xFD, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_Y, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xED, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xDD, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},

    {ARG_TYPE_SHORTPTR  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xCD, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xCD, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_X, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xDD, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR_X , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xDD, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_Y, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0xDD, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_addw[] = {
    {ARG_TYPE_X,  ARG_TYPE_BYTE       , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x1C, 2, GEN_FLAG_CHECK_LONG},
    {ARG_TYPE_X,  ARG_TYPE_WORD       , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x1C, 2, GEN_FLAG_NONE},
    {ARG_TYPE_X,  ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xBB, 2, GEN_FLAG_CHECK_LONG},
    {ARG_TYPE_X,  ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xBB, 2, GEN_FLAG_NONE},
    {ARG_TYPE_X,  ARG_TYPE_SHORTOFF_SP, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xFB, 1, GEN_FLAG_NONE},
    {ARG_TYPE_Y,  ARG_TYPE_BYTE       , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xA9, 2, GEN_FLAG_CHECK_LONG},
    {ARG_TYPE_Y,  ARG_TYPE_WORD       , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xA9, 2, GEN_FLAG_NONE},
    {ARG_TYPE_Y,  ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xB9, 2, GEN_FLAG_CHECK_LONG},
    {ARG_TYPE_Y,  ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xB9, 2, GEN_FLAG_NONE},
    {ARG_TYPE_Y,  ARG_TYPE_SHORTOFF_SP, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xF9, 1, GEN_FLAG_NONE},
    {ARG_TYPE_SP, ARG_TYPE_BYTE       , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x5B, 1, GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_clr[] = {
    {ARG_TYPE_A          , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x4F, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x3F, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x5F, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_OFF_X      , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x7F, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_X , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x6F, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x4F, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_OFF_Y      , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x7F, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x6F, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_Y  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x4F, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_SP, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x0F, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0x3F, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR    , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x3F, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_X , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0x6F, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x6F, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0x6F, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_cp[] = {
    {ARG_TYPE_A, ARG_TYPE_BYTE       , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xA1, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xB1, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xC1, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_OFF_X      , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xF1, 0, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTOFF_X , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xE1, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGOFF_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xD1, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_OFF_Y      , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xF1, 0, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTOFF_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xE1, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGOFF_Y  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xD1, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTOFF_SP, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x11, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTPTR   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xC1, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGPTR    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xC1, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTPTR_X , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xD1, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGPTR_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xD1, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTPTR_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0xD1, 1, GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_cpw[] = {
    {ARG_TYPE_X, ARG_TYPE_BYTE       , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xA3, 2, GEN_FLAG_CHECK_LONG | GEN_FLAG_NONE},
    {ARG_TYPE_X, ARG_TYPE_WORD       , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xA3, 2,                       GEN_FLAG_NONE},
    {ARG_TYPE_X, ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xB3, 1,                       GEN_FLAG_NONE},
    {ARG_TYPE_X, ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xC3, 2,                       GEN_FLAG_NONE},
    {ARG_TYPE_X, ARG_TYPE_OFF_Y      , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xF3, 0,                       GEN_FLAG_NONE},
    {ARG_TYPE_X, ARG_TYPE_SHORTOFF_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xE3, 1,                       GEN_FLAG_NONE},
    {ARG_TYPE_X, ARG_TYPE_LONGOFF_Y  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xD3, 2,                       GEN_FLAG_NONE},
    {ARG_TYPE_X, ARG_TYPE_SHORTOFF_SP, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x13, 1,                       GEN_FLAG_NONE},
    {ARG_TYPE_X, ARG_TYPE_SHORTPTR   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xC3, 1,                       GEN_FLAG_NONE},
    {ARG_TYPE_X, ARG_TYPE_LONGPTR    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xC3, 2,                       GEN_FLAG_NONE},
    {ARG_TYPE_X, ARG_TYPE_SHORTPTR_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0xD3, 1,                       GEN_FLAG_NONE},

    {ARG_TYPE_Y, ARG_TYPE_BYTE       , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xA3, 2, GEN_FLAG_CHECK_LONG | GEN_FLAG_NONE},
    {ARG_TYPE_Y, ARG_TYPE_WORD       , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xA3, 2,                       GEN_FLAG_NONE},
    {ARG_TYPE_Y, ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xB3, 1,                       GEN_FLAG_NONE},
    {ARG_TYPE_Y, ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xC3, 2,                       GEN_FLAG_NONE},
    {ARG_TYPE_Y, ARG_TYPE_OFF_X      , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xF3, 0,                       GEN_FLAG_NONE},
    {ARG_TYPE_Y, ARG_TYPE_SHORTOFF_X , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xE3, 1,                       GEN_FLAG_NONE},
    {ARG_TYPE_Y, ARG_TYPE_LONGOFF_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xD3, 2,                       GEN_FLAG_NONE},
    {ARG_TYPE_Y, ARG_TYPE_SHORTPTR   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0xC3, 1,                       GEN_FLAG_NONE},
    {ARG_TYPE_Y, ARG_TYPE_SHORTPTR_X , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xD3, 1,                       GEN_FLAG_NONE},
    {ARG_TYPE_Y, ARG_TYPE_LONGPTR_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xD3, 2,                       GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_cpl[] = {
    {ARG_TYPE_A          , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x43, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x33, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x53, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_OFF_X      , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x73, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_X , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x63, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x43, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_OFF_Y      , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x73, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x63, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_Y  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x43, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_SP, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x03, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0x33, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR    , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x33, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_X , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0x63, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x63, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0x63, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_dec[] = {
    {ARG_TYPE_A          , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x4A, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x3A, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x5A, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_OFF_X      , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x7A, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_X , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x6A, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x4A, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_OFF_Y      , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x7A, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x6A, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_Y  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x4A, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_SP, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x0A, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0x3A, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR    , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x3A, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_X , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0x6A, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x6A, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0x6A, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_exg[] = {
    {ARG_TYPE_A, ARG_TYPE_XL      , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x41, 0,                       GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_YL      , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x61, 0,                       GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x31, 2, GEN_FLAG_CHECK_LONG | GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGMEM , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x31, 2, GEN_FLAG_CHECK_LONG | GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_inc[] = {
    {ARG_TYPE_A          , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x4C, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x3C, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x5C, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_OFF_X      , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x7C, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_X , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x6C, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x4C, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_OFF_Y      , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x7C, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x6C, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_Y  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x4C, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_SP, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x0C, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0x3C, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR    , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x3C, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_X , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0x6C, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x6C, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0x6C, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_incw[] = {
    {ARG_TYPE_X, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x5C, 0, GEN_FLAG_NONE},
    {ARG_TYPE_Y, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x5C, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_jp[] = {
    {ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xCC, 2, GEN_FLAG_ARG_DST | GEN_FLAG_CHECK_LONG | GEN_FLAG_NONE},
    {ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xCC, 2, GEN_FLAG_ARG_DST |                       GEN_FLAG_NONE},
    {ARG_TYPE_OFF_X      , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xFC, 0,                                          GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_X , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xEC, 1, GEN_FLAG_ARG_DST |                       GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xDC, 2, GEN_FLAG_ARG_DST |                       GEN_FLAG_NONE},
    {ARG_TYPE_OFF_Y      , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xFC, 0,                                          GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xEC, 1, GEN_FLAG_ARG_DST |                       GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_Y  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xDC, 2, GEN_FLAG_ARG_DST |                       GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xCC, 1, GEN_FLAG_ARG_DST |                       GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR    , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xCC, 2, GEN_FLAG_ARG_DST |                       GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_X , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xDC, 1, GEN_FLAG_ARG_DST |                       GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xDC, 2, GEN_FLAG_ARG_DST |                       GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0xDC, 1, GEN_FLAG_ARG_DST |                       GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_jpf[] = {
    {ARG_TYPE_SHORTMEM , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xAC, 3, GEN_FLAG_ARG_DST | GEN_FLAG_CHECK_EXT  | GEN_FLAG_NONE},
    {ARG_TYPE_LONGMEM  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xAC, 3, GEN_FLAG_ARG_DST | GEN_FLAG_CHECK_EXT  | GEN_FLAG_NONE},
    {ARG_TYPE_EXTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xAC, 3, GEN_FLAG_ARG_DST |                       GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xAC, 2, GEN_FLAG_ARG_DST | GEN_FLAG_CHECK_LONG | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xAC, 2, GEN_FLAG_ARG_DST |                       GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_ldf[] = {
    {ARG_TYPE_A, ARG_TYPE_SHORTMEM  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xBC, 3, GEN_FLAG_CHECK_EXT  | GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xBC, 3, GEN_FLAG_CHECK_EXT  | GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_EXTMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xBC, 3,                       GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTOFF_X, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xAF, 3, GEN_FLAG_CHECK_EXT  | GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGOFF_X , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xAF, 3, GEN_FLAG_CHECK_EXT  | GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_EXTOFF_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xAF, 3,                       GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTOFF_Y, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xAF, 3, GEN_FLAG_CHECK_EXT  | GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGOFF_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xAF, 3, GEN_FLAG_CHECK_EXT  | GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_EXTOFF_Y  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xAF, 3,                       GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTPTR_X, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xAF, 2, GEN_FLAG_CHECK_LONG | GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGPTR_X , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xAF, 2,                       GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTPTR_Y, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0xAF, 2, GEN_FLAG_CHECK_LONG | GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGPTR_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0xAF, 2,                       GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTPTR  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xBC, 2, GEN_FLAG_CHECK_LONG | GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGPTR   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xBC, 2,                       GEN_FLAG_NONE},

    {ARG_TYPE_SHORTMEM  , ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xBD, 3, GEN_FLAG_ARG_DST | GEN_FLAG_CHECK_EXT  | GEN_FLAG_NONE},
    {ARG_TYPE_LONGMEM   , ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xBD, 3, GEN_FLAG_ARG_DST | GEN_FLAG_CHECK_EXT  | GEN_FLAG_NONE},
    {ARG_TYPE_EXTMEM    , ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xBD, 3, GEN_FLAG_ARG_DST |                       GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_X, ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xA7, 3, GEN_FLAG_ARG_DST | GEN_FLAG_CHECK_EXT  | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_X , ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xA7, 3, GEN_FLAG_ARG_DST | GEN_FLAG_CHECK_EXT  | GEN_FLAG_NONE},
    {ARG_TYPE_EXTOFF_X  , ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xA7, 3, GEN_FLAG_ARG_DST |                       GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_Y, ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xA7, 3, GEN_FLAG_ARG_DST | GEN_FLAG_CHECK_EXT  | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_Y , ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xA7, 3, GEN_FLAG_ARG_DST | GEN_FLAG_CHECK_EXT  | GEN_FLAG_NONE},
    {ARG_TYPE_EXTOFF_Y  , ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xA7, 3, GEN_FLAG_ARG_DST |                       GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_X, ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xA7, 2, GEN_FLAG_ARG_DST | GEN_FLAG_CHECK_LONG | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR_X , ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xA7, 2, GEN_FLAG_ARG_DST |                       GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_Y, ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0xA7, 2, GEN_FLAG_ARG_DST | GEN_FLAG_CHECK_LONG | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR_Y , ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0xA7, 2, GEN_FLAG_ARG_DST |                       GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR  , ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xBD, 2, GEN_FLAG_ARG_DST | GEN_FLAG_CHECK_LONG | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR   , ARG_TYPE_A, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xBD, 2, GEN_FLAG_ARG_DST |                       GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_ldw[] = {
    {ARG_TYPE_X       , ARG_TYPE_BYTE       , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xAE, 2, GEN_FLAG_CHECK_LONG | GEN_FLAG_NONE},
    {ARG_TYPE_X       , ARG_TYPE_WORD       , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xAE, 2,                       GEN_FLAG_NONE},
    {ARG_TYPE_X       , ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xBE, 1,                       GEN_FLAG_NONE},
    {ARG_TYPE_X       , ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xCE, 2,                       GEN_FLAG_NONE},
    {ARG_TYPE_X       , ARG_TYPE_OFF_X      , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xFE, 0,                       GEN_FLAG_NONE},
    {ARG_TYPE_X       , ARG_TYPE_SHORTOFF_X , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xEE, 1,                       GEN_FLAG_NONE},
    {ARG_TYPE_X       , ARG_TYPE_LONGOFF_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xDE, 2,                       GEN_FLAG_NONE},
    {ARG_TYPE_X       , ARG_TYPE_SHORTOFF_SP, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x1E, 1,                       GEN_FLAG_NONE},
    {ARG_TYPE_X       , ARG_TYPE_SHORTPTR   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xCE, 1,                       GEN_FLAG_NONE},
    {ARG_TYPE_X       , ARG_TYPE_LONGPTR    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xCE, 2,                       GEN_FLAG_NONE},
    {ARG_TYPE_X       , ARG_TYPE_SHORTPTR_X , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xDE, 1,                       GEN_FLAG_NONE},
    {ARG_TYPE_X       , ARG_TYPE_LONGPTR_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xDE, 2,                       GEN_FLAG_NONE},

    {ARG_TYPE_SHORTMEM   , ARG_TYPE_X          , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xBF, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGMEM    , ARG_TYPE_X          , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xCF, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_OFF_X      , ARG_TYPE_Y          , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xFF, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_X , ARG_TYPE_Y          , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xEF, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_X  , ARG_TYPE_Y          , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xDF, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_SP, ARG_TYPE_X          , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x1F, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR   , ARG_TYPE_X          , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xCF, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR    , ARG_TYPE_X          , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xCF, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_X , ARG_TYPE_Y          , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xDF, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR_X  , ARG_TYPE_Y          , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xDF, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},

    {ARG_TYPE_Y          , ARG_TYPE_BYTE       , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xAE, 2, GEN_FLAG_CHECK_LONG | GEN_FLAG_NONE},
    {ARG_TYPE_Y          , ARG_TYPE_WORD       , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xAE, 2,                       GEN_FLAG_NONE},
    {ARG_TYPE_Y          , ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xBE, 1,                       GEN_FLAG_NONE},
    {ARG_TYPE_Y          , ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xCE, 2,                       GEN_FLAG_NONE},
    {ARG_TYPE_Y          , ARG_TYPE_OFF_Y      , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xFE, 0,                       GEN_FLAG_NONE},
    {ARG_TYPE_Y          , ARG_TYPE_SHORTOFF_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xEE, 1,                       GEN_FLAG_NONE},
    {ARG_TYPE_Y          , ARG_TYPE_LONGOFF_Y  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xDE, 2,                       GEN_FLAG_NONE},
    {ARG_TYPE_Y          , ARG_TYPE_SHORTOFF_SP, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x16, 1,                       GEN_FLAG_NONE},
    {ARG_TYPE_Y          , ARG_TYPE_SHORTPTR   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0xCE, 1,                       GEN_FLAG_NONE},
    {ARG_TYPE_Y          , ARG_TYPE_SHORTPTR_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0xDE, 1,                       GEN_FLAG_NONE},

    {ARG_TYPE_SHORTMEM   , ARG_TYPE_Y          , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xBF, 1, GEN_FLAG_ARG_DST |    GEN_FLAG_NONE},
    {ARG_TYPE_LONGMEM    , ARG_TYPE_Y          , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xCF, 2, GEN_FLAG_ARG_DST |    GEN_FLAG_NONE},
    {ARG_TYPE_OFF_Y      , ARG_TYPE_X          , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xFF, 0,                       GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_Y , ARG_TYPE_X          , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xEF, 1, GEN_FLAG_ARG_DST |    GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_Y  , ARG_TYPE_X          , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xDF, 2, GEN_FLAG_ARG_DST |    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_SP, ARG_TYPE_Y          , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x17, 1, GEN_FLAG_ARG_DST |    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR   , ARG_TYPE_Y          , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0xCF, 1, GEN_FLAG_ARG_DST |    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_Y , ARG_TYPE_X          , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0xDF, 1, GEN_FLAG_ARG_DST |    GEN_FLAG_NONE},

    {ARG_TYPE_Y          , ARG_TYPE_X          , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x93, 0,                       GEN_FLAG_NONE},
    {ARG_TYPE_X          , ARG_TYPE_Y          , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x93, 0,                       GEN_FLAG_NONE},
    {ARG_TYPE_X          , ARG_TYPE_SP         , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x96, 0,                       GEN_FLAG_NONE},
    {ARG_TYPE_SP         , ARG_TYPE_X          , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x94, 0,                       GEN_FLAG_NONE},
    {ARG_TYPE_Y          , ARG_TYPE_SP         , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x96, 0,                       GEN_FLAG_NONE},
    {ARG_TYPE_SP         , ARG_TYPE_Y          , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x94, 0,                       GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_neg[] = {
    {ARG_TYPE_A          , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x40, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x30, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x50, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_OFF_X      , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x70, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_X , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x60, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x40, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_OFF_Y      , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x70, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x60, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_Y  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x40, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_SP, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0x30, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR    , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x30, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_X , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0x60, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x60, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0x60, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_negw[] = {
    {ARG_TYPE_X   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x50, 0, GEN_FLAG_NONE},
    {ARG_TYPE_Y   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x50, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_or[] = {
    {ARG_TYPE_A, ARG_TYPE_BYTE       , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xAA, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xBA, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xCA, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_OFF_X      , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xFA, 0, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTOFF_X , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xEA, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGOFF_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xDA, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_OFF_Y      , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xFA, 0, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTOFF_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xEA, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGOFF_Y  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xDA, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTOFF_SP, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x1A, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTPTR   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xCA, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGPTR    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xCA, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTPTR_X , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xDA, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGPTR_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xDA, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTPTR_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0xDA, 1, GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_pop[] = {
    {ARG_TYPE_A       , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x84, 0, GEN_FLAG_NONE},
    {ARG_TYPE_CC      , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x86, 0, GEN_FLAG_NONE},
    {ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x32, 2, GEN_FLAG_ARG_DST | GEN_FLAG_CHECK_LONG},
    {ARG_TYPE_LONGMEM , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x32, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_popw[] = {
    {ARG_TYPE_X   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x85, 0, GEN_FLAG_NONE},
    {ARG_TYPE_Y   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x85, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_push[] = {
    {ARG_TYPE_A       , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x88, 0,                    GEN_FLAG_NONE      },
    {ARG_TYPE_CC      , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x8A, 0,                    GEN_FLAG_NONE      },
    {ARG_TYPE_BYTE    , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x4B, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE      },
    {ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x3B, 2, GEN_FLAG_ARG_DST | GEN_FLAG_CHECK_LONG},
    {ARG_TYPE_LONGMEM , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x3B, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE      },

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_pushw[] = {
    {ARG_TYPE_X   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x89, 0, GEN_FLAG_NONE},
    {ARG_TYPE_Y   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x89, 0, GEN_FLAG_NONE},
    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_rlc[] = {
    {ARG_TYPE_A          , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x49, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x39, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x59, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_OFF_X      , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x79, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_X , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x69, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x49, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_OFF_Y      , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x79, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x69, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_Y  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x49, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_SP, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x09, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0x39, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR    , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x39, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_X , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0x69, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x69, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0x69, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_rrc[] = {
    {ARG_TYPE_A          , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x46, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x36, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x56, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_OFF_X      , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x76, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_X , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x66, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x46, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_OFF_Y      , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x76, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x66, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_Y  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x46, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_SP, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x06, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0x36, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR    , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x36, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_X , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0x66, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x66, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0x66, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_sbc[] = {
    {ARG_TYPE_A, ARG_TYPE_BYTE       , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xA2, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xB2, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xC2, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_OFF_X      , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xF2, 0, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTOFF_X , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xE2, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGOFF_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xD2, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_OFF_Y      , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xF2, 0, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTOFF_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xE2, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGOFF_Y  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xD2, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTOFF_SP, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x12, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTPTR   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xC2, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGPTR    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xC2, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTPTR_X , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xD2, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_LONGPTR_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xD2, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A, ARG_TYPE_SHORTPTR_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0xD2, 1, GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_sll[] = {
    {ARG_TYPE_A          , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x48, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x38, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x58, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_OFF_X      , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x78, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_X , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x68, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x48, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_OFF_Y      , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x78, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x68, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_Y  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x48, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_SP, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x08, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0x38, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR    , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x38, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_X , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0x68, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x68, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0x68, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_sra[] = {
    {ARG_TYPE_A          , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x47, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x37, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x57, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_OFF_X      , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x77, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_X , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x67, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x47, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_OFF_Y      , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x77, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x67, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_Y  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x47, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_SP, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x07, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0x37, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR    , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x37, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_X , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0x67, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x67, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0x67, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_srl[] = {
    {ARG_TYPE_A          , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x44, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x34, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x54, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_OFF_X      , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x74, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_X , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x64, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x44, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_OFF_Y      , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x74, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x64, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_Y  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x44, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_SP, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x04, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0x34, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR    , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x34, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_X , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0x64, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x64, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0x64, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_sub[] = {
    {ARG_TYPE_A , ARG_TYPE_BYTE       , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xA0, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xB0, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xC0, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_OFF_X      , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xF0, 0, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_SHORTOFF_X , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xE0, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_LONGOFF_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xD0, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_OFF_Y      , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xF0, 0, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_SHORTOFF_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xE0, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_LONGOFF_Y  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xD0, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_SHORTOFF_SP, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x10, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_SHORTPTR   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xC0, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_LONGPTR    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xC0, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_SHORTPTR_X , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xD0, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_LONGPTR_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xD0, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_SHORTPTR_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0xD0, 1, GEN_FLAG_NONE},
    {ARG_TYPE_SP, ARG_TYPE_BYTE       , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x52, 1, GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_subw[] = {
    {ARG_TYPE_X,  ARG_TYPE_BYTE       , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x1D, 2, GEN_FLAG_CHECK_LONG},
    {ARG_TYPE_X,  ARG_TYPE_WORD       , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x1D, 2, GEN_FLAG_NONE},
    {ARG_TYPE_X,  ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xB0, 2, GEN_FLAG_CHECK_LONG},
    {ARG_TYPE_X,  ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xB0, 2, GEN_FLAG_NONE},
    {ARG_TYPE_X,  ARG_TYPE_SHORTOFF_SP, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xF0, 1, GEN_FLAG_NONE},
    {ARG_TYPE_Y,  ARG_TYPE_BYTE       , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xA2, 2, GEN_FLAG_CHECK_LONG},
    {ARG_TYPE_Y,  ARG_TYPE_WORD       , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xA2, 2, GEN_FLAG_NONE},
    {ARG_TYPE_Y,  ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xB2, 2, GEN_FLAG_CHECK_LONG},
    {ARG_TYPE_Y,  ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xB2, 2, GEN_FLAG_NONE},
    {ARG_TYPE_Y,  ARG_TYPE_SHORTOFF_SP, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xF2, 1, GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_swap[] = {
    {ARG_TYPE_A          , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x4E, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x3E, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x5E, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_OFF_X      , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x7E, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_X , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x6E, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x4E, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_OFF_Y      , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x7E, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x6E, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_Y  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x4E, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_SP, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x0E, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0x3E, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR    , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x3E, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_X , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0x6E, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x6E, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0x6E, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_tnz[] = {
    {ARG_TYPE_A          , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x4D, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x3D, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x5D, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_OFF_X      , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x7D, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_X , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x6D, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x4D, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_OFF_Y      , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x7D, 0,                    GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x6D, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGOFF_Y  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x4D, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTOFF_SP, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x0D, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR   , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0x3D, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR    , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x3D, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_X , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0x6D, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_LONGPTR_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0x6D, 2, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},
    {ARG_TYPE_SHORTPTR_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0x6D, 1, GEN_FLAG_ARG_DST | GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

static const struct gen_info_t _gen_info_xor[] = {
    {ARG_TYPE_A , ARG_TYPE_BYTE       , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xA8, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_SHORTMEM   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xB8, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_LONGMEM    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xC8, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_OFF_X      , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xF8, 0, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_SHORTOFF_X , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xE8, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_LONGOFF_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xD8, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_OFF_Y      , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xF8, 0, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_SHORTOFF_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xE8, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_LONGOFF_Y  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0xD8, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_SHORTOFF_SP, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x18, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_SHORTPTR   , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xC8, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_LONGPTR    , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xC8, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_SHORTPTR_X , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIX , 0xD8, 1, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_LONGPTR_X  , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PWSP, 0xD8, 2, GEN_FLAG_NONE},
    {ARG_TYPE_A , ARG_TYPE_SHORTPTR_Y , ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PIY , 0xD8, 1, GEN_FLAG_NONE},

    {ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x00, 0, GEN_FLAG_END},
};

/*
 *
 */
static int _assemble_uni(struct asm_context_t *ctx, struct arg_t *args, struct gen_info_t *geninfo)
{
    struct gen_info_t *gen;
    struct arg_t *arg;

    for (gen = (struct gen_info_t*)geninfo; !(gen->flag & GEN_FLAG_END); gen++)
    {
        if (args[0].type == gen->arg0 &&
            args[1].type == gen->arg1 &&
            args[2].type == gen->arg2 &&
            args[3].type == gen->arg3)
        {
            if (gen->flag & GEN_FLAG_ARG_DST)
                arg = &args[0];
            else
                arg = &args[1];

            if (gen->prebyte != PREBYTE_NONE)
                section_pushdata(ctx->section, &gen->prebyte, 1);
            section_pushdata(ctx->section, &gen->opcode, 1);
            if (gen->arglen)
            {
                uint64_t value;

                value = 0;
                if (arg->symbol)
                {
                    if ((gen->flag & GEN_FLAG_CHECK_LONG) && arg->type != ARG_TYPE_LONGMEM)
                    {
                        debug_emsgf("Symbol not longmem", SQ NL, arg->symbol->name);
                        return -1;
                    } else if ((gen->flag & GEN_FLAG_CHECK_EXT) && arg->type != ARG_TYPE_EXTMEM) {
                        debug_emsgf("Symbol not extmem", SQ NL, arg->symbol->name);
                        return -1;
                    }

                    relocations_add(&ctx->relocations, ctx->section->name, arg->symbol->name,
                            ctx->section->length, gen->arglen, 0, RELOCATION_TYPE_ABOSULTE);
                } else {
                    if (gen->arglen == 2)
                        value = host_tobe16(arg->value);
                    else if (gen->arglen == 3)
                        value = host_tobe24(arg->value);
                    else
                        value = arg->value;
                }
                section_pushdata(ctx->section, &value, gen->arglen);
            }
            return 0;
        }
    }

    return -1;
}

static const struct gen_info_t _gen_info_callr[] = { {ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0xAD, 1, GEN_FLAG_NONE}, };
static const struct gen_info_t _gen_info_jra[]   = { {ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x20, 1, GEN_FLAG_NONE}, };
static const struct gen_info_t _gen_info_jreq[]  = { {ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x27, 1, GEN_FLAG_NONE}, };
static const struct gen_info_t _gen_info_jrf[]   = { {ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x21, 1, GEN_FLAG_NONE}, };
static const struct gen_info_t _gen_info_jrh[]   = { {ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x29, 1, GEN_FLAG_NONE}, };
static const struct gen_info_t _gen_info_jrih[]  = { {ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x2F, 1, GEN_FLAG_NONE}, };
static const struct gen_info_t _gen_info_jril[]  = { {ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x2E, 1, GEN_FLAG_NONE}, };
static const struct gen_info_t _gen_info_jrm[]   = { {ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x2D, 1, GEN_FLAG_NONE}, };
static const struct gen_info_t _gen_info_jrmi[]  = { {ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x2B, 1, GEN_FLAG_NONE}, };
static const struct gen_info_t _gen_info_jrnc[]  = { {ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x24, 1, GEN_FLAG_NONE}, };
static const struct gen_info_t _gen_info_jrne[]  = { {ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x26, 1, GEN_FLAG_NONE}, };
static const struct gen_info_t _gen_info_jrnh[]  = { {ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x28, 1, GEN_FLAG_NONE}, };
static const struct gen_info_t _gen_info_jrnm[]  = { {ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_PDY , 0x2C, 1, GEN_FLAG_NONE}, };
static const struct gen_info_t _gen_info_jrnv[]  = { {ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x28, 1, GEN_FLAG_NONE}, };
static const struct gen_info_t _gen_info_jrpl[]  = { {ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x2A, 1, GEN_FLAG_NONE}, };
static const struct gen_info_t _gen_info_jrsge[] = { {ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x2E, 1, GEN_FLAG_NONE}, };
static const struct gen_info_t _gen_info_jrsgt[] = { {ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x2C, 1, GEN_FLAG_NONE}, };
static const struct gen_info_t _gen_info_jrsle[] = { {ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x2D, 1, GEN_FLAG_NONE}, };
static const struct gen_info_t _gen_info_jrslt[] = { {ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x2F, 1, GEN_FLAG_NONE}, };
static const struct gen_info_t _gen_info_jrt[]   = { {ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x20, 1, GEN_FLAG_NONE}, };
static const struct gen_info_t _gen_info_jruge[] = { {ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x24, 1, GEN_FLAG_NONE}, };
static const struct gen_info_t _gen_info_jrugt[] = { {ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x22, 1, GEN_FLAG_NONE}, };
static const struct gen_info_t _gen_info_jrule[] = { {ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x23, 1, GEN_FLAG_NONE}, };
static const struct gen_info_t _gen_info_jrc[]   = { {ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x25, 1, GEN_FLAG_NONE}, };
static const struct gen_info_t _gen_info_jrult[] = { {ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x25, 1, GEN_FLAG_NONE}, };
static const struct gen_info_t _gen_info_jrv[]   = { {ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, ARG_TYPE_NONE, ARG_TYPE_NONE, PREBYTE_NONE, 0x29, 1, GEN_FLAG_NONE}, };

/*
 *
 */
static int _assemble_jr(struct asm_context_t *ctx, struct arg_t *args, struct gen_info_t *geninfo)
{
    struct gen_info_t *gen;
    struct arg_t *arg;

    if (args[2].type != ARG_TYPE_NONE)
        return -1;

    arg = &args[0];
    gen = (struct gen_info_t*)geninfo;
    if (args[0].type == gen->arg0 &&
        args[1].type == gen->arg1 &&
        args[2].type == gen->arg2 &&
        args[3].type == gen->arg3)
    {
        if (gen->prebyte != PREBYTE_NONE)
            section_pushdata(ctx->section, &gen->prebyte, 1);
        section_pushdata(ctx->section, &gen->opcode, 1);
        if (gen->arglen)
        {
            uint64_t value;

            value = 0;
            if (arg->symbol)
            {
                relocations_add(&ctx->relocations, ctx->section->name, arg->symbol->name,
                        ctx->section->length, gen->arglen, 1 /* adjust */, RELOCATION_TYPE_RELATIVE);
            } else {
                if (value > 0xff)
                {
                    debug_emsg("Rel out of range");
                    return -1;
                }

                value = host_tole64(arg->value);
            }
            section_pushdata(ctx->section, &value, gen->arglen);
        }
        return 0;
    }

    return -1;
}

static const struct gen_info_t _gen_info_btjt[] = { {ARG_TYPE_LONGMEM, ARG_TYPE_BYTE, ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, PREBYTE_PWSP, 0x00, 0, GEN_FLAG_EVEN }, };
static const struct gen_info_t _gen_info_btjf[] = { {ARG_TYPE_LONGMEM, ARG_TYPE_BYTE, ARG_TYPE_SHORTMEM, ARG_TYPE_NONE, PREBYTE_PWSP, 0x00, 0, GEN_FLAG_ODD  }, };
static const struct gen_info_t _gen_info_bset[] = { {ARG_TYPE_LONGMEM, ARG_TYPE_BYTE, ARG_TYPE_NONE    , ARG_TYPE_NONE, PREBYTE_PWSP, 0x10, 0, GEN_FLAG_EVEN }, };
static const struct gen_info_t _gen_info_bres[] = { {ARG_TYPE_LONGMEM, ARG_TYPE_BYTE, ARG_TYPE_NONE    , ARG_TYPE_NONE, PREBYTE_PWSP, 0x10, 0, GEN_FLAG_ODD  }, };
static const struct gen_info_t _gen_info_bccm[] = { {ARG_TYPE_LONGMEM, ARG_TYPE_BYTE, ARG_TYPE_NONE    , ARG_TYPE_NONE, PREBYTE_PDY , 0x10, 0, GEN_FLAG_ODD  }, };
static const struct gen_info_t _gen_info_bcpl[] = { {ARG_TYPE_LONGMEM, ARG_TYPE_BYTE, ARG_TYPE_NONE    , ARG_TYPE_NONE, PREBYTE_PDY , 0x10, 0, GEN_FLAG_EVEN }, };

/*
 *
 */
static int _assemble_bit(struct asm_context_t *ctx, struct arg_t *args, struct gen_info_t *gen)
{
    struct arg_t *argmem, *argbit, *arglabel;

    if (((gen->arg0 != ARG_TYPE_NONE) && args[0].type == ARG_TYPE_NONE) ||
         gen->arg1 != args[1].type ||
         gen->arg2 != args[2].type)
    {
        return -1;
    }

    argmem = &args[0];

    if (args[1].type != ARG_TYPE_BYTE)
        return -1;
    argbit = &args[1];

    arglabel = NULL;
    if (args[2].type != ARG_TYPE_NONE)
        arglabel = &args[2];

    if (1)
    {
        uint8_t opcode;
        uint8_t bit;
        uint64_t value;

        /* prebyte */
        if (gen->prebyte != PREBYTE_NONE)
            section_pushdata(ctx->section, &gen->prebyte, 1);

        /* opcode + n */
        {
            if (argbit->symbol)
            {
                if (argbit->symbol->type != SYMBOL_TYPE_CONST)
                {
                    debug_emsgf("Symbol not constant", SQ NL, argbit->symbol->name);
                    return -1;
                }
                bit = argbit->symbol->val64;
            } else {
                bit = argbit->value;
            }
            if (bit < 0 || bit > 7)
            {
                debug_emsg("Bit out of range (0-7)");
                return -1;
            }

            opcode = gen->opcode;
            if (gen->flag & GEN_FLAG_ODD)
                opcode |= 1 + 2 * bit;
            else
                opcode |= 2 * bit;
            section_pushdata(ctx->section, &opcode, 1);
        }

        /* longmem */
        {
            value = 0;
            if (argmem->symbol)
            {
                if (argmem->type != ARG_TYPE_LONGMEM)
                {
                    debug_emsgf("Symbol not longmem", SQ NL, argmem->symbol->name);
                    return -1;
                }

                relocations_add(&ctx->relocations, ctx->section->name, argmem->symbol->name,
                        ctx->section->length, 2, 0, RELOCATION_TYPE_ABOSULTE);
            } else {
                if (argmem->value < 0 || argmem->value > 0xffff)
                {
                    debug_emsg("Longmem value out of range");
                    return -1;
                }
                value = argmem->value;
            }
            value = host_tobe16(value);
            section_pushdata(ctx->section, &value, 2);
        }

        /* rel */
        if (arglabel)
        {
            value = 0;
            if (arglabel->symbol)
            {
                if (arglabel->type != ARG_TYPE_SHORTMEM)
                {
                    debug_emsgf("Symbol not shortmem", SQ NL, arglabel->symbol->name);
                    return -1;
                }
                relocations_add(&ctx->relocations, ctx->section->name, arglabel->symbol->name,
                        ctx->section->length, 1, 1 /* adjust */, RELOCATION_TYPE_RELATIVE);
            } else {
                value = host_tole64(arglabel->value);
            }
            section_pushdata(ctx->section, &value, 1);
        }

        return 0;
    }

    return -1;
}

/*
 *
 */
static int _assemble_mov(struct asm_context_t *ctx, struct arg_t *args, struct gen_info_t *geninfo)
{
    uint8_t opcode;
    int64_t val0, val1;

    if (args[0].type == ARG_TYPE_NONE || 
        args[1].type == ARG_TYPE_NONE ||
        args[2].type != ARG_TYPE_NONE)
        return -1;

    val0 = 0;
    val1 = 0;
    if (args[0].type == ARG_TYPE_LONGMEM)
    {
        if (args[1].type == ARG_TYPE_BYTE)
        {
            opcode = 0x35;
            section_pushdata(ctx->section, &opcode, 1);

            if (args[1].symbol)
            {
                relocations_add(&ctx->relocations, ctx->section->name, args[1].symbol->name,
                        ctx->section->length, 1, 0, RELOCATION_TYPE_ABOSULTE);
            } else {
                val1 = args[1].value;
            }
            val1 = host_tole64(val1);
            section_pushdata(ctx->section, &val1, 1);

            if (args[0].symbol)
            {
                relocations_add(&ctx->relocations, ctx->section->name, args[0].symbol->name,
                        ctx->section->length, 2, 0, RELOCATION_TYPE_ABOSULTE);
            } else {
                val0 = args[0].value;
            }
            val0 = host_tobe16(val0);
            section_pushdata(ctx->section, &val0, 2);

            return 0;
        } else if (args[1].type == ARG_TYPE_SHORTMEM || args[1].type == ARG_TYPE_LONGMEM) {
            opcode = 0x55;

            section_pushdata(ctx->section, &opcode, 1);

            val0 = 0;
            val1 = 0;
            if (args[1].type == ARG_TYPE_LONGMEM)
            {
                if (args[1].symbol)
                {
                    relocations_add(&ctx->relocations, ctx->section->name, args[1].symbol->name,
                            ctx->section->length, 2, 0, RELOCATION_TYPE_ABOSULTE);
                } else {
                    val1 = args[1].value;
                }
            } else {
                if (args[1].symbol)
                {
                    debug_emsgf("Symbol not longmem", SQ NL, args[1].symbol->name);
                    return -1;
                }
                val1 = args[1].value;
            }
            val1 = host_tobe16(val1);
            section_pushdata(ctx->section, &val1, 2);

            if (args[0].symbol)
            {
                relocations_add(&ctx->relocations, ctx->section->name, args[0].symbol->name,
                        ctx->section->length, 2, 0, RELOCATION_TYPE_ABOSULTE);
            } else {
                val0 = args[0].value;
            }
            val0 = host_tobe16(val0);
            section_pushdata(ctx->section, &val0, 2);
            return 0;
        } else {
            return -1;
        }
    } else if (args[0].type == ARG_TYPE_SHORTMEM) {
        if (!args[0].symbol && args[1].type == ARG_TYPE_BYTE) {
            opcode = 0x35;
            section_pushdata(ctx->section, &opcode, 1);

            if (args[1].symbol)
            {
                relocations_add(&ctx->relocations, ctx->section->name, args[1].symbol->name,
                        ctx->section->length, 1, 0, RELOCATION_TYPE_ABOSULTE);
            } else {
                val1 = args[1].value;
            }
            val1 = host_tole64(val1);
            section_pushdata(ctx->section, &val1, 1);

            val0 = args[0].value;
            val0 = host_tobe16(val0);
            section_pushdata(ctx->section, &val0, 2);

            return 0;
        } else if (args[1].type == ARG_TYPE_SHORTMEM) {
            opcode = 0x45;

            section_pushdata(ctx->section, &opcode, 1);

            if (args[1].symbol)
            {
                relocations_add(&ctx->relocations, ctx->section->name, args[1].symbol->name,
                        ctx->section->length, 1, 0, RELOCATION_TYPE_ABOSULTE);
            } else {
                val1 = args[1].value;
            }
            val1 = host_tole64(val1);
            section_pushdata(ctx->section, &val1, 1);

            if (args[0].symbol)
            {
                relocations_add(&ctx->relocations, ctx->section->name, args[0].symbol->name,
                        ctx->section->length, 1, 0, RELOCATION_TYPE_ABOSULTE);
            } else {
                val0 = args[0].value;
            }
            val0 = host_tole64(val0);
            section_pushdata(ctx->section, &val0, 1);

            return 0;
        } else if (!args[0].symbol && args[1].type == ARG_TYPE_LONGMEM) {
            opcode = 0x55;

            section_pushdata(ctx->section, &opcode, 1);

            if (args[1].symbol)
            {
                relocations_add(&ctx->relocations, ctx->section->name, args[1].symbol->name,
                        ctx->section->length, 2, 0, RELOCATION_TYPE_ABOSULTE);
            } else {
                val1 = args[1].value;
            }
            val1 = host_tobe16(val1);
            section_pushdata(ctx->section, &val1, 2);

            val0 = args[0].value;
            val0 = host_tobe16(val0);
            section_pushdata(ctx->section, &val0, 2);

            return 0;
        } else {
            return -1;
        }
    }

    return -1;
}

/*
 *
 */
struct gen_functions_t {
    char *name;
    int (*func)(struct asm_context_t *, struct arg_t *, struct gen_info_t *);
    struct gen_info_t *geninfo;
};

static const struct gen_functions_t _gen_functions[] = {
    {"adc"  , _assemble_uni , (struct gen_info_t*)_gen_info_adc},
    {"add"  , _assemble_uni , (struct gen_info_t*)_gen_info_add},
    {"addw" , _assemble_uni , (struct gen_info_t*)_gen_info_addw},
    {"and"  , _assemble_uni , (struct gen_info_t*)_gen_info_and},
    {"bccm" , _assemble_bit , (struct gen_info_t*)_gen_info_bccm},
    {"bcp"  , _assemble_uni , (struct gen_info_t*)_gen_info_bcp},
    {"bcpl" , _assemble_bit , (struct gen_info_t*)_gen_info_bcpl},
    {"break", _assemble_uni , (struct gen_info_t*)_gen_info_break},
    {"bres" , _assemble_bit , (struct gen_info_t*)_gen_info_bres},
    {"bset" , _assemble_bit , (struct gen_info_t*)_gen_info_bset},
    {"btjf" , _assemble_bit , (struct gen_info_t*)_gen_info_btjf},
    {"btjt" , _assemble_bit , (struct gen_info_t*)_gen_info_btjt},
    {"call" , _assemble_uni , (struct gen_info_t*)_gen_info_call},
    {"callf", _assemble_uni , (struct gen_info_t*)_gen_info_callf},
    {"callr", _assemble_jr  , (struct gen_info_t*)_gen_info_callr},
    {"ccf"  , _assemble_uni , (struct gen_info_t*)_gen_info_ccf},
    {"clr"  , _assemble_uni , (struct gen_info_t*)_gen_info_clr},
    {"clrw" , _assemble_uni , (struct gen_info_t*)_gen_info_clrw},
    {"cp"   , _assemble_uni , (struct gen_info_t*)_gen_info_cp},
    {"cpw"  , _assemble_uni , (struct gen_info_t*)_gen_info_cpw},
    {"cpl"  , _assemble_uni , (struct gen_info_t*)_gen_info_cpl},
    {"cplw" , _assemble_uni , (struct gen_info_t*)_gen_info_cplw},
    {"dec"  , _assemble_uni , (struct gen_info_t*)_gen_info_dec},
    {"decw" , _assemble_uni , (struct gen_info_t*)_gen_info_decw},
    {"div"  , _assemble_uni , (struct gen_info_t*)_gen_info_div},
    {"divw" , _assemble_uni , (struct gen_info_t*)_gen_info_divw},
    {"exg"  , _assemble_uni , (struct gen_info_t*)_gen_info_exg},
    {"exgw" , _assemble_uni , (struct gen_info_t*)_gen_info_exgw},
    {"halt" , _assemble_uni , (struct gen_info_t*)_gen_info_halt},
    {"inc"  , _assemble_uni , (struct gen_info_t*)_gen_info_inc},
    {"incw" , _assemble_uni , (struct gen_info_t*)_gen_info_incw},
    {"int"  , _assemble_uni , (struct gen_info_t*)_gen_info_int},
    {"iret" , _assemble_uni , (struct gen_info_t*)_gen_info_iret},
    {"jp"   , _assemble_uni , (struct gen_info_t*)_gen_info_jp},
    {"jpf"  , _assemble_uni , (struct gen_info_t*)_gen_info_jpf},
    {"jra"  , _assemble_jr  , (struct gen_info_t*)_gen_info_jra},
    {"jreq" , _assemble_jr  , (struct gen_info_t*)_gen_info_jreq},
    {"jrf"  , _assemble_jr  , (struct gen_info_t*)_gen_info_jrf},
    {"jrh"  , _assemble_jr  , (struct gen_info_t*)_gen_info_jrh},
    {"jrih" , _assemble_jr  , (struct gen_info_t*)_gen_info_jrih},
    {"jril" , _assemble_jr  , (struct gen_info_t*)_gen_info_jril},
    {"jrm"  , _assemble_jr  , (struct gen_info_t*)_gen_info_jrm},
    {"jrmi" , _assemble_jr  , (struct gen_info_t*)_gen_info_jrmi},
    {"jrnc" , _assemble_jr  , (struct gen_info_t*)_gen_info_jrnc},
    {"jrne" , _assemble_jr  , (struct gen_info_t*)_gen_info_jrne},
    {"jrnh" , _assemble_jr  , (struct gen_info_t*)_gen_info_jrnh},
    {"jrnm" , _assemble_jr  , (struct gen_info_t*)_gen_info_jrnm},
    {"jrnv" , _assemble_jr  , (struct gen_info_t*)_gen_info_jrnv},
    {"jrpl" , _assemble_jr  , (struct gen_info_t*)_gen_info_jrpl},
    {"jrsge", _assemble_jr  , (struct gen_info_t*)_gen_info_jrsge},
    {"jrsgt", _assemble_jr  , (struct gen_info_t*)_gen_info_jrsgt},
    {"jrsle", _assemble_jr  , (struct gen_info_t*)_gen_info_jrsle},
    {"jrslt", _assemble_jr  , (struct gen_info_t*)_gen_info_jrslt},
    {"jrt"  , _assemble_jr  , (struct gen_info_t*)_gen_info_jrt},
    {"jruge", _assemble_jr  , (struct gen_info_t*)_gen_info_jruge},
    {"jrugt", _assemble_jr  , (struct gen_info_t*)_gen_info_jrugt},
    {"jrule", _assemble_jr  , (struct gen_info_t*)_gen_info_jrule},
    {"jrc"  , _assemble_jr  , (struct gen_info_t*)_gen_info_jrc},
    {"jrult", _assemble_jr  , (struct gen_info_t*)_gen_info_jrult},
    {"jrv"  , _assemble_jr  , (struct gen_info_t*)_gen_info_jrv},
    {"ld"   , _assemble_uni , (struct gen_info_t*)_gen_info_ld},
    {"ldf"  , _assemble_uni , (struct gen_info_t*)_gen_info_ldf},
    {"ldw"  , _assemble_uni , (struct gen_info_t*)_gen_info_ldw},
    {"mov"  , _assemble_mov , NULL},
    {"neg"  , _assemble_uni , (struct gen_info_t*)_gen_info_neg},
    {"negw" , _assemble_uni , (struct gen_info_t*)_gen_info_negw},
    {"mul"  , _assemble_uni , (struct gen_info_t*)_gen_info_mul},
    {"nop"  , _assemble_uni , (struct gen_info_t*)_gen_info_nop},
    {"or"   , _assemble_uni , (struct gen_info_t*)_gen_info_or},
    {"pop"  , _assemble_uni , (struct gen_info_t*)_gen_info_pop},
    {"popw" , _assemble_uni , (struct gen_info_t*)_gen_info_popw},
    {"push" , _assemble_uni , (struct gen_info_t*)_gen_info_push},
    {"pushw", _assemble_uni , (struct gen_info_t*)_gen_info_pushw},
    {"rcf"  , _assemble_uni , (struct gen_info_t*)_gen_info_rcf},
    {"ret"  , _assemble_uni , (struct gen_info_t*)_gen_info_ret},
    {"retf" , _assemble_uni , (struct gen_info_t*)_gen_info_retf},
    {"rim"  , _assemble_uni , (struct gen_info_t*)_gen_info_rim},
    {"rlc"  , _assemble_uni , (struct gen_info_t*)_gen_info_rlc},
    {"rlcw" , _assemble_uni , (struct gen_info_t*)_gen_info_rlcw},
    {"rlwa" , _assemble_uni , (struct gen_info_t*)_gen_info_rlwa},
    {"rrc"  , _assemble_uni , (struct gen_info_t*)_gen_info_rrc},
    {"rrcw" , _assemble_uni , (struct gen_info_t*)_gen_info_rrcw},
    {"rrwa" , _assemble_uni , (struct gen_info_t*)_gen_info_rrwa},
    {"rvf"  , _assemble_uni , (struct gen_info_t*)_gen_info_rvf},
    {"sbc"  , _assemble_uni , (struct gen_info_t*)_gen_info_sbc},
    {"scf"  , _assemble_uni , (struct gen_info_t*)_gen_info_scf},
    {"sim"  , _assemble_uni , (struct gen_info_t*)_gen_info_sim},
    {"sll"  , _assemble_uni , (struct gen_info_t*)_gen_info_sll},
    {"sla"  , _assemble_uni , (struct gen_info_t*)_gen_info_sll},
    {"sllw" , _assemble_uni , (struct gen_info_t*)_gen_info_sllw},
    {"slaw" , _assemble_uni , (struct gen_info_t*)_gen_info_sllw},
    {"sra"  , _assemble_uni , (struct gen_info_t*)_gen_info_sra},
    {"sraw" , _assemble_uni , (struct gen_info_t*)_gen_info_sraw},
    {"srl"  , _assemble_uni , (struct gen_info_t*)_gen_info_srl},
    {"srlw" , _assemble_uni , (struct gen_info_t*)_gen_info_srlw},
    {"sub"  , _assemble_uni , (struct gen_info_t*)_gen_info_sub},
    {"subw" , _assemble_uni , (struct gen_info_t*)_gen_info_subw},
    {"swap" , _assemble_uni , (struct gen_info_t*)_gen_info_swap},
    {"swapw", _assemble_uni , (struct gen_info_t*)_gen_info_swapw},
    {"tnz"  , _assemble_uni , (struct gen_info_t*)_gen_info_tnz},
    {"tnzw" , _assemble_uni , (struct gen_info_t*)_gen_info_tnzw},
    {"trap" , _assemble_uni , (struct gen_info_t*)_gen_info_trap},
    {"wfi"  , _assemble_uni , (struct gen_info_t*)_gen_info_wfi},
    {"wfe"  , _assemble_uni , (struct gen_info_t*)_gen_info_wfe},
    {"xor"  , _assemble_uni , (struct gen_info_t*)_gen_info_xor},
    {NULL, NULL},
};

/*
 * 
 */
static int _assemble(struct asm_context_t *ctx, char *name, struct arg_t *args)
{
    struct gen_functions_t *gf;

    for (gf = (struct gen_functions_t*)_gen_functions; gf->name; gf++)
    {
        if (strcmp(gf->name, name) != 0)
            continue;

        if ((*gf->func)(ctx, args, gf->geninfo) < 0)
        {
            debug_emsgf("Invalid arguments to instruction", "\"%s\"" NL, name);
            return -1;
        } else {
            return 0;
        }
    }

    debug_emsgf("Unknown instruction", "%s" NL, name);
    return -1;
}

