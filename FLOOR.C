/*{ $Workfile:   floor.c  $
 *  $Revision:   1.15  $
 *
 *  b0badel functions having to do with floors.
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
 
#define FLOOR_C

#include <stdlib.h>
#include <string.h>
#include "ctdl.h"
#include "protocol.h"
#include "externs.h"

/* GLOBAL Contents:
**
** doFloor()        do a floor command.
** gotoFloor()      return an accessable room on a given floor.
** floorMap()       show all known rooms, divided by floors
** fetchRoom()      make sure we have the proper floor for this room
** findFloor()      return the floor # associated with a gen number
** globFloor()      read all new messages on this floor.
*/

typedef enum
{
    BACKWARD = -1,
    FORWARD  = 1
}
DIRECTION;


/* local prototypes */

static char findGen         (void);
static void makeFloor       (void);
static void addToFloor      (void);
static int  addARoom        (char *buffer);
static void killFloor       (void);
static int  floorAide       (void);
static void showMapHeader   (int mask);
static void listFloor       (int mask);
static void lFloor          (int mask);
static int  floorExists     (label floor);
static int  floorMatch      (label floor);
static void jumpFloor       (DIRECTION step);
static void zFloor          (void);
static int  globFloor       (int WC);
static int  makeFUnknown    (char *user);
static int  makeFKnown      (char *user);
static int  doFWork         (char *user, int delta);


/*
** findGen() - Find a unused generation number
*/
static char findGen (void)
{
    register char gen;
    register rover;

    for (gen=0; gen < 1+cfg.floorCount; gen++) 
    {
        for (rover=0; rover<cfg.floorCount; rover++)
            if (floorTab[rover].flInUse && (gen == floorTab[rover].flGen))
                break;
        if (rover >= cfg.floorCount)
            return gen;
    }
    return 1 + cfg.floorCount;
}


/*
** makeFloor() - Create a floor.
*/
static void makeFloor (void)
{
    int         i, nFloor, nRoom, sFloor, hold;
    label       floor;
    struct flTab *new;

    mPrintf("reate floor\n ");
    cleanFloor();       /* clean house... */
    getNormStr("Name of new floor", floor, NAMESIZE, YES);
    if (strlen(floor) == 0)
        return;

    for (i=0; i<cfg.floorCount; i++)
    {
        if (floorTab[i].flInUse && stricmp(floorTab[i].flName, floor) == 0) 
        {
            mPrintf("a [%s] already exists!\n ", floor);
            return;
        }
    }

    for (nFloor=0; floorTab[nFloor].flInUse && nFloor<cfg.floorCount; nFloor++)
        ;

    if (nFloor >= cfg.floorCount)
    {
        new = realloc(floorTab, (1+cfg.floorCount) * sizeof floorTab[0]);

        if (new != NULL)
        {
            floorTab = new;
            cfg.floorCount++;
        }
        else
        {
            mPrintf("Cannot create floor!\n ");
            return;
        }
    }
    strcpy(floorTab[nFloor].flName, floor);
    floorTab[nFloor].flGen   = findGen();
    floorTab[nFloor].flInUse = YES;

    sFloor = thisFloor;
    thisFloor = nFloor;
    getList(addARoom, "Rooms in this floor");

    if ((nRoom = gotoFloor(floorTab[nFloor].flGen)) != ERROR) 
    {
        sprintf(msgBuf.mbtext,"floor [%s] created by %s\n ", floor, uname());
        aideMessage(NO);
        thisFloor = nFloor;
        getRoom(nRoom);
    }
    else
    {           /* no rooms? reclaim it! */
        thisFloor = sFloor;
        floorTab[nFloor].flInUse = NO;
    }
    putFloor(nFloor);
}


/*
** addToFloor() - Add new rooms to a floor.
*/
static void addToFloor (void)
{
    int hold;

    putRoom(hold=thisRoom);
    getList(addARoom, "Rooms to add to this floor");
    cleanFloor();
    getRoom(hold);
}


/*
** addARoom()
*/
static int addARoom (char *buffer)
{
    int i = roomExists(buffer);

    if (i != ERROR && (canEnter(i) || i == thisRoom)) 
    {
        if (i == LOBBY || i == MAILROOM || i == AIDEROOM)
        {
            mPrintf("Can't move %s\n ", formRoom(i, NO));
        }
        else
        {
            getRoom(i);
            roomBuf.rbflags.floorGen = floorTab[thisFloor].flGen;
            noteRoom();
            putRoom(i);
        }
    }
    else
    {
        mPrintf("%s not found\n ",buffer);
    }

    return YES;         /* return int necessary for getList() */
}


