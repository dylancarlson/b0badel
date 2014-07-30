/*{ $Workfile:   rooma.c  $
 *  $Revision:   1.15  $
 *
 *  room code for Citadel bulletin board system
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
#include <string.h>
#include "ctdl.h"
#include "clock.h"
#include "dirlist.h"
#include "protocol.h"
#include "externs.h"

/*  GLOBAL functions:
**
**  dumpRoom()      tells us # new messages etc
**  canEnter()      Legal to enter this room?
**  saveRmPtr()     updates the room pointers when you leave a room
**  gotoRoom()      handles the Goto command
**  hasNew()        returns TRUE if room has new messages
**  listRooms()     lists known rooms
**  partialExist()  look for a partial match with a roomname
**  pushRoom()      save current room # for Backto
**  retRoom()       guts of the Backto command
**  roomCheck()     returns slot# of named room if user has access
**  roomExists()    returns slot# of named room else ERROR
**  setlog()        initialize logBuf for logged & unlogged people
*/

/*** static prototypes ***/

static int legalMatch       (int i, label target);
static int hasForgotten     (int i);
static int noNew            (int i);
static int pickRooms        (int (*pc)(int));
static int knowRoom         (int i);


#define USTKSIZ 8                   /* allow up to 8 [B]ackto's */
static int lastStack [USTKSIZ];     /* last rooms visited       */
static int lastPtr   [USTKSIZ];     /* last msg in above room?  */
static int lPtrTab   [MAXROOMS];    /* For .Backto              */


/*
** dumpRoom() - tells us # new messages etc
*/
void dumpRoom (int tellSkipped)
{
    char hasSkipped;
    int  i, count, newCount;
    long l_read = logBuf.lbvisit[logBuf.lbgen[thisRoom]&CALLMASK];

    for (newCount=count=i=0; i < MSGSPERRM; i++) 
    {
        /*
         * Msg is still in system?  Count it.
         */
        if (roomBuf.msg[i].rbmsgNo >= cfg.oldest) 
        {
            count++;
            /*
             * don't boggle -- just checking against newest
             * as of the last time we were in this room
             */
            if (roomBuf.msg[i].rbmsgNo > l_read)
                newCount++;
        }
    }
    if (!loggedIn && thisRoom == MAILROOM)      /* Kludge for new users */
        newCount = count = 1;                   /* So they see intro.   */

    if (roomBuf.rbflags.READONLY)
        mPrintf("[readonly]\n ");
    mPrintf(" %s\n ", plural("message", (long)count));
    if ((thisRoom == MAILROOM || loggedIn) && newCount > 0)
        mPrintf(" %d new\n", newCount);

    if (tellSkipped) 
    {
        for (i = LOBBY, hasSkipped = NO; i < MAXROOMS; i++) 
        {
            if (roomTab[i].rtflags.INUSE &&
                roomTab[i].rtgen == LBGEN(logBuf,i)) 
            {
                if (roomTab[i].rtflags.SKIP)
                    hasSkipped = YES;
                else if (roomTab[i].rtlastMessage >
                    (1+logBuf.lbvisit[logBuf.lbgen[i] & CALLMASK]))
                    break;
            }
        }

        if (i == MAXROOMS && hasSkipped) 
        {
            mPrintf("\n Skipped %ss: \n ", room_area);
            for (i = LOBBY; i < MAXROOMS; i++)    /* No need to match gen #s. */
                if (roomTab[i].rtflags.SKIP && roomTab[i].rtflags.INUSE) 
                {
                    roomTab[i].rtflags.SKIP = 0;                   /* Clear. */
                    mPrintf(" %s ", formRoom(i, YES));
                }
        }
    }
}



/*
** canEnter() - ok to enter this room?
*/
int canEnter (int i)
{
    if ((!roomTab[i].rtflags.INUSE) || (i == thisRoom))
    {
        return NO;
    }

    if ((roomTab[i].rtgen == LBGEN(logBuf,i))
      || SomeSysop()
      || (aide && !(roomTab[i].rtflags.INVITE || cfg.aideforget)))
    {
        return (i != AIDEROOM || aide);
    }
    return NO;
}


