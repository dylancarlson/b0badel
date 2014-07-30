/*{ $Workfile:   modem.c  $
 *  $Revision:   1.12  $
 * 
 *  modem code for b0badel BBS
}*/
/*    Copyright (c) 1989, 1991 by Robert P. Lee 
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

#define MODEM_C

#include <conio.h>      /* for kbhit() */
#include "ctdl.h"
#include "externs.h"
#include "event.h"
#include "audit.h"
#include "protocol.h"

/* GLOBAL Contents:
**
** iChar()          top-level user-input function
** connect()        chat mode
** modIn()          returns a user char
** modemInit()      top-level initialize-all-modem-stuff
** ringSysop()      signal chat-mode request
*/


#define CONTROL_R   0x12
#define CONTROL_T   0x14
#define CONTROL_X   0x18
#define CONTROL_Y   0x19
#define SLEEPING    240L    /* user must be sleeping... 4 minute time out */
#define RING_LIMIT  4       /* number of times we ring the sysop */

/*
** iChar() is the top-level user-input function -- this is the
**         function the rest of Citadel uses to obtain user input
*/
unsigned iChar (void)
{
    unsigned c;

    if (justLostCarrier)
    {
        return 0;           /* ugly patch   */
    }

    c = cfg.filter [modIn() & 0x7f];

    if (c == TAB)
    {
        return c;           /* we don't echo TABs here */
    }
    else
    {
        oChar((char)c);     /* sensitive to echo mode, adjusts crtColumn */
    }

    return c;
}


/*
** connect() - talk straight to the modem
*/
void connect (int line_echo, int mapCR, int local_echo)
{
    register char c;
    register FILE *capture = NULL;
    pathbuf chatFile;

    xprintf("^X to exit\n");
    ctdlfile(chatFile, cfg.auditdir, "chat.rec");
    modemOpen();

    while (1) 
    {
        if (mdhit() && (c = getraw())) 
        {
            if (capture)                /* we got it -- we save it?     */
            {
                putc(c, capture);
            }

            if (line_echo)              /* echo to the modem? */
            {
                if ((c == '\n' || c == '\r') && mapCR) 
                {
                    doCR();
                }
                else
                {   
                    conout(c);
                    modout(c);
                }
            }
            else
            {
                conout(c);
            }
        }

        if (kbhit()) 
        {
            c = getch() & 0x00ff;
#if 0
            if (c == ESC)   /* local <ESC> exits */
            {
                pause(5);           /* .05 second delay */

                if (kbhit())        /* faster than humanly possible */
                {
                    modout(ESC);    /* must be ANSI.SYS generating it */
                    c = getch();
                }
                else 
                {
                    break;          /* console user hit <ESC> */
                }
            }
#endif
            if (c == CONTROL_X)     /* local ^X exits */
            {
                break;
            }
            else if (c == CONTROL_R)    /* ^R toggles capture mode */
            {
                if (capture != NULL) 
                {
                    puts("\n (CAPTURE OFF)");
                    fclose(capture);
                    capture = NULL;
                }
                else if (capture = safeopen(chatFile, "ab"))
                {
                    puts("\n (CAPTURE ON)");
                }
                continue;
            }

            modout(c);

            if (c == '\r') 
            {
                c = '\n';
                if (mapCR)
                {
                    modout(c);
                }
            }

            if (local_echo) 
            {
                conout(c);

                if (capture)
                {
                    putc(c, capture);
                }
            }
        }
    }

    if (capture != NULL)
    {
        fclose(capture);
    }

    if (dropDTR && !gotcarrier())
    {
        modemClose();
    }
}


/*
** modemInit() - is responsible for all modem-related initialization
*/
void modemInit (void)
{
    rawmodeminit();
    haveCarrier = modStat = gotcarrier();
}


