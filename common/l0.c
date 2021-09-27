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
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
/* */
#include <debug.h>
#include <types.h>
#include <llist.h>
#include <btorder.h>
//#include <token.h>
#include "l0.h"
#include "bmem.h"

#if 1
    #define PRINTF(...) printf(__VA_ARGS__)
#else
    #define PRINTF(...)
#endif


#pragma pack(push, 1)

struct l0_file_head_t {
#define L0_HEAD_MAGIC    0x00306C2E
    uint32_t magic;
    uint16_t version;
    uint8_t reserved[26];
};

struct l0_block_info_t {
#define L0_SYMBOL_MAGIC      0xAC10
#define L0_RELOCATION_MAGIC  0xAC11
#define L0_SECTION_MAGIC     0xAC12
    uint16_t magic;
    uint32_t length;
    uint16_t cs;
    uint8_t reserved[24];
};

struct l0_symbol_block_t {
    union {
        struct {
            uint8_t  exp      : 1;  /* export */
            uint8_t  ext      : 1;  /* extern */
            uint16_t reserved : 14;
        };
    } flag;
    uint8_t width;
    int64_t value;
    /* name */
    /* section */
};

struct l0_relocation_block_t {
    uint8_t type;
    uint32_t offset; /* offset of fixup from start of section */
    uint32_t length; /* length of fixup */
    int32_t  adj;    /* adjust offset of relative fixup */
    /* symbol */
    /* section */
};

struct l0_section_block_t {
    union {
        struct {
            uint8_t  noload   : 1;
            uint16_t reserved : 15;
        };
    } flag;
    uint32_t length;
    /* name */
    /* data */
};

#pragma pack(pop)

static uint16_t _block_cs(struct l0_block_info_t *block);

#define CURRENT_VERSION 0x0001



/*
 *
 */
int l0_save(char *fpath,
        struct symbols_t *symbols,
        struct relocations_t *relocations,
        struct sections_t *sections)
{
    int fd;
    int wr;
    int wlen;
    mode_t mode;
    struct l0_file_head_t head;
    struct llist_t *ll;
    struct bmem_t bmem;
    struct l0_block_info_t *iblock;
    char *pbuf;

    bmem_init(&bmem);

    mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    umask(~mode);
    fd = open(fpath, O_WRONLY | O_CREAT | O_TRUNC, mode);
    if (fd < 0)
    {
        debug_emsgf("Failed to open file", "\"%s\": %s" NL, fpath, strerror(errno));
        return -1;
    }

    /* write head */
    {
        wlen = sizeof(struct l0_file_head_t);
        memset(&head, 0, wlen);
        head.magic   = host_tole32(L0_HEAD_MAGIC);
        head.version = host_tole32(CURRENT_VERSION);

        wr = write(fd, &head, wlen);
        if (wr != wlen)
            goto error;
    }

    /* write symbols */
    for (ll = symbols->first; ll; ll = ll->next)
    {
        struct symbol_t *s;
        uint32_t length;
        struct l0_symbol_block_t *sblock;
        char *section;
        char nosection[1];

        nosection[0] = 0;

        s = ll->p;

        if (s->type == SYMBOL_TYPE_NONE)
            continue;

        if (s->type != SYMBOL_TYPE_LABEL && s->type != SYMBOL_TYPE_EXTERN)
            continue;

        section = NULL;
        if (s->section)
            section = s->section;
        else if (s->type != SYMBOL_TYPE_LABEL)
            section = nosection;

        if (!section)
        {
            debug_emsgf("Symbol has not section attribute", "\"%s\"" NL, s->name);
            goto error;
        }

        length  = sizeof(struct l0_block_info_t);
        length += sizeof(struct l0_symbol_block_t);
        length += strlen(s->name) + 1;
        length += strlen(section) + 1;

        if (bmem_alloc(&bmem, length) < 0)
            goto error;

        pbuf = bmem.buf;

        /* make iblock */
        {
            iblock = (struct l0_block_info_t*)pbuf;

            memset(iblock, 0, sizeof(struct l0_block_info_t));
            iblock->magic  = host_tole16(L0_SYMBOL_MAGIC);
            iblock->length = host_tole32(length);

            pbuf += sizeof(struct l0_block_info_t);
        }

        /* write symbol block head */
        {
            sblock = (struct l0_symbol_block_t*)pbuf;

            memset(sblock, 0, sizeof(struct l0_symbol_block_t));
            sblock->flag.exp = s->exp;
            sblock->flag.ext = s->type == SYMBOL_TYPE_EXTERN ? 1 : 0;
            sblock->width    = s->width;
            sblock->value    = s->val64;

            pbuf += sizeof(struct l0_symbol_block_t);
        }

        /* write symbol name */
        {
            strcpy(pbuf, s->name);
            pbuf += strlen(s->name) + 1;
        }

        /* write symbol's section name */
        {
            strcpy(pbuf, section);
            pbuf += strlen(section) + 1;
        }

        /* make cs for iblock */
        iblock->cs = host_tole16(_block_cs(iblock));

        /* write to file */
        wr = write(fd, bmem.buf, length);
        if (wr != length)
            goto error;
    }

