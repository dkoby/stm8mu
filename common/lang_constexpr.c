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
#include <lang_util.h>
#include <app_common.h>
#include "lang_constexpr.h"

#if 0
    #define PRINTF(...) printf(__VA_ARGS__)
#else
    #define PRINTF(...)
#endif

static struct {
#define EXPR_STACK_SIZE   1024
    int64_t stack[EXPR_STACK_SIZE];
    int depth;
} exprstack;

static void _exprstack_flush();
static void _exprstack_push(int64_t value);
static int64_t _exprstack_pop();
static void _expr(struct symbols_t *sl, struct token_t *token);
static void _exprr(struct symbols_t *sl, struct token_t *token);
static void _or_opd(struct symbols_t *sl, struct token_t *token);
static void _or_opdr(struct symbols_t *sl, struct token_t *token);
static void _xor_opd(struct symbols_t *sl, struct token_t *token);
static void _xor_opdr(struct symbols_t *sl, struct token_t *token);
static void _and_opd(struct symbols_t *sl, struct token_t *token);
static void _and_opdr(struct symbols_t *sl, struct token_t *token);
static void _shift_opd(struct symbols_t *sl, struct token_t *token);
static void _shift_opdr(struct symbols_t *sl, struct token_t *token);
static void _add_opd(struct symbols_t *sl, struct token_t *token);
static void _add_opdr(struct symbols_t *sl, struct token_t *token);
static void _mul_opd(struct symbols_t *sl, struct token_t *token);
static void _not_opd(struct symbols_t *sl, struct token_t *token);

/*
 *
 */
int lang_constexpr(struct symbols_t *sl, struct token_t *token, int64_t *value)
{
    if (!token_get(token, TOKEN_TYPE_CURLY_OPEN, TOKEN_NEXT))
        return -1;

    /*
     * Expression syntax.
     *
     *     EXPR      = EXPR, "|", OR_OPD | OR_OPD
     *     OR_OPD    = OR_OPD, "^", XOR_OPD | XOR_OPD
     *     XOR_OPD   = XOR_OPD, "&", AND_OPD | AND_OPD
     *     AND_OPD   = AND_OPD, "<<", SHIFT_OPD | AND_OPD, ">>", SHIFT_OPD | SHIFT_OPD
     *     SHIFT_OPD = SHIFT_OPD, "+", ADD_OPD | SHIFT_OPD, "-", ADD_OPD | ADD_OPD
     *     ADD_OPD   = ADD_OPD, "*", MUL_OPD | ADD_OPD, "/", MUL_OPD | ADD_OPD, "%", MUL_OPD | MUL_OPD
     *     MUL_OPD   = "~", MUL_OPD | NOT_OPD
     *     NOT_OPD   = NUMBER | SYMBOL | "(", EXPR, ")"
     *
     * Eliminate of left recursion.
     *
     *     EXPR       = OR_OPD, EXPRR
     *     EXPRR      = "|", OR_OPD, EXPRR | 0
     *     OR_OPD     = XOR_OPD, OR_OPDR
     *     OR_OPDR    = "^", XOR_OPD, OR_OPDR | 0
     *     XOR_OPD    = AND_OPD, XOR_OPDR
     *     XOR_OPDR   = "&", AND_OPD, XOR_OPDR | 0
     *     AND_OPD    = SHIFT_OPD, AND_OPDR
     *     AND_OPDR   = "<<", SHIFT_OPD, AND_OPDR | ">>", SHIFT_OPD, AND_OPDR | 0
     *     SHIFT_OPD  = ADD_OPD, SHIFT_OPDR
     *     SHIFT_OPDR = "+", ADD_OPD, SHIFT_OPDR | "-", ADD_OPD, SHIFT_OPDR | 0
     *     ADD_OPD    = MUL_OPD, ADD_OPDR
     *     ADD_OPDR   = "*", MUL_OPD, ADD_OPDR | "/", MUL_OPD, ADD_OPDR |"%", MUL_OPD, ADD_OPDR | 0
     *     MUL_OPD    = "!", NOT_OPD | NOT_OPD
     *     NOT_OPD    = NUMBER | SYMBOL | "(", EXPR, ")"
     */

    _exprstack_flush();

    _expr(sl, token);

    *value = _exprstack_pop();

    if (!token_get(token, TOKEN_TYPE_CURLY_CLOSE, TOKEN_NEXT))
    {
        debug_emsg("Missing \"}\" in expr");
        token_print_rollback(token);
        app_close(APP_EXITCODE_ERROR);
    }

    token_drop(token);

    return 0;
}

/*
 *
 */
static void _exprstack_flush()
{
    exprstack.depth = 0;
}

