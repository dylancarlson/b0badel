/*{ $Workfile:   ctdl.h  $
 *  $Revision:   1.13  $
 *
 *  Header file for all b0badel C files
}*/

#ifndef CTDL_H
#define CTDL_H

#include <stdio.h>
#include <ctype.h>

#ifndef MSDOS

    #define MSDOS 1

#endif
#ifdef __ZTC__

    #define strcmpi(p,q)    strcmpl(p,q)
    #define stricmp(p,q)    strcmpl(p,q)

#endif

#include "sysdep.h"

#define NODESIZE        20              /* node name for networking     */
#define NAMESIZE        30              /* length of room names         */
                                        /* was 20 in STadel -b0b-       */
#define PATHSIZE        100             /* maximum length of paths      */
#define ADDRSIZE        128             /* and net addresses            */
#define ORGSIZE         40              /* and org field                */

typedef char label      [NAMESIZE];     /* Semi-generic                 */
typedef char pathbuf    [PATHSIZE];
typedef char NA         [ADDRSIZE];
typedef char CHAR;                      /* alignment filler */

#define YES     1
#define NO      0
#define TRUE    1
#define FALSE   0
#define ERROR   (-1)

/* ASCII characters: */
#define HUP       0
#define SOH       1
#define STX       2
#define CNTRLC    3
#define EOT       4
#define ACK       6
#define BELL      7
#define BACKSPACE 8
#define TAB       9
#define NEWLINE  10
#define CNTRLl   12
#define FORMFEED 12
#define CRETURN  13
#define CNTRLO   15
#define DLE      16
#define SYN      22
#define XOFF     19
#define XON      17
#define NAK      21
#define CAN      24
#define CNTRLZ   26
#define ESC      27
#define SPECIAL ESC
#define DEL     127

/*
 * special system rooms that will always be there.
 */
#define LOBBY           0       /* Lobby> is >always< room 0.           */
#define MAILROOM        1       /* Mail>  is >always< room 1.           */
#define AIDEROOM        2       /* Aide>  is >always< room 2.           */

#define MSGSPERRM       75      /* messages per room                    */
                                /* was 58 in STadel -b0b-               */

#define SECTSIZE        128     /* Size of a Xmodem sector              */
#define YMSECTSIZE      1024    /* Size of a Ymodem sector              */

#define BLKSIZE         512     /* size of a messagebase sector         */
#define MAXCODE         400     /* codebuffer inside cfg                */

#define MAXROOMS        48      /* number of rooms allowed in system    */
#define SHARED_ROOMS    16      /* # rooms that can be shared with a    */
                                /* single system                        */
/*
 * modem stuff:
 *
 * If you change the number of baud rates supported here, you need to
 * modify the mCCs strings in struct config, findBaud(), and grabCC()
 * in SYSDEP.C, and setBaud() in LIBDEP.C
 */

#define NUMBAUDS        5

#define ONLY_300        0        /* 300 baud only        */
#define UPTO1200        1        /* +1200 baud           */
#define UPTO2400        2        /* +2400 baud           */
#define UPTO9600        3        /* +9600 baud           */
#define UPTO19200       4        /* +19200 baud          */

#define NOVICE_WIDTH    39       /* width of the #novice prompt string */

/************************************************************************
 * Citadel programs use readSysTab() and writeSysTab() to write an      *
 * image of the external variables in RAM to disk, and later restore    *
 * it.  The image is stored in CTDLTABL.SYS .  If CTDLTABL.SYS is lost, *
 * CONFIGUR will automatically reconstruct it the hard way when invoked,*
 * and write a new CTDLTABL.SYS out when finished.  CTDLTABL.SYS is     *
 * always destroyed after reading, to minimize the possibility of       *
 * reading an out-of-date version.  In general, the technique works     *
 * well and saves time and head-banging on bootup.  You should,         *
 * however, note carefully the following caution:                       *
 *  o  Whenever you change the declarations in Ctdl.h you should:       *
 *   -->  destroy the current CTDLTABL.SYS file                         *
 *   -->  recompile and reload all citadel programs which access        *
 *        CTDLTABL.SYS -- currently CITADEL.TOS & CONFIGUR.TOS          *
 *   -->  use CONFIGUR.TOS to build a new CTDLTABL.SYS file             *
 *                                                                      *
 * If you ignore these warnings, little pixies will prick you in your   *
 * sleep for the rest of your life.                                     *
 ************************************************************************/

struct config {
    int  maxMSector;            /* Max # of sectors (simulated)         */
    int  catChar;               /* Location of next write in msg file   */
    int  catSector;

