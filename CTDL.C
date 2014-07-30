/*{ $Workfile:   ctdl.c  $
 *  $Revision:   1.21  $
 *
 *  b0badel main()
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
#include <conio.h>
#include <dos.h>
#include "ctdl.h"
#include "event.h"
#include "audit.h"
#include "protocol.h"
#include "door.h"
#include "poll.h"
#include "externs.h"

/* main() and the following GLOBAL functions:
**
** conGetYesNo()    Get Y or N from the console user
** givePrompt()     display the room-level prompt
** exitCitadel()    leave gracefully
*/

extern char checkloops, netDebug;

#define MAX_USER_ERRORS 25

typedef enum
{
    FORWARD = 1,
    BACKWARD = -1
}
DIRECTION;


static int  maximus = NO;   /* D:\MAX\MAX.EXE on '%' key */

/* prototypes for static functions */

static void crMenu      (void);
static void doChat      (int nochat);
static void doTerminal  (void);
static int  doAide      (int prefix, int cmd);
static void doForget    (int prefix);
static void doGoto      (int prefix);
static void doHelp      (int prefix);
static int  menulogin   (int prefix);
static int  pLogin      (void);
static void doLogout    (int prefix, int cmd);
static void doUngoto    (int prefix);
static void doSkip      (int prefix);
static void skipToDir   (void);
static int  doRegular   (int prefix, int c);
static void greeting    (void);
static int  getCommand  (void);
static int  xopen       (char *filename);
static void initCitadel (void);
static void doMove      (DIRECTION step);
static int  doMax       (void);


/*
** crMenu() - little menu when user hits cr at room prompt
*/
static void crMenu(void)
{
    if (thisRoom == LOBBY)
    {
        tutorial("crlobby.mnu");
    }
    else if (thisRoom == MAILROOM)
    {
        tutorial("crmail.mnu");
    }
    else if (roomTab[thisRoom].rtflags.ISDIR)
    {
        tutorial("crdir.mnu");
    }
    else
    {
        tutorial("crmsg.mnu");
    }
}


/*
** doChat() response to [C]hat request
*/
static void doChat(int nochat)
{
    int hadcarrier = gotcarrier();

    if (onConsole) 
    {
        mPrintf("\b --- System operator is online ---");
        doCR();
        connect(YES,YES,YES);
    }
    else
    {
        mPrintf("hat ");
        if (nochat) 
        {
            if (!dotutorial("nochat.blb", YES))
            {
                mPrintf("- The SysOp's not around right now.\n ");
            }
        }
        else
        {
            ringSysop();
        }
    }

    if (hadcarrier && !gotcarrier())
    {
        haveCarrier = FALSE;
        justLostCarrier = TRUE;
    }
}


/*
** conGetYesNo() - prompts console user with a Y/N question,
**                  returns TRUE on "Y"
*/
int conGetYesNo (char *message)
{
    xprintf("\r%s? (y/n): ", message);

    while (1) 
    {
        char c = tolower(getch());

        if (c == 'y' || c == 'n') 
        {
            putch(c);
            xCR();
            return c == 'y';
        }

        putch(BELL);
    }
}


/*
** doTerminal()
*/
static void doTerminal(void)
{
    xprintf(" Terminal mode\n");
    connect(conGetYesNo("Echo to modem"),
            conGetYesNo("Echo keyboard"),
            conGetYesNo("Echo CR as CRLF"));
}


