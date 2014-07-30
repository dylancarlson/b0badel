/*{ $Workfile:   doenter.c  $
 *  $Revision:   1.14  $
 *
 *  b0badel [.]Enter and [.]Upload commands
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
#include "protocol.h"
#include "externs.h"

/* GLOBAL contents:
**
** doEnter()    the [E] and .E commands
** doUpload()   the [U] and .U commands
*/

static void readBatch       (void);
static int  tell_unlogged   (void);
static int  batch_upload    (int protocol);
static int  findRoom        (void);
static void makeRoom        (void);
static void upload          (int WCmode);

/*
** readBatch - the batch upload primitive
*/
static void readBatch (void)
{
    if (xchdir(roomBuf.rbdirname)) 
    {
        mPrintf(xBatchStr, protocol[WC]);
        /*
         * grab files -- recWCfile returns -1 on null batch header
         *               or got notbatch file when expected.
         */
        batchWC = YES;
        while (recXfile(sendARchar) == 0)
            ;
        batchWC = NO;
    }
}

/*
** tell_unlogged() - Don't let user get away with something
*/
static int tell_unlogged (void)
{
    if (!loggedIn) 
    {
        mPrintf(" - Login first!\n ");
    }
    return !loggedIn;
}

/*
** batch_upload - checks for legality, includes prompt
*/
static int batch_upload (int protocol)
{
    if (tell_unlogged())
        return NO;

    if (roomBuf.rbflags.ISDIR) 
    {
        if (roomBuf.rbflags.UPLOAD || SomeSysop()) 
        {
            mPrintf("Batch files\n ");
            if (protocol != XMODEM && protocol != YMODEM)   /* set 1k default */
                WC = YMODEM;
            readBatch();
        }
        else
        {
            mPrintf("\b- Uploads are not permitted here.\n ");
            return NO;
        }
    }
    else
    {
        mPrintf("- Goto a directory %s first!\n ", room_area);
        return NO;
    }
    return YES;
}

/*
** doEnter() - Enter something
*/
void doEnter (int prefix, char cmd)
{
    if (cmd == 'f')
    {
        mPrintf("\bUpload ");
        if (prefix == NO)
        {
            WC = XMODEM;
            mPrintf("Xmodem F");
        }
    }
    else
    {
        mPrintf("\bEnter ");
        WC = ASCII;
    }

    if (prefix) 
    {
        do 
        { 
            cmd = toupper(iChar()); 
        } 
        while (rwProtocol(&cmd))
            ;
    }
    else
    {
        oChar(cmd);
        cmd = toupper(cmd);
    }

    switch (cmd) 
    {
    case 'B':               /* Batch files */
        mPrintf ("\b");
        batch_upload (WC);
        break;

    case 'F':               /* File */
        if (tell_unlogged())
            return;
        if (!roomBuf.rbflags.ISDIR)
            mPrintf("- This is not a directory %s.\n ", room_area);

        else if (roomBuf.rbflags.UPLOAD || SomeSysop()) 
        {
            mPrintf("ile: ");
            upload(WC);
        }
        else
        {
            mPrintf("\b- Uploads not permitted in this %s\n ", room_area);
        }
        break;

    case 'C':
        mPrintf("onfiguration");
        configure();
        break;

    case 'P':               /* Password */
        mPrintf("assword");
        if (tell_unlogged())
            return;
        changepw();
        break;

    case 'N':               /* Net message */
        mPrintf ("et_m");
    case 'H':               /* Held message */
        mPrintf ("eld_m");
    case 'M':               /* Message */
        mPrintf ("essage");
    case '\n':
        if (!(loggedIn || cfg.unlogEnterOk || thisRoom == MAILROOM)) 
        {
            mPrintf(" - Login first!\n ");
            return;
        }
        if (roomBuf.rbflags.READONLY && !SomeSysop())
            mPrintf("- this %s is READ ONLY.\n ", room_area);
        else
        {
            if (thisRoom == MAILROOM && !onConsole)
                echo = CALLER;
            if (cmd == 'H') 
            {
                if (heldMessage)
                    hldMessage();
                else
                    mPrintf("- None held\n ");
            }
            else if (cmd == 'N')
                netMessage(WC);
            else
                makeMessage(WC);
            echo = BOTH;                /* restore echo... */
        }
        break;

    case 'R':               /* Room */
        mPrintf("oom");
        if (tell_unlogged())
            return;

        if (!(cfg.nonAideRoomOk || aide))
            mPrintf("- only AIDES may create new %ss\n ", room_area);
        else 
            makeRoom();

        break;

    case '?':
    default:
        tutorial("entopt.mnu");
        break;
    }
}

