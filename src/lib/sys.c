/*
 * Epson's sys.c of libstdio (gdb simulated I/O: _init_sys, read_str, write_str, read, write),
 * compiled into the firmware as a library of its own (config/library.txt: once, without
 * -mlong-calls). The text is C:\EPSON\GNU33\UTILITY\lib_src\libstdio\libstdio\src\sys.c as of
 * its 2007 release: the 2011 change (T.Tasaki, WRITE_BUF[0] = 0 in _init_sys and the faster
 * write()) is not in the firmware. Its .bss is placed at 0x84358 (tools/gen_lds.py BSS_LIB).
 */
/************************************************************************
 *                                                                      *
 *      Copyright (C) SEIKO EPSON CORP. 2007                            *
 *                                                                      *
 *      File name: sys.c                                                *
 *                                                                      *
 *      This is system file to support read(), write(), init_sys()      *
 *      with gdb simulated IO function.                                 *
 *      To use read(), write(), please call init_sys() in first.        *
 *                                                                      *
 *      Revision history                                                *
 *              2007.06.28      D.Fujimoto      for libstdio            *
 ************************************************************************/
#include <libstdio.h>
#include "common.h"

/* Prototype */
void read_str(char *);
void write_str(char *);
int read(int, char *, int);
int write(int, char *, int);

/* Buffer for simulated stdin, WRITE_BUF[0] is size (1-0x40, 0 means no data)
   WRITE_BUF[1-64] is buffer area for data, max 64 bytes
   used in write () */
volatile unsigned char WRITE_BUF[65] DSTRAM;

/* Buffer for simulated stdin, READ_BUF[0] is size (1-0x40, 0 means EOF)
   READ_BUF[1-64] is buffer area for data, max 64 bytes
   used in read() */
volatile unsigned char READ_BUF[65] DSTRAM;

/* If 1: READ_BUFFER become EOF, 0: not EOF */
static unsigned char READ_EOF DSTRAM;


/*******************************************************************************
 * _init_sys
 *   Type :     void
 *   Ret val :  none
 *   Argument : void
 *   Function : _init_sys initialize read() and write() buffer area.
 *******************************************************************************/
void _init_sys(void)
{
    READ_EOF = 0;           /* Not EOF */
    READ_BUF[0] = 0;        /* BUFFER is empty */
}


/*******************************************************************************
 * read_str
 *   Type :     void
 *   Ret val :  none
 *   Argument : char *str       Read string buffer pointer
 *   Function : Caller should reserve enough area for string,
 *              Send pointer to read_str, and read_str set string
 *              with 0 terminate.
 *******************************************************************************/
void read_str(char *str)
{
    int iSize;

    do {
        iSize = read(1, str, 1);    /* Read 1 char */
        if (iSize == 0){
            break;                  /* No string */
        }
        if (*str == '\r') {
            continue;               /* If CR, ignore and read again */
        }
        if (*str == '\n') {         /* If LF, break */
            break;
        }
        str++;
    } while (1);

    *str = 0;                       /* Instead of LF, put 0 */
    return;
}


/*******************************************************************************
 * write_str
 *   Type :     void
 *   Ret val :  none
 *   Argument : char *str       Write string buffer pointer
 *   Function : Send NULL terminate string to write.
 *******************************************************************************/
void write_str(char *str)
{
    int i;
    char *c;

    i = 0;
    c = str;
    while (*c != 0) {       /* Check string length */
        c++;
        i++;
    }
    write(1, str, i);       /* Write str */
    return;
}


/*******************************************************************************
 * read
 *   Type :     int
 *   Ret val:   read() get and return required bytes with using simulated input.
 *              If EOF return 0.
 *   Argument : int fhDummy             Dummy file handler
 *              char *psReadBuf         Buffer for read data
 *              int iReadBytes          Required byte size of read data
 *   Function : Read simulated input.
 *                      READ_FLASH: Break point label for stdin
 *                      READ_BUF: Buffer area for stdin
 *******************************************************************************/
int read(int fhDummy, char *psReadBuf, int iReadBytes)
{
    int iBytes;             /* Data size written to psReadBuf */
    int iSize;              /* Data size in READ_BUF */
    char *psBuf;            /* Top of read buffer */
    static int iNdxPos;     /* Current position in READ_BUF*/

    /* Start */
    iBytes = 0;             /* No read now */
    psBuf = psReadBuf;

    /* This loop repeat for each byte to copy READ_BUF to psReadBuf */
    for (;;) {

        /* If iReadByte become 0, return */
        if (iReadBytes == 0)    /* If required size become 0, return */
            break;          /* for() break */

        /* If EOF, return 0 */
        if (READ_EOF == 1)
            break;          /* for() break */

        /* If there is data, copy 1 byte */
        iSize = READ_BUF[0];
        if (iSize > 0 ) {
            *psBuf = READ_BUF[iNdxPos];
            psBuf++;
            iReadBytes--;
            iNdxPos++;
            iSize--;
            iBytes++;
            READ_BUF[0] = (unsigned char)(iSize & 0xff);
        } else {
        /* If no data, read 0-64 bytes from simulated input */
            asm(".global READ_FLASH");
            asm("READ_FLASH:");     /* Label for simulated stdin */
            if (READ_BUF[0] == 0)
                    READ_EOF = 1;   /* If size is 0, EOF */
            iNdxPos = 1;            /* Reset index position */
        }
    } /* back to for (;;) */

    return(iBytes);
}


/*******************************************************************************
 * write
 *   Type :     int
 *   Ret val:   write data byte size with using simulated stdout
 *   Argument : int fhDummy             Dummy file handler
 *              char *psWriteBuf        Buffer for write data
 *              int iWriteBytes         Byte size of write data
 *   Function : Write simulated output.
 *                      WRITE_FLASH: Break point label for stdout
 *                      WRITE_BUF: Buffer area for stdout
 *******************************************************************************/
int write(int fhDummy, char *psWriBuf, int iWriBytes)
{
    int iBytes;     /* Remain data bytes waiting WRITE_BUF */
    int iSize;      /* Data size to write to WRITE_BUF */
    int iCount;     /* Counter to copy data to WRITE_BUF */

    iBytes = iWriBytes;

    for (;;) {                  /* Repeat each 255 bytes */

        /* If remain 0, return original size */
        if (iBytes == 0)        /* Remain size become 0, so return */
                break;          /* for() break */

        /* If remain > 64, write size is 64 */
        if (iBytes > 64) {
            iSize = 64;             /* 64 bytes to WRITE_BUF */
            iBytes = iBytes - 64;   /* Remain data */
        } else {                    /* Not over 64 */
            iSize = iBytes;         /* Under 64 bytes to WRITE_BUF */
            iBytes = 0;             /* No remain data */
        }

        /* Copy psWriBuf to WRITE_BUF */
        WRITE_BUF[0] = (unsigned char)(iSize & 0xff);   /* Set size */
        for (iCount = 1 ; iCount <= iSize ; iCount++) {
            WRITE_BUF[iCount] = *psWriBuf;              /* Copy data */
            psWriBuf++;
        }
        asm(".global WRITE_FLASH");
        asm("WRITE_FLASH:");                            /* Label for simulated stdout */

    }       /* back to for (;;) */

    return(iWriBytes);
}
