/************************************************************************
 *                              cfgscan.c                               *
 *              rebuild tables from existing sys files.                 *
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
 * 87Sep07 orc  extracted from CONFG.C, CONFG1.C                        *
 *                                                                      *
 ************************************************************************/

#include <stdlib.h>
#ifdef __ZTC__
#include <sys\stat.h>
#include <io.h>
#endif
#include "ctdl.h"
#include "library.h"
#include "ctdlnet.h"
#include "event.h"
#include "zaploop.h"


/* GLOBAL Contents:
**
**  msgInit()       sets up cfg.catChar, catSect etc.
**  indexRooms()    build RAM index to ctdlroom.sys
**  logInit()       builds the RAM index to CTDLLOG.SYS
**  sortLog()       sort CTDLLOG by time since last call
*/

/************************************************************************
 *                External variable declarations in CONFG.C             *
 ************************************************************************/

extern struct config cfg;       /* The configuration variable           */
extern struct msgB msgBuf;      /* The -sole- message buffer            */

extern struct netBuffer netBuf;
extern struct evt_type *evtTab;
extern struct flTab *floorTab;  /* floor table.                         */

static int thisChar;            /* next char in sectBuf                 */
static int oldChar;             /* Old value of thisChar                */

static unsigned thisSector;     /* next sector in msgfl                 */
static unsigned oldSector;      /* Old value of thisSector              */

char sectBuf[BLKSIZE];          /* msgfl sector buffer (simulated)      */

extern struct rTable roomTab[]; /* RAM index of rooms           */
extern struct aRoom  roomBuf;   /* room buffer                  */
extern struct logBuffer logBuf; /* Log buffer of a person       */
extern struct lTable   *logTab; /* RAM index of pippuls         */
extern int floorfl;             /* file descriptor for floors   */
extern int thisRoom;            /* room currently in roomBuf    */
extern int logfl;               /* log file descriptor          */
extern int msgfl;               /* file descriptor for the msg file     */
extern int netfl;
extern int roomfl;
extern int thisNet;
extern struct netTable *netTab;

extern long mailCount;
extern int  regen;              /* just regenerating tables? */
extern int  msgZap, logZap, roomZap, floorZap;
extern char lobbyfloor[80];

void *xmalloc();

/*** prototypes for static functions ***/

static int msgseek (int sector, int byte);
static void getmessage (void);
static unsigned char getmsgchar (void);
static void getmsgstr (char *dest, unsigned int lim);

/*
** getmessage() reads a message off disk into RAM.
**      a previous call to setUp has specified the message.
*/
static void getmessage (void)
{
    register unsigned char c;

    msgBuf.mbid[0] = msgBuf.mbroom[0] = 0;

    while (getmsgchar() != 0xff)
        ;

    getmsgstr(msgBuf.mbid, NAMESIZE);

    while ((c = getmsgchar()) != 'M') 
    {
        if (c == 'R')
        {
            getmsgstr(msgBuf.mbroom, NAMESIZE);
        }
        else
        {
            while (getmsgchar())        /* read til we find a nul */
                ;
        }
    }

    if (stricmp(msgBuf.mbroom, "Mail") == 0)
    {
        mailCount++;
    }
}


/*
** getmsgchar() returns sequential chars from message on disk
*/
static unsigned char getmsgchar (void)
{
    unsigned char gotten;

    oldChar = thisChar;
    oldSector = thisSector;

    gotten = sectBuf[thisChar];

    if ((thisChar = (1 + thisChar) % BLKSIZE) == 0) 
    {
        /* time to read next sector in: */

        thisSector = (1 + thisSector) % cfg.maxMSector;

        dseek(msgfl, (long)(thisSector) * (long)(BLKSIZE), 0);

        if (dread(msgfl, sectBuf, BLKSIZE) != BLKSIZE)
        {
            crashout("nextMsgChar-read fail\n");
        }
        crypte(sectBuf, BLKSIZE, 0);
    }
    return gotten;
}


/*
** getmsgstr() - reads a string from message.buf
*/
static void getmsgstr (char *dest, unsigned int lim)
{
    register char c;

    while ((c = getmsgchar()) != 0)
    {
        if (lim > 0) 
        {
            --lim;
            *dest++ = c;
        }
    }
    *dest = 0;
}


