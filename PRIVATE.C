/*{ $Workfile:   private.c  $
**  $Revision:   1.11  $
**
**  part of b0badel BBS
**
**  Contains:
**  int privateReply()
**  int doMailFirst()
}*/

#include <stdlib.h>
#include <string.h>
#include "ctdl.h"
#include "protocol.h"
#include "externs.h"


/* privateReply()
**
** The idea here is to allow private responses in the Mail room to
** public messages in other rooms.  The response is made via an [M]
** (for Mail) option in the More prompt (see msg.c).
*/
int privateReply(void)
{
    label               sender;
    label               receiver;
    struct logBuffer    log_buffer;
    int                 log_index;
    int                 program_flag;
    int                 saveroom;
    int                 got_text;
    int                 upper;
    char                *text;

    if (!loggedIn)
    {
        mPrintf("Login first!\n ");
        return NO;
    }

    strcpy(receiver, msgBuf.mbauth);
    strcpy(sender, logBuf.lbname);


    /*** don't allow replies to "Citadel" or unknown users ***/

    program_flag = (stricmp(receiver, program) == 0);

    log_index = getnmlog(msgBuf.mbauth, &log_buffer);

    if ((log_index == ERROR) || program_flag)
    {
        if (!receiver[0])
        {
            strcpy(receiver, "anonymous");
        }
        mPrintf("Can't reply to %s\n ", receiver);
        return NO;
    }

    /*** construct new message buffer ***/

    zero_struct(msgBuf);

    strcpy(msgBuf.mbto, receiver);
    strcpy(msgBuf.mbauth, sender);

    saveroom = thisRoom;
    putRoom(saveroom);
    getRoom(MAILROOM);

    echo = CALLER;          /* make privacy in mail room */

    got_text = getText(ASCII);

    echo = BOTH;            /* restore echo to console */

    if (got_text)
    {
        /*** add lower case if message was all uppers ***/

        text = msgBuf.mbtext;
        upper = YES;

        while (*text && upper)
        {
            upper = (toupper(*text) == *text);
            text ++;
        }
        if (upper)
        {
            fakeFullCase(msgBuf.mbtext);
        }

        /*** store message in mail room ***/

        storeMessage(&log_buffer, log_index);

        /*** get back to from where we came ***/

        putRoom(MAILROOM);
        getRoom(saveroom);
        return YES;
    }
    else  /* no text */
    {
        /*** get back to from where we came ***/

        putRoom(MAILROOM);
        getRoom(saveroom);
        return NO;
    }

}  /* end of privateReply() */


/* doMailFirst()
**
** This routine handles immediate mail processing by the user as s/he
** logs in, as was requested by some local (Sonoma County, CA) users.
** It is called by login() in log.c.
*/
int doMailFirst(void)
{
    long           lastmsg, lastcall;
    int            saveroom;

    lastmsg = logBuf.lbId [MSGSPERRM - 1];

    lastcall= logBuf.lbvisit [logBuf.lbgen [MAILROOM] & CALLMASK];


    if (lastmsg > lastcall && lastmsg > cfg.oldest)
    {
        doCR();
        mPrintf("* You have private mail in Mail> *");
        doCR();

        if (getNo("  Read your mail now"))
        {
            if (thisRoom == MAILROOM)
            {
                doRead(NO, 1, 'n');
            }
            else
            {
                saveroom = thisRoom;

                gotoRoom(roomTab [MAILROOM].rtname, 'S');

                doRead(NO, 1, 'n');

                doCR();
                mPrintf(" Returning to %s ", formRoom(saveroom, YES));
                doCR();

                gotoRoom(roomTab [saveroom].rtname, 'R');
            }
        }
    }
    return YES;

}  /* end of doMailFirst() */
