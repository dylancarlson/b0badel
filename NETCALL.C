/*{ $Workfile:   netcall.c  $
 *  $Revision:   1.12  $
 * 
 *  master mode Citadel networking
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
 
#include <io.h>
#include <stdarg.h>
#include <string.h>
#include "ctdl.h"
#include "externs.h"
#include "ctdlnet.h"
#include "dirlist.h"
#include "protocol.h"

/* GLOBAL Contents:
**
**  netWCstartup()      does beginWC, issues neterror() on failure
**  caller()            Network with callee
**  mastermode()        assume role of sender
**  readNegMail()       reads and processes negative acks
**  sendSharedRooms()   Sends all shared rooms to receiver
**  netcommand()        Sends a command to the receiver
**  s_n_m()             Send normal mail
**  s_f_m()             Send forwarded mail
*/

/* prototypes for static functions */

static void netlogin        (void);
static int  caller_stabilize(void);
static void sendMail        (void);
static void checkMail       (void);
static void throwMessages   (char *p, int index);
static void sendSharedRooms (void);
static int  s_n_m           (void);
static int  s_f_m           (void);


/*
** netWCstartup()
*/
int netWCstartup (char *from)
{
    if (beginWC())
    {
        return YES;
    }
    neterror(YES, "%s froze", from);
    return NO;
}


/*
** netlogin() Sends ID to the receiver
*/
static void netlogin (void)
{
    netcommand(LOGIN, &cfg.codeBuf[cfg.nodeId],
        &cfg.codeBuf[cfg.nodeName],
        netBuf.herPasswd,
        NULL);
}


/*
** caller() -- called someone for networking
*/
int caller (void)
{
    char proto;

    splitF(netLog, "Have Carrier\n");

    if (!caller_stabilize())
    {
        return NOT_STABILISED;
    }

    usingWCprotocol = XMODEM;
    netlogin();

    if (gotcarrier()) 
    {
        doSetup();

        if (netcommand(OPTIONS, netymodem ? "yx" : "x", NULL))
        {
            if (netopt(1+sectBuf, &proto))
            {
                splitF(netLog, "(%s)\n", protocol[usingWCprotocol=proto]);
            }
        }
        mastermode(NO);

        doResults();
        splitF(netLog, "(%s)\n\n", tod(NO));
        return CALL_OK;
    }
    else
    {
        return NO_ID;
    }
}


/*
**  mastermode() assume role of sender
*/
void mastermode (int reversed)
{
    if (netBuf.nbflags.mailpending) 
    {
        if (gotcarrier())
            sendMail();
        if (gotcarrier())
            checkMail();
    }
    if (gotcarrier())
        sendSharedRooms();
    if (gotcarrier())
        nfs_process();
    if (gotcarrier())
        if ((netBuf.nbflags.ld_rr || !netBuf.nbflags.ld)
            && !reversed && netcommand(ROLE_REVERSAL, NULL)) 
        {
            splitF(netLog, "[slave]\n");
            slavemode(YES);
        }
        else
            netcommand(HANGUP, NULL);
    hangup();
}


/*
**  caller_stabilize() - Tries to stabilize the network call
*/
static int caller_stabilize (void)
{
    int tries;

    pause(20);

    for (tries = 0; tries < 20 && gotcarrier(); ++tries) 
    {
        register int x1, x2, x3, i;

        modputs("\007\007\007\rE");     /* send magic 7,13,69 Citadel ID */

        for (i = 50; i--;)
        {
            x1 = receive(1);

            if (x1 == ERROR || x1 == 248)
            {
                break;
            }

            if (netDebug)
            {
                splitF(netLog, "%c", x1);
            }
        }

        if (x1 == 248) 
        {
            if ((x2 = receive(2)) != ERROR)
            {
                x3 = receive(2);
            }

            if (x2 == 242 && x3 == 186)     /* bingo! */
            {
                modout(ACK);

                if (netDebug)
                {
                    splitF(netLog, "[ACK]\n");
                }

                return gotcarrier();
            }
            else if (netDebug)
            {
                splitF(netLog, "<%d,%d>", x2, x3);
            }
        }
    }

    splitF(netLog, "Not stabilized\n");
    hangup();
    return FALSE;
}


