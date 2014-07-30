/************************************************************************
 *                              roomedit.c                              *
 *                      Edit-room function & friends                    *
 *                                                                      *
 * 90Aug01 b0b  prompt for autonetting when setting up a shared room    *
 * 88Jul15 orc  modified to reduce possibility of stack overflows       *
 * 87Oct07 orc  extracted from roomb.c                                  *
 *                                                                      *
 ************************************************************************/

#include "ctdl.h"
#include "ctdlnet.h"

/************************************************************************
 *                                                                      *
 *      * roomreport()          formats a summary of the current room   *
 *      * togglemsg()           Toggle a switch and print a message     *
 *      * whosnetting()         List systems sharing this room          *
 *      * addToList()           Adds a system to a room networking list *
 *      * setBB()               Set/clear backbone setting for a node   *
 *      * takeFromList()        Removes a system from a networking list *
 *      * forgetRoom()          Cause users to forget a private room    *
 *      renameRoom()            Sysop special to rename rooms           *
 *                                                                      *
 ************************************************************************/

/************************************************************************
 *                  External variable declarations in ROOMEDIT.C        *
 ************************************************************************/

/*
extern char *on;
extern char *off;
extern char *no;
extern char *yes;
*/

char *not = "not ";
char *public_str = "public";
char *private_str = "private";
char *perm_str = "permanent";
char *temp_str = "temporary";

/************************************************************************
 *              External variable definitions for ROOMEDIT.C            *
 ************************************************************************/

extern struct aRoom     roomBuf;        /* Room buffer                  */
extern struct rTable    roomTab[];      /* RAM index                    */
extern int roomfl;                      /* Room file descriptor         */
extern struct config    cfg;            /* Other variables              */
extern struct msgB      msgBuf;         /* Message buffer               */
extern struct msgB      tempMess;       /* For held messages            */
extern struct logBuffer logBuf;         /* Person buffer                */
extern struct netBuffer netBuf;
struct netTable  *netTab;               /* RAM index of nodes           */
extern int  thisRoom;                   /* Current room                 */
extern char remoteSysop;
extern char outFlag;                    /* Output flag                  */
extern char loggedIn;                   /* Logged in?                   */
extern char haveCarrier;                /* Have carrier?                */
extern char onConsole;                  /* How about on Console?        */
extern char aide;
extern char expert;                     /* expert?                      */
extern char eventExit;
extern char heldMessage;
extern char echo;

char *EOS(), *uname();

/*
 ****************************************************************
 *      roomreport() - formats a summary of the current room    *
 ****************************************************************
 */
static
roomreport(buffer)
char *buffer;
{
    sprintf(buffer, "a %s%s%s%s%s %s %sroom",
        roomBuf.rbflags.SHARED   ? "shared, "   : "",
        roomBuf.rbflags.READONLY ? "readonly, " : "",
        roomBuf.rbflags.ARCHIVE  ? "archived, " : "",
        roomBuf.rbflags.ANON     ? "anonymous, ": "",
        roomBuf.rbflags.PUBLIC   ?  public_str
            : roomBuf.rbflags.INVITE ? "invitation-only"
            : private_str,
        roomBuf.rbflags.PERMROOM ? perm_str     : temp_str,
        roomBuf.rbflags.ISDIR    ? "directory " : "");

    if (roomBuf.rbflags.SHARED && roomBuf.rbflags.AUTONET)
        strcat(buffer," (autonetting)");

    if (roomBuf.rbflags.ISDIR)
        sprintf(EOS(buffer), " (directory %s%s%s%s)",
            roomBuf.rbdirname[0] ? roomBuf.rbdirname : "<undefined>",
            roomBuf.rbflags.DOWNLOAD ? ", readable" : "",
            roomBuf.rbflags.UPLOAD ? ", writable" : "",
            roomBuf.rbflags.NETDOWNLOAD ? ", net readable" : "");
}


/*
 ****************************************************************
 *      togglemsg() - toggle a switch and print a message       *
 ****************************************************************
 */
static int room_changed;        /* flag to put a message in Aide> */

static
togglemsg(name, variable)
char *name;
char *variable;
{
    *variable = !*variable;
    mPrintf("%s%s\n ",(*variable)?"":not, name);
    room_changed++;
}


/*
 ****************************************************************
 *      whosnetting() - write out what systems share this room  *
 ****************************************************************
 */
