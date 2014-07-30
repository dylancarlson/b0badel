/************************************************************************
 *                              cfgmisc.c                               *
 *                      misc routines that configur uses                *
 *                                                                      *
 *                Copyright 1988 by David L. Parsons                    *
 *                       All Rights Reserved                            *
 *                                                                      *
 *             Permission is granted to use these sources               *
 *          for non-commercial purposes ONLY.  You may not              *
 *          include them in whole or in part in any commercial          *
 *          or shareware software; you may not redistribute             *
 *          these sources  unless you have the express written          *
 *          consent of David L. Parsons.                                *
 *                                                                      *
 * 88Jul16 orc  make readXstring return 0 on "" fields                  *
 * 88Jul03 orc  Created.                                                *
 *                                                                      *
 ************************************************************************/

#include "ctdl.h"
#include "ctdlnet.h"
#include "event.h"
#include "audit.h"
#include "poll.h"
#include "cfg.h"

/************************************************************************
 *                                                                      *
 *      exists()        does a system directory exist?                  *
 *      readXstring()   read a string, adjust pointers, check size.     *
 *      readDstring()   read in a dir name                              *
 *      readString()    reads a '#<id> "<value>"  since scanf can't     *
 *      doformat()      parse a string a'la C.                          *
 *      illegal()       Prints out configur error message and aborts    *
 *      syntax()        print out a syntax error message                *
 *      copystring()    Copy <size-1> characters, null-terminate        *
 *                                                                      *
 ************************************************************************/

/************************************************************************
 *                External variable declarations in CONFG.C             *
 ************************************************************************/

struct msgB msgBuf;
extern struct config cfg;
extern struct evt_type *evtTab;         /* events gonna happen?         */
extern struct netBuffer netBuf;         /* network stuff                */
extern struct flTab *floorTab;          /* floor table.                 */
extern struct rTable roomTab[MAXROOMS]; /* RAM index of rooms           */
extern struct aRoom  roomBuf;           /* room buffer                  */
extern int floorfl;                     /* file descriptor for floors   */
extern int thisRoom;                    /* room currently in roomBuf    */
extern struct logBuffer logBuf;         /* Log buffer of a person       */
extern struct lTable   *logTab;         /* RAM index of pippuls         */
extern int logfl;                       /* log file descriptor          */
extern int netfl;
extern struct netTable *netTab;
extern int  thisNet;
extern struct poll_t *pollTab;

extern int msgfl;               /* file descriptor for the msg file     */
extern int regen;               /* just regenerating tables? */
extern int msgZap, logZap, roomZap, floorZap;
extern int attended;

extern char msgLine[];
extern int lineNo;      /* line in CTDLCNFG.SYS we're looking at right now.. */

extern int offset;
extern char *nextCode;



/*
 ****************************************************************
 *      exists() does a system directory exist?                 *
 ****************************************************************
 */
exists(where)
{
    char here[200];
    char c;

    getcd(here);

    if (cd(&cfg.codeBuf[where]) != 0) 
    {
        if (attended) 
        {
            printf("Directory %s does not exist; create it? ",
                &cfg.codeBuf[where]);
            fflush(stdout);
            putchar(c = toupper(getch()));
            if (c != '\n')
                putchar('\n');
            if (c == 'Y') 
            {
                if (mkdir(&cfg.codeBuf[where]) != 0)
                    illegal("Cannot create directory!");
            }
            else
                exit(3);
        }
        else
        {
            sprintf(here,"Directory '%s' does not exist.",&cfg.codeBuf[where]);
            illegal(here);
        }
    }
    cd(here);
}

/*
 ****************************************************************
 *    readXstring() read a string, adjust pointers, check size. *
 ****************************************************************
 */
readXstring(line, maxsize, flag)
char *line;
int  maxsize, flag;
{
    int retval = offset;
    char msg[80];

    readString(line, nextCode, flag);

    if (strlen(nextCode) >= maxsize) 
    {
        sprintf(msg, "Argument too long; must be less than %d!", maxsize);
        syntax(msg);
    }
    else if (*nextCode == 0)
        return 0;

    while (*nextCode++)
        offset++;
    offset++;

    return retval;
}


/*
 ****************************************
 *    readDstring() read in a dir name  *
 ****************************************
 */
