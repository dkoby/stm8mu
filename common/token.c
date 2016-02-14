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
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
/* */
#include <debug.h>
#include <llist.h>
#include <app_common.h>
#include "token.h"

/*
 *
 */
void token_prepare(struct token_t *token, char *path)
{
    token->file.fd = open(path, O_RDONLY);
    if (token->file.fd < 0)
    {
        debug_emsgf("Failed to open file", "%s, %s" NL, path, strerror(errno));
        app_close(APP_EXITCODE_ERROR);
    }

    token->file.cnt      = 0;
    token->file.line     = 1;
    strcpy(token->file.fname, path);

    token->trace.wp       = 0;
    token->trace.ncurrent = 0;
    token->trace.nnext    = 0;
}

/*
 * RETURN
 *     character readed, NULL on EOF
 */
static char *token_getchar(struct token_t *token)
{
    if (token->trace.rollback)
    {
        int wp, n;

        wp = token->trace.wp;
        n  = token->trace.rollback--;
        return &token->trace.buf[(TOKEN_TRACE_SIZE + wp - n) % TOKEN_TRACE_SIZE];
    } else {
        char *ch;

        if (token->file.cnt == 0)
        {
            int rd;

            rd = read(token->file.fd, token->file.buf, TOKEN_FILE_BUF_SIZE);
            if (rd < 0)
            {
                debug_emsg("File read error");
                app_close(APP_EXITCODE_ERROR);
            }
            if (rd == 0)
                return NULL;

            token->file.cnt  = rd;
            token->file.pbuf = token->file.buf;
        }

        token->file.cnt--;

        ch = token->file.pbuf++;

        if (*ch == '\n')
            token->file.line++;

        token->trace.buf[token->trace.wp] = *ch;
        token->trace.nnext++;

        token->trace.wp++;
        if (token->trace.wp >= TOKEN_TRACE_SIZE)
            token->trace.wp = 0;

        if (token->trace.nnext + token->trace.ncurrent > TOKEN_TRACE_SIZE)
        {
            /* NOTREACHED ? */
            debug_emsg("Rollback exceed");
            app_close(APP_EXITCODE_ERROR);
        }

        return ch;
    }
}

//#define TOKEN_GETCHAR(t) _token_getchar(t)

#ifdef TOKEN_GETCHAR
static char *_token_getchar(struct token_t *token)
{
    char *ch;

    ch = token_getchar(token);

    if (ch)
    {
        if (*ch == '\n')
            printf("ch CR" NL);
        else if (*ch == '\r')
            printf("ch LF" NL);
        else
            printf("ch '%c'" NL, *ch);
    } else {
        printf("ch EOF" NL);
    }

    return ch;
}
#else
    #define TOKEN_GETCHAR(t) token_getchar(t)
#endif


/*
 * RETURN
 *     pointer to token name, NULL if token does not appear in input stream
 */
