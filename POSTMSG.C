/*{ $Workfile:   postmsg.c  $
**  $Revision:   1.12  $
**
**  Message posting routines for b0badel
}*/

#include "ctdl.h"
#include "ctdlnet.h"
#include "protocol.h"
#include "msg.h"
#include <stdarg.h>
#include <string.h>
#include <io.h>
#include "externs.h"

/* postmail()              Post/validate mail
** spoolmessage()          Write a message to disk
** * savenetmail()         update pointers and billing info
** * msgputchar()          Writes successive message chars to disk
** * msgflush()            Wraps up message-to-disk store
** * msgprintf()           printf() for saving a message to ctdlmsg
** putmessage()            Stores a message to disk
** * noteLogMessage()      Enter message into log record
** * aNoteLogMessage()     ... does archiving too
** note2Message()          makes room for a new message
** notemessage()           Enter message into current room
** msgToDisk()             Archive a message to the disk
** storeMessage()          save a message
*/

/* prototypes for static functions */

static void putfield        (FILE *fd, char key, char *field);
static void savenetmail     (void);
static int  msgputchar      (char c);
static void msgflush        (void);
static void noteLogMessage  (struct logBuffer *p, int logno);
static void aNoteLogMessage (struct logBuffer *who, int logno);


/*
** postmail() - try to deliver or forward electronic mail
**              this routine is used only by mailers,
**              because mail originating locally needs
**              accounting and a prettier format
*/
int postmail(int savemail)
{
    struct logBuffer logb;
    FILE   *spool;
    label   nextnode;
    label   lastnode;
    pathbuf spoolfile;
    NA      from, to;
    int     cost;
    int     oldnet, oldroom;
    int     where, logidx;

    oldnet = thisNet;
    strcpy(to, msgBuf.mbto);
    strcpy(lastnode, netBuf.netName);

    if (cfg.forward_mail && parsepath(to, nextnode, savemail)) 
    {
        if ((where = findnode(nextnode, &cost)) != ERROR) 
        {
            if (savemail) 
            {
                msgBuf.mborig[0] = msgBuf.mboname[0] = 0;

                sprintf(from, "%s!%s", lastnode, msgBuf.mbauth);
                strcpy(msgBuf.mbauth, from);

                changeto(to, nextnode);
                ctdlfile(spoolfile, cfg.netdir, "%d.fwd", where);

                if ((spool = safeopen(spoolfile,"ab")) != NULL) 
                {
                    putspool(spool);
                    fclose(spool);

                    /* netBuf already points at the proper netnode... */

                    netBuf.nbflags.mailpending = YES;
                    putNet(where);
                }
                else
                {
                    where = ERROR;
                }
            }
        }
        getNet(oldnet);
        return (where != ERROR);
    }
    logidx = getnmlog(msgBuf.mbto, &logb);
    if (logidx != ERROR || stricmp(msgBuf.mbto, "Sysop") == 0) 
    {
        if (savemail) 
        {
            oldroom = thisRoom;
            getRoom(MAILROOM);
            if (logidx != ERROR)
                strcpy(msgBuf.mbto, logb.lbname);
            storeMessage(&logb, logidx);
            getRoom(oldroom);
        }
        return YES;
    }
    return NO;
}


/*
** putfield() stores nonblank fields to disk
*/
static void putfield (FILE *fd, char key, char *field)
{
    if (*field) 
	{
        fprintf(fd, "%c%s", key, field);
        fputc(0, fd);
    }
    return;
}


/*
** putspool() stores a message to disk
*/
void putspool(FILE *f)
{
    register char *s;
    
    fprintf(f, "\377%lu", 1+cfg.newest); 
    putc(0,f);                              /* message ID   */
    putfield(f, 'D', msgBuf.mbdate);        /* date     */
    putfield(f, 'C', msgBuf.mbtime);        /* time     */
    putfield(f, 'R', roomBuf.rbname);       /* room name    */
    putfield(f, 'A', msgBuf.mbauth);        /* author       */
    putfield(f, 'T', msgBuf.mbto);          /* destination  */
    putfield(f, 'J', msgBuf.mbsubject);     /* subject      */
    putfield(f, 'I', msgBuf.mborg);         /* org info     */
    putfield(f, 'Z', msgBuf.mbroute);       /* network info */
    putfield(f, 'N', msgBuf.mboname);
    putfield(f, 'O', msgBuf.mborig);

    putc('M', f);
    for (s = msgBuf.mbtext;  *s;  s++)
    {
        putc(*s, f);
    }
    putc(0,f);
}


