/*{ $Workfile:   msg.c  $
 *  $Revision:   1.15  $
 *
 *  Message handling for b0badel bulletin board system
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
 
#include <stdlib.h>
#include <string.h>
#include "ctdl.h"
#include "externs.h"
#include "ctdlnet.h"
#include "msg.h"
#include "protocol.h"
#include "dateread.h"

/*
**   mAbort()            Did someone [N]/[P]/[S]?
**   mFormat()           formats a string to modem and console 
**   fakeFullCase()      Fix up an uppercase only message.
**   netMessage()        Enter a net message.
**   makenetmessage()    Make a message a netmessage.
**   hldMessage()        Handles held messages.
**   makeMessage()       Menu-level routine to enter a message
**   replymsg()          reply to a mail or normal message
**   canreplyto()        can this user reply to this message?
**   showMessages()      print a roomful of msgs.
*/

#define BACKUP    -1       /* flag to read previous msg */

/* static data */

static char aideMsgFlag     = NO;   /* true to go to aide message menu  */
static char killMsgFlag     = NO;   /* true to kill the current message */
static char journalMessage  = NO;   /* true to journal current msg      */
static char lptMessage      = NO;   /* true to print current msg        */


/* prototypes for static functions */

static int  getrecipient     (struct logBuffer *logptr, int *logindex);
static int  procmessage      (char WCmode);
static int  replyMessage     (void);
static int  copymessage      (int m, char mode);
static int  aideMessageFuncs (int m);
static int  promptmsg        (int msgLoc, long msgNo);
static int  aideMsgMenu      (void);
static void msgToPrinter     (int m);


/*
** mAbort() - returns YES if the user has aborted typeout
*/
int mAbort(void)
{
    register int c;
    int i;

    /* Check for abort/pause from user */

    if (outFlag == IMPERVIOUS)
    {
        return NO;
    }
    else if ((onConsole && kbhit()) || (haveCarrier && mdhit())) 
    {
        c = modIn() & 0x7f;

        if (watchForNet && c == 07  /* Citadel  */
            && receive(1)    == 13  /* network  */
            && receive(1)    == 69) /* sequence */
        {
            watchForNet = NO;      /* rule out nasty recursion...       */

            /*** Try 15 times to get a network acknowledgement. ***/

            for (i = 15; gotcarrier() && i; --i) 
            {
                if (check_for_init() && netAck()) 
                {
                    OutOfNet();
                    outFlag = OUTSKIP;
                    return YES;
                }
                mflush();
            }
            watchForNet = YES;
        }

        c = cfg.filter[c];
        c = toupper(c);

        switch (c)
        {
        case ' ':                       /* pause on spacebar */
        case XOFF:
        case 'P':
            if (justLostCarrier)
            {
                c = HUP;
                break;
            }

            c = modIn() & 0x7f;
            c = cfg.filter[c];
            c = toupper(c);

        case 'K':
        case '.':
            if (aide)
            {
                if (c == '.')
                {
                    aideMsgFlag = YES;
                    return YES;
                }
                else if (c == 'K' && thisRoom != AIDEROOM)
                {
                    killMsgFlag = YES;
                    return YES;
                }
            }
            break;

        case 'J':                   /* J) for Jump paragraph */
            outFlag = OUTPARAGRAPH;
            break;

        case 'N':                   /* N) for Next */
            outFlag = OUTNEXT;
            return YES;
            break;

        case 'Q':                   /* Q) for Quit  (synonym) */
        case 'X':                   /* X) for eXit  (synonym) */
        case 'S':                   /* S) for Stop            */
            outFlag = OUTSKIP;
            return YES;

        default:
            break;
        }
    }
    else if (haveCarrier && !gotcarrier())
    {
        modIn();            /* Let modIn() report the problem       */
    }

    return NO;
}


/*
** mFormat() - formats a string to modem and console
*/
void mFormat(char *string)
{
    char wordBuf[MAXWORD];
    int  i = 0;

    while (string[i]
        && (outFlag==OUTOK || outFlag==IMPERVIOUS || outFlag==OUTPARAGRAPH)) 
    {
        i = sgetword(wordBuf, string, i, MAXWORD-1);
        softWord(wordBuf);

        if (mAbort())
        {
            return;
        }
    }
}

