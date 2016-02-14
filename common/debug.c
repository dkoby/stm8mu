#include <stdio.h>
#include "debug.h"

/*
 *
 */
static void _phead(int size)
{
    int i;
    /* print header */
    {
        printf(NL);
        if (size < 0x100)
            printf("..");
        else if (size < 0x10000)
            printf("....");
        else if (size < 0xffffffff)
            printf(".........");

        printf("..");
        for (i = 0; i < 16; i++)
        {
            if (i && ((i % 8) == 0))
                printf(".");
            printf("%02X.", i);
        }
    }
}

void debug_buf(uint8_t *buf, uint32_t len)
{
    int i;
    uint32_t offset;

    offset = 0;
    for (i = 0; i < len; i++)
    {
        if ((i % 256) == 0)
            _phead(len);
        if ((i % 16) == 0)
        {
            printf("."NL);
            if (len < 0x100)
                printf("%02X", offset);
            else if (len < 0x10000)
                printf("%04X", offset);
            else if (len < 0xffffffff)
                printf("%08X", offset);
            printf("  ");
            offset += 16;
        } else {
            if ((i % 8) == 0)
                printf(" ");
        }

        printf("%02X ", *buf++);
    }
    printf(NL);
    len = 0;
}

