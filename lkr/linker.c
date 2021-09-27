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
#include <debug.h>
#include <btorder.h>
#include <l0.h>
#include <token.h>
#include "linker.h"
#include "lang.h"
#include "memdata.h"
#include "srec.h"

struct linker_context_t lcontext;

static void _load_file(struct linker_context_t *ctx, char *path);
static void _print_map(struct linker_context_t *ctx);
static void _glue_sections(struct linker_context_t *ctx);
static void _patch_sections(struct linker_context_t *ctx);
static void _lscript(struct linker_context_t *ctx);
static void _write_srec(struct linker_context_t *ctx, char *path);

/*
 *
 */
void linker_init()
{
    struct linker_context_t *ctx = &lcontext;

    lcontext.flist = NULL;

    symbols_init(&ctx->symbols);
    symbols_init(&ctx->result.symbols);
    sections_init(&ctx->result.sections);
    relocations_init(&ctx->result.relocations);
    tokens_init(&ctx->tokens);
}

/*
 *
 */
void linker_run()
{
    struct linker_context_t *ctx = &lcontext;

    while (app.innum--)
        _load_file(ctx, *app.infiles++);

#if 0
    printf("Link" NL);
#endif
    _glue_sections(ctx);
    _lscript(ctx);
    _patch_sections(ctx);

    if (*app.outputfile)
    {
#if 0
        printf("Write %s" NL, app.outputfile);
#endif
        _write_srec(ctx, app.outputfile);
    }

    if (app.printmap)
        _print_map(ctx);
}

/*
 *
 */
void linker_destroy()
{
    struct linker_context_t *ctx = &lcontext;

    llist_destroy(ctx->flist);
    symbols_destroy(&ctx->symbols);
    symbols_destroy(&ctx->result.symbols);
    sections_destroy(&ctx->result.sections);
    relocations_destroy(&ctx->result.relocations);
    tokens_destroy(&ctx->tokens);
}

/*
 *
 */
struct symbol_t * linker_add_symbol(struct linker_context_t *ctx, char *name, int64_t value)
{
    struct symbol_t *s;

    if (symbol_find(&ctx->symbols, name))
    {
        debug_emsgf("Symbol redefined", "\"%s\"" NL, name);
        return NULL;
    }

    s = symbols_add(&ctx->symbols, name);
    symbol_set_const(s, value);

    return s;
}

/*
 *
 */
static void _destroy_file_data(void *p)
{
    struct linker_file_data_t *fd;

    if (!p)
        return;

    fd = p;

    if (fd->fname)
        free(fd->fname);
    symbols_destroy(&fd->symbols);
    sections_destroy(&fd->sections);
    relocations_destroy(&fd->relocations);
}

/*
 *
 */
static void _load_file(struct linker_context_t *ctx, char *path)
{
    char *fname, *pfname;
    struct llist_t *head;
    struct linker_file_data_t *fd, *lfd;

    fname = NULL;
    fd    = NULL;

#if 0
    printf("Load file \"%s\"" NL, path);
#endif

    /* remove path from file name */
    {
        int slen;

        slen = strlen(path);

        pfname = path + strlen(path);
        while (slen--)
        {
            if (*pfname == '/' || *pfname == '\\')
            {
                pfname++;
                break;
            }
            pfname--;
        }
    }

    fname = malloc(strlen(pfname) + 1);
    if (!fname)
        goto error;
    strcpy(fname, pfname);

    fd = malloc(sizeof(struct linker_file_data_t));
    if (!fd)
        goto error;

    fd->fname = fname;
    fname = NULL;
    symbols_init(&fd->symbols);
    sections_init(&fd->sections);
    relocations_init(&fd->relocations);

    head = llist_add(ctx->flist, fd, _destroy_file_data, fd);
    if (!head)
        goto error;
    ctx->flist = head;
    lfd = fd;
    fd = NULL;

    if (l0_load(path, &lfd->symbols, &lfd->sections, &lfd->relocations) < 0)
        goto error;

    return;
error:
    if (fname)
        free(fname);
    if (fd)
        _destroy_file_data(fd);
    debug_emsgf("Failed to load file", "\"%s\"" NL, path);
    app_close(APP_EXITCODE_ERROR);
    return;
}