/*
** sendMail() - send mail to network
*/
static void sendMail (void)
{
    pathbuf fn;

    splitF(netLog, "Sending mail.\n");
    if (netcommand(SEND_MAIL, NULL) && netWCstartup("Mail")) 
    {
        splitF(netLog, "Sent %s.\n", plural("message", (long)s_n_m()) );
        if (gotcarrier()) 
        {
            ctdlfile(fn, cfg.netdir, "%d.fwd", thisNet);

            if ((spool = safeopen(fn, "rb")) != NULL) 
            {
                splitF(netLog, "Forwarded %s.\n", plural("message",(long)s_f_m()));
                fclose(spool);
            }
            if (gotcarrier()) 
            {
                endWC();
                dunlink(fn);
                netBuf.nbflags.mailpending = NO;
            }
        }
    }
}


/*
** checkMail() -- negative acknowledgement on netmail
*/
static void checkMail (void)
{
    pathbuf chkfile;

    ctdlfile(chkfile, cfg.netdir, netmlcheck);

    if ((upfd = safeopen(chkfile, "wb")) != NULL) 
    {
        splitF(netLog, "Asking for checkmail\n");

        if ( netcommand(CHECK_MAIL, NULL)
          && enterfile(sendARchar, usingWCprotocol))
        {
            checkNegMail = YES;
        }
        fclose(upfd);
    }
    else
    {
        neterror(NO, "Couldn't create %s", chkfile);
    }
}


/*
** readNegMail() -- reads and processes negative acks
*/
void readNegMail (void)
{
    NA author, target;
    pathbuf context, chkfile;
    int key;

    ctdlfile(chkfile, cfg.netdir, netmlcheck);

    if ((spool = safeopen(chkfile, "rb")) == NULL)
    {
        return;
    }

    while ((key = getc(spool)) != EOF && key != NO_ERROR) 
    {
        zero_struct(msgBuf);
        strcpy(msgBuf.mbauth, program);
        getstrspool(author, ADDRSIZE);
        getstrspool(target, ADDRSIZE);
        getstrspool(context, ADDRSIZE);

        if (key == NO_RECIPIENT
            && strlen(author) > 0
            && stricmp(author, program) == 0) 
        {
            strcpy(msgBuf.mbto, author);
            sprintf(msgBuf.mbtext, "Cannot deliver mail to %s@%s (%s)",
                target, rmtname, context);
            postmail(YES);
        }
    }
    fclose(spool);
    dunlink(chkfile);
}


/*
** throw messages from a room
*/
static void throwMessages(char *p, int index)
{
    register idx;
    int  room;
    char *tosend;
    long lastmsg = 0;

    /* 
     * Hub -> ?   : @L, @O -> @H
     * Local -> ? : @H, @O -> @L
     */
    if (netBuf.shared[index].NRhub)     /* figure out sending classes   */
    {   
        tosend = "OLH";                 /* send all messages here       */
        route_char = ROUTE_HUB;         /* mapped to @H                 */
    }
    else
    {
        tosend = "OH";                  /* send originate & hub messages*/
        route_char = ROUTE_LOCAL;
    }

    if (!netWCstartup("Share"))
    {
        return;
    }

    /*
     * send messages that are > than the last message we sent and that
     * citadel can find and that have the route-char set and are
     * properly routable.
     */

    for (idx = 0; idx < MSGSPERRM; ++idx)
    {
        if ( netBuf.shared[index].NRlast < roomBuf.msg[idx].rbmsgNo
          && findMessage(roomBuf.msg[idx].rbmsgLoc, roomBuf.msg[idx].rbmsgNo)) 
        {
            #ifdef DEBUG
                splitF(netLog, "message#%ld,route=<%s>,auth=<%s>\n",
                    roomBuf.msg[idx].rbmsgNo,
                    msgBuf.mbroute,
                    msgBuf.mbauth);
            #endif

            if (ROUTEOK(&msgBuf) && strchr(tosend, ROUTECHAR(&msgBuf))
                && stricmp(ROUTEFROM(&msgBuf), rmt_id) != 0) 
            {
                netPrintMsg(roomBuf.msg[idx].rbmsgLoc,roomBuf.msg[idx].rbmsgNo);
                /*
                 * update lastMess to last message actually sent...
                 */
                if (roomBuf.msg[idx].rbmsgNo > lastmsg)
                {
                    lastmsg = roomBuf.msg[idx].rbmsgNo;
                }
            }
        }
    }

    if (WCError)
    {
        hangup();
    }
    else
    {
        endWC();
        #ifdef DEBUG
            splitF(netLog, "old lastMess=%ld, lastmsg=%ld\n",
                netBuf.shared[index].NRlast, lastmsg);
        #endif
        sr_sent[index] = YES;
    }
}


