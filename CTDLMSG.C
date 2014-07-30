/*{ $Workfile:   ctdlmsg.c  $
 *  $Revision:   1.13  $
 * 
 *  routines to pull messages out of ctdlmsg.sys
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

#define CTDLMSG_C

#include <io.h>
#include <stdlib.h>
#include <string.h>
#include "ctdl.h"
#include "externs.h"
#include "ctdlnet.h"
#include "protocol.h"
#include "msg.h"                /* struct mBuf */

/* GLOBAL Contents:
**
**  getmsgchar()    returns sequential chars from message on disk
**  findMessage()   gets all set up to do something with a message
**  msgseek()       sets location to begin reading message from
**  sgetword()      get a word from a string
**  softWord()      writes one word to modem & console, soft CR's
**  outword()       like softWord(), except treats CR's as hard always
**  printmessage()  finds and prints out a message
**  doFindMsg()     finds a message, reports any error
**  doPrintMsg()    prints out header and message
**  getstrspool()   Gets a string from networked message
**  getspool()      Gets a message from a global file
*/

struct mBuf msgs = 
{
    0, ~0, 0, ~0
};

/*** prototypes for static functions ***/

static UINT msggetword  (char *dest, int lim);
static void getmsgstr   (char *dest, int lim);
static void getmessage  (void);
static void prtHdr      (void);
static void singleHdr   (void);
static void prtBody     (void);

/*** static variables ***/

static char prevChar = 0;
static UINT GMCCache = 0;       /* single character cache */


/*
** msggetword() - fetches one word from current message, off disk
**                returns nonzero if more stuff is following
*/
static UINT msggetword (char *dest, int lim)
{
    UINT c = getmsgchar();

    --lim;      /* play it safe */

    /* pick up any leading blanks: */

    for (; c == ' ' && c && lim; c = getmsgchar())
    {
        *dest++ = c;
        --lim;
    }

    if (!lim)
    {
        return c;
    }

    /* step through word: */

    for (; c != ' ' && c && lim; c = getmsgchar())
    {
        *dest++ = c;
        --lim;
    }

    if (!lim)
    {
        return c;
    }

    /* trailing blanks: */

    for (; c == ' ' && c && lim; c = getmsgchar())
    {
        *dest++ = c;
        lim--;
    }

    if (c)
    {
        GMCCache = c;       /* took one too many, cache it */
    }

    *dest = 0;              /* tie off string   */
    return c;
}


/*
** getmsgstr() - reads a string from messagefile
*/
static void getmsgstr (char *dest, int lim)
{
    unsigned c;

    while ((c = getmsgchar()) != 0)     /* read every character */
    {                                
        if (lim)                /* if we have room then */
        {
            lim--;
            *dest++ = c;        /* copy char to buffer  */
        }
    }

    if (!lim)
    {
        dest--;                 /* Ensure not overwrite next door neighbor */
    }

    *dest = 0;                  /* tie string off with null  */
}


/*
** getmessage() - load a message into msgBuf. (the location was set earlier)
*/
static void getmessage (void)
{
    UINT c;

    msgBuf.mbroute  [0] = 0;
    msgBuf.mborg    [0] = 0;
    msgBuf.mbauth   [0] = 0;
    msgBuf.mbtime   [0] = 0;
    msgBuf.mbdate   [0] = 0;
    msgBuf.mborig   [0] = 0;
    msgBuf.mboname  [0] = 0;
    msgBuf.mbroom   [0] = 0;
    msgBuf.mbtext   [0] = 0;
    msgBuf.mbto     [0] = 0;
	msgBuf.mbsubject[0] = 0;

    msgBuf.mbcost = 0;

    while (getmsgchar() != 0xff)
        ;

    getmsgstr(msgBuf.mbid, NAMESIZE);

    while ((c = getmsgchar()) != 'M' && isalpha(c)) 
    {
        switch (c) 
        {
        case 'A':       getmsgstr(msgBuf.mbauth,  ADDRSIZE);
            break;
        case 'T':       getmsgstr(msgBuf.mbto,    ADDRSIZE);
            break;
        case 'D':       getmsgstr(msgBuf.mbdate,  NAMESIZE);
            break;
        case 'C':       getmsgstr(msgBuf.mbtime,  NAMESIZE);
            break;
        case 'R':       getmsgstr(msgBuf.mbroom,  NAMESIZE);
            break;
        case 'I':       getmsgstr(msgBuf.mborg,   ORGSIZE);
            break;
        case 'N':       getmsgstr(msgBuf.mboname, NAMESIZE);
            break;
        case 'O':       getmsgstr(msgBuf.mborig,  NAMESIZE);
            break;
        case 'J':       getmsgstr(msgBuf.mbsubject, ORGSIZE);
            break;
        case 'Z':       getmsgstr(msgBuf.mbroute, NAMESIZE+2);
            break;
        default:
            getmsgstr(msgBuf.mbtext, MAXTEXT);  /* discard unknown field  */
            msgBuf.mbtext[0] = 0;
            break;
        }
    }

    if (c == 0xff)
    {
        GMCCache = c;
    }
}


