/*{ $Workfile:   netrcv.c  $
 *  $Revision:   1.14  $
 * 
 *  Slave mode networker
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
#include <string.h>
#include <stdlib.h>
#include "ctdl.h"
#include "externs.h"
#include "ctdlnet.h"
#include "dirlist.h"
#include "protocol.h"

/*  GLOBAL Contents:
**
**  issharing()     Is this room netting with caller?
**  called()        Deal with a network caller
**  slavemode()     Receive stuff, like it says.
**  netopt()        parse an options string.
**  doSetup()       set up for networking with a specific system
**  doResults()     Processes results
*/

extern char     checkNegMail;

struct cmd_data 
{
    unsigned command;
    char arg[4][NAMESIZE];
} ;

static char processMail;

char sr_sent[SHARED_ROOMS];
char sr_rcvd[SHARED_ROOMS];

char netrmspool[] = "$netroom.%d";
char netmlspool[] = "$tmpmail";

static char oops[] = "System error";    /* `huh?' reply for OS oddities. */

/*** static function prototypes ***/

static void     reply       (int state, char *reason);
static unsigned getnetcmd   (struct cmd_data *cmd, int login);
static void     chkproto    (char *optstr);
static void     catchfile   (char *fname, char *size);
static void     getnetlogin (void);
static void     check_mail  (void);
static void     targetCheck (void);
static void     doNetRooms  (void);
static void     getnetmail  (void);
static void     flingfile   (char *room, char *files);
static void     catchroom   (char *rmname);


/*
** reply() Replies to caller
*/
static void reply (int state, char *reason)
{
    if (beginWC()) 
    {
        (*sendPFchar)(state);

        if (state == NO) 
        {
            wcprintf("%s", reason);
            splitF(netLog, "Reply BAD: %s.\n", reason);
        }
        endWC();
    }
    else
    {
        neterror(YES, "Reply hangup");
    }
}


/*
** issharing() - See if given room slot is networking with system
**               that has just called
*/
int issharing (int slot)
{
    register int index, room;

    for (index = 0; index < SHARED_ROOMS; ++index) 
    {
        room = netBuf.shared[index].NRidx;

        if ((room == slot)
          && (netBuf.shared[index].NRgen == roomTab[slot].rtgen))
        {
            return index;
        }
    }
    return ERROR;
}


/*
** called() - deal with a network caller.
*/
void called (void)
{
    usingWCprotocol = XMODEM;
    getnetlogin();
    if (gotcarrier()) 
    {
        doSetup();
        slavemode(NO);
        splitF(netLog, "Done with %s (%s)\n", rmtname, tod(NO));
        hangup();
        doResults();
    }
}


/*
** getnetcmd() Gets next command from caller
*/
static unsigned getnetcmd (struct cmd_data *cmd, int login)
{
    register i;

    for (i = 0; i < SECTSIZE; ++i)
    {
        sectBuf[i] = 0;
    }
    zero_struct(*cmd);

    counter = 0;

    if (login)
    {
        sectBuf[counter++] = LOGIN;
    }

    if (enterfile(increment, usingWCprotocol) && gotcarrier()) 
    {
        register char *ptr = sectBuf;

        cmd->command = 0xff & sectBuf[0];

        for (i = 0; *++ptr && (i < 4); ++i)  
        {
            copystring(cmd->arg[i], ptr, NAMESIZE);
            ptr = EOS(ptr);
        }
    }
    else
    {
        hangup();
        cmd->command = HANGUP;
    }

    return cmd->command;
}



/*
** slavemode() - process net commands
*/
void slavemode (int reversed)
{
    label msg;
    struct cmd_data cmd;

    while (gotcarrier())
    {
        switch (getnetcmd(&cmd, NO)) 
        {
        case HANGUP:
            hangup();
            return;

        case SEND_MAIL:
            getnetmail();
            break;

        case CHECK_MAIL:
            check_mail();
            break;

        case BATCH_REQUEST:
            flingfile(cmd.arg[0], cmd.arg[1]);
            break;

        case SEND_FILE:
            catchfile(cmd.arg[0], cmd.arg[2]);
            break;

        case NET_ROOM:
            catchroom(cmd.arg[0]);
            break;

        case ROLE_REVERSAL:
            if (reversed)
            {
                reply(NO, "double role-reverse");
            }
            else if (rmtslot != ERROR) 
            {
                splitF(netLog, "[master]\n");
                reply(YES, NULL);
                mastermode(YES);
            }
            else
            {
                reply(NO, "No");
            }
            return;

        case OPTIONS:
            chkproto(cmd.arg[0]);
            break;

        default:
            sprintf(msg, "<%d> unknown", cmd.command);
            reply(NO, msg);
            break;
        }
    }
}


