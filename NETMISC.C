/*{ $Workfile:   netmisc.c  $
 *  $Revision:   1.13  $
 *
 *  Miscellaneous networking functions
}*/

#include <stdlib.h>
#include "ctdl.h"
#include "externs.h"
#include "ctdlnet.h"
#include "event.h"
#include "dirlist.h"

/*
**  normID()            Normalizes a node id.
**  srchNet()           Searches net for the given Id.
**  * creditsetting()   give a user net credits
**  listnodes()         Write up nodes on the net.
**  * getSendFiles()    Get files to send to another system
**  * fileRequest()     For network requests of files.
**  * addNetNode()      Add a node to the net listing
**  * showNode()        Show the values for a net node
**  * roomsIshare()     list all the rooms a node shares
**  * editNode()        Edit a net node
**  netStuff()          Handles networking for the sysop
**  mesgpending()       does a given room have messages?
**  netmesg()           Does this system need to share something*
**  netPrintMsg()       Put a message out in network form
**  * subtract()        accumulate filesizes.
**  sysRoomLeft()       how much room left in net recept area
**  netchdir()          cd() or error out.
*/

extern struct netTable  *netTab;

static char *bauds[] = 
{
    "300", "1200", "2400", "9600", "19200" 
};


#define netPending(i)   (netTab[i].ntflags.in_use \
                        && (netTab[i].ntflags.mailpending \
                        || netTab[i].ntflags.filepending || netmesg(i) ))

/*
** normID() - Normalizes a node id
*/
int normID (register char *source, register char *dest)
{
    while (!isalpha(*source) && *source)
        source++;
    if (!*source)
        return FALSE;
    *dest++ = toupper(*source++);
    while (!isalpha(*source) && *source)
        source++;
    if (!*source)
        return FALSE;
    *dest++ = toupper(*source++);
    while (*source) 
    {
        if (isdigit(*source))
            *dest++ = *source;
        source++;
    }
    *dest = 0;
    return TRUE;
}

/*
** srchNet() - Searches net for the given Id.
*/
int srchNet (char *forId)
{
    int   i;
    label temp;

    for (i = 0; i < cfg.netSize; ++i) 
    {
        if (netTab[i].ntflags.in_use && hash(forId) == netTab[i].ntidhash) 
        {
            getNet(i);
            normID(netBuf.netId, temp);
            if (!stricmp(temp, forId))
            {
                return i;
            }
        }
    }
    return ERROR;
}

/*
** shownets() - show the nets that a given node is in
*/
static void shownets (long mask)
{
    char c;
    int  i;
    long bit = 1L;

    for (c = ' ', i = 0; i < 32; ++i, bit <<= 1)
    {
        if (mask & bit) 
        {
            mPrintf("%c%d", c, i);
            c = ',';
        }
    }
}

/*
** listnodes() - write up nodes on the net
*/
void listnodes (int extended)
{
    int i;

    outFlag = OUTOK;
    for (i = 0; i < cfg.netSize; i++) 
    {
        getNet(i);
        if (netBuf.nbflags.in_use)
        {
            if (extended) 
            {
                mPrintf("%c %-20.20s %-20.20s%c %5s",
                    netPending(i) ? '*' : ' ',
                    netBuf.netName, netBuf.netId,
                    netBuf.nbflags.rec_only ? 'R' : ' ',
                    bauds[netBuf.baudCode]);
                shownets(netBuf.nbflags.what_net);
                mCR();
            }
            else if (netBuf.nbflags.what_net)
            {
                mPrintf("%s\n ", netBuf.netName);
            }
        }
    }
}

/*
** getSendFiles() - Send files to another system
*/
static void getSendFiles (void)
{
    label   sysName;
    pathbuf sysFile;
    int     sendidx;

    if ((sendidx = getSysName("To", sysName)) == ERROR)
        return;

    getNormStr("File(s)", sysFile, 100, YES);
    if (strlen(sysFile) < 1)
        return;

    if (getdirentry(sysFile))
        nfs_put(sendidx, SEND_FILE, sysFile);
    else
        mPrintf("File not found\n ");
}