readDstring(from)
char *from;
{
    int retval = offset;
    int size;

    readString(from, nextCode, FALSE);

    if (*nextCode == 0)
        return 0;

    size = strlen(nextCode);
    if (size-- > 0 && (nextCode[size] == '\\' || nextCode[size] == '/'))
        nextCode[size] = 0;

    while (*nextCode++)
        offset++;
    offset++;

    return retval;
}

/*
 ****************************************************************
 *    readString() reads a '#<id> "<value>"  since scanf can't  *
 ****************************************************************
 */
readString(source, destination, doProc)
char *source, *destination;
char doProc;
{
    char string[300], last = 0;
    int  i, j;

    for (i = 0; source[i] != '"'; i++)
        ;

    for (j = 0, i++; source[i] != '"' || last == '\\'; i++, j++) 
    {
        string[j] = source[i];
        if (doProc)
            last = source[i];
    }

    string[j] = 0;
    strcpy(destination, string);
    if (doProc)
        doformat(destination);
}

/*
 ************************************************
 *      doformat() -- parse a string a'la C.    *
 ************************************************
 */
doformat(s)
char *s;
{
    char *p, *q;
    int i;

    for (p=q=s; *q; ++q) 
    {
        if (*q == '\\')
            switch (*(++q)) 
            {
            case 'n' :
                *p++ = '\n';
                break;
            case 't' :
                *p++ = '\t';
                break;
            case 'b' :
                *p++ = '\b';
                break;
            case 'r' :
                *p++ = '\r';
                break;
            case 'f' :
                *p++ = '\f';
                break;
            default :
                if (isdigit(*q)) 
                {
                    for (i=0; isdigit(*q); ++q)
                    {
                        i = (i<<3) + (*q - '0');
                    }
                    *p = i;
                    --q;
                }
                else
                    *p++ = *q;
                break;
            }
        else
            *p++ = *q;
    }
    *p = 0;
}

/*
 ****************************************************************
 *    illegal() Prints out configur error message and aborts    *
 ****************************************************************
 */
illegal(errorstring)
char *errorstring;
{
    printf("\007ERROR: %s\n", errorstring);
    exit(7);
}

/*
 ************************************************
 * syntax() print out a syntax error message    *
 ************************************************
 */
syntax(msg)
char *msg;
{
    printf("\007ERROR (line %d): %s\n>> %s\n", lineNo, msg, msgLine);
    exit(7);
}


/*
 ****************************************************************
 *      copystring() - copy a string of length (size) and make  *
 *                     it null-terminated.                      *
 ****************************************************************
 */
copystring(s1,s2,size)
char *s1, *s2;
{
    strncpy(s1, s2, size);
    s1[size-1] = 0;
}



/*
** copyString() copies a '#<id> "string", trapping length overrun
*/
copyString (char *source, char *destination, int length)
{
    int oops = 0;

    while (*source++ != '"')
        ;

    while (length > 0 && *source)
    {
        if (*source == '"')
            break;

        if (*source == '\\')
        {
            source++;

            switch (*source)
            {
            case 'n' :
                *destination++ = '\n';
                break;

            case 't' :
                *destination++ = '\t';
                break;

            case 'b' :
                *destination++ = '\b';
                break;

            case 'r' :
                *destination++ = '\r';
                break;

            case 'f' :
                *destination++ = '\f';
                break;

            case 0:
                *destination++ = '\\';
                --length;
                oops = 1;

            default:
                *destination++ = *source;
                break;
            }

            source++;

            if (oops)
                break;
        }
        else
        {
            *destination++ = *source++;
        }
    }
    *destination = 0;
}


/*
 ************************************************
 *      setvariable() set a switchable variable *
 ************************************************
 */
int doTime = 0,
    hourOut= -1;
#ifdef OLD_CODE
    int hs_bug = 0;
#endif

extern char graphic;         /* allow ESCapes through? */

int depend[DEPENDSIZE];


#define VarMatch(string)    (!stricmp(var, string))