/*
** doAide()
*/
static int doAide(int prefix, int cmd)
{
    label oldName;
    int   rm, i;

    if (!aide)
    {
        return NO;
    }

    mPrintf("\bAide ");

    if (prefix)
    {
        cmd = toupper(iChar());
    }
    else
    {
        oChar((char)cmd);
        cmd = toupper(cmd);
    }

    switch (cmd) 
    {
    case 'C':
        doChat(NO);
        break;
    case 'D':
        sprintf(msgBuf.mbtext, "The following empty %ss deleted by %s: ",
            room_area, uname());
        i = strlen(msgBuf.mbtext);
        mPrintf("elete empty %ss\n ", room_area);
        strcpy(oldName, roomBuf.rbname);
        indexRooms();

        getRoom( ((rm=roomExists(oldName)) != ERROR) ? rm : LOBBY);
        fetchRoom(NO);

        if (strlen(msgBuf.mbtext) > i)  /* don't create message in aide */
            aideMessage(NO);            /* unless something died        */
        break;
    case 'E':
        mPrintf("dit %s", room_area);
        renameRoom();
        break;
    case 'I':
        mPrintf("nsert message- ");
        if (thisRoom == AIDEROOM || pullMId == 0L) 
        {
            mPrintf("nope!\n ");
            break;
        }
        if (!getNo(confirm))
            break;
        note2Message(pullMId, pullMLoc);
        noteRoom();
        putRoom(thisRoom);
        sprintf(msgBuf.mbtext, "Following message inserted in %s> by %s",
            roomBuf.rbname, uname());
        aideMessage(YES);
        break;
    case 'K':
        mPrintf("ill %s- ", room_area);
        if (thisRoom==LOBBY || thisRoom==MAILROOM || thisRoom==AIDEROOM) 
        {
            mPrintf("not %s!\n ", formRoom(thisRoom, NO));
            break;
        }
        if (!getNo(confirm))
            break;

        sprintf( msgBuf.mbtext, "%s %s killed by %s",
            formRoom(thisRoom, YES), room_area, uname());
        aideMessage(NO);

        roomBuf.rbflags.INUSE = NO;
        noteRoom();
        putRoom(thisRoom);
        thisFloor = LOBBYFLOOR;
        getRoom(LOBBY);
        break;
    case 'S':
        setclock();
        break;
    case '?':
        tutorial("aide.mnu");
        break;
    default:
        whazzit();
        break;
    }
    return YES;
}


/*
** doForget() Forget a room
*/
static void doForget(int prefix)
{
    if (prefix) 
    {
        mPrintf("\b\b ");
        listRooms(l_FGT);
    }
    else
    {
        mPrintf("\bForget");
        if (thisRoom==LOBBY || thisRoom==MAILROOM || thisRoom==AIDEROOM) 
        {
            mPrintf(" - you can't forget %s!\n ", formRoom(thisRoom,NO));
            return;
        }
        mPrintf(" %s - ", roomBuf.rbname);
        if (!getNo(confirm))
            return;
        logBuf.lbgen[thisRoom] =
            (char)((roomBuf.rbgen + FORGET_OFFSET) % MAXGEN) << GENSHIFT;
        gotoRoom("", 'S');
        /* gotoRoom(roomTab[0].rtname, 'S');  */
    }
}


/*
** doGoto() Goto a room
*/
static void doGoto(int prefix)
{
    label roomName;

    outFlag = IMPERVIOUS;
    mPrintf("oto ");

    if (prefix) 
    {
        if (!expert)
        {
            mPrintf("which %s (? for list): ", room_area);
        }
        getNormStr("", roomName, NAMESIZE, YES);

        if (strcmp(roomName, "?") == 0)
        {
            listRooms(l_NEW|l_OLD);
            doCR();
            mPrintf("Goto which %s? ", room_area);
            getNormStr("", roomName, NAMESIZE, YES);
        }
        gotoRoom(roomName, 'R');
    }
    else
    {
        gotoRoom("", 'R');
    }
    if (!expert)
    {
        crMenu();
    }
}


/*
** doHelp() Ask for help
*/
static void doHelp(int prefix)
{
    label topic;

    mPrintf("elp ");

    if (prefix) 
    {
        getNormStr("", topic, 9, YES);
    }
    else
    {
        topic[0] = 0;
    }

    switch (topic[0])
    {
    case 0:
        tutorial("dohelp.hlp");
        break;

    case '?':
        tutorial("topics.hlp");
        break;
    
    default:
        strcat(topic, ".hlp");
        tutorial(topic);
    }
}


/*
** menulogin() Log somebody in
*/
static int menulogin(int prefix)
{
    label pw, lname;

    outFlag = OUTOK;

    getNormStr("\n username", lname, NAMESIZE, YES);

    if (!strcmpi(lname, "new") || (lname[0] < ' '))
    {
        return login (NULL, NULL);
    }

    echo = CALLER;

    getNormStr("password", pw, NAMESIZE, NO);

    echo = BOTH;

    return login(lname, pw);
}


