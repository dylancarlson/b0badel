/*{ $Workfile:   door.c  $
 *  $Revision:   1.13  $
 *
 *  Citadel code to access doors
}*/

#include "ctdl.h"
#include "door.h"
#include "externs.h"

/*
**  * pickdoor()    low-level find a shell escape
**  * finddoor()    Find and validate a shell escape
**  rundoor()       Redirect i/o and run a shell escape.
**  * makedoor()    add a shell escape to the list.
**  initdoor()      read a list of shell escapes.
**  dodoor()        menu-level execute shell escape
*/

struct doorway *doors = NULL;   /* doors defined in the system  */
struct doorway *shell = NULL;   /* "outside" doorway...         */

static char *linkdir = NULL;    /* directory linked via mode `l'*/

/* dummies used as flags */

struct doorway foo[1];

static struct doorway *no_access  = &(foo[-1]);
static struct doorway *badroom    = &(foo[-3]);

char *EOS();

/*
** pickdoor()   find a shell escape
*/
static struct doorway *pickdoor(char *cmd, struct doorway *list)
{
    while (list && stricmp(list->dr_name, cmd) != 0)
        list = list->dr_next;
    return list;
}


/*
** legaldoor()  Is this door visible at this time?
*/
static struct doorway *legaldoor (struct doorway *list)
{
    switch (list->dr_mode & DR_UMASK) 
    {
    case DR_SYSOP:
        if (!SomeSysop())
            return no_access;
        break;
    case DR_AIDE:
        if (!aide)
            return no_access;
        break;
    }
    switch (list->dr_mode & DR_IOMASK) 
    {
    case DR_CONSOLE:
        if (!onConsole)
            return no_access;
        break;
    case DR_MODEM:
        if (onConsole)
            return no_access;
        break;
    }
    /*
     * check room accessability...
     */
    if (list->dr_mode & DR_DIR) 
    {
        if (!roomBuf.rbflags.ISDIR)
            return badroom;
        if ((list->dr_mode & DR_READ) && !roomBuf.rbflags.DOWNLOAD)
            return badroom;
        if ((list->dr_mode & DR_WRITE) && !roomBuf.rbflags.UPLOAD)
            return badroom;
    }
    return list;
}


int islegal(struct doorway *list, struct doorway **ptr)
{
    *ptr = legaldoor(list);

    return (*ptr != no_access && *ptr != badroom);
}


/*
** finddoor()   find and validate a shell escape
*/
static struct doorway *finddoor (char *cmd, struct doorway *list)
{
    struct doorway *ptr;

    for (ptr=NULL; list; list=list->dr_next)
        if (stricmp(list->dr_name, cmd) == 0 && islegal(list, &ptr))
            break;
    return ptr;
}


/*
** rundoor()    execute a shell escape
*/
void rundoor(struct doorway *door, char *tail)
{
    char realtail[129];
    char ttyname[20];
    int  tty;
    long status, dosexec();
    int hold[3];
    int held = NO;
    int i;
    int watching = 0;

    char *p, *strtok();
    extern char dropDTR;
    extern char onConsole;

    strcpy(realtail, door->dr_tail);
    /*
     * if a tail can be passed in, check to make sure there are no pathnames
     * associated with it....
     */
    if (tail && (door->dr_mode & DR_TAIL)) 
    {
        if (!(SomeSysop() || onConsole) 
            && (strchr(tail,':') || strchr(tail, '\\'))) 
        {
            mPrintf("Cannot have `:' or `\\' in arguments\n ");
            return;
        }
        sprintf(EOS(realtail), " %s", tail);
    }
    if ((door->dr_mode & DR_NAMED) && loggedIn)
        sprintf(EOS(realtail), " %s", logBuf.lbname);

    if ((door->dr_mode & DR_LINKED) && !xchdir(door->dr_dir))
        return;

    if ((door->dr_mode & DR_LINKED) || !roomBuf.rbflags.ISDIR
        || xchdir(roomBuf.rbdirname)) 
    {
        if (!onConsole && !(door->dr_mode & DR_FOSSIL)) 
        {
            /* program called from remote, is not FOSSIL aware */

            /* make stdin/stdout point at remote*/

            sprintf(ttyname, "COM%d", 1 + cfg.com_port);

            tty = dos_open(ttyname,O_RDWR);

            if (tty < 0) 
            {
                mPrintf("tty open error! (%d)\n ", tty);
                return;
            }
            for (i = 0; i < 3; ++i) 
            {
                hold[i] = dup(i);
                dup2(tty, i);
            }
            held = YES;
        }
        if (!onConsole & (door->dr_mode & DR_WATCHDOG))
        {
            watchon();
            watching = YES;
        }

        status = dosexec(door->dr_cmd, realtail);

        if (held)
        {
            for (i = 0; i < 3; ++i) 
            {
                dup2(hold[i], i);
                dclose(hold[i]);
            }
            close(tty);
        }

        if (onConsole) 
        {
            if (dropDTR && !gotcarrier())
            {
                modemClose();
            }
        }

        if (watching)
        {
            watchoff();
            watching = NO;
        }

        if (status < 0)
            mPrintf("%s: status=%d\n ", door->dr_name, (int)status);

        homeSpace();                    /* flip back to the citadel homedir */
    }
}


