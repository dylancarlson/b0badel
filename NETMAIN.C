/*{ $Workfile:   netmain.c  $
 *  $Revision:   1.13  $
 * 
 *  Citadel network driver
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

#define NETMAIN_C

#include <io.h>
#include <conio.h>
#include <string.h>
#include <stdlib.h>

#include "ctdl.h"
#include "ctdlnet.h"
#include "clock.h"
#include "event.h"
#include "protocol.h"
#include "poll.h"
#include "externs.h"

/*  GLOBAL Contents:
**
**  increment()         copy a byte into sectBuf
**  check_for_init()    Looks for networking init sequence
**  readMail()          Integrates mail into the data base
**  inMail()            Integrates mail into database
**  netmode()           Handles the net stuff
**  checkpolling()      Try to poll all the active #polling systems
**  pollnet()           do the actual polling
**  callsys()           dial net node n
**  netAck()            respond to a 7-13-69 network init sequence
*/

static int  noKill;
static int  errCount = 0;
static long netFin;            /* when the whole mess is done with    */

/* prototypes for static functions */

static int  getNetBaud  (void);
static void netDelay    (void);
static void openNet     (void);
static void closeNet    (void);
static void netSetTime  (int length);
static int  netTimeLeft (void);
static int  callout     (int i);
static int  dialer      (int i, int abort);
static void nmcalled    (void);


#define inthisnet(x,what)   \
    ( netTab[x].ntflags.in_use      \
  && !netTab[x].ntflags.rec_only    \
  && (netTab[x].ntflags.what_net & (1L << (what)) ) )

#define mustpoll(x,day)     \
    ( netTab[x].ntflags.poll_day & (1 << (day)) )

#define needtocall(i)       \
   ( netTab[i].ntflags.mailpending \
  || netTab[i].ntflags.filepending \
  || netmesg(i))


/*
** increment()
*/
int increment (int c)
{
    if (counter > (SECTSIZE + 2)) 
    {
        hangup();
        return NO;
    }
    sectBuf[counter++] = (char)c;
    return YES;
}


/*
** check_for_init() Looks for networking initialization sequence
**      loosened up a bit for STadel 3.2a -- a CR will automatically
**      drop into state 2 no matter what the previous character was.
*/
int check_for_init (void)
{
    int count = 100;
    int ch;
    int seq = 0;

    while (count--)
    {
        if (mdhit()) 
        {
            ch = getraw();

            if (netDebug)
            {
                splitF(netLog, "%d.%d ", ch, seq);
            }

            switch (ch) 
            {
            case 07:
                seq = 1;        /* \a */
                break;

            case 13:
                seq = 2;        /* \r */
                break;

            case 69:
                if (seq == 2)   /* got all three */
                {
                    return YES;
                }

            default:
                seq = 0;        /* restart */
                break;
            }
        }
        else
        {
            pause(1);
        }
    }
    return NO;
}


/*
** readMail() Integrates mail into the msgbase
*/
void readMail (char zap, void (*mailer)(void))
{
    pathbuf temp, tmp2;
    extern char netmlspool[];

    ctdlfile(temp, cfg.netdir, netmlspool);

    if (spool = safeopen(temp, "rb")) 
    {
        getRoom(MAILROOM);
        noKill = NO;
        while (getspool())
        {
            if (stricmp(&cfg.codeBuf[cfg.nodeId], msgBuf.mborig) != 0)
            {
                (*mailer)();
            }
        }

        fclose(spool);
        if (zap)
        {
            if (noKill) 
            {
                ctdlfile(tmp2, cfg.netdir, "temp%d.$$$", errCount++);
                drename(temp, tmp2);
            }
            else
            {
                dunlink(temp);
            }
        }
    }
    else
    {
        neterror(YES, "No tempmail file");
    }
}


/*
** inMail() integrates mail into database
*/
void inMail (void)
{
    NA to;

    if (msgBuf.mbto[0]) 
    {
        strcpy(to, msgBuf.mbto);        /* postmail burns to: field */
        splitF(netLog, "%s mail to `%s'\n",
            postmail(YES) ? "Delivering" : "Cannot deliver", to);
    }
    else
    {
        splitF(netLog, "No recipient!\n");
        noKill = YES;
    }
}


/*
** netDelay()   Twiddle our thumbs for 30 seconds
*/
static void netDelay (void)
{
    long time = 30L;            /* this used to be a random number! */

    startTimer();

    while ((timeLeft(netFin) > 0)
      && (chkTimeSince() < time)
      && !(kbhit() || gotcarrier()) )
        ;

    if(kbhit()) 
    {
        int c = getch();

        xprintf("%s left.\n", plural("minute", (long)netTimeLeft()) );

        if (c == 'Q' && conGetYesNo("Quit b0badel?")) 
        {
            Abandon = YES;
            exitValue = SYSOP_EXIT;
        }

        while (kbhit())               /* gobble type-ahead */
        {
            getch();
        }
    }
}