/*
** getmsgchar() -- reads a character from messagefile
*/
UINT getmsgchar (void)
{
    UINT gotten;

    if (GMCCache)           /* char in cache -- return it */
    {                   
        gotten = GMCCache;
        GMCCache = 0;
    }
    else
    {
        msgs.oldChar   = msgs.thisChar;
        msgs.oldSector = msgs.thisSector;

        gotten = 0xff & msgs.sectBuf[msgs.thisChar++];

        if ((msgs.thisChar %= BLKSIZE) == 0) 
        {
            /* time to read next sector in: */
            msgs.thisSector = (++(msgs.thisSector)) % cfg.maxMSector;

            dseek(msgfl, ((long)BLKSIZE)*(long)(msgs.thisSector), 0);

            if (dread(msgfl, msgs.sectBuf, BLKSIZE) != BLKSIZE)
            {
                crashout("msgfile read error");
            }
            crypte(msgs.sectBuf, BLKSIZE, 0);
        }
    }
    return gotten;
}



/*
** findMessage() gets all set up to do something with a message
*/
int findMessage (int loc, long id)
{
    for (msgseek(loc, 0); msgs.thisSector == loc; ) 
    {
        getmessage();

        if (id == atol(msgBuf.mbid))
        {
            return YES;
        }
    }
    return NO;
}


/*
** msgseek() -- sets location to begin reading message from
*/
void msgseek (UINT sector, int byteptr)
{
    GMCCache  = 0;      /* init character cache */

    if (sector >= cfg.maxMSector)
    {
        crashout("msgseek error (sector %u is beyond last sector)", sector);
        return;
    }

    msgs.thisChar = byteptr;

    if (sector != msgs.thisSector) 
    {
        msgs.thisSector = sector;

        dseek(msgfl, (long)sector * (long)BLKSIZE, 0);

        if (dread(msgfl, msgs.sectBuf, BLKSIZE) != BLKSIZE)
        {
            crashout("msgfile read of sector %u failed", sector);
        }
        crypte(msgs.sectBuf, BLKSIZE, 0);
    }
}


/*
** sgetword() - get a word from a string
*/
int sgetword (char *dest, char *source, int offset, int lim)
{
    int i, j;

    /* skip leading blanks if any */
    for (i = 0; source[offset+i] == ' ' && i < lim; i++)
        ;

    /* step over word */

    for (;source[offset+i] != ' ' && i <  lim && source[offset+i] != 0; i++ )
        ;

    /* pick up any trailing blanks */

    for (;  source[offset+i]==' ' && i<lim;  i++)
        ;

    /* copy word over */

    for (j = 0; j < i; j++)
    {
        dest[j] = source[offset+j];
    }
    dest[j] = 0;        /* null to tie off string */

    return offset+i;
}


/*
** softWord() writes one word to modem & console, but only performs
**      a hard carriage return on "\n " ("\nwhatever" don't cut it)
*/
void softWord (char *st)
{
    if (outFlag == OUTNEXT)
    {
        return;
    }
    
    {   /*** calc length of word to doCR() first if necessary ***/

        char *ptr = st;
        char oldchar = prevChar;
        int  newcolumn = crtColumn;

        while (*ptr)   
        {
            if ((oldchar == '\n') && (*ptr == ' '))
            {
                newcolumn = 1;
            }

            if (*ptr == TAB)
            {
                ++newcolumn;

                while (newcolumn % 8)
                {
                    ++newcolumn;
                }
            }
            else if (*ptr >= ' ')
            {
                ++newcolumn;
            }

            oldchar = *ptr++;

            if (newcolumn >= termWidth)
            {
                break;
            }
        }

        if (newcolumn >= termWidth)     /* it will overflow, so doCR() now */
        {
            doCR();

            if (*st == ' ' || *st == '\n')
            {
                ++st;
            }
            prevChar = ' ';
        }
    }

    for (; *st; ++st) 
    {
        if (mAbort() || (outFlag == OUTNEXT))
        {
            return;
        }

        if (*st == TAB)
        {
            oChar (' ');

            while ((crtColumn % 8) && (crtColumn < termWidth))
            {
                oChar (' ');
            }
            if (crtColumn == termWidth)
            {
                doCR();
            }

            prevChar = ' ';
            continue;
        }

        if ((*st == '\n') || (*st == '\r'))
        {
            prevChar = '\n';
            continue;           /* don't doCR() until it's followed by ' ' */
        }

        /* worry about words longer than a line:        */

        if (crtColumn >= termWidth)
        {
            doCR();
            prevChar = ' ';
        }

        if (prevChar == '\n')   /* just passed a newline */
        {
            if (*st == ' ')     /* space marks end of paragraph */
            {
                if (outFlag == OUTPARAGRAPH)
                {
                    outFlag = OUTOK;
                }

                doCR();         /* start of a new "paragraph" */
            }
            else
            {
                oChar(' ');     /* soft newline replaced by space */
            }
        }
        oChar(*st);
        prevChar = *st;
    }
}


