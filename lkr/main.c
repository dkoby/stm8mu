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
#include <unistd.h>
#include <signal.h>
/* */
#include <debug.h>
#include <lang_util.h>
#include <version.h>
#include "app.h"
#include "linker.h"

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

    app.innum        = 0;
    app.printmap     = 0;
    app.printmapdata = 0;
    *app.lscript     = 0;
    *app.outputfile  = 0;
    *app.s19head     = 0;

    linker_init();

    _get_options(argc, argv);
}

/*
 *
 */
static void app_run()
{
    linker_run();
}

/*
 *
 */
void app_close(int code)
{
    linker_destroy();;
    exit(code);
}

/*
 *
 */
static void _print_head()
{
    printf(NL "Linker for STM8. Written and copyrights by Dmitry Kobylin." NL);
    printf("Version %u.%u.%u ("__DATE__")" NL, MAJOR, MINOR, BUILD);
    printf("THIS SOFTWARE COMES WITH ABSOLUTELY NO WARRANTY! USE AT YOUR OWN RISK!" NL NL);
}

/*
 *
 */
static void _print_help(int argc, char **argv)
{
    _print_head();

    printf("Usage: %s <OPTIONS> <INPUT_FILES>"NL, argv[0]);
    printf(NL);
    printf("OPTIONS:"NL);
    printf("    -h, --help         print this help" NL);
    printf("    -p, --noprint      suppress \".print\" directive" NL);
    printf("    -M                 output map" NL);
    printf("    -MD                output data in map" NL);
    printf("    -D<symbol>=<value> define symbol passed to linker script" NL);
    printf("    --script=<path>    linker script" NL);
    printf("    --output=<path>    output file (S19 format)" NL);
    printf("    --s19head=<value>  value for S0 record of S19" NL);

    printf(NL);
}

/*
 *
 */
static void _get_options(int argc, char** argv)
{
    int i;
    char symbol[TOKEN_STRING_MAX * 2 + 1];

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
        } else if (sscanf(argv[i], "--script=%s", app.lscript)) {

        } else if (sscanf(argv[i], "--output=%s", app.outputfile)) {

        } else if (sscanf(argv[i], "--s19head=%s", app.s19head)) {

        } else if (strcmp(argv[i], "-M") == 0) {
            app.printmap = 1;
        } else if (strcmp(argv[i], "-MD") == 0) {
            app.printmapdata = 1;
        } else if (sscanf(argv[i], "-D%s", symbol)) {
            char *ch;
            int64_t value;

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

            if (!linker_add_symbol(&lcontext, symbol, value))
            {
                debug_emsgf("Failed to add symbol", "\"%s\"" NL, symbol);
                app_close(APP_EXITCODE_ERROR);
            }
        } else if (strcmp("-p", argv[i]) == 0 || strcmp("--noprint", argv[i]) == 0) {
            app.noprint = 1;
        } else {
            app.infiles = &argv[i];
            app.innum   = argc - i;
            break;
        }
    }

    if (app.innum == 0)
    {
        debug_emsg("No input files was specified" NL);
        app_close(APP_EXITCODE_ERROR);
    }
    if (!*app.lscript)
    {
        debug_emsg("No linker script was specified" NL);
        app_close(APP_EXITCODE_ERROR);
    }
}