/*
** openNet()    set up networker stuff
*/
static void openNet (void)
{
    pathbuf name;

    rmtname[0] = 0;

    if (logNetResults) 
    {
        ctdlfile(name, cfg.auditdir, "netlog.sys");
        if ((netLog = safeopen(name, "a")) == NULL)
        {
            neterror(NO, "Couldn't open %s", name);
        }
    }
    else
    {
        netLog = NULL;
    }
    inNet = YES;
    loggedIn = NO;
}


/*
** netmode() - runs a network event
*/
void netmode (int length, int whichnet)
{
    int tmp, x, dow;
    int first, start = 0;
    pathbuf name;
    char *polled = xmalloc(cfg.netSize);
    char *needto = xmalloc(cfg.netSize);
    char *missed = xmalloc(cfg.netSize);

    openNet();
    netSetTime(length);

    splitF(netLog,
        "\n-------Networking Mode (net %d) for ~%s--------\n%s\n",
        whichnet, plural("minute", (long)netTimeLeft()), formDate());

    modemOpen();
    dow = julian_date(&now) % 7;

    /* as a general rule, call l-d systems once a night,
    ** local systems 5 times a night MAXIMUM.
    */
    for (x = 0; x < cfg.netSize; ++x) 
    {
        if (inthisnet(x, whichnet)) 
        {
            polled[x] = netTab[x].ntflags.ld ? netTab[x].ntflags.ld : 5;
            needto[x] = mustpoll(x, dow);
            missed[x] = needtocall(x) || needto[x];
        }
        else
        {
            missed[x] = polled[x] = needto[x] = 0;
        }
    }

    while (timeLeft(netFin) > 0) 
    {
        netDelay();

        if (Abandon)
        {
            break;
        }

        if (gotcarrier())
        {
            nmcalled();
        }

        if (cfg.netSize != 0) 
        {
            first = start;
            do 
            {
                x = start;
                start = (1 + start) % cfg.netSize;

                if (polled[x] && (needto[x] || needtocall(x))) 
                {
                    if (callout(x)) 
                    {
                        --polled[x];
                        if (caller() != NOT_STABILISED)
                        {
                            needto[x] = missed[x] = 0;
                        }
                    }
                    break;
                }
            } while (start != first)
                ;
        }
    }

    for (msgBuf.mbtext[0] = 0, x = 0, first = YES; x < cfg.netSize; ++x)
    {
        if (missed[x]) 
        {
            if (first)
            {
                first = NO;
            }
            else
            {
                strcat(msgBuf.mbtext,", ");
            }
            getNet(x);
            strcat(msgBuf.mbtext, netBuf.netName);
        }
    }

    if (!first) 
    {
        rmtname[0] = 0;
        neterror(NO, "Couldn't reach %s", msgBuf.mbtext);
    }

    free(polled);
    free(needto);
    free(missed);

    closeNet();
}
/* end of netmode() */


/*
** checkpolling()  Try to poll all the active #polling systems
*/
int checkpolling (void)
{
    int timenow, active, day, j, count, which;
    int idx;

    timenow = minutes(&day);

    for (idx = 0; idx < cfg.poll_count; ++idx) 
    {
        if (poll_today(pollTab[idx].p_days, day)) 
        {
            int after_start = (timenow > pollTab[idx].p_start);
            int before_end  = (timenow < pollTab[idx].p_end);

            if (pollTab[idx].p_start > pollTab[idx].p_end)
            {
                active = after_start || before_end;
            }
            else
            {
                active = after_start && before_end;
            }

            if (active) 
            {
                which = pollTab[idx].p_net;

                for (count = j = 0; j < cfg.netSize; ++j)
                {
                    if (inthisnet(j, which) && needtocall(j))
                    {
                        ++count;
                    }
                }
                if (count)
                {
                    pollnet(which);
                }
            }
        }
    }
    return NO;
}


/*
** pollnet() - do the actual polling
*/
int pollnet (int which)
{
    int j;

    if (loggedIn)
    {
        terminate(YES, 'P');
    }
    openNet();
    modemOpen();

    splitF(netLog,
        "\n --Polling systems in net %d--\n%s\n", which, formDate());

    for (j = 0; j < cfg.netSize; ++j) 
    {
        if (inthisnet(j, which) && needtocall(j)) 
        {
            if (callout(j))
            {
                caller();
            }
        }
    }
    closeNet();
    return YES;
}


/*
** closeNet()   restore citadel to normal
*/
static void closeNet (void)
{
    usingWCprotocol = ASCII;

    if (log) 
    {
        fclose(log);
        log = NULL;
        heldMessage = NO;

        if (IngestLogfile()) 
        {
            strcpy(msgBuf.mbtext, tempMess.mbtext);
            aideMessage(NO);
        }
    }

    heldMessage = haveCarrier = modStat = inNet = NO;
    if (netLog)
    {
        fclose(netLog);
    }
}


/*
** netSetTime() Start the net timer
*/
static void netSetTime (int length)
{
    int delayed;
    extern struct evt_type *nextEvt;

    timeis(&now);

    delayed = ((now.hr * 60) + now.mm) - nextEvt->evtTime;

    /* allow 30 seconds slop time */

    netFin = upTime() + ((length - delayed) * 60L) + 30L;
}


