/*{ $Workfile:   log.c  $
 *  $Revision:   1.14  $
 *
 *  userlog code for Citadel bulletin board system
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
#include "clock.h"
#include "externs.h"

/*
**  login()         log a user in
**  changepw()      change a user's password
**  newUser()       log a new user.
**  storeLog()      store data in log
**  terminate()     log off the system.
*/

static int badpw = NO;

/*
**      bubblelog() - slide a logtable entry to logTab[0]
*/
static void bubblelog (int start)
{
    struct lTable temp;

    logTab[start].ltnewest = cfg.newest;
    copy_struct(logTab[start], temp);
    while (--start >= 0)
        copy_struct(logTab[start], logTab[1+start]);
    copy_struct(temp, logTab[0]);
}


/*
 ************************************************************************
 *      getpwlog() - find the user with a given password                *
 ************************************************************************
 */
static
getpwlog(pw, p)
label pw;
struct logBuffer *p;
{
    register pwh, i;

    if (strlen(pw) < 2)
        return ERROR;
    for (pwh=hash(pw), i=0; i < cfg.MAXLOGTAB; i++)
        if (logTab[i].ltpwhash == pwh) 
        {
            getLog(p, logTab[i].ltlogSlot);
            if (stricmp(pw, p->lbpw) == 0)
                return i;
        }
    return ERROR;
}


/*
** login() is the menu-level routine to log someone in
*/
int login (char *username, char *password)
{
    int  found, ltentry;
    long today;

    ltentry = getpwlog(password, &logBuf);

    found = (ltentry != ERROR);

    if (found)
    {
        if (labelcmp(logBuf.lbname, username))
        {
            found = NO;
        }
    }

    if (found && *password) 
    {
        badpw = NO;
        loggedIn = YES;
        setlog();
        logindex = logTab[ltentry].ltlogSlot;
        bubblelog(ltentry);

        /* reset event trigger because a user might have logged in within
         * the 5 minute warning period
         */
        if (evtRunning && isPreemptive(nextEvt)) 
        {
            evtTrig = 300L;
            warned = NO;
        }

        /*** reset download limits if the last login wasn't today ***/

        timeis(&now);
        today = julian_date(&now);
        if (today != logBuf.lblast) 
        {
            logBuf.lblast = today;
            logBuf.lblimit= 0L;
        }

        outFlag = OUTOK;
        hotblurb("notice.blb");

        if (heldMessage && !strcmp(tempMess.mbauth, logBuf.lbname))
        {
            mPrintf("You have a message in your held message buffer.\n ");
        }
        else
        {
            heldMessage = NO;
        }

        logMessage(L_IN, logBuf.lbname, NO);

        doMailFirst();

        listRooms(l_NEW);
        outFlag = OUTOK;
    }
    else
    {
        /* discourage password-guessing: */

        if (strlen(password) > 1 && !onConsole) 
        {
            if (badpw)
            {
                pause(500 * badpw);
            }
            ++badpw;
        }
        if (!(cfg.unlogLoginOk || onConsole)) 
        {
            outFlag = IMPERVIOUS;
            if (!dotutorial("deny.blb", YES))
            {
                mPrintf("Bad password\n ");
            }
            outFlag = OUTOK;
        }
        else if (getNo("No record: Enter as new user"))
        {
            return newUser(username);
        }
        return NO;
    }
    return YES;
}


/*
 ************************************************************************
 *      changepw() - change the user's password.  If the user can't     *
 *                provide his/her old password, assume it's an illegal  *
 *                user and don't let him/her change the password        *
 ************************************************************************
 */
changepw()
{
    label pw;
    struct logBuffer temp;

    /*
     * save password so we can find current user again
     */
    getNormStr("\n Old password", pw, NAMESIZE, NO);
    if (stricmp(pw, logBuf.lbpw) != 0)
        return;

    while(1) 
    {
        getNormStr("New password", pw, NAMESIZE, NO);
        if (strlen(pw) < 1 || !onLine())
            return;
        if (strlen(pw) > 1 && getpwlog(pw, &temp) == ERROR)
            break;
        mPrintf("Poor password\n ");
    }

    strcpy(logBuf.lbpw, pw);
    logTab[0].ltpwhash = hash(pw);
    storeLog();

    mPrintf("\n nm: %s\n ", logBuf.lbname);
    echo = CALLER;
    mPrintf(   "pw: %s\n ", logBuf.lbpw);
    echo = BOTH;
}


/*
** legalName() - keeps control chars, etc. out of names
*/
int legalName (char *name)
{
    char *p = name;
    int  h, i;

    /*** reject strings with control chars ***/

    while (*p)
    {
        if (*p++ < ' ')
        {
            return NO;
        }
    }

    /*** reject strings with illegal chars used in networking ***/

    if (strpbrk(name, "@_%!#") != NULL)
    {
        return NO;
    }

    /*** reject strings that don't hash uniquely ***/

    h = hash(name);

    if (h == 0 || h == hash("Citadel") || h == hash("Sysop"))
    {
        return NO;
    }

    for (i = 0; i < cfg.MAXLOGTAB; i++)
    {
        if (h == logTab[i].ltnmhash)
        {
            return NO;
        }
    }

    return YES;     /*** name is acceptable ***/
}


