/*{ $Workfile:   libdep.c  $
**  $Revision:   1.13  $
**
**  System dependent stuff that's used in b0badel and utility programs.
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
 
#define LIBDEP_C

#include <dos.h>
#include <bios.h>
#include <time.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "ctdl.h"
#include "library.h"
#include "clock.h"

#ifdef __ZTC__

#include <io.h>
#include <direct.h>

    #define dosdate_t   dos_date_t
    #define dostime_t   dos_time_t

    #define _dos_getdate(p)    dos_getdate(p)
    #define _dos_gettime(p)    dos_gettime(p)
    #define _dos_getdrive(p)   dos_getdrive(p)
    #define _dos_setdrive(n,p) dos_setdrive(n,p)

#else

    long clock();

#endif


/* GLOBAL Contents:
**
**  cd()            change your working directory.
**  getcd()         put the working directory into a string
**  upTime()        Return # seconds the system has been up
**  pause()         pauses for N/100 seconds.
**  timeSince()     how long since general timer init.
**  timeLeft()      Return # seconds left until an event.
**  diskSpaceLeft() amount of space left on specified disk.
**  timeis()        gets date from system.
**  julian_date()   returns a long representing the date
**  day_of_week()   return the ascii name for today
**  xmalloc()       Allocate memory or die
**  xstrdup()       dup a string or die
**  EOS()           return a pointer to end of string
**  safeopen()      open a non-tty file
**  ctdlfile()      construct a file in a citadel directory
*/


#define E_NO_ERR   0              /* the anti-error       */
#define E_NO_DSK  -1              /* no disk              */
#define E_NO_DIR  -2              /* no directory         */

/*
** cd() -- Change your current working directory.
*/
int cd (char *path)
{
    int      i;
    unsigned newdrive, thisdrive, dummy;

    if (path[1] == ':') 
    {
        newdrive = toupper(*path) - 'A' + 1;
        _dos_setdrive(newdrive, &dummy);
        _dos_getdrive(&thisdrive);

        if (thisdrive != newdrive)
        {
            return E_NO_DSK;    /* yikes! drive doesn't exist */
        }
        path += 2;      
    }

    if (path[0] == 0)           /* nothing == root directory.   */
    {
        path = "\\";
    }
    else
    {
        for (i = 0; path[i]; i++)       /* map all '/' into '\' */
        {
            if (path[i] == '/')
            {
                path[i] = '\\';
            }
        }

        if (i > 1 && path[i-1] == '\\') /* remove trailing '\'  */
        {
            path[i-1] = 0;
        }
    }

    if (chdir(path) != 0)
    {
        return E_NO_DIR;
    }

    return E_NO_ERR;
}


/*
** getcd() - return the current working directory.
*/
void getcd (pathbuf path)
{
    getcwd(path, 100);

    if (strlen(path) == 2 && path[1] == ':')
    {
        strcat(path,"\\");
    }
}


/*
** upTime() -- Return # seconds since the system came up
*/
long upTime (void)
{
    return clock()/CLK_TCK;
}


/*
** pause() busy-waits N/100 seconds
*/
void pause(int i)
{
#ifdef __ZTC__

    /*** Zortech/Turbo code by Nisi ***/

    clock_t clock_finish, clock_delay;

    clock_delay = (clock_t)(i * CLOCKS_PER_SEC / 100.0);
    clock_finish = clock_delay + clock();

    while (clock_finish > clock())
        ;

    return;

#else
    long x;
    long delay;

    delay = 10L * i;

    for (x = clock(); (clock()-x) < delay; )
        ;
#endif
}


/*
** timeSince() return # seconds since timer started
*/
long timeSince (long x)
{
    return (clock() - x) / CLK_TCK;
}


/*
** timeLeft() -- return # seconds left in this timer
*/
long timeLeft (long p)
{
    return p - upTime();
}

/*
** diskSpaceLeft() -- Amount of space left on the disk
*/
void diskSpaceLeft (char *path, long *sectors, long *bytes)
{
    int drv = (path[1] == ':') ? (*path & 0x1F) : 0;

#ifdef __ZTC__

    *bytes = dos_getdiskfreespace(drv);

    *sectors = ((*bytes) + (long)(SECTSIZE-1)) / (long)(SECTSIZE);

#else
    struct diskfree_t drive;

    _dos_getdiskfree(drv, &drive);

    *bytes = (long)(drive.avail_clusters) * (long)(drive.sectors_per_cluster)
        * (long)(drive.bytes_per_sector);

    *sectors = ((*bytes) + (long)(SECTSIZE-1)) / (long)(SECTSIZE);

#endif
}


/*
** timeis() gets raw date from system
*/
struct clock *timeis (struct clock *clk)
{
    struct dostime_t time;
    struct dosdate_t date;

    _dos_getdate(&date);
    _dos_gettime(&time);
    clk->yr = date.year - 1900;
    clk->mo = date.month;
    clk->dy = date.day;
    clk->hr = time.hour;
    clk->mm = time.minute;
    clk->ss = time.second;
    return clk;
}


/*
** julian_date() -- returns a long representing the date
*/
long julian_date (struct clock *time)
{
    long c, y, m, d;

    y = time->yr + 1900;        /* year - 1900 */
    m = time->mo;               /* month, 0..11 */
    d = time->dy;               /* day, 1..31 */

    if(m > 2)
    {
        m -= 3L;
    }
    else
    {
        m += 9L;
        y -= 1L;
    }
    c = y / 100L;
    y %= 100L;

    return ((146097L * c) >> 2)
           + ((1461L * y) >> 2)
           + (((153L * m) + 2) / 5)
           + d + 1721119L;
}


/*
** day_of_week() return the ascii name for today
*/
char *day_of_week (struct clock *clk)
{
    return _day[julian_date(clk) % 7];
}


/*
** xmalloc() -- Allocate memory or die
*/
void *xmalloc (int size)
{
    void *memory;

    if (size > 0)
    {
        memory = malloc(size);

        if (memory != NULL)
        {
            return memory;
        }
    }
    else    /*** note the programming error to stderr ***/
    {
        fprintf(stderr, "\nOuch! Someone just tried to malloc(%d).\n", size);
        return xmalloc(8);
    }

    crashout("xmalloc(%d) - out of memory", size);
}


/*
** xstrdup() -- strdup() or die
*/
char *xstrdup (char *string)
{
    char *memory = strdup(string);

    if (memory != NULL)
    {
        return memory;
    }

    crashout("xstrdup() out of memory");
}


/*
** EOS() -- return a pointer to end of string
*/
char *EOS (char *s)
{
    while (*s)
    {
        ++s;
    }
    return s;
}


#ifdef MSDOS
/*
** safeopen() -- open a non-tty file
*/
FILE *safeopen (char *fn, char *mode)
{
    FILE *handle;

    if (stricmp(fn, ".dir") == 0)
    {
        fn = "$DIR";    /* so we can net request *.* from STadel systems */
    }

    if (((handle = fopen(fn,mode)) != NULL) && isatty(fileno(handle))) 
    {
        fclose(handle);
        return NULL;
    }
    return handle;
}
#endif


/*
** ctdlfile() -- construct a filename in a citadel directory
*/
void ctdlfile (char *dest, int dir, char *fmt, ...)
{
    va_list ptr;

    sprintf(dest, "%s\\", &cfg.codeBuf[dir]);
    va_start(ptr, fmt);
    vsprintf(EOS(dest), fmt, ptr);
}
