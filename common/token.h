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
#ifndef _TOKEN_H
#define _TOKEN_H

#include <limits.h>
/* */

#define TOKEN_MAX_NAME_SIZE   1024
#define TOKEN_STRING_MAX      (TOKEN_MAX_NAME_SIZE + 1)
#define TOKEN_TRACE_SIZE      (TOKEN_MAX_NAME_SIZE * 2)
#define TOKEN_FILE_BUF_SIZE   512

/*
 * : colon
 * ; semicolon
 * ' apostrophe
 * " quote mark
 * . dot
 * . comma
 * * asterisk
 * ! exclamation
 * _ underscore
 * { curly bracket
 * [ bracket
 * ( round bracket
 * # hash
 */

enum token_type_t {
    TOKEN_TYPE_SYMBOL,
    TOKEN_TYPE_LINE,
    TOKEN_TYPE_EOF,
    TOKEN_TYPE_DOT,
    TOKEN_TYPE_EQUAL,
    TOKEN_TYPE_COMMA,
    TOKEN_TYPE_HASH,
    TOKEN_TYPE_COLON,
    TOKEN_TYPE_CURLY_OPEN,
    TOKEN_TYPE_CURLY_CLOSE,
    TOKEN_TYPE_BRACKET_OPEN,
    TOKEN_TYPE_BRACKET_CLOSE,
    TOKEN_TYPE_ROUND_OPEN,
    TOKEN_TYPE_ROUND_CLOSE,
    TOKEN_TYPE_NUMBER,
    TOKEN_TYPE_STRING,
    TOKEN_TYPE_CHAR,
    TOKEN_TYPE_COMMENT,
    TOKEN_TYPE_PLUS,
    TOKEN_TYPE_MINUS,
    TOKEN_TYPE_OR,
    TOKEN_TYPE_XOR,
    TOKEN_TYPE_AND,
    TOKEN_TYPE_SHIFT_LEFT,
    TOKEN_TYPE_SHIFT_RIGHT,
    TOKEN_TYPE_MUL,
    TOKEN_TYPE_DIV,
    TOKEN_TYPE_MOD,
    TOKEN_TYPE_NEGATE,
};

enum {
    TOKEN_CURRENT,
    TOKEN_NEXT,
};

struct token_t {
    struct {
        int fd;
        char fname[PATH_MAX];
        char buf[TOKEN_FILE_BUF_SIZE];
        char *pbuf;
        int cnt;

        int line;
    } file;

    struct {
        char buf[TOKEN_TRACE_SIZE];
        int wp;

        int ncurrent;
        int nnext;
        int rollback;
    } trace;

    char name[TOKEN_STRING_MAX];
};

enum token_number_format_t {
    TOKEN_NUMBER_FORMAT_DECIMAL,
    TOKEN_NUMBER_FORMAT_HEX,
    TOKEN_NUMBER_FORMAT_BINARY,
    TOKEN_NUMBER_FORMAT_OCTAL,
};

void token_prepare(struct token_t *token, char *path);
char *token_get(struct token_t *token, enum token_type_t type, int whence);
void token_drop(struct token_t *token);
void token_print_rollback(struct token_t *token);

/*******************************************
 * For easy wipeout collate tokens in list.
 *******************************************/
struct tokens_t {
    struct llist_t *first;
};

void tokens_init(struct tokens_t *tl);
struct token_t *token_new(struct tokens_t *tl);
void token_remove(struct tokens_t *tl, struct token_t *t);
void tokens_destroy(struct tokens_t *tl);

#endif