/*
** doUpload() - the [U] and .U command
*/
void doUpload (int prefix)
{
    char     cmd = 'U';
    int      explicit_xmodem = NO;

    extern char WC;

    mPrintf("pload ");

    if (tell_unlogged())
        return;

    WC = XMODEM;

    if (prefix) 
    {
        do 
        { 
            cmd = toupper(iChar());
            explicit_xmodem = (cmd == 'X');     /* for 128-byte batch */
        } 
        while (rwProtocol(&cmd))
            ;
    }
    else
    {
        mPrintf("Xmodem %s", expert? "F" : "File -\n\n Enter F");
        cmd = 'F';
    }

    switch (cmd) 
    {
    case 'B':               /* Batch file transfer */
        mPrintf ("\b");
        batch_upload (explicit_xmodem ? XMODEM : YMODEM);
        break;

    case '\n':
    case '\r':
        mPrintf("F");
    case 'F':
        if (!roomBuf.rbflags.ISDIR) 
        {
            mPrintf("\b - This is not a directory %s\n ", room_area);
        }
        else if (roomBuf.rbflags.UPLOAD || SomeSysop()) 
        {
            mPrintf("ilename: ");
            upload(WC);
        }
        else
        {
            mPrintf("\b - Uploads not permitted to this %s.\n ", room_area);
        }
        break;

    case 'N':
        mPrintf("et  ");
    case 'M':
        mPrintf("\bmessage");
        if (roomBuf.rbflags.READONLY && !SomeSysop())
            mPrintf("- this %s is READONLY\n ", room_area);
        else
        {
            if (thisRoom == MAILROOM && !onConsole)
                echo = CALLER;
            if (cmd == 'N')
                netMessage(WC);
            else
                makeMessage(WC);

            echo = BOTH;                /* restore echo... */
        }
        break;
    case '?':
    default:
        tutorial("upopt.hlp");
        break;
    }
}


/*
** findRoom() looks for a unused room
*/
static int findRoom()
{
    int roomRover;

    for (roomRover = 0; roomRover < MAXROOMS; ++roomRover) 
    {
        if (!roomTab[roomRover].rtflags.INUSE)
        {
            return roomRover;
        }
    }
    return ERROR;
}