/*
** hFormat() - formats a string with hard CR's
*/
void hFormat (char *string)
{
    char wordBuf[MAXWORD];
    int  i = 0;

    while (string[i])
    {
        switch (outFlag)
        {
        case OUTOK:
        case IMPERVIOUS:
        case OUTPARAGRAPH:
            i = sgetword(wordBuf, string, i, MAXWORD - 1);

            outword(wordBuf);

            if (mAbort())
            {
        default:
                return;
            }
        }
    }
}


/*
** fakeFullCase() - convert an all-caps string to upper & lower
*/
void fakeFullCase(char *text)
{
    char *c;
    char lastWasPeriod;
    int  state;
    enum STATES
    {
        NUTHIN, FIRSTBLANK, BLANKI
    };

    /* We assume an imaginary period preceding the text.*/
    for (lastWasPeriod = YES, c=text; *c; c++) 
    {
        if (*c != '.' && *c != '?' && *c != '!') 
        {
            if (isalpha(*c)) 
            {
                if (lastWasPeriod)
                {
                    *c = (char)toupper(*c);
                    lastWasPeriod = NO;
                }
                else
                {
                    *c = (char)tolower(*c);
                }
            }
        }
        else
        {
            lastWasPeriod = YES;
        }
    }

    /* little state machine to search for ' i ': */

    for (state = NUTHIN, c=text; *c; c++) 
    {
        switch (state) 
        {
        case NUTHIN:
            state = isspace(*c) ? FIRSTBLANK : NUTHIN;
            break;

        case FIRSTBLANK:
            state = (*c == 'i') ? BLANKI : NUTHIN;
            break;

        case BLANKI:
            state = isspace(*c) ? FIRSTBLANK : NUTHIN;

            if (!isalpha(*c))   /* trap space, apostrophy, etc. */
            {
                *(c-1)  = 'I';
            }
            break;
        }
    }
}


/*
** getrecipient() - Find out who to send a message to.
*/
static int getrecipient(struct logBuffer *logptr, int *logindex)
{
    label node;

    *logindex = ERROR;

    if (thisRoom != MAILROOM)  
    {
        msgBuf.mbto[0] = 0;         /* Zero recipient             */
        mCR();
        return YES;
    }

    if (!msgBuf.mbto[0]) 
    {
        if ((cfg.noMail && !aide) || !loggedIn) 
        {
            mPrintf("- private mail to `sysop'\n ");
            strcpy(msgBuf.mbto, "Sysop");
            return YES;
        }
        getNormStr("\n To", msgBuf.mbto, ADDRSIZE, YES);
        if (strlen(msgBuf.mbto) < 1)
        {
            return NO;
        }
    }

    if (parsepath(msgBuf.mbto, node, NO))
    {
        return verifynettable(node);
    }
    else if (stricmp(msgBuf.mbto, program) == 0) 
    {
        if (!aide) 
        {
            mPrintf("Only AIDES may mail to `%s'\n ", program);
            return NO;
        }
    }
    else if ((*logindex = getnmlog(msgBuf.mbto, logptr)) != ERROR)
    {
        strcpy(msgBuf.mbto, logptr->lbname);
    }
    else if (stricmp(msgBuf.mbto, "Sysop") != 0) 
    {
        mPrintf("No `%s' known\n ", msgBuf.mbto);
        return NO;
    }
    return YES;
}


/*
** procmessage() - main message-entry code.
*/
static int procmessage(char WCmode)
{
    struct logBuffer person;
    char *pc;
    char c;
    int  upper, logno, temp;

    if (loggedIn)
    {
        strcpy(msgBuf.mbauth, logBuf.lbname);
    }

    if (getrecipient(&person, &logno)) 
    {
        if (getText(WCmode)) 
        {
            for (upper=YES, pc=msgBuf.mbtext; *pc && upper; pc++)
            {
                if (toupper(*pc) != *pc)
                {
                    upper=NO;
                }
            }

            if (upper)
            {
                fakeFullCase(msgBuf.mbtext);
            }

            if (roomTab[thisRoom].rtflags.ANON) 
            {
                /* this is the ONLY PLACE Anonymous gets set to YES */

                doCR();
                Anonymous = getYes("Anonymous");
            }
            storeMessage(&person, logno);
            Anonymous = NO;
            doCR();
            return YES;
        }
    }
    doCR();
    return NO;
}