    long oldest;                /* 32-bit ID# of first message in system*/
    long newest;                /* 32-bit ID# of last  message in system*/

    int  cryptSeed;             /* xor crypt offset                     */
    int  sizeLTentry;           /* contains size of a logTab entry      */
    int  MAXLOGTAB;             /* number of log entries supported      */

    /*
     * system identification for users & networking.
     */
    int  nodeName;              /* name for networking                  */
    int  nodeTitle;             /* name displayed for callers           */
    int  nodeId;                /* phone number/network id              */
    /*
     * system directory offsets in codeBuf
     */
    int  sysdir;                /* where we keep various system files   */
    int  helpdir;               /* .hlp, .mnu, .blb files               */
    int  msgdir;                /* primary messagefile                  */
    char mirror;                /* Auto backup of message files?        */
    CHAR dum1;
    int  mirrordir;             /* directory to put autobackup msgfile  */
    int  netdir;                /* where net files are found            */
    int  auditdir;              /* where audit files are found          */
    int  receiptdir;            /* where sendfile stuff goes            */
    int  recSize;               /* how many K we can recieve.           */

    char unlogEnterOk;          /* TRUE if OK to enter messages anon    */
    char unlogReadOk;           /* TRUE if unlogged folks can read mess */
    char unlogLoginOk;          /* TRUE if spontan. new accounts ok.    */
    char nonAideRoomOk;         /* TRUE general folks can make rooms    */
    char noMail;                /* TRUE if mail is not allowed          */
    char allNet;                /* TRUE if all users get net privvies   */
    char paranoid;              /* force remote callers to login        */
    char aideforget;            /* aides can forget rooms               */

    char usa;                   /* is this system in north america?     */
    char call_log;              /* if > 0, keep a call-log.             */
    char pathalias;             /* do intelligent mail routing          */
    char forward_mail;          /* forward mail through this node?      */
    char noChat;                /* TRUE to suppress chat attempts       */
    char debug;                 /* turns debug printout on and off      */
    char diskusage;             /* do a df() after each directory       */
    char fZap;                  /* default flags for +zap,              */
    char fNetlog;               /*                   +netlog,           */
    char fNetdeb;               /*                   +netdebug          */

    int  evtCount;              /* number of events to deal with...     */
    int  floorCount;            /* number of floors to deal with...     */
    int  zap_count;             /* loop zap table size                  */
    int  netSize;               /* How many on the net?                 */
    int  poll_count;            /* # polling events                     */

    int  com_port;              /* com port that the modem is on...     */
                                /* (only useful for MS-DOS version)     */
    long poll_delay;            /* idle time before polling systems     */

    char sysPassword[60];       /* Remote sysop                         */
    char filter[128];           /* input character translation table    */
    char codeBuf[MAXCODE];      /* strings buffer                       */
    char shave[8];              /* shave-and-a-haircut/2 bits pauses    */

    char sysBaud;               /* What's our baud rate going to be?    */
    char search_baud;           /* TRUE to do flip flop search for baud */
    char connectPrompt;         /* prompt for C/R when connected        */
    CHAR dum2;
    int  connectDelay;          /* how long to wait after connect before*/
                                /* autobauding                          */
    int  modemSetup;            /* string to set up the modem           */
    int  mCCs[NUMBAUDS];        /* strings for getting condition codes  */
                                /* from the modem                       */
    char modemCC;               /* does the modem do condition codes?   */
    char probug;                /* prometheus 2400 baud modem bug...    */
    int  dialPrefix;            /* string to prefix telephone #'s with  */
    int  dialSuffix;            /* string to append to telephone #'s    */
    int  local_time;            /* how long to wait 'til local hangup   */
    int  ld_time;               /* how long to wait 'til l-d hangup     */

    int  shell;                 /* shell to execute if you do an ^LO    */
    int  sysopName;             /* user to throw SYSOP mail at.         */
    int  archiveMail;           /* archive all this users' mail         */
    int  hub;                   /* for forwarding mail.                 */
    int  organization;          /* descriptive field for headers        */
    int  syswidth;              /* default terminal width               */
    /*
     * accounting variables
     */
    long download;              /* download limit...                    */
    char ld_cost;               /* cost to mail to a l-d system         */
    char hubcost;               /* cost to route mail through #hub      */

    char novice [NOVICE_WIDTH];     /* prompt string for novices */
    char novice_cr;
  
    char spares [8];                /* a little room to grow */
} ;

/************************************************************************
 *                                Room data                             *
 ************************************************************************/

