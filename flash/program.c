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
#include <string.h>
/* */
#include <btorder.h>
#include <debug.h>
#include <stm8chip.h>
#include <srec.h>
#include <types.h>
#include "app.h"
#include "cport.h"
#include "ew.h"

#define CODE_SYNC   0x7F
#define CODE_ACK    0x79
#define CODE_NACK   0x1F

#define CODE_GET       0x00
#define CODE_READMEM   0x11
#define CODE_ERASEMEM  0x43
#define CODE_WRITEMEM  0x31
#define CODE_SPEED     0x03
#define CODE_GO        0x21

struct chipinfo_t {
#define SUPPORT_READMEM    (1 << 0)
#define SUPPORT_ERASEMEM   (1 << 1)
#define SUPPORT_WRITEMEM   (1 << 2)
#define SUPPORT_SPEED      (1 << 3)
#define SUPPORT_GO         (1 << 4)
    int cmd_supported;
    uint8_t version;
};

#define RESPONSE_TIMEOUT   500

#define DATABUF_SIZE       256

static struct chipinfo_t chipinfo;

static int _sync();
static int _get();
static int _go(uint32_t address);
static int _read(uint32_t offset, uint8_t *data, uint32_t len);
static int _write(uint32_t offset, uint8_t *data, uint32_t len);
static struct memdata_t *_getew(struct stm8chip_t *chip);


/*
 *
 */
void program_write()
{
    struct stm8chip_t *chip;
    struct memdata_t *ew;
    struct memdata_t *upload;

    chip = app.chip;
    ew = NULL;
    upload = NULL;

    stm8chip_print(chip);

    chipinfo.cmd_supported = 0;

    cport_set_timeout(RESPONSE_TIMEOUT);

    if (_sync() < 0)
    {
        debug_emsg("SYNC failed");
        goto error;
    }

    if (_get() < 0)
    {
        debug_emsg("GET failed");
        goto error;
    }

    if (!(chipinfo.cmd_supported & SUPPORT_READMEM))
    {
        debug_emsg("Read memory not supported by target");
        goto error;
    }

    if (!(chipinfo.cmd_supported & SUPPORT_WRITEMEM))
    {
        debug_emsg("Write memory not supported by target");
        goto error;
    }

    ew = _getew(chip);
    if (!ew)
    {
        debug_emsg("Failed to get E/W routines");
        goto error;
    }

    /* write erase/write routines */
    if (ew)
    {
        struct memdata_row_t *row;
        struct llist_t *loop;

        memdata_mkloop(ew, &loop);
        while ((row = memdata_next(&loop)))
        {
            if (_write(row->offset, row->data, row->length) < 0)
            {
                debug_emsg("Failed to upload E/W routines");
                goto error;
            }
        }
    }

    /* read BL option, write if necessary to enable bootloder */
    {
        uint16_t optbl;

        if (_read(chip->optbl, (uint8_t*)&optbl, 2) < 0)
        {
            debug_emsg("Failed to read OPTBL");
            goto error;
        }
        optbl = le16to_host(optbl);
        printf(TAB "OPTBL %04X" NL, optbl);
#define OPTBL_VAL  (host_tobe16(0x55AA))
        if (optbl != OPTBL_VAL)
        {
            optbl = host_tole16(OPTBL_VAL);
            if (_write(chip->optbl, (uint8_t*)&optbl, 2) < 0)
            {
                debug_emsg("Failed to write OPTBL");
                goto error;
            }
        }
    }

    /* upload program to target */
    {
        struct memdata_row_t *row;
        uint32_t elength;
        struct llist_t *loop;

        upload = srec_read(app.inputfile);
        if (!upload)
        {
            debug_emsg("Failed to read memory data");
            goto error;
        }
        memdata_pack(&upload);
#if 0
        memdata_print(upload);
#endif
        memdata_mkloop(upload, &loop);

        if (chip->eeprom.offset + chip->eeprom.length == chip->options.offset)
            elength = chip->eeprom.offset + chip->eeprom.length;
        else
            elength = chip->eeprom.offset;

        while ((row = memdata_next(&loop)))
        {
            /* sanity check */
            if (row->offset >= chip->eeprom.offset && row->offset + row->length <= chip->eeprom.offset + elength)
            {
                if (elength > chip->eeprom.length)
                    printf("Write EEPROM/OPTIONS" NL);
                else
                    printf("Write EEPROM" NL);
            } else if (row->offset >= chip->options.offset && row->offset + row->length <= chip->options.offset + chip->options.length) {
                printf("Write OPTIONS" NL);
            } else if (row->offset >= chip->flash.offset && row->offset + row->length <= chip->flash.offset + chip->flash.length) {
                printf("Write FLASH" NL);
            } else {
                debug_emsgf("Unknown memory region", "%06X %06X" NL, row->offset, row->length);
                goto error;
            }

            if (_write(row->offset, row->data, row->length) < 0)
            {
                debug_emsg("Failed to upload data to target");
                goto error;
            }
        }
    }

    if (_go(chip->flash.offset))
    {
        debug_emsg("Failed to execute GO");
        goto error;
    }

    printf(NL "SUCCESS" NL);
    goto cleanup;
error:
    app_close(APP_EXITCODE_ERROR);
cleanup:
    if (ew)
        memdata_destroy(ew);
    if (upload)
        memdata_destroy(upload);
}