/*
** pLogin() - top level login routine
*/
static int pLogin(void)
{
    int retry;

    for (retry = 0; retry < 5 && haveCarrier; ++retry)
    {
        if (menulogin(YES))
        {
            return YES;
        }
    }
    return NO;
}


/*
** doLogout()
*/
static void doLogout(int prefix, int cmd)
{

    if (loggedIn && heldMessage) 
    {
        mPrintf("\bWARNING: You have a held message!\n T");
        if (!expert)
        {
            mPrintf("\b Go back and use the .EH command.\n \n T");
        }
    }
    mPrintf("erminate ");

    if (prefix)
    {
        cmd = iChar();
    }
    else if (cmd != 'y')
    {
        oChar((char)cmd);
    }

    switch (toupper(cmd)) 
    {
    case '?':
        tutorial("logout.mnu");
        break;

    case 'Q':
        mFormat("uick\n ");
        terminate(YES, '-');
        break;

    case 'S':
        mFormat("tay\n ");
        terminate(NO, '-');
        break;

    case 'Y':
        if (prefix) 
        {
            mFormat("es\n ");   /* .ty suggested by George Fassett Jul-88 */
        }
        else    /* what is called on hotkey 'T' */
        {
            mFormat("- ");
            if (!getNo(confirm))
                break;
        }
        terminate(YES, ' ');
        break;

    default:
        whazzit();
    }
}


/*
** doUngoto()
*/
static void doUngoto(int prefix)
{
    label target;

    mPrintf("\bBack to ");
    if (prefix)
    {
        getNormStr("", target, NAMESIZE, YES);
    }
    else
    {
        doCR();
        target[0] = 0;
    }
    retRoom(target);

    if (!expert)
    {
        crMenu();
    }
}


/*
** doSkip() Skip a room
*/
static void doSkip(int prefix)
{
    label roomName;                      /* In case of ".Skip" */

    outFlag = IMPERVIOUS;
    mPrintf("kip %s> Goto ", roomTab[thisRoom].rtname);

    if (prefix) 
    {
        mPrintf("which %s? ", room_area);
        getNormStr("", roomName, NAMESIZE, YES);

        if (strcmp(roomName, "?") == 0)
        {
            tutorial("skip.hlp");
            return;
        }
    }
    else
    {
        roomName[0] = 0;
    }

    roomTab[thisRoom].rtflags.SKIP = YES;   /* Set bit */
    gotoRoom(roomName, 'S');

    if (!expert)
    {
        crMenu();
    }
}


/*
** skipToDir() - takes you to the next directory room
*/
static void skipToDir (void)
{
    int i;
    int nextroom = LOBBY;    

    outFlag = IMPERVIOUS;

    for (i = thisRoom + 1; i < MAXROOMS; ++i)
    {
        if (canEnter(i) && roomTab[i].rtflags.ISDIR)
        {
            nextroom = i;
            break;
        }
    }

    if (nextroom != thisRoom)
    {
        mPrintf(" Skip %s, Goto ", roomTab[thisRoom].rtname);

        roomTab[thisRoom].rtflags.SKIP = YES;
        pushRoom(thisRoom, logBuf.lbgen[thisRoom] & CALLMASK);
        getRoom(nextroom);
        mPrintf("%s\n ", roomBuf.rbname);
        fetchRoom(NO);
    }
    else
    {
        mPrintf(" No Directory rooms found!");
    }

    if (!expert)
    {
        crMenu();
    }
}