/*
** makeRoom() - create a new room
*/
static void makeRoom (void)
{
    label nm, oldName;
    int  i;

    /* update lastMessage for current room: */

    logBuf.lbgen[thisRoom] = roomBuf.rbgen << GENSHIFT;

    strcpy(oldName, roomBuf.rbname);
    
    thisRoom = findRoom();

    if (thisRoom == ERROR) 
    {
        /* try and recycle an empty room */
        indexRooms();
        thisRoom = findRoom();

        if (thisRoom == ERROR) 
        {
            mPrintf("- No room!\n ");

            /* If we didn't create a room, return to the old room
            **   or to LOBBY if we recycled the old room.
            */
    abandon:    
            if (roomExists(oldName) == ERROR)
            {            
                getRoom(LOBBY);
            }
            else
            {
                getRoom(roomExists(oldName));
            }
            return;
        }
    }

    getNormStr("\n Name of new room", nm, NAMESIZE, YES);
    if (*nm == '\0')
    {
        goto abandon;
    }

    if (roomExists(nm) >= 0) 
    {
        mPrintf("A '%s' already exists.\n ", nm);
        goto abandon;
    }
    if (!expert)
    {
        tutorial("newroom.blb");
    }

    roomBuf.rbflags.INUSE = YES;
    roomBuf.rbflags.READONLY = roomBuf.rbflags.PERMROOM =
        roomBuf.rbflags.ISDIR    = roomBuf.rbflags.UPLOAD   =
        roomBuf.rbflags.DOWNLOAD = roomBuf.rbflags.ANON     =
        roomBuf.rbflags.INVITE   = roomBuf.rbflags.NETDOWNLOAD =
        roomBuf.rbflags.AUTONET  = roomBuf.rbflags.SHARED   =
        roomBuf.rbflags.ARCHIVE  = NO;
    roomBuf.rbdirname[0] = 0;

    mPrintf("Public %s", room_area);
    roomBuf.rbflags.PUBLIC = getYesNo("");

    mPrintf("'%s', a %s %s\n ", nm,
        roomBuf.rbflags.PUBLIC ? "public" : "private",
        room_area);

    if(!getNo("Install it"))
    {
        goto abandon;
    }

    strcpy(roomBuf.rbname, nm);

    for (i = 0;  i < MSGSPERRM;  i++) 
    {
        roomBuf.msg[i].rbmsgNo = 0l;     /* mark all slots empty */
        roomBuf.msg[i].rbmsgLoc = 0;
    }
    roomBuf.rbgen = (roomTab[thisRoom].rtgen + 1) % MAXGEN;
    roomBuf.rbflags.floorGen = floorTab[thisFloor].flGen;

    roomBuf.rblastNet = roomBuf.rblastLocal = 0l;

    noteRoom();                         /* index new room       */
    putRoom(thisRoom);

    /* update logBuf: */
    logBuf.lbgen[thisRoom] = roomBuf.rbgen << GENSHIFT;
    sprintf(msgBuf.mbtext, "%s> created by %s", nm, uname());
    aideMessage(NO);
    return;
}


/*
** upload() - enters a file into current directory
*/
static void upload (int WCmode)
{
    label   file;
    char    *strchr();
    int     sendARchar();
    char    successful;
    char    msg[80];
    int     len;

    getNormStr("", file, NAMESIZE, YES);

    if (strpbrk(file," :\\/")) 
    {
        mPrintf("Illegal filename.\n ");
        return;
    }
    if (strlen(file) == 0 || !xchdir(roomBuf.rbdirname))
        return;

    if (upfd = safeopen(file, "r")) 
    {           /* File already exists */
        mPrintf("A %s already exists.\n ", file);
        fclose(upfd);
    }
    else if (upfd = safeopen(file, "wb")) 
    {
        int mode = (WCmode == ASCII) ? XMODEM : WCmode;

        mPrintf(ulWaitStr, protocol[mode]);
        successful = enterfile(sendARchar, WCmode = mode);
        fclose(upfd);

        if (successful) 
        {
            zero_struct(msgBuf);
            sprintf(msgBuf.mbtext, "File \"%s\" uploaded into %s by %s.\n\n ",
                file, roomBuf.rbname, uname());

            len = strlen(msgBuf.mbtext);

            getNormStr("Enter a brief description of the file\n ",
                msgBuf.mbtext + len, 500, YES);
            
            /* write out the tag to $dir */

            putTag(file, msgBuf.mbtext[len] ?
                msgBuf.mbtext + len : "No description available");

            /* put notice in Aide room */

            aideMessage(NO);

            /* if room is DLable, put msg in room for users to notice */

            if (roomBuf.rbflags.DOWNLOAD)
            {
                storeMessage(NULL, ERROR);
            }

            zero_struct(msgBuf);
        }
        else
        {
            dunlink(file);
        }
    }
    else
    {
        mPrintf("Can't create %s!\n ", file);
    }
    homeSpace();
}