/*
** netTimeLeft()
*/
static int netTimeLeft (void)
{
    return (int)(timeLeft(netFin) / 60L);
}


#define MIN(x,y)  ((x) < (y) ? (x) : (y))

/*
** callsys() Dial net node #i
*/
int callsys (int i)
{
    char  call[80];
    char  dialprg[80];
    int   baudrate;

    rmtslot = i;
    getNet(i);
    normID(netBuf.netId, rmt_id);               /* Cosmetics */
    strcpy(rmtname, netBuf.netName);

    splitF(netLog, "(%s) Calling %s\n ", tod(NO), netBuf.netName);

    setBaud(MIN(netBuf.baudCode, cfg.sysBaud)); /* set up baudrate */
    mflush();

    call[0] = 0;

    if (strlen(netBuf.access) > 0)
    {
        strcat(call, netBuf.access);
    }
    else if (netBuf.nbflags.ld || !cfg.usa) 
    {
        if (cfg.usa)
        {
            strcat(call, "1");
        }
        strcat(call, &rmt_id[2]);
    }
    else
    {
        strcat(call, &rmt_id[5]);
    }

    if (netBuf.nbflags.dialer) 
    {
        ctdlfile(dialprg, cfg.netdir, "dial_%d.exe", 0xff & netBuf.nbflags.dialer);
        return dosexec(dialprg, call);
    }

    modputs(&cfg.codeBuf[cfg.dialPrefix]);
    modputs(call);
    modputs(&cfg.codeBuf[cfg.dialSuffix]);
    return 0;
}


/*
** callout() calls some other system
*/
static int callout (int i)
{
    if (dialer(i, NO)) 
    {
        splitF(netLog, "No luck.\n");
        return NO;
    }
    return YES;
}


/*
** dialer() - higher level dial routine, calls callsys()
**            Returns 0 on connect.
*/
static int dialer (int i, int abort)
{
    if (!callsys(i)) 
    {
        long tick = (long)(netBuf.nbflags.ld
            ? cfg.ld_time
            : cfg.local_time);

        for (startTimer(); chkTimeSince() < tick; ) 
        {
            if (gotcarrier())
            {
                return 0;
            }
            else if (abort && kbhit()) 
            {
                getch();
                hangup();
                return 2;
            }
        }
    }
    hangup();
    return 1;
}


/*
** OutOfNet() talk to a network caller when not networking
*/
void OutOfNet (void)
{
    openNet();
    splitF(netLog, "\n----Network caller----\n%s\n", tod(NO));
    called();
    closeNet();
}


/*
** netAck() respond to a 7-13-69 network init sequence
*/
int netAck (void)
{
    register int c = 0, click;

    if (gotcarrier()) 
    {
        modout(~7 );
        modout(~13);
        modout(~69);
        /*
         * wait for an ACK to show up...
         */
        for (click=0; click < 200; click++) 
        {
            if (mdhit()) 
            {           /* got a character? */
                c = getraw();
                if (netDebug)
                    splitF(netLog, "<%d>", c);
                if (c == ACK) 
                {
                    if (netDebug)
                        splitF(netLog, "-ACK\n");
                    return YES;
                }
            }
            pause(1);
        }
        if (netDebug)
        {
            splitF(netLog, "-NAK\n");
        }
    }
    return NO;
}


/*
** getNetBaud()  gets baud of network caller
*/
static int getNetBaud (void)
{
    int retry = 15, baud, HOLD, f, valid;
    char laterMessage[100];

    if (cfg.search_baud)
    {
        HOLD = cfg.modemCC && (mmesgbaud() != ERROR);
    }
    else
    {
        setBaud(cfg.sysBaud);
        HOLD = YES;
    }

    pause(100);         /* Pause a full second */
    while (retry-- > 0 && gotcarrier()) 
    {
        mflush();

        if (HOLD) 
        {
            if (netDebug)
                splitF(netLog, "-HOLD\n");
            valid = check_for_init();
        }
        else
        {
            for (baud=cfg.sysBaud; baud >= 0 && gotcarrier(); --baud) 
            {
                setBaud(baud);
                if (netDebug)
                    splitF(netLog, "-%d\n", baud);
                /*
                 * if we pulled in a valid init sequence, slap into
                 * net mode.
                 */
                if (valid = check_for_init()) 
                {
                    HOLD = YES;
                    break;
                }
            }
        }

        if (valid && netAck())
        {
            return YES;
        }
    }

    /*** lost carrier or timed out... ***/

    if (gotcarrier()) 
    {
        sprintf(laterMessage,
            "The system will be networking for another %d minutes.\r\n",
            netTimeLeft());

        if (HOLD)
        {
            modputs(laterMessage);
        }
        else for (baud = cfg.sysBaud; baud >= 0; baud--) 
        {
            setBaud(baud);
            modputs(laterMessage);
        }
        hangup();
    }
    return NO;
}


/*
** nmcalled() - deal with a caller from network mode
*/
static void nmcalled (void)
{
    splitF(netLog, "Caller detected (%s)\n", tod(NO));

    if (getNetBaud() && gotcarrier())
    {
        called();
    }
    else
    {
        splitF(netLog, "Call not stabilized\n");
    }
}



