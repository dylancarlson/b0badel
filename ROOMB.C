/*{ $Workfile:   roomb.c  $
 *  $Revision:   1.16  $
 *
 *  room code for b0badel bulletin board system
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

#include <string.h>
#include <stdlib.h>
#include "ctdl.h"
#include "externs.h"
#include "ctdlnet.h"
#include "protocol.h"

#undef toupper
#undef tolower

/*  GLOBAL functions:
**
**  getnoecho()         get a character, don't echo
**  getYesNo()          prompts for a yes/no response
**  getYes()            asks for a yes/no, defaults to NO
**  getNo()             asks for a yes/no, defaults to YES
**  asknumber()         gets a number between (bottom, top)
**  getNumber()         prompt user for a number, limited range
**  getString()         read a string in from user
**  getText()           reads a message in from user
**  indexRooms()        build RAM index to ctdlroom.sys
**  matchString()       search for given string
**  getNormStr()        get a string and normalise it
**  getList()           get a list of strings from the user
*/

static int  coreGetYesNo    (char *prompt, char d_fault);
static int  editText        (char *buf, int lim);
static void printDraft      (void);
static int  putBufChar      (int c);
static char *findString     (char *buf, char *pattern);
static void editChange      (char *buf, char *old, char *new);
static void replaceString   (char *buf, int size);
static void insertParagraph (char *buf, int size);

static int masterCount;     /* counting chars going into msgBuf */

/*
** getnoecho() - get a character, don't echo
*/
char getnoecho()
{
    return justLostCarrier ? 0 : cfg.filter[0x7f & modIn()];
}


/*
** coreGetYesNo() - returns TRUE on 'Y'
*/
static int coreGetYesNo(char *prompt, char d_fault)
{
    char c = '\0';

    while (onLine() && c != 'Y' && c != 'N')
    {
        outFlag = IMPERVIOUS;
        mPrintf("%s (%s/%s)? ", prompt,
            (d_fault == 'Y') ? "Y" : "y",
            (d_fault == 'N') ? "N" : "n");

        c = toupper(getnoecho());

        if (c == '\n' && d_fault)
        {
            c = d_fault;
        }
        mPrintf("%c\n ", (c >= ' ') ? c : '?');
    } 

    outFlag = OUTOK;
    return (c == 'Y');
}


/*
** getYesNo() prompts with (y/n), returns TRUE on 'Y'
*/
int getYesNo (char *prompt)
{
    return coreGetYesNo(prompt, '\0');
}


/*
** getYes() prompts with (Y/n),
**          returns TRUE on 'Y' or '\n', FALSE on 'N'
*/
int getYes (char *prompt)
{
    return coreGetYesNo(prompt, 'Y');
}


/*
** getNo()  prompts with (y/N),
**          returns TRUE on 'Y', FALSE on 'N' or '\n'
*/
int getNo (char *prompt)
{
    return coreGetYesNo(prompt, 'N');
}


/*
** editText() - the editor menu
**  returns ERROR if user decides to Continue
**           YES if user decides to Save
**           NO if user decides to Abandon
**           NO if msg becomes Held
*/
static int editText (char *buf, int lim)
{
    while (onLine()) 
    {
        outFlag = IMPERVIOUS;
        mPrintf("\n editor cmd: ");
        switch (toupper(iChar())) 
        {
        case 'A':
            mPrintf("bandon- ");
            if (getNo(confirm))
                return NO;
            break;
        case 'C':
            mPrintf("ontinue");
            doCR();
            return ERROR;
        case 'L':
            mPrintf("ist (formatted)\n ");
            printDraft();
            break;
        case 'P':
            mPrintf("rint formatted\n ");
            printDraft();
            break;
        case 'V':
            mPrintf("iew formatted\n ");
            printDraft();
            break;
        case 'I':
            mPrintf("nsert paragraph break\n ");
            insertParagraph(buf, lim);
            break;
        case 'R':
            mPrintf("eplace string\n ");
            replaceString(buf, lim);
            break;
        case 'S':
            mPrintf("ave message");
            return YES;
        case 'T':
            getNormStr("\bTitle", msgBuf.mbsubject, ORGSIZE, YES);
            break;
        case 'H':
            mPrintf("old message");
            if (heldMessage) 
            {
                mPrintf("- One is already held\n ");
                break;
            }
            else
            {
                copy_struct(msgBuf, tempMess);
                heldMessage = YES;
                return NO;
            }
        case 'N':
            mPrintf("etwork save");
            if (makenetmessage())
                return YES;
            break;
        case '?':
            tutorial("edit.mnu");
            break;
        default:
            whazzit();
        }
    }

    /* OOPS!  User dropped carrier.
    ** Save message in case he calls back right away
    */
    copy_struct(msgBuf, tempMess);
    heldMessage = YES;
    return NO;
}


