/*{ $Workfile:   sysop.c  $
 *  $Revision:   1.13  $
 * 
 *  b0badel's sysop menu
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
#include "event.h"
#include "audit.h"
#include "protocol.h"
#include "door.h"
#include "poll.h"
#include "externs.h"

/* GLOBAL contents:
**
** doSysop()  The ^L sysop menu
** showdays() shows days of the week from a mask
*/

extern struct evt_type   *nextEvt;  /* next event                                       */
extern struct doorway    *shell;
extern struct poll_t     *pollTab;


/*
** showdays() - print a day_of_week mask
*/
void showdays (char mask)
{
    int j;

    for (j = 0; j < 7; ++j)
    {
        if (mask & (1 << j))
        {
            mPrintf(" %s", _day[j]);
        }
    }
}


/*
** doSysop() - Handles the sysop-only menu, returns a boolean.
**             If return value is YES, system will do a whazzit()
*/
int doSysop (void)
{
    char    *day_of_week();
    long    oldest;
    label   who;
    int     size, count, callcode;
    int     i, j;
    int     netNo;
    long    timeTil;
    char    list[80];

    static char *on  = "on";
    static char *off = "off";

    /*** check system password if not on console ***/

    if (!(onConsole || remoteSysop)) 
    {
        if (!aide || strlen(cfg.sysPassword) == 0)
        {
            return YES;
        }

        getNormStr("Sysop password", list, 78, NO);

        if (strcmp(list, cfg.sysPassword))
        {
            return YES;
        }
        remoteSysop = YES;
    }

    while (onLine()) 
    {
        outFlag = OUTOK;
        mPrintf("\n sysop cmd: ");

        switch (toupper(getnoecho())) 
        {
        case 'B':
            setBaud((int)getNumber("Baudrate", 0l, (long)(NUMBAUDS-1)));
            break;

        case 'F':
            getNormStr("File grab", list, 78, YES);
            if (strlen(list) && !ingestFile(list))
            {
                mPrintf("No %s", list);
            }
            break;

        case 'C':
            cfg.noChat = !cfg.noChat;
            mPrintf("Chat %s", cfg.noChat ? off : on);
            break;

        case 'D':
            Debug = !Debug;
            mPrintf("Debug %s", Debug ? on : off);
            break;

        case 'I':
            mPrintf("\n b0badel %s", b0bVersion);
            mPrintf("\n nodeId: %s", &cfg.codeBuf[cfg.nodeId]);
            mPrintf("\n nodeName: %s", &cfg.codeBuf[cfg.nodeName]);
            mPrintf("\n nodeTitle: %s", &cfg.codeBuf[cfg.nodeTitle]);
            mPrintf("\n Chat %s, Debug %s, Fossil %s.",
                cfg.noChat ? off : on,
                Debug      ? on  : off,
                fossil     ? on  : off);
            break;

        case 'M':
            mPrintf("System now on MODEM\n ");
            if (onConsole) 
            {
                onConsole = NO;
                modemOpen();
            }
            return NO;

        case 'O':
            if (shell) 
            {
                doCR();
                rundoor(shell,NULL);
            }
            else
            {
                mPrintf("No shell defined!");
            }
            break;

        case 'T':
            if ((netNo = getSysName("Telephone", who)) != ERROR) 
            {
                modemOpen();           /* make sure we can call out... */
                callsys(netNo);        /* ... call out        */
                connect(NO, NO, NO);   /* ... connect        */
            }
            break;

        case 'S':
            oChar('S');
            setclock();
            break;

        case 'X':                                /* normal exit */
            mPrintf("Exit sysop menu.\n ");
            return NO;

        case 'Q':
            mPrintf("Exit Citadel- ");
            if (!getNo(confirm))
            {
                break;
            }
            Abandon = YES;
            exitValue = remoteSysop ? REMOTE_EXIT : SYSOP_EXIT;
            return NO;

        case 'N':
            netStuff();
            break;

        case 'R':
            mPrintf("Reinitialize modem");
            hangup();
            if (dropDTR)
            {
                modemClose();
            }
            break;

        case 'U':
            mPrintf("User log editor\n ");
            editUser();
            break;

        case 'Y':
            if (evtRunning) 
            {
                long hr, min, sec, dy;

                timeTil = timeLeft(evtClock);
                sec     = timeTil % 60L;
                timeTil /= 60L;
                min     = timeTil % 60L;
                timeTil /= 60L;
                hr      = timeTil % 24L;
                dy      = timeTil / 24L;

                mPrintf("Next event: `%s', triggers in ", nextEvt->evtMsg);
                if (dy)
                {
                    mPrintf("%s, ", plural("day", dy));
                }
                if (hr)
                {
                    mPrintf("%s, ", plural("hour", hr));
                }
                if (min)
                {
                    mPrintf("%s, ", plural("minute", min));
                }
                mPrintf("%s\n ", plural("second", sec));
            }
            else
            {
                mPrintf("No scheduled events.\n ");
            }

            for (i = 0; i < cfg.evtCount; ++i) 
            {
                mPrintf("%2d: %-19s (%02d) @ %d:%02d", i,
                    evtTab[i].evtMsg,
                    evtTab[i].evtFlags,
                    evtTab[i].evtTime/60, evtTab[i].evtTime%60);
                /*
                 * print out what days this event will be active in...
                 */
                if (evtTab[i].evtDay != 0x7f)
                    showdays(evtTab[i].evtDay);
                mCR();
            }
            mCR();
            for (i = 0; i < cfg.poll_count; ++i) 
            {
                mPrintf("poll %02d from %d:%02d to %d:%02d ",
                    pollTab[i].p_net,
                    pollTab[i].p_start/60, pollTab[i].p_start%60,
                    pollTab[i].p_end/60,   pollTab[i].p_end%60);
                /*
                 * print out what days this event will be active in...
                 */
                if (pollTab[i].p_days != 0x7f)
                    showdays(pollTab[i].p_days);
                mCR();
            }
            break;

        case '?':
            tutorial("ctdlopt.mnu");
            break;

        default:
            whazzit();

        }   /* end of getnoecho() switch */

    }       /* end of while(online()) */

    return NO;
}


