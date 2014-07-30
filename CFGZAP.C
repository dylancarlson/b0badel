/************************************************************************
 *                              cfgzap.c                                *
 *              initialize various citadel system files.                *
 *                                                                      *
 *                Copyright 1988 by David L. Parsons                    *
 *                       All Rights Reserved                            *
 *                                                                      *
 *             Permission is granted to use these sources               *
 *          for non-commercial purposes ONLY.  You may not              *
 *          include them in whole or in part in any commercial          *
 *          or shareware software; you may not redistribute             *
 *          these sources  unless you have the express written          *
 *          consent of David L. Parsons.                                *
 *                                                                      *
 * 87Sep07 orc  Created from old CONFG.C, CONFG1.C                      *
 *                                                                      *
 ************************************************************************/

#include "ctdl.h"
#include "ctdlnet.h"

/************************************************************************
 *                                                                      *
 *      zapMsgFile()            initialize ctdlmsg.sys                  *
 *      realZap()               does work of zapMsgFile()               *
 *      zapRoomFile()           erase & re-initialize ctdlroom.sys      *
 *      zapLogFile()            erases & re-initializes CTDLLOG.SYS     *
 *                                                                      *
 ************************************************************************/

/************************************************************************
 *                External variable declarations in CONFG.C             *
 ************************************************************************/

extern struct config cfg;               /* The configuration variable.  */
extern struct netBuffer netBuf;
extern struct evt_type *evtTab;
extern struct flTab *floorTab;          /* floor table.                 */
extern struct rTable roomTab[MAXROOMS]; /* RAM index of rooms           */
extern struct aRoom  roomBuf;           /* room buffer                  */
extern char sectBuf[BLKSIZE];
extern int  floorfl;                    /* file descriptor for floors   */
extern int  thisRoom;                   /* room currently in roomBuf    */
extern struct logBuffer logBuf;         /* Log buffer of a person       */
extern struct lTable   *logTab;         /* RAM index of pippuls         */
extern int  logfl;                      /* log file descriptor          */
extern int  netfl;
extern int  msgfl;              /* file descriptor for the msg file     */
extern int  thisNet;
extern struct netTable *netTab;

extern long mailCount;
extern int regen;               /* just regenerating tables? */
extern int msgZap, logZap, roomZap, floorZap;
extern char lobbyfloor[80];
extern char baseroom[80];

/************************************************************************
 *              External function definitions for CFGZAP.C              *
 ************************************************************************/

extern FILE *fopen();

#ifndef __ZTC__
unsigned toupper();
#endif

/*
 ************************
 *      zapMsgFl()      *
 ************************
 */
zapMsgFile()
{
    pathbuf fn;
    unsigned c;

    if (!msgZap) 
    {
        printf("Destroy all messages? ");
        putchar(c=toupper(getch()));
        putchar('\n');
        if (c != 'Y')
            return NO;
    }

    if (cfg.mirror)
        printf("Creating primary message file.\n");
    realZap();
    if (cfg.mirror) 
    {
        dclose(msgfl);
        ctdlfile(fn, cfg.mirrordir, "ctdlmsg.sys");
        dunlink(fn);
        if ((msgfl = dcreat(fn)) < 0)
            illegal("Can't create the secondary message file!");
        printf("Creating secondary message file.\n");
        realZap();
    }
    return YES;
}

/*
 ************************************************
 *      realZap() does work of zapMsgFile       *
 ************************************************
 */
realZap()
{
    int i;
    unsigned sect;

    /* put null message in first sector... */
    sectBuf[0] = 0xFF;  /*   \                                */
    sectBuf[1] =  '1';  /*    >  Message ID "1" MS-DOS style  */
    sectBuf[2] =   0;   /*   /                                */
    sectBuf[3] =  'M';  /*   \    Null messsage               */
    sectBuf[4] =   0;   /*   /                                */

    cfg.newest = cfg.oldest = 1L;

    cfg.catSector = 0;
    cfg.catChar = 5;

    for (i=5; i<BLKSIZE; i++)
        sectBuf[i] = 0;

    printf("\r%ld bytes to be cleared\n0",
        (long)BLKSIZE * (long)(cfg.maxMSector));
    fflush(stdout);
    crypte(sectBuf, BLKSIZE, 0);        /* encrypt      */
    if (dwrite(msgfl,sectBuf, BLKSIZE) != BLKSIZE)
        printf(": write failed\n");

    crypte(sectBuf, BLKSIZE, 0);        /* decrypt      */
    sectBuf[0] = 0;
    crypte(sectBuf, BLKSIZE, 0);        /* encrypt      */
    for (sect = 1;  sect < cfg.maxMSector;  sect++) 
    {
        printf("\r%ld", (long)(1+sect)*(long)BLKSIZE);
        fflush(stdout);
        if (dwrite(msgfl,sectBuf, BLKSIZE) != BLKSIZE)
            printf(": write failed\n");
    }
    putchar('\n');
    crypte(sectBuf, BLKSIZE, 0);        /* decrypt      */
    return YES;
}