/*
** saveRmPtr() - updates the room pointers when you [g] out of a room
*/
void saveRmPtr (char mode)
{
    if (mode != 'S')    /* 'S' for Skip */
    {
        if (loggedIn)
        {
            logBuf.lbgen[thisRoom] = roomBuf.rbgen << GENSHIFT;
        }
        roomTab[thisRoom].rtflags.SKIP = 0;
    }
}


/*
** gotoRoom() is the fn to travel to a new room
**   returns YES if room is not LOBBY
*/
int gotoRoom (char *nam, char mode)
{
    int  i, foundit, roomNo, nextRoom;
    int  lPtr, lRoom;
    int  no_new = NO;

    lRoom = thisRoom;
    lPtr  = (logBuf.lbgen[thisRoom] & CALLMASK);

    if (strlen(nam) == 0) 
    {
        saveRmPtr(mode);

        nextRoom = foundit = -1;

        for (i = 0; i < MAXROOMS  &&  foundit < 0; ++i)
        {
            if ( canEnter(i)
              && roomTab[i].rtlastMessage >
                   logBuf.lbvisit[ logBuf.lbgen[i] & CALLMASK ]
              && roomTab[i].rtlastMessage >= cfg.oldest
              && roomTab[i].rtflags.SKIP == 0)
            {
                if (okRoom(i))      /* see macro in ctdl.h */
                {
                    foundit = i;
                }
                else if (nextRoom < 0)
                {
                    nextRoom = i;
                }
            }
        }

        /*
         * no new rooms on this floor...
         */
        if (foundit < 0) 
        {
            if (flGlobalFlag)   /* TRUE during ;R(ead) floor command */
            {
                /* return to first room on floor */

                foundit = gotoFloor(floorTab[thisFloor].flGen);
                no_new = YES;
            }
            else if (mode == 'G')       /* return to LOBBY */ 
            {
                foundit = LOBBY;
                no_new = YES;
            }
            else if (nextRoom < 0) 
            {
                /* no new rooms whatsoever */
                foundit = LOBBY;
                no_new = YES;
            }
            else                        /* first room on next floor */
            {
                foundit = nextRoom;
            }
        }

        if (foundit != thisRoom) 
        {
            pushRoom(lRoom, lPtr);
            getRoom(foundit);
        }
        mPrintf("%s\n ", roomBuf.rbname);
    }
    else
    {
        no_new = NO;
        /* non-empty room name, so now we look for it: */
        if ( (roomNo = roomCheck(roomExists, nam)) == ERROR &&
            (roomNo = roomCheck(partialExist, nam)) == ERROR ) 
        {
            mPrintf(" no %s %s\n ", nam, room_area);
        }
        else
        {
            saveRmPtr(mode);
            pushRoom(lRoom, lPtr);
            getRoom(foundit=roomNo);

            /* if may have been unknown... if so, note it:      */
            if (LBGEN(logBuf,thisRoom) != roomBuf.rbgen)
                logBuf.lbgen[thisRoom] = (roomBuf.rbgen<<GENSHIFT)+MAXVISIT-1;
        }
    }
    fetchRoom(no_new);
    return !no_new;
}


/*
** legalMatch() - finds a legal room that matches the target
*/
static int legalMatch (int i, label target)
{

    if (canEnter(i)) 
    {
        char *endbuf = roomTab[i].rtname;

        while (*endbuf)
        {
            endbuf++;
        }

        if (matchString(roomTab[i].rtname, target, endbuf))
        {
            return YES;
        }
    }
    return NO;
}


/*
** hasForgotten() lists public forgotten rooms
*/
static int hasForgotten (int i)
{
    if (roomTab[i].rtflags.PUBLIC) 
    {
        int j = roomTab[i].rtgen - LBGEN(logBuf,i);

        return (j == FORGET_OFFSET) || (j == -FORGET_OFFSET);
    }
    return NO;
}


static char pHidden;

/*
** hasNew() - returns TRUE if room has new messages
*/
int hasNew (int i)
{
    long lastMsg = roomTab[i].rtlastMessage;
    long visit = logBuf.lbvisit[logBuf.lbgen[i] & CALLMASK] + 1;

    if (roomTab[i].rtgen == LBGEN(logBuf, i)) 
    {
        if (!roomTab[i].rtflags.PUBLIC)
        {
            ++pHidden;
        }
        return (lastMsg > cfg.oldest && lastMsg >= visit);
    }
    return NO;
}