static
whosnetting()
{
    register i, slot;

    mPrintf("Systems sharing this room:\n ");
    for (i=0; i<cfg.netSize; i++)
    {
        if (netTab[i].ntflags.in_use) 
        {
            getNet(i);
            if ((slot=issharing(thisRoom)) != ERROR)
                mPrintf(netBuf.shared[slot].NRhub ? "%-21s(backbone)\n "
                    : "%s\n ",  netBuf.netName);
        }
    }
}


/*
 ****************************************************************
 *      addToList() - Adds a system to a room networking list   *
 ****************************************************************
 */
static
addToList(name)
char *name;
{
    int slot, i, idx;

    if (name[0] == '?') 
    {
        listnodes(NO);
        return YES;
    }

    if ((slot = netnmidx(name)) == ERROR) 
    {
        mPrintf("No '%s' known\n ", name);
        return YES;
    }

    getNet(slot);

    if (issharing(thisRoom) != ERROR)
        return YES;

    for (i=0; i<SHARED_ROOMS; i++) 
    {
        if ((idx = netBuf.shared[i].NRidx) >= 0) 
        {
            /*
             * check if this room needs to have sharing cleared.
             */
            if (netBuf.shared[i].NRgen != roomTab[idx].rtgen
                || !roomTab[idx].rtflags.SHARED) 
            {
                netBuf.shared[i].NRidx = ERROR;
                break;
            }
        }
        else
            break;
    }

    if (i >= SHARED_ROOMS) 
    {
        mPrintf("Already sharing %d rooms with %s\n ",
            SHARED_ROOMS, name);
        return YES;
    }

    netBuf.shared[i].NRidx  = thisRoom;
    netBuf.shared[i].NRlast = cfg.newest;
    netBuf.shared[i].NRgen  = roomBuf.rbgen;
    netBuf.shared[i].NRhub  = NO;
    putNet(slot);
    return YES;
}


/*
 ********************************************************
 *      setBB() - set/clear backbone setting for a node *
 ********************************************************
 */
static
setBB(name)
char *name;
{
    int slot, i;

    if ((slot = netnmidx(name)) != ERROR) 
    {
        getNet(slot);
        if ((i=issharing(thisRoom)) != ERROR) 
        {
            netBuf.shared[i].NRhub = !netBuf.shared[i].NRhub;
            putNet(slot);
            mPrintf("%s is %sbackboned to %s\n ",
                roomBuf.rbname, (netBuf.shared[i].NRhub ? "" : not),
                netBuf.netName);
        }
        else
        {
            mPrintf("This room isn't shared with `%s'\n ", netBuf.netName);
        }
    }
    else
        mPrintf("No `%s'\n ", name);
    return YES;
}


/*
 ************************************************************************
 *      takeFromList() - Removes a system from a room networking list   *
 ************************************************************************
 */
static
takeFromList(name)
char *name;
{
    int slot, i;

    if ((slot = netnmidx(name)) != ERROR) 
    {
        getNet(slot);
        if ((i=issharing(thisRoom)) != ERROR) 
        {
            netBuf.shared[i].NRidx = -1;
            putNet(slot);
        }
    }
    else
        mPrintf("No `%s'\n ", name);
    return YES;
}


/*
 ****************************************************************
 *      forgetRoom() - cause users to forget a private room.    *
 ****************************************************************
 */
static
forgetRoom(shared, toinvite)
{
    char msg[40];

    sprintf(msg, "Cause %s users to forget room", toinvite ? "all":"non-aide");
    if (getYesNo(msg)) 
    {
        if (!shared || getYesNo("This will force you to re-enter \
all of the systems sharing; are you sure?")) 
        {
            roomBuf.rbgen = (roomBuf.rbgen +1) % MAXGEN;
            logBuf.lbgen[thisRoom] = (logBuf.lbgen[thisRoom] & CALLMASK) +
                (roomBuf.rbgen << GENSHIFT);
            roomTab[thisRoom].rtgen = roomBuf.rbgen;
        }
    }
}


/*
** initialArchive() of a room
*/
static void initialArchive (char *filename)
{
    int msgRover = 0;
    long number;

    strcpy(tempMess.mbtext, msgBuf.mbtext);
    mPrintf("Doing the initial archive\n ");

    while (msgRover < MSGSPERRM)
    {
        number = roomBuf.msg[msgRover].rbmsgNo;

        if (number > cfg.oldest && number < cfg.newest) 
        {
            msgToDisk(filename, roomBuf.msg[msgRover].rbmsgNo,
                roomBuf.msg[msgRover].rbmsgLoc);

            mPrintf("%ld\n ", roomBuf.msg[msgRover].rbmsgNo);
        }
        ++msgRover; 
    }
    strcpy(msgBuf.mbtext, tempMess.mbtext);
}