/*
** netMessage() - Enter a net message
*/
void netMessage(char WCmode)
{
    zero_struct(msgBuf);
    if (makenetmessage()) 
    {
        procmessage(WCmode);
        logBuf.credit--;
    }
}


/*
** makenetmessage() - make a message a netmessage
*/
int makenetmessage(void)
{
    label who, node;

    if (thisRoom == MAILROOM) 
    {       /* get address for private mail */
        node[0] = 0;
        if (!verifynettable(node))
        {
            return NO;
        }
        if (msgBuf.mbto[0])
        {
            strcpy(who, msgBuf.mbto);
        }
        else
        {
            getNormStr("To", who, NAMESIZE, YES);
        }
        sprintf(msgBuf.mbto, "%s@%s", who, node);
    }
    else if (loggedIn && logBuf.lbflags.NET_PRIVS) 
    {
        if (!roomBuf.rbflags.SHARED) 
        {
            mPrintf("- This is not a netroom\n ");
            return NO;
        }

        if (!roomBuf.rbflags.AUTONET && (logBuf.credit == 0)) 
        {
            mPrintf("- You can't pay the postage!\n ");
            return NO;
        }
        strcpy(msgBuf.mbroute, "@O");
        strcpy(msgBuf.mboname, &cfg.codeBuf[cfg.nodeName]);
    }
    else
    {
        mPrintf("- You don't have netprivs\n ");
        return NO;
    }
    strcpy(msgBuf.mborg, &cfg.codeBuf[cfg.organization]);
    return YES;
}


/*
** replyMessage() - reply to a Mail> message
*/
static int replyMessage(void)
{
    NA who;

    if (thisRoom == MAILROOM) 
    {
        strcpy(who, msgBuf.mbauth);
        if (who[0] && msgBuf.mboname[0]) 
        {
            strcat(who, "@");
            strcat(who, msgBuf.mboname);
        }
    }

    zero_struct(msgBuf);

    if (thisRoom == MAILROOM)
    {
        strcpy(msgBuf.mbto, who);
    }

    if (roomBuf.rbflags.SHARED && roomBuf.rbflags.AUTONET) 
    {
        strcpy(msgBuf.mbroute, "@O");
        strcpy(msgBuf.mboname, &cfg.codeBuf[cfg.nodeName]);
        strcpy(msgBuf.mborg, &cfg.codeBuf[cfg.organization]);
    }

    return procmessage(ASCII);
}


/*
** hldMessage() - handles held messages
*/
int hldMessage(void)
{
    heldMessage = NO;
    copy_struct(tempMess, msgBuf);

    if (roomBuf.rbflags.SHARED) 
    {
        if (roomBuf.rbflags.AUTONET) 
        {
            strcpy(msgBuf.mbroute, "@O");
            strcpy(msgBuf.mboname, &cfg.codeBuf[cfg.nodeName]);
            strcpy(msgBuf.mborg, &cfg.codeBuf[cfg.organization]);
        }
    }
    else
    {
        msgBuf.mboname[0] = 0;
    }

    return procmessage(ASCII);
}


/*
** makeMessage() - menu-level routine to enter a message
*/
void makeMessage(char WCmode)
{
    zero_struct(msgBuf);
    /*
     * brute force netsave messages in autonet rooms...
     */
    if (roomBuf.rbflags.SHARED && roomBuf.rbflags.AUTONET) 
    {
        strcpy(msgBuf.mbroute, "@O");
        strcpy(msgBuf.mboname, &cfg.codeBuf[cfg.nodeName]);
        strcpy(msgBuf.mborg,   &cfg.codeBuf[cfg.organization]);
    }
    procmessage(WCmode);
}


