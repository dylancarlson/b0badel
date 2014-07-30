/************************************************************************
 *                              cfg.c                                   *
 *      configuration program for Citadel bulletin board system.        *
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
 * 90Jan21 b0b  Changed VERSION to b0bVersion                           *
 * 88Jul03 orc  Utility functions put into cfgmisc.c                    *
 * 87Sep07 orc  Ripped out of old CONFG.C, CONFG1.C                     *
 *                                                                      *
 ************************************************************************/

#include "ctdl.h"
#include "ctdlnet.h"
#include "event.h"
#include "audit.h"
#include "poll.h"
#include "zaploop.h"
#include "cfg.h"

/************************************************************************
 *                                                                      *
 *      init()                  system startup initialization           *
 *      setvariable()           sets a switchable variable              *
 *      main()                  main controller                         *
 *      checkdepend()           check initialization dependencies       *
 *      illegal()               abort bottleneck                        *
 *      wrapup()                finishes and writes ctdlTabl.sys        *
 *                                                                      *
 ************************************************************************/

/************************************************************************
 *                External variable declarations in CONFG.C             *
 ************************************************************************/

extern struct config cfg;
extern struct evt_type *evtTab;         /* events gonna happen?         */
extern struct netBuffer netBuf;         /* network stuff                */
extern struct flTab *floorTab;          /* floor table.                 */
extern struct rTable roomTab[MAXROOMS]; /* RAM index of rooms           */
extern struct logBuffer logBuf;         /* Log buffer of a person       */
extern struct lTable   *logTab;         /* RAM index of pippuls         */
extern int floorfl;                     /* file descriptor for floors   */
extern int logfl;                       /* log file descriptor          */
extern int netfl;
extern int roomfl;                      /* room file                    */
extern struct netTable *netTab;
extern int  thisNet;
extern struct poll_t *pollTab;
extern struct zaphash *zap;
extern int thisRoom;                    /* room currently in roomBuf    */

extern struct aRoom roomBuf;            /* room buffer                  */

extern int doTime, hourOut;
#ifdef OLD_CODE
    int hs_bug;
#endif
extern int depend[];

void setvariable(char *var, int arg);

int  msgfl;                      /* file descriptor for the msg file     */
char baseroom[80] = "Lobby";
char *program  = "configure";
long mailCount = 0;
char regen    = 0;          /* just regenerating tables? */
char msgZap   = NO;
char logZap   = NO;
char roomZap  = NO;
char floorZap = NO;
char lobbyfloor[80];
int  attended = YES;

char msgLine[200];
int  lineNo = 0;        /* line in CTDLCNFG.SYS we're looking at right now.. */
char graphic=0;         /* allow ESCapes through? */
char checkloops=0;

char *strtok();
void *xmalloc();

/*
 ************************
 *      init()          *
 ************************
 */