/*
** savenetmail() - update pointers and billing info for netmail
*/
static void savenetmail(void)
{
    FILE *mail;
    pathbuf mailfile;

    ctdlfile(mailfile, cfg.netdir, "%d.ml", thisNet);

    if (mail = safeopen(mailfile, "a")) 
    {
        fprintf(mail, "%ld %d\n", 1+cfg.newest, cfg.catSector);
        fclose(mail);
        logBuf.credit -= msgBuf.mbcost;
        storeLog();

        if (!netBuf.nbflags.mailpending) 
        {
            netBuf.nbflags.mailpending = YES;
            putNet(thisNet);
        }
    }
    else
    {
        mPrintf("Queuing error!\n ");
    }
    return;
}


/*
** msgputchar() - write out a character to the messagefile
*/
static int msgputchar(char c)
{
    long address;

    if (msgs.sectBuf[msgs.thisChar] == (char)0xFF)      /* killed a message */
    {
        logBuf.lbvisit[(MAXVISIT-1)] = ++cfg.oldest;
    }

    msgs.sectBuf[(msgs.thisChar)++] = c;

    if ((msgs.thisChar %= BLKSIZE) == 0) 
    {
        /*
         * write this sector out...
         */
        crypte(msgs.sectBuf, BLKSIZE, 0);
        address = (long)(msgs.thisSector) * (long)(BLKSIZE);

        dseek(msgfl, address, 0);
        if (dwrite(msgfl, (msgs.sectBuf), BLKSIZE) != BLKSIZE)
        {
            crashout("msgfile write error");
        }
        if (cfg.mirror) 
        {
            dseek(msgfl2, address, 0);
            if (dwrite(msgfl2, (msgs.sectBuf), BLKSIZE) != BLKSIZE)
            {
                crashout("mirror write error");
            }
        }

        msgs.thisSector = (++(msgs.thisSector)) % cfg.maxMSector;

        /* ... and read in the next */

        dseek(msgfl, ((long)(msgs.thisSector)) * ((long)BLKSIZE), 0);

        if (dread(msgfl, (msgs.sectBuf), BLKSIZE) != BLKSIZE)
        {
            crashout("msgfile read error");
        }
        crypte(msgs.sectBuf, BLKSIZE, 0);
    }
    return YES;
}


/*
** msgflush() - wraps up writing a message to disk, takes
**              into account 2nd msg file if necessary
*/
static void msgflush(void)
{
    long address = (long)(msgs.thisSector) * (long)(BLKSIZE);
    crypte(msgs.sectBuf, BLKSIZE, 0);

    dseek(msgfl, address, 0);
    if (dwrite(msgfl, msgs.sectBuf, BLKSIZE) != BLKSIZE)
    {
        crashout("msg file flush error");
    }

    if (cfg.mirror) 
    {
        dseek(msgfl2, address, 0);
        if (dwrite(msgfl2, msgs.sectBuf, BLKSIZE) != BLKSIZE)
        {
            crashout("mirror file flush error");
        }
    }
    crypte(msgs.sectBuf, BLKSIZE, 0);
}


/*
** msgprintf() - printf() to ctdlmsg.sys
*/
void msgprintf(const char *format, ...)
{
    va_list         arg_ptr;
    char            string[MAXWORD];
    register char * p;

    va_start(arg_ptr, format);
    vsprintf(string, format, arg_ptr);

    for (p = string; *p; )
    {
        msgputchar(*p++);
    }
    msgputchar(0);
}