/*
** copymessage() - Moves a message for aideMessageFuncs()
*/
static int copymessage(int m, char mode)
{
    label blah;
    int   i, roomTarg, ourRoom = thisRoom;
    int   curRoom;
    int   roomExists(), partialExist();

    curRoom = thisRoom;

    if (mode == 'M' || mode == 'C') 
    {
        mPrintf("%s message to (C/R=`%s')? ",
            (mode == 'M') ? "Move" : "Copy", oldTarget);

        getString("", blah, 20, ESC, YES);

        if (blah[0] == ESC || !onLine())
        {
            return NO;
        }
        if (strlen(blah) == 0)
        {
            strcpy(blah, oldTarget);
        }
        if ((roomTarg = roomCheck(roomExists, blah)) == ERROR) 
        {
            if ((roomTarg = roomCheck(partialExist, blah)) == ERROR) 
            {
                mPrintf("`%s' does not exist.\n ", blah);
                return NO;
            }
            else
            {
                thisRoom = roomTarg;
                if (roomCheck(partialExist, blah) != ERROR) 
                {
                    thisRoom = curRoom;
                    mPrintf("`%s' is ambiguous.\n ", blah);
                    return NO;
                }
                thisRoom = curRoom;
            }
        }
        strcpy(oldTarget, roomTab[roomTarg].rtname);
    }
    else    /* mode == 'K' */
    {
        if (!getYes("Kill that message"))
        {
            return NO;
        }
    }

    pullMLoc = roomBuf.msg[m].rbmsgLoc;
    pullMId  = roomBuf.msg[m].rbmsgNo ;

    if (mode != 'C') 
    {
        /* delete the message from this room. */

        for (i=m; i>0; i--) 
        {
            roomBuf.msg[i].rbmsgLoc = roomBuf.msg[i-1].rbmsgLoc;
            roomBuf.msg[i].rbmsgNo  = roomBuf.msg[i-1].rbmsgNo ;
        }
        roomBuf.msg[0].rbmsgNo  = 0L;   /* mark new slot at end as free */
        roomBuf.msg[0].rbmsgLoc = 0 ;   /* mark new slot at end as free */
    }
    noteRoom();
    putRoom(ourRoom);

    if (mode != 'K') 
    {
        getRoom(roomTarg);
        note2Message(pullMId, pullMLoc);
        noteRoom();
        putRoom(thisRoom);
        getRoom(ourRoom);
    }

    if (!TheSysop())    /* report Aide activity */
    {
        switch (mode)
        {
        case 'K':
            sprintf (msgBuf.mbtext,
                "Following message deleted from %s> by %s",
                roomBuf.rbname, uname());
            break;

        case 'M':
            sprintf (msgBuf.mbtext,
                "Following message moved from %s> to %s> by %s",
                roomBuf.rbname, oldTarget, uname());
            break;

        case 'C':
            sprintf (msgBuf.mbtext,
                "Following message copied from %s> to %s> by %s",
                roomBuf.rbname, oldTarget, uname());
        }
        aideMessage(YES);
    }
    return (mode != 'C');
}


/*
** aideMessageFuncs() - aide message manipulation menu
*/
static int aideMessageFuncs(int m)
{
    char ans;

    outFlag = OUTOK;
    
    while (1) 
    {
        outFlag = IMPERVIOUS;

        doCR();
        mPrintf(" %sM)ove C)opy ", (thisRoom == AIDEROOM) ? "" : "K)ill ");

        if (SomeSysop())
        {
            mPrintf("J)ournal %s", onConsole ? "P)rint " : "");
        }

        mPrintf("or eXit ? ");

        ans = (char)toupper(iChar());

        mPrintf("\b \n\r ");

        outFlag = OUTOK;

        if (!onLine())
        {
            return NO;
        }

        switch (ans)
        {
        case 'X':   /* eXit */
        case 'Q':   /* Quit */
        case 'E':   /* Exit */
            mPrintf("eXit");
            return NO;

        case 'K':
            if (thisRoom == AIDEROOM)   /* can't allow that! */
            {
                break;
            }
        case 'M':
        case 'C':
            if (copymessage(m, ans))
            {
                return YES;             /* return "pulled" flag */
            }
            break;

        case 'P':
            if (onConsole && SomeSysop())
            {
                msgToPrinter(m);
            }
            break;

        case 'J':
            if (SomeSysop()) 
            {
                if (!sendARinit())
                {
                    break;
                }
                printmessage(roomBuf.msg[m].rbmsgLoc, roomBuf.msg[m].rbmsgNo);
                sendARend();
            }
        default:
            break;
        }
    }
}