#define MAXGEN         32       /* six bits of generation => 64 of them */
#define FORGET_OFFSET  (MAXGEN / 2)     /* For forgetting rooms         */

#define l_OLD   0x01            /* flag bits for listRoom()/listFloor() */
#define l_NEW   0x02
#define l_FGT   0x04            /* list forgottten rooms                */
#define l_EXCL  0x08            /* don't list current room/floor        */
#define l_LONG  0x10            /* long listing (for ;k...)             */

struct rflags {                 /* Room flags                           */
    char     INUSE;             /* Room in use?                         */
    char     PUBLIC;            /* Room public?                         */
    char     ISDIR;             /* Room directory?                      */
    char     PERMROOM;          /* Room permanent?                      */
    char     SKIP;              /* Room skipped? (temporary for user)   */
    char     UPLOAD;            /* Can room be uploaded to?             */
    char     DOWNLOAD;          /* Can room be downloaded from?         */
    char     SHARED;            /* Is this a shared room?               */
    char     ARCHIVE;           /* Is this room archived somewhere?     */
    char     ANON;              /* is this an anonymous room?           */
    char     INVITE;            /* is this an invitation-only room?     */
    char     NETDOWNLOAD;       /* net-downloadable room?               */
    char     AUTONET;           /* (shared room -- net all messages?)   */
    char     READONLY;          /* readonly room?                       */
    char     floorGen;          /* floor this room is in                */
} ;

struct rTable {                 /* The summation of a room              */
    unsigned rtgen;             /* generation # of room                 */
    struct rflags rtflags;      /* public/private flag etc              */
    label rtname;               /* name of room                         */
    CHAR dummy;
    long rtlastNet;             /* # of last net message in room        */
    long rtlastLocal;           /* # of last @L message in room         */
    long rtlastMessage;         /* # of most recent message in room     */
} ;

struct aRoom {                  /* The appearance of a room:            */
    unsigned    rbgen;          /* generation # of room                 */
    struct rflags rbflags;      /* same bits as flags above             */
    label       rbname;         /* name of room                         */
    CHAR        dummy;
    long        rblastNet;
    long        rblastLocal;
    long        rblastMessage;
    char        rbfill[8];
    char        rbdirname[100]; /* user directory for this room's files */
    struct aRmsg {
        long rbmsgNo;           /* every message gets unique#           */
        int  rbmsgLoc;          /* sector message starts in             */
    } msg[MSGSPERRM];
    short       rb_wwiv_id;     /* WWIV network room id  -b0b-          */
    label       sparelabel;     /* spare label (for FIDO, maybe?) -b0b- */
    short       spare1;
    short       spare2;
} ;

/************************************************************************
 *                            floor stuff                               *
 ************************************************************************/

struct flTab {
    label flName;               /* floor name                           */
    char flGen;                 /* floor generation #                   */
    char flInUse;               /* is this floor in use?                */
    char flexp0;                /* 4 expansion fields                   */
    char flexp1;
    char flexp2;
    char flexp3;
} ;

#define LOBBYFLOOR      0       /* generation number and* table index   */

/************************************************************************
 *                            userlog stuff                             *
 ************************************************************************/

#define CRYPTADD      117       /*                                      */

struct lflags {                 /* Flags for person in log              */
    char     lfexpand;          /* Uppercase?                           */
    char     LFMASK;            /* Linefeeds?                           */
    char     EXPERT;            /* Expert?                              */
    char     AIDE;              /* Vice-Grand-Poobah?                   */
    char     L_INUSE;           /* Is this slot in use?                 */
    char     TIME;              /* Send time to user of msg creation?   */
    char     OLDTOO;            /* Print out last oldmessage on [N]ew?  */
    char     NET_PRIVS;         /* User have net privileges?            */
    char     unused;            /* used to be NOTFLOOR                  */
    char     validated;         /* user validation in paranoid systems  */
    char     spares[6];         /* a total of 16 bytes in lflags        */
} ;

#define MAXVISIT        8       /* #visits we remember old newestLo for */

#define GENSHIFT        3       /* Where the generation # is            */
#define CALLMASK        7       /* For finding last visit               */

