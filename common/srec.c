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
#include <stdio.h>
#include <errno.h>
#include <string.h>
/* */
#include <btorder.h>
#include "debug.h"
#include "memdata.h"
#include "types.h"

#define MAX_TOKEN_SIZE 1024
struct srec_parser_t {
    enum {
        STATE_NONE,
        STATE_S,
        STATE_BYTE_COUNT,
        STATE_ADDRESS,
        STATE_DATA,
        STATE_CS,
    } state;

    struct {
        enum srec_record_type_t {
            RTYPE_S0 = 0,
            RTYPE_S1 = 1,
            RTYPE_S2 = 2,
            RTYPE_S3 = 3,
            RTYPE_S5 = 5,
            RTYPE_S6 = 6,
            RTYPE_S7 = 7,
            RTYPE_S8 = 8,
            RTYPE_S9 = 9,
        } type;

        uint32_t length;  /* record length */
        uint32_t address;
        uint8_t  cs0;
        uint8_t  cs1;
        uint8_t  data[MAX_TOKEN_SIZE];
        uint8_t  *pdata;
        uint32_t dlen;
    } record;

    char token[MAX_TOKEN_SIZE + 1];
    char trace[MAX_TOKEN_SIZE + 1];

    int line;
    int lpos;
};

static struct srec_parser_t parser;

static void _print_trace(char *trace);
static uint8_t _ch2num(char ch);

/*
 * Read S-record file.
 *
 * RETURN
 *     Pointer to memdata_t structure. NULL on error.
 *     Structure should be destroyed in future by memdata_destroy().
 */
struct memdata_t *srec_read(const char *path)
{
    struct memdata_t *md;
    FILE *f;
    char ch;
    char *token;
    char *trace;
    uint32_t tlen;

    f  = NULL;
    md = memdata_create();
    if (!md)
    {
        debug_emsg("Failed to create memory data");
        goto error;
    }

    printf("Read file \"%s\"" NL, path);

    f = fopen(path, "r");
    if (!f)
    {
        debug_emsgf("Failed to open file", "\"%s\", %s"NL, path, strerror(errno));
        goto error;
    }

    parser.state = STATE_NONE;
#define FIRST_LINE   1
#define FIRST_POS    0
    parser.line  = FIRST_LINE;
    parser.lpos  = FIRST_POS;

    trace = parser.trace;
    token = parser.token;
    tlen  = 0;