    /* write relocations */
    for (ll = relocations->first; ll; ll = ll->next)
    {
        struct relocation_t *r;
        uint32_t length;
        struct l0_relocation_block_t *rblock;

        r = ll->p;

        length  = sizeof(struct l0_block_info_t);
        length += sizeof(struct l0_relocation_block_t);
        length += strlen(r->symbol)  + 1;
        length += strlen(r->section) + 1;

        if (bmem_alloc(&bmem, length) < 0)
            goto error;

        pbuf = bmem.buf;

        /* make iblock */
        {
            iblock = (struct l0_block_info_t*)pbuf;

            memset(iblock, 0, sizeof(struct l0_block_info_t));
            iblock->magic  = host_tole16(L0_RELOCATION_MAGIC);
            iblock->length = host_tole32(length);

            pbuf += sizeof(struct l0_block_info_t);
        }

        /* write relocation block head */
        {
            rblock = (struct l0_relocation_block_t *)pbuf;

            memset(rblock, 0, sizeof(struct l0_relocation_block_t));
            rblock->type   = r->type;
            rblock->offset = host_tole32(r->offset);
            rblock->length = host_tole32(r->length);
            rblock->adj    = host_tole32(r->adjust);

            pbuf += sizeof(struct l0_relocation_block_t);
        }

        /* write symbol name */
        {
            strcpy(pbuf, r->symbol);
            pbuf += strlen(r->symbol) + 1;
        }

        /* write symbol's section name */
        {
            strcpy(pbuf, r->section);
            pbuf += strlen(r->section) + 1;
        }

        /* make cs for iblock */
        iblock->cs = host_tole16(_block_cs(iblock));

        /* write to file */
        wr = write(fd, bmem.buf, length);
        if (wr != length)
            goto error;
    }

    /* write sections */
    for (ll = sections->first; ll; ll = ll->next)
    {
        struct section_t *s;
        uint32_t length;
        struct l0_section_block_t *sblock;

        s = ll->p;

        /* drop empty section */
        if (s->length == 0)
            continue;

        length  = sizeof(struct l0_block_info_t);
        length += sizeof(struct l0_section_block_t);
        length += strlen(s->name) + 1;
        if (!s->noload)
            length += s->length;

        if (bmem_alloc(&bmem, length) < 0)
            goto error;

        pbuf = bmem.buf;

        /* make iblock */
        {
            iblock = (struct l0_block_info_t*)pbuf;

            memset(iblock, 0, sizeof(struct l0_block_info_t));
            iblock->magic  = host_tole16(L0_SECTION_MAGIC);
            iblock->length = host_tole32(length);

            pbuf += sizeof(struct l0_block_info_t);
        }

        /* write section block head */
        {
            sblock = (struct l0_section_block_t *)pbuf;

            memset(sblock, 0, sizeof(struct l0_section_block_t));
            sblock->flag.noload = s->noload;
            sblock->length      = host_tole32(s->length);

            pbuf += sizeof(struct l0_section_block_t);
        }

        /* write section name */
        {
            strcpy(pbuf, s->name);
            pbuf += strlen(s->name) + 1;
        }

        /* write section's data */
        if (!s->noload)
            memcpy(pbuf, s->data, s->length);

        /* make cs for iblock */
        iblock->cs = host_tole16(_block_cs(iblock));

        /* write to file */
        wr = write(fd, bmem.buf, length);
        if (wr != length)
            goto error;
    }

    close(fd);
    bmem_destroy(&bmem);
    return 0;
error:
    debug_emsgf("Failed to write file", "\"%s\": %s" NL, fpath, strerror(errno));
    bmem_destroy(&bmem);
    return -1;
}

/*
 *
 */
int l0_load(char *fpath,
        struct symbols_t *symbols,
        struct sections_t *sections,
        struct relocations_t *relocations)
{
    int fd;
    int rd;
    int rlen;
    struct l0_file_head_t *head;
    struct l0_block_info_t *iblock;
    struct bmem_t bmem;

    fd = open(fpath, O_RDONLY);
    if (fd < 0)
    {
        debug_emsgf("Failed to open file", "\"%s\": %s" NL, fpath, strerror(errno));
        return -1;
    }