struct logBuffer {              /* The appearance of a user:            */
    char  lbnulls;              /* #nulls, lCase, lFeeds                */
    struct lflags lbflags;      /* UCMASK, LFMASK, EXPERT, AIDE, INUSE  */
    char  lbwidth;              /* terminal width                       */
    char  lbheight;             /* terminal height                      */
    char  lbcls[6];             /* Clear screen code -b0b-              */
    CHAR  dum1;
    int   credit;               /* Credit for long distance calls       */
    label lbname;               /* caller's name (handle)               */
    label lbpw;                 /* caller's password                    */
    label lbrealname;           /* caller's real name for closed system */
    long  lblimit;              /* # bytes the user can download today  */
    long  lblast;               /* last day the user logged in          */
    char  lbgen[MAXROOMS];      /* 5 bits gen, 3 bits lastvisit         */
    long  lbvisit[MAXVISIT];    /* newestLo for this and 3 prev. visits */
    int   lbslot[MSGSPERRM];    /* for private mail                     */
    long  lbId[MSGSPERRM];      /* for private mail                     */
} ;

#define LBGEN(p,x)      (((p).lbgen[x] >> GENSHIFT) & 0x1F)

struct lTable {                 /* Summation of a person:               */
    int  ltpwhash;              /* hash of password                     */
    int  ltnmhash;              /* hash of name                         */
    int  ltlogSlot;             /* location in userlog.buf              */
    long ltnewest;              /* last message on last call            */
} ;

/************************************************************************
 *                      message stuff                                   *
 ************************************************************************/

#define MAXTEXT      7500       /* maximum chars in edit buffer         */
#define MAXWORD      256        /* maximum length of a word             */

struct msgB {                   /* This is what a msg looks like        */
    int  AAAIIEEE;              /* a little bit of alignment...         */
    char mbtext[MAXTEXT];       /* buffer text is edited in             */

    label mbid;                 /* local number of message              */
    label mbdate;               /* creation date                        */
    label mbtime;               /* creation time                        */
    label mbroom;               /* creation room                        */
    NA    mbauth;               /* name of author                       */
    NA    mbto;                 /* private message to                   */
    /*
     * fields that are only used while the message is being entered.
     */
    int   mbcost;               /* postage due for this message         */
    /*
     * the following fields are used only by the networker.
     */
    char  mbroute[NAMESIZE+2];  /* routing code of message              */
    label mboname;              /* short human name for origin system   */
    label mborig;               /* US xxx xxx xxxx style ID             */
    /*]
      ] added by popular demand
      ] */
    char  mborg [ORGSIZE];      /* organization field                   */
    char  mbsubject [ORGSIZE];  /* subject field     -b0b-              */
    short mbspare   [56];       /* room to grow      -b0b-              */
};

/* values for showMess routine */
#define NEWoNLY         0
#define OLDaNDnEW       1
#define OLDoNLY         2
#define GLOBALnEW       3

/************************************************************************
 *                      modem stuff                                     *
 ************************************************************************/

#define ERRORMAX       10       /* How many errors to suffer through    */
#define MINUTE         60       /* How long is a minute?                */
#define STARTUP        YES      /* start a WC transfer                  */
#define FINISH         NO       /* end a WC transfer                    */

/*  output XON/XOFF etc flag... */
#define OUTOK           0       /* normal output                        */
#define OUTPAUSE        1       /* a pause has been requested           */
#define OUTNEXT         2       /* quit this message, get the next      */
#define OUTSKIP         3       /* stop current process                 */
#define OUTPARAGRAPH    4       /* skip to next paragraph               */
#define IMPERVIOUS      5       /* make current output unstoppable      */

#define NEITHER         0       /* don't echo input at all              */
#define CALLER          1       /* echo to caller only --passwords etc  */
#define BOTH            2       /* echo to caller and console both      */

/************************************************************************
 *                      Various `standard' exit stati                   *
 ************************************************************************/

#define SYSOP_EXIT      0               /* horrid error or ^Lq          */
#define CRASH_EXIT      2               /* possibly fixable error       */
#define REMOTE_EXIT     3               /* ^Lq from remote              */

/************************************************************************
 *                      Useful pseudo functions                         *
 ************************************************************************/

#define onLine()        ((haveCarrier || onConsole) && !eventExit)
#define zero_struct(x)  blkfill(&(x), 0, sizeof(x))

#define okRoom(i)       \
     ((!flGlobalFlag)   \
   || (roomTab[i].rtflags.floorGen == floorTab[thisFloor].flGen))

/* okRoom(i) used to be:
** (notFloor || (roomTab[i].rtflags.floorGen == floorTab[thisFloor].flGen))
*/
#define TheSysop()      (aide && onConsole)
#define SomeSysop()     ((aide && onConsole) || remoteSysop)
#define copy_struct(src, dest)  blkcpy(&(dest), &(src), sizeof(src))
#define copy_array(src, dest)   blkcpy( (dest),  (src), sizeof(src))

#endif /* end ifndef CTDL_H */