/*
** doMakeWork() for makeKnown/makeUnknown.
*/
static int doMakeWork (char *user, int val)
{
    struct logBuffer person;
    int target;

    if ((target = getnmlog(user, &person)) == ERROR) 
    {
        mPrintf("'%s' not found\n ", user);
    }
    else if (LBGEN(person, thisRoom) != val)
    {
        person.lbgen[thisRoom] = (val << GENSHIFT) + MAXVISIT - 1;
        putLog(&person, target);
    }
    return YES;
}


/*
** makeKnown() - invite a user into this room
*/
static int makeKnown (char *user)
{
    return doMakeWork(user, roomBuf.rbgen);
}


/*
** makeUnknown() - evict a user
*/
static int makeUnknown (char *user)
{
    return doMakeWork(user, (roomBuf.rbgen + (MAXGEN-1)) % MAXGEN);
}


/*
** renameRoom() - and more! the room-editing menu
*/
renameRoom()
{
    int r;
    int setBB(), addToList(), takeFromList();
    char nm[NAMESIZE];
    char c, goodOne, wasDir, wasShare;
    char message[60];
    char *desc;
    char *formRoom();
    char *help;
    char *cmds;         /* commands that can be used for editing */

#define AIDEEDIT    "TMOPVCIE"
#define SYSOPEDIT   "TMOPDARWYSUVNZCIE"
#define SPECEDIT    "MDARWYSUVNCIE"
#define DIREDIT     "RWN"
#define SHAREEDIT   "UYZ"

    if (thisRoom == LOBBY || thisRoom == AIDEROOM)
    {
        if (SomeSysop()) 
        {
            help = "specedit.mnu";
            cmds = SPECEDIT;
        }
        else
        {
            goto reject;
        }
    }
    else if (thisRoom == MAILROOM) 
    {
reject:
        mPrintf("- you can't edit %s\n ", formRoom(thisRoom, NO));
        return NO;
    }
    else if (SomeSysop()) 
    {
        help = "roomedit.mnu";
        cmds = SYSOPEDIT;
    }
    else
    {
        help = "aideedit.mnu";
        cmds = AIDEEDIT;
    }

    wasDir   = roomBuf.rbflags.ISDIR;
    wasShare = roomBuf.rbflags.SHARED;
    roomBuf.rbflags.INUSE = YES;

    room_changed = 0;

    sprintf(msgBuf.mbtext,"%s, formerly ", formRoom(thisRoom, NO));
    roomreport(EOS(msgBuf.mbtext));
    desc = EOS(msgBuf.mbtext);

    mCR();
    if (!expert)
    {
        mPrintf("%s\n ", desc);
    }

    while (onLine()) 
    {
        outFlag = OUTOK;
        mPrintf("\n Edit (%s): ", roomBuf.rbname);

        if ((c=toupper(getnoecho())) == 'X') 
        {
            mPrintf("Exit room-editing\n ");
            break;
        }

        if (c == '?')
            tutorial(help);

        else if (!strchr(cmds,c)
            || (strchr(SHAREEDIT,c) && !roomBuf.rbflags.SHARED)
            || (strchr(DIREDIT,c) && !roomBuf.rbflags.ISDIR))
            whazzit();
        else
            switch (c) 
            {
            case 'T':
                mPrintf("  P)ublic, H)idden or I)nvitational ? ");
                switch (toupper(iChar())) 
                {
                case 'P':
                    mPrintf("ublic\n ");
                    roomBuf.rbflags.PUBLIC = YES;
                    roomBuf.rbflags.INVITE = NO;
                    room_changed++;
                    break;
                case 'H':
                    mPrintf("idden\n ");
                    roomBuf.rbflags.PUBLIC = NO;
                    roomBuf.rbflags.INVITE = NO;
                    room_changed++;
                    forgetRoom(wasShare, NO);
                    break;
                case 'I':
                    mPrintf("nvitation-only\n ");
                    roomBuf.rbflags.PUBLIC = NO;
                    roomBuf.rbflags.INVITE = YES;
                    room_changed++;
                    forgetRoom(wasShare, YES);
                    break;
                default:
                    whazzit();
                }
                break;

            case 'M':
                togglemsg("Read-only", &(roomBuf.rbflags.READONLY));
                break;

            case 'O':
                togglemsg("Anonymous", &(roomBuf.rbflags.ANON));
                break;

            case 'P':
                roomBuf.rbflags.PERMROOM = !roomBuf.rbflags.PERMROOM;
                mPrintf("%s room\n ", roomBuf.rbflags.PERMROOM ?
                    perm_str : temp_str);
                room_changed++;
                break;

            case 'D':
                if (roomBuf.rbflags.ISDIR = getYesNo("Directory room")) 
                {
                    if (!wasDir) 
                    {
                        wasDir =
                            roomBuf.rbflags.PERMROOM =
                            roomBuf.rbflags.UPLOAD =
                            roomBuf.rbflags.NETDOWNLOAD =
                            roomBuf.rbflags.DOWNLOAD = YES;
                    }
                    room_changed++;
                    getArea(&roomBuf);
                }
                else
                {
                    room_changed += wasDir;
                }
                break;

            case 'A':
                if (roomBuf.rbflags.ARCHIVE) 
                {
                    roomBuf.rbflags.ARCHIVE = NO;
                    mPrintf("not archived\n ");
                    room_changed++;
                }
                else if (getNo("Archive this room")) 
                {
                    getNormStr("Archive to what file", message, 60, YES);
                    if (strlen(message) && addArchiveList(thisRoom, message)) 
                    {
                        initialArchive(message);
                        roomBuf.rbflags.ARCHIVE = YES;
                        room_changed++;
                    }
                }
                break;

            case 'R':
                togglemsg("readable", &(roomBuf.rbflags.DOWNLOAD));
                break;

            case 'W':
                togglemsg("writeable", &(roomBuf.rbflags.UPLOAD));
                break;

            case 'Y':
                getList(setBB, "Set/Clear backbone status");
                break;

            case 'S':
                if (wasShare)
                {
                    if (getYesNo("Shared room"))
                    {
                        sprintf(message,
                            "Systems to add to network list for this room");

                        getList(addToList, message);
                    }
                    else
                    {
                        roomBuf.rbflags.SHARED = NO;
                        roomBuf.rbflags.AUTONET = NO;
                        room_changed++;
                    }
                }
                else if (roomBuf.rbflags.SHARED = getYesNo("Shared room"))
                {
                    roomBuf.rbflags.AUTONET = getYesNo("Auto net messages?");

                    sprintf(message, "Systems to network this room with");

                    getList(addToList, message);
                    room_changed++;
                }
                break;

            case 'U':
                getList(takeFromList, "Systems to remove from room-sharing");
                break;

            case 'V':
                roomreport(desc);
                mPrintf("Values:\n %s\n ", desc);
                if (SomeSysop() && roomBuf.rbflags.SHARED)
                    whosnetting();
                break;

            case 'N':
                togglemsg("network readable", &(roomBuf.rbflags.NETDOWNLOAD));
                break;

            case 'Z':
                togglemsg("autonet", &(roomBuf.rbflags.AUTONET) );
                break;

            case 'C':
                getNormStr("Change name to", nm, NAMESIZE, YES);
                if (strlen(nm) > 0) 
                {
                    /* do room name check */
                    if ((r = roomExists(nm)) >= 0 && r != thisRoom)
                    {
                        mPrintf("A %s already exists!\n ", nm);
                    }
                    else
                    {
                        strcpy(roomBuf.rbname, nm);   /* also in room itself  */
                        room_changed++;
                    }
                }
                break;

            case 'I':
                if (!roomBuf.rbflags.PUBLIC)
                {
                    getList(makeKnown,"Users to invite");
                    break;
                }

            case 'E':
                if (roomBuf.rbflags.PUBLIC)
                {
                    mPrintf("This is not a private room!\n ");
                }
                else
                {
                    getList(makeUnknown,"Users to evict");
                }
                break;
            }
    }
    noteRoom();
    putRoom(thisRoom);

    if (room_changed)
    {
        sprintf(desc, ", has been edited to %s, ", formRoom(thisRoom, NO));
        roomreport(EOS(msgBuf.mbtext));
        sprintf(EOS(msgBuf.mbtext),", by %s.", uname());
        aideMessage(NO);
    }
    return YES;
}