/*
** fileRequest() - network request files.
*/
static void fileRequest()
{
    label   reqroom;
    label   reqfile;
    pathbuf reqdir;
    label   system;
    int     reqidx;

    if ((reqidx = getSysName("From", system)) == ERROR)
        return;
    getNormStr("What room", reqroom, NAMESIZE, YES);
    if (strlen(reqroom) < 1)
        return;
    getNormStr("Filename", reqfile, NAMESIZE, YES);
    if (strlen(reqfile) < 1)
        return;
    getNormStr("Destination directory", reqdir, PATHSIZE, YES);
    if (strlen(reqdir) < 1)
        return;
    if (xchdir(reqdir))
        nfs_put(reqidx, FILE_REQUEST, reqfile, reqdir, reqroom);
}

/*
** addNetNode() Add a node to the net
*/
static void addNetNode()
{
    int     newidx, i;
    label   temp;
    void    *save;

    zero_struct(netBuf);                /* Useful initialization       */

    while (1) 
    {
        getNormStr("System name", netBuf.netName, NAMESIZE, YES);
        if (strlen(netBuf.netName) == 0)
            return;
        if (NNisok(netBuf.netName))
            break;
        mPrintf("Illegal character in name\n ");
    }
    getNormStr("System ID (phone number)", netBuf.netId, NAMESIZE, YES);
    if (strlen(netBuf.netId) == 0)
        return;
    netBuf.baudCode = getNumber("Baudrate (0-3) ", 0L, (long)(NUMBAUDS-1));
    netBuf.nbflags.ld      = !getYesNo("Is system local");
    netBuf.nbflags.ld_rr   = TRUE;
    netBuf.nbflags.rec_only= FALSE;
    netBuf.nbflags.in_use  = TRUE;
    netBuf.nbflags.what_net= 1L;
    netBuf.nbflags.dialer  = 0;
    netBuf.nbflags.poll_day= 0;

#if 0
    netBuf.nbflags.wwiv_id = (getYesNo("Is it a WWIV system"))
        ? getNumber("Enter WWIV node number (0 if none)", 0L, 0x1fffL)
        : 0;
#endif

    /*** find a slot for new table entry ***/

    for (newidx = 0; newidx < cfg.netSize; newidx++)
    {
        if (!netTab[newidx].ntflags.in_use)
            break;
    }

    /*** if necessary, expand the log ***/

    if (newidx >= cfg.netSize) 
    {
        newidx = cfg.netSize;
        cfg.netSize++;
        save = netTab;      /* safety net */

        netTab = realloc(netTab, sizeof(struct netBuffer) * cfg.netSize);

        if (netTab == NULL)     /* oops! */
        {
            printf ("\nHeap big realloc() error ... node not added\n");
            netTab = save;
            cfg.netSize--;
            zero_struct(netBuf);
            return;
        }
    }

    for (i = 0; i < SHARED_ROOMS; i++)
        netBuf.shared[i].NRidx = -1;

    normID(netBuf.netId, temp);
    netTab[newidx].ntnmhash = hash(netBuf.netName);
    netTab[newidx].ntidhash = hash(temp);

    putNet(newidx);
}

/*
** showNode()
*/
static void showNode()
{
    int i;
    char c;

    mPrintf("\n ID: %s\n ", netBuf.netId);
    if (strlen(netBuf.access))
        mPrintf("Access: %s\n ", netBuf.access);

    mPrintf("A ");

    if (netBuf.nbflags.rec_only)
        mPrintf("receive-only ");

    if (netBuf.nbflags.ld) 
    {
        if (!netBuf.nbflags.ld_rr)
            mPrintf("non-role-reversing ");
        mPrintf("l-d ");
        if (netBuf.nbflags.ld > 1)
            mPrintf("(poll count = %d) ", netBuf.nbflags.ld);
    }
    else
        mPrintf("local ");

    mPrintf("system running at %s baud.\n ", bauds[netBuf.baudCode]);

    if (netBuf.nbflags.mailpending)
        mPrintf("[mail pending]\n ");
    if (netBuf.nbflags.filepending)
        mPrintf("[files pending]\n ");
    if (netBuf.nbflags.dialer)
        mPrintf("Dialer #%d\n ", 0xff & netBuf.nbflags.dialer);
    if (netBuf.nbflags.wwiv_id)
        mPrintf("WWIV node #%d\n ", netBuf.nbflags.wwiv_id); 

    mPrintf("this node is ");
    if (netBuf.nbflags.what_net) 
    {
        mPrintf("in net:");
        shownets(netBuf.nbflags.what_net);
        if (netBuf.nbflags.poll_day) 
        {
            mPrintf("\n polls on:");
            showdays(netBuf.nbflags.poll_day);
        }
    }
    else
        mPrintf("not assigned to a net");
    mCR();
}