/*
** killFloor() - Kill off a floor and (optionally) all the rooms in it.
*/
static void killFloor (void)
{
    int i, first=YES;
    char kGen;
    int roomKill;
    extern char confirm[];
    char *eos;

    mPrintf("ill floor- ");
    if (thisFloor == LOBBYFLOOR) 
    {
        mPrintf("not this floor\n ");
        return;
    }

    sprintf(msgBuf.mbtext,"floor [%s] killed by %s\n ",
        floorTab[thisFloor].flName, uname());
    for (eos = msgBuf.mbtext; *eos; eos++)
        ;
    if (getNo(confirm)) 
    {
        putRoom(thisRoom);
        roomKill = onConsole?getYesNo("Kill all rooms on this floor?"):NO;

        kGen = floorTab[thisFloor].flGen;
        for (i=0; i<MAXROOMS; i++)
            if (roomTab[i].rtflags.INUSE && roomTab[i].rtflags.floorGen == kGen) 
            {
                getRoom(i);
                if (roomKill) 
                {
                    roomBuf.rbflags.INUSE = NO;
                    sprintf(eos, "%s %s>",(first ? "Rooms deleted:" : ","),
                        roomBuf.rbname);
                    while (*eos)
                        eos++;
                    first = NO;
                }
                else
                    roomBuf.rbflags.floorGen = LOBBYFLOOR;
                noteRoom();
                putRoom(i);
            }
        floorTab[thisFloor].flInUse = NO;
        putFloor(thisFloor);
        aideMessage(NO);
        thisFloor = LOBBYFLOOR;
        getRoom(LOBBY);
    }
}


/*
** doFloor() - do a floor command.
*/
int doFloor (char c)
{
    label where, floor;
    int nFloor, nRoom;

    switch (c=toupper(c)) 
    {
    case 'G':
        mPrintf("oto ");
        getString("", where, NAMESIZE-1, '?', YES);
        if (strcmp(where, "?") == 0) 
        {
            doCR();
            listFloor(l_OLD|l_NEW|l_LONG);
        }
        else if (strlen(where) > 0) 
        {
            normalise(where);
            if ( ((nFloor=floorExists(where)) == ERROR
                && (nFloor=floorMatch(where)) == ERROR)
                || ((nRoom=gotoFloor(floorTab[nFloor].flGen)) == ERROR) ) 
            {
                mPrintf("No such floor\n ");
                return;
            }
            pushRoom(thisRoom, logBuf.lbgen[thisRoom] & CALLMASK);
            saveRmPtr('R');
            putRoom(thisRoom);
            getRoom(nRoom);
            fetchRoom(NO);
        }
        else
        {
            pushRoom(thisRoom, logBuf.lbgen[thisRoom] & CALLMASK);
            saveRmPtr('R');
            putRoom(thisRoom);
            jumpFloor(FORWARD);
        }
        break;
    case 'K':
        mPrintf("nown floors");
        listFloor(l_OLD|l_NEW);
        break;
    case '>':
        oChar('\b');
        jumpFloor(FORWARD);
        break;
    case '<':
        oChar('\b');
        jumpFloor(BACKWARD);
        break;
    case 'S':
        mPrintf("kip [%s] goto ", floorTab[thisFloor].flName);
        jumpFloor(FORWARD);
        break;
    case 'Z':
        zFloor();
        break;
    case 'R':
        mPrintf("ead ");
        if (!(loggedIn || cfg.unlogReadOk) ) 
        {
            mPrintf("- [L]og in first!\n ");
            break;
        }
        WC = ASCII;
        singleMsg = NO;
        
        while (1)
        {
            c = toupper(iChar());

            if (rwProtocol(&c))     /* user specified protocol */
            {
                singleMsg = NO;
            }
            else if (c == 'M')      /* "More" (one at a time) */
            {
                if (WC != ASCII)    /* protocol not allowed - abort */
                {
                    return YES;
                }
                mPrintf("ore ");
                singleMsg = YES;
            }
            else
            {
                break;              /* got our char */
            }
        }

        switch (c) 
        {
        case '\n':
            break;
        case 'N':
            mPrintf("ew");
            break;
        case 'G':
            mPrintf("lobal");
            break;
        case '?':
            tutorial("readflr.hlp");
            return NO;
        default:
            return YES;
        }
        doCR();
        globFloor(WC);
        break;

    case 'A':
        return floorAide();
    default:
        return YES;
    case '?':
        tutorial("floor.mnu");
        break;
    }
    return NO;
}