/*
** netopt() - parse a network options string and return a pointer
**            to the list of options we will support during this session.                                             *
*/
char *netopt (char *optstr, char *proto)
{
    for (; *optstr; ++optstr)
    {
        switch (*optstr) 
        {
        case 'y':
            if (!netymodem)
            {
                break;
            }
            *proto = YMODEM;
            return "y";
            
        case 'x':
            *proto = XMODEM;
            return "x";
        }
    }
    *proto = XMODEM;            /* always set to base level protocol */
    return NULL;                /* and return a FAIL code */
}


/*
** chkproto() - handle the OPTIONS command 
*/
static void chkproto (char *optstr)
{
    char *rec;
    char proto;
    extern char usingWCprotocol;
    extern char *protocol[];

    if (rec = netopt(optstr, &proto)) 
    {
        if (netWCstartup("Proto")) 
        {
            /* "Y" packet has protocol in it */
            wcprintf("%c%s", YES, rec);
            endWC();
        }
        splitF(netLog, "(%s)\n", protocol[usingWCprotocol=proto]);
    }
    else
    {
        reply(NO, optstr);
    }
}


/*
** catchfile() Receive a sent file
*/
static void catchfile (char *fname, char *size)
{
    label fn;
    int count=0;

    strcpy(fn, fname);
    splitF(netLog, "Receiving file `%s'\n", fn);

    if (netchdir(&cfg.codeBuf[cfg.receiptdir])) 
    {
        if (atol(size) > sysRoomLeft())
            reply(NO, "No room for file");
        else
        {
            while (upfd = safeopen(fn, "rb")) 
            {
                fclose(upfd);
                sprintf(fn, "a.%d", count++);
            }
            upfd = safeopen(fn, "wb");
            reply(YES, NULL);
            enterfile(sendARchar, usingWCprotocol);
            fclose(upfd);
            neterror(NO, count ? "received %s (saved as %s)"
                : "received %s", fname, fn);
        }
    }
    else
    {
        reply(NO, oops);
    }
    homeSpace();
}


/*
** doSetup() -- set up for networking with a specific system
*/
void doSetup (void)
{
    register int i;

    processMail = checkNegMail = NO;

    for (i = 0; i < SHARED_ROOMS; ++i) 
    {
        sr_sent[i] = NO;
        sr_rcvd[i] = NO;
    }
}


/*
** doResults() -- processes results of receiving thingies and such.
*/
void doResults (void)
{
    register int i;

    if (processMail)
    {
        readMail(YES, inMail);
    }
    if (rmtslot == ERROR)
    {                           /* unregistered callers can't */
        return;                 /* do anything but send mail  */
    }

    if (checkNegMail)
    {
        readNegMail();
    }

    doNetRooms();

    /* update shared room indices for all rooms that we sent to the
    ** other system.
    */

    for (i = 0; i < SHARED_ROOMS; ++i)
    {
        if (sr_sent[i])
        {
            netTab[thisNet].Tshared[i].NRlast
                = netBuf.shared[i].NRlast
                = cfg.newest;
        }
    }

    putNet(thisNet);
}


/*
** getnetlogin() - get the network login packet from caller
*/
static void getnetlogin (void)
{
    struct cmd_data login;

    if (getnetcmd(&login, YES) != HANGUP) 
    {
        normID(login.arg[0], rmt_id);
        strcpy(rmtname, login.arg[1]);

        if (rmtname[0] == 0)
        {
            neterror(YES, "Bad login packet");
        }
        else if ((rmtslot = srchNet(rmt_id)) == ERROR)
        {
            neterror(NO, "New caller %s (%s)", rmtname, rmt_id);
        }
        else if (login.arg[2][0] && login.arg[2][0] != ' '
          && strcmp(login.arg[2], netBuf.myPasswd))
        {
            neterror(YES, "Bad password `%s'", login.arg[2]);
        }

        splitF(netLog, "Call from %s (%s)\n", rmtname, rmt_id);
    }
}


/*
** check_mail() does negative acks on netMail
*/
static void check_mail (void)
{
    if (processMail) 
    {
        reply(YES, NULL);
        splitF(netLog, "Checking mail\n");

        if (beginWC()) 
        {
            readMail(NO, targetCheck);
            endWC();
        }
    }
    else
    {
        reply(NO, "No mail to check");
    }
}

/*
** targetCheck() - checks for existence of recipients
*/
static void targetCheck(void)
{
    if (msgBuf.mbto[0] && postmail(NO))
    {
        return;
    }

    wcprintf("%c%s", msgBuf.mbto[0] ? NO_RECIPIENT : BAD_FORM, msgBuf.mbauth);
    wcprintf("%s", msgBuf.mbto);
    wcprintf("%s @ %s", msgBuf.mbdate, msgBuf.mbtime);
}


