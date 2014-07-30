/*{ $Workfile:   zaploop.c  $
 *  $Revision:   1.11  $
 *
 *  the loop zapper, created by orc 88Jun17
}*/

#include <fcntl.h>
#include <io.h>
#include <string.h>
#include <stdlib.h>
#include "ctdl.h"
#include "zaploop.h"
#include "externs.h"
#include <ctype.h>

static struct zaploop zapnode;
static int zapdbx;
static int zapcur = ERROR;

static struct mupdate 
{
    long mulast;                /* newest message found here... */
    int  muloc;                 /* bucket for this message      */

} *nzmsg;

static int nzsize;

char checkloops = 0;

#define NORMDATE(x)     ((x)-2447314L)  /* make the date rel to Jun 1, '88 */

/* prototypes for local functions */

static void putx ();
static void getx (int);
static int loadx (char *);
static int makebucket (char *, int);


/*
** msgtime() -- get the creation date of the message in msgBuf.
*/
long msgtime(void)
{
    int count, hr, min;
    char apm;
    long days, parsedate();

    if ((days = parsedate(msgBuf.mbdate)) == ERROR)
    {
        return 0;
    }

    days = NORMDATE(days) * 1440L;

    count = sscanf(msgBuf.mbtime,"%d:%d %cm", &hr, &min, &apm);

    if (count < 2)
    {
        return ERROR;
    }
    if (hr == 12)       /* 12:xx < 1:00.... */
    {
        hr -= 12;
    }
    if (count == 3 && tolower(apm) == 'p')
    {
        hr += 12;
    }

    return days + (60L * hr) + min;
}


/*
** init_zap() - open the zaploop database
*/
void init_zap (void)
{
    register i;
    pathbuf zapfile;

    ctdlfile(zapfile, cfg.netdir, "ctdlloop.zap");

    if ((zapdbx=dopen(zapfile,O_RDWR)) < 0) 
    {
        splitF(netLog, "cannot open %s\n", zapfile);
        checkloops=0;
    }
    nzmsg = (struct mupdate*)malloc(sizeof *nzmsg);
    nzsize = 0;
    if (!nzmsg) 
    {
        splitF(netLog, "(zl) Out of memory\n");
        dclose(zapdbx);
        checkloops=0;
    }
}


/*
** close_zap() - close the zaploop database
*/
void close_zap (void)
{
    register i;

    if (nzmsg) 
    {
        for (i=0; i<nzsize; i++)
            if (nzmsg[i].mulast > 0L) 
            {
                getx(nzmsg[i].muloc);
                zapnode.lxlast = nzmsg[i].mulast;
                putx();
            }
        free(nzmsg);
        nzmsg = NULL;
        nzsize = 0;
    }
    if (zapdbx >= 0)
        dclose(zapdbx);
    zapdbx = -1;
}


/*
** loadx() -- load up the appropriate zap record for a node
*/
static int loadx (char *addr)
{
    int i;
    int ahash = hash(addr);

#define THIS(x) (stricmp(x,zapnode.lxaddr)==0 && zapnode.lxroom==thisRoom)

    if (addr[0] == 0 || strlen(addr) > 12)
        return 0;
    if (zapcur != ERROR && THIS(addr))
        return 1;
    for (i=0; i<cfg.zap_count; i++)
        if (ahash == zap[i].zhash) 
        {
            getx(zap[i].zbucket);
            if (stricmp(addr, zapnode.lxaddr) != 0)
                continue;
            while (zapnode.lxroom != thisRoom && zapnode.lxchain >= 0)
                getx(zapnode.lxchain);
            if (zapnode.lxroom != thisRoom)
                zap[i].zbucket = makebucket(addr, zap[i].zbucket);
            return 1;
        }
    /*
     * new node id -- add a new record
     */
    if (zap = realloc(zap, (1+cfg.zap_count) * sizeof(struct zaphash))) 
    {
        zap[cfg.zap_count].zhash = ahash;
        zap[cfg.zap_count].zbucket = makebucket(addr, -1);
        cfg.zap_count++;
        splitF(netLog, "Created new zap entry for %s (%d)\n",
            msgBuf.mboname, thisRoom);
        return 1;
    }
    splitF(netLog, "(loadx) Out of memory.\n");
    checkloops=0;
    return 0;
}


/*
** makebucket() -- make up a new zap entry for this address/room
*/
static int makebucket (char *address, int chain)
{
    long pos = dseek(zapdbx, 0L, 2);    /* seek to eof */

    if (pos >= 0)
        pos /= sizeof zapnode;
    zapcur = (int)pos;
    strcpy(zapnode.lxaddr, address);
    zapnode.lxlast = 0L;
    zapnode.lxchain= chain;
    zapnode.lxroom = thisRoom;
    putx();
    return zapcur;
}


static void getx(int i)
{
    zapcur = i;
    dseek(zapdbx, zapcur * sizeof zapnode, 0);
    dread(zapdbx, (char *)&zapnode, sizeof zapnode);
}


static void putx()
{
    if (zapcur != ERROR) 
    {
        dseek(zapdbx, zapcur * sizeof zapnode, 0);
        dwrite(zapdbx, (char *)&zapnode, sizeof zapnode);
    }
}


/* 
** notseen() - looping message?
*/
int notseen (void)
{
    long last;
    label chk;

    last = msgtime();

    if (last == ERROR || last == 0)     /* anonymous date or time maybe? */
    {
        return 1;
    }

    if (checkloops && normID(msgBuf.mborig, chk) && loadx(chk))
    {
        if (last > zapnode.lxlast) 
        {
            int i;
            struct mupdate *tmp;

            for (i=0; i<nzsize; i++)
            {
                if (nzmsg[i].muloc == zapcur)  
                {
                    if (last > nzmsg[i].mulast)
                        nzmsg[i].mulast = last;

                    return 1;       
                }
            }

            tmp = realloc(nzmsg, (1+nzsize)*sizeof(struct mupdate));

            if (tmp == NULL)
            {
                splitF(netLog, "(zl) out of memory\n");
                checkloops = 0;
            }
            else
            {
                nzmsg = tmp;
                nzmsg[nzsize].mulast = last;
                nzmsg[nzsize].muloc  = zapcur;
                ++nzsize;
            }
            return 1;
        }
        else
        {
            return 0;
        }
    }
    return 1;
}