init()
{
    int c, i;
    pathbuf netFile, msgFile, roomFile, logFile, floorFile;

    cfg.sizeLTentry = sizeof(*logTab);

    /* shave-and-a-haircut/two bits pause pattern for ringing sysop: */

    cfg.shave[0] = 40;
    cfg.shave[1] = 20;
    cfg.shave[2] = 20;
    cfg.shave[3] = 40;
    cfg.shave[4] = 80;
    cfg.shave[5] = 40;
    cfg.shave[6] =250;  /* which will sign-extend to infinity... */

    /* initialize input character-translation table:    */
    for (i = 0; i < 32; i++)
        cfg.filter[i] = 0;
    for (i = 32; i < 128; i++)
        cfg.filter[i] = i;

    if (graphic)
        cfg.filter[ESC] = ESC;


    cfg.filter[SPECIAL]= SPECIAL;
    cfg.filter[CNTRLl] = CNTRLl;
    cfg.filter[DEL]    = cfg.filter[BACKSPACE] = BACKSPACE;
    cfg.filter[XOFF]   = 'P';
    cfg.filter['\r']   = '\n';
    cfg.filter['\t']   = '\t';      /* b0b */
    cfg.filter[CNTRLO] = 'N';

    ctdlfile(netFile,  cfg.netdir, "ctdlnet.sys");
    if ((netfl = dopen(netFile, O_RDWR)) < 0) 
    {
        if ((netfl = dcreat(netFile)) < 0)
            illegal("Can't create the net file!");
        printf(">> %s not found, creating new file.\n", netFile);
    }

    ctdlfile(msgFile,  cfg.msgdir, "ctdlmsg.sys");
    /* open message file */
    if ((msgfl = dopen(msgFile, O_RDWR)) < 0) 
    {
        if (!attended)
            illegal("Can't find the message file!");
        if ((msgfl = dcreat(msgFile)) < 0)
            illegal("Can't create message file!");
        printf(">> %s not found, creating new file.\n", msgFile);
        msgZap = YES;
        regen = NO;
    }

    ctdlfile(floorFile,cfg.sysdir, "ctdlflr.sys");
    /* open floor file */
    if ((floorfl = dopen(floorFile, O_RDWR)) < 0) 
    {
        if (!attended)
            illegal("Can't find the floor file!");
        if ((floorfl = dcreat(floorFile)) < 0)
            illegal("Can't create floor file!");
        printf(">> %s not found, creating new file.\n", floorFile);
        floorZap = YES;
        regen = NO;
    }
    /* open room file */
    ctdlfile(roomFile, cfg.sysdir, "ctdlroom.sys");
    if ((roomfl = dopen(roomFile, O_RDWR)) < 0) 
    {
        if (!attended)
            illegal("Can't find room file!");
        if ((roomfl = dcreat(roomFile)) < 0)
            illegal("Can't create room file!");
        printf(">> %s not found, creating new file.\n", roomFile);
        roomZap = YES;
        regen = NO;
    }

    ctdlfile(logFile,  cfg.sysdir, "ctdllog.sys");
    /* open userlog file */
    if ((logfl = dopen(logFile, O_RDWR)) < 0) 
    {
        if (!attended)
            illegal("Can't find log file!");
        if ((logfl = dcreat(logFile)) < 0)
            illegal("Can't create log file!");
        printf(">> %s not found, creating new file.\n", logFile);
        logZap = YES;
        regen = NO;
    }

    if (floorZap || logZap || roomZap || msgZap)
        c = 'Y';
    else if (attended && !regen) 
    {
        printf("Initialize log, message, floor and/or room files?");
        c = toupper(getch());
        putch(c);
        xCR();
    }
    else                /* unattended creation... */
        return;
    if (c == 'Y') 
    {
        /*
         * each of these has an additional go/no-go interrogation
         * unless the file was just created, then it just goes...
         */
        msgZap   = zapMsgFile();
        roomZap  = zapRoomFile();
        logZap   = zapLogFile();
        floorZap = zapFloorFile();
    }
}



set_audit(x, bit)
/*
 * set_audit: Set or clear a specific audit type
 */
{
    if (x)
        cfg.call_log |= bit;
    else
        cfg.call_log &= ~bit;
}




decodeday(when)
char *when;
{
    static int daycode;
    static char *dy;
    static int i;
    extern char *_day[];

    daycode = 0x7f;
    if (when && stricmp(when, "all") != 0) 
    {
        daycode = 0;
        for (dy=strtok(when,","); dy; dy=strtok(NULL,",")) 
        {
            for (i=0; i<7; i++)
                if (stricmp(dy, _day[i]) == 0)
                    break;
            if (i<7) 
            {
                daycode |= (1<<i);
            }
            else
            {
                return 0;
            }
        }
    }
    return daycode;
}


static char ws[] = "\t ";

struct eventID
{
    char        *str;
    EVENT_TYPE  type;

} eMatch[] =
{
    {   "timeout",      EVENT_TIMEOUT       },
    {   "preemptive",   EVENT_PREEMPTIVE    },
    {   "network",      EVENT_NETWORK       },
    {   "online",       EVENT_ONLINE        },
    {   "offline",      EVENT_OFFLINE       }
};