/*
** floorAide() - floor-oriented aide functions
*/
static int floorAide (void)
{
    label floor;
    int   idx;

    if (loggedIn && aide) 
    {
        mPrintf("ide ");
        switch (toupper(iChar())) 
        {
        case 'C':
            makeFloor();
            break;
        case 'R':
            getNormStr("ename floor; new name", floor, NAMESIZE, YES);
            if (strlen(floor) == 0)
                break;
            else if ((idx=floorExists(floor)) != ERROR && idx != thisFloor)
                mPrintf("That floor already exists!\n ");
            else
            {
                sprintf(msgBuf.mbtext,"floor [%s] renamed to [%s] by %s",
                    floorTab[thisFloor].flName,
                    floor,
                    uname());
                aideMessage(NO);
                strcpy(floorTab[thisFloor].flName, floor);
                putFloor(thisFloor);
            }
            break;
        case 'I':
            mPrintf("nvite users to [%s]\n ", floorTab[thisFloor].flName);
            getList(makeFKnown, "users to invite");
            break;
        case 'E':
            mPrintf("vict users from [%s]\n ", floorTab[thisFloor].flName);
            getList(makeFUnknown, "users to evict");
            break;
        case 'M':
            mPrintf("ove %ss to this floor\n ", room_area);
            addToFloor();
            break;
        case 'K':
            killFloor();
            break;
        case '?':
            tutorial("aideflr.mnu");
            break;
        default:
            return YES;
        }
        return NO;
    }
    return YES;
}


/*
** gotoFloor() - return an accessable room on a given floor.
*/
int gotoFloor (char genNumber)
{
    register rover;

    for (rover=0; rover<MAXROOMS; rover++)
        if ((canEnter(rover) || rover == thisRoom)
            && roomTab[rover].rtflags.floorGen == genNumber)
            return rover;

    return ERROR;
}


static char *largefill = "%23c";
static char *smallfill = "%7c";

/*
** mapHead() - show how to interpret floor map
*/
static void showMapHeader(int mask)
{
    if (!expert && (mask & l_LONG))
    {
        int i = termWidth - 2;

        tutorial("roomname.hlp");

        /*** draw separator line ***/

        while (--i)
        {
            oChar('-');
        }
    }
    mPrintf("\n ");
}


/*
** listFloor() - List accessable floors.
*/
static void listFloor (int mask)
{
    showMapHeader(mask);

    if (mask & l_NEW) 
    {
        mPrintf("\n Floors with unread messages:\n ");
        lFloor(mask & ~l_OLD);
    }
    if (mask & l_OLD)
    {
        mPrintf("\n No unseen messages in:\n ");
        lFloor(mask & ~l_NEW);
    }
}


/*
** floorMap() - show all known rooms, divided by floors
*/
void floorMap()
{
    int mask = l_OLD | l_NEW | l_LONG;

    mPrintf("\bMap of Known system:\n ");

    showMapHeader(mask);

    lFloor(mask);
}


/*
** lFloor() - print a formatted room list divided by floors
*/
static void lFloor (int mask)
{
    int     i, j, count;
    int     flag;
    int     otR = thisRoom;
    char    *fillstring;
    int     maxdots;
    
    if (termWidth > 60)
    {
        fillstring = largefill;
        maxdots = NAMESIZE;
    }
    else
    {
        fillstring = smallfill;
        maxdots = termWidth - 5;
    }

    thisRoom = -1;


    for (i = 0; i < cfg.floorCount; ++i)
    {
        if (floorTab[i].flInUse && ((mask&l_EXCL) == 0 || i != thisFloor)) 
        {
            for (flag = l_OLD, count = j = 0; j < MAXROOMS; ++j) 
            {
                if (roomTab[j].rtflags.floorGen == floorTab[i].flGen
                    && canEnter(j)) 
                {
                    ++count;
                    if (hasNew(j)) 
                    {
                        flag = l_NEW;
                        break;
                    }
                }
            }

            if (count && (mask & flag)) 
            {
                char *floorname = floorTab[i].flName;

                mPrintf("{%s}", floorname);

                if (mask & l_LONG) 
                {
                    int first = YES;

                    for (j = strlen(floorname);j < maxdots; ++j)
                    {
                        mFormat(".");   /* fill out field with dots */
                    }

                    /* mFormat(" ");*/
                    CRfill = fillstring;
                    doCR();

                    for (j = 0; j < MAXROOMS; ++j)
                    {
                        if (roomTab[j].rtflags.floorGen == floorTab[i].flGen
                          && canEnter(j))
                        {
                            char *roomname = formRoom(j, YES);

                            if (!first)
                            {    
                                if ((crtColumn + strlen(roomname) + 2)
                                    < termWidth)
                                {
                                    mPrintf("  ");  /* add to current line */
                                }
                                else
                                {
                                    doCR();        /* or go to next line */
                                }
                            }
                            mPrintf("%s", roomname);
                            first = NO;
                        }
                    }
                    CRfill = NULL;
                    mCR();
                }
                else
                {
                    mPrintf("\n ");
                }
            }
        }
    }
    thisRoom = otR;
}


