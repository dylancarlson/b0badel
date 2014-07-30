/*{ $Workfile:   misc.c  $
 *  $Revision:   1.15  $
 *
 *  miscellaneous b0badel BBS functions
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
#include "dirlist.h"
#include "clock.h"
#include "audit.h"
#include "event.h"
#include "protocol.h"
#include "dateread.h"
#include "externs.h"

/* GLOBAL functions:
**
** setclock()       allow changing of date
** configure()      sets terminal parameters via dialogue
** new_configure()  configure dialogue for new users
** readglobal()     core for read global-new
** ingestFile()     puts file in held message buffer
** typeWC()         send a file via WC protocol
** download()       display a file via a protocol
** typefile()       display a file via ascii
** tutorial()       first level for printing a help file
** dotutorial()     print a file from the #helpdir
** writeformatted() prints a helpfile.
** plural()         pluralise a message
** mCR()            generate a hard CR that looks like "\n "
** parsedate()      convert a citadel-format date
** dateok()         is this item okay to print?
** * WCHeader()       Give the `ready for WC download' message
** batch()          Set up/shut down a batch transfer.
** whazzit()        print a `huh' message on bad input
** uname()          return user name or "<anonymous>"
*/

extern struct clock now;

/* static prototypes */

static void dlstat          (char *fname, long size);
static int  moreglobal      (void);
static void writeformatted  (FILE *fd, int helpline);
static int  dl_not_ok       (long time, int size);
static int  WCHeader        (struct dirList *fn);


/*
** setclock() - gets the date from the aide and remembers it
*/
void setclock (void)
{
    int  tmp;
    struct clock tts;

    timeis(&tts);
    mPrintf("et date (current date is %s %s)\n ", formDate(), tod(NO));
    if (getNo("Change the date")) 
    {
        while (onLine()) 
        {
            tmp = asknumber("Year",  91L, 2090L, tts.yr + 1900);
            if (tmp > 99)
            {
                tmp -= 1900;
            }
            if (tmp > 90 && tmp < 190) 
            {
                tts.yr = tmp;
                break;
            }
            else
            {
                mPrintf("Year must be after 1990\n ");
            }
        }
        tts.mo = asknumber("Month", 1L, 12L,  tts.mo);
        tts.dy = asknumber("Day", 1L, 31L,    tts.dy);
        tts.hr = asknumber("Hour", 0L, 23L,   tts.hr);
        tts.mm = asknumber("Minute", 0L, 59L, tts.mm);
        if (onLine()) 
        {
            set_time(&tts);
            setUpEvent();       /* date changed so regrab the event stuff */
        }
    }
}


/*
** configure() - sets up terminal width etc via dialogue
*/
void configure (void)
{
    if (loggedIn) 
    {
        mCR();
        mPrintf ("\n Your current setup:\n ");
        mPrintf ("\n %s, %d nulls, linefeeds %s",
            (expert) ? "Expert" : "Not expert",
            termNulls,
            (termLF) ? "ON" : "OFF");
        mPrintf ("\n Screen width is set to %d", termWidth);
        mPrintf ("\n %s time messages were created",
            sendTime ? "Show" : "Don't show");
        mPrintf ("\n %s last old message with New",
            oldToo ? "Show" : "Don't show");

        mCR();

        if (getNo ("\n Reset your configuration") == NO)
            return;
    }

    mPrintf("\n Hit <Return> or <Enter> for default\n ");
    mCR();
    termWidth = asknumber ("Screen width", 32L, 255L, termWidth);
    termNulls = (getNo    ("Do you need nulls"))  
        ? asknumber ("   How many", 0L, 255L, 10) : 0;
    termLF    = getYes    ("Linefeeds after each CR");
    sendTime  = getYes    ("Display time in message headers");
    oldToo    = getNo     ("Show last old message with New");
    expert    = getNo     ("Use shorter `expert' prompts?");

    /**** make it real (sort of) ***/

    logBuf.lbwidth          = termWidth;
    logBuf.lbnulls          = termNulls;
    logBuf.lbflags.EXPERT   = expert;
    logBuf.lbflags.LFMASK   = termLF;
    logBuf.lbflags.AIDE     = aide;
    logBuf.lbflags.TIME     = sendTime;
    logBuf.lbflags.OLDTOO   = oldToo;
}