/*
** doRegular()
*/
static int doRegular(int prefix, int c)
{
    static int errorCount = 0;

    switch (c) 
    {
    case 'A': 
        if (prefix)
        {
            if (!doAide (prefix, 'e'))
            {
                return YES;
            }
        }
        else
        {
            tutorial ("articles.blb");     /* A for Arkles - *b0b*/
        }
        break;
    case 'B': 
        doUngoto(prefix);               
        break;
    case 'C': 
        doChat(cfg.noChat);     
        break;
    case 'D': 
        doDownload(prefix);     
        break;
    case 'P':   
        mPrintf("\bE");         /* some folks try to P) Post *b0b*/
    case 'E': 
        doEnter(prefix, 'm');   
        break;
    case 'F': 
        doRead(NO, 1, 'f');     
        break;
    case 'J': 
        mPrintf("\bG");         /* some folks try to J) Jump *b0b*/
    case 'G': 
        doGoto(prefix);         
        break;
    case 'H': 
        doHelp(prefix);         
        break;
    case 'K': 
        mPrintf("nown %ss\n ", room_area);
        listRooms(l_NEW|l_OLD);
        break;
    case 'L':
        if (loggedIn)
        {
            if (thisRoom == MAILROOM)
            {
                mPrintf("\bList users:");
                listUsers();
            }
            else
            {
                listFiles();
            }
        }
        else
        {
            mPrintf("ogin ");
            menulogin(prefix);
        }
        break;
    case 'M': 
        mPrintf("\bMail\n ");
        gotoRoom("Mail", 'R');
        break;                                  /* M) for Mail *b0b*/
    case 'N': 
        doRead(NO, 1, 'n');     
        break;
    case 'O': 
        doRead(NO, 1, 'o');     
        break;
    case 'Q': 
        mPrintf("- Read Global New\n");         /* by popular demand! *b0b*/
        singleMsg = !prefix;
        readglobal();           
        break;
    case 'R': 
        if (prefix)
            doRead(prefix, 0, 'r');
        else
            doRead(NO, 1, 'r'); 
        break;
    case 'S': 
        doSkip(prefix);         
        break;
    case 'T': 
        doLogout(prefix, 'y');  
        break;
    case 'U': 
        doUpload(prefix);               
        break;                          /* Upload command *b0b*/
    case 'X': 
        if (onConsole) 
            doTerminal();               
        break;                          /* talk to remote *b0b*/
#if 0
    case 'Y':
        if (!doAide (prefix, 'e'))      /* room edit (used to be 'A') *b0b*/
        {
            return YES;
        }
        break;
#endif
    case 'Z': 
        doForget(prefix);               
        break;
    case '*': 
        floorMap();               
        break;                          /* map of system *b0b*/
    case '>': 
        doFloor('>');           
        break;
    case '<': 
        doFloor('<');           
        break;
    case '+': 
        doMove(FORWARD);    
        break;
    case '-': 
        doMove(BACKWARD);     
        break;
    case '?':
        if (prefix)
        {
            tutorial ("summary.hlp");
        }
        else 
        {
            tutorial (termWidth < 79 ? "main40.mnu" : "mainopt.mnu");

            if (onConsole)
            {
                xprintf("** X) Terminal  ^L) Sysop  ^LM) Modem **");
            }
        }
        break;
    case '!': 
        dodoor();                       
        break;
    case '#': 
        configure();                    
        break;
    case '\\':
        skipToDir();
        break;
    case '%':
        if (doMax())
        {
            break;
        }
        /* else fall thru */

    default:
        if (++errorCount > MAX_USER_ERRORS) 
        {
            logMessage(EVIL_SIGNAL, "", 'E');
            hangup();
        }
        crMenu();       /* give a little help */
        return NO;
    }
    errorCount = 0;
    return NO;
}


/*
** greeting() Say hello to a new caller
*/
static void greeting()
{
    char c;

    thisRoom = LOBBY;
    thisFloor = LOBBYFLOOR;
    setlog();
    watchForNet = YES;
    mAbort();

    if (loggedIn)
    {
        terminate(NO, 'g');
    }

    if (!dotutorial("banner.blb",NO))
    {
        mPrintf("\n Welcome to %s ", &cfg.codeBuf[cfg.nodeTitle]);
        mPrintf("\n Running b0badel V%s ", b0bVersion);
        mPrintf("\n Written by CrT, HAW, orc, b0b and others.\n ");
    }
    watchForNet = NO;

    xprintf("\n Chat flag %sabled ", cfg.noChat ? "dis" : "en");
    xprintf("\n MODEM mode (press <Esc> for CONSOLE mode).\n ");

    if (gotcarrier() && !pLogin()) 
    {
        mPrintf("Access denied!\n ");
        hangup();
    }
}