/*
** floorExists() -  Does this floor exist?
*/
static int floorExists (label floor)
{
    register int i = 0;

    for (; i < cfg.floorCount; ++i)
    {
        if (floorTab[i].flInUse && stricmp(floor, floorTab[i].flName) == 0)
        {
            return i;
        }
    }
    return ERROR;
}


/*
** floorMatch() - Partial string match for floors
*/
static int floorMatch (label floor)
{
    register i = 0;
    register char *p;

    for (; i < cfg.floorCount; ++i) 
    {
        p = floorTab[i].flName;
        if (floorTab[i].flInUse && matchString(p, floor, EOS(p)))
        {
            return i;
        }
    }
    return ERROR;
}


/*
** findFloor() -- return the floor # associated with a gen number
*/
int findFloor (char gen)
{
    int i;

    for (i = 0; i < cfg.floorCount; ++i)
    {
        if (floorTab[i].flInUse && floorTab[i].flGen == gen)
        {
            return i;
        }
    }
    return ERROR;
}


/*
** fetchRoom() - make sure we have the proper floor for this room
*/
void fetchRoom (int flag)
{
    int oldFloor = thisFloor;

    if ((thisFloor = findFloor(roomBuf.rbflags.floorGen)) == ERROR) 
    {
        thisFloor = roomBuf.rbflags.floorGen = LOBBYFLOOR;
        noteRoom();
        putRoom(thisRoom);
    }

    dumpRoom(flag);
}


/*
** jumpFloor() - move to the next/last floor.
*/
static void jumpFloor (DIRECTION step)
{
    int nextroom;
    int i = thisFloor + step;

    while (i != thisFloor)
    {
        if (i >= cfg.floorCount)
        {
            i = LOBBYFLOOR;
        }
        else if (i < 0)
        {
            i = cfg.floorCount - 1;
        }

        if (floorTab[i].flInUse
            && (nextroom = gotoFloor(floorTab[i].flGen)) != ERROR) 
        {
            /* use this one */

            putRoom(thisRoom);
            getRoom(nextroom);
            break;
        }
        i += step;
    }

    thisFloor = i;
    mPrintf("[%s]\n ", floorTab[i].flName);
    dumpRoom(NO);
}


/*
** zFloor() - forget all the rooms on a floor.
*/
static void zFloor (void)
{
    if (thisFloor == LOBBYFLOOR)
    {
        mPrintf(" You can't forget [%s]!\n ", floorTab[thisFloor].flName);
    }
    else
    {

        mPrintf(" Forget [%s]- ", floorTab[thisFloor].flName);
        if (getNo(confirm)) 
        {
            int i;

            for (i = 0; i < MAXROOMS; ++i)
            {
                if (roomTab[i].rtflags.INUSE)
                {
                    if (roomTab[i].rtflags.floorGen
                        == floorTab[thisFloor].flGen) 
                    {
                        int gen = (roomTab[i].rtgen + FORGET_OFFSET) % MAXGEN;

                        logBuf.lbgen[i] = gen << GENSHIFT;
                    }
                }
            }
            gotoRoom(roomTab[0].rtname, 'R');
        }
    }
}


/*
** globFloor() Does ;R{VWXY}N
*/
static int globFloor (int WC)
{
    gotoFloor(floorTab[thisFloor].flGen);   /* goto 1st room of this floor */

    flGlobalFlag = TRUE;

    if (initWC(WC)) 
    {
        readglobal();
        if (WC != ASCII)
        {
            endWC();
        }
        usingWCprotocol = ASCII;
    }
    flGlobalFlag = FALSE;

    return TRUE;
}


/*
** makeFKnown() invite a user onto this floor
*/
static int makeFKnown (char *user)
{
    return doFWork(user, 0);
}


/*
** makeFUnknown() evict a user
*/
static int makeFUnknown (char *user)
{
    return doFWork(user, MAXGEN-1);
}


/*
** doFWork() for makeFKnown/makeFUnknown.
*/
static int doFWork (char *user, int delta)
{
    struct logBuffer person;
    int target, change=0, newVal;
    register i;

    if ((target = getnmlog(user, &person)) == ERROR)
        mPrintf("'%s' not found\n ", user);
    else
    {
        /*
         * can't invite people to Lobby>, Mail>, or Aide>
         */
        for (i=1+AIDEROOM; i<MAXROOMS; i++)
            if (roomTab[i].rtflags.INUSE
                && roomTab[i].rtflags.floorGen == floorTab[thisFloor].flGen)
            {
                newVal = (roomTab[i].rtgen + delta) % MAXGEN;
                if ((person.lbgen[i] >> GENSHIFT) != newVal) 
                {
                    change++;
                    person.lbgen[i] = (newVal << GENSHIFT) + MAXVISIT - 1;
                }
            }
        if (change)
            putLog(&person, target);
    }
    return YES;
}