buildevent(line)
char *line;
{
    int eHr, eMin, eFlags;
    char *eType, *eName;
    struct evt_type anEvt;
    char *when, *dy, *time, *dura, *flag;
    int dayCode, i;

    zero_struct(anEvt);
    when = NULL;

    strtok(line, ws);
    eType = strtok(NULL, ws);
    time = strtok(NULL, ws);
    if (time && !isdigit(*time)) 
    {
        when = time;
        time = strtok(NULL, ws);
    }
    dura = strtok(NULL, ws);
    eName= strtok(NULL, ws);
    flag = strtok(NULL, ws);
    if (!flag)
        syntax("Improperly formed #event");

    if ((dayCode = decodeday(when)) == 0)
        syntax("bad day code in #event");

    sscanf(time, "%d:%d", &eHr, &eMin);
    anEvt.evtTime= (eHr*60) + eMin;
    anEvt.evtDay = dayCode;
    anEvt.evtLen = atoi(dura);
    anEvt.evtFlags = eFlags = atoi(flag);
    copystring(anEvt.evtMsg, eName, NAMESIZE);

    for (i = 0; i < EVENT_EOL; ++i)
    {
        if (stricmp(eMatch[i].str, eType))
            continue;

        anEvt.evtType = eMatch[i].type;
        break;
    }

    if (i == EVENT_EOL)
    {
        syntax("bad event type!");
    }

    addevent(&anEvt);
}


buildpoll(line)
char *line;
{
    char *net, *start, *end, *when;
    int hr, min;
    int ac;
    struct poll_t temp;
    struct poll_t *tmp;
    int i;

    strtok(line, ws);
    net = strtok(NULL, ws);
    start = strtok(NULL, ws);
    end = strtok(NULL, ws);
    when = strtok(NULL, ws);

    if (net && start && end) 
    {
        temp.p_net = atoi(net);
        if (temp.p_net < 0 || temp.p_net > 31)
            syntax("bad #poll net");

        if ((ac=sscanf(start, "%d:%d", &hr, &min)) < 1)
            syntax("malformed #poll start time");
        if (ac == 1)
            min = 0;
        temp.p_start = (hr*60)+min;

        if ((ac=sscanf(end, "%d:%d", &hr, &min)) < 1)
            syntax("malformed #poll end time");
        if (ac == 1)
            min = 0;
        temp.p_end = (hr*60)+min;

        if ((temp.p_days = decodeday(when)) == 0)
            syntax("bad #poll days");

        tmp = xmalloc(sizeof(*pollTab) * (1+cfg.poll_count));

        for (i=0; i<cfg.poll_count; i++)
            copy_struct(pollTab[i], tmp[i]);

        if (pollTab != NULL)
            free(pollTab);

        pollTab = tmp;

        copy_struct(temp, pollTab[cfg.poll_count]);
        cfg.poll_count++;
    }
    else
        syntax("#poll syntax");
}


/*
 ************************************************
 * main()                                       *
 ************************************************
 */

/*
 * configur + [args]   * use existing CTDLTABL.SYS
 *  -or-
 * configur [arg]      * rebuild tables
 */

int offset = 1;
char *nextCode;