/*
** replymsg() - reply to a mail or normal message
*/
int replymsg (int ask)
{
    int retval;

    if (ask) 
    {
        mCR();
        if (!getNo("Respond"))
        {
            return NO;
        }
    }
    if (thisRoom != MAILROOM)
    {
        msgBuf.mbauth[0] = 0;
    }

    retval = replyMessage();

    if (thisRoom == MAILROOM && !onConsole)
    {
        echo = CALLER;        /* Restore privacy zapped by make... */
    }

    outFlag = OUTOK;

    return retval;
}


/*
** canreplyto() - can this user reply to this message?
*/
int canreplyto(char mode)
{
    if (loggedIn && usingWCprotocol == ASCII && mode == NEWoNLY) 
    {
        if (thisRoom == MAILROOM) 
        {
            if (msgBuf.mbauth[0] == 0 || stricmp(msgBuf.mbauth, program) == 0)
            {
                return NO;
            }
            if (msgBuf.mboname[0] && logBuf.lbflags.NET_PRIVS) 
            {
                if (netnmidx(msgBuf.mboname) == ERROR)
                {
                    return NO;
                }
                if (netBuf.nbflags.ld && ((int)cfg.ld_cost > logBuf.credit))
                {
                    return NO;
                }
            }
            else if (stricmp(msgBuf.mbauth, logBuf.lbname) == 0)
            {
                return NO;
            }
        }
        return !roomBuf.rbflags.READONLY;
    }
    return NO;
}


/*
** promptmsg() - give .r(ead) m(ore) prompts
*/
static int promptmsg(int msgLoc, long msgNo)
{
    int sendARchar();
    int (*pfc)();
    int c, rep, local_msg;
    int reprompt = YES;

    local_msg = (msgBuf.mboname[0] == 0)
      || (stricmp(msgBuf.mboname, &cfg.codeBuf[cfg.nodeName]) == 0);

    while (1) 
    {
        outFlag = OUTOK;
        rep = canreplyto(NEWoNLY);

        if (reprompt) 
        {
            doCR();
            mPrintf(" %s", expert ? "a n b" : "A)gain N)ext B)ack");

            if (rep)
            {
                if (thisRoom == MAILROOM)
                {
                    mPrintf("%s", expert ? " r" : " R)eply");
                }
                else
                {
                    if (msgBuf.mbauth[0] && local_msg)
                    {
                        mPrintf("%s", expert ? " m" : " M)ail");
                    }

                    mPrintf("%s", expert ? " e" : " E)nter");
                }

                if (heldMessage)
                {
                    mPrintf("%s", expert ? " h" : " H)eld");
                }
            }

            if (aide)
            {
                if (thisRoom != AIDEROOM)
                {
                    mPrintf("%s", expert ? " k" : " K)ill");
                }

                mPrintf("%s", expert ? " ." : " .)aide");
            }

            mPrintf("%s ? ", expert ? " q" : " Q)uit");
        }
        if (!onLine())
        {
            return 0;
        }

        c = toupper(iChar());

        if (c == 'A') 
        {
            mPrintf ("gain\n ");
            printmessage (msgLoc, msgNo);
            reprompt = YES;
        }
        else if ((c == '.') && aide)
        {
            mPrintf("\b-Aide\n ");
            aideMsgFlag = YES;
            return 0;
        }
        else if ((c == 'K') && aide && (thisRoom != AIDEROOM))
        {
            mPrintf("ill\n ");
            killMsgFlag = YES;
            return 0;
        }
        else if (c == 'J' && SomeSysop()) 
        {
            mPrintf("ournal\n ");
            journalMessage = YES;
            return 0;
        }
        else if (c == 'H' && heldMessage) 
        {
            mPrintf("eld message\n ");
            hldMessage();
            return 0;
        }
        else if ((c == 'E' || c == 'R') && rep) 
        {
            mPrintf("\bEnter reply\n ");
            return replymsg(0);
        }
        else if ((c == 'M') && rep && local_msg) 
        {
            mPrintf("\bMail reply\n ");
            if (privateReply())
            {
                return 0;
            }
            else
            {
                reprompt = YES;
            }
        }
        else if (c == 'Q') 
        {
            mPrintf("uit\n ");
            outFlag = OUTSKIP;
            return 0;
        }
        else if (c == 'N')
        {
            mPrintf("ext\n ");
            return 0;
        }
        else if (c == '\n')
        {
            return 0;
        }
        else if (c == 'B')          /* read previous message */
        {
            mPrintf("ack one\n ");
            return BACKUP;
        }
        else if (c == '?') 
        {
            mPrintf("\n A) read message Again");
            mPrintf("\n N) read Next message (default)");
            mPrintf("\n B) Back to previous message");

            if (rep)
            {
                if (thisRoom == MAILROOM)
                {
                    mPrintf("\n R) Reply to private mail");
                }
                else
                {
                    if (msgBuf.mbauth[0] && local_msg)
                    {
                        mPrintf("\n M) reply via private Mail");
                    }

                    mPrintf("\n E) Enter a message");
                }
            }
            if (heldMessage)
            {
                mPrintf("\n H) continue with Held message");
            }
            if (aide)
            {
                if (thisRoom != AIDEROOM)
                {
                    mPrintf("\n K) Kill that message");
                }

                mPrintf("\n .) Aide message functions");
            }
            mPrintf("\n Q) Quit reading messages\n ");

            reprompt = YES;
        }
        else if (c == 'P' || c == ' ')  /* ignore P and SpaceBar */
        {
            reprompt = NO;
            mPrintf("\b \b");
        }
        else
        {
            whazzit();
            reprompt = YES;
        }
    }
}


