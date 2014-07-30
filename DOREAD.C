/*{ $Workfile:   doread.c  $
 *  $Revision:   1.16  $
 *
 *  b0badel [E]nter commands
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
 

#include <fcntl.h>
#include "ctdl.h"
#include "dirlist.h"
#include "protocol.h"
#include "dateread.h"
#include "externs.h"

static void systat      (void);
static int alphasort    (struct dirList *s1, struct dirList *s2);
static void wildcard    (int (*fn)(struct dirList *), char *pattern);

char WC;                        /* WC mode rwProtocol returns           */
char dPass = 0;                 /* for reading from a date              */
long dDate;                     /* the date in julian_date form         */
char dFormatted = 0;            /* display formatted files              */

static long FDSectCount;        /* size of files in directory   */
static char FDextended;         /* list extended directory?     */

/*
** getpdate() get a date for reading from
*/
static int getpdate()
{
    label adate;
    long parsedate();

    if (dPass) 
    {
        mPrintf("%s when? ", (dPass==dAFTER)?"After":"Before");
        getNormStr("", adate, NAMESIZE, YES);
        if ((dDate = parsedate(adate)) == ERROR) 
        {
            mPrintf("bad date\n ");
            return NO;
        }
    }
    return TRUE;
}


/*
** fileok() check download permissions
*/
static int fileok()
{
    if (roomBuf.rbflags.ISDIR) 
    {
        if (roomBuf.rbflags.DOWNLOAD || SomeSysop())
            return TRUE;
        else
            mPrintf("\b This directory %s is for uploads only.\n ", room_area);
    }
    else
        mPrintf("\b - This is not a directory %s\n ", room_area);
    return NO;
}


/*
** readfiles() read files.
*/
static void readfiles (int protocol, char *fname)
{
    if (protocol == ASCII)
    {
        wildcard(typefile, fname);
    }
    else if (protocol != TODISK) 
    {
        usingWCprotocol = protocol;
        wildcard(download, fname);
        usingWCprotocol = ASCII;
    }
    else
    {
        mPrintf("Can't journal files!\n ");
    }
}


/*
** printdir() prints out one filename and size, for a dir listing
*/
static int printdir (struct dirList *fn)
{
    char *desc;
    extern char *getTag();
    extern char *monthTab[];

    if (fn->fd_name[0] != '$')  /* $dir is the directory file */
    {
        outFlag = OUTOK;

        if (FDextended) 
        {
            desc = getTag(fn->fd_name);
            CRfill = (termWidth > 62) ? "%13c|                " : "  ";
            mPrintf("%-13s|%7ld ", fn->fd_name,fn->fd_size);
            mPrintf("%02d%s%02d ", fn->fd_date._year,
                monthTab[fn->fd_date._month],
                fn->fd_date._day);
            if (desc) 
            {
                if (termWidth <= 62)
                    doCR();
                mFormat(desc);
            }
            CRfill = NULL;
            doCR();
        }
        else
            mPrintf("%-14s%7ld ", fn->fd_name, fn->fd_size);
        FDSectCount += fn->fd_size;
        mAbort();                       /* chance to next(!)/pause/skip */
    }
    return (outFlag != OUTSKIP);
}


/*
** readdir() read the directory.
*/
static void readdir (char *fname)
{
    int printdir();
    long sectors, bytes;

    FDSectCount = 0L;
    if (FDextended)
        tagSetup();

    doCR();
    wildcard(printdir, strlen(fname) ? fname : "*.*");
    if (FDextended)
        tagClose();

    mPrintf("\n %s total.\n ", plural("byte", FDSectCount));
    if (onConsole || cfg.diskusage) 
    {
        diskSpaceLeft(roomBuf.rbdirname, &sectors, &bytes);
        mPrintf("%s free in %s\n ", plural("byte", bytes),
            roomBuf.rbdirname);
    }

    FDextended = NO;
}



/*
** rwProtocol() Figure out what protocol to use for a r/w command.
*/
int rwProtocol (char *cp)
{
    switch (*cp) 
    {
    case 'V': WC = VANILLA;
        break;
    case 'Y': WC = YMODEM;
        break;
    case 'X': WC = XMODEM;
        break;
    default : return NO;
    }
    mPrintf("\b%s ", protocol[WC]);
    return YES;
}


