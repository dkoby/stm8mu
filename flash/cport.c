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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/select.h>
#include <termios.h>
#include <errno.h>
/* */
#include <debug.h>
#include "app.h"
#include "cport.h"

#define COMMDEV_DEBUG

#ifdef COMMDEV_DEBUG
    #define COMMDEV_DEBUGF(...) printf(__VA_ARGS__)
#else
    #define COMMDEV_DEBUGF(...)
#endif

#define BAUDRATE B115200

static struct cport_t {
    int fd;
    struct termios otios;
} cport;

#define DEFAULT_RECV_TIMEOUT   500
static uint32_t timeout = DEFAULT_RECV_TIMEOUT;

/*
 *
 */
void cport_init()
{
    cport.fd = -1;
}

/*
 * open USB device
 *
 * ARGS
 *     cport    pointer to cport_t structure
 */
void cport_open(char *dev, speed_t baud)
{
    struct termios tios;

    cport.fd = open(dev, O_RDWR | O_NOCTTY | O_NONBLOCK); 
    if (cport.fd < 0)
    {
	perror(dev);
	app_close(APP_EXITCODE_ERROR);
    }

    tcgetattr(cport.fd, &tios);

    /* save old setting */
    memcpy(&cport.otios, &tios, sizeof(struct termios));
    
    tios.c_iflag = IGNBRK;
    tios.c_oflag = 0;
    tios.c_cflag = CS8 | CREAD | PARENB | CLOCAL; /* 8 data bits, enable parity generation (even) */
    tios.c_lflag = 0;
    tios.c_cc[VMIN]   = 1;
    tios.c_cc[VTIME]  = 0;

    cfsetospeed(&tios, baud);
    cfsetispeed(&tios, baud);
    tcsetattr(cport.fd, TCSAFLUSH, &tios);
    tcflush(cport.fd, TCIOFLUSH);
}

/*
 *
 */
void cport_close()
{
    if (cport.fd < 0)
        return;

    tcsetattr(cport.fd, TCSAFLUSH, &cport.otios);
    close(cport.fd);
}

#define IO_TIMEOUT         200

/*
 * RETURN
 *     count of bytes sended
 */
int cport_send(uint8_t *buf, int len)
{
    fd_set wset;
    int n, wr, sl;
    struct timeval tv; 

    n = len;
    while (len)
    {
        FD_ZERO(&wset);
        FD_SET(cport.fd, &wset);
        tv.tv_sec  = IO_TIMEOUT / 1000;
        tv.tv_usec = (IO_TIMEOUT % 1000) * 1000;

        sl = select(cport.fd + 1, NULL, &wset, NULL, &tv);
        if (sl == 0)
            return 0;
        if (sl < 0)
        {
            if (errno == EINTR)
                printf("select EINTR" NEW_LINE);

            perror(__FUNCTION__);
            app_close(APP_EXITCODE_ERROR);
        }

        wr = write(cport.fd, buf, len);
        if (wr < 0)
        {
            perror(__FUNCTION__);
            app_close(APP_EXITCODE_ERROR);
        }
        len -= wr;
        buf += wr;
    }

    /* is that necessary? */
    tcdrain(cport.fd);

    return n;
}

/*
 * RETURN
 *     zero on timeout, count of bytes received on success, exit from program
 *     on errors
 */
int cport_recv(uint8_t *buf, int len)
{
    fd_set rset;
    struct timeval tv; 
    int sl, rd;

    FD_ZERO(&rset);
    FD_SET(cport.fd, &rset);
    tv.tv_sec  = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    sl = select(cport.fd + 1, &rset, NULL, NULL, &tv);
    if (sl == 0)
        return 0;
    if (sl < 0)
    {
        if (errno == EINTR)
            printf("select EINTR" NEW_LINE);

        perror(__FUNCTION__);
        app_close(APP_EXITCODE_ERROR);
    }

    rd = read(cport.fd, buf, len);
    if (rd < 0)
    {
        if (errno == EINTR)
            printf("EINTR" NEW_LINE);

        perror(__FUNCTION__);
        app_close(APP_EXITCODE_ERROR);
    }

    return rd;
}

/*
 *
 */
void cport_set_timeout(uint32_t timeout)
{
    timeout = timeout;
}