/*
 *
 */
static void _exprstack_push(int64_t value)
{
    if (exprstack.depth > EXPR_STACK_SIZE)
    {
        debug_emsg("Expression stack overflow");
        app_close(APP_EXITCODE_ERROR);
    }
    PRINTF("push %llu" NL, value);
    exprstack.stack[exprstack.depth++] = value;
}

/*
 *
 */
static int64_t _exprstack_pop()
{
    if (exprstack.depth <= 0)
    {
        debug_emsg("Expression stack underflow");
        app_close(APP_EXITCODE_ERROR);
    }

    PRINTF("pop  %llu" NL, exprstack.stack[exprstack.depth - 1]);
    return exprstack.stack[--exprstack.depth];
}

/*
 *
 */
static void _expr(struct symbols_t *sl, struct token_t *token)
{
    PRINTF("%s" NL, __FUNCTION__);

    _or_opd(sl, token);
    _exprr(sl, token);
}

/*
 *
 */
static void _exprr(struct symbols_t *sl, struct token_t *token)
{
    int64_t operand1, operand2;

    PRINTF("%s" NL, __FUNCTION__);

    if (!token_get(token, TOKEN_TYPE_OR, TOKEN_NEXT))
        return;

    PRINTF("|" NL);

    _or_opd(sl, token);

    operand2 = _exprstack_pop(sl);
    operand1 = _exprstack_pop(sl);

    _exprstack_push(operand1 | operand2);

    _exprr(sl, token);
}

/*
 *
 */
static void _or_opd(struct symbols_t *sl, struct token_t *token)
{
    PRINTF("%s" NL, __FUNCTION__);

    _xor_opd(sl, token);
    _or_opdr(sl, token);
}

/*
 *
 */
static void _or_opdr(struct symbols_t *sl, struct token_t *token)
{
    int64_t operand1, operand2;

    PRINTF("%s" NL, __FUNCTION__);
    if (!token_get(token, TOKEN_TYPE_XOR, TOKEN_NEXT))
        return;

    PRINTF("^" NL);

    _xor_opd(sl, token);

    operand2 = _exprstack_pop(sl);
    operand1 = _exprstack_pop(sl);

    _exprstack_push(operand1 ^ operand2);

    _or_opdr(sl, token);
}

/*
 *
 */
static void _xor_opd(struct symbols_t *sl, struct token_t *token)
{
    PRINTF("%s" NL, __FUNCTION__);

    _and_opd(sl, token);
    _xor_opdr(sl, token);
}

/*
 *
 */
static void _xor_opdr(struct symbols_t *sl, struct token_t *token)
{
    int64_t operand1, operand2;

    PRINTF("%s" NL, __FUNCTION__);
    if (!token_get(token, TOKEN_TYPE_AND, TOKEN_NEXT))
        return;

    PRINTF("&" NL);

    _and_opd(sl, token);

    operand2 = _exprstack_pop(sl);
    operand1 = _exprstack_pop(sl);

    _exprstack_push(operand1 & operand2);

    _xor_opdr(sl, token);
}

/*
 *
 */
static void _and_opd(struct symbols_t *sl, struct token_t *token)
{
    PRINTF("%s" NL, __FUNCTION__);

    _shift_opd(sl, token);
    _and_opdr(sl, token);
}

/*
 *
 */
static void _and_opdr(struct symbols_t *sl, struct token_t *token)
{
    int64_t operand1, operand2;
    enum {
        OPERATION_SHIFT_LEFT,
        OPERATION_SHIFT_RIGHT,
    } operation;
    
    PRINTF("%s" NL, __FUNCTION__);
    if (token_get(token, TOKEN_TYPE_SHIFT_LEFT, TOKEN_NEXT))
        operation = OPERATION_SHIFT_LEFT;
    else if (token_get(token, TOKEN_TYPE_SHIFT_RIGHT, TOKEN_NEXT))
        operation = OPERATION_SHIFT_RIGHT;
    else
        return;

    PRINTF("SHIFT %u" NL, operation);

    _shift_opd(sl, token);

    operand2 = _exprstack_pop(sl);
    operand1 = _exprstack_pop(sl);

    _exprstack_push(operation == OPERATION_SHIFT_LEFT ? operand1 << operand2 : operand1 >> operand2);

    _and_opdr(sl, token);
}

/*
 *
 */
static void _shift_opd(struct symbols_t *sl, struct token_t *token)
{
    PRINTF("%s" NL, __FUNCTION__);
    _add_opd(sl, token);
    _shift_opdr(sl, token);
}

/*
 *
 */