/*
** initWC() -- set up for a protocol download
*/
int initWC (int mode)
{
    int started = 0;

    if (mode == ASCII)
    {
        return 1;
    }

    if (mode != TODISK)
    {
        mPrintf(dlWaitStr, protocol[mode]);
    }

    usingWCprotocol = mode;

    if (!(started = beginWC()))
    {    
        usingWCprotocol = ASCII;
    }
    return started;
}


/* doRead() - do the R(ead) command. The argument `hack' is for
**            calling doread with special commands like `d', `f',
**            `o', `n', etc.
*/
enum
{
    MESSAGE,       /* .r[nora]  */
    GLOBAL,        /* .rg       */
    STATUS,        /* .rs       */
    DIR,           /* .rd, .re  */
    HEADER,        /* .rh       */
    FILES,         /* .rf, .rb  */
    INVITED        /* .ri       */
};


doRead(prefix, hack, cmd)
char cmd;
{
    int                 i, count;
    int                 whichMess = NEWoNLY;
    int                 revOrder = NO;
    int                 wantuser = NO;
    int                 reading  = MESSAGE;
    char                fname[80];
    struct logBuffer    lbuf;

    mPrintf("\bRead ");

    dFormatted = dPass = singleMsg = batchWC = 0;
    msguser[0] = 0;
    WC = ASCII;

    if (!loggedIn)
    {
        if (!cfg.unlogReadOk) 
        {
            if (thisRoom == MAILROOM)
            {
                showMessages(whichMess, revOrder);
            }
            else
            {
                mPrintf("- [L]og in first!\n ");
            }
            return;
        }
    }

    if (prefix) 
    {
        while (prefix && onLine() && (cmd=toupper(iChar())))
        {
            if (!rwProtocol(&cmd))
            {
                switch (cmd) 
                {
                case 'T':
                    mPrintf("ext ");
                    dFormatted = TRUE;
                    break;

                case 'M':
                    mPrintf("ore ");
                    singleMsg = TRUE;
                    break;

                case 'U':
                    mPrintf("ser ");
                    wantuser = TRUE;
                    break;

                case '+':
                    mPrintf("\bafter ");
                    dPass = dAFTER;
                    break;

                case '-':
                    mPrintf("\bbefore ");
                    dPass = dBEFORE;
                    break;

                case 'C':
                    mPrintf("\b%s ", protocol [WC = CAPTURE] );
                    break;

                case 'J':
                    if (SomeSysop()) 
                    {
                        mPrintf("\b%s ", protocol [ WC = TODISK] );
                        break;
                    }

                default :
                    prefix = NO;    /* break out of while{} loop */
                    break;
                }
            }
        }
    }
    else
    {
        if (hack == 1)      /* if reading [N]ew [O]ld [F]orward [R]everse */
        {
            singleMsg = TRUE;   /* do it one at a time  -b0b- */
        }
        oChar(cmd);
        cmd = toupper(cmd);
    }

    switch (cmd) 
    {
    case 'A':
        mPrintf("ll");
        whichMess = OLDaNDnEW;
        break;

    case 'R':
        mPrintf("everse");
        revOrder = TRUE;
        whichMess = OLDaNDnEW;
        break;

    case 'N':
        mPrintf("ew");
        whichMess = NEWoNLY;
        break;

    case 'O':
        mPrintf("ld Reverse");
        revOrder = TRUE;
        whichMess = OLDoNLY;
        break;

    case 'Q':
        mPrintf("\bG");
    case 'G':
        mPrintf("lobal new-messages");
        whichMess = GLOBALnEW;
        reading = GLOBAL;
        break;

    case '\n':
    case '\r': 
        break;

    case 'E':
        if (fileok()) 
        {
            FDextended = YES;
            mPrintf("xtended d");
        }
    case 'D':
        if (fileok()) 
        {
            mPrintf("irectory ");
            getNormStr("", fname, 80, YES);
            wantuser = NO;
            reading = DIR;
            break;
        }
        return;

    case 'S':
        mPrintf("tatus");
        reading = STATUS;
        dPass = 0;
        wantuser = NO;
        break;

    case 'F':                           /* [F] and (.F) stay..  */
        if (1 == hack) 
        {                               /* but .ra takes the    */
            mPrintf("orward");          /* place of .rf         */
            whichMess = OLDaNDnEW;
            break;
        }
        else if (fileok()) 
        {
            mPrintf("ile: ");
            getNormStr("", fname, 80, YES);
            if (strlen(fname) < 1)
                return;
            wantuser = NO;
            reading = FILES;
            break;
        }
        return;

    case 'B':
        if (fileok()) 
        {
            batchWC = (WC == YMODEM || WC == XMODEM);
            mPrintf("%s file ", batchWC?"atch":"inary");
            getNormStr("", fname, 80, YES);
            if (strlen(fname) < 1)
                return;
            wantuser = NO;
            reading = FILES;
            break;
        }
        return;

    case 'I':
        if (aide && !roomBuf.rbflags.PUBLIC) 
        {
            mPrintf("nvited users");
            wantuser= NO;
            dPass   = 0;
            reading = INVITED;
            break;
        }

    case '?':
        tutorial("readopt.mnu");
        return;

    default:
        whazzit();
        return;

    }

    if (wantuser) 
    {
        getNormStr(" - which user", msguser, NAMESIZE, YES);
        if (msguser[0] == 0)
            goto out;
    }
    else if (reading != DIR && reading != FILES)
    {
        doCR();
    }

    if (!getpdate())
    {
        return;
    }

    if (reading == FILES)
    {
        readfiles(WC, fname);
    }
    else
    {
        if (!initWC(WC))
            return;

        switch (reading) 
        {
        case STATUS: 
            systat();
            break;

        case DIR:
            readdir(fname);
            break;

        case GLOBAL:
            readglobal();
            break;

        case INVITED:
            for (i=0; outFlag == OUTOK && i < cfg.MAXLOGTAB; i++) 
            {
                getLog(&lbuf, i);
                if (lbuf.lbflags.L_INUSE
                    && roomBuf.rbgen == LBGEN(lbuf,thisRoom))
                    mPrintf(" %s\n", lbuf.lbname);
            }
            break;

        default:
            showMessages(whichMess, revOrder);
            break;
    
        }

        if (WC != ASCII)
            endWC();

        usingWCprotocol = ASCII;
    }

out:
    dPass = singleMsg = 0;          /* be paranoid */
    msguser[0] = 0;
}


