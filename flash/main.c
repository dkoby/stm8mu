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
#include <limits.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
/* */
#include <debug.h>
#include <memdata.h>
#include <stm8chip.h>
#include <version.h>
#include "cport.h"
#include "app.h"
#include "program.h"

#define ENV_CPORT_TTY     "CPORT_TTY"

static void app_init(int argc, char** argv);
static void app_run();
static void _get_options(int argc, char** argv);
static void _print_head();
static void _print_help(int argc, char **argv);

struct app_context_t app;

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

    cport_init();

    app.baud         = B115200;
    app.action       = ACTION_UNKNOWN;
    *app.cportpath   = 0;
    *app.inputfile   = 0;
    app.chip         = NULL;

    _get_options(argc, argv);
}

/*
 *
 */
static void app_run()
{
    if (!app.cportpath[0])
    {
        printf(ERR_PREFIX "Device not specified" NL);
        app_close(APP_EXITCODE_ERROR);
    }
    if (app.action == ACTION_WRITE && *app.inputfile == 0)
    {
        printf(ERR_PREFIX "Input file not specified" NL);
        app_close(APP_EXITCODE_ERROR);
    }

    cport_open(app.cportpath, app.baud);

    switch (app.action)
    {
        case ACTION_WRITE:
            program_write();
            break;
        case ACTION_GO:
            program_go();
            break;
        default:
            debug_wmsg("Unknown action");
            break;
    }
}

/*
 *
 */
void app_close(int code)
{
    cport_close();
    exit(code);
}

/*
 *
 */
static void _print_head()
{
    printf(NL "Flash programming tool for STM8. Written and copyrights by Dmitry Kobylin." NL);
    printf("Version %u.%u.%u ("__DATE__")" NL, MAJOR, MINOR, BUILD);
    printf("THIS SOFTWARE COMES WITH ABSOLUTELY NO WARRANTY! USE AT YOUR OWN RISK!" NL NL);
}

/*
 *
 */
static void _print_help(int argc, char **argv)
{
    struct stm8chip_t *chip;
    int n;

    printf("Usage: %s <OPTIONS> <ACTION>"NL, argv[0]);
    printf(NL);
    printf("OPTIONS:"NL);
    printf("    -h, --help         print this help"NL);
    printf("    --cport=<path>     communication port for device"NL);
    printf("    --baud=<value>     baudrate (one of 4800, 9600, 19200, 38400, 57600, 115200)"NL);
    printf("    --input=<path>     input file"NL);
    printf("    --chip=<path>      chip to read/write"NL);
    printf(NL);
    printf("ACTION:"NL);
    printf("    write              write data to flash/EEPROM of target"NL);
    printf("       go              send GO command to target"NL);
    printf(NL);
    printf("Environment variables:" NL);
    printf("    CPORT_TTY       tty to use, overided by \"--cport\" option"NL
           "                    if specified"NL);

    printf("Supported chips:" NL);
    chip = stm8chips;
    n    = 0;
    while (*chip->name)
    {
        printf("    %s ", chip->name);
        chip++;
        n++;
        if ((n % 8) == 0)
            printf(NL);
    }
    printf(NL);
}

/*
 *
 */
static void _get_options(int argc, char** argv)
{
    int i;
    char *penv;
    char chip[PATH_MAX];

    if (argc <= 2)
    {
        _print_help(argc, argv);
        app_close(APP_EXITCODE_ERROR);
    }

    penv = getenv(ENV_CPORT_TTY);
    if (penv != NULL)
        strcpy(app.cportpath, penv);

    for (i = 1; i < argc - 1; i++)
    {
        if (strcmp("-h", argv[i]) == 0 || strcmp("--help", argv[i]) == 0)
        {
            _print_help(argc, argv);
            app_close(APP_EXITCODE_ERROR);
        } else if (sscanf(argv[i], "--cport=%s", app.cportpath)) {

        } else if (sscanf(argv[i], "--baud=%u", &app.baud)) {
            switch (app.baud)
            {
                case 4800:   app.baud = B4800; break;
                case 9600:   app.baud = B9600; break;
                case 19200:  app.baud = B19200; break;
                case 38400:  app.baud = B38400; break;
                case 57600:  app.baud = B57600; break;
                case 115200: app.baud = B115200; break;
                default:
                    printf(ERR_PREFIX "unknown baudrate \"%u\"" NL, app.baud);
                    app_close(APP_EXITCODE_ERROR);
            }
        } else if (sscanf(argv[i], "--chip=%s", chip)) {
            struct stm8chip_t *pc;

            pc = stm8chips;
            while (*pc->name)
            {
                if (strcmp(pc->name, chip) == 0)
                {
                    app.chip = pc;
                    break;
                }

                pc++;
            }
            if (!app.chip)
            {
                printf(ERR_PREFIX "Unknown chip \"%s\"" NL, chip);
                app_close(APP_EXITCODE_ERROR);
            }
        } else if (sscanf(argv[i], "--input=%s", app.inputfile)) {

        } else {
            printf(ERR_PREFIX "Unknown option \"%s\"" NL, argv[i]);
            app_close(APP_EXITCODE_ERROR);
        }
    }

#define ACTION_ARG  (argv[argc - 1])
    /* check action */
    if (strcmp(ACTION_ARG, "write") == 0)
        app.action = ACTION_WRITE;
    else if (strcmp(ACTION_ARG, "go") == 0)
        app.action = ACTION_GO;

    if (app.action == ACTION_UNKNOWN)
    {
        printf(ERR_PREFIX "Unknown action \"%s\"" NL, ACTION_ARG);
        app_close(APP_EXITCODE_ERROR);
    }

    if (!app.chip)
    {
        debug_emsg("Chip not specified");
        app_close(APP_EXITCODE_ERROR);
    }
}