/*
** makedoor()   add a shell escape to the system list
*/
static makedoor(rp, mode, name, line, remark)
struct doorway **rp;
char *name, *line, *remark;
{
    char *cname, *ctail;
    struct doorway *runner, *tmp, *malloc();
    char *strtok(), *xstrdup();

    cname = strtok(line," \t");
    ctail = strtok(NULL,"\n");

    tmp = malloc(sizeof tmp[0]);
    tmp->dr_mode = mode;
    strcpy(tmp->dr_name, name);
    tmp->dr_cmd  = xstrdup(cname);
    tmp->dr_dir  = linkdir ? xstrdup(linkdir) : "";
    tmp->dr_tail = ctail ? xstrdup(ctail) : "";
    tmp->dr_remark = remark ? xstrdup(remark) : "";
    tmp->dr_next = *rp;
    *rp = tmp;
}


/*
** initdoor()    read the list of shell escapes from disk
*/
void initdoor (void)
{
    int  doorMode;
    char *p, *name, *command, *remark;
    char *strrchr(), *strtok();
    char line[180];
    FILE *f, *safeopen();

    /*
     *  <name> <mode> <command> [tail] [#remark]
     */

    ctdlfile(line, cfg.sysdir, "ctdldoor.sys");
    f = safeopen(line, "r");

    if (f != NULL) 
    {
        while (fgets(line, 180, f)) 
        {
            int finished = NO;

            strtok(line, "\n");

            doorMode = 0;

            remark = strrchr(line, '#');

            if (remark != NULL)
            {
                *remark++ = '\0';
            }
            name = strtok(line,"\t ");

            if (name == NULL)
            {
                continue;
            }
            if (strlen(name) >= DOORSIZE)
            {
                crashout("door name <%s> is too long", name);
            }

            for (p = strtok(NULL,"\t "); (p != NULL) && *p && (!finished); p++)
            {
                switch (tolower(*p)) 
                {
                case 'u':
                    doorMode &= ~DR_UMASK;
                    break;
                case 's':
                    doorMode = (doorMode & ~DR_UMASK) | DR_SYSOP;
                    break;
                case 'a':
                    doorMode = (doorMode & ~DR_UMASK) | DR_AIDE;
                    break;
                case 'd':
                    doorMode |= DR_DIR;
                    break;
                case 'r':
                    doorMode |= DR_READ;
                    break;
                case 'w':
                    doorMode |= DR_WRITE;
                    break;
                case 'n':
                    doorMode |= DR_NAMED;
                    break;
                case 't':
                    doorMode |= DR_TAIL;
                    break;
                case 'f':
                    doorMode |= DR_FOSSIL;      /* door is FOSSIL aware */
                    break;
                case 'x':
                    doorMode |= DR_WATCHDOG;    /* reboot on lost carrier */
                    break;
                case 'c':
                    doorMode = (doorMode & ~DR_IOMASK) | DR_CONSOLE;
                    break;
                case 'm':
                    doorMode = (doorMode & ~DR_IOMASK) | DR_MODEM;
                    break;
                case 'l':
                    doorMode |= DR_LINKED;
                    linkdir = p + 1;
                    finished = YES;
                    break;
                default:
                    crashout("mode <%c> for door %s", *p, name);
                }
            }

            command = strtok(NULL, "\0");
            if (command != NULL)
            {
                makedoor(&doors, doorMode, name, command, remark);
            }
            linkdir = NULL;
        }

        /*
         * set up the shell for outside commands
         */
        shell = pickdoor("shell", doors);
        fclose(f);
    }

    if (cfg.shell && !shell)
    {
        makedoor(&shell, DR_SYSOP, "^L shell", &cfg.codeBuf[cfg.shell], NULL);
    }
}


/*
** dodoor()     do a shell escape
*/
void dodoor (void)
{
    char cmdline[80];
    char *cmd, *tail, *strtok();
    struct doorway *p, *toexec;
    extern char *CRfill;

    if (!loggedIn && !onConsole)
        return;

    getString("", cmdline, 80, '?', YES);

    if (cmdline[0] == '?')      /* list available doors */
    {
        int stat = NO;

        for (p = doors; p; p = p->dr_next)
        {
            if (islegal(p, &toexec)) 
            {
                if (stat == NO)
                {
                    mPrintf("Available doors:\n ");
                    stat = YES;
                }
                CRfill = "%11c | ";
                mPrintf("!%-11s| %s", p->dr_name, p->dr_remark);
                CRfill = NULL;
                mCR();
            }
        }
        if (stat == NO)
        {
            mPrintf("No available doors.\n ");
        }
        return;
    }
    normalise(cmdline);

    if (cmdline[0]) 
    {
        cmd = strtok(cmdline,"\t ");
        tail= strtok(NULL, "\n");

        toexec = finddoor(cmd, doors);
        if (toexec == NULL)
            mPrintf("%s: no such doorway\n ", cmd);
        else if (toexec == no_access)
            mPrintf("%s: permission denied\n ", cmd);
        else if (toexec == badroom)
            mPrintf("Not here!\n ");
        else
            rundoor(toexec, tail);
    }
}