void doDownload (prefix)
{
    char cmd = 'D';
    char fname[80];
    int whichMess, revOrder;
    int i, count;
    struct logBuffer lbuf;
    int reading = MESSAGE;

    revOrder = batchWC = 0;
    WC = XMODEM;
    whichMess = NEWoNLY;

    mPrintf("ownload ");

    if (!(loggedIn || cfg.unlogReadOk) ) 
    {
        mPrintf("- [L]og in first!\n ");
        return;
    }

    if (prefix) 
    {
        while (prefix && onLine() && (cmd=toupper(iChar())) )
            if (!rwProtocol(&cmd))
                prefix = NO;
    }
    else
    {
        mPrintf("Xmodem F");            /* no prefix implies Xmodem File */
        cmd = 'F';
    }

    switch (cmd) 
    {
    case 'A':
        mPrintf("ll messages in this %s\n", room_area);
        doCR();
        whichMess = OLDaNDnEW;
        reading = MESSAGE;
        break;

    case 'Q':
        mPrintf("\bG");
    case 'G':
        mPrintf("lobal new messages\n");
        doCR();
        whichMess = GLOBALnEW;
        reading = GLOBAL;
        break;

    case 'N':
        mPrintf("ew messages in this %s\n", room_area);
        doCR();
        reading = MESSAGE;
        whichMess = NEWoNLY;
        break;

    case '\n':
    case '\r':
        mPrintf("F");
    case 'F':
        if (fileok()) 
        {
            mPrintf("%silename:", expert? "" : "ile -\n\n Enter F");
            getNormStr("", fname, 80, YES);
            if (strlen(fname) < 1)
                return;
            reading = FILES;
            break;
        }
        return;

    case 'B':
        if (fileok()) 
        {
            batchWC = (WC == YMODEM || WC == XMODEM);
            mPrintf("%s", batchWC?"atch Filespec:":"inary file:");
            getNormStr("", fname, 80, YES);
            if (strlen(fname) < 1)
                return;
            reading = FILES;
            break;
        }
        return;

    case '?':
        tutorial("downopt.hlp");
        return;

    default:
        whazzit();
        return;
    }

    if (reading == FILES)
        readfiles(WC, fname);

    else
    {
        if (!initWC(WC))
            return;

        if (reading == GLOBAL)
            readglobal();
        else
            showMessages(whichMess, revOrder);

        endWC();
        usingWCprotocol = ASCII;
    }
}


