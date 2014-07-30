/*{ $Workfile:   edituser.c  $
 *  $Revision:   1.12  $
 * 
 *  User Log editor for b0badel - invoke with ^LU
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
 
#define EDITUSER_C

#include <string.h>
#include "ctdl.h"
#include "externs.h"
#include "event.h"
#include "protocol.h"
#include "audit.h"
#include "door.h"
#include "poll.h"


/* GLOBAL Contents:
**
** listUsers()  handy component routine
** editUser()   the top routine
*/

/* prototypes for static functions */

static int killuser         (struct logBuffer *user, int slot);
static int aidetoggle       (struct logBuffer *user, int slot);
static int nettoggle        (struct logBuffer *user, int slot);
static int creditsetting    (struct logBuffer *user, int slot);
static int change_password  (struct logBuffer *user, int slot);
static int change_username  (struct logBuffer *user, int slot);
static void viewuser        (struct logBuffer *user);


static int killuser (struct logBuffer *user, int slot)
{
    if (user->credit) 
    {
        mPrintf("%s has %s. - ", user->lbname,
            plural("l-d credit", (long)(user->credit)));
    }
    if (getNo(confirm)) 
    {
        if (loggedIn && logTab[slot].ltlogSlot == logindex)
        {
            terminate(YES, 'k');
        }
        mPrintf("%s killed\n ", user->lbname);


        /*** zero out the structure and logTab hashes ***/

        blkfill (user, 0, sizeof (struct logBuffer));

        logTab[slot].ltpwhash = logTab[slot].ltnmhash = 0;

        return YES;
    }
    else
    {
        return NO;
    }
}


static int aidetoggle (struct logBuffer *user, int slot)
{
    mPrintf("%s %s aide privileges - ", 
        user->lbname,(user->lbflags.AIDE)?"loses":"gets");

    if (getNo(confirm)) 
    {
        user->lbflags.AIDE = !user->lbflags.AIDE;
        if (loggedIn && logTab[slot].ltlogSlot == logindex)
        {
            aide = user->lbflags.AIDE;
        }
        return YES;
    }
    else
    {
        return NO;
    }
}


static int nettoggle (struct logBuffer  *user, int slot)
{
    mPrintf("%s %s net privileges - ", user->lbname,
        user->lbflags.NET_PRIVS ? "loses":"gets");

    if (getNo(confirm)) 
    {
        user->lbflags.NET_PRIVS = !user->lbflags.NET_PRIVS;

        if (loggedIn && logindex == logTab[slot].ltlogSlot)
        {
            logBuf.lbflags.NET_PRIVS = user->lbflags.NET_PRIVS;
        }
        return YES;
    }
    else
    {
        return NO;
    }
}


static int creditsetting (struct logBuffer *user, int slot)
{
    if (user->lbflags.NET_PRIVS || nettoggle(user, slot)) 
    {
        mPrintf("%s has %s; ", user->lbname, 
            plural("credit", (long)(user->credit)));

        user->credit = asknumber("new setting", 0L, 0x7FFFL, user->credit);

        if (loggedIn && logindex == logTab[slot].ltlogSlot)
        {
            logBuf.credit = user->credit;
        }
        return YES;
    }
    else
    {
        return NO;
    }
}


static int change_password (struct logBuffer *user, int slot)
{
    register int   pwhash, i;
    label          pw;
    int            poorchoice = YES;

    while (poorchoice)  
    {
        poorchoice = NO;

        getNormStr("New password", pw, NAMESIZE, YES);

        if (strlen(pw) < 1 || !onLine())
        {
            return NO;
        }

        if (strlen(pw) > 2)
        {
            pwhash = hash(pw);

            for (i=0; i < cfg.MAXLOGTAB; i++)
            {
                if (logTab[i].ltpwhash == pwhash) 
                {
                    poorchoice = YES;
                    break;
                }
            }
        }
        else
        {
            poorchoice = YES;
        }

        if (poorchoice)
        {
            mPrintf("Poor password\n ");
        }
    }

    strcpy (user->lbpw, pw);

    if (loggedIn && logTab[slot].ltlogSlot == logindex)
    {
        strcpy (logBuf.lbpw, pw);
    }

    logTab[slot].ltpwhash = pwhash;

    return YES;
}