/*
** outword() -- writes one word to modem & console but,
**              unlike softWord(), it ALWAYS does a doCR() when it
**              encounters a newline character.  This routine was
**              added to help in outputting messages that had been
**              formatted by BIX, Usenet, WWIV, Fido, etc.
*/
void outword (char *string)
{
    char *ptr;
    int  column, overflag = NO;

    if (outFlag == OUTNEXT)     /* oops */
    {
        return;
    }

    /*** doCR() if this word goes past end of line ***/

    ptr = string;

    for (column = crtColumn; *ptr; ptr++)
    {
        if (*ptr == '\n' || *ptr == '\r')
        {
            column = 1;
        }
        else if (*ptr == TAB)
        {
            ++column;

            while (column % 8)
            {
                ++column;
            }
        }
        else
        {
            ++column;
        }

        /*** if line overflows, doCR() and we're outta here ***/

        if (column >= termWidth)
        {
            doCR();
            overflag = YES;
            break;
        }
    }

    ptr = string;

    if (overflag && *ptr == ' ')
        ptr++;

    while (*ptr)
    {
        /*** user called for next paragraph? ***/

        if (outFlag == OUTPARAGRAPH)
        {
            outFlag = OUTOK;
            doCR();
        }

        /*** output spaces for tabs ***/

        if (*ptr == TAB)
        {
            oChar (' ');

            while (crtColumn % 8)
            {
                oChar (' ');
            }
        }

        /*** handle CRs and LFs ***/

        else if (*ptr == '\n' || *ptr == '\r')
        {
            doCR();

            /*** don't doCR() twice on CRLF or LFCR ***/

            if (   (*ptr == '\n' && ptr[1] == '\r')
                || (*ptr == '\r' && ptr[1] == '\n'))
            {
                ++ptr;
            }
            ++ptr;
            continue;
        }

        /*** normal viewable ASCII or backspace char ***/

        else if ((*ptr >= ' ' && *ptr < DEL)
              || (*ptr == '\b'))
        {
            oChar(*ptr);
        }

        /*** worry about words longer than a line: ***/

        if (crtColumn >= termWidth)
            doCR();

        ptr++;
    }
}


/*
** printmessage() -- prints a message
*/
void printmessage (int loc, long id)
{
    if (doFindMsg(loc, id))
    {
        doPrintMsg();
    }
}


/*
** doFindMsg() -- find a message, report any error
*/
int doFindMsg (int loc, long id)
{
    if (findMessage(loc, id))
        return YES;
    mPrintf("Can't find message %lu in sector %u!\n ", id, loc);
    return NO;
}


/*
** doPrintMsg() -- prints out header and message
*/
void doPrintMsg (void)
{
    if (singleMsg)
    {
        singleHdr();
    }
    else
    {
        prtHdr();
    }

    prtBody();
}


/*
** prtHdr -- print a Citadel-ish message header
*/
static void prtHdr (void)
{
    mPrintf("\n  ");

    if (msgBuf.mbdate[0])
        mPrintf(" %s", msgBuf.mbdate);

    if (msgBuf.mbtime[0] && sendTime)
        mPrintf(" %s", msgBuf.mbtime);

    if (msgBuf.mbauth[0])
        mPrintf(" from %s", msgBuf.mbauth);

    if (msgBuf.mboname[0])
        mPrintf(" @ %s", msgBuf.mboname);

    if (msgBuf.mbto[0])
        mPrintf(" to %s", msgBuf.mbto);

    if (msgBuf.mbsubject[0])
        mPrintf("\n  \042%s\042", msgBuf.mbsubject);
}


