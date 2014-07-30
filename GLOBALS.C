/*{ $Workfile:   globals.c  $
 *  $Revision:   1.15  $
 *
 *  global variables for b0badel
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

#define GLOBALS_C
 
#include "ctdl.h"

#define UINT unsigned int

char    Abandon     = NO;       /* True when time to bring system down  */
char    aide        = NO;       /* aide privileges flag             */
char    checkNegMail;
char    dropDTR     = YES;      /* hang up phone when in console mode   */
char    echo        = BOTH;     /* Either NEITHER, CALLER, or BOTH  */
char    eventExit   = NO;       /* true when an event goes off      */
char    expert      = NO;       /* true to suppress hints etc.      */
char    haveCarrier;            /* set if DCD == YES                */
char    heldMessage = NO;       /* true if there's a held message   */
char    inNet       = NO;       /* are we in the networker now?     */
char    justLostCarrier = NO;   /* connectus interruptus            */
char    loggedIn    = NO;       /* Global have-caller flag          */
char    logNetResults = NO;
char    modStat;                /* Whether modem had carrier LAST   */
                                /*   time you checked.              */

char    netymodem   = NO;       /* use 1k blocks when networking?   */
char    netDebug    = NO;
char    newCarrier  = NO;       /* Just got carrier                 */
char    oldToo      = YES;      /* Send last old on new request?    */
char    outFlag     = OUTOK;
char    remoteSysop = NO;       /* Is current user a sysop          */
char    sendTime    = YES;      /* send time msg created            */
char    termLF      = NO;       /* LF-after-CR flag                 */
char    usingWCprotocol = NO;   /* True during WC protocol transfers    */
char    watchForNet = NO;       /* only true during greeting()      */

int     Anonymous   = NO;       /* post next message anonymously    */
int     counter     = 0;        /* unfortunate name; used by networker */
int     exitValue   = CRASH_EXIT;
int     flGlobalFlag = FALSE;   /* TRUE only during ;R(ead)     */
int     fossil      = 0;        /* TRUE if fossil is active     */
int     lastRoom;               /* last room stack pointer      */
int     logindex    = 0;        /* who's logged in now...       */
int     roomfl;                 /* Room file descriptor         */
int     singleMsg   = YES;      /* pause after each message     */
int     termNulls   = 0;        /* # nulls to send at EOLN      */
int     termWidth   = 80;       /* width to format output to    */

char    confirm[]   = "confirm";
char    Room_Area[] = "Room";
char    room_area[] = "room";
char    pauseline[] = " <space> to pause, Q to Quit\n ";

char    netmlcheck[] = "$chkmail";

/** the %s in these strings gets filled with the protocol string **/

char    *dlWaitStr  = "\n Waiting (%s protocol download)\n ";
char    *ulWaitStr  = "\n Waiting (%s protocol upload)\n ";
char    *xBatchStr  = "\n Waiting (%s Batch transfer)\n ";

char    *program    = "b0badel";
char    *net_alias  = NULL;

int     rmtslot;
label   rmtname;
label   rmt_id;

/* this array must be in sync with the enum in protocol.h */

char *protocol[] = 
{ 
    "ascii",
    "vanilla",
    "xmodem",
    "ymodem",
    "capture",
    "journal"
};

char sectBuf[SECTSIZE + 5];

struct msgB tempMess;           /* For held messages        */

label oldTarget;
label msguser;                  /* display messages from/to this user */

FILE *log       = NULL;         /* net error logfile        */
FILE *netLog    = NULL;
FILE *upfd      = NULL;
FILE *spool     = NULL;


struct msgB     msgBuf;         /* The -sole- message buffer            */

int     msgfl;                  /* file descriptor for the msg file     */
int     msgfl2;                 /* file descriptor for the backup file  */

UINT    pullMLoc;
long    pullMId;
int     crtColumn;

/* event stuff */

char forceOut;                  /* next event is preemptive?    */
int  evtRunning = FALSE;        /* an event is waiting          */
long evtClock;                  /* time when next event takes place */
long evtTrig;                   /* timeLeft trigger for warnings    */
char warned     = NO;           /* been warned of impending event   */