/*
 *
 */
void program_go()
{
    struct stm8chip_t *chip;

    chip = app.chip;

    stm8chip_print(chip);

    chipinfo.cmd_supported = 0;

    cport_set_timeout(RESPONSE_TIMEOUT);

    if (_sync() < 0)
    {
        debug_emsg("SYNC failed");
        goto error;
    }

    if (_get() < 0)
    {
        debug_emsg("GET failed");
        goto error;
    }

    if (!(chipinfo.cmd_supported & SUPPORT_GO))
    {
        debug_emsg("GO not supported by target");
        goto error;
    }

    if (_go(chip->flash.offset))
    {
        debug_emsg("Failed to execute GO");
        goto error;
    }

    printf(NL "SUCCESS" NL);
    return;
error:
    app_close(APP_EXITCODE_ERROR);
}

/*
 *
 */
static int _sync()
{
    uint8_t cmd, ack;

    printf("SYNC ");

    cmd = CODE_SYNC;
    do 
    {
        if (cport_send(&cmd, 1) != 1)
            break;

        if (cport_recv(&ack, 1) != 1)
            break;

        printf("%02X" NL, ack);
        if (ack != CODE_ACK)
            return -1;
        return 0;
    } while (0);

    printf("TO" NL);

    return -1;
}

/*
 *
 */
static int _get()
{
    uint16_t cmd;
    uint8_t ack;
    uint8_t n;
    uint8_t data[DATABUF_SIZE];
    int i, len, rd;
    uint8_t version;

    printf("GET ");

    /* send Command ID + Complement */
    {
        cmd  = CODE_GET;
        cmd |= (cmd ^ 0xff) << 8;

        if (cport_send((uint8_t*)&cmd, 2) != 2)
            goto timeout;
    }

    /* receive ACK/NACK */
    {
        if (cport_recv(&ack, 1) != 1)
            goto timeout;

        printf("%02X ", ack);
        if (ack != CODE_ACK)
            goto error;
    }

    /* receive number of bytes to be sended by STM8 */
    {
        if (cport_recv(&n, 1) != 1)
            goto timeout;

        printf("%02X ", n);

        if (n == 0)
        {
            printf("NO COMMANDS SUPPORTED");
            goto error;
        }
    }

    /* receive bootloader version */
    {
        /* receive ACK */
        {
            if (cport_recv(&version, 1) != 1)
                goto timeout;

            printf("v%02X ", version);
            chipinfo.version = version;
        }
    }

    printf("| ");

    /* receive supported commands */
    {
        uint8_t *p;

        len = n;
        p   = data;
        while (n)
        {
            rd = cport_recv(p, n);
            if (rd == 0)
                goto timeout;

            n -= rd;
            p += rd;
        }

        for (i = 0; i < len; i++)
        {
            if (data[i] == CODE_READMEM ) chipinfo.cmd_supported |= SUPPORT_READMEM;
            if (data[i] == CODE_ERASEMEM) chipinfo.cmd_supported |= SUPPORT_ERASEMEM;
            if (data[i] == CODE_WRITEMEM) chipinfo.cmd_supported |= SUPPORT_WRITEMEM;
            if (data[i] == CODE_SPEED   ) chipinfo.cmd_supported |= SUPPORT_SPEED;
            if (data[i] == CODE_GO      ) chipinfo.cmd_supported |= SUPPORT_GO;

            printf("%02X ", data[i]);
        }
    }

    printf("| ");

    /* receive ACK */
    {
        if (cport_recv(&ack, 1) != 1)
            goto timeout;

        printf("%02X ", ack);
        if (ack != CODE_ACK)
            goto error;
    }

    printf(NL);
    return 0;
timeout:
    printf("TO");
error:
    printf(NL);
    return -1;
}