/*
 *
 */
static void _print_symbols(struct symbols_t *symbols)
{
    struct llist_t *loop;
    struct symbol_t *s;

    printf(NL);
    printf("-------------" NL);
    printf("-- Symbols --" NL);
    printf("-------------" NL);

    symbols_mkloop(symbols, &loop);
    while ((s = symbols_next(&loop)))
    {
        if (s->type == SYMBOL_TYPE_NONE)
            continue;
        switch (s->type)
        {
            case SYMBOL_TYPE_CONST:  printf("CONST"); break;
            case SYMBOL_TYPE_EXTERN: printf("EXTERN"); break;
            case SYMBOL_TYPE_LABEL:  printf("LABEL"); break;
            default:                 printf("-----");
        }
        printf(", width %u", s->width);
        printf(", export %u", s->exp);
        printf(", value 0x%06llX (%lld)", (long long int)s->val64, (long long int)s->val64);
        printf(" \"%s\"", s->name);
        if (s->section)
            printf(", section \"%s\"", s->section);

        printf(NL);

        /* print attributes */
        {
            struct symbol_attr_t *a;
            struct llist_t *loop;

            symbol_attr_mkloop(s, &loop);
            while ((a = symbol_attr_next(&loop)))
                printf("\tattr \"%s\" = \"%s\"" NL, a->name, a->value ? a->value : "NULL");
        }
    }
}

/*
 *
 */
