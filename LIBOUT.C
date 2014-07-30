/*{ $Workfile:   libout.c  $
 *  $Revision:   1.12  $
 * 
 *  output functions
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
 
#define LIBOUT_C

#include "ctdl.h"
#include "externs.h"
#include "protocol.h"

/*
** doCR()                  do a CR...
** oChar()                 top-level user-output function
** conout()                Dump a character to the console.
*/

/*
** doCR() does a newline on modem and console
*/
void doCR (void)
{
    crtColumn = 1;

    if (outFlag != OUTOK && outFlag != IMPERVIOUS)
    {
        return; /* output is being s(kip)ped    */
    }

    if (usingWCprotocol == ASCII)
    {
        if (haveCarrier) 
        {
            int i = termNulls;

            modout('\r');

            while(i--)
            {
                modout(0);
            }

            if (termLF)
            {
                modout('\n');
            }

            if (echo == BOTH)
            {
                xCR();
            }
        }
        else
        {
            xCR();
        }
    }
    else
    {
        (*sendPFchar)('\n');
    }

    if (CRfill != NULL) 
    {
        if (termWidth >= 39)            /* for odd printing formats     */
        {
            mPrintf(CRfill, ' ');       /* like .RE or ;K in tiny term  */
        }
        else
        {
            mPrintf("  ");              /* there's no room for CRfill   */
        }
    }
}


/*
** oChar() is the top-level user-output function
**   sends to modem port and console both
*/
void oChar (char c)
{
    if (outFlag != OUTOK && outFlag != IMPERVIOUS)      /* s(kip) mode  */
    {
        return;
    }

    if (c == '\n')
    {
        doCR();     /* doCR() is itself sensitive to usingWCprotocol */
        return;
    }

    if (usingWCprotocol != ASCII)
    {
        (*sendPFchar)(c);       /* whatever routine is current */
    }
    else
    {
        if (c == '\r')          /* ignore; doCR() handled it */
        {
            return;
        }
        if (echo == CALLER)
        {
            if (onConsole)
            {
                conout(c);
            }
            else if (haveCarrier)
            {
                modout(c);
            }
        }
        else /* if (echo == BOTH) */
        {
            conout(c);

            if (haveCarrier)
            {
                modout(c);
            }
        }
    }

    ++crtColumn;
    return;
}


/*
** conout()
*/
void conout (int c)
{
    if (c)
    {
        if ((echo == BOTH) || onConsole) 
        {
            if (c == '\n')
            {
                xCR();      /* output crlf to console */
            }
            else if (c != BELL)
            {
                putch(c);   /* output any char except BELL */
            }
        }
    }
}
