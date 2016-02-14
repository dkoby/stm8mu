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
#include <token.h>
#include <app_common.h>

/*
 *
 */
int lang_util_str2num(char *sdata, int64_t *value)
{
    uint64_t result;
    uint64_t weight;
    enum token_number_format_t format;
    int len;
    char *p;

    result = 0;

    if (*sdata == 0)
    {
        debug_emsg("Zero length string with number");
        goto error;
    } else if (*sdata == '$') {
        format = TOKEN_NUMBER_FORMAT_HEX;
    } else if (*sdata == '%') {
        format = TOKEN_NUMBER_FORMAT_BINARY;
    } else if (*sdata == '@') {
        format = TOKEN_NUMBER_FORMAT_OCTAL;
    } else if (*sdata >= '0' || *sdata <= '9') {
        format = TOKEN_NUMBER_FORMAT_DECIMAL;
    } else {
        debug_emsg("Invalid number format");
        goto error;
    }

    if (format == TOKEN_NUMBER_FORMAT_DECIMAL)
    {
        len = strlen(sdata);
        if (len < 1)
        {
            debug_emsg("Too short string for number");
            goto error;
        }

        result = 0;
        weight = 1;
        p      = sdata + len - 1;
        while (len--)
        {
            if (*p >= '0' && *p <= '9')
            {
                result += (*p - '0') * weight;
            } else {
                /* NOTREACHED */
                debug_emsg("Invalid decimal number");
                goto error;
            }

            p--;
            weight *= 10;
        }
    } else if (format == TOKEN_NUMBER_FORMAT_HEX) {
        len = strlen(sdata) - 1;
        if (len < 1)
        {
            debug_emsg("Too short string for number");
            goto error;
        }

        result = 0;
        weight = 1;
        p      = sdata + 1 + len - 1;
        while (len--)
        {
            if (*p >= '0' && *p <= '9')
                result += (*p - '0') * weight;
            else if (*p >= 'a' && *p <= 'f')
                result += (*p - 'a' + 10) * weight;
            else if (*p >= 'A' && *p <= 'F')
                result += (*p - 'A' + 10) * weight;
            else {
                /* NOTREACHED */
                debug_emsgf("Invalid hex number", "%s %c" NL, sdata, *p);
                goto error;
            }

            p--;
            weight <<= 4;
        }
    } else if (format == TOKEN_NUMBER_FORMAT_BINARY) {
        len = strlen(sdata) - 1;
        if (len < 1)
        {
            debug_emsg("Too short string for number");
            goto error;
        }

        result = 0;
        weight = 1;
        p      = sdata + 1 + len - 1;
        while (len--)
        {
            if (*p >= '0' && *p <= '1')
            {
                result += (*p - '0') * weight;
            } else {
                /* NOTREACHED */
                debug_emsg("Invalid binary number");
                goto error;
            }

            p--;
            weight <<= 1;
        }
    } else if (format == TOKEN_NUMBER_FORMAT_OCTAL) {
        len = strlen(sdata) - 1;
        if (len < 1)
        {
            debug_emsg("Too short string for number");
            goto error;
        }

        result = 0;
        weight = 1;
        p      = sdata + 1 + len - 1;
        while (len--)
        {
            if (*p >= '0' && *p <= '7')
            {
                result += (*p - '0') * weight;
            } else {
                /* NOTREACHED */
                debug_emsg("Invalid octal number");
                goto error;
            }

            p--;
            weight <<= 3;
        }
    }

    *value = result;
    return 0;
error:
    debug_emsg("Failed to convert to number");
    return -1;
}

/*
 * XXX
 *     No buffer length check.
 *     Only octal and binary numbers support.
 */
void lang_util_num2str(int64_t num, enum token_number_format_t format, char *st)
{
    uint64_t weight;

    *st = 0;
    if (format == TOKEN_NUMBER_FORMAT_BINARY) {
        int print;

        *st++ = '%';
        weight = 1llu << 63;

        print = 0;
        while (weight)
        {
            if (num & weight)
            {
                print = 1;
                *st++ = '1';
            } else {
                if (weight == 1 || print)
                    *st++ = '0';
            }

            weight >>= 1;
        }
    } else if (format == TOKEN_NUMBER_FORMAT_OCTAL) {
        uint64_t div;
        int print;

        *st++  = '@';
        weight = 1152921504606846976ull; /* XXX */

        print = 0;
        while (weight > 0)
        {
            div = num / weight;

            if (div)
                print = 1;

            if (weight < 8 || print)
                *st++ = div + '0';

            if (weight == 0)
                break;

            if (num > weight)
                num -= weight;
            weight >>= 3;
        }
    }

    *st = 0;
}

