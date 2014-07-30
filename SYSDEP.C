/*{ $Workfile:   sysdep.c  $
 *  $Revision:   1.13  $
 * 
 *  System-dependent stuff used inside b0badel
}*/

#include <stdarg.h>
#include <setjmp.h>
#include <process.h>
#include "ctdl.h"
#include "externs.h"
#include "ctdlnet.h"
#include "clock.h"
#include "audit.h"
#include "mach_dep.h"

#ifdef __ZTC__
    
    #define dosdate_t   dos_date_t
    #define dostime_t   dos_time_t

#endif

/************************************************************************
 *                                                                      *
 *      getArea()               get an area from the sysop.             *
 *      homeSpace()             takes us to our home space.             *
 *      xchdir()                chdir or print an error message         *
 *      * check_CR()            scan input for carriage returns.        *
 *      findBaud()              does flip flop search for baud.         *
 *      crashout()              Print an error message, log an error,   *
 *                              then roll belly-up and die.             *
 *      xprintf()               multitasking printf()                   *
 *      mPrintf()               writes a line to modem & console.       *
 *      wcprintf()              printf() for the networker.             *
 *      splitF()                debug formatter.                        *
 *      set_time()              set date (system dependent code).       *
 *      dosexec()               execute an external program             *
 *      systemCommands()        run outside commands in the O.S.        *
 *      systemInit()            system dependent init.                  *
 *      systemShutdown()        system dependent shutdown.              *
 *                                                                      *
 ************************************************************************/

#define NEEDED      0

#define ENOERR      0                   /* the anti-error       */
#define ENODSK     -1                   /* no disk              */
#define ENODIR     -2                   /* no directory         */

extern char *indextable;

static pathbuf homeDir;         /* directory where we live      */

/*
 ************************************************************************
 *      getArea() get area to assign to a directory room from sysop     *
 ************************************************************************
 */
getArea(roomData)
struct aRoom *roomData;
{
    char finished = NO;
    pathbuf nm;
    char message[100];

    if (roomData->rbdirname[0] == 0)    /* default to current directory */
        getcd(roomData->rbdirname);

    while (onLine()) 
    {
        mPrintf("Directory (C/R == `%s'): ",roomData->rbdirname);
        getNormStr("", nm, PATHSIZE-3, YES);
        if (strlen(nm) == 0)
            return;

        else if (cd(nm) == 0) 
        {
            sprintf(message, "Use existing %s", nm);
            if (getYes(message))
                break;
        }
        else
        {
            sprintf(message, "create new %s", nm);
            if (getYes(message))
                if (mkdir(nm) != 0 || cd(nm) != ENOERR)
                    mPrintf("Can't create directory\n ");
                else
                    break;
        }
        homeSpace();
    }
    getcd(roomData->rbdirname);
}


/*
** homeSpace() takes us home!
*/
void homeSpace (void)
{
    cd(homeDir);
}


/*
 ****************************************
 *      xchdir() - cd or error message  *
 ****************************************
 */
int xchdir (char *path)
{
    if (cd(path) != ENOERR) 
    {
        mPrintf("No directory <%s>\n ", path);
        homeSpace();
        return NO;
    }
    return YES;
}


/************************************************************************
 * BAUD HANDLER:                                                        *
 *    The code in here has to discover what baud rate the caller is at. *
 * For some computers, this should be ridiculously easy.                *
 ************************************************************************/

static OKBaud(speed)
{
    int receive();
    long ff;
    register c;

    setBaud(speed);
    mflush();

    if (cfg.connectPrompt) 
    {
        modputs("\007\r\nType return:");
        c = receive(5);
    }
    else
    {
        c = receive(1);
    }

    c &= 0x7f;

    return (c == '\r') || (c == 7);
}


static int scanmmesg(id)
char *id;
{
    register idx;

    for (idx=0; idx <= cfg.sysBaud; ++idx)
    {
        if (!strcmp(&cfg.codeBuf[cfg.mCCs[idx]], id)) 
        {
            setBaud(idx);
            return idx;
        }
    }
    return ERROR;
}


/*** for modems that return connection strings... ***/

#define MMESGSIZ 50

int mmesgbaud()
{
    char buffer[MMESGSIZ];
    register c, i;
    long x;

    for (i = 0, x = clock(); (clock() - x) < CLK_TCK; ) 
    {
        if (mdhit())
        {
            if ((c = getMod()) == '\r') 
            {
                buffer[i] = 0;

                if ((i = scanmmesg(buffer)) != ERROR)
                {
                    return i;
                }
                i = 0;
            }
            else if (i < MMESGSIZ-1)
            {                           /* truncate long messages. */
                buffer[i++] = 0x7f & c; /* and only accept 7 bits. */
            }
            else
            {
                i = 0;
            }
        }
    }

    buffer[i] = 0;

    return scanmmesg(buffer);
}


static
scanbaud()
{
    int  bps;
    long time;

    pause(100*cfg.connectDelay);

    if (cfg.modemCC && mmesgbaud() != ERROR)
        return YES;

    if (cfg.search_baud && (cfg.sysBaud != ONLY_300)) 
    {
        for (bps=0, time=clock(); gotcarrier() && timeSince(time) < 45L; )
            if (OKBaud(bps))
                return YES;
            else
                bps = (1+bps) % (1+cfg.sysBaud);

        return NO;
    }

    setBaud(cfg.sysBaud);
    return YES;
}


