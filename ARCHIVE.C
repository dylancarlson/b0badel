/*{ $Workfile:   archive.c  $
 *  $Revision:   1.11  $
 * 
 *  Routines to archive messages & things
}*/
/* 
**    Copyright (c) 1989, 1991 by Robert P. Lee 
**        Sebastopol, CA, U.S.A. 95473-0846 
**               All Rights Reserved 
** 
**    Permission is granted to use these files for 
**  non-commercial purposes only.  You may not include 
**  them in whole or in part in any commercial software. 
** 
**    Use of this source code or resulting executables 
**  by military organizations, or by companies holding 
**  contracts with military organizations, is expressly 
**  forbidden. 
*/ 
 
#include "ctdl.h"
#include "protocol.h"
#include "externs.h"

/* GLOBAL contents:
**
** ARsetup()        set up the system for archiving
** sendARchar()     dump a character to a file
** sendARend()      restore everything after archiving
*/

static int  (*pfc)(int);
static int  otermwidth;
static char oprotocol;

/*
** ARsetup() -- set up the system for archiving
*/
int ARsetup (char *filename)
{
    if (upfd = safeopen(filename, "a")) 
    {
        otermwidth = termWidth;
        termWidth = 79;
        oprotocol = usingWCprotocol;
        usingWCprotocol = TODISK;
        pfc = sendPFchar;
        sendPFchar = sendARchar;
        return 1;
    }
    return 0;
}


/*
** sendARchar() -- dump a character to a file
*/
int sendARchar (int c)
{
    return fputc(c, upfd) != EOF;
}


/*
** sendARend() -- restore everything
*/
int sendARend (void)
{
    sendPFchar = pfc;
    usingWCprotocol = oprotocol;
    termWidth = otermwidth;
    return fclose(upfd);
}