/*
** noNew() - returns TRUE if room has NO new messages
*/
static int noNew (int i)
{
    long lastMsg = roomTab[i].rtlastMessage;
    long visit = logBuf.lbvisit[logBuf.lbgen[i] & CALLMASK] + 1;

    if (roomTab[i].rtgen == LBGEN(logBuf, i)) 
    {
        if (!roomTab[i].rtflags.PUBLIC)
        {
            ++pHidden;
        }
        return !(lastMsg > cfg.oldest && lastMsg >= visit);
    }
    return NO;
}

/*
** pickRooms() - uses a function argument to output rooms to any spec
*/
static int pickRooms(int (*pc)(int))
{
    int i, count;

    for (count = i = 0; i < MAXROOMS; ++i)
    {
        if (okRoom(i) && roomTab[i].rtflags.INUSE && (*pc)(i) ) 
        {
            ++count;
            mPrintf(" %s ", formRoom(i, YES));
        }
    }
    return count;
}


/*
** listRooms() - lists known rooms
*/
void listRooms (int mode)
{
    if (mode & l_FGT) 
    {
        mPrintf("\n Forgotten public %ss:\n ", room_area);
        pickRooms(hasForgotten);
    }
    else
    {
        pHidden = 0;
        if (mode & l_NEW) 
        {
            mPrintf("\n %ss with unread messages:\n ", Room_Area);
            pickRooms(hasNew);
        }
        if (mode & l_OLD) 
        {
            mPrintf("\n No unseen msgs in:\n ");
            pickRooms(noNew);
        }
        if (pHidden && !expert)
        {
            mPrintf("\n \n * == hidden %s\n ", room_area);
        }
    }
}


/*
** knowRoom() check to see if current person knows given room
** Return 0 if not know room, 2 if forgot room, 1 otherwise
*/
static int knowRoom (int i)
{
    register diff = (roomTab[i].rtgen - LBGEN(logBuf,i));

    if (diff == FORGET_OFFSET || diff == -FORGET_OFFSET)
    {
        return 2;
    }
    return diff ? 0 : 1;
}


/*
** partialExist() - look for a partial match with a roomname
*/
int partialExist (label target)
{
    int rover;

    for (rover = (1+thisRoom) % MAXROOMS;
      rover != thisRoom;
      rover = (1 + rover) % MAXROOMS)
    {
        if (legalMatch(rover, target))
        {
            return rover;
        }
    }
    return ERROR;
}


/*
** pushRoom() - Save room # and last message there for Backto
*/
void pushRoom (int rmId, int rmMsgPtr)
{
    register int i;

    if (lastRoom == USTKSIZ-1) 
    {
        for (i=1;i<USTKSIZ;i++) 
        {
            lastStack[i-1] = lastStack[i];
            lastPtr  [i-1] = lastPtr  [i];
        }
    }
    else
        ++lastRoom;
    lastStack[lastRoom] = rmId;
    lastPtr  [lastRoom] = rmMsgPtr;
}


/*
** retRoom() - the Backto command
*/
void retRoom (char *roomName)
{
    if (strlen(roomName) == 0) 
    {
        if (lastRoom == -1) 
        {
            mPrintf("No %s to go Backto!\n ", room_area);
            return;
        }
        getRoom(lastStack[lastRoom]);
        logBuf.lbgen[thisRoom] = (logBuf.lbgen[thisRoom] & (~CALLMASK))
            + lastPtr[lastRoom];
        --lastRoom;
    }
    else
    {
        int slot;

        if ((slot = roomCheck(roomExists, roomName)) == ERROR
          && (slot = roomCheck(partialExist, roomName)) == ERROR) 
        {
            mPrintf("No %s %s\n", roomName, room_area);
            return;
        }
        pushRoom(thisRoom, (logBuf.lbgen[thisRoom] & CALLMASK) );
        getRoom(slot);
        logBuf.lbgen[thisRoom] = lPtrTab[thisRoom];
    }
    fetchRoom(NO);
}