main(argc, argv)
int  argc;
char **argv;
{
    FILE *cfgfile, *pwdfile;
    char line[90], cmd[90], var[90];
    int  arg, args;
    int  i, j;
    char *p, *strchr(), *ap;
    int lock;

    extern char b0bVersion[];

    static struct evt_type timeEvent=
        {
        0, 0x7f, 0, EVENT_TIMEOUT, 0, "timeout", 480, 1 };

    lobbyfloor[0] = 0;
    zero_struct(cfg);

    printf("Configure b0badel %s\n", b0bVersion);

/*    cfg.codeBuf[0] = '.'; */    /* make a dummy directory for sysdir... */
    cfg.codeBuf[0] = 0;

    cfg.sysPassword[0] = cfg.MAXLOGTAB = cfg.evtCount =
        cfg.modemCC = cfg.call_log = 0;

    cfg.noChat     = YES;
    cfg.usa        = YES;
    cfg.local_time = 25L;
    cfg.ld_time    = 50L;
    cfg.syswidth   = 39;

    cfg.floorCount = 1;         /* will be reset later on... */

    cfg.ld_cost = 1;            /* one AMU to send mail to a ld site */
    cfg.hubcost = 2;            /* two AMU's to have it flung */
    cfg.forward_mail = YES;     /* allow mail to route through here */
    cfg.pathalias = NO;         /* don't look for path aliases */

    cfg.novice_cr = YES;        /* b0b */

    strcpy(cfg.novice, "Type '?' for menu");

    if ((cfgfile = fopen("b0badel.cfg", "r")) == NULL) 
    {
        printf("Can't open %s\n", "b0badel.cfg");
        exit(CRASH_EXIT);
    }

    offset = 1;
    nextCode = &cfg.codeBuf[offset];

    for (i=0; i<DEPENDSIZE; i++)
        depend[i] = 0;

    ++argv, --argc;
    if (argc > 0 && strcmp(*argv, "+") == 0 && readSysTab(YES)) 
    {
        regen = YES;
        ++argv, --argc;
        if (argc > 0) 
        {       /* change named variables */
            do 
            {
                ap = *argv;
                if ((p = strchr(ap, '=')) != NULL) 
                {
                    *p++ = 0;
                    arg = atoi(p);
                }
                else if (!strncmp(ap, "no", 2)) 
                {
                    arg = 0;
                    ap += 2;
                }
                else
                {
                    arg = 1;
                }
                setvariable(ap, arg);
                ++argv, --argc;

            } while (argc > 0)
                ;
            writeSysTab();
            return;
        }
    }

    if (evtTab)     /* always wipe the event table */
    {
        cfg.evtCount = 0;
        free(evtTab);
        evtTab = NULL;
    }

    if (pollTab)    /* always wipe the poll table */
    {
        cfg.poll_count = 0;
        free(pollTab);
        pollTab = NULL;
    }
    cfg.poll_delay = 5;

    while (fgets(line, 90, cfgfile)) 
    {
        cmd[0] = 0;
        lineNo++;
        strtok(line,"\n");
        strcpy(msgLine, line);

        if (sscanf(line, "%s", cmd) > 0) 
        {
            if (stricmp(cmd, "#event") == 0)
                buildevent(line);
            else if (stricmp(cmd, "#polling") == 0)
                buildpoll(line);
            else if (stricmp(cmd, "#nodeTitle") == 0) 
            {
                cfg.nodeTitle = readXstring(line, MAXCODE, YES);
                depend[NODETITLE]++;
            }
            else if (!stricmp(cmd, "#NOVICE")) 
            {
                copyString (line, cfg.novice, NOVICE_WIDTH);
            }

            else if (stricmp(cmd, "#modemSetup") == 0) 
            {
                cfg.modemSetup = readXstring(line, MAXCODE, YES);
            }
            else if (stricmp(cmd, "#reply300") == 0) 
            {
                cfg.mCCs[ONLY_300] = readXstring(line, MAXCODE, YES);
                cfg.modemCC++;
            }
            else if (stricmp(cmd, "#reply1200") == 0) 
            {
                cfg.mCCs[UPTO1200] = readXstring(line, MAXCODE, YES);
                cfg.modemCC++;
            }
            else if (stricmp(cmd, "#reply2400") == 0) 
            {
                cfg.mCCs[UPTO2400] = readXstring(line, MAXCODE, YES);
                cfg.modemCC++;
            }
            else if (stricmp(cmd, "#reply9600") == 0) 
            {
                cfg.mCCs[UPTO9600] = readXstring(line, MAXCODE, YES);
                cfg.modemCC++;
            }
            else if (stricmp(cmd, "#reply19200") == 0) 
            {
                cfg.mCCs[UPTO19200] = readXstring(line, MAXCODE, YES);
                cfg.modemCC++;
            }
            else if (stricmp(cmd, "#sysdir") == 0) 
            {
                cfg.sysdir = readDstring(line);
                depend[SYSDIR]++;
            }
            else if (stricmp(cmd, "#helpdir") == 0) 
            {
                cfg.helpdir = readDstring(line);
                depend[HELPDIR]++;
            }
            else if (stricmp(cmd, "#msgdir") == 0) 
            {
                cfg.msgdir = readDstring(line);
                depend[MSGDIR]++;
            }
            else if (stricmp(cmd, "#mirrordir") == 0) 
            {
                cfg.mirrordir = readDstring(line);
                depend[MIRRORDIR]++;
            }
            else if (stricmp(cmd, "#auditdir") == 0) 
            {
                cfg.auditdir = readDstring(line);
                depend[AUDITDIR]++;
            }
            else if (stricmp(cmd, "#netdir") == 0
                || stricmp(cmd, "#spooldir") == 0) 
            {
                cfg.netdir = readDstring(line);
                depend[NETDIR]++;
            }
            else if (stricmp(cmd, "#receiptdir") == 0) 
            {
                cfg.receiptdir = readDstring(line);
                depend[RECEIPTDIR]++;
            }
            else if (stricmp(cmd, "#shell") == 0) 
            {
                cfg.shell = readXstring(line, MAXCODE, NO);
            }
            else if (stricmp(cmd, "#sysop") == 0) 
            {
                cfg.sysopName = readXstring(line, NAMESIZE, NO);
            }
            else if (stricmp(cmd, "#archive-mail") == 0) 
            {
                cfg.archiveMail = readXstring(line, NAMESIZE, NO);
            }
            else if (stricmp(cmd, "#sysPassword") == 0) 
            {
                readString(line, cfg.sysPassword, NO);
                if (cfg.sysPassword[0]) 
                {
                    if ((pwdfile = fopen(cfg.sysPassword, "r")) == NULL)
                        printf("Can't open system password file %s\n",cfg.sysPassword);
                    else
                    {
                        fgets(cfg.sysPassword, 59, pwdfile);
                        fclose(pwdfile);
                        strtok(cfg.sysPassword,"\n");
                        if (strlen(cfg.sysPassword) < 15)
                            printf("System password is too short -- ignored\n");
                        else
                            continue;
                    }
                }
                cfg.sysPassword[0] = 0;
            }
            else if (stricmp(cmd, "#callOutSuffix") == 0) 
            {
                cfg.dialSuffix = readXstring(line, MAXCODE, YES);
                depend[NETSUFFIX]++;
            }
            else if (stricmp(cmd, "#callOutPrefix") == 0) 
            {
                cfg.dialPrefix = readXstring(line, MAXCODE, YES);
                depend[NETPREFIX]++;
            }
            else if (stricmp(cmd, "#nodeName") == 0) 
            {
                cfg.nodeName = readXstring(line, NAMESIZE, NO);
                if (NNisok(&cfg.codeBuf[cfg.nodeName])) 
                {
                    depend[NODENAME]++;
                    if (strlen(&cfg.codeBuf[cfg.nodeName]) >= NODESIZE)
                        printf("Nodename is longer than %d characters\n",
                            NODESIZE-1);
                }
                else
                    syntax("illegal character in nodeName");
            }
            else if (stricmp(cmd, "#nodeId") == 0) 
            {
                cfg.nodeId = readXstring(line, NAMESIZE, NO);
                depend[NODEID]++;
            }
            else if (stricmp(cmd, "#organization") == 0)
                cfg.organization = readXstring(line, ORGSIZE, NO);
            else if (stricmp(cmd, "#hub") == 0)
                cfg.hub = readXstring(line, NODESIZE, NO);
            else if (stricmp(cmd, "#basefloor") == 0) 
            {
                readString(line, lobbyfloor, YES);
                if (strlen(lobbyfloor) > NAMESIZE-1)
                    syntax("basefloor too long; must be less than 20 chars");
            }
            else if (stricmp(cmd, "#baseroom") == 0) 
            {
                readString(line, baseroom, YES);
                if (strlen(baseroom) > NAMESIZE-1)
                    syntax("basefloor too long; must be less than 20 chars");
            }
            else if (strcmp(cmd, "#define") == 0) 
            {
                if (sscanf(line, "%s %s %d", cmd, var, &arg) == 3)
                    setvariable(var, arg);
                else
                    syntax("bad #define setting!");
            }
            else if (cmd[0] == '#') 
            {
                /*
                 * Allow C-86 style #VAR VALUE things....
                 *
                 */
                strcpy(var, &cmd[1]);
                if (sscanf(line, "%s %d", cmd, &arg) == 2)
                    setvariable(var, arg);
                else
                    syntax("bad variable setting!");
            }
        }
    }
    checkdepend(depend);
    if (nextCode < &cfg.codeBuf[MAXCODE]) 
    {
        if (doTime) 
        {
            timeEvent.evtRel= hourOut*60;
            addevent(&timeEvent);
        }
        attended = (argc < 1);
        init();
        wrapup();
    }
    else
        illegal("codeBuf overflow!");
}