/*
** doNetRooms() integrates temporary files into data base
*/
static void doNetRooms (void)
{
    pathbuf tempfile;
    int i;
    int roomNo;

    init_zap();         /* turn on the loop zapper */

    for (i = 0; i < SHARED_ROOMS; i++) 
    {
        if (sr_rcvd[i]) 
        {
            ctdlfile(tempfile, cfg.netdir, netrmspool,
                roomNo=netBuf.shared[i].NRidx);

            if ((spool = safeopen(tempfile, "rb")) == NULL) 
            {
                neterror(YES, "Can't open %s", tempfile);
                return;
            }
            getRoom(roomNo);
            while (getspool()) 
            {
                /*
                 * make ABSOLUTELY sure that the routing address is
                 * properly set up
                 */
                if (!msgBuf.mbroute[0])
                    sprintf(msgBuf.mbroute, "@L%s", rmt_id);
                /*
                 * forbid message-bouncing
                 */
                if (stricmp(&cfg.codeBuf[cfg.nodeId], msgBuf.mborig) != 0) 
                {
                    /*
                     * Set the message route char to reflect what this
                     * node thinks the link is.
                     *
                     * ?->Local : @H, @O -> @L
                     * ?->Hub   : @L, @O -> @H
                     *
                     * Unless the room is a forwarder, that is...
                     */

                    ROUTECHAR(&msgBuf) =
                        (netBuf.shared[i].NRhub ? ROUTE_HUB : ROUTE_LOCAL);

                    if (notseen())
                        storeMessage(NULL, ERROR);
                    else
                        splitF(netLog, "reject %s <%s in %s, %s@%s>\n",
                            msgBuf.mboname,
                            msgBuf.mbauth, msgBuf.mbroom,
                            msgBuf.mbdate, msgBuf.mbtime);
                }
            }
            fclose(spool);
            dunlink(tempfile);
        }
    }
    close_zap();        /* turn off loop zapping stuff */
}


/*
** getnetmail() - Grabs mail from caller
*/
static void getnetmail (void)
{
    char    success;
    pathbuf tempfile;

    splitF(netLog, "Receiving mail\n");
    ctdlfile(tempfile, cfg.netdir, netmlspool);
    if (upfd = safeopen(tempfile, "wb")) 
    {
        reply(YES, NULL);
        success = enterfile(sendARchar, usingWCprotocol);
        fclose(upfd);
        if (success)
            processMail = YES;
        else
            dunlink(tempfile);
    }
    else
    {
        reply(NO, oops);
    }
}


/*
** flingfile() - sendfile driver
*/
static void flingfile (char *room, char *files)
{
    int  roomSlot;
/*
    extern char usingWCprotocol;
    extern char batchWC;
*/
    char msg[60];
    char netproto = usingWCprotocol;

    splitF(netLog, "Request %s (%s)\n", files, room);

    if ((roomSlot = roomExists(room)) == ERROR
      ||  ! ( roomTab[roomSlot].rtflags.ISDIR
           && roomTab[roomSlot].rtflags.NETDOWNLOAD) ) 
    {
        sprintf(msg, "(%s) does not exist", room);
        reply(NO, msg);
        return;
    }
    getRoom(roomSlot);

    if (netchdir(roomBuf.rbdirname)) 
    {
        reply(YES, NULL);
        batchWC = YES;          /* xmodem batch download... */

        usingWCprotocol = YMODEM;
        netwildcard(download, files);
        usingWCprotocol = netproto;
    }
    else
    {
        reply(NO, oops);
    }
}


/*
** catchroom() - Get a shared room
*/
static void catchroom (char *rmname)
{

    int  slot;
    char success;
    int  arraySlot;
    char *p;
    char msg[60];
    pathbuf tempfile;

    p = chk_name(net_alias, thisNet, rmname);

    if ((slot = roomExists(p)) == ERROR) 
    {
        sprintf(msg, "(%s) does not exist", p);
        reply(NO, msg);
        return;
    }

    if ( rmtslot == ERROR
      || !roomTab[slot].rtflags.SHARED
      || (arraySlot = issharing(slot)) == ERROR) 
    {
        sprintf(msg, "(%s) isn't shared with you", p);
        reply(NO, msg);
        return;
    }

    ctdlfile(tempfile, cfg.netdir, netrmspool, slot);

    if ((upfd = safeopen(tempfile, "wb")) == NULL) 
    {
        reply(NO, oops);
        return;
    }

    reply(YES, NULL);
    splitF(netLog, (p == rmname) ? "Receiving %s\n" : "Receiving %s(%s)\n",
        p, rmname);

    success = enterfile(sendARchar, usingWCprotocol);
    fclose(upfd);

    if (success)
    {
        sr_rcvd[arraySlot] = YES;
    }
    else
    {
        dunlink(tempfile);
    }
}