/*
** roomsIshare() - list all the rooms a node shares
*/
static void roomsIshare (int place)
{
    int i, first = 1;
    int rmidx;
    char *rmalias, *rmname;

    for (i=0; i<SHARED_ROOMS; i++) 
    {
        rmidx = netBuf.shared[i].NRidx;
        if (rmidx >= 0 && netBuf.shared[i].NRgen == roomTab[rmidx].rtgen) 
        {
            rmname = roomTab[rmidx].rtname;
            rmalias= chk_alias(net_alias, place, rmname);
            mPrintf( (rmname!=rmalias) ? "%s %s(%s)" : "%s %s",
                (first ? "" : ","), rmname, rmalias);
            first = 0;
        }
    }
    doCR();
}

/*
** killNode() - Kill the node in the current netBuf struct.
*/
static int killNode (int place)
{
    if (netBuf.nbflags.mailpending)
    {
        mPrintf("There is outgoing mail.\n ");
    }
    if (getNo(confirm)) 
    {
        pathbuf temp;

        /* clear all node data */

        zero_struct(netBuf);
        putNet(place);

        /* delete all related files */

        ctdlfile(temp, cfg.netdir, "%d.ml", thisNet);
        unlink(temp);
        ctdlfile(temp, cfg.netdir, "%d.fwd", thisNet);
        unlink(temp);
        ctdlfile(temp, cfg.netdir, "%d.nfs", thisNet);
        unlink(temp);

        return TRUE;
    }
    return FALSE;
}