/*
** roomCheck() - returns slot# of named room
**  (provided you can get to it...)
*/
int roomCheck (int (*checker)(char *), char *nam)
{
    int roomNo = (*checker)(nam);

    if ( (roomNo == ERROR)
      || (roomNo == AIDEROOM && !aide))
    {
        return ERROR;
    }

    if ( (roomTab[roomNo].rtflags.INVITE)
      && (LBGEN(logBuf,roomNo) != roomTab[roomNo].rtgen)
      && (!SomeSysop()) )
    {
        return ERROR;
    }

    if (!roomTab[roomNo].rtflags.PUBLIC && !loggedIn)
    {
        return ERROR;
    }

    return roomNo;      /* it's okay! */
}


/*
** roomExists() - returns slot# of named room
*/
int roomExists (char *room)
{
    int i;

    for (i = 0; i < MAXROOMS; ++i)
    {
        if (roomTab[i].rtflags.INUSE && !stricmp(room, roomTab[i].rtname))
        {
            return i;
        }
    }
    return ERROR;
}


/*
** setlog() - initialize logBuf for logged & unlogged people
*/
void setlog (void)
{
    int g, i, j, ourSlot;

    remoteSysop = NO;           /* no sysop functions for you, boyo */
    lastRoom = -1;              /* no backing up, either */
    getRoom(thisRoom);          /* reload room pointers for you */

    if (loggedIn) 
    {
        termWidth = 0xff & logBuf.lbwidth;
        termNulls = logBuf.lbnulls;
        termLF    = logBuf.lbflags.LFMASK;
        expert    = logBuf.lbflags.EXPERT;
        aide      = logBuf.lbflags.AIDE;
        sendTime  = logBuf.lbflags.TIME;
        oldToo    = logBuf.lbflags.OLDTOO;

        /*
         * set gen on all unknown rooms  --  INUSE or no
         */
        for (i=0; i < MAXROOMS; i++) 
        {
            if (!roomTab[i].rtflags.PUBLIC) 
            {
                /*
                 * it is private -- is it unknown?
                 */
                if ((LBGEN(logBuf,i) != roomTab[i].rtgen) ||
                    (!aide && i == AIDEROOM) ) 
                {
                    /*
                     * yes -- set gen = (realgen-1) % MAXGEN
                     */
                    j = (roomTab[i].rtgen + (MAXGEN-1)) % MAXGEN;
                    logBuf.lbgen[i] = (j << GENSHIFT) + (MAXVISIT-1);
                }
            }
            else if (LBGEN(logBuf,i) != roomTab[i].rtgen)  
            {
                /*
                 * newly created public room -- remember to visit it
                 */
                j = roomTab[i].rtgen - (logBuf.lbgen[i] >> GENSHIFT);
                if (j < 0)
                    g = -j;
                else
                    g = j;
                if (g != FORGET_OFFSET)
                    logBuf.lbgen[i] = (roomTab[i].rtgen << GENSHIFT) +1;
            }
        }
        /*
         * mark mail> for new private mail.
         */
        roomTab[MAILROOM].rtlastMessage = logBuf.lbId[MSGSPERRM-1];
        /*
         * slide lbvisit array down and change lbgen entries to match
         */
        for (i=MAXVISIT-1; i > 0; i--)
            logBuf.lbvisit[i] = logBuf.lbvisit[i-1];

        for (i=0; i < MAXROOMS; i++) 
        {
            if ((logBuf.lbgen[i] & CALLMASK) < MAXVISIT-2)
                logBuf.lbgen[i]++;
            lPtrTab[i] = logBuf.lbgen[i];
        }
    }
    else
    {
        termWidth = cfg.syswidth;
        termNulls = 5;
        remoteSysop = aide = expert = oldToo = NO;
        termLF = sendTime = YES;

        zero_struct(logBuf);
        /*
         * set up logBuf so everything is new...
         */
        for (i=0; i<MAXVISIT; i++)
            logBuf.lbvisit[i] = cfg.oldest;
        /*
         * no mail for anonymous folks
         */
        roomTab[MAILROOM].rtlastMessage = cfg.newest;

        for (i = 0; i < MAXROOMS;  i++) 
        {
            g = roomTab[i].rtgen;
            if (!roomTab[i].rtflags.PUBLIC)
                g = (g+MAXGEN-1) % MAXGEN;
            lPtrTab[i] = logBuf.lbgen[i] = (g << GENSHIFT) + (MAXVISIT-1);
        }
    }
    logBuf.lbvisit[0] = cfg.newest;
}