/*
 *
 */
static int _go(uint32_t address)
{
    uint16_t cmd;
    uint8_t ack;

    printf("GO ");

    /* send Command ID + Complement */
    {
        cmd  = CODE_GO;
        cmd |= (cmd ^ 0xff) << 8;

        if (cport_send((uint8_t*)&cmd, 2) != 2)
            goto timeout;
    }

    /* receive ACK/NACK */
    {
        if (cport_recv(&ack, 1) != 1)
            goto timeout;

        printf("%02X ", ack);
        if (ack != CODE_ACK)
            goto error;
    }

    /* send start address, checksum */
    {
        uint32_t addr;
        uint8_t  cs;

        addr = host_tobe32(address);

        cs  = (addr >>  0) & 0xff;
        cs ^= (addr >>  8) & 0xff;
        cs ^= (addr >> 16) & 0xff;
        cs ^= (addr >> 24) & 0xff;

        if (cport_send((uint8_t*)&addr, 4) != 4)
            goto timeout;
        if (cport_send(&cs, 1) != 1)
            goto timeout;
    }

    /* receive ACK/NACK */
    {
        if (cport_recv(&ack, 1) != 1)
            goto timeout;

        printf("%02X ", ack);
        if (ack != CODE_ACK)
            goto error;
    }

    printf(NL);
    return 0;
timeout:
    printf("TO");
error:
    printf(NL);
    return -1;
}

/*
 *
 */
static int _read(uint32_t offset, uint8_t *data, uint32_t len)
{
    uint16_t cmd;
    uint8_t ack;
    int ntry;
    int n;

#define MAX_BYTES_TO_READ   255
#define READ_NRETRY         1

    printf("READ  %06X %06X ", offset, len);

    if (len <= 0)
        return 0;

    ntry = READ_NRETRY;
    while (len > 0)
    {
        n = len;
        if (n > MAX_BYTES_TO_READ)
            n = MAX_BYTES_TO_READ;

        /* send Command ID + Complement */
        {
            cmd  = CODE_READMEM;
            cmd |= (cmd ^ 0xff) << 8;

            if (cport_send((uint8_t*)&cmd, 2) != 2)
                goto timeout;
        }

        /* receive ACK/NACK */
        {
            if (cport_recv(&ack, 1) != 1)
                goto timeout;

            if (ack != CODE_ACK)
                goto nack;
        }

        /* send start address, checksum */
        {
            uint32_t addr;
            uint8_t  cs;

            addr = host_tobe32(offset);

            cs  = (addr >>  0) & 0xff;
            cs ^= (addr >>  8) & 0xff;
            cs ^= (addr >> 16) & 0xff;
            cs ^= (addr >> 24) & 0xff;

            if (cport_send((uint8_t*)&addr, 4) != 4)
                goto timeout;
            if (cport_send(&cs, 1) != 1)
                goto timeout;
        }

        /* receive ACK/NACK */
        {
            if (cport_recv(&ack, 1) != 1)
                goto timeout;

            if (ack != CODE_ACK)
                goto nack;
        }

        /* send number of bytes to read */
        {
            uint16_t nread;

            nread  = n - 1;
            nread ^= (nread ^ 0xff) << 8;

            if (cport_send((uint8_t*)&nread, 2) != 2)
                goto timeout;
        }

        /* receive ACK/NACK */
        {
            if (cport_recv(&ack, 1) != 1)
                goto timeout;

            if (ack != CODE_ACK)
                goto nack;
        }

        /* receive data */
        len    -= n;
        offset += n;
        while (n > 0)
        {
            int rd;

            rd = cport_recv(data, n);
            if (rd == 0)
                goto timeout;

            n    -= rd;
            data += rd;
        }

        printf(".");
        ntry = READ_NRETRY;
        continue;
nack:
        printf("X");
        if (--ntry == 0)
            goto error;
    }

    printf(NL);
    return 0;
timeout:
    printf("TO");
error:
    printf(NL);
    return -1;
}

/*
 *
 */
