/*{ $Workfile:   liblog.c  $
 *  $Revision:   1.11  $
 * 
 *  Citadel log code for the library
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

#define LIBLOG_C

#include <io.h> 
#include "ctdl.h"
#include "library.h"

/* GLOBAL Contents:
**
**  getLog()   Load an userlog entry.
**  putLog()   Store an userlog entry.
**  getnmidx() Load the userlog entry for someone, return the logTab[] index.
**  getnmlog() Load the userlog entry for somebody, return the userlog slot.
*/

struct logBuffer logBuf;    /* Log buffer of a person */
int logfl;                  /* log file descriptor    */

static void cryptLog (struct logBuffer *p, int n);

#define LOGBUFSIZE (sizeof(struct logBuffer))

/*
** cryptLog - encrypte/decrypte log entry using index as key
*/
static void cryptLog (struct logBuffer *p, int n)
{
    crypte(p, (unsigned)LOGBUFSIZE, (unsigned)(n * 3));
}


/*
** getLog()
*/
void getLog (struct logBuffer *p, int n)
{
    int size;

    dseek(logfl, (long)n * LOGBUFSIZE, 0);

    size = dread(logfl, p, LOGBUFSIZE);

    if (size != LOGBUFSIZE)
    {
        crashout("getlog(%d) read %d/expected %d", n, size, LOGBUFSIZE);
    }
    cryptLog(p, n);
}


/*
** putLog()
*/
void putLog (struct logBuffer *p, int n)
{
    int size;

    cryptLog(p, n);

    dseek(logfl, (long)n * LOGBUFSIZE, 0);

    size = dwrite(logfl, p, LOGBUFSIZE);

    if (size != LOGBUFSIZE)
    {
        crashout("putlog(%d) wrote %d/expected %d", n, size, sizeof p[0]);
    }
    cryptLog(p, n);
}


/*
** getnmidx() loads log record for named person.
*/
int getnmidx (char *name, struct logBuffer *log)
{
    register namehash, i;
    int logNo;

    namehash = hash(name);

    for (i = 0; i < cfg.MAXLOGTAB; ++i)
    {
        if (logTab[i].ltnmhash == namehash) 
        {
            getLog(log, logTab[i].ltlogSlot);

            if (!labelcmp(name, log->lbname))
            {
                return i;
            }
        }
    }
    return ERROR;
}


/*
**  getnmlog() loads log record for named person.
*/
int getnmlog (char *name, struct logBuffer *log)
{
    register i = getnmidx(name, log);

    return (i == ERROR) ? i : logTab[i].ltlogSlot;
}