void setvariable(char *var, int arg)
{
    char *xmalloc();
    long msgsize;

    if VarMatch("NOVICE-CR")     /* b0b */
        cfg.novice_cr = arg;
    else if VarMatch("CRYPTSEED")
        cfg.cryptSeed = arg;
    else if VarMatch("MESSAGEK") 
    {
        msgsize = (1024L * (long)arg) + (long)(BLKSIZE-1);
        cfg.maxMSector = msgsize/BLKSIZE;
        depend[MESSAGEK]++;
    }
    else if VarMatch("LOGINOK")
        cfg.unlogLoginOk= arg;
    else if VarMatch("ENTEROK")
        cfg.unlogEnterOk= arg;
    else if VarMatch("READOK")
        cfg.unlogReadOk = arg;
    else if VarMatch("ROOMOK")
        cfg.nonAideRoomOk=arg;
    else if VarMatch("ALLMAIL")
        cfg.noMail = !arg;
    else if VarMatch("ALLNET")
        cfg.allNet = arg;
    else if VarMatch("PARANOID")    /* obsolete flag */
        ;
    else if VarMatch("AIDE-FORGET")
        cfg.aideforget = arg;
    else if VarMatch("TIMEOUT")
        doTime = arg;
    else if VarMatch("HOUROUT") 
    {
        if (arg > 23)
            syntax("Illegal HOUROUT (must be 0 to 23)");
        else
            hourOut = arg;
    }
    else if VarMatch("MIRRORMSG")
        cfg.mirror = arg;
    else if VarMatch("LOGSIZE") 
    {
        cfg.MAXLOGTAB = arg;
        if (arg > 0)
        {
            if (!regen)
            {
                logTab = (struct lTable *) xmalloc(sizeof(*logTab) * arg);
            }
            depend[LTABSIZE]++;
        }
    }
    else if VarMatch("POLL-DELAY") 
    {
        if (arg < 1)
            syntax("POLL-DELAY must be > 0");
        else
            cfg.poll_delay = 60L * arg;
    }
    else if VarMatch("AUDIT-CALLS")  /* audit calls      */
        set_audit(arg, aLOGIN);
    else if VarMatch("AUDIT-FILES")  /* audit downloads  */
        set_audit(arg, aDNLOAD);
    else if VarMatch("AUDIT-EXIT")   /* system shutdowns */
        set_audit(arg, aEXIT);
    else if VarMatch("USA")
        cfg.usa = arg;
    else if VarMatch("LOCAL-TIME")
        cfg.local_time = arg;
    else if VarMatch("LD-TIME")
        cfg.ld_time = arg;
    else if VarMatch("SEARCHBAUD")
        cfg.search_baud = arg;
    else if VarMatch("RECEIPTK")
        cfg.recSize = arg;
    else if VarMatch("CONNECTDELAY")
        cfg.connectDelay = arg;
    else if VarMatch("CONNECTPROMPT")
        cfg.connectPrompt = arg;
    else if VarMatch("SYSBAUD") 
    {
        cfg.sysBaud = arg;
        depend[SYSBAUD]++;
        if (arg >= NUMBAUDS)
            syntax("Invalid SYSBAUD");
    }
    else if VarMatch("CALL-LOG")
        set_audit(arg, aLOGIN|aEXIT);
    else if VarMatch("WIDTH") 
    {
        if (arg < 10 || arg > 255)
            syntax("WIDTH must be between 10 and 255");
        cfg.syswidth = arg;
    }
    else if VarMatch("DOWNLOAD")
        cfg.download = 1024L * arg;
    else if VarMatch("SHOWUSAGE")
        cfg.diskusage = arg;
    else if VarMatch("INIT-SPEED")
        cfg.probug = arg;

#ifdef OLD_CODE
    else if VarMatch("HS-BUG")
        hs_bug = arg;
#endif

    else if VarMatch("COM")
    {
        if (arg >= 1 && arg <= 4)
            cfg.com_port = arg-1;
        else
            illegal("We only support COM1 through COM4");
    }
    else if VarMatch("ZAPLOOPS")
        cfg.fZap = arg;
    else if VarMatch("NETLOG")
        cfg.fNetlog = arg;
    else if VarMatch("NETDEBUG")
        cfg.fNetdeb = arg;
    else if VarMatch("CHAT")
        cfg.noChat = !arg;
    else if VarMatch("LD-COST")
        cfg.ld_cost = arg;
    else if VarMatch("HUB-COST")
        cfg.hubcost = arg;
    else if VarMatch("FORWARD-MAIL")
        cfg.forward_mail = arg;
    else if VarMatch("PATHALIAS")
        cfg.pathalias = arg;
    else if VarMatch("ESC")
        graphic = arg;
    else
        printf("No variable `%s'.\n", var);
}