/*
** msgInit() - sets up lowId, highId, cfg.catSector and cfg.catChar,
**             by scanning over ctdlmsg.sys
*/
msgInit()
{
    long firstmsg, currentmsg;
    long lowest, highest;

    msgseek(0, 0);
    getmessage();

    /* get the ID# */
    sscanf(msgBuf.mbid, "%ld", &firstmsg);
    printf("file starts with msg %ld\n", firstmsg);

    cfg.newest = cfg.oldest = firstmsg;

    lowest  = firstmsg - cfg.maxMSector;     /* lowest possible msg # */
    highest = firstmsg + cfg.maxMSector;     /* highest possible msg # */

    cfg.catSector = thisSector;
    cfg.catChar = thisChar;

    for (getmessage(); sscanf(msgBuf.mbid, "%ld", &currentmsg), currentmsg != firstmsg;
        getmessage()) 
    {
        printf("found msg #%lu    \r", currentmsg);

        /* find highest and lowest message IDs: */

        if ((currentmsg < cfg.oldest) && (currentmsg >= lowest))
        {
            cfg.oldest = currentmsg;
            printf("\ncfg.oldest is now #%ld\n", cfg.oldest);
        }
        if (currentmsg > cfg.newest) 
        {
            cfg.newest = currentmsg;

            /* read rest of message in and remember where it ends,      */
            /* in case it turns out to be the last message              */
            /* in which case, that's where to start writing next message*/

            while (getmsgchar())
                ;

            cfg.catSector = thisSector;
            cfg.catChar = thisChar;
        }
    }
    printf("\nmessages run from %ld to %ld\n", cfg.oldest, cfg.newest);
}


/*
** msgseek() sets location to begin reading message from
*/
static int msgseek (int sector, int byte)
{
    if (sector >= cfg.maxMSector)
    {
        crashout("msgseek: bad sector=%d, byte=%d", sector, byte);
    }
    thisChar = byte;
    thisSector = sector;

    dseek(msgfl, (long)(BLKSIZE) * (long)(sector), 0);

    if (dread(msgfl,sectBuf, BLKSIZE) != BLKSIZE)
    {
        printf("?msgseek read fail");
    }
    crypte(sectBuf, BLKSIZE, 0);
}


addevent(evt)
struct evt_type *evt;
{
    struct evt_type *tmp;
    int i;

    if (cfg.evtCount < 0)
    {
        cfg.evtCount = 0;   /* safety */
    }

    tmp = xmalloc(sizeof(*evtTab) * (1+cfg.evtCount));

    for (i=0; i<cfg.evtCount; i++)
    {
        copy_struct(evtTab[i], tmp[i]);
    }

    if (evtTab != NULL)
    {
        free(evtTab);
    }

    evtTab = tmp;
    copy_struct(*evt, evtTab[cfg.evtCount]);
    cfg.evtCount++;
}


/*
** indexRooms() -- build RAM index to CTDLROOM.SYS, displays stats.
*/
indexRooms()
{
    int goodroom, m, n, roomCount, slot;
    int i;

    roomCount = 0;

    for (slot = 0;  slot < MAXROOMS;  slot++) 
    {
        getRoom(slot);
        if (slot == LOBBY) 
        {
            /*
             * lobby is ALWAYS here...
             */
            roomBuf.rbflags.INVITE = NO;
            roomBuf.rbflags.PERMROOM = roomBuf.rbflags.PUBLIC =
                roomBuf.rbflags.INUSE = YES;
        }
        else if (roomBuf.rbflags.INUSE) 
        {
            if (msgZap)
            {
                roomBuf.rblastMessage =
                    roomBuf.rblastNet =
                    roomBuf.rblastLocal = 0L;
            }

            for (m = 0, n = 0, goodroom = NO; m < MSGSPERRM; ++m)
            {
                if (msgZap)
                {
                    roomBuf.msg[n].rbmsgNo = 0L;
                    roomBuf.msg[n].rbmsgLoc= 0;
                    ++n;
                }
                else if ((roomBuf.msg[m].rbmsgNo >= cfg.oldest)
                      && (roomBuf.msg[m].rbmsgNo <= cfg.newest))
                {
                    goodroom = YES;
                    roomBuf.msg[n].rbmsgNo = roomBuf.msg[m].rbmsgNo;
                    roomBuf.msg[n].rbmsgLoc= roomBuf.msg[m].rbmsgLoc;
                    ++n;
                }
            }

            while (n < MSGSPERRM)
            {
                roomBuf.msg[n].rbmsgNo = 0L;
                roomBuf.msg[n].rbmsgLoc= 0;
                ++n;
            }

            roomBuf.rbflags.INUSE = (goodroom || roomBuf.rbflags.PERMROOM);

            if (msgZap || roomBuf.rblastNet > cfg.newest)
            {
                roomBuf.rblastNet = 0L;
            }
            if (msgZap || roomBuf.rblastLocal > cfg.newest)
            {
                roomBuf.rblastLocal = 0L;
            }

            if (roomBuf.rbflags.INUSE)
            {
                roomCount++;
            }
            else
            {
                zero_struct(roomBuf.rbflags);
            }

            if (floorZap)
            {
                roomBuf.rbflags.floorGen = LOBBY;
            }
        }
        if (roomBuf.rbflags.INUSE)
        {
            printf("room %3d: %s\n", slot, roomBuf.rbname);
        }
        noteRoom();
        putRoom(slot);
    }
    puts("--------");
}