/*
** showMessages() - print a roomful of msgs
*/
void showMessages (char whichMess, char revOrder)
{
    int  i, pulled, temp;
    int  start, increment;
    long lowLim, highLim, msgNo;

    if (thisRoom == MAILROOM && !loggedIn) 
    {
        tutorial("POLICY.HLP");
        echo = BOTH;
        return;
    }

    if (thisRoom == MAILROOM && !onConsole)
    {
        echo = CALLER;
    }

    if (usingWCprotocol != ASCII)
    {
        singleMsg = NO;
    }
    else if (!expert)
    {
        mPrintf("\n \nN)ext, P)ause, S)top\n \n");
    }

    /* Allow for reverse retrieval: */

    if (revOrder) 
    {
        start = MSGSPERRM - 1;
        increment = -1;
    }
    else
    {
        start = 0;
        increment = 1;
    }

    switch (whichMess)   
    {
    case OLDaNDnEW:
        lowLim  = cfg.oldest;
        highLim = cfg.newest;
        break;

    case OLDoNLY:
        lowLim  = cfg.oldest;
        highLim = logBuf.lbvisit[ logBuf.lbgen[thisRoom] & CALLMASK];
        break;

    default:
        lowLim  = logBuf.lbvisit[ logBuf.lbgen[thisRoom] & CALLMASK]+1;
        highLim = cfg.newest;
        if (!revOrder
          && usingWCprotocol == ASCII
          && thisRoom != MAILROOM
          && oldToo) 
        {
            for (i = MSGSPERRM-1; i != -1; i--)
            {
                if (lowLim > roomBuf.msg[i].rbmsgNo
                    && roomBuf.msg[i].rbmsgNo >= cfg.oldest)
                    break;
            }

            if (i != -1)
            {
                lowLim = roomBuf.msg[i].rbmsgNo;
            }
        }

        /* stuff may have scrolled off system unseen, so... */

        if (lowLim < cfg.oldest)
        {
            lowLim = cfg.oldest;
        }
    }

    aideMsgFlag = NO;
    killMsgFlag = NO;

    for (i = start; i < MSGSPERRM && i >= 0 && onLine(); i += increment) 
    {

        msgNo = roomBuf.msg[i].rbmsgNo;

        if (msgNo >= lowLim && msgNo <= highLim) 
        {

            if (!doFindMsg(roomBuf.msg[i].rbmsgLoc, msgNo))
            {
                continue;
            }
            if ( msguser[0]
              && strnicmp(msguser, msgBuf.mbto,   strlen(msguser))
              && strnicmp(msguser, msgBuf.mbauth, strlen(msguser)))
            {
                continue;
            }
            if (dPass && !dateok(parsedate(msgBuf.mbdate)))
            {
                continue;
            }

            doPrintMsg();

            /* Aide functions invoked by an mAbort() */

            if (aideMsgFlag || killMsgFlag) 
            {
                if (aideMsgFlag)
                {
                    aideMsgFlag = NO;
                    pulled = aideMessageFuncs(i);
                }
                else
                {
                    killMsgFlag = NO;
                    pulled = copymessage(i, 'K');

                }
                outFlag = OUTOK;

                if (pulled) 
                {
                    if (revOrder)
                    {
                        i++;
                    }
                    continue;
                }
            }

            if (singleMsg && outFlag != OUTSKIP) 
            {

                temp = promptmsg(roomBuf.msg[i].rbmsgLoc, msgNo);

                if (temp == BACKUP) 
                {

                    if (revOrder && msgNo == roomBuf.rblastMessage)
                    {
                        break;         /* nothing to go back to */
                    }
                    if (!revOrder)
                    {
                        if (i == 0 || roomBuf.msg[i-1].rbmsgNo < cfg.oldest)
                        {
                            break;      /* nothing to go back to */
                        }
                    }

                    /* suddenly all msgs are fair game */

                    lowLim = cfg.oldest;
                    highLim = cfg.newest;

                    i -= increment;        /* back one in current direction */
                    i -= increment;        /* cancel out i+= at top of loop */
                }
                else if (temp) 
                {           /* message was deleted? */
                    i--;
                }
            }

            if (outFlag != OUTOK) 
            {
                if (outFlag == OUTNEXT || outFlag == OUTPARAGRAPH)
                {
                    outFlag = OUTOK;
                }
                else if (outFlag == OUTSKIP)
                {
                    break;
                }
            }

            if (!onLine())
            {
                break;
            }

            /* This code is repeated, because these flags can be set
            ** during doPrintMsg() by mAbort(), or by promptmsg().
            */

            if (aideMsgFlag || killMsgFlag) 
            {
                if (aideMsgFlag)
                {
                    aideMsgFlag = NO;
                    pulled = aideMessageFuncs(i);
                }
                else
                {
                    killMsgFlag = NO;
                    pulled = copymessage(i, 'K');
                }

                if (pulled) 
                {
                    if (revOrder)
                    {
                        i++;
                    }
                    continue;
                }
            }

            if (thisRoom == MAILROOM
                && !singleMsg
                && canreplyto(whichMess)
                && replymsg(1))
            {
                i--;
            }
        }
    }
    echo = BOTH;
}


/*
** sendPRNchar() -- dump a character to printer
*/
int sendPRNchar(int c)
{
    int lastchar = fputc(c, stdprn);

    if (lastchar == NEWLINE)
    {
        lastchar = fputc(CRETURN, stdprn);
    }
    return lastchar != EOF;
}


/*
** msgToPrinter - sends message m from current room to the printer
*/
static void msgToPrinter(int m)
{
    int (*pfc)();

    mPrintf("\bprint ");

    usingWCprotocol = TOPRINTER;    /* set up for Archive message print */
    pfc = sendPFchar;               /* stash output vector */
    sendPFchar = sendPRNchar;       /* replace with Archive */

    doCR();
    mPrintf ("Message found in %s %s @ %s:",
        formRoom(thisRoom, NO),
        room_area,
        &cfg.codeBuf[cfg.nodeTitle]);

    doCR();
    doCR();
    printmessage(roomBuf.msg[m].rbmsgLoc, roomBuf.msg[m].rbmsgNo);
    doCR();
    fflush (upfd);               /* flush DOS buffer */
    usingWCprotocol=ASCII;       /* repair */
    sendPFchar = pfc;
}