char *token_get(struct token_t *token, enum token_type_t type, int whence)
{
    char *ch;
    int tpayload; /* token payload length */
    int tlength;  /* whole token length */
    int drop;     /* length of dropped prefix */
    struct {
        int escape;
        enum token_number_format_t number;
    } format;

    if (whence == TOKEN_CURRENT)
        token->trace.rollback = token->trace.ncurrent + token->trace.nnext;
    else
        token->trace.rollback = token->trace.nnext;

//    printf("> TOCKEN GET, %u, %u, NCURRENT %u NNEXT %u" NL, type, whence, token->trace.ncurrent, token->trace.nnext);

    tpayload        = 0;
    tlength         = 0;
    drop            = 0;
    format.escape   = 0;
    format.number   = TOKEN_NUMBER_FORMAT_DECIMAL;
    while (1)
    {
        ch = TOKEN_GETCHAR(token);
        if (!ch)
        {
            if (type == TOKEN_TYPE_EOF)
                break;
            return NULL;
        }
        if (tpayload >= TOKEN_MAX_NAME_SIZE)
        {
            /* NOTREACHED */
            debug_emsg("Token size exceed");
            app_close(APP_EXITCODE_ERROR);
        }

        if (tlength == 0)
        {
            if (*ch == ' ' || *ch == '\t' || *ch == '\r')
            {
                drop++;
                continue;
            }
        }

        if (type == TOKEN_TYPE_EOF)
        {
            return NULL;
        } else if (type == TOKEN_TYPE_SYMBOL) {
            if (tlength == 0)
            {
                if (!(
                        (*ch == '_') || (*ch >= 'a' && *ch <= 'z') || (*ch >= 'A' && *ch <= 'Z')))
                    return NULL;
            } else {
                if (!(
                        (*ch == '_') || (*ch >= 'a' && *ch <= 'z') || (*ch >= 'A' && *ch <= 'Z') || (*ch >= '0' && *ch <= '9')))
                {
                    break;
                }
            }

            token->name[tpayload++] = *ch;
            tlength++;
        } else if (type == TOKEN_TYPE_NUMBER) {
            int skip;

            skip = 0;
            if (tlength == 0)
            {
                if (*ch == '$')
                    format.number = TOKEN_NUMBER_FORMAT_HEX;
                else if (*ch == '%')
                    format.number = TOKEN_NUMBER_FORMAT_BINARY;
                else if (*ch == '@')
                    format.number = TOKEN_NUMBER_FORMAT_OCTAL;
                else if (*ch >= '0' && *ch <= '9')
                    format.number = TOKEN_NUMBER_FORMAT_DECIMAL;
                else
                    return NULL;
            } else {
                if (*ch == '_')
                {
                    skip = 1;
                } else {
                    /* TODO negative number */
                    if (format.number == TOKEN_NUMBER_FORMAT_HEX)
                    {
                        if (!( (*ch >= 'a' && *ch <= 'f') || (*ch >= 'A' && *ch <= 'F') || (*ch >= '0' && *ch <= '9') ))
                            break;
                    } else if (format.number == TOKEN_NUMBER_FORMAT_BINARY) {
                        if (!(*ch == '0' || *ch == '1'))
                            break;
                    } else if (format.number == TOKEN_NUMBER_FORMAT_OCTAL) {
                        if (!(*ch >= '0' && *ch <= '7'))
                            break;
                    } else if (format.number == TOKEN_NUMBER_FORMAT_DECIMAL) {
                        if (!(*ch >= '0' && *ch <= '9'))
                            break;
                    }
                }
            }
            if (!skip)
                token->name[tpayload++] = *ch;

            tlength++;
        } else if (type == TOKEN_TYPE_STRING) {
            if (tlength == 0)
            {
                if (*ch != '"')
                    return NULL;
            } else {
                if (format.escape)
                {
                    char c;

                    switch (*ch)
                    {
                        case 'n': c = '\n'; break;
                        case 'r': c = '\r'; break;
                        case '0': c = 0;    break;
                        default:
                            c = *ch;
                    }

                    format.escape = 0;
                    token->name[tpayload++] = c;
                } else {
                    if (*ch == '\\')
                    {
                        format.escape = 1;
                    } else {
                        if (*ch == '"')
                        {
                            tlength++;
                            break;
                        }
                        token->name[tpayload++] = *ch;
                    }
                }
            }

            tlength++;
        } else if (type == TOKEN_TYPE_CHAR) {
            if (tlength == 0)
            {
                if (*ch != '\'')
                    return NULL;
            } else {
                if (tpayload)
                {
                    if (*ch != '\'')
                        return NULL;
                    tlength++;
                    break;
                } else {
                    if (format.escape)
                    {
                        char c;

                        switch (*ch)
                        {
                            case 'n': c = '\n'; break;
                            case 'r': c = '\r'; break;
                            case '0': c = 0;    break;
                            default:
                                c = *ch;
                        }

                        format.escape = 0;
                        token->name[tpayload++] = c;
                    } else {
                        if (*ch == '\\')
                        {
                            format.escape = 1;
                        } else {
                            token->name[tpayload++] = *ch;
                        }
                    }
                }
            }

            tlength++;
        } else if (type == TOKEN_TYPE_COMMENT) {
            if (*ch == '\n')
            {
                token->name[tpayload++] = *ch;
                tlength++;
                break;
            }

            if (tlength == 0 && *ch != ';')
                return NULL;

            token->name[tpayload++] = *ch;
            tlength++;
        } else if (type == TOKEN_TYPE_LINE) {
            token->name[tpayload++] = *ch;
            tlength++;
            if (*ch == '\n')
                break;
        } else if (type == TOKEN_TYPE_SHIFT_LEFT) {
            if (*ch != '<')
                return NULL;

            token->name[tpayload++] = *ch;
            tlength++;

            if (tpayload == 2)
                break;
        } else if (type == TOKEN_TYPE_SHIFT_RIGHT) {
            if (*ch != '>')
                return NULL;

            token->name[tpayload++] = *ch;
            tlength++;

            if (tpayload == 2)
                break;
        } else {
            if (
                type == TOKEN_TYPE_NEGATE ||
                type == TOKEN_TYPE_MUL ||
                type == TOKEN_TYPE_DIV ||
                type == TOKEN_TYPE_MOD ||
                type == TOKEN_TYPE_AND ||
                type == TOKEN_TYPE_XOR ||
                type == TOKEN_TYPE_OR ||
                type == TOKEN_TYPE_MINUS ||
                type == TOKEN_TYPE_PLUS ||
                type == TOKEN_TYPE_DOT ||
                type == TOKEN_TYPE_EQUAL ||
                type == TOKEN_TYPE_COMMA ||
                type == TOKEN_TYPE_HASH ||
                type == TOKEN_TYPE_COLON ||
                type == TOKEN_TYPE_CURLY_OPEN ||
                type == TOKEN_TYPE_CURLY_CLOSE ||
                type == TOKEN_TYPE_BRACKET_OPEN ||
                type == TOKEN_TYPE_BRACKET_CLOSE ||
                type == TOKEN_TYPE_ROUND_OPEN ||
                type == TOKEN_TYPE_ROUND_CLOSE
                )
            {
                char cmp;

                switch (type)
                {
                    case TOKEN_TYPE_MINUS:         cmp = '-'; break;
                    case TOKEN_TYPE_PLUS:          cmp = '+'; break;
                    case TOKEN_TYPE_DOT:           cmp = '.'; break;
                    case TOKEN_TYPE_EQUAL:         cmp = '='; break;
                    case TOKEN_TYPE_COMMA:         cmp = ','; break;
                    case TOKEN_TYPE_HASH:          cmp = '#'; break;
                    case TOKEN_TYPE_COLON:         cmp = ':'; break;
                    case TOKEN_TYPE_CURLY_OPEN:    cmp = '{'; break;
                    case TOKEN_TYPE_CURLY_CLOSE:   cmp = '}'; break;
                    case TOKEN_TYPE_BRACKET_OPEN:  cmp = '['; break;
                    case TOKEN_TYPE_BRACKET_CLOSE: cmp = ']'; break;
                    case TOKEN_TYPE_ROUND_OPEN:    cmp = '('; break;
                    case TOKEN_TYPE_ROUND_CLOSE:   cmp = ')'; break;
                    case TOKEN_TYPE_OR:            cmp = '|'; break;
                    case TOKEN_TYPE_XOR:           cmp = '^'; break;
                    case TOKEN_TYPE_AND:           cmp = '&'; break;
                    case TOKEN_TYPE_MUL:           cmp = '*'; break;
                    case TOKEN_TYPE_DIV:           cmp = '/'; break;
                    case TOKEN_TYPE_MOD:           cmp = '%'; break;
                    case TOKEN_TYPE_NEGATE:        cmp = '~'; break;
                    default:
                        /* NOTREACHED */
                        cmp = 0;
                        debug_emsgf("Unspecified character token", "%d" NL, type);
                        app_close(APP_EXITCODE_ERROR);
                        return NULL;
                }

                if (*ch != cmp)
                    return NULL;
                token->name[tpayload++] = *ch;
                tlength++;
                break;
            } else {
                /* NOTREACHED */
                debug_emsgf("Unspecified token type", "%d" NL, type);
                app_close(APP_EXITCODE_ERROR);
            }
        }
    }

    /* reallign current/next tokens */
    {
        int n;

        if (whence == TOKEN_CURRENT)
            n = token->trace.ncurrent + token->trace.nnext;
        else
            n = token->trace.nnext;

        if (drop + tlength > n)
        {
            /* NOTREACHED */
            debug_emsg("Token tlength overflow");
            app_close(APP_EXITCODE_ERROR);
        }

        token->trace.ncurrent = tlength;
        token->trace.nnext    = n - (token->trace.ncurrent + drop);
    }

    token->name[tpayload] = 0;

#if 0
    printf("< TOCKEN GET, %u, %u, NCURRENT %u NNEXT %u" NL, type, whence, token->trace.ncurrent, token->trace.nnext);
    if (type != TOKEN_TYPE_EOF)
        printf("TOCKEN: \"%s\"" NL, token->name);
#endif

    return token->name;
}