/*
** editNode() Edit a net node
*/
static void editNode()
{
    label sysname, temp;
    int i, place, pd;
    char nets[80];
    long mask, l, k, atol();
    char dayp, *p, *strtok();
    extern char *_day[];

    if ((place=getSysName("Node to edit", sysname)) == ERROR)
        return;
    showNode();

    while (onLine()) 
    {
        outFlag = OUTOK;
        mPrintf("\n (%s) edit fn: ", netBuf.netName);
        switch (toupper(getnoecho())) 
        {
        case '?':
            tutorial("netedit.mnu");
            break;
        case 'A':
            getString("Access string", netBuf.access, 40, 0, YES);
            break;
        case 'R':
            mPrintf("%ss shared\n ", Room_Area);
            roomsIshare(place);
            break;
        case 'X':
            mPrintf("Exit to net menu");
            normID(netBuf.netId, temp);
            netTab[place].ntnmhash = hash(netBuf.netName);
            netTab[place].ntidhash = hash(temp);
            putNet(place);
            return;
        case 'B':
            netBuf.baudCode = asknumber("Baudrate", 0l, (long)(NUMBAUDS-1),
                netBuf.baudCode);
            break;
        case 'F':
            getNormStr("Set poll days", nets, 80, YES);
            if (stricmp(nets,"all") == 0)
                netBuf.nbflags.poll_day = 0x7f;
            else
            {
                pd = 0;
                for (p=strtok(nets,"\t ,"); p; p=strtok(NULL,"\t ,")) 
                {
                    for (i=0; i<7; ++i)
                        if (stricmp(p,_day[i]) == 0) 
                        {
                            pd |= (1<<i);
                            break;
                        }
                    if (i>=7) 
                    {
                        mPrintf("bad day %s\n ", p);
                        pd = -1;
                        break;
                    }
                }
                if (pd >= 0)
                    netBuf.nbflags.poll_day = pd;
            }
            break;
        case 'N':
            getNormStr("Name change to", temp, NAMESIZE, YES);
            if (strlen(temp) != 0)
                if (netnmidx(temp) != ERROR)
                    mPrintf("%s already exists!\n ",temp);
                else if (NNisok(temp))
                    strcpy(netBuf.netName, temp);
                else
                    mPrintf("Bad nodename\n ");
            break;
        case 'I':
            getString("New node ID", temp, NAMESIZE, 0, YES);
            if (strlen(temp) != 0)
                strcpy(netBuf.netId, temp);
            break;
        case 'K':
            mPrintf("Kill node\n ");
            if (killNode(place))
                return;
            break;
        case 'L':
            netBuf.nbflags.ld = !getYesNo("Is system local");
            break;
        case 'C':
            netBuf.nbflags.rec_only = !getYesNo("Ok to call this system");
            break;
        case 'V':
            showNode();
            break;
        case 'P':
            mPrintf("Passwords\n ");
            mPrintf("local = `%s'\n remote= `%s'\n ",
                netBuf.myPasswd, netBuf.herPasswd);
            if (getYesNo("Change passwords")) 
            {
                getNormStr("Local pw", netBuf.myPasswd, NAMESIZE, YES);
                getNormStr("Remote pw",netBuf.herPasswd,NAMESIZE, YES);
            }
            break;
        case 'U':
            getNormStr("Use what nets", nets, 79, YES);
            if (strlen(nets) > 0) 
            {
                p = &nets[strchr("+-",nets[0]) ? 1 : 0];
                for (mask=0L,p=strtok(p,"\t ,"); p; p=strtok(NULL,"\t ,"))
                {
                    k = atol(p);        /* explicit evaluation to avoid */
                    l = 1L << k;        /* compiler weirdnesses         */
                    mask |= l;
                }
                switch (nets[0]) 
                {
                case '+':
                    netBuf.nbflags.what_net |= mask;
                    break;
                case '-':
                    netBuf.nbflags.what_net &= ~mask;
                    break;
                default:
                    netBuf.nbflags.what_net = mask;
                    break;
                }
            }
            break;
        case 'E':
            mFormat("External dialer ");
            mPrintf(netBuf.nbflags.dialer ? "(dial_%d.prg)" : "(none)",
                0xff & netBuf.nbflags.dialer);
            mCR();
            netBuf.nbflags.dialer = asknumber("New dialer (0 to disable)",
                0L, 10L,
                netBuf.nbflags.dialer);
            break;
        case 'W':
            netBuf.nbflags.wwiv_id =
                getNumber("WWIV node number (0 if none)", 0L, 0x1fffL);
            break;
        case 'Z':
            if (netBuf.nbflags.ld) 
            {
                mPrintf("Poll count=%d\n ", netBuf.nbflags.ld);
                netBuf.nbflags.ld = getNumber("New poll count", 1L, 127L);
                break;
            } /* else fall into next case, which has the same test... */
        case 'D':
            if (netBuf.nbflags.ld) 
            {
                netBuf.nbflags.ld_rr = getYes("Allow l-d role reversal");
                break;
            } /* else fall into default case */
        default:
            whazzit();
            break;
        }
    }
}

/*
** netStuff() - Sysop net management
*/
netStuff()
{
    char tmp[10];
    int  topoll;
    int  place;
    label sysname;
    extern char justLostCarrier;

    while (onLine()) 
    {
        outFlag = OUTOK;
        mPrintf("\n Net function: ");
        switch (toupper(getnoecho())) 
        {
        case 'R':
            mPrintf("Request File\n ");
            fileRequest();
            break;
        case 'S':
            mPrintf("Send File\n ");
            getSendFiles();
            break;
        case 'X':
            mPrintf("Exit to main menu");
            return;
        case 'V':
            mPrintf("View netlist\n ");
            listnodes(YES);
            break;
        case 'P':
            getNormStr("Poll what net", tmp, 9, YES);
            if (strlen(tmp) > 0 && (topoll=atoi(tmp)) >= 0 && topoll <= 32) 
            {
                pollnet(topoll);
                haveCarrier = onConsole = FALSE;
                justLostCarrier = TRUE;
            }
            break;
        case 'A':
            mPrintf("Add node- ");
            addNetNode();
            break;
        case 'E':
            mPrintf("Edit- ");
            editNode();
            break;
        case 'K':
            mPrintf("Kill- ");
            if ((place = getSysName("Node to Kill", sysname)) != ERROR)
            {
                killNode(place);
            }
            break;
        case '?':
            tutorial("netopt.mnu");
            break;
        default:
            whazzit();
        }
    }
}

