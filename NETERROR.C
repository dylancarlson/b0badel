/*{ $Workfile:   neterror.c  $
 *  $Revision:   1.12  $
 * 
 *  error logging to a file for b0badel
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
 
#include <stdarg.h>

#include "ctdl.h"
#include "externs.h"

/* GLOBAL Contents:
**
** neterror()       write a message to the errorfile
** IngestLogfile()  read the error message into the message buffer
*/

static pathbuf logfile;        /* the name of the logfile */

/*
** neterror() - write a error message to a logfile
*/
void neterror (int hup, char *format, ...)
{
    va_list arg_ptr;

    va_start(arg_ptr, format);

    if (hup)
    {
        hangup();
    }

    if (!log) 
    {
        ctdlfile(logfile, cfg.netdir, "$logfile");

        if ((log=safeopen(logfile,"w")) == NULL) 
        {
            fprintf(stderr, "Logfile open failure!\n");
            return;
        }
        fputc(' ',log);
    }

    if (rmtname[0])
    {
        fprintf(log, "(%s) ", rmtname);
    }

    vfprintf(log, format, arg_ptr);
    fputs("\n ", log);
    fflush(log);
}


/*
** IngestLogfile()
*/
int IngestLogfile(void)
{
    return ingestFile(logfile);
}