/*
 ********************************************************
 * checkdepend() -- check initialization dependencies   *
 ********************************************************
 */
checkdepend(depend)
int depend[];
{
    int i;

    if (depend[SYSDIR])
        exists(cfg.sysdir);
    else
        illegal("#sysdir not defined");

    if (depend[HELPDIR])
        exists(cfg.helpdir);
    else
    {
        puts("#helpdir NOT defined -- using #sysdir");
        cfg.helpdir = cfg.sysdir;
    }
    if (depend[MSGDIR])
        exists(cfg.msgdir);
    else
    {
        puts("#msgdir NOT defined -- using #sysdir");
        cfg.msgdir = cfg.sysdir;
    }
    if (depend[AUDITDIR])
        exists(cfg.auditdir);
    else
    {
        puts("#auditdir NOT defined -- using #sysdir");
        cfg.auditdir = cfg.sysdir;
    }


    if (depend[NODETITLE] && depend[MESSAGEK] && depend[LTABSIZE]
        && depend[SYSBAUD]) 
    {
        if (cfg.mirror)
            if (depend[MIRRORDIR])
                exists(cfg.mirrordir);
            else
                illegal("#mirrordir not defined!");

        if (depend[NETDIR])
            exists(cfg.netdir);
        else
            illegal("no #netdir");
        if (!(depend[NETPREFIX] && depend[NETSUFFIX]
            && depend[NODENAME] && depend[NODEID]))
            illegal("network not fully defined!");

        if (depend[RECEIPTDIR])
            exists(cfg.receiptdir);
        else
        {
            if (depend[NETDIR])
                puts("#receiptdir NOT defined -- using #netdir");
            else
                illegal("#receiptdir NOT defined");
            cfg.receiptdir = cfg.netdir;
        }

        if (doTime && hourOut < 0)
            illegal("HOUROUT not defined for TIMEOUT!");

        if (cfg.modemCC)
            if (!cfg.search_baud)
                puts("?-modemCC w/o searchBaud.");
            else
                for (i=0; i<cfg.sysBaud; i++)
                    if (cfg.mCCs[i] == 0)
                        illegal("baud rate detection not fully defined!");
    }
    else
        illegal("system not fully defined!");
}


/*
 ************************
 *      wrapup()        *
 ************************
 */
wrapup()
{
    if (!regen) 
    {
        if (!msgZap)
            msgInit();
        if (!roomZap)
            indexRooms();
        zapInit();
        if (!floorZap)
            floorInit();
        if (!logZap)
            logInit();
        netInit();
        if (mailCount)
            printf("%ld mail message%s\n", mailCount,(mailCount>1)?"s":"");
    }
#ifdef OLD_CODE
    if (hs_bug && cfg.probug == 0)      /* set up modem at highest speed */
        cfg.probug = cfg.sysBaud;
#endif
    printf("Wrote %d bytes to ctdltabl.sys\n", writeSysTab());
    dclose(roomfl);
    dclose(msgfl);
    dclose(logfl);
    dclose(floorfl);
    dclose(netfl);
}


mPrintf() 
{
}
atof() 
{
}
