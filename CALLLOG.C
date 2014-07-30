/*{ $Workfile:   calllog.c  $
 *  $Revision:   1.11  $
 * 
 *  handles b0badel call log
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

#define CALLLOG_C
 
#include <stdarg.h>
#include <string.h>
#include "ctdl.h"
#include "clock.h"
#include "audit.h"
#include "externs.h"

/* GLOBAL contents:
**
** logMessage()     Put out str to file.
*/

static void record (char *file, const char *format, ...); 

static char  newuser, evil;
static label person = { 0 };
static int   lastdy, curBaud = 0;
static int   userBaud;


/*
** logMessage() -   Puts message out.
**                  Also, on date change and first output of system,
**                  insert blank line.
*/
void logMessage (char val, char *str, char sig)
{
    label x;
    char *dt;
    char *sep = "";

    if (!cfg.call_log)  /* sysop doesn't keep a log? */
    {
        return;
    }

    switch (val) 
    {
    case FIRST_IN:
        if (cfg.call_log & aEXIT)
        {
            record("call", "\nSystem brought up %s @ %s\n",
                formDate(), tod(NO));
        }
        lastdy = now.dy;
        return;

    case LAST_OUT:
        if (cfg.call_log & aEXIT)
        {
            record("call", "System brought down %s @ %s\n",
                formDate(), tod(NO));
        }
        return;

    case BAUD:
        curBaud = byteRate;
        return;

    case L_IN:
        if (cfg.call_log & aLOGIN) 
        {
            strcpy(person, str);
            newuser = sig;
            /* if we logged in while on console, set our baudrate
             * to zero.
             */
            userBaud = onConsole ? 0 : curBaud;
            evil = NO;
            dt = formDate();

            if (lastdy != now.dy) 
            {
                sep = "\n";
                lastdy = now.dy;
            }
            record("call", "%s%-20s: %s %s - ", sep, str, dt, tod(NO));
        }
        return;

    case EVIL_SIGNAL:
        evil = YES;
        return;

    case L_OUT:
        if (person[0] && (cfg.call_log & aLOGIN)) 
        {
            sprintf(x, userBaud ? "%d0" : "console", curBaud);
            record("call", "%s (%s) %c %c %c\n",
                tod(NO), x,   newuser ? newuser : ' ',
                sig ? sig : ' ', evil ? 'E' : ' ');

            person[0] = 0;
        }
        return;

    case READ_FILE:
        if (cfg.call_log & aDNLOAD)
        {
            record("file", "%s read by %s (%s @ %s)\n", str, person,
                formDate(), tod(YES));
        }

        /* fall through */
    default:
        return;
    }
} 


/*
** record() statistics in a logfile
*/
static void record (char *filename, const char *format, ...)
{
    pathbuf filespec;
    FILE    *file;

    ctdlfile(filespec, cfg.auditdir, "%slog.sys", filename);

    file = safeopen(filespec, "a");

    if (file)
    {
        va_list arg_ptr;    /* pointer to variable argument list */

        va_start(arg_ptr, format);
        vfprintf(file, format, arg_ptr);
        va_end(arg_ptr);
        fclose(file);
    }
    else
    {
        crashout("audit failure on %s", filespec);
    }
}