/*
** modIn() toplevel modem-input function
**   If DCD status has changed since the last access, reports
**      carrier present or absent and sets flags as appropriate.
**   In case of a carrier loss, waits 20 ticks and rechecks
**      carrier to make sure it was not a temporary glitch.
**   If carrier lost returns 0 (HUP).
**   If carrier is newly received, returns newCarrier = YES.
**   If carrier is present and state has not changed,
**      gets a character if present and returns it.
*/
int modIn (void)
{
    register char c;
    register long ts;
    register char signal = NO;

    startTimer();

    while(1) 
    {
        if ((!onConsole) && (c = gotcarrier()) != modStat) 
        {
            /* carrier changed   */
            if (c)  
            {   
                xprintf("Carrier detected\n");      /* carrier present   */

                if (findBaud()) 
                {
                    warned = justLostCarrier = NO;
                    haveCarrier = newCarrier = YES;
                    modStat = c;
                    logMessage(BAUD, "", NO);
                }
                else
                {
                    hangup();
                }

                return HUP;
            }
            else
            {
                pause(50);                      /* confirm it's not a glitch */
                if (!gotcarrier()) 
                {
                    xprintf("Carrier lost\n");
                    hangup();
                    haveCarrier = modStat = NO;
                    justLostCarrier = YES;

                    return HUP;     /* HUP is 0, by the way */
                }
            }
        }

        if (!onConsole && mdhit() && haveCarrier)
        {
            return getMod();    /* return the character */
        }

        if (kbhit())      /* console user hit a key */
        {
            c = getch();

            if (onConsole)
            {
                return c;
            }

            switch (c & 0x7f)
            {
            case ESC:       /* switch to console mode */

                xprintf("`CONSOLE' mode\n ");
                onConsole = YES;
                warned = NO;
                if (dropDTR && !gotcarrier())
                {
                    modemClose();
                }
                return HUP;

            case CONTROL_Y:         /* break in to chat */

                mPrintf("\n --- Entering sysop chat mode --- ");
                doCR();
                connect(YES,YES,YES);

                if (!gotcarrier())
                {
                    terminate(YES, 'p');
                    hangup();
                    haveCarrier = modStat = NO;
                    justLostCarrier = YES;

                    return HUP;     /* HUP is 0, by the way */
                }
                else
                {
                    mPrintf("\n --- Leaving sysop chat mode --- ");
                    doCR();
                }

                startTimer();       /* it's been a while! */
                break;
            }
        }

        if (evtRunning && timeLeft(evtClock) < evtTrig) 
        {
            if (haveCarrier || onConsole) 
            {
                if (!forceOut)
                {
                    continue;
                }

                if (warned)
                {
                    outFlag = IMPERVIOUS;

                    mPrintf("\n System event: %s\n ", nextEvt->evtMsg);

                    if (!(onConsole && nextEvt->evtType == EVENT_OFFLINE))
                    {
                        mPrintf("Bye!\n ");
                        terminate(YES, 'p');
                    }
                    outFlag = OUTOK;
                }
                else 
                {
                    if (!(onConsole && nextEvt->evtType == EVENT_OFFLINE))
                    {
                        outFlag = IMPERVIOUS;
                        mPrintf("\n WARNING: A system event will terminate"
                            " your session in %d minutes.\n ",
                            timeLeft(evtClock)/60);

                        outFlag = OUTOK;
                    }
                    warned = YES;
                    evtTrig = 0L;     /* reset trigger to immediate dropout */
                    continue;
                }
            }
            /* nobody logged in, so wait for the end of the line */
            else if (timeLeft(evtClock) > 0)
            {
                continue;
            }
            eventExit = YES;
            return HUP;
        }

        /*
         * check for no input.  (Short-circuit evaluation, remember!)
         */

        if (!onConsole) 
        {
            ts = chkTimeSince();

            if (haveCarrier) 
            {
                if (ts >= SLEEPING) 
                {
                    mPrintf("Sleeping? Call again :-)");
                    terminate(YES, 't');
                    hangup();
                    haveCarrier = modStat = NO;
                    justLostCarrier = YES;

                    return HUP;     /* HUP is 0, by the way */
                }
                else if (!signal && ts >= SLEEPING - 15L) 
                {
                    oChar(BELL);
                    signal = YES;
                }
            }
            else if (ts > cfg.poll_delay)
            {
                if (checkpolling())
                {
                    return HUP;
                }
                else
                {
                    startTimer();
                }
            }
        }
    }
}


/*
** ringSysop() -- Signals a chat mode request.
*/
void ringSysop (void)
{
    int i, ring;

    mPrintf("\n Ringing sysop.\n ");

    for (i = ring = 0; ring < RING_LIMIT && !(mdhit() || kbhit()); ) 
    {
        putchar(BELL);
        modout(BELL);

        pause(0xff & cfg.shave[i]);

        if (++i >= 7) 
        {
            i = 0;
            ++ring;
        }

        if (!gotcarrier())
        {
            return;
        }
    }

    if (kbhit()) 
    {
        getch();
        onConsole = YES;
        mPrintf("\n -- The SYSOP is here to CHAT --\n ");
        connect(YES, YES, YES);
        onConsole = NO;
    }
    else if (ring >= RING_LIMIT) 
    {
        cfg.noChat = YES;       /* don't allow further requests */
        mPrintf("Sorry, Sysop not around...\n ");
    }
    else
    {
        getraw();
    }
}