/*
 *
 */
void token_drop(struct token_t *token)
{
    token->trace.ncurrent = 0;
}

/*
 *
 */
void token_print_rollback(struct token_t *token)
{
    int n, wp;
    char *rp;
    char ch;

    wp   = token->trace.wp;
    n  = token->trace.ncurrent + token->trace.nnext;
    rp = &token->trace.buf[(TOKEN_TRACE_SIZE + wp - n) % TOKEN_TRACE_SIZE];

    printf("%s, line %u:" NL, token->file.fname, token->file.line);
    while (n--)
    {
        ch = *rp++;
        if (rp >= &token->trace.buf[TOKEN_TRACE_SIZE])
            rp = token->trace.buf;
        printf("%c", ch);
    }
    printf(NL);
}

/*
 *
 */
static void _token_close(struct token_t *token)
{
    if (token->file.fd >= 0)
        close(token->file.fd);
    token->file.fd = -1;
}

/*******************************************
 * For easy wipeout collate tokens in list.
 *******************************************/

/*
 *
 */
void tokens_init(struct tokens_t *tl)
{
    tl->first = NULL;
}

/*
 *
 */
static void _token_destroy(void *p)
{
    struct token_t *token;
    if (!p)
        return;

    token = p;

    _token_close(token);
    free(token);
}

/*
 *
 */
struct token_t *token_new(struct tokens_t *tl)
{
    struct llist_t *head;
    struct token_t *t;

    t = malloc(sizeof(struct token_t));
    if (!t)
        goto error;

    memset(t, 0, sizeof(struct token_t));
    t->file.fd = -1;

    head = llist_add(tl->first, t, _token_destroy, t);
    if (!head)
        goto error;
    tl->first = head;

    return t;
error:
    debug_emsg("Can not create token");
    if (t)
        _token_destroy(t);
    app_close(APP_EXITCODE_ERROR);
    return NULL;
}

/*
 *
 */
void token_remove(struct tokens_t *tl, struct token_t *t)
{
    tl->first = llist_remove(tl->first, llist_find(tl->first, t));
}

/*
 *
 */
void tokens_destroy(struct tokens_t *tl)
{
    if (!tl)
        return;
    llist_destroy(tl->first);
}