/*
** newUser() prompts for name and password
*/
int newUser (char *name)
{
    label       username, realname, password;
    struct      logBuffer temp;
    int         g, i;
    long        low;

    heldMessage = NO;   /* wipe out any held messages                   */
    new_configure();    /* ask a few configuration questions            */

    dotutorial("password.blb", NO);

    do 
    {
        /* get name and check for uniqueness... */

        while (onLine()) 
        {
            if (name[0] && (name != NULL)) 
            {
                strcpy(username, name);
                name[0] = 0;
                mPrintf("username: %s\n ", username);
            }
            else
            {
                outFlag = OUTOK;
                getNormStr("username", username, NAMESIZE, YES);
            }

            if (strlen(username) < 1)
            {
                return NO;
            }
            else if (legalName(username)) 
            {
                break;      /* out of while() loop */
            }

            mPrintf("Unusable name - try again\n ");
        }

        getNormStr("realname", realname, NAMESIZE, YES);

        if (strlen(realname) < 1)
        {
            strcpy(realname, username);
        }

        /* get password and check for uniqueness...     */

        while (onLine()) 
        {
            echo = CALLER;
            getNormStr("password", password, NAMESIZE, YES);
            echo = BOTH;
            if (strlen(password) < 1)
                return NO;
            if (getpwlog(password, &temp) == ERROR)
                break;
            mPrintf("Password disallowed\n ");
        }

        mPrintf("\n username: %s ", username);
        mPrintf("\n realname: %s\n ", realname);
        echo = CALLER;
        mPrintf(   "password: %s\n \n ", password);
        echo = BOTH;
    } while (!getYesNo("OK") && onLine())
        ;


    if (onLine()) 
    {
        logMessage(L_IN, username, '+');

        /*
         * grab oldest log entry for the new one.
         */
        bubblelog(cfg.MAXLOGTAB-1);

        logTab[0].ltpwhash = hash(password);
        logTab[0].ltnmhash = hash(username);

        logindex = logTab[0].ltlogSlot;
        zero_struct(logBuf);

        strcpy(logBuf.lbname, username);
        strcpy(logBuf.lbrealname, realname);
        strcpy(logBuf.lbpw, password);
        logBuf.lbflags.L_INUSE = loggedIn = YES;
        logBuf.lbflags.NET_PRIVS = cfg.allNet;
        timeis(&now);
        logBuf.lblast = julian_date(&now);

        low = (cfg.newest-cfg.oldest > 50L) ? (cfg.newest-50L) : cfg.oldest;

        logBuf.lbvisit[0]= cfg.newest;
        for (i=1; i<MAXVISIT-1; i++)
            logBuf.lbvisit[i] = low;
        logBuf.lbvisit[MAXVISIT-1]= cfg.oldest;

        for (i=0; i<MAXROOMS; i++) 
        {
            g = roomTab[i].rtgen;
            if (!roomTab[i].rtflags.PUBLIC)
                g = (g+MAXGEN-1) % MAXGEN;
            logBuf.lbgen[i] = (g << GENSHIFT) + (MAXVISIT-1);
        }
        roomTab[MAILROOM].rtlastMessage = 0L;

        storeLog();

        /*** force the new user to read policy.hlp ***/

        dotutorial("policy.hlp", expert);

        /*** now make them read the Notices, like everyone else ***/

        hotblurb("notice.blb");

        return YES;
    }
    return NO;
}


/*
** storeLog() stores the current log record.
*/
void storeLog (void)
{
    if (loggedIn) 
    {
        logTab[0].ltnewest = logBuf.lbvisit[0] = cfg.newest;
        logBuf.lbwidth         = termWidth;
        logBuf.lbnulls         = termNulls;
        logBuf.lbflags.EXPERT  = expert;
        logBuf.lbflags.LFMASK  = termLF;
        logBuf.lbflags.AIDE    = aide;
        logBuf.lbflags.TIME    = sendTime;
        logBuf.lbflags.OLDTOO  = oldToo;
        putLog(&logBuf, logindex);
    }
}

/*
** terminate() is menu-level routine to exit system
**  1. parameter <disconnect> is YES or NO.
**  2. if <disconnect> is YES, breaks modem connection
**     or turns off onConsole, as appropriate.
**  3. modifies externs: struct logBuf, struct *logTab
*/
void terminate (char disconnect, char flag)
{
    int i;

    badpw = NO;

    if (loggedIn) 
    {
        if (strchr(" -g", flag))
        {
            logBuf.lbgen[thisRoom] = roomBuf.rbgen << GENSHIFT;
        }

        storeLog();

        logMessage(L_OUT, "", flag);

        if (flag != ' ' || !dotutorial("logout.blb", NO))
        {
            mPrintf(" %s logged out\n ", logBuf.lbname);
        }
        loggedIn = NO;
        setlog();
    }

    if (disconnect) 
    {
        int chk = haveCarrier;

        if (chk)
        {
            hangup();
        }

        if (onConsole) 
        {
            onConsole = NO;
            modemOpen();
            xprintf("\n`MODEM' mode.\n");
        }
        else
        {
            if (chk)
            {
                modIn();
            }
        }
        xprintf("%s %s\n", formDate(), tod(NO));
    }

    for (i = 0; i < MAXROOMS; i++)      /* Clear skip bits */
    {
        roomTab[i].rtflags.SKIP = 0;
    }
    lastRoom = -1;
}