/*
** givePrompt() - the room-level prompt
*/
void givePrompt()
{
    outFlag = IMPERVIOUS;

    if (loggedIn)
    {
        xprintf("\n(%s)", logBuf.lbname);   /* for sysop to see */
    }
    doCR();

    if (!expert)
    {
        if (loggedIn)
        {
            mPrintf("%s", cfg.novice);
            if (cfg.novice_cr)
            {
                doCR();
            }
        }
        else
        {
            mPrintf("Type L to Login, ? for menu");
            doCR();
        }
    }
    mPrintf("%s ", formRoom(thisRoom, NO));

    if (strcmp(roomBuf.rbname, roomTab[thisRoom].rtname))
    {
        crashout("room %d mismatch; rbname=<%s>, rtname=<%s>\n",
            thisRoom, roomBuf.rbname, roomTab[thisRoom].rtname);
    }
    outFlag = OUTOK;
    return;
}



static int getCommand(void)
{
    int c;

    outFlag = OUTOK;
    givePrompt();

    c = toupper(iChar());

    if (c == CNTRLl)
    {
        return doSysop();
    }
    else if (c == HUP) 
    {
        if (newCarrier) 
        {
            greeting();
            newCarrier = NO;
        }
        return NO;
    }
    else if (strchr(";'", c))       /* extended floor command */
    {
        return doFloor(iChar());
    }
    else if (strchr(" .,/", c))     /* extended "dot" command */
    {
        c = toupper(iChar());
        return doRegular(YES, c); 
    }
    else                            /* regular single key command */
    {
        return doRegular(NO, c);
    }
}



/*
** main()
*/
void main(int argc, char **argv)
{
    int recovered = 0;

    fprintf (stderr, "\n\n\
        b0badel V%s\n\
        Written by CrT, HAW, orc, b0b and others.\n", b0bVersion);

    while (argc-- > 1) 
    {
        if (!stricmp(argv[argc], "+netlog"))
        {
            logNetResults = YES;
        }
        else if (!stricmp(argv[argc], "+debug"))
        {
            Debug = YES;
        }
        else if (!stricmp(argv[argc], "+hup"))
        {
            dropDTR = NO;
        }
        else if (!stricmp(argv[argc], "+netdebug"))
        {
            netDebug = YES;
        }
        else if (!stricmp(argv[argc], "+zap"))
        {
            checkloops = YES;
        }
        else if (!stricmp(argv[argc], "+ymodem"))
        {
            netymodem = YES;
        }
        else if (!stricmp(argv[argc], "+area")) 
        {
            strcpy (room_area, "area");
            strcpy (Room_Area, "Area");
        }
        else if (!stricmp(argv[argc], "+maximus"))
        {
            maximus = YES;      /* d:\max\max.exe installed on '%' key */
        }
        else
        {
            recovered = argc;
        }
    }
    initCitadel();
    initEvent();                /* set up for next event */
    initdoor();                 /* load doorways here... */
    greeting();

    if (recovered) 
    {
        strcpy(msgBuf.mbtext, "System brought up from apparent crash.");
        aideMessage(NO);
    }

    logMessage(FIRST_IN, "", NO);

    while (!Abandon) 
    {
        if (getCommand())
        {
            whazzit();
        }
        if (justLostCarrier) 
        {
            justLostCarrier = NO;
            if (loggedIn)
            {
                terminate(YES, 'd');
            }
        }
        if (eventExit) 
        {
            doEvent();
        }
    }

    if (loggedIn)
    {
        terminate(NO, ' ');
    }

    logMessage(LAST_OUT, "", NO);

    exitCitadel(exitValue);
}


/*
** xopen() opens a file or dies.
*/
static int xopen (char *filename)
{
    register fd;

    if ((fd = dopen(filename, O_RDWR)) < 0)
    {
        crashout("No %s", filename);
    }
    return fd;
}


