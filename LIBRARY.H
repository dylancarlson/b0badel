/*{ $Workfile:   library.h  $
 *  $Revision:   1.1  $
 * 
 *  externs and prototypes for b0badel library functions
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

#ifndef CTDL_H
#include "ctdl.h"
#endif

/** the application must provide these functions: **/

void crashout  (char *format, ...);    /* error msg, terminate program */
void mPrintf   (char *format, ...);    /* output a string to wherever  */

/** from libcryp.c **/

void crypte (void *buffer, register unsigned count, register unsigned seed);

/** from libtag.c **/

int  tagSetup (void);
char *getTag  (char *fname);
void putTag   (char *filename, char *string);
void tagClose (void);

/** from libalias.c **/

struct alias 
{
    int  a_sys;
    char *a_name;
    char *a_alias;
    struct alias *a_next;
};

struct alias *load_alias (char *filename);

char *chk_alias  (struct alias *tab, int sys, char *name);
char *chk_name   (struct alias *tab, int sys, char *alias);

/** from liblog.c **/

#ifndef LIBLOG_C
   extern struct logBuffer logBuf;  /* Person's log buffer  */
   extern int  logfl;               /* log file descriptor  */
#endif

void putLog          (struct logBuffer *p, int n);
void getLog          (struct logBuffer *p, int n);
int  getnmidx        (char *name, struct logBuffer *log);
int  getnmlog        (char *name, struct logBuffer *log);

/** from libdata.c **/

extern char Debug;                  /* normal debugging  */
extern char *_day[];                /* days of the week  */

/** from libdep.c **/

int  cd           (char *path);
void getcd        (pathbuf path);
long upTime       (void);
void pause        (int i);
long timeSince    (long x);
long timeLeft     (long p);
void diskSpaceLeft(char *path, long *sectors, long *bytes);
void *xmalloc     (int size);
char *xstrdup     (char *string);
char *EOS         (char *s);
FILE *safeopen    (char *fn, char *mode);
void ctdlfile     (char *dest, int dir, char *fmt, ...);

/** from io.asm, etc. **/

int  getraw          (void);
int  gotcarrier      (void);
int  mdhit           (void);
void modout          (char c);
void modemClose      (void);
void modemOpen       (void);
void setBaud         (int rate);

int  putch           (char c);

/** from libtimer.c **/

long chkTimeSince    (void);
void startTimer      (void);


/** from libtabl.c **/

#ifndef LIBTABL_C **
   extern struct config cfg;           /* configuration data   */
   extern struct lTable    *logTab;    /* Log  index in RAM    */
   extern struct rTable    roomTab[];  /* Room index in RAM    */
#endif

int  readSysTab      (int kill);
int  writeSysTab     (void);


/** from hash.c **/

int  hash            (char *str);
int  labelcmp        (char *p1, char *p2);


/** from libmisc.c **/

char *tod            (int stdTime);
char *formDate       (void);
char *formRoom       (int roomno, int markprivate);
void copystring      (char *s1, char *s2, int size);
void normalise       (char *s);

/** from libnet.c **/

void getNet          (int n);
void putNet          (int n);
int  netnmidx        (char *name);

/** from libarch.c **/

int  addArchiveList  (int room, char *fn);
char *findArchiveName(int room);
void initArchiveList (void);