/*
** getSysName() - Get a node, put it in netBuf, return slot # or ERROR
*/
int getSysName(char *prompt, char *system)
{
    int place;

    getString(prompt, system, NAMESIZE, '?', YES);
    if (strcmp(system, "?") == 0) 
    {
        mPrintf("Systems:\n ");
        listnodes(NO);
        return getSysName(prompt, system);
    }
    else
    {
        normalise(system);
        if (strlen(system) < 1)
            return ERROR;
        if ((place = netnmidx(system)) == ERROR) 
        {
            mPrintf("%s not listed!\n ", system);
        }
        else
            return place;
    }
    return ERROR;
}

/*
** mesgpending() - Any messages to be shared with other nodes?
*/
int mesgpending (struct netroom *nr)
{
    register rmidx = nr->NRidx;

    if (rmidx >= 0 && nr->NRgen == roomTab[rmidx].rtgen)
        return (roomTab[rmidx].rtlastNet > nr->NRlast)
        || (nr->NRhub && (roomTab[rmidx].rtlastLocal > nr->NRlast));
    return NO;
}

/*
** netmesg() - Does this system has a room with data to share?
*/
int netmesg (int slot)
{
    register i;

    for (i = 0; i < SHARED_ROOMS; i++)
        if (mesgpending(&netTab[slot].Tshared[i]))
            return YES;
    return NO;
}

/*
** netPrintMsg() - networking dump of a message
*/
void netPrintMsg (int loc, long id)
{
    register c;

    if (!findMessage(loc,id))   /* can't find it...     */
    {
        return;
    }
    /*
     * send message text proper
     */
    sendXmh();
    (*sendPFchar)('M');

    while ((*sendPFchar)(c=getmsgchar()) && c)
        ;
}


char route_char = 'L';  /* routing code -- set by dumpNetRoom */

/*
** sendXmh() - throw message headers across the net
*/
void sendXmh (void)
{
    long val;
    label srcid;

    /*
     * send header fields out
     */
    if (msgBuf.mbauth[0])
        wcprintf("A%s", msgBuf.mbauth);
    if (msgBuf.mbdate[0])
        wcprintf("D%s", msgBuf.mbdate);
    if (msgBuf.mbtime[0])
        wcprintf("C%s", msgBuf.mbtime);
    if (msgBuf.mbroom[0])
        wcprintf("R%s", msgBuf.mbroom);
    if (msgBuf.mbto[0])
        wcprintf("T%s", msgBuf.mbto);
    if (msgBuf.mbsubject[0])
        wcprintf("J%s", msgBuf.mbsubject);

    wcprintf("N%s", msgBuf.mboname[0]
        ? msgBuf.mboname
        : &cfg.codeBuf[cfg.nodeName]);
    wcprintf("O%s", msgBuf.mborig[0]
        ? msgBuf.mborig
        : &cfg.codeBuf[cfg.nodeId]);
    wcprintf("I%s", msgBuf.mborg);

    if (route_char) 
    {
        /*
         * either Z@L<id> or Z@H<id>
         */
        normID(&cfg.codeBuf[cfg.nodeId], srcid);
        wcprintf("Z@%c%s", route_char, srcid);
    }
}

static long remaining;      /* used by subtract() and sysRoomLeft() */

/*
** subtract() - accumulate filesizes
*/
static int subtract (struct dirList *p)
{
    remaining -= p->fd_size;
    return YES;
}

/*
** sysRoomLeft() - how much room left in net recept area
*/
long sysRoomLeft (void)
{
    remaining = 1024L * cfg.recSize;
    netwildcard(subtract, "*.*");
    return remaining;
}

/*
** netchdir() - change directory or die
*/
int netchdir (char *path)
{
    if (cd(path)) 
    {
        splitF(netLog, "No directory <%s>\n", path);
        return NO;
    }
    return YES;
}