/*
 ************************
 *      zapRoomFile()   *
 ************************
 */
zapRoomFile()
{
    int i;
    int c;

    if (roomZap == NO) 
    {
        printf("Wipe room file? ");
        putchar(c=toupper(getch()));
        putchar('\n');
        if (c != 'Y')
            return NO;
    }

    zero_struct(roomBuf);
#ifdef XYZZY
    {
        int i;

        for (i=0; i < sizeof(roomBuf); i++)
            if ( ((char*)&roomBuf)[i] )
                crashout("the universe does not work");
    }
    /*zero_struct(roomBuf.rbflags);*/
#endif

#ifdef XYZZY
    roomBuf.rbgen = 0;
    roomBuf.rbdirname[0] = roomBuf.rbmoderator[0] = 0;
    roomBuf.rbname[0] = 0;   /* unnecessary -- but I like it... */
    for (i = 0;  i < MSGSPERRM;  i++) 
    {
        roomBuf.msg[i].rbmsgNo =  0l;
        roomBuf.msg[i].rbmsgLoc = 0;
    }
#endif

    for (i = 0;  i < MAXROOMS;  i++) 
    {
        printf("clearing room %d\r", i);
        putRoom(i);
        noteRoom();
    }
    putchar('\n');

    /* Lobby> always exists -- guarantees us a place to stand! */
    thisRoom = 0;
    strcpy(roomBuf.rbname, baseroom);
    roomBuf.rbflags.PERMROOM = roomBuf.rbflags.PUBLIC = 
        roomBuf.rbflags.INUSE = YES;

    putRoom(LOBBY);
    noteRoom();

    /* Mail> is also permanent...       */
    thisRoom = MAILROOM;
    strcpy(roomBuf.rbname, "Mail");
    /* Don't bother to copy flags, they remain the same (right?)        */
    putRoom(MAILROOM);
    noteRoom();

    /* Aide> also...                    */
    thisRoom = AIDEROOM;
    strcpy(roomBuf.rbname, "Aide");
    roomBuf.rbflags.PERMROOM = roomBuf.rbflags.INUSE = YES;
    roomBuf.rbflags.PUBLIC = NO;
    putRoom(AIDEROOM);
    noteRoom();

    return YES;
}

/*
 ************************
 *      zapFloorFile()  *
 ************************
 */
zapFloorFile()
{
    int i;
    register c;
    pathbuf flf;
    struct flTab *xmalloc();

    if (floorZap == NO) 
    {
        printf("Wipe floor file? ");
        putchar(c=toupper(getch()));
        putchar('\n');
        if (c != 'Y')
            return NO;
    }

    cfg.floorCount = (1L+dseek(floorfl, 0L, 2)) / sizeof floorTab[0];
    if (cfg.floorCount < 1)
        cfg.floorCount = 1;

    floorTab = xmalloc(cfg.floorCount * sizeof floorTab[0]);

    for (i=0; i < cfg.floorCount; i++) 
    {
        zero_struct(floorTab[i]);
        if (i==0) 
        {
            floorTab[i].flInUse = YES;
            floorTab[i].flGen   = LOBBYFLOOR;
            strcpy(floorTab[i].flName, lobbyfloor[0] ? lobbyfloor : baseroom);
            printf(" 0: %s\n",floorTab[i].flName);
        }
        else
            printf("%2d:<empty>\r", i);
        putFloor(i);
    }
    return YES;
}

/*
 ************************
 *      zapLogFile()    *
 ************************
 */
zapLogFile()
{
    int i;
    int c;

    if (logZap == NO) 
    {
        printf("Wipe out log file? ");
        putchar(c=toupper(getch()));
        putchar('\n');
        if (c != 'Y')
            return NO;
    }

    /* clear RAM buffer out:                    */
    clrLBuf();
    logBuf.lbflags.L_INUSE = NO;
    for (i=0; i < NAMESIZE; i++) 
    {
        logBuf.lbname[i] = 0;
        logBuf.lbpw[i] = 0;
    }

    /* write empty buffer all over file;        */
    for (i = 0; i < cfg.MAXLOGTAB; i++) 
    {
        printf("Clearing log #%d\r", i);
        putLog(&logBuf, i);
        logTab[i].ltnewest = logBuf.lbvisit[0];
        logTab[i].ltlogSlot= i;
        logTab[i].ltnmhash = hash(logBuf.lbname);
        logTab[i].ltpwhash = hash(logBuf.lbpw  );
    }
    putchar('\n');
    return YES;
}


clrLBuf()
{
    register i;

    for (i=0; i < MSGSPERRM; i++) 
    {
        logBuf.lbslot[i] = 0 ;
        logBuf.lbId  [i] = 0L;
    }

    for (i=0; i<MAXVISIT; i++)
        logBuf.lbvisit[i] = 0L;

    for (i=0; i<MAXROOMS; i++)
        logBuf.lbgen[i] = (logBuf.lbgen[i] & ~CALLMASK) + MAXVISIT-1;
}