/*
** sendSharedRooms() -- Sends all shared rooms to receiver
*/
static void sendSharedRooms (void)
{
    int msgRover;
    int idx;
    char *p;

    for (idx = 0; gotcarrier() && idx < SHARED_ROOMS; ++idx) 
    {
        if (sharing(idx))
        {
            if (mesgpending(&netBuf.shared[idx])) 
            {
                getRoom(netBuf.shared[idx].NRidx);
                p = chk_alias(net_alias, thisNet, roomBuf.rbname);

                splitF(netLog, (p == roomBuf.rbname)
                    ? "Sending %s\n"
                    : "Sending %s(%s)\n", roomBuf.rbname, p);

                if (netcommand(NET_ROOM, p, NULL))
                {
                    throwMessages(p, idx);
                }
            }
            else
            {                           /* nothing to send is the moral */
                sr_sent[idx] = YES;     /* equivalent of sending stuff  */
            }
        }
    }
}


/*
** netcommand() Sends a command
*/
int netcommand (int cmd, ...)
{
    va_list args;
    char *p;
    int count;

    if (!netWCstartup("Cmd"))
    {
        return NO;
    }

    if (cmd != LOGIN)
    {
        (*sendPFchar)(cmd);
    }

    va_start(args, cmd);

    for (count = 0; count < 4 && (p = va_arg(args, char*)) != NULL; count++)
    {
        wcprintf("%s", p);
    }
    va_end(args);

    (*sendPFchar)(0);
    endWC();

    counter = 0;

    if (cmd == HANGUP || cmd == LOGIN)  /* hup?  Okay... */
    {
        return YES;
    }

    if (enterfile(increment, usingWCprotocol) && gotcarrier()) 
    {
        if (sectBuf[0])
        {
            return YES;
        }
        if (cmd != OPTIONS)
        {
            neterror(NO, "replies `%s'", &sectBuf[1]);
        }
    }
    else
    {
        neterror(YES, "<%d> hangup", cmd);
    }
    return NO;
}


/*
** s_n_m() - Send normal mail
*/
static int s_n_m (void)
{
    int     msgs = 0;
    int     ournet = thisNet;
    int     cost;
    int     loc;
    long    id;
    FILE    *mailfile;
    pathbuf filename;
    char    temp[80];
    NA      to;
    label   first;
    char    c;

    ctdlfile(filename, cfg.netdir, "%d.ml", thisNet);

    if ((mailfile = safeopen(filename, "r")) == NULL)
    {
        return 0;
    }

    route_char = 0;

    while (fgets(temp, 80, mailfile) && !WCError)
    {
        if (sscanf(temp, "%ld %d", &id, &loc) == 2 && findMessage(loc,id)) 
        {
            strcpy(to, msgBuf.mbto);
            parsepath(to, first, YES);          /* get pathname and */
            findnode(first, &cost);             /* modify to: field */
            changeto(to, first);

            sendXmh();
            (*sendPFchar)('M');

            while ((*sendPFchar)(c = getmsgchar()) && c)
                ;

            ++msgs;
        }
    }
    fclose(mailfile);
    getNet(ournet);

    if (WCError) 
    {
        hangup();
        return 0;
    }

    dunlink(filename);
    return msgs;
}


/*
** s_f_m() -- Send forwarded mail
*/
static int s_f_m (void)
{
    register i,c;
    int count = 0;
    route_char = 0;

    while (getspool() && !WCError) 
    {
        sendXmh();
        (*sendPFchar)('M');

        for (i = 0; ((c = msgBuf.mbtext[i]) != 0) && (!WCError); ++i)
        {
            (*sendPFchar)(c);
        }
        (*sendPFchar)(0);
        ++count;
    }
    if (WCError) 
    {
        hangup();
        return 0;
    }
    return count;
}