/*
** asknumber() gets a number between (bottom, top)
*/
long asknumber (char *prompt, long bottom, long top, int d_fault)
{
    long try;
    char numstring[NAMESIZE];

    while (onLine()) 
    {
        if (*prompt)
        {
            if (d_fault)
            {
                mPrintf("%s (%d): ", prompt, d_fault);
            }
            else
            {
                mPrintf("%s: ", prompt);
            }
        }
        getString("", numstring, NAMESIZE, 0, YES);

        try = strlen(numstring) ? atol(numstring) : d_fault;

        if (try < bottom)
        {
            mPrintf("Must be at least %ld\n ", bottom);
        }
        else if (try > top)
        {
            mPrintf("Must be no more than %ld\n ", top);
        }
        else
        {
            break;      /* got a good number */
        }
    } 
    return try;
}


/*
** getNumber() get a number w/o a default
*/
long getNumber (char *prompt, long bottom, long top)
{
    return asknumber(prompt, bottom, top, (int)bottom - 1);
}


/*
** getString() - get a '\n' terminated string from the user
*/
void getString (char *prompt, char *buf, int lim, int exChar, int visible)
{
    char c;
    int  i = 0;

    outFlag = IMPERVIOUS;

    if (strlen(prompt))
    {
        mPrintf("%s: ", prompt);
    }

    while ((c = getnoecho()) != '\n'
        && i < lim
        && onLine()) 
    {
        outFlag = OUTOK;

        if (c == BACKSPACE) 
        {
            if (i) 
            {
                oChar(BACKSPACE);
                oChar(' ');
                oChar(BACKSPACE);
                --i;
            }
            else
            {
                oChar(BELL);
            }
        }
        else if (c)         /* don't allow nulls in the string */
        {
            buf[i++] = c;

            if (c == exChar)
            {
                break;      /* optional exit character; bye! */
            }

            oChar(visible ? c : 'X');

            if (i >= lim) 
            {
                oChar(BELL);
                oChar(BACKSPACE); 
                --i;
            }
        }
    }
    mCR();
    buf[i] = '\0';
}


/*
** printDraft() - print a message in progress
*/
static void printDraft (void)
{

    outFlag = OUTOK;

    if (msgBuf.mbsubject[0])
    {
        mPrintf("\n | \"%s\"", msgBuf.mbsubject);
    }

    if (msgBuf.mbto[0])
    {
        mPrintf("\n | to %s ", msgBuf.mbto);
    }

    if (roomTab[thisRoom].rtflags.ANON)
    {
        mPrintf("\n ****");
    }
    else
    {
        mPrintf("\n | ");
        mPrintf(formDate());

        if (msgBuf.mboname[0] && loggedIn)       /* if network */
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
        }
        else if (loggedIn)
        {
            mPrintf(" from %s", msgBuf.mbauth);
        }
    }
    doCR();
    doCR();

    if (msgBuf.mbtext[0]) 
    {
        outFlag = OUTOK;
        hFormat(msgBuf.mbtext);
        outFlag = OUTOK;
        doCR();
    }
}


/*
** putBufChar() - write a character into msgBuf
*/
static int putBufChar (int c)
{
    char result;

    if (masterCount > MAXTEXT - 2)
        return 0;

    if (result = cfg.filter[c&0x7f]) 
    {
        msgBuf.mbtext[masterCount++] = result;
        msgBuf.mbtext[masterCount] = 0; /* EOL just for luck    */
    }
    return YES;
}