/*
** initCitadel()
*/
static void initCitadel (void)
{
    pathbuf sysfile;
    FILE *bfd;
    register c, i;
    char temp[20];

    if (!readSysTab(YES))           /* set up the system */
        exit(SYSOP_EXIT);           /* or bomb           */
    if (cfg.fZap)
        checkloops = YES;
    if (cfg.fNetlog)
        logNetResults = YES;
    if (cfg.fNetdeb)
        netDebug = YES;
    if (cfg.debug)
        Debug = YES;

    systemInit();
    strcpy(oldTarget, "Aide");

    /* open message files: */
    ctdlfile(sysfile, cfg.msgdir, "ctdlmsg.sys");
    msgfl = xopen(sysfile);
    ctdlfile(sysfile, cfg.sysdir, "ctdlroom.sys");
    roomfl = xopen(sysfile);
    ctdlfile(sysfile, cfg.sysdir, "ctdllog.sys");
    logfl = xopen(sysfile);
    ctdlfile(sysfile, cfg.sysdir, "ctdlflr.sys");
    floorfl = xopen(sysfile);
    ctdlfile(sysfile, cfg.netdir, "ctdlnet.sys");
    netfl = xopen(sysfile);
    if (cfg.mirror) 
    {
        ctdlfile(sysfile, cfg.mirrordir, "ctdlmsg.sys");
        msgfl2 = xopen(sysfile);
    }
    /* set up network aliases */
    ctdlfile(sysfile, cfg.netdir, "alias.sys");
    net_alias = load_alias(sysfile);

    initArchiveList();          /* archived rooms, anyone? */
    loadFloor();                /* set up the floor table  */
    getRoom(LOBBY);             /* load Lobby>             */
    modemInit();
    onConsole = NO;             /* point input at the modem*/
}

/*
** doMove() - Stonehedge type movement (+/-)
*/
static void doMove (DIRECTION step)
{
    int i = thisRoom + step;
    int lval = logBuf.lbgen[thisRoom] & CALLMASK;
    int lroom = thisRoom;

    while (i != thisRoom) 
    {
        if (i < LOBBY)
        {
            i = MAXROOMS - 1;
        }
        else if (i >= MAXROOMS)
        {
            i = LOBBY;
        }
        if (canEnter(i) && okRoom(i)) 
        {
            pushRoom(lroom, lval);
            getRoom(i);
            break;
        }
        i += step;
    }

    mPrintf("\b%s\n ", roomBuf.rbname);
    fetchRoom(NO);
}


/*
** exitCitadel()
*/
void exitCitadel (int status)
{
    hangup();                           /* punt the current user        */

    if (dropDTR)                        /* enable the phone?            */
    {
        modemClose();
    }

    systemShutdown();                   /* reset the system             */
    writeSysTab();                      /* preserve citadel tables      */
    exit(status);                       /* and return                   */
}

/*
** doMax() - invoke Maximus
*/
static int doMax(void)
{
    static unsigned no_of_drives = 0;
    static unsigned d = 4;              /* meaning the D: drive */
    int status;
    char *cmd = "MAX.EXE";
    char tail[128];
    char buffer[128];

    if (!loggedIn)  /* don't let them in without an account */
    {
        return NO;
    }

    if (!maximus)
    {
        return NO;
    }

    mPrintf(" Entering Subsystem...\n ");

    dos_setdrive(4, &no_of_drives);

    if (chdir("D:\\MAX"))
    {
        mPrintf(" couldn't change directory");
        return NO;
    }

    sprintf(tail, "\042-j%s;Y;%s\042", logBuf.lbrealname, logBuf.lbpw);

    if (!onConsole) 
    {
        sprintf(buffer, " -b%d", byteRate * 10);
        strcat(tail, buffer);
    }

    status = dosexec(cmd, tail);

#if 0   /* debugging */
    xprintf("\n dosexec(%s, %s) returned errorlevel %d", cmd, tail, status);
    xprintf("\n current directory is %s", getcwd(buffer, 128));
    doCR();
#endif

    if (onConsole)
    {
        if (dropDTR && !gotcarrier())
        {
            modemClose();
        }
    }

    return YES;
}