    while (1)
    {
        if (tlen >= MAX_TOKEN_SIZE)
        {
            debug_emsg("MAX_TOKEN_SIZE exceed");
            goto error;
        }

        if (fread(&ch, 1, 1, f) == 0)
        {
            if (feof(f))
            {
                if (parser.state != STATE_NONE)
                {
                    debug_emsg("Unexpected end of file reached");
                    goto error;
                }
                break;
            }
            if (ferror(f))
            {
                debug_emsg("File read error");
                goto error;
            }
        }
        parser.lpos++;
        *trace++ = ch;

        /* skip CR character */
        if (ch == '\r')
            continue;

        *token++ = ch;
        tlen++;

        if (parser.state == STATE_NONE)
        {
            if (ch == '\n')
            {
                parser.line++;
                parser.lpos = FIRST_POS;
                trace = parser.trace;
            } else if (ch == 'S') {
                parser.state = STATE_S;
            } else {
                _print_trace(trace);
                goto error;
            }
        } else if (parser.state == STATE_S) {
            if (ch >= '0' && ch <= '9' && ch != '4')
            {
                parser.state = STATE_BYTE_COUNT;
                parser.record.type    = RTYPE_S0 + ch - '0';
                parser.record.length  = 0;
                parser.record.address = 0;
                parser.record.cs0     = 0;
                parser.record.cs1     = 0;
                parser.record.pdata   = parser.record.data;
                *parser.record.pdata  = 0;
                goto next_token;
            } else {
                _print_trace(trace);
                goto error;
            }
        } else if (parser.state == STATE_BYTE_COUNT) {
            int num;

            if ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F'))
            {
                num = tlen % 2 ? _ch2num(ch) << 4 : _ch2num(ch);

                parser.record.cs0    += num;
                parser.record.length |= num;
            } else {
                _print_trace(trace);
                goto error;
            }
            if (tlen >= 2)
            {
                if (parser.record.length < 3)
                {
                    _print_trace(trace);
                    debug_emsg("Invalid byte cound");
                    goto error;
                }
                parser.state = STATE_ADDRESS;
                goto next_token;
            }
        } else if (parser.state == STATE_ADDRESS) {
            int addrlen;
            int num;

            if (!((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F')))
            {
                _print_trace(trace);
                goto error;
            }

            switch (parser.record.type)
            {
                case RTYPE_S0: addrlen = 2; break;
                case RTYPE_S1: addrlen = 2; break;
                case RTYPE_S2: addrlen = 3; break;
                case RTYPE_S3: addrlen = 4; break;
                case RTYPE_S5: addrlen = 2; break;
                case RTYPE_S6: addrlen = 3; break;
                case RTYPE_S7: addrlen = 4; break;
                case RTYPE_S8: addrlen = 3; break;
                case RTYPE_S9: addrlen = 2; break;
                default:
                    debug_emsgf("Unknown record type", "at line %u" NL, parser.line);
                    goto error;
            }

            num = tlen % 2 ? _ch2num(ch) << 4 : _ch2num(ch);
            parser.record.cs0     += num;
            parser.record.address |= num << (8 * (addrlen - (tlen - 1) / 2 - 1));

            if (tlen >= addrlen * 2)
            {
                parser.record.dlen  = parser.record.length - addrlen - 1;

                if (parser.record.dlen == 0)
                    parser.state = STATE_CS;
                else
                    parser.state = STATE_DATA;
                goto next_token;
            }
        } else if (parser.state == STATE_DATA) {
            int num;

            if (!((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F')))
            {
                _print_trace(trace);
                goto error;
            }

            num = tlen % 2 ? _ch2num(ch) << 4 : _ch2num(ch);
            parser.record.cs0 += num;

            *parser.record.pdata |= num;

            if ((tlen % 2) == 0)
                *++parser.record.pdata = 0;

            if (tlen >= parser.record.dlen * 2)
            {
                parser.state = STATE_CS;
                goto next_token;
            }

        } else if (parser.state == STATE_CS) {
            int num;

            if (!((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F')))
            {
                _print_trace(trace);
                goto error;
            }

            num = tlen % 2 ? _ch2num(ch) << 4 : _ch2num(ch);
            parser.record.cs1 |= num;

            if (tlen >= 2)
            {
                if (parser.record.cs0 ^ parser.record.cs1 ^ 0xFF)
                {
                    debug_emsgf("Checksum mismatch", "at line %u" NL, parser.line);
                    goto error;
                }

                if (parser.record.type == RTYPE_S0)
                {
                    printf("S19 Comment: %s" NL, parser.record.data);
                } else {
                    switch (parser.record.type)
                    {
                        case RTYPE_S1:
                        case RTYPE_S2:
                        case RTYPE_S3:
                            if (memdata_add(md, parser.record.address, parser.record.data, parser.record.dlen))
                            {
                                debug_emsg("Failed to append memory data");
                                goto error;
                            }
                            break;
                        default:
                            ;
                    }
                }

                parser.state = STATE_NONE;
                goto next_token;
            }
        }

        continue;
next_token:
        tlen = 0;
        token = parser.token;
    }

    fclose(f);
    return md;
error:
    if (f)
        fclose(f);
    if (md)
        memdata_destroy(md);
    return NULL;
}

/*
 *
 */
static char nsbuf[9];


static char *_num2str(uint32_t num, int width)
{
    char *p;

    p = nsbuf;
    *p = 0;
    if (width > 4 || width <= 0)
    {
        /* NOTREACHED */
        debug_emsg("Invalid width");
        return nsbuf;
    }

    switch (width)
    {
        case 1: num = host_tole32(num); break;
        case 2: num = host_tobe16(num); break;
        case 3: num = host_tobe24(num); break;
        case 4: num = host_tobe32(num); break;
    }

    while (width--)
    {
        uint8_t digit;

        digit = (num >> 4) & 0x0f;
        *p = digit;
        if (digit < 10)
            *p += '0';
        else
            *p += 'A' - 10;
        p++;

        digit = num & 0x0f;
        *p = digit;
        if (digit < 10)
            *p += '0';
        else
            *p += 'A' - 10;
        p++;

        num >>= 8;
    }

    return nsbuf;
}

/*
 *
 */
static uint8_t _mkcs(void *data, int len)
{
    uint8_t cs;
    uint8_t *p;

    cs = 0;
    p  = data;
    while (len--)
        cs += *p++;

    return cs;
}

/*
 *
 */
static int _write_data(FILE *f, enum srec_record_type_t rtype, uint32_t address, void *data, uint32_t length)
{
    uint8_t addrwidth;
    uint8_t cs;
    uint8_t rchar;
    uint8_t *pdata;
    uint32_t bytecount;

    pdata = data;
    if (rtype == RTYPE_S0)
    {
        address   = 0;
        addrwidth = 2;
    } else {
        if (length < 0x010000)
        {
            rtype     = RTYPE_S1;
            addrwidth = 2;
        } else if (length < 0x01000000) {
            rtype     = RTYPE_S2;
            addrwidth = 3;
        } else {
            rtype     = RTYPE_S3;
            addrwidth = 4;
        }
    }

    rchar = rtype + '0';

#define DATASPLIT        16
#define CS_WIDTH         1
#define BYTECOUNT_WIDTH  1

    while (length)
    {
        /* write Sx */
        if (!fwrite("S", 1, 1, f))
           return -1; 
        if (!fwrite(&rchar, 1, 1, f))
           return -1; 

        bytecount = length;
        if (bytecount > DATASPLIT)
            bytecount = DATASPLIT;

        /* write bytecount */
        cs = bytecount + addrwidth + CS_WIDTH;
        if (!fwrite(_num2str(bytecount + addrwidth + CS_WIDTH, BYTECOUNT_WIDTH), BYTECOUNT_WIDTH * 2, 1, f))
            return -1;

        cs += _mkcs(&address, addrwidth);

        /* write address */
        if (!fwrite(_num2str(address, addrwidth), addrwidth * 2, 1, f))
            return -1;

        /* write data */
        {
            uint8_t n;

            n = bytecount;
            while (n--)
            {
                uint8_t num;

                num = *pdata;
                if (!fwrite(_num2str(num, 1), 2, 1, f))
                    return -1;

                cs += *pdata++;
            }
        }

        cs ^= 0xff;
        /* write cs */
        if (!fwrite(_num2str(cs, 1), 2, 1, f))
            return -1;

        /* write CR LF */
        if (!fwrite("\r\n", 2, 1, f))
            return -1;

//        printf("%u %u %u" NL, bytecount, length, address);
        length  -= bytecount;
        address += bytecount;
    }

    return 0;
}




/*
 *
 */
int srec_write(const char *path, struct memdata_t *memdata, char *comment)
{
    FILE *f;

    f = fopen(path, "w");
    if (!f)
    {
        debug_emsgf("Faile to open file for writing", SQ ", %s" NL, path, strerror(errno));
        goto error;
    }

    if (comment)
        if (_write_data(f, RTYPE_S0, 0, comment, strlen(comment)) < 0)
            goto error;

    {
        struct llist_t *loop;
        struct memdata_row_t *row;

        memdata_mkloop(memdata, &loop);
        while ((row = memdata_next(&loop)))
        {
            if (_write_data(f, RTYPE_S1, row->offset, row->data, row->length) < 0)
                goto error;
        }
    }

    fclose(f);
    return 0;
error:
    return -1;
}

/*
 *
 */
static void _print_trace(char *trace)
{
    char *ptrace;

    debug_emsgf("Unexpected character", "at line %u, position %u"NL, parser.line, parser.lpos);

    *trace = 0;
    printf("%s" NL, trace);

    ptrace = parser.trace;
    while (ptrace < trace)
        printf("%c", *ptrace++);
    printf(NL);
    ptrace = parser.trace;
    while (++ptrace < trace)
        printf(" ");
    printf("^" NL);

}

/*
 *
 */
static uint8_t _ch2num(char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    else
        return ch - 'A' + 10;
}