static int _write(uint32_t offset, uint8_t *data, uint32_t len)
{
    uint16_t cmd;
    uint8_t ack;
    uint8_t cs;
    int ntry;
    int n;

#define MAX_BYTES_TO_WRITE   128
#define WRITE_NRETRY         1

    printf("WRITE %06X %06X ", offset, len);

    if (len <= 0)
        return 0;

    ntry = WRITE_NRETRY;
    while (len > 0)
    {
        n = len;
        if ((offset % MAX_BYTES_TO_WRITE) + n > MAX_BYTES_TO_WRITE)
            n = MAX_BYTES_TO_WRITE - (offset % MAX_BYTES_TO_WRITE);

//        printf("N = %u\n", n);

        /* send Command ID + Complement */
        {
            cmd  = CODE_WRITEMEM;
            cmd |= (cmd ^ 0xff) << 8;

            if (cport_send((uint8_t*)&cmd, 2) != 2)
                goto timeout;
        }

        /* receive ACK/NACK */
        {
            if (cport_recv(&ack, 1) != 1)
                goto timeout;

            if (ack != CODE_ACK)
                goto nack;
        }

        /* send start address, checksum */
        {
            uint32_t addr;

            addr = host_tobe32(offset);

            cs  = (addr >>  0) & 0xff;
            cs ^= (addr >>  8) & 0xff;
            cs ^= (addr >> 16) & 0xff;
            cs ^= (addr >> 24) & 0xff;

            if (cport_send((uint8_t*)&addr, 4) != 4)
                goto timeout;
            if (cport_send(&cs, 1) != 1)
                goto timeout;
        }

        /* receive ACK/NACK */
        {
            if (cport_recv(&ack, 1) != 1)
                goto timeout;

            if (ack != CODE_ACK)
                goto nack;
        }

        /* calculate checksum for data */
        {
            uint8_t *p;
            int l;

            cs = 0;
            l  = n;

            p = data;
            while (l--)
                cs ^= *p++;
        }

        /* write number of bytes to send */
        {
            uint8_t nsend;

            nsend = n - 1;
            cs ^= nsend;
            if (cport_send(&nsend, 1) != 1)
                goto timeout;
        }
        /* write data */
        if (cport_send(data, n) == 0)
            goto timeout;
        /* write checksum */
        if (cport_send(&cs, 1) == 0)
            goto timeout;

        /* receive ACK/NACK */
        {
            if (cport_recv(&ack, 1) != 1)
                goto timeout;

            if (ack != CODE_ACK)
                goto nack;
        }

        data   += n;
        offset += n;
        len    -= n;

        printf(".");
        ntry = WRITE_NRETRY;
        continue;
nack:
        printf("X");
        if (--ntry == 0)
            goto error;
    }

    printf(NL);
    return 0;
timeout:
    printf("TO");
error:
    printf(NL);
    return -1;
}

/*
 * Get erase / write routines
 */
static struct memdata_t *_getew(struct stm8chip_t *chip)
{
    struct memdata_t *md;
    struct ew_data_t *ewdata;

    md = NULL;
    switch(chipinfo.version)
    {
        case 0x10:
            {
                uint32_t flashsize;
                uint8_t dumb;

                /*
                 * Check flash size of target
                 */
                cport_set_timeout(100);

                flashsize = 0;

                if (_read(chip->flash.offset + (256 * 1024) - 1, &dumb, 1) == 0)
                    flashsize = 256;
                else if (_read(chip->flash.offset + (32 * 1024) - 1, &dumb, 1) == 0)
                    flashsize = 32;
                else if (_read(chip->flash.offset + (8 * 1024) - 1, &dumb, 1) == 0)
                    flashsize = 8;

                cport_set_timeout(RESPONSE_TIMEOUT);

                switch (flashsize)
                {
                    case 256: ewdata = &ew_data_256k_10; break;
                    case  32: ewdata = &ew_data_32k_10;  break;
                    case   8: ewdata = &ew_data_8k_10;   break;
                    default:
                        debug_emsg("Target not supported (unknown flash size)");
                        goto error;
                }
            }
            break;
        case 0x12: ewdata = &ew_data_32k_12;  break;
        case 0x13: ewdata = &ew_data_32k_13;  break;
        case 0x20: ewdata = &ew_data_128k_20; break;
        case 0x21: ewdata = &ew_data_128k_21; break;
        case 0x22: ewdata = &ew_data_128k_22; break;
        default:
            debug_emsg("Target not supported");
            goto error;
    }

    md = memdata_create();
    if (!md)
    {
        debug_emsg("Failed to create memory data for E/W routines");
        goto error;
    }

    if (memdata_add(md, ewdata->offset, (uint8_t*)ewdata->data, ewdata->length) < 0)
    {
        debug_emsg("Failed to add memory data for E/W routines");
        goto error;
    }

#if 0
    memdata_print(md);
#endif
    return md;
error:
    if (md)
        memdata_destroy(md);
    return NULL;
}