/*
** new_configure() - sets up terminal width etc. for new user
*/
void new_configure (void)
{
    mPrintf("\n Hit <Return> or <Enter> for default\n ");
    mCR();
    termWidth = asknumber ("Screen width", 32L, 255L, termWidth);
    termLF    = getYes    ("Linefeeds after each CR");

    /*** stash results and defualts in logBuf ***/

    logBuf.lbwidth          = termWidth;
    logBuf.lbnulls          = termNulls = 0;
    logBuf.lbflags.EXPERT   = expert = NO;
    logBuf.lbflags.LFMASK   = termLF;
    logBuf.lbflags.AIDE     = aide;
    logBuf.lbflags.TIME     = sendTime  = YES;
    logBuf.lbflags.OLDTOO   = oldToo    = YES;
}

#define REREADROOM -1

/*
** moreglobal() - pause before changing rooms in readglobal
*/
static int moreglobal (void)
{
    int c;

    while (1) 
    {
        if (thisRoom != MAILROOM && canreplyto(NEWoNLY)) 
        {

            mPrintf("\n No more new messages in %s.\n ",roomBuf.rbname);
            if (expert)
            {
                mPrintf(" g e%s q ? ", heldMessage ? " h" : "");
            }
            else
            {
                mPrintf(" G)oto next, E)nter%s, Q)uit ? ",
                    heldMessage ? ", H)eld" : "");
            }

            if (!onLine())
                return 0;
            c = toupper(iChar());
            if (c == 'H' && heldMessage) 
            {
                mPrintf("eld message\n ");
                hldMessage();
                return 1;
            }
            else if (c == 'E') 
            {
                mPrintf("nter\n ");
                replymsg(0);
                return 1;
            }
            else if (c == 'Q') 
            {
                mPrintf("uit\n ");
                outFlag = OUTSKIP;
                return 0;
            }
            else if (c == 'G') 
            {
                mPrintf("\b\n Goto ");
                return 1;
            }
            else if (c == '\n') 
            {
                mPrintf(" Goto ");
                return 1;
            }
            else if (c == 'N') 
            {
                mPrintf("\b\n Goto ");
                return 1;
            }
            else if (c == 'B' || c == 'A') 
            {
                mPrintf("\b\n Again ");
                return REREADROOM;
            }
            else if (c == '?')
                tutorial("nextroom.mnu");
            else 
                whazzit();
        }
        else
        {
            return 0;
        }
    }
}

/*
** readglobal() - core for read global-new
*/
int readglobal (void)
{
    int c, reloop;

    while (outFlag != OUTSKIP && onLine()) 
    {

        showMessages(NEWoNLY, NO);

        if (outFlag == OUTSKIP)
            return 0;

        if (singleMsg) 
            if (moreglobal() == REREADROOM)
                continue;

        if (outFlag == OUTSKIP || !onLine())
            return 0;

        if (!gotoRoom("", 'R'))
            return 0; 

        if (singleMsg)
            do 
            {     /* reloop loop */

                if (expert)
                    mPrintf ("\n n s z q ? ");
                else
                    mPrintf ("\n N)ew, S)kip, Z)ap, Q)uit ? ");

                if (!onLine())
                    return 0;

                c = toupper(iChar());

                reloop = NO;                

                if (c == '?')               /* universal plea for help */
                {
                    tutorial("thisroom.mnu");
                    reloop = YES;
                }
                else if (c == 'S')
                {
                    mPrintf("\b\n Skip %s\n Goto ", roomTab[thisRoom].rtname);

                    roomTab[thisRoom].rtflags.SKIP = YES;       /* Set SKIP bit */

                    if (gotoRoom("", 'S'))                  /* skip this room */
                        reloop = YES;
                    else
                        return 0;
                }
                else if (c == 'Q')          /* Q)uit */
                {
                    mPrintf("uit\n");
                    return 0;
                }
                else if (c == 'Z')           /* Z)Forget */
                {
                    mPrintf("\n Forget");
                    if ( thisRoom == LOBBY
                        || thisRoom == MAILROOM
                        || thisRoom == AIDEROOM )
                    {
                        mPrintf(" - you can't forget %s!\n ",
                            formRoom(thisRoom,NO));

                        reloop = YES;
                    }
                    else
                    {
                        mPrintf(" %s - ", roomBuf.rbname);

                        if (getNo ("confirm") != NO)
                        {
                            logBuf.lbgen[thisRoom] =
                                ((roomBuf.rbgen + FORGET_OFFSET) % MAXGEN)
                                << GENSHIFT;

                            if (gotoRoom("", 'S'))     /* skipto next room */
                                reloop = YES;
                            else
                                return 0;
                        }
                    }
                }
            } while (reloop)
            ;       /* stay in loop while S)kipping */

        givePrompt();
        mPrintf(".Read %sNew\n ", singleMsg ? "More " : "");
    }
    return 0;
}