/*
** singleHdr -- multiline header for More messages
*/
static void singleHdr (void)
{
    int orglen;

    if (usingWCprotocol == ASCII)
    {
        xprintf("\033[2J");          /* local ANSI clearscreen */
    }

    if (!onConsole) 
    {
        if (usingWCprotocol == ASCII)
        {
            oChar(FORMFEED);             /* form-feed for cls */
        }

        if (loggedIn)               /* tell SysOp who's online */
        {
            xprintf(" | user: %s", logBuf.lbname);
        }
    }

    doCR();

    /* print Title or the roomname */

    if (msgBuf.mbsubject[0])
    {
        mPrintf(" | \042%s\042", msgBuf.mbsubject);
    }
    else
    {
        mPrintf(" | %s ", formRoom(thisRoom, NO));      /* room reminder */

        if (stricmp(msgBuf.mbroom, roomBuf.rbname))
        {
            mPrintf(" << %s",msgBuf.mbroom);
        }
    }

    if (msgBuf.mbto[0])
    {
        doCR();
        mPrintf(" | to %s ", msgBuf.mbto);
    }

    if (msgBuf.mbdate[0]) 
    {
        mPrintf("\n | %s", msgBuf.mbdate);

        if (msgBuf.mbtime[0] && sendTime)
        {
            mPrintf(" %s", msgBuf.mbtime);
        }
    }

    if (msgBuf.mboname[0])              /* if it came in on the net */
    {
        mPrintf("\n | from %s", msgBuf.mbauth);

        if (crtColumn + strlen(msgBuf.mboname) + 4 > termWidth)
        {
            mPrintf("\n |    @ %s", msgBuf.mboname);
        }
        else
        {
            mPrintf(" @ %s", msgBuf.mboname);
        }

        if (msgBuf.mborg[0])
        {
            orglen = strlen(msgBuf.mborg) + 6;

            if (orglen < termWidth)     /* must fit on one line */
            {
                /* decide whether we need a new line for this */

                if (termWidth > (orglen + crtColumn))
                {
                    mPrintf(" (%s)", msgBuf.mborg);
                }
                else    /* new line required */
                {
                    mPrintf("\n | (%s)", msgBuf.mborg);
                }
            }
        }
    }
    else if (msgBuf.mbauth[0])    /* life is simpler without the network */
    {
        mPrintf(" from %s", msgBuf.mbauth);
    }

}

/*
** prtBody -- print the body of a message
*/
static void prtBody (void)
{
    register incoming;
    int i;

    doCR();

    if ((outFlag == OUTNEXT) || mAbort())
    {
        return;
    }

    if (singleMsg)
    {
        i = 0;

        while (msgBuf.mbtext[i] == ' ')
            i++;

        if (msgBuf.mbtext[i] != '\n')
        {               /* if author didn't do it, add a */
            doCR();     /* pretty CR for singleMsg start */
        }
    }

    do
    {
        incoming = msggetword(msgBuf.mbtext, 150);

        outword (msgBuf.mbtext);

        if (incoming && mAbort())
        {
            if (outFlag == OUTNEXT)     /* If <N>ext, extra line */
            {
                doCR();
            }
            break;
        }

    } while (incoming)
        ;

    doCR();
}


/*
 ********************************************************
 *      getstrspool() - gets a string from netfile      *
 ********************************************************
 */
void getstrspool (char *place, int length)
{
    register i, c;

    /*
     * predecrement length so that there is a place to put nulls
     */
    for (i=0,length--; (c=getc(spool)) != EOF && c && i < length; )
        place[i++] = (c == '\r') ? '\n' : c ;

    place[i] = 0;
}


/*
 ************************************************
 *      getspool() - gets a message from a file *
 ************************************************
 */
int getspool (void)
{
    register c;

    zero_struct(msgBuf);
    while ((c=getc(spool)) != EOF)
    {
        switch (c) 
        {
        case 'A': getstrspool(msgBuf.mbauth, ADDRSIZE);
            break;
        case 'D': getstrspool(msgBuf.mbdate, NAMESIZE);
            break;
        case 'I': getstrspool(msgBuf.mborg,  ORGSIZE);
            break;
        case 'N': getstrspool(msgBuf.mboname,NAMESIZE);
            break;
        case 'O': getstrspool(msgBuf.mborig, NAMESIZE);
            break;
        case 'R': getstrspool(msgBuf.mbroom, NAMESIZE);
            break;
        case 'T': getstrspool(msgBuf.mbto,   ADDRSIZE);
            break;
        case 'C': getstrspool(msgBuf.mbtime, NAMESIZE);
            break;
        case 'Z': getstrspool(msgBuf.mbroute,NAMESIZE+2);
            break;
        case 'J': getstrspool(msgBuf.mbsubject, ORGSIZE);
            break;
        case 'M': getstrspool(msgBuf.mbtext, MAXTEXT);
            return 1;
        case ' ': /* skip spaces? */
            break;
        default:
            /*
             * all unknown fields are discarded
             */
            while ((c = getc(spool)) != EOF && c)
                ;
            break;
        }
    }
    return 0;
}