/*
** getText() - gets a message from the user
*/
int getText (int WCmode)
{
    int  idx, rc;
    int  span, wrap, i;         /* word wrap variables */
    char lc, c, beeped = NO;

    lc = '\n';

    if (WCmode) 
    {
        mPrintf(ulWaitStr, protocol[WCmode]);
        masterCount = 0;
        if (!enterfile(putBufChar, WCmode))
        {
            return NO;
        }
    }
    else
    {
        if (!expert) 
        {
            tutorial("entry.blb");
            mPrintf("Enter message (end with empty line)");
        }
        printDraft();
    }

    while (onLine()) 
    {
        outFlag = OUTOK;
        idx = strlen(msgBuf.mbtext);

        if (!WCmode) 
        {
            span = 0;           /* counting chars on this line */

            while (idx < MAXTEXT-1 && onLine()) 
            {
                c = iChar();

                if (!onLine())          /* oops! */
                {
                    return NO;
                }

                if (lc == '\n')         /* two \n's exits the msg */
                {
                    if ( c == '\n'
                      || c == '.'       /* accomodate common exit chars */
                      || c == '/')      /*  from competing BBS programs */
                    {
                        if (idx)        /* remove the \n from the msg */
                        {
                            msgBuf.mbtext[--idx] = 0;
                        }
                        break;
                    }
                }

                if (c == BACKSPACE && span > 0) 
                {
                    if (idx > 0 && msgBuf.mbtext[idx-1] != '\n') 
                    {
                        oChar(' ');
                        oChar(BACKSPACE);
                        --idx;
                        --span;
                    }
                    else
                    {
                        oChar(BELL);
                    }
                }
                else if (c == TAB)  /* fill tab with spaces */
                {
                    int spaces;

                    if (span > (termWidth - 10)
                      || idx > (MAXTEXT - 10) )
                    {
                        oChar(BELL);    /* don't allow tabs near eol or eof */
                        continue;
                    }

                    spaces = 8 - (span % 8);

                    while (spaces--)
                    {
                        msgBuf.mbtext[idx++] = ' ';
                        oChar(' ');
                        ++span;
                    }
                }
                else if (c && c != ESC)         /* don't allow nul or ESC */
                {       
                    msgBuf.mbtext[idx++] = c;

                    if (idx > (MAXTEXT - 80))
                    {
                        if (!beeped)
                        {
                            oChar(BELL);
                            beeped = YES;
                        }
                    }
                    if (c == '\n') 
                    {
                        msgBuf.mbtext[idx++] = ' ';    /* make a hard CR */
                        oChar(' ');
                        span = 1;
                    }
                    else
                    {
                        ++span;
                    }

                    if (span == (termWidth - 1))       /* time to wrap? */
                    {   
                        int work = idx - 1;

                        while (msgBuf.mbtext[work] > ' ')
                        {
                            --work;     /* counting chars to wrap one word */
                        }
                        wrap = (idx - 1) - work;

                        if ((wrap != 0) && (wrap < termWidth - 1)) 
                        {
                            for (i = 0; i < wrap; ++i)
                            {
                                oChar(BACKSPACE);       /* back over word */
                            }
                            for (i = 0; i < wrap; ++i)
                            {
                                oChar(' ');         /* replace w/ spaces */
                            }

                            doCR();                     /* newline to user  */
                            ++work;                     /* advance over ' ' */

                            while (work < idx)
                            {
                                oChar(msgBuf.mbtext[work++]);   /* new word */
                            }
                            span = wrap;
                        }
                        else
                        {
                            doCR();                 /* no need to wrap */
                            span = 0;
                        }
                    }
                }
                else
                {
                    oChar(BELL);
                }
                lc = c;
            }
            if (idx >= (MAXTEXT - 1))
            {
                mPrintf(" \007buffer overflow!\n ");
            }
        }
        msgBuf.mbtext[idx] = '\0';
        WCmode = ASCII;

        rc = editText(msgBuf.mbtext, MAXTEXT - 1);
        if (rc != ERROR)
        {
            break;
        }
    }

    if (rc && onLine())                 /* Filter null messages */
    {
        while (idx > 0)
        {
            if (isprint(msgBuf.mbtext[--idx]))
            {
                return YES;
            }
        }
    }
    return NO;
}