int findBaud()
{
    if (scanbaud()) 
    {                   /* if we were able to figure out the baud */
        mflush();       /* flush all input on the modem and wait  */
        pause(50);      /* for half a second before returning     */
        return YES;
    }
    return NO;
}


/*
 ************************************************************************
 * crashout() -- something really* horrible has happened, so print      *
 *               an appropriate message and abandon cpu                 *
 ************************************************************************
 */
void crashout (char *msg, ...)
{
    FILE *fd, *safeopen();
    va_list arg_ptr;

    fd = safeopen("crash", "a");
    outFlag = IMPERVIOUS;

    mPrintf("\n Whoops! Think I'll die now....\n ");

    va_start(arg_ptr, msg);

    vsprintf(msgBuf.mbtext, msg, arg_ptr);
    printf("%s\n", msgBuf.mbtext);

    if (fd) 
    {
        fputs(msgBuf.mbtext, fd);
        fprintf(fd," (%s @ %s)\n", formDate(), tod(NO));
        fclose(fd);
    }
    exitCitadel(CRASH_EXIT);
}


/************************************************************************
 * SYSTEM FORMATTING:                                                   *
 *    These functions take care of formatting to strange places not     *
 * handled by normal C library functions.                               *
 *   xprintf() print to the console if citadel is attached.             *
 *   mPrintf() print out the modem port via a mFormat() call.           *
 *   wcprintf() print out modem port via WC, append a 0 byte.           *
 *   splitF() debug function, prints to both screen and disk.           *
 ************************************************************************/


/*
 ************************************************
 *      mPrintf() printf() to modem and console *
 ************************************************
 */
void mPrintf (char *format, ...)
{
    va_list arg_ptr;
    char string[MAXWORD];

    va_start(arg_ptr, format);
    vsprintf(string, format, arg_ptr);
    mFormat(string);
}

/*
 ************************************************
 *      splitF() printf() to file and console   *
 ************************************************
 */
void splitF (FILE *diskFile, char *format, ...)
{
    va_list arg_ptr;
    char string[MAXWORD];

    va_start(arg_ptr, format);
    vsprintf(string, format, arg_ptr);
    fputs(string, stdout);
    if (diskFile) 
    {
        fputs(string,diskFile);
        fflush(diskFile);
    }
}

/*
 ****************************************
 *      wcprintf() networker printf()   *
 ****************************************
 */
void wcprintf(char *format, ...)
{
    va_list arg_ptr;
    char string[MAXWORD];
    register char *p;
    extern int (*sendPFchar)();

    va_start(arg_ptr, format);
    vsprintf(string, format, arg_ptr);
    for (p=string; *p;)
        (*sendPFchar)(*p++);
    (*sendPFchar)(0);
}


/*
 ****************************************
 *      set_time() set system date      *
 ****************************************
 */
set_time(clk)
struct clock *clk;
{
    struct dosdate_t date;
    struct dostime_t time;

    dos_getdate(&date);
    dos_gettime(&time);
    date.day = clk->dy;
    date.month = clk->mo;
    date.year = clk->yr+1900;
    time.hour = clk->hr;
    time.minute = clk->mm;
    if (dos_setdate(&date) || dos_settime(&time))
        return NO;
    return YES;
}


/*
 ************************************************************************
 * dosexec() -- execute a subprocess with the `normal' environment      *
 ************************************************************************
 */
extern char *indextable;

long dosexec (char *cmd, char *tail)
{
    extern long far pexec(int, char far *, char far *, char far *);
    pathbuf here;
    long status;
    char pstring[130];

    sprintf(pstring, "%c%s",strlen(tail), tail);
    getcd(here);
    writeSysTab();
    fixmodem();
    signal(SIGINT,SIG_DFL);

    /* status = pexec(0, cmd, pstring, (char far *)(0)); */

    {   /* new code replaces pexec */

        char *args[32];     /* few programs take more than 30 arguments! */
        char *p = tail;
        int i = 1;

        args[0] = cmd;

        if (p != NULL)
        {
            for (i = 1; i < 31; ++i)
            {
                while (*p && (*p == ' '))  /* skip leading spaces */
                    ++p;

                if (*p)
                {
                    args[i] = p++;

                    while (*p && (*p != ' '))
                    {
                        ++p;
                    }
                    if (*p)
                    {
                        *p++ = 0;
                    }
                }
                else break;
            }
        }
        args[i] = NULL;
        status = (long)spawnvp(0, cmd, args);
    }

    signal(SIGINT,SIG_IGN);
    setmodem();
    cd(here);
    unlink(indextable);
    return status;
}


/*
 ********************************************************
 *      systemInit() system dependent initialization    *
 ********************************************************
 */
systemInit()
{
    signal(SIGINT, SIG_IGN);
    getcd(homeDir);             /* get our home directory.              */
    setmodem();                 /* get the appropriate modem handler... */
}


/*
 ********************************************************
 *      systemShutdown() system dependent shutdown code *
 ********************************************************
 */
systemShutdown()
{
    homeSpace();
    fixmodem();
    signal(SIGINT,SIG_DFL);
    xprintf("Bye\n");
}