/*
 ****************************************
 *      logInit() indexes ctdllog.sys   *
 ****************************************
 */
logInit()
{
    int i,j;
    int logSort();
    int count = 0;
    int mod;
    int loop;

    if (dseek(logfl,0L,0) != 0L)
        illegal("Rewinding logfl failed!");
    /* clear logTab */
    for (i = 0; i < cfg.MAXLOGTAB; i++)
        logTab[i].ltnewest = 0l;

    /* load logTab: */
    for (loop = 0;  loop < cfg.MAXLOGTAB;  loop++) 
    {
        getLog(&logBuf, loop);

        if ((mod = msgZap) != 0) 
        {
            clrLBuf();
            logTab[loop].ltnewest = 1L;
        }
        else
        {
            logTab[loop].ltnewest = logBuf.lbvisit[0];
        }

        logTab[loop].ltlogSlot = loop;

        if (logBuf.lbflags.L_INUSE) 
        {
            logTab[loop].ltnmhash = hash(logBuf.lbname);
            logTab[loop].ltpwhash = hash(logBuf.lbpw  );
            /*
             * destroy bad mail.
             */
            if (!msgZap)
                for (j=0; j<MSGSPERRM; j++)
                    if (logBuf.lbId[j] > cfg.newest) 
                    {
                        logBuf.lbId[j] = 0L;
                        logBuf.lbslot[j] = 0;
                        mod++;
                    }
            if (mod)
                putLog(&logBuf, loop);
            printf("log %3d: %s\n", loop, logBuf.lbname);
        }
        else
        {
            logTab[loop].ltnmhash = 0;
            logTab[loop].ltpwhash = 0;
        }

    }
    puts("--------");
    qsort(logTab, cfg.MAXLOGTAB, sizeof logTab[0], logSort);
}


/*
 ************************************************
 *      logSort() Sorts 2 entries in logTab     *
 ************************************************
 */
int logSort(s1, s2)
struct lTable *s1, *s2;
{
    if (s1->ltnmhash == 0 && s2->ltnmhash == 0)
        return 0;
    if (s1->ltnmhash == 0 && s2->ltnmhash != 0)
        return 1;
    if (s1->ltnmhash != 0 && s2->ltnmhash == 0)
        return -1;
    if (s1->ltnewest < s2->ltnewest)
        return 1;
    if (s1->ltnewest > s2->ltnewest)
        return -1;
    return 0;
}

/*
 ********************************************************
 *      floorInit()     Clean up the floor table.       *
 ********************************************************
 */
floorInit()
{
    register i;

    cfg.floorCount = (1L+dseek(floorfl, 0L, 2)) / sizeof floorTab[0];
    loadFloor();
    for (i=0; i < cfg.floorCount; i++)
        if (floorTab[i].flInUse)
            printf("floor %3d: %s\n", i, floorTab[i].flName);
    puts("--------");
}


/*
 ************************************************
 *      netInit() Initialize RAM index for net  *
 ************************************************
 */
netInit()
{
    label temp;
    register i, m, room;

    cfg.netSize = (int) ((1L + dseek(netfl, 0l, 2)) / sizeof (netBuf));

    if (cfg.netSize)
    {
        netTab = xmalloc(sizeof (*netTab) * cfg.netSize);

        for (i=0; i<cfg.netSize; i++) 
        {
            getNet(i);
            if (strlen(netBuf.netName) > NAMESIZE)
                netBuf.nbflags.in_use = NO;
            if (netBuf.nbflags.in_use) 
            {
                for (m=0; m<SHARED_ROOMS; m++) 
                {
                    if (msgZap)
                        netBuf.shared[m].NRlast = 0L;
                    room=netBuf.shared[m].NRidx;
                    if (room >= 0 && roomTab[room].rtgen != netBuf.shared[m].NRgen)
                        netBuf.shared[m].NRidx = -1;
                }
                normID(netBuf.netId, temp);
                netTab[i].ntnmhash = hash(netBuf.netName);
                netTab[i].ntidhash = hash(temp);
                putNet(i);
                printf("System %3d: %-12s\n", i, netBuf.netName);
            }
        }
        puts("--------");
    }
}