/*
** putmessage() - stores a message to disk. Always called
**                 before notemessage() - newest not ++ed
*/
int putmessage(char *room)
{
    label destnode;
    int cost, nbpos;
    register char *p;

    msgseek(cfg.catSector, cfg.catChar);
    msgputchar(0xFF);

    /* write message ID */

    msgprintf("%lu", cfg.newest + 1);

    /* write date & time */

    if (Anonymous)
    {
        msgprintf("D****");
        msgprintf("C ");
    }
    else
    {
        msgprintf("D%s", msgBuf.mbdate[0] ? msgBuf.mbdate : formDate());
        msgprintf("C%s", msgBuf.mbtime[0] ? msgBuf.mbtime : tod(YES));
    }

    msgprintf("R%s", room ? room : roomBuf.rbname);

    /* write author's name out */

    if (Anonymous)                      /*  Anonymous message   */
    {
        msgprintf("A*** ******");       /*  From *** ******     */
    }
    else if (msgBuf.mbauth[0])
    {
        msgprintf("A%s", msgBuf.mbauth);
    }

    if (msgBuf.mbto[0]) /* private message -- write addressee   */
    {
        if (parsepath(msgBuf.mbto, destnode, NO))   /* netted! */
        {
            nbpos = findnode(destnode, &cost);  /* load netbuffer       */
            getNet(nbpos);                      /* then load the netbuf */
            savenetmail();                      /* and update pointers  */
        }
        msgprintf("T%s", msgBuf.mbto);
    }
    else if (ROUTEOK(&msgBuf))
    {
        /* message in a shared room... */

        msgprintf("Z%s", msgBuf.mbroute);

        /* keep incoming @L messages noted in a different place */

        if (ROUTECHAR(&msgBuf) == ROUTE_LOCAL)
        {
            roomBuf.rblastLocal = 1+cfg.newest;
        }
        else
        {
            roomBuf.rblastNet = 1+cfg.newest;
        }
    }

    if (msgBuf.mborig[0])
    {
        msgprintf("O%s", msgBuf.mborig);
    }

    if (roomBuf.rbflags.ANON)
    {
        if (msgBuf.mboname[0])          /*  Anonymous shared room         */
        {
            msgprintf("N*********");    /*  From *** ****** @ *********   */
        }
    }
    else
    {
        if (msgBuf.mboname[0])          /* name of originating system */
        {
            msgprintf("N%s", msgBuf.mboname);
        }
        if (msgBuf.mborg[0])            /* ID of originating system */
        {
            msgprintf("I%s", msgBuf.mborg);
        }
    }

	if (msgBuf.mbsubject[0])
    {
	    msgprintf("J%s", msgBuf.mbsubject);
    }

    /* after everything else, write out the message.... */

    msgputchar('M');

    for (p = msgBuf.mbtext; *p;  p++)
    {
        msgputchar(*p);
    }

    msgputchar(0);

    msgflush();
    return YES;
}


/*
** noteLogMessage() - slots message into log record
*/
static void noteLogMessage(struct logBuffer *p, int logno)
{
    int i = 0;

    for (; i < MSGSPERRM-1; i++) 
    {
        /* slide email pointers down */
        p->lbslot[i] = p->lbslot[1+i];
        p->lbId  [i] = p->lbId  [1+i];
    }
    p->lbId[MSGSPERRM-1]  = cfg.newest;         /* and slot this message in */
    p->lbslot[MSGSPERRM-1]= cfg.catSector;

    putLog(p, logno);
}


/*
** aNoteLogMessage() - log (& conditionally archive) a mail message
*/
static void aNoteLogMessage(struct logBuffer *who, int logno)
{
    noteLogMessage(who, logno);

    if (cfg.archiveMail) 
    {
        char *arcp;
        pathbuf sysopmail;

        arcp = &cfg.codeBuf[cfg.archiveMail];
        /*
         * the mail could be to sysop, and be aliased to this user, so we
         * need to look at the logrecord instead of the messagebuf for the
         * to address...
         */
        if ( stricmp(who->lbname,   arcp) == 0
          || stricmp(msgBuf.mbauth, arcp) == 0) 
        {
            ctdlfile(sysopmail, cfg.auditdir, "sysop.msg");
            msgToDisk(sysopmail,cfg.newest,cfg.catSector);
        }
    }
}


/*
** note2Message() - makes room for a new message.
*/
void note2Message(long id, unsigned int loc)
{
    int  i = 0;

    /* scroll all the message pointers to make room for another msg */

    for (; i < MSGSPERRM-1; i++) 
    {
        roomBuf.msg[i].rbmsgLoc  = roomBuf.msg[i+1].rbmsgLoc;
        roomBuf.msg[i].rbmsgNo   = roomBuf.msg[i+1].rbmsgNo ;
    }

    roomBuf.msg[MSGSPERRM-1].rbmsgNo  = id;
    roomBuf.msg[MSGSPERRM-1].rbmsgLoc = loc;
}