static int change_username (struct logBuffer *user, int slot)
{
    register int   namehash, i;
    label          name;
    int            poorchoice = YES;

    while (poorchoice)  
    {
        poorchoice = NO;

        getNormStr("New username", name, NAMESIZE, YES);

        if (strlen(name) < 1 || !onLine())
        {
            return NO;
        }

        if (strlen(name) > 2)
        {
            namehash = hash(name);

            for (i=0; i < cfg.MAXLOGTAB; i++)
            {
                if (logTab[i].ltnmhash == namehash) 
                {
                    poorchoice = YES;
                    break;
                }
            }
        }
        else
        {
            poorchoice = YES;
        }

        if (poorchoice)
        {
            mPrintf("Name not allowed\n ");
        }
    }

    strcpy (user->lbname, name);

    if (loggedIn && logTab[slot].ltlogSlot == logindex)
    {
        strcpy (logBuf.lbname, name);
    }

    logTab[slot].ltnmhash = namehash;

    return YES;
}


static void viewuser (struct logBuffer *user)
{
    mPrintf("realname: %s\n ", user->lbrealname);      
    mPrintf("password: %s\n ", user->lbpw);
    mPrintf("%s Aide, %s Expert\n ", 
        user->lbflags.AIDE   ? "Is" : "Not",
        user->lbflags.EXPERT ? "Is" : "Not");

    if (user->lbflags.NET_PRIVS)
    {
        mPrintf("Has network access, %s", 
            plural("credit", (long)(user->credit)));
    }
    else
    {
        mPrintf("No network access");
    }

    return;
}


/*
** listUsers() - may be usable by users as well
*/
int listUsers (void)
{
    int i, comma = NO, empty = NO;
    struct logBuffer  userbuf;

    doCR();

    for (i = 0; i < cfg.MAXLOGTAB; i++) 
    {  
        if (!onLine())
        {
            return NO;
        }

        getLog (&userbuf, i);

        if (*userbuf.lbname)
        {
            mPrintf("%s%s", (comma || empty) ? ", " : "", userbuf.lbname);
            comma = YES;
        }
        else if (SomeSysop())
        {
            mPrintf("%s@", comma ? ", " : "");
            comma = NO;
            empty = YES;
        }
    }
    doCR();

    return YES;
}


/*
** editUser() - a menu unto itself
*/
int editUser (void)
{
    label    who;
    int      account, slot;
    int      changed = NO;
    struct   logBuffer user;

    /*** get a user name ***/

    while (1)
    {
        getNormStr("username", who, NAMESIZE, YES);

        if (strlen(who) == 0)
        {
            return NO;
        }
        if (strcmp(who, "?") == 0)
        {
            listUsers();
            continue;
        }
        if ((account = getnmidx(who, &user)) == ERROR)
        {
            mPrintf("No such username\n ");
            return NO;
        }
        else
        {
            break;
        }
    }

    /*** the account is now in our local user struct ***/

    slot = logTab[account].ltlogSlot;

    viewuser (&user);

    while (onLine()) 
    {
        outFlag = OUTOK;
        mPrintf("\n \n edit %s: ", user.lbname);

        switch (toupper (getnoecho())) 
        {
        case 'A':
            changed |= aidetoggle (&user, account);
            break;

        case 'C':
            mPrintf("Credits\n ");
            changed |= creditsetting (&user, account);
            break;

        case 'K':
            mPrintf("Kill %s\n ", user.lbname);      
            if (killuser (&user, account))
            {
                putLog (&user, logTab[account].ltlogSlot);
                return YES;
            }
            break;

        case 'N':
            mPrintf("Net access\n ");
            changed |= nettoggle (&user, account);
            break;

        case 'P':
            mPrintf("Password change\n ");
            changed |= change_password (&user, account);
            break;

        case 'U':
            mPrintf("Username change\n ");
            changed |= change_username (&user, account);
            break;

        case 'V':
            mPrintf("View account\n ");
            mPrintf("username: %s\n ", user.lbname);      
            viewuser (&user);
            break;

        case 'X':
            if (changed)
            {
                putLog (&user, logTab[account].ltlogSlot);
            }
            mPrintf("eXit user edit menu.\n ");
            return YES;

        default:
            mPrintf("\n A) Aide");
            mPrintf("\n C) Credits for netmail");
            mPrintf("\n K) Kill %s", user.lbname);
            mPrintf("\n N) Net access");
            mPrintf("\n P) Password");
            mPrintf("\n U) Username");
            mPrintf("\n V) View account");
            mPrintf("\n X) eXit this menu");
        }
    }

    /*** oops! lost carrier or got kicked off or something ***/

    return NO;
}