/*
 ************************************************
 *      getx() - load in a zaptable entry       *
 ************************************************
 */
static int zapcur = ERROR;
static struct zaploop zapnode;
static int zapfl;

static void getx (int i)
{
    zapcur = i;
    dseek(zapfl, zapcur * sizeof zapnode, 0);
    dread(zapfl, &zapnode, sizeof(zapnode));
}


static void putx (int i)
{
    dseek(zapfl, zapcur * sizeof zapnode, 0);
    dwrite(zapfl, &zapnode, sizeof(zapnode));
}


/*
 ********************************************************
 *      zapInit() -- zero the zap table.                *
 ********************************************************
 */
void zapInit (void)
{
    pathbuf zapfile;
    int zh, j, idx, zfsize;
    int *linktab;

    ctdlfile(zapfile, cfg.netdir, "ctdlloop.zap");
    cfg.zap_count = 0;

    zap = NULL;

#ifdef __ZTC__

    zapfl = open(zapfile, 2);

    if (zapfl == -1)
    {
        if ((zapfl = creat(zapfile, S_IREAD | S_IWRITE)) == -1)
        {
            crashout("cannot create zaptable %s\n", zapfile);
        }
        else
        {
            close(zapfl);
        }
        return;
    }

#else

    if ((zapfl = dopen(zapfile, O_RDWR)) < 0) 
    {
        if ((zapfl=dcreat(zapfile)) < 0)
        {
            crashout("cannot create zaptable %s\n", zapfile);
        }
        else
        {
            dclose(zapfl);
        }
        return;
    }
#endif

    zap = (struct zaphash *)malloc(1);

    zfsize = (1L+dseek(zapfl, 0L, SEEK_END)) / sizeof(zapnode);

    printf("zfsize = %d\n", zfsize);

    if (zfsize)
    {
        linktab = xmalloc(zfsize * sizeof(*linktab));
    }
    else
    {
        dclose(zapfl);
        return;
    }

    for (idx=0; idx<zfsize; idx++)
        linktab[idx] = 0;
    zero_struct(zapnode);

    for (idx=zfsize-1; idx >= 0;) 
    {
        getx(idx);
        linktab[idx] = 1;
        zh = hash(zapnode.lxaddr);

        zap = realloc(zap, (1+cfg.zap_count) * sizeof zap[0]);

        if (zap == NULL) 
        {
            printf("Out of memory reallocating zaphash!\n");
            cfg.zap_count = 0;
            free(linktab);
            break;
        }
        zap[cfg.zap_count].zhash = zh;
        zap[cfg.zap_count].zbucket = idx;
        printf("%3d:%-15s%d", cfg.zap_count, zapnode.lxaddr, idx);

        while (zapnode.lxchain >= 0) 
        {
            if (linktab[zapnode.lxchain]) 
            {
                printf("-chain rebuild collision at %d\n", zapnode.lxchain);
                zap = NULL;
                cfg.zap_count = 0;
                free(linktab);
                return;
            }
            linktab[zapnode.lxchain] = 1;
            printf("->%d", zapnode.lxchain);
            getx(zapnode.lxchain);
        }
        putchar('\n');
        cfg.zap_count++;
        while (idx >= 0 && linktab[idx])
            --idx;
    }
    printf("%d zaptable entries\n", cfg.zap_count);
    puts("--------");
    free(linktab);
    dclose(zapfl);
}


/*
 ****************************************
 *      normID() Normalizes a node id.  *
 ****************************************
 */
normID(source, dest)
label source, dest;
{
    while (!isalpha(*source) && *source)
        source++;
    if (!*source)
        return NO;
    *dest++ = toupper(*source++);
    while (!isalpha(*source) && *source)
        source++;
    if (!*source)
        return NO;
    *dest++ = toupper(*source++);
    while (*source) 
    {
        if (isdigit(*source))
            *dest++ = *source;
        source++;
    }
    *dest = 0;
    return YES;
}