static void _shift_opdr(struct symbols_t *sl, struct token_t *token)
{
    int64_t operand1, operand2;
    enum {
        OPERATION_ADD,
        OPERATION_SUBSTRUCT,
    } operation;
    
    PRINTF("%s" NL, __FUNCTION__);
    if (token_get(token, TOKEN_TYPE_PLUS, TOKEN_NEXT))
        operation = OPERATION_ADD;
    else if (token_get(token, TOKEN_TYPE_MINUS, TOKEN_NEXT))
        operation = OPERATION_SUBSTRUCT;
    else
        return;

    PRINTF("ADD operation %u" NL, operation);

    _add_opd(sl, token);

    operand2 = _exprstack_pop(sl);
    operand1 = _exprstack_pop(sl);

    _exprstack_push(operation == OPERATION_ADD ? operand1 + operand2 : operand1 - operand2);

    _shift_opdr(sl, token);
}

/*
 *
 */
static void _add_opd(struct symbols_t *sl, struct token_t *token)
{
    PRINTF("%s" NL, __FUNCTION__);
    _mul_opd(sl, token);
    _add_opdr(sl, token);
}

/*
 *
 */
static void _add_opdr(struct symbols_t *sl, struct token_t *token)
{
    int64_t operand1, operand2;
    enum {
        OPERATION_MUL,
        OPERATION_DIV,
        OPERATION_MOD,
    } operation;
    
    PRINTF("%s" NL, __FUNCTION__);
    if (token_get(token, TOKEN_TYPE_MUL, TOKEN_NEXT))
        operation = OPERATION_MUL;
    else if (token_get(token, TOKEN_TYPE_DIV, TOKEN_NEXT))
        operation = OPERATION_DIV;
    else if (token_get(token, TOKEN_TYPE_MOD, TOKEN_NEXT))
        operation = OPERATION_MOD;
    else
        return;

    PRINTF("MUL operation %u" NL, operation);

    _mul_opd(sl, token);

    operand2 = _exprstack_pop(sl);
    operand1 = _exprstack_pop(sl);

    if (operation == OPERATION_MUL)
        _exprstack_push(operand1 * operand2);
    else if (operation == OPERATION_DIV)
        _exprstack_push(operand1 / operand2);
    else if (operation == OPERATION_MOD)
        _exprstack_push(operand1 % operand2);

    _add_opdr(sl, token);
}

/*
 *
 */
static void _mul_opd(struct symbols_t *sl, struct token_t *token)
{
    int64_t operand;
    
    PRINTF("%s" NL, __FUNCTION__);
    if (token_get(token, TOKEN_TYPE_NEGATE, TOKEN_NEXT))
    {
        PRINTF("NEG" NL);

        _not_opd(sl, token);
        operand = _exprstack_pop(sl);

        PRINTF("~ operand %lld" NL, operand);

        _exprstack_push(~operand);
    } else {
        _not_opd(sl, token);
    }
}

/*
 *
 */
static void _not_opd(struct symbols_t *sl, struct token_t *token)
{
    char *tname;
    
    PRINTF("%s" NL, __FUNCTION__);
    if ((tname = token_get(token, TOKEN_TYPE_NUMBER, TOKEN_NEXT)))
    {
        int64_t value;

        PRINTF("NUM %s" NL, tname);

        if (lang_util_str2num(tname, &value) < 0)
        {
            token_print_rollback(token);
            app_close(APP_EXITCODE_ERROR);
        }

        _exprstack_push(value);
    } else if ((tname = token_get(token, TOKEN_TYPE_SYMBOL, TOKEN_NEXT))) {
        char name[TOKEN_STRING_MAX];
        struct symbol_t *s;
        int64_t value;

        strcpy(name, tname);
        if (lang_util_question_expand(sl, name) < 0)
        {
            token_print_rollback(token);
            app_close(APP_EXITCODE_ERROR);
        }

        s = symbol_get_const(sl, name, &value);
        if (!s)
        {
            debug_emsgf("Symbol not found", "%s" NL, name);
            token_print_rollback(token);
            app_close(APP_EXITCODE_ERROR);
        }
        _exprstack_push(value);
    } else if (token_get(token, TOKEN_TYPE_ROUND_OPEN, TOKEN_NEXT)) {
        _expr(sl, token);
        if (!token_get(token, TOKEN_TYPE_ROUND_CLOSE, TOKEN_NEXT))
        {
            debug_emsg("Missing \")\" in expr");
            token_print_rollback(token);
            app_close(APP_EXITCODE_ERROR);
        }
    } else {
        debug_emsg("Empty expression");
        token_print_rollback(token);
        app_close(APP_EXITCODE_ERROR);
    }
}

