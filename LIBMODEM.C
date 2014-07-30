/*{ $Workfile:   libmodem.c  $
 *  $Revision:   1.12  $
 * 
 *  functions that talk to the modem
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
 
#define LIBMODEM_C

#include <time.h>
#include "ctdl.h"
#include "library.h"

/* GLOBAL Contents:
**
** modputs()        Force a string out to the modem.
** rawmodeminit()   initialize modem.
** hangup()         hang up the phone.
** receive()        read modem char or time out.
*/


/*
** modputs() - force a string out to remote. modputs lets you put
**                 delays into the output by putting a %NN% into the
**                 string.  A %NN% will make modputs stop for NN tenths
**                 of a second before continuing.
*/
void modputs (char *s)
{
    char c;

    while ((c = *s++) != 0) 
    {
        if (c == '%' && *s) 
        {
            if (isdigit(*s)) 
            {
                int  delay = 0;

                for (delay = 0; isdigit(*s); ++s)
                {
                    delay = (delay * 10) + (*s - '0');
                }
                if (*s == '%')
                {
                    ++s;
                }
                if (Debug)
                {
                    printf("[%d]", delay);
                }
                pause(10 * delay);
                continue;
            }
            c = *s++;
        }
        if (Debug)
        {
            putch(isprint(c) ? c : '.');
        }
        pause(5);       /* ??? */
        modout(c);
    }
}


/*
** rawmodeminit() - Initializes the modem in a system dependent manner.
*/
void rawmodeminit (void)
{
    modemOpen();                                /* enable the modem     */
    setBaud(cfg.probug);                        /* setup baud rate      */
    modputs(&cfg.codeBuf[cfg.modemSetup]);      /* reset the modem      */
}


/*
** hangup() - This code _must_ force hangup, one way or the other.
*/
void hangup (void)
{
    if (gotcarrier()) 
    {
        modemClose();
        startTimer();
        while (gotcarrier() && (chkTimeSince() < 30))
            ;
    }
    else
    {
        modout('\r');                   /* kick the modem out of dialing, */
        pause(50);                      /* then wait .5 seconds           */
    }
    rawmodeminit();                     /* reset the 'ol modem.           */
}


/*
** receive() -- wait a while for a modem character
*/
int receive (int seconds)
{
    register long delay = (long)(CLK_TCK) * (long)(seconds);
    register long start = clock();

    while (gotcarrier()) 
    {
        if (mdhit())
        {
            return getraw();
        }

        if ((clock() - start) >= delay)
        {
            return ERROR;
        }
    }

    return ERROR;   /* carrier lost! */
}
