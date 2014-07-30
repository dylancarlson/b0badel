/*{ $Workfile:   externs.h  $
 *  $Revision:   1.20  $
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
#include "library.h"
 
#define UINT unsigned int
#define REG  register

/********* GLOBAL data *********/

extern char *CRfill;                /* for menu printing */
extern char *dlWaitStr;
extern char *jrnlFile;
extern char *program;
extern char *protocol[];            /* protocol names    */
extern char *ulWaitStr;
extern char *xBatchStr;

extern char b0bVersion[];
extern char confirm[];
extern char MACHINE[];
extern char netmlcheck[];
extern char pauseline[];
extern char Room_Area[];
extern char room_area[];
extern char sectBuf[SECTSIZE + 5];
extern char VERSION[];

extern char sr_rcvd[];              /* status flags for receiving */
extern char sr_sent[];              /*   and sending shared rooms */

extern char Abandon;                /* True when time to bring system down  */
extern char aide;                   /* User aide?                   */
extern char batchWC;                /* batch file transfer?         */
extern char checkloops;
extern char checkNegMail;
extern char dFormatted;
extern char dropDTR;                /* hang up phone when in console mode   */
extern char echo;              
extern char eventExit;              /* true when an event goes off  */
extern char expert;                 /* is user expert?              */
extern char forceOut;               /* next event is preemptive?    */
extern char haveCarrier;       
extern char heldMessage;       
extern char inNet;                  /* are we in the networker now? */
extern char justLostCarrier;
extern char loggedIn;               /* Are we logged in?            */
extern char logNetResults;
extern char modStat;
extern char netDebug;
extern char netymodem;              /* use 1k blocks to network     */
extern char newCarrier;
extern char oldToo;                 /* Print old msg on New request */
extern char onConsole;              /* Where IO is ...              */
extern char outFlag;
extern char outFlag;           
extern char remoteSysop;       
extern char remoteSysop;            /* Is current user a sysop?     */
extern char route_char;
extern char seemodem;
extern char sendTime;               /* send time msg created        */
extern char termLF;                 /* LF-after-CR flag             */
extern char usingWCprotocol;        /* WC protocol flag             */
extern char WC;                     /* WC mode rwProtocol returns   */
extern char WCError;
extern char watchForNet;            /* only true during greeting()  */
extern char warned;                 /* warned of impending events?  */

extern FILE *log;
extern FILE *netLog;
extern FILE *spool;
extern FILE *upfd;

extern int  (*sendPFchar)(int);

extern int  Anonymous;              /* post next message anonymously*/
extern int  byteRate;               /* bytes per second (really?)   */
extern int  counter;                /* used by networker            */
extern int  crtColumn;              /* where we are on the current line */
extern int  evtRunning;             /* gonna have an event?         */
extern int  exitValue;
extern int  flGlobalFlag;           /* flag true only during ;R(ead) */
extern int  floorfl;                /* file descriptor for floors   */
extern int  fossil;                 /* TRUE if fossil driver active */
extern int  lastRoom;               /* For Ungoto command           */
extern int  logindex;
extern int  msgfl2;                 /* Another for auto backup      */
extern int  msgfl2;                 /* file descriptor for the backup file  */
extern int  msgfl;                  /* file descriptor for the msg file     */
extern int  msgfl;                  /* Message file descriptor      */
extern int  netfl;                  /* Net file                     */
extern int  PATCHNUM;
extern int  rmtslot;
extern int  roomfl;                 /* file descriptor for rooms    */
extern int  singleMsg;              /* for Quick Scan command       */
extern int  termNulls;              /* # nulls to send at EOLN      */
extern int  termWidth;         
extern int  thisFloor;
extern int  thisFloor;              /* Current floor                */
extern int  thisNet;                /* current netBuf loaded        */
extern int  thisRoom;               /* Current room                 */

extern label msguser;
extern label oldTarget;
extern label rmtname;
extern label rmt_id;

extern long evtClock;               /* next event time              */
extern long evtTrig;                /* trigger time for messages    */
extern long pullMId;                /* Id of msg to be pulled       */

extern struct evt_type  *nextEvt;   /* next event           */
extern struct flTab     *floorTab;
extern struct alias     *net_alias; /* network aliases      */

extern struct aRoom     roomBuf;    /* Room buffer                  */

extern struct msgB      msgBuf;     /* Message buffer               */
extern struct msgB      tempMess;   /* Message buffer               */
extern struct netBuffer netBuf;

extern UINT pullMLoc;               /* Loc of msg to be pulled      */

/*{**** GLOBAL Function Prototypes *****/

char *matchString    (char *buf, char *pattern, char *bufEnd);
char *netopt         (char *optstr, char *proto);
char *plural         (char *msg, long number);
char *uname          (void);
char getnoecho       (void);