/*
** indexRooms() - rebuild roomTab[] and delete empty rooms.
*/
void indexRooms(void)
{
    int goodRoom, slot;

    for (slot = 0;  slot < MAXROOMS;  ++slot) 
    {
        if (roomTab[slot].rtflags.INUSE) 
        {
            goodRoom = (roomTab[slot].rtlastMessage > cfg.oldest
                || roomTab[slot].rtflags.PERMROOM);

            if (goodRoom) 
            {
                /* clean out unused floor references */
                if (findFloor(roomTab[slot].rtflags.floorGen) == ERROR) 
                {
                    getRoom(slot);
                    roomBuf.rbflags.floorGen = LOBBYFLOOR;
                    noteRoom();
                    putRoom(slot);
                }
            }
            else /* !goodRoom */
            {   
                getRoom(slot);
                roomBuf.rbflags.ISDIR = roomBuf.rbflags.PERMROOM =
                    roomBuf.rbflags.INUSE = NO;
                noteRoom();
                putRoom(slot);
                strcat(msgBuf.mbtext, roomBuf.rbname);
                strcat(msgBuf.mbtext, "> ");
            }
        }
    }
}



/*
** matchString() - finds a substring, searching backwards from bufEnd
**      Note that the compare is case insensitive.
**      Returns location of the substring, or NULL.
*/
char *matchString (char *buf, char *pattern, char *bufEnd)
{
    char *location = bufEnd;
    char matched = NO;

    while (matched == NO  &&  --location >= buf)
    {
        char *pattptr = pattern;
        char *matchptr = location;

        matched = YES;      /* optimism abounds */

        while (*pattptr)
        {
            if (tolower(*pattptr++) != tolower(*matchptr++))
            {
                matched = NO;
                break;
            }
        }
    }

    return matched ? location : NULL;
}


/*
** getNormStr() - gets a string and deletes leading & trailing blanks etc.
*/
void getNormStr(char *prompt, char *s, int size, int doEcho)
{
    getString(prompt, s, size, 0, doEcho);
    normalise(s);
}


/*
** findString() in a message
**
*/
static char *findString (char *buf, char *pattern)
{
    char *location = matchString(buf, pattern, EOS(buf));

    if (location == NULL)
    {    
        mPrintf("No match.\n ");
    }
    return location;
}


/*
** editChange() - new strings for old, new strings for old
*/
static void editChange (char *buf, char *old, char *new)
{
    char *pc1, *pc2;

    /* delete old string: */

    pc1 = buf;
    pc2 = buf + strlen(old);

    while (*pc1)
    {
        *pc1++ = *pc2++;
    }

    /* make room for new string: */

    pc2 = pc1 + strlen(new);

    while (pc1 >= buf)
    {
        *pc2-- = *pc1--;
    }

    /* insert new string: */

    while (*new)
    {
        *buf++ = *new++;
    }
}


/*
** replaceString() - used by editor R)eplace command
*/
static void replaceString (char *buf, int size)
{
    char old[2*SECTSIZE];
    char new[2*SECTSIZE];
    char *location;

    getString("Search string", old, 2*SECTSIZE, 0, YES);

    location = findString(buf, old);
    if (location != NULL) 
    {
        getString("Replacement", new, 2*SECTSIZE, 0, YES);

        if (strlen(buf) + strlen(new) - strlen(old) >= size)
        {
            mPrintf("*\007Buffer overflow*\n ");
        }
        else
        {
            editChange(location, old, new);
        }
    }
}


/*
** insertParagraph() - editor I)nsert command
*/
static void insertParagraph (char *buf, int size)
{
    char old[2*SECTSIZE];
    char new[2*SECTSIZE];
    char *location;

    if (strlen(buf) + 6 >= size)
    {
        mPrintf("No room!\n ");
    }
    else
    {
        getString("At string", old, (2*SECTSIZE)-2, 0, YES);

        location = findString(buf, old);

        if (location)
        {
            sprintf(new, "\n %s", old);
            editChange(location, old, new);
        }
    }
}


/*
** getList() - get and process a bunch of names
*/
void getList(int (*fn)(label), char *prompt)
{
    label buffer;

    mPrintf("%s (Empty line to end)\n ", prompt);
    do 
    {
        mPrintf(": ");
        getNormStr("", buffer, NAMESIZE, YES);
        if (strlen(buffer) && !(*fn)(buffer))
            break;
    } 
    while (strlen(buffer))
        ;
}