/*
** ingestFile() - copy a file into the held mesg buffer
*/
int ingestFile (char *name)
{
    FILE *fd;
    register c, index;

    if (fd = safeopen(name, "r")) 
    {
        sendCinit();
        while ((c=getc(fd)) != EOF && sendCchar(c))
            ;
        sendCend();
        fclose(fd);
        return 1;
    }
    return 0;
}


/*
** typeWC() - Send a file via WC
*/
int typeWC (FILE *fd)
{
    int data;

    if (beginWC()) 
    {
        while ((data = fgetc(fd)) != EOF && (*sendPFchar)(data))
            ;
        endWC();
        return 1;
    }
    return 0;
}


/*
** download() - dumps a host file with no formatting
*/
int download (struct dirList *fn)
{
    FILE *fp;
    int status = YES;

    if (fp = safeopen(fn->fd_name, "rb")) 
    {
        if (status = (WCHeader(fn) && typeWC(fp))) 
        {
            if (loggedIn && cfg.download && !onConsole)
            {
                logBuf.lblimit += fn->fd_size;
            }
            if (!batchWC)
            {
                oChar(BELL);
            }
        }
        fclose(fp);
    }
    return status;
}


/*
** typefile() - dump out an ascii file
*/
int typefile (struct dirList *p)
{
    register c;
    register gc;
    FILE *fp;

    outFlag = OUTOK;
    if (fp=safeopen(p->fd_name, dFormatted ? "r" : "rb")) 
    {
        if (dFormatted)
        {
            writeformatted(fp, YES);
        }
        else
        {
            doCR();
            while ((c=getc(fp)) != EOF) 
            {
                conout(c);
                if (gc=gotcarrier())
                    modout(c);
                if (mAbort() || !(gc || onConsole))
                    break;
            }
        }
        fclose(fp);
    }
    return (outFlag != OUTSKIP);
}


/*
** tutorial() - prints a helpfile on the modem & console.
*/
int tutorial (char *topic)
{
    outFlag = OUTOK;

    return hotblurb (topic);
}


/*
** dotutorial() - print a file from the #helpdir
*/
int dotutorial (char *topic, int help)
{
    if (help && !expert)
    {
        mPrintf(pauseline);    /* char pauseline[] is in globals.c */
    }

    return hotblurb (topic);
}



/*
** writeformatted() - display a file
*/
static void writeformatted (FILE *fd, int helpline)
{
    char line[MAXWORD];

    outFlag = OUTOK;

    if (usingWCprotocol == ASCII && helpline && !expert)
    {
        mFormat(pauseline);
    }
    doCR();

    while (fgets(line, MAXWORD, fd))
    {
        mFormat(line);
    }
    mCR();
}


/*
** plural() - pluralise a message
*/
char *plural (char *msg, long number)
{
    static char scratch[40];

    sprintf(scratch, "%ld %s%s", number, msg, (number != 1L) ? "s" : "");
    return scratch;
}


/* 
** mCR() - generate a hard CR that looks like "\n "
*/
void mCR (void)
{
    mFormat("\n ");
}


/*
** parsedate() - convert a citadel-format date into a days-since-1970
**               long integer
*/
long parsedate (register char *s)
{
    int yr = 0, mon, dy = 0;
    extern char *monthTab[];

    if (*s) 
    {
        if (isdigit(*s)) 
        {
            while (isdigit(*s)) 
            {
                yr = (yr * 10) + (*s - '0');
                ++s;
            }
            if (yr >= 1900)
                yr -= 1900;
            now.yr = yr;
        }
        else 
            timeis(&now);
        for (mon=12; mon > 0; --mon)
            if (strnicmp(monthTab[mon],s,3) == 0)
                break;
        if (mon==0)
            return ERROR;
        now.mo = mon;
        while (*s && !isdigit(*s))
            ++s;
        if (!*s)
            return ERROR;
        while (isdigit(*s)) 
        {
            dy = (dy * 10) + (*s - '0');
            ++s;
        }
        now.dy = dy;
        return julian_date(&now);
    }
    else if (loggedIn)
        return logBuf.lblast;
    else
        return ERROR;
}