int  ARsetup         (char *filename);
int  batch           (int mode, struct dirList *list, int count);
int  beginWC         (void);
int  caller          (void);
int  callsys         (int i);
int  canEnter        (int i);
int  canreplyto      (char mode);
int  check_for_init  (void);
int  checkpolling    (void);
int  conGetYesNo     (char *prompt);
int  doFloor         (char c);
int  doMailFirst     (void);
int  doSysop         (void);
int  dateok          (long time);
int  doFindMsg       (int loc, long id);
int  dotutorial      (char *topic, int help);
int  download        (struct dirList *fn);
int  editUser        (void);
int  endWC           (void);
int  enterfile       (int (*pc)(int c), char mode);
int  findFloor       (char gen);
int  findMessage     (int loc, long id);
int  findnode        (char *node, int *cost);
int  getMod          (void);
int  getNo           (char *prompt);
int  getText         (int WCmode);
int  getYes          (char *prompt);
int  getYesNo        (char *prompt);
int  getspool        (void);
int  gotoFloor       (char genNumber);
int  gotoRoom        (char *nam, char mode);
int  enablecom       (int comport);
int  hasNew          (int i);
int  hldMessage      (void);
int  increment       (int c);
int  ingestFile      (char *name);
int  IngestLogfile   (void);
int  issharing       (int slot);
int  listUsers       (void);
int  mAbort          (void);
int  makenetmessage  (void);
int  minutes         (int *today);
int  mmesgbaud       (void);
int  modIn           (void);
int  NNisok          (register char *node);
int  netAck          (void);
int  netchdir        (char *path);
int  netcommand      (int cmd, ...);
int  netmesg         (int slot);
int  netWCstartup    (char *from);
int  notseen         (void);
int  normID          (register char *source, register char *dest);
int  parsepath       (char *netpath, char *node, int edit);
int  partialExist    (label target);
int  pollnet         (int which);
int  postmail        (int savemail);
int  privateReply    (void);
int  putmessage      (char *room);
int  readglobal      (void);
int  receive         (int seconds);
int  replymsg        (int ask);
int  roomCheck       (int (*checker)(char *), char *nam);
int  roomExists      (char *room);
int  sendARchar      (int c);
int  sendARinit      (void);
int  sendARend       (void);
int  sendCchar       (int c);
int  sendCend        (void);
int  sendCinit       (void);
int  sendYhdr        (char *name, long size);
int  sgetword        (char *dest, char *source, int offset, int lim);
int  srchNet         (char *forId);
int  tutorial        (char *topic);
int  typefile        (struct dirList *p);
int  typeWC          (FILE *fd);
int  xchdir          (char *path);

long asknumber       (char *prompt, long bottom, long top, int d_fault);
long dosexec         (char *cmd, char *tail);
long getNumber       (char *prompt, long bottom, long top);
long parsedate       (REG char *s);
long sysRoomLeft     (void);

UINT iChar           (void);
UINT getmsgchar      (void);

void aideMessage     (int noteDeletedMessage);
void called          (void);
void changeto        (char *oldto, char *firstnode);
void cleanFloor      (void);
void close_zap       (void);
void configure       (void);
void connect         (int line_echo, int mapCR, int local_echo);
void conout          (int c);
void crashout        (char *format, ...);
void crypte          (void *buf, REG UINT count, REG UINT seed);
void doCR            (void);
void doEnter         (int prefix, char cmd);
void doEvent         (void);
void doPrintMsg      (void);
void doResults       (void);
void doSetup         (void);
void doUpload        (int prefix);
void dumpRoom        (int tellSkipped);
void exitCitadel     (int status);
void fakeFullCase    (char *text);
void fetchRoom       (int flag);
void floorMap        (void);
void getList         (int (*fn)(label), char *prompt);
void getNormStr      (char *prompt, char *s, int size, int doEcho);
void getRoom         (int rm);
void getString       (char *prompt, char *buf, int lim, int exChar, int vis);
void getstrspool     (char *place, int length);
void hangup          (void);
void hFormat         (char *string);
void homeSpace       (void);
void indexRooms      (void);
void init_zap        (void);
void initEvent       (void);
void inMail          (void);
void lFloor          (int mask);
void listRooms       (int mode);
void loadFloor       (void);
void logMessage      (char val, char *str, char sig);
void makeMessage     (char WCmode);
void makeRoom        (void);
void mastermode      (int reversed);
void mCR             (void);
void mflush          (void);
void mFormat         (char *string);
void modemInit       (void);
void modputs         (char *s);
void msgprintf       (const char *format, ...);
void msgseek         (UINT sector, int byteptr);
void msgToDisk       (char *filename, long id, UINT loc);
void neterror        (int hup, char *format, ...);
void netMessage      (char WCmode);
void netmode         (int length, int whichnet);
void netPrintMsg     (int loc, long id);
void netwildcard     (int (*fn)(struct dirList *), char *pattern);
void new_configure   (void);
void nfs_process     (void);
void note2Message    (long id, UINT loc);
void noteRoom        (void);
void notemessage     (struct logBuffer *logptr, int logidx);
void oChar           (char c);
void OutOfNet        (void);
void outword         (char *string);
void readNegMail     (void);
void slavemode       (int reversed);
void softWord        (char *st);
void terminate       (char disconnect, char flag);
void printmessage    (int loc, long id);
void pushRoom        (int rmId, int rmMsgPtr);
void putFloor        (int i);
void putspool        (FILE *f);
void putRoom         (int rm);
void rawmodeminit    (void);
void readMail        (char zap, void (*mailer)(void));
void retRoom         (char *roomName);
void ringSysop       (void);
void saveRmPtr       (char mode);
void sendXmh         (void);
void setclock        (void);
void setUpEvent      (void);
void setlog          (void);
void setmodem        (void);
void showMessages    (char whichMess, char revOrder);
void showdays        (char mask);
void splitF          (FILE *diskFile, char *format, ...);
void storeLog        (void);
void storeMessage    (struct logBuffer *logptr, int logidx);
void update_mail     (void);
void wcprintf        (char *format, ...);
void whazzit         (void);
void xCR             (void);

/******************************}*/