    bmem_init(&bmem);

    /* read file head */
    {
        rlen = sizeof(struct l0_file_head_t);
        if (bmem_alloc(&bmem, rlen) < 0)
            goto error;

        rd = read(fd, bmem.buf, rlen);
        if (rd != rlen)
            goto error;
        head = bmem.buf;

        if (head->magic != le32to_host(L0_HEAD_MAGIC))
        {
            debug_emsg("File format error");
            goto error;
        }
        if (head->version != le32to_host(CURRENT_VERSION))
        {
            debug_emsg("File format version mismatch");
            goto error;
        }
    }

    while (1)
    {
        off_t bpos;
        uint32_t length;
        char *pbuf;

        bpos = lseek(fd, 0, SEEK_CUR);
        if (bpos == (off_t)-1)
            goto error;

        rlen = sizeof(struct l0_block_info_t);
        if (bmem_alloc(&bmem, rlen) < 0)
            goto error;

        /* read iblock */
        {
            rd = read(fd, bmem.buf, rlen);
            if (rd != rlen)
            {
                if (rd == 0)
                    goto done;
                goto error;
            }
            iblock = bmem.buf;
        }

        length = le32to_host(iblock->length);

        if (bmem_alloc(&bmem, length) < 0)
            goto error;

        if (lseek(fd, bpos, SEEK_SET) == (off_t)-1)
            goto error;

        /* read whole block */
        rlen = length;
        rd = read(fd, bmem.buf, rlen);
        if (rd != rlen)
            goto error;

        iblock = bmem.buf;

        if (le16to_host(iblock->cs) != _block_cs(iblock))
        {
            debug_emsg("Block checksum mismatch");
            goto error;
        }

        pbuf = bmem.buf;
        switch (le16to_host(iblock->magic))
        {
            case L0_SYMBOL_MAGIC:
                {
                    struct symbol_t *s;
                    struct l0_symbol_block_t *block;
                    char *name;
                    char *section;

                    pbuf += sizeof(struct l0_block_info_t);
                    block = (struct l0_symbol_block_t*)pbuf;
                    pbuf += sizeof(struct l0_symbol_block_t);

                    name    = pbuf;
                    section = name + strlen(name) + 1;

                    s = symbols_add(symbols, name);
                    s->exp   = block->flag.exp;
                    s->width = block->width;
                    s->val64 = block->value;
                    if (block->flag.ext)
                        s->type = SYMBOL_TYPE_EXTERN;
                    else
                        s->type = SYMBOL_TYPE_LABEL;

                    if (strlen(section))
                        symbol_set_section(s, section);
                }
                break;
            case L0_RELOCATION_MAGIC:
                {
                    struct l0_relocation_block_t *block;
                    char *symbol;
                    char *section;

                    pbuf += sizeof(struct l0_block_info_t);
                    block = (struct l0_relocation_block_t*)pbuf;
                    pbuf += sizeof(struct l0_relocation_block_t);

                    symbol  = pbuf;
                    section = symbol + strlen(symbol) + 1;

                    relocations_add(relocations, section, symbol,
                            le32to_host(block->offset),
                            le32to_host(block->length),
                            le32to_host(block->adj),
                            block->type);
                }
                break;
            case L0_SECTION_MAGIC:
                {
                    struct section_t *s;
                    struct l0_section_block_t *block;
                    char *name;
                    void *data;

                    pbuf += sizeof(struct l0_block_info_t);
                    block = (struct l0_section_block_t *)pbuf;
                    pbuf += sizeof(struct l0_section_block_t);

                    name = pbuf;
                    data = name + strlen(name) + 1;

                    s = section_select(sections, name);
                    s->noload = block->flag.noload;

                    if (!s->noload)
                        section_pushdata(s, data, le32to_host(block->length));
                    else
                        s->length = le32to_host(block->length);
                }
                break;
        }
    }

done:
    close(fd);
    bmem_destroy(&bmem);
    return 0;
error:
    debug_emsgf("Failed to read file", "\"%s\"" NL, fpath);
    if (fd >= 0)
        close(fd);
    bmem_destroy(&bmem);
    return -1;
}

/*
 *
 */
static uint16_t _block_cs(struct l0_block_info_t *block)
{
    uint16_t cs, bcs;
    uint8_t *p;
    uint32_t length;

    bcs = block->cs;
    block->cs = 0;

    cs = 0;
    p = (uint8_t*)block;
    length = le32to_host(block->length);

    while (length--)
        cs += *p++;

    block->cs = bcs;

    return cs;
}