/*
** dateok() -- is this item okay to print?
*/
int dateok (long time)
{
    if (dPass == dAFTER)
    {
        return (time > dDate);
    }
    if (dPass == dBEFORE)
    {
        return (time < dDate);
    }
    return YES;
}


/*
** dl_not_ok() Check that this download can be done.
*/
static int dl_not_ok (long time, int size)
{
    long allowed, timeLeft();

    if (!onConsole) 
    {
        if (evtRunning && isPreemptive(nextEvt) && timeLeft(evtClock) < time) 
        {
            mPrintf("Not now.\n ");
            return YES;
        }
        allowed = cfg.download - logBuf.lblimit;
        if (cfg.download && size > allowed) 
        {
            mPrintf( (allowed>0) ? "Your download limit is %s\n "
                : "You have exceeded your download limit\n ",
                plural("byte", allowed));
            return YES;
        }
    }
    return NO;
}


/*
** dlstat() - print a download statistics message
*/
static void dlstat(char *fname, long size)
{
    mPrintf("\n [ %s - %s ]\n ", fname, plural("byte", size));
}


/*
** WCHeader() Give the 'ready for WC download' message.
*/
static int WCHeader (struct dirList *fn)
{
    long size, time, sectors;
    long timeLeft();
    int Protocol = usingWCprotocol;
    int tooLong = NO;
    char msg[80];

    if (batchWC) 
    {
        switch (usingWCprotocol) 
        {
        case XMODEM:
        case YMODEM:
            if (sendYhdr(fn->fd_name, fn->fd_size))
                break;
            /* otherwise fall into the default case.... */
        default:
            return NO;
        }
    }
    else
    {
        usingWCprotocol = ASCII;
        if (byteRate) 
        {
            sectors = (fn->fd_size + (long)(SECTSIZE - 1)) / (long)(SECTSIZE);
            time = (sectors*133L)/(long)(byteRate);

            if (dl_not_ok(time, fn->fd_size))
                return NO;

            dlstat(fn->fd_name, fn->fd_size);
        }
        
        switch (Protocol)
        {
        case XMODEM:
        case YMODEM:
            mPrintf(dlWaitStr, protocol[Protocol]);
            break;

        default:
            sprintf(msg, " Ready for %s download", protocol[Protocol]);
            if (!getYesNo(msg))
                return NO;
        }
        usingWCprotocol = Protocol;
    }
    if (!inNet)
        logMessage(READ_FILE, fn->fd_name, NO);

    return YES;
}


/*
** batch() Set up/shut down a batch transfer.
*/
int batch (int mode, struct dirList *list, int count)
{
    int Protocol = usingWCprotocol;
    long sectors, time, bytes;
    char msg[80];
    int i;

    if (mode == STARTUP) 
    {
        usingWCprotocol = ASCII;
        if (byteRate) 
        {
            sectors = time = bytes = 0L;
            for (i=0;i<count;i++) 
            {
                bytes += list[i].fd_size;
                sectors += (list[i].fd_size + (SECTSIZE-1))/(long)(SECTSIZE);
            }
            time = ((sectors+count) * 133L) / (long)(byteRate);
            if (dl_not_ok(time, bytes))
                return NO;
            dlstat(plural("file", (long)count), bytes);
        }
        mPrintf(xBatchStr, protocol[Protocol]);
        usingWCprotocol = Protocol;
        return YES;
    }
    else if (usingWCprotocol == YMODEM || usingWCprotocol == XMODEM)
    {
        return sendYhdr(NULL, 0L);
    }
    return NO;
}


/*
** whazzit() - print a `huh' message on bad input
*/
void whazzit (void)
{
    mPrintf("?%s\n ", expert ? "" : " (Type `?' for menu)");
    if (!onConsole)
    {
        mflush();
    }
}


/*
** uname() - return user name or <anonymous>
*/
char *uname (void)
{
    return loggedIn ? logBuf.lbname : "<anonymous>";
}
