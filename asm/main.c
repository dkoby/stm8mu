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
/* */
#include <debug.h>
#include <l0.h>
#include <version.h>
#include "app.h"
#include "lang_util.h"
#include "assembler.h"


struct app_context_t app;

static void app_init(int argc, char** argv);
static void app_run();
static void _get_options(int argc, char** argv);
static void _print_head();
static void _print_help(int argc, char **argv);

/*
 *
 */
int main(int argc, char** argv)
{
    app_init(argc, argv);

    app_run();
    app_close(APP_EXITCODE_OK);
    return 0;
}

/*
 * 
 */
static void _sigact(int sig, siginfo_t *sinf, void *context)
{
    switch (sig)
    {
	case SIGINT:
	case SIGTERM:
	    debug_imsg("SIGTERM or SIGINT received, terminating");
            app_close(APP_EXITCODE_SIGTERM);
	    break;
	default:
	    debug_emsgf("Unknown signal received", "%u" NL, sig);
	    break;
    }
}

/*
 *
 */
static void app_init(int argc, char** argv)
{
    _print_head();

    /* hook signal handler */
    {
        struct sigaction act;

        memset(&act, 0, sizeof(act));
        act.sa_sigaction = _sigact;
        act.sa_flags = SA_SIGINFO | SA_RESETHAND;
        if (sigaction(SIGTERM, &act, NULL) == -1)
        {
            debug_emsg("Failed to install signal handler");
            app_close(APP_EXITCODE_ERROR);
        }
        if (sigaction(SIGINT, &act, NULL) == -1)
        {
            debug_emsg("Failed to install signal handler");
            app_close(APP_EXITCODE_ERROR);
        }
    }

    *app.inputfile  = 0;
    *app.outputfile = 0;
    app.printresult = 0;
    app.noprint     = 0;

    assembler_init();

    _get_options(argc, argv);
}

/*
 *
 */
static void app_run()
{
    if (assembler(app.asmcontext, app.inputfile) < 0)
        app_close(APP_EXITCODE_ERROR);
    app.asmcontext->pass++;
    if (assembler(app.asmcontext, app.inputfile) < 0)
        app_close(APP_EXITCODE_ERROR);

    if (app.printresult)
        assembler_print_result(app.asmcontext);

    {
        struct llist_t *ll;

        for (ll = app.asmcontext->sections.first; ll; ll = ll->next)
        {
            if (((struct section_t*)ll->p)->length)
                break;
        }
        if (!ll)
        {
            debug_wmsg("No output data");
            app_close(APP_EXITCODE_ERROR);
        }
    }

    if (l0_save(app.outputfile,
            &app.asmcontext->symbols,
            &app.asmcontext->relocations,
            &app.asmcontext->sections) < 0)
    {
        app_close(APP_EXITCODE_ERROR);
    }
}

/*
 *
 */
void app_close(int code)
{
    assembler_destroy();
    exit(code);
}

/*
 *
 */
static void _print_head()
{
    printf(NL "Assembler for STM8. Written and copyrights by Dmitry Kobylin." NL);
    printf("Version %u.%u.%u ("__DATE__")" NL, MAJOR, MINOR, BUILD);
    printf("THIS SOFTWARE COMES WITH ABSOLUTELY NO WARRANTY! USE AT YOUR OWN RISK!" NL NL);
}

/*
 *
 */
static void _print_help(int argc, char **argv)
{
    printf("Usage: %s <OPTIONS> <INPUT_FILE>"NL, argv[0]);
    printf(NL);
    printf("OPTIONS:"NL);
    printf("    -h, --help         print this help" NL);
    printf("    -I, --info         print result information of assembling" NL);
    printf("    -p, --noprint      suppress \".print\" directive" NL);
    printf("    -D<symbol>=<value> define constant symbol" NL);
    printf("    --output=<path>    output file" NL);

    printf(NL);
}

/*
 *
 */
static void _get_options(int argc, char** argv)
{
    int i;
    char symbol[TOKEN_STRING_MAX * 2 + 1];

    *symbol = 0;
    if (argc <= 1)
    {
        _print_help(argc, argv);
        app_close(APP_EXITCODE_ERROR);
    }

    for (i = 1; i < argc; i++)
    {
        if (strcmp("-h", argv[i]) == 0 || strcmp("--help", argv[i]) == 0)
        {
            _print_help(argc, argv);
            app_close(APP_EXITCODE_ERROR);
        } else if (strcmp("-I", argv[i]) == 0 || strcmp("--info", argv[i]) == 0) {
            app.printresult = 1;
        } else if (sscanf(argv[i], "-D%s", symbol)) {
            char *ch;
            int64_t value;
            struct symbol_t *s;

            ch = symbol;
            while (*ch)
            {
                if (*ch == '=')
                {
                    *ch = 0;
                    ch++;
                    break;
                }
                ch++;
            }
            if (!*ch)
            {
                debug_emsg("No value followed \"-D\"" NL);
                app_close(APP_EXITCODE_ERROR);
            }
            if (lang_util_str2num(ch, &value) < 0)
                app_close(APP_EXITCODE_ERROR);

            s = symbols_add(&app.asmcontext->symbols, symbol);
            symbol_set_const(s, value);
        } else if (strcmp("-p", argv[i]) == 0 || strcmp("--noprint", argv[i]) == 0) {
            app.noprint = 1;
        } else if (sscanf(argv[i], "--output=%s", app.outputfile)) {

        } else {
            if (i == argc - 1)
            {
                strcpy(app.inputfile, argv[i]);
            } else {
                printf(ERR_PREFIX "Unknown option \"%s\"" NL, argv[i]);
                app_close(APP_EXITCODE_ERROR);
            }
        }
    }

    if (!*app.inputfile)
    {
        debug_emsg("Input file not specified" NL);
        app_close(APP_EXITCODE_ERROR);
    }
    if (!*app.outputfile)
    {
        char *in, *out;
        char *dot;

        in = app.inputfile;
        out = app.outputfile;

        dot = NULL;
        while (*in)
        {
            if (*in == '.')
                dot = out;
            if (*in == '/')
                dot = NULL;

            *out++ = *in++;
        }

        if (dot)
            out = dot;

        *out++ = '.';
        *out++ = 'l';
        *out++ = '0';
        *out   = 0;
    }
}

