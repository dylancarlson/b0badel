/*{ $Workfile:   event.c  $
 *  $Revision:   1.14  $
 *
 *  code to handle b0badel events
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
 
#define EVENT_C

#include <stdlib.h>
#include "ctdl.h"
#include "externs.h"
#include "event.h"
#include "clock.h"

/* GLOBAL Contents:
**
** minutes()        returns day of week and minutes into that day
** initEvent()      Initialise the event table.
** setUpEvent()     Set up next event.
** doEvent()        Do an event.
*/

/* global data */

struct evt_type *nextEvt;           /* next event to execute */

/* static prototypes and data */

static void rollDay (char *mask);

static char todayMask;      /* bitmask for today    */
static int  thisMinute;     /* minute of the day    */

/* this macro keeps a minutes calculation from exceeding one day */

#define BIND_TIME(x) ((x) % (24 * 60)) 


/*
** minutes() - calculates day of week and minutes into that day
**          based on the contents of the global clock struct "now"
*/
int minutes(int *today)
{
    timeis(&now);
    *today = (int)(julian_date(&now) % 7L);

    return (60 * now.hr) + now.mm;
}


/*
** evtSort() - qsort() routine sorts on evtTime for initEvent()
*/
static int evtSort (struct evt_type *p1, struct evt_type *p2)
{
    int diff = p1->evtTime - p2->evtTime;

    if (diff)
    {
        return diff;
    }

    /* times match, sort days */

    return p1->evtDay - p2->evtDay;
}


/*
** initEvent() - initializes the event table
*/
void initEvent (void)
{

    if (cfg.evtCount > 0) 
    {
        int i, dow;

        thisMinute = minutes(&dow);
        todayMask  = 1 << dow;

        for (i = 0; i < cfg.evtCount; ++i)
        {
            /* set time of "relative" events (TIMEOUTs) */

            if (evtTab[i].evtRel)
            {
                evtTab[i].evtTime = BIND_TIME(thisMinute + evtTab[i].evtRel);
            }
        }
        qsort(evtTab, cfg.evtCount, sizeof(*evtTab), evtSort);
        setUpEvent();
    }
}


/*
** rollDay - handy little function to advance a day bit-mask
*/
static void rollDay (char *mask)
{
    *mask <<= 1;

    if (*mask == 0x80)
    {
        *mask = 0x01;
    }
}

/*
** setUpEvent() - set up timers for the next event
*/
void setUpEvent (void)
{
    int timeNow;
    int today;

    evtRunning  = FALSE;
    evtClock    = upTime();
    timeNow     = minutes(&today);

    if (cfg.evtCount > 0) 
    {
        char dayMask = 0x01 << today;
        int  start, end;
        int  count, i;

        /* search for the next active event */

        for (count = 0; count < 7; ++count, rollDay(&dayMask)) 
        {
            for (i = 0; i < cfg.evtCount; ++i) 
            {
                nextEvt = &evtTab[i];

                if (!(nextEvt->evtDay & dayMask))
                {
                    continue;   /* wrong day */
                }

                start = nextEvt->evtTime;
                end   = BIND_TIME(start + nextEvt->evtLen);

                if (timeNow < start || timeNow < end) 
                {
                    /* found one... */

                    evtClock += (start - timeNow) * 60L;
                    evtRunning = TRUE;
                    forceOut = isPreemptive(nextEvt);

                    if (forceOut) 
                    {
                        warned = FALSE;
                        evtTrig = 300L;     /* 5 minute warning value */
                    }
                    else
                    {
                        evtTrig = 0L;
                    }
                    return;
                }
            }
            timeNow -= 24 * 60; /* checking a day ahead so back off the */
                                /* desired time to yesterday...         */
        }
    }
}


/*
** doEvent() - do an event, if it's time.
*/
void doEvent (void)
{
    eventExit = warned = FALSE;
    exitValue = nextEvt->evtFlags;

    if (evtRunning)
    {
        switch (nextEvt->evtType)
        {
        case EVENT_NETWORK:     /* do a net event */
            if (loggedIn)
            {
                terminate(YES, 'p');
            }
            netmode(nextEvt->evtLen, exitValue);
            break;
        
        case EVENT_ONLINE:      /* bring system up if it isn't already */
            if (!gotcarrier())
            {    
                modemInit();
                onConsole = NO;
            }
            break;

        case EVENT_OFFLINE:     /* kick off modem user, take system offline */
            if (!onConsole)
            {
                if (loggedIn)
                {
                    terminate((char)YES, 'p');
                }
                modemClose();
                onConsole = YES;
                xprintf("\n `CONSOLE' mode\n ");
            }
            break;

        default:
            Abandon = TRUE;     /* flag for exit to DOS */
            return;
        }
        setUpEvent();           /* schedule the next event */
    }
}
