/*{ $Workfile:   libmisc.c  $
 *  $Revision:   1.12  $
 * 
 *  functions that everyone uses
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
 
#define LIBMISC_C

#include <string.h>
#include "ctdl.h"
#include "externs.h"
#include "clock.h"

/* GLOBAL Contents:
**
** tod()        forms the current time.
** formDate()   forms the current date.
** formRoom()   returns a string with the room formatted
** normalise()  clean up a string.
** copystring() Copy a <n> byte string, null terminate dest
*/

char *monthTab[] = 
{
    "", "Jan", "Feb", "Mar",
    "Apr", "May", "Jun",
    "Jul", "Aug", "Sep",
    "Oct", "Nov", "Dec"
};

struct clock now;               /* global clock used by all clock routines */


/*
** tod() - forms the current time
*/
char *tod (int stdTime)
{
    static char timeBuf[20];
    int hr, m;

    timeis(&now);

    if (stdTime) 
    {
        hr = now.hr;
        m = (hr >= 12) ? 'p' : 'a';

        if (hr >= 13)
        {
            hr -= 12;
        }

        if (hr == 0)
        {
            hr = 12;
        }
        sprintf(timeBuf, "%d:%02d %cm", hr, now.mm, m);
    }
    else
    {
        sprintf(timeBuf, "%d:%02d", now.hr, now.mm);
    }
    return timeBuf;
}


/*
** formRoom() - format the roomname into a string
*/
char *formRoom (int roomno, int markprivate)
{
    static char display [40];
    static char frMatrix[2][2] =    /* room suffix characters */
    {
        { '>', ')' },
        { ']', ':' }
    };

    int one = roomTab[roomno].rtflags.ISDIR  ? 1 : 0;
    int two = roomTab[roomno].rtflags.SHARED ? 1 : 0;

    if (roomTab[roomno].rtflags.INUSE) 
    {
        sprintf(display, "%s%c%s",
            roomTab[roomno].rtname, frMatrix[one][two],
            (markprivate && !roomTab[roomno].rtflags.PUBLIC) ? "*" : "");

        return display;
    }
    return "";
}


/*
** formDate() - forms the current date.
*/
char *formDate (void)
{
    static char dateBuf[10];

    timeis(&now);
    sprintf(dateBuf, "%02d%s%02d", now.yr, monthTab[now.mo], now.dy);
    return dateBuf;
}


/*
** normalise() - normalize a string
*/
void normalise (char *s)
{
    char *pc, *base;

    for (base = pc = s; isspace(*s); ++s)       /* eat leading spaces   */
        ;

    while (*s)                                  /* copy the string over */
    {
        if (*s == ' ' && s[1] == ' ')           /* killing runs of ` 's */
        {
            s++;
        }
        else
        {
            *pc++ = *s++;
        }
    }

    *pc = 0;

    while (pc > base && isspace(pc[-1]))        /* eat trailing spaces */
    {
        *--pc = 0;
    }
}


/*
** copystring() - copy a string of length (size) and make it null-terminated.
*/
void copystring (char *s1, char *s2, int size)
{
    strncpy(s1, s2, size);
    s1[size-1] = 0;
}