/*
** notemessage() - slots message into current room
*/
void notemessage(struct logBuffer *logptr, int logidx)
{
    struct logBuffer lbuf2;
    char *fn;
    char *findArchiveName();

    logBuf.lbvisit[0] = ++cfg.newest;

    if (thisRoom != MAILROOM) 
    {
        note2Message(cfg.newest, cfg.catSector);
        noteRoom();
        putRoom(thisRoom);
    }
    else
    {
        if (stricmp(msgBuf.mbto, "Sysop") == 0)  
        {
            if (cfg.sysopName && &cfg.codeBuf[cfg.sysopName]) 
            {
                logidx = getnmlog(&cfg.codeBuf[cfg.sysopName], &lbuf2);
                if (logidx == ERROR)
                    mPrintf("\n Lost track of Sysop.\n ");
            }
            else
                logidx = ERROR;

            if (logidx != ERROR)
                aNoteLogMessage(&lbuf2, logidx);
            else
            {
                getRoom(AIDEROOM);
                /*
                 * enter in Aide> room -- 'sysop' is special
                 */
                note2Message(cfg.newest, cfg.catSector);
                /*
                 * write it to disk
                 */
                noteRoom();
                putRoom(AIDEROOM);
                getRoom(MAILROOM);
            }
        }
        else if (stricmp(msgBuf.mbto, "Citadel") == 0) 
        {
            for (logidx = 0; logidx < cfg.MAXLOGTAB; logidx++) 
            {
                getLog(&lbuf2, logidx);
                if (lbuf2.lbflags.L_INUSE)
                    aNoteLogMessage(&lbuf2, logidx);
            }
        }
        else if (logptr && logidx != ERROR)
            aNoteLogMessage(logptr, logidx);    /* note in recipient    */

        if (loggedIn) 
        {
            if (logindex != logidx)
                noteLogMessage(&logBuf, logindex);/* note in ourself    */
            update_mail();                      /* update room also     */
        }
    }

    msgBuf.mbto[0] = 0;
    cfg.catSector= msgs.thisSector;
    cfg.catChar  = msgs.thisChar;

    if (roomBuf.rbflags.ARCHIVE) 
    {
        if ((fn = findArchiveName(thisRoom)) == NULL) 
        {
            sprintf(msgBuf.mbtext, "Archive error in `%s'.",roomBuf.rbname);
            aideMessage(NO);
        }
        else
            msgToDisk(fn, roomBuf.msg[MSGSPERRM-1].rbmsgNo,
                roomBuf.msg[MSGSPERRM-1].rbmsgLoc);
    }
}


/*
** msgToDisk() - Puts a message to the given disk file
*/
void msgToDisk(char *filename, long id, unsigned int loc)
{
    if (ARsetup(filename)) 
    {
        printmessage(loc, id);
        sendARend();
    }
    else
    {
        mPrintf("ERROR: Couldn't open output file %s\n ", filename);
    }
}


/*
** storeMessage() - save a message
*/
void storeMessage(struct logBuffer *logptr, int logidx)
{
    if (putmessage(NULL))
    {
        notemessage(logptr, logidx);
    }
}


/*
** aideMessage() - saves auto message in Aide>
*/
void aideMessage(int noteDeletedMessage)
{
    int ourRoom;

    /* Ensures not a net message    */
    msgBuf.mboname[0]= msgBuf.mborig[0] =
    msgBuf.mborg[0]  = msgBuf.mbdate[0] =
    msgBuf.mbtime[0] = msgBuf.mbsubject[0] = 0;

    /* message is already set up in msgBuf.mbtext */
    putRoom(ourRoom = thisRoom);
    getRoom(AIDEROOM);

    strcpy(msgBuf.mbauth, program);
    msgBuf.mbto[0] = 0;
    storeMessage(NULL, ERROR);

    if (noteDeletedMessage)
    {
        note2Message(pullMId, pullMLoc);
    }
    noteRoom();
    putRoom(AIDEROOM);

    getRoom(ourRoom);
}