static void _print_relocations(struct relocations_t *relocations)
{
    struct relocation_t *r;
    struct llist_t *loop;

    printf(NL);
    printf("-----------------" NL);
    printf("-- Relocations --" NL);
    printf("-----------------" NL);

    relocations_mkloop(relocations, &loop);
    while ((r = relocations_next(&loop)))
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

/*
 *
 */
static void _print_sections(struct sections_t *sections)
{
    struct llist_t *loop;
    struct section_t *s;

    printf(NL);
    printf("--------------" NL);
    printf("-- Sections --" NL);
    printf("--------------" NL);

    sections_mkloop(sections, &loop);
    while ((s = sections_next(&loop)))
    {
        printf(NL);
        printf("Section \"%s\" %s" NL, s->name, s->noload ? "NOLOAD" : "");
        if (!s->noload)
            printf("    LMA    0x%06X" NL, s->lma);
        printf("    VMA    0x%06X" NL, s->vma);
        printf("    size   0x%06X" NL, s->length);
        if (!s->noload && app.printmapdata)
            debug_buf((uint8_t*)s->data, s->length);
    }
}

/*
 *
 */
static void _print_map(struct linker_context_t *ctx)
{
    struct llist_t *floop;

    printf(NL);
    printf("############" NL);
    printf("## Input  ##" NL);
    printf("############" NL);

    for (floop = ctx->flist; floop; floop = floop->next)
    {
        struct linker_file_data_t *fd = floop->p;

        printf(NL);
        printf("*****************************" NL);
        printf("** %s " NL, fd->fname);
        printf("*****************************" NL);

        _print_symbols(&fd->symbols);
        _print_relocations(&fd->relocations);
        _print_sections(&fd->sections);

        printf(NL);
    }

    printf(NL);
    printf("############" NL);
    printf("## Output ##" NL);
    printf("############" NL);

    _print_symbols(&ctx->result.symbols);
    _print_relocations(&ctx->result.relocations);
    _print_sections(&ctx->result.sections);
}

struct _symbol_find_info_t {
    char *sname;             /* symbol to find */
    char *fexclude;          /* exclude file from search */

    char *ffound;            /* name of file in which symbol was found, NULL if symbol was found on linker context */
    struct symbol_t *symbol; /* pointer to found symbol */
};

/*
 *
 */
void _symbol_find_extern(struct linker_context_t *ctx, struct _symbol_find_info_t *find)
{
    struct llist_t *floop;
    struct symbol_t *sext;

    /* find symbol in files */
    sext = NULL;
    for (floop = ctx->flist; floop; floop = floop->next)
    {
        struct linker_file_data_t *fd = floop->p;
        struct symbol_t *s;
        struct llist_t *loop;

        if (strcmp(fd->fname, find->fexclude) == 0)
            continue;

        symbols_mkloop(&fd->symbols, &loop);
        while ((s = symbols_next(&loop)))
        {
            if (strcmp(find->sname, s->name) == 0 && s->exp)
            {
                if (sext)
                {
                    debug_emsgf("Symbol redefined", "\"%s\"" NL, find->sname);
                    app_close(APP_EXITCODE_ERROR);
                }
                sext = s;
                find->ffound = fd->fname;
            }
        }
    }

    /* find symbol in linker context */
    {
        struct symbol_t *s;
        struct llist_t *loop;

        symbols_mkloop(&ctx->symbols, &loop);
        while ((s = symbols_next(&loop)))
        {
            if (strcmp(find->sname, s->name) == 0)
            {
                if (sext)
                {
                    debug_emsgf("Symbol redefined", "\"%s\"" NL, find->sname);
                    app_close(APP_EXITCODE_ERROR);
                }
                sext = s;
            }
        }
    }

    find->symbol = sext;
}


/*
 *
 */
static char *_mkname(char *f, char *s)
{
    static char namebuf[TOKEN_STRING_MAX * 2];

    *namebuf = 0;

    strcat(namebuf, f);
    strcat(namebuf, ":");
    strcat(namebuf, s);

    return namebuf;
}

/*
 *
 */
static void _add_relocation(struct linker_context_t *ctx, struct linker_file_data_t *fd, struct symbol_t *s, char *rsname)
{
    struct llist_t *loop;
    struct relocation_t *r;

    relocations_mkloop(&fd->relocations, &loop);
    while ((r = relocations_next(&loop)))
    {
        struct section_t *section;

        if (strcmp(r->symbol, s->name) != 0)
            continue;

        section = section_find(&ctx->result.sections, r->section);
        if (!section)
        {
            /* NOTREACHED */
            debug_emsg("Section not found for relocation");
            app_close(APP_EXITCODE_ERROR);
        }

        if (r->length != s->width)
        {
            /* NOTREACHED */
            debug_emsgf("Relocation mismatch symbol width", "\"%s\""NEW_LINE, s->name);
            app_close(APP_EXITCODE_ERROR);
        }

        relocations_add(&ctx->result.relocations,
                section->name,                  /* section name to patch */
                rsname,  /* symbol from witch value should retereived */
                r->offset + section->offset,    /* offset of relocation */
                r->length,                      /* */
                r->adjust,
                r->type
        );
    }

}

/*
 *
 */
static void _add_symbols(struct linker_context_t *ctx, struct linker_file_data_t *fd)
{
    struct symbol_t *s;
    struct symbol_t *sext;
    struct llist_t *loop;

    symbols_mkloop(&fd->symbols, &loop);
    while ((s = symbols_next(&loop)))
    {
        if (s->type == SYMBOL_TYPE_EXTERN)
        {
            struct _symbol_find_info_t find;

            find.sname    = s->name;
            find.fexclude = fd->fname;
            find.symbol   = NULL;
            find.ffound   = NULL;

            _symbol_find_extern(ctx, &find);
            sext = find.symbol;

            if (sext && find.ffound)
            {
                /*
                 * Extern symbol was found in file.
                 */
                _add_relocation(ctx, fd, s, _mkname(find.ffound, s->name));
            } else {
                if (sext)
                {
                    /*
                     * Extern symbol was found in linker context.
                     */
                    struct symbol_t *ns;

                    if (!symbol_find(&ctx->result.symbols, s->name))
                    {
                        ns = symbols_add(&ctx->result.symbols, s->name);
                        symbol_set_const(ns, find.symbol->val64);
                        ns->width = s->width;
                    }
                } else {
                    /*
                     * Not found, add as extern symbol. Check in future in linker context (after linker script pass).
                     */
                    struct symbol_t *ns;

                    if (!symbol_find(&ctx->result.symbols, s->name))
                    {
                        ns = symbols_add(&ctx->result.symbols, s->name);
                        ns->type  = SYMBOL_TYPE_EXTERN;
                        ns->width = s->width;
                    }
                }
                _add_relocation(ctx, fd, s, s->name);
            }
        } else {
            struct symbol_t *ns;
            struct section_t *rs;

            if (!s->section)
            {
                /* NOTREACHED */
                debug_emsgf("Symbol has not section", "\"%s\"" NL, s->name);
                app_close(APP_EXITCODE_ERROR);
            }

            rs = section_find(&ctx->result.sections, s->section);
            if (!rs)
            {
                /* NOTREACHED */
                debug_emsgf("Section not found", "\"%s\"" NL, s->section);
                app_close(APP_EXITCODE_ERROR);
            }

            ns = symbols_add(&ctx->result.symbols, _mkname(fd->fname, s->name));
            ns->type   = SYMBOL_TYPE_LABEL;
            ns->width  = s->width;
            ns->offset = s->offset + rs->offset;
            ns->exp    = s->exp; /* not used, just for debug */
            symbol_set_section(ns, s->section);

            _add_relocation(ctx, fd, s, _mkname(fd->fname, s->name));
        }
    }
}

/*
 *
 */
static void _glue_sections(struct linker_context_t *ctx)
{
    struct llist_t *floop;

    for (floop = ctx->flist; floop; floop = floop->next)
    {
        struct linker_file_data_t *fd = floop->p;

        /* loop thru sections of file */
        {
            struct llist_t *loop;
            struct section_t *rsection;
            struct section_t *section;

            sections_mkloop(&fd->sections, &loop);
            while ((section = sections_next(&loop)))
            {
                rsection = section_select(&ctx->result.sections, section->name);

                if (!rsection->noload)
                    rsection->noload = section->noload;
                if (rsection->noload != section->noload)
                {
                    debug_emsgf("NOLOAD attribute of section mismatch", "\"%s\"" NL, section->name);
                    app_close(APP_EXITCODE_ERROR);
                }

                section_pushdata(rsection, section->data, section->length);
            }

            /* fix, rename symbols */
            _add_symbols(ctx, fd);

            /* add section offset */
            sections_mkloop(&ctx->result.sections, &loop);
            while ((section = sections_next(&loop)))
                section->offset += section->length;
        }
    }
}

/*
 *
 */
static uint64_t _mkpatch(uint64_t value, int width)
{
    switch (width)
    {
        case 1: return host_tole32(value);
        case 2: return host_tobe16(value);
        case 3: return host_tobe24(value);
        default:
            /* NOTREACHED */
            debug_emsg("Invalid width");
            app_close(APP_EXITCODE_ERROR);
            return 0;
    }
}

#if 0
/*
 *
 */
static int64_t _mkdif(int64_t val1, int64_t val2)
{
    if (val2 > val1)
        return val2 - val1;
    else
        return val1 - val2;
}

/*
 *
 */
static int64_t _maxnum(int width)
{
    if (width == 1)
        return 0xff;
    if (width == 2)
        return 0xffff;
    if (width == 3)
        return 0xffffff;

    /* NOTREACHED */
    debug_emsg("Invalid width");
    app_close(APP_EXITCODE_ERROR);
    return 0;
}
#endif

/*
 *
 */
static int64_t _smaxnum(int width)
{
    if (width == 1)
        return 0x7F;

    /* NOTREACHED */
    debug_emsg("Invalid width");
    app_close(APP_EXITCODE_ERROR);
    return 0;
}

/*
 *
 */
static int64_t _sminnum(int width)
{
    if (width == 1)
        return -128;

    /* NOTREACHED */
    debug_emsg("Invalid width");
    app_close(APP_EXITCODE_ERROR);
    return 0;
}




/*
 *
 */
static void _patch_sections(struct linker_context_t *ctx)
{
    /* check sections overlap */
    {
        struct llist_t *loop;
        struct section_t *s0;

        sections_mkloop(&ctx->result.sections, &loop);
        while ((s0 = sections_next(&loop)))
        {
            struct llist_t *loop;
            struct section_t *s1;

            if ((s0->vma + s0->length) > 0x010000)
            {
                debug_wmsgf("Section cross 64kb - be care", "\"%s\"" NL, s0->name);
            }

            sections_mkloop(&ctx->result.sections, &loop);
            while ((s1 = sections_next(&loop)))
            {
                if (strcmp(s0->name, s1->name) == 0)
                    continue;

                /* LMA */
                if (!s0->noload && !s1->noload)
                {
                    if (
                        (s0->lma  < s1->lma && (s0->lma + s0->length) > s1->lma) ||
                        (s0->lma >= s1->lma && (s1->lma + s1->length) > s0->lma))
                    {
                        debug_emsgf("LMA of sections overlaps", "\"%s\" \"%s\"" NL, s0->name, s1->name);
                        app_close(APP_EXITCODE_ERROR);
                        return;
                    }
                }

                if (
                    (s0->vma  < s1->vma && (s0->vma + s0->length) > s1->vma) ||
                    (s0->vma >= s1->vma && (s1->vma + s1->length) > s0->vma))
                {
                    debug_emsgf("VMA of sections overlaps", "\"%s\" \"%s\"" NL, s0->name, s1->name);
                    app_close(APP_EXITCODE_ERROR);
                    return;
                }
            }
        }
    }

    /* fix symbols */
    {
        struct llist_t *loop;
        struct symbol_t *s;

        symbols_mkloop(&ctx->result.symbols, &loop);
        while ((s = symbols_next(&loop)))
        {
            struct section_t *section;

            if (s->type != SYMBOL_TYPE_LABEL)
                continue;

            section = section_find(&ctx->result.sections, s->section);
            if (!section)
            {
                /* NOTREACHED */
                debug_emsgf("Section not found", "\"%s\"" NL, s->section);
                app_close(APP_EXITCODE_ERROR);
            }

            s->offset += section->vma;
        }
    }

    /* apply relocations */
    {
        struct llist_t *loop;
        struct relocation_t *relocation;

        relocations_mkloop(&ctx->result.relocations, &loop);
        while ((relocation = relocations_next(&loop)))
        {
            struct symbol_t *symbol;
            struct section_t *rsection, *ssection;
            int64_t patch;

            symbol = symbol_find(&ctx->result.symbols, relocation->symbol);
            if (!symbol)
            {
                /* NOTREACHED */
                debug_emsg("NULL");
                app_close(APP_EXITCODE_ERROR);
            }

            if (symbol->type == SYMBOL_TYPE_EXTERN)
            {
                struct symbol_t *ns;

                ns = symbol_find(&ctx->symbols, relocation->symbol);
                if (!ns)
                {
                    debug_emsgf("Undefined reference to symbol", "\"%s\"" NL, relocation->symbol);
                    app_close(APP_EXITCODE_ERROR);
                }

                /* Change symbol in result symbols */
                symbol->val64 = ns->val64;
                symbol->type  = SYMBOL_TYPE_CONST;
            }

            rsection = section_find(&ctx->result.sections, relocation->section);
            if (!rsection)
            {
                /* NOTREACHED */
                debug_emsg("NULL");
                app_close(APP_EXITCODE_ERROR);
            }

            if (symbol->type == SYMBOL_TYPE_CONST)
            {
                patch = _mkpatch(symbol->val64, symbol->width);
                section_patch(rsection, relocation->offset, &patch, relocation->length);
                continue;
            }

            ssection = section_find(&ctx->result.sections, symbol->section);
            if (!ssection)
            {
                /* NOTREACHED */
                debug_emsg("NULL");
                app_close(APP_EXITCODE_ERROR);
            }

            if (relocation->type == RELOCATION_TYPE_ABOSULTE)
            {
#if 0
                if (_mkdif(relocation->offset + rsection->vma, symbol->offset) > _maxnum(relocation->length))
                {
                    debug_emsgf("Symbol jump too long",
                            "\"%s\", symbol VMA 0x%06llX, relocation vma 0x%06X" NL,
                            symbol->name, symbol->offset, relocation->offset + rsection->vma);
                    app_close(APP_EXITCODE_ERROR);
                }
#endif

                patch = symbol->offset;
                patch = _mkpatch(patch, symbol->width);
            } else {
                int64_t jump;

                jump = symbol->offset - (rsection->vma + relocation->offset + relocation->adjust);

                if ((jump <  0 && jump < _sminnum(relocation->length)) ||
                    (jump >= 0 && jump > _smaxnum(relocation->length)))
                {
                    debug_emsgf("Symbol jump too long",
                            "\"%s\", symbol VMA 0x%06llX, relocation vma 0x%06X, jump %lld" NL,
                            symbol->name, (long long int)symbol->offset, (rsection->vma + relocation->offset + relocation->adjust), (long long int)jump);
                    app_close(APP_EXITCODE_ERROR);
                }

                patch = jump;
                patch = _mkpatch(patch, symbol->width);
            }
            section_patch(rsection, relocation->offset, &patch, relocation->length);
        }
    }
}

/*
 *
 */
static void _lscript(struct linker_context_t *ctx)
{
    struct token_t *token;

    token = token_new(&ctx->tokens);
    token_prepare(token, app.lscript);

    while (1)
    {
        token_drop(token);
        if (lang_eof(token) == 0)
            break;
        if (lang_comment(token) == 0)
            continue;
        if (lang_const_symbol(ctx, token) == 0)
            continue;
        if (lang_directive(ctx, token) == 0)
            continue;

        debug_emsg("Unknown construction in script");
        goto error;
    }

    goto noerror;
error:
    token_print_rollback(token);
    debug_emsgf("Error in file", "%s" NL, app.lscript);
noerror:
    token_remove(&ctx->tokens, token);
}


/*
 *
 */
static void _write_srec(struct linker_context_t *ctx, char *path)
{
    struct llist_t *loop;
    struct section_t *section;
    struct memdata_t *md;
    int havedata;

    md = memdata_create();
    if (!md)
    {
        debug_emsg("Failed to allocate memdata");
        goto error;
    }

    havedata = 0;

    /* make memdata from sections */
    sections_mkloop(&ctx->result.sections, &loop);
    while ((section = sections_next(&loop)))
    {
        if (section->noload || !section->length)
            continue;

        havedata = 1;

        if (memdata_add(md, section->lma, (uint8_t *)section->data, section->length) < 0)
        {
            debug_emsg("Faile to add memory data");
            goto error;
        }
    }

    if (!havedata)
    {
        debug_emsg("No output data");
        goto error;
    }

    if (memdata_pack(&md) < 0)
    {
        debug_emsg("Faile to pack memory data");
        goto error;
    }

#if 0
    memdata_print(md);
#endif

    if (srec_write(path, md, *app.s19head ? app.s19head : NULL) < 0)
        goto error;

    memdata_destroy(md);
    return;
error:
    debug_emsgf("Failed to write file", SQ NL, path);
    if (md)
        memdata_destroy(md);
    app_close(APP_EXITCODE_ERROR);
}