listFiles()
{
    char fname[80];

    if (!fileok())
        return;

    mPrintf("ist files\n");
    doCR();
    mPrintf("\n Filespec [*.*]: ");
    getNormStr("", fname, 80, YES);

    FDextended = TRUE;
    readdir(fname);
}


/*
 ********************************************************
 *      systat() prints out current system status       *
 ********************************************************
 */
static void systat (void)
{
    extern char b0bVersion[];
    extern int fossil;
    long size, allowed;
    int i, roomCount;
    char *plural(), *mydate, *day_of_week();
    extern struct clock now;

    for (roomCount = i = 0; i < MAXROOMS; i++)
        if (roomTab[i].rtflags.INUSE)
            roomCount++;

    mPrintf("\n This is %s\n ", &cfg.codeBuf[cfg.nodeTitle]);
    mydate = formDate();
    mPrintf("%s, %s %s\n ", day_of_week(&now), mydate, tod(YES));
    mPrintf("Running b0badel Version %s\n ", b0bVersion);

    if (loggedIn) 
    {
        mPrintf("Logged in as %s\n ", logBuf.lbname);
        mPrintf("Your name is %s\n ", logBuf.lbrealname);
        if (logBuf.lbflags.NET_PRIVS)
            mPrintf("You have net privileges, %s.\n ",
                plural("LD credit", (long)(logBuf.credit)) );
        if (cfg.download) 
        {
            allowed = (cfg.download - logBuf.lblimit) / 1024L;
            if (allowed > 0)
                mPrintf("Your download limit is %ldK\n ", allowed);
            else
                mPrintf("You may not download anymore today.\n ");
        }
    }
    /*
     * suggested by Christopher Robin 13-feb-87
     */
    mPrintf("chat is %sabled.\n ",cfg.noChat?"dis":"en");

    mPrintf("%s, last is %lu,\n ",
        plural("message", cfg.newest-cfg.oldest+1L),
        cfg.newest);
    size = ((long)(cfg.maxMSector) * (long)BLKSIZE) / 1024L;
    mPrintf("%ldK message space,\n ", size);
    mPrintf("%d-entry log.\n ", cfg.MAXLOGTAB);
    mPrintf("%d %s slots, %d in use.\n ", MAXROOMS, room_area, roomCount);
    if (cfg.oldest > 1)
        size = ((long)cfg.maxMSector * (long)BLKSIZE)
                / (long)(cfg.newest - cfg.oldest + 1L);
    else
        size = (((long)cfg.catSector * (long)BLKSIZE) + (long)cfg.catChar)
                / cfg.newest;

    mPrintf("Average message length: %ld\n ",  size);
    if (aide)
    {
        mPrintf("Fossil is %sactive\n", (fossil) ? "" : "NOT ");
    }
    doCR();
}


/*
** alphasort() - directory compare routine fed to qsort()
*/
static int alphasort (struct dirList *s1, struct dirList *s2)
{
    return strcmp(s1->fd_name, s2->fd_name);
}


/*
** wildcard() Do something with the directory *
*/
static void wildcard (int (*fn)(struct dirList *), char *pattern)
{
    int i, count;
    struct dirList *list;
    extern char usingWCprotocol, batchWC;

    if (xchdir(roomBuf.rbdirname)) 
    {
        if (count=scandir(pattern, &list)) 
        {
            qsort(list, count, sizeof list[0], alphasort);
            if (!batchWC || batch(STARTUP, list, count)) 
            {
                for (i=0; i<count && (*fn)(&list[i]); i++)
                    ;
                if (batchWC)
                    batch(~STARTUP, NULL, 0);
            }
            freedir(list, count);
        }
        else if (pattern[0]) 
        {
            usingWCprotocol = ASCII;
            mPrintf("No %s\n ", pattern);
        }
        homeSpace();
    }
    batchWC = NO;
}


/*
** netwildcard() - quietly do something with a pattern
*/
void netwildcard (int (*fn)(struct dirList *), char *pattern)
{
    int i, count;
    struct dirList *list;

    if (count = scandir(pattern, &list)) 
    {
        for (i = 0; i < count && (*fn)(&list[i]); ++i)
            ;
        freedir(list, count);
    }

    if (batchWC)
    {
        batch(~STARTUP, NULL, 0);
    }
    batchWC = NO;
}

