/*{ $Workfile:   libtabl.c  $
**  $Revision:   1.12  $
**
** Code to handle CTDLTABL.SYS
}*/

#define LIBTABL_C

#include <io.h>
#include "ctdl.h"
#include "externs.h"
#include "ctdlnet.h"
#include "event.h"
#include "poll.h"
#include "zaploop.h"

/* GLOBAL contents:
**
** readSysTab()     restores system state from ctdltabl.sys
** writeSysTab()    saves state of system in ctdltabl.sys
*/

struct config   cfg;
struct rTable   roomTab[MAXROOMS];
struct lTable   *logTab = NULL;
struct netTable *netTab = NULL;
struct evt_type *evtTab = NULL;
struct poll_t   *pollTab= NULL;
struct zaphash  *zap    = NULL;

char *indextable = "ctdltabl.sys";

struct 
{
    unsigned cfgSize;
    unsigned logSize;
    unsigned roomSize;
    unsigned evtSize;
} sysHeader;

/*** prototypes for static functions ***/

static ltread (void *block, unsigned size, int fd);


/*
** ltread() - reads in from file the important stuff
**            returns: TRUE on success, else FALSE
*/
static ltread (void *block, unsigned size, int fd)
{
    if (dread(fd, block, size) != size) 
    {
        fprintf(stderr,"could not read %u bytes\n",size);
        dclose(fd);
        return FALSE;
    }
    return TRUE;
}


/*
** readSysTab() - restores state of system from CTDLTABL.SYS
**                returns: TRUE on success, else FALSE
** destroys CTDLTABL.SYS after read, to prevent erroneous re-use
** in the event of a crash (as is the Citadel tradition).
*/
int readSysTab (int kill)
{
    int fd;
    int i;
    int retval = TRUE;

    if ((fd = dopen(indextable, O_RDONLY)) < 0) 
    {
        printf("No %s\n", indextable);    /* Tsk, tsk! */
        return FALSE;
    }

    if (dread(fd,&sysHeader,sizeof(sysHeader)) != sizeof(sysHeader)) 
    {
        printf("%s header read\n",indextable);
        close(fd);
        return FALSE;
    }

    if (!ltread(&cfg, sizeof(cfg), fd))
    {
        return FALSE;
    }

    if (sysHeader.cfgSize != sizeof(cfg))
    {
        printf("sysHeader.cfgSize != sizeof(cfg)\n");
        retval = FALSE;
    }
    if (sysHeader.evtSize != cfg.evtCount)
    {
        printf("sysHeader.evtSize != cfg.evtCount\n");
        retval = FALSE;
    }
    if (sysHeader.roomSize != sizeof(roomTab))
    {
        printf("sysHeader.roomSize != sizeof roomTab\n");
        printf("in other words, %d != %d\n",
            sysHeader.roomSize, sizeof roomTab);
        retval = FALSE;
    }
    if (sysHeader.logSize != sizeof(*logTab) * cfg.MAXLOGTAB) 
    {
        printf("sysHeader.logSize != sizeof(*logTab) * cfg.MAXLOGTAB\n");
        retval = FALSE;
    }
    if (retval == FALSE)
    {
        printf("Size mismatch in %s\n",indextable);
        dclose(fd);
        return FALSE;
    }

    logTab = xmalloc(sysHeader.logSize);

    if (!ltread(logTab, sysHeader.logSize, fd))
    {
        return FALSE;
    }

    if (!ltread(roomTab, sizeof roomTab, fd))
    {
        return FALSE;
    }

    if (cfg.netSize > 0)
    {
        netTab = xmalloc(sizeof(*netTab) * cfg.netSize);

        if (!ltread(netTab, (sizeof(*netTab) * cfg.netSize), fd))
        {
            return FALSE;
        }
    }
    else    /* malloc 1 for safety's sake */
    {
        netTab = xmalloc(sizeof(*netTab));
    }

    if (cfg.evtCount > 0)
    {
        evtTab = xmalloc(sizeof(*evtTab) * cfg.evtCount);
        if (!ltread(evtTab, (sizeof(*evtTab) * cfg.evtCount), fd))
        {
            return FALSE;
        }
    }
    else    /* malloc 1 for safety's sake */
    {
        evtTab = xmalloc(sizeof(*evtTab));
    }

    if (cfg.poll_count > 0)
    {
        pollTab = xmalloc(sizeof(*pollTab) * cfg.poll_count);
        if (!ltread(pollTab, (sizeof(*pollTab) * cfg.poll_count), fd))
        {
            return FALSE;
        }
    }
    else    /* malloc 1 for safety's sake */
    {
        pollTab = xmalloc(sizeof(*pollTab));
    }

    if (cfg.zap_count > 0)
    {
        zap = xmalloc(sizeof(*zap) * cfg.zap_count);
        if (!ltread(zap, (sizeof(*zap) * cfg.zap_count), fd))
        {
            return FALSE;
        }
    }
    else    /* malloc 1 for safety's sake */
    {
        zap = xmalloc(sizeof(*zap));
    }

    close(fd);

    if (kill)
    {
        unlink(indextable);
    }
    return TRUE;
}


/*
** writeSysTab() - saves state of system in CTDLTABL.SYS
**                 returns: TRUE on success, else ERROR
*/
int writeSysTab (void)
{
    int table, totwrote;

    unlink(indextable);

    if ((table = dcreat(indextable)) < 0) 
    {
        printf("Can't create %s!\n", indextable);
        return ERROR;
    }

    /* Write out some key stuff so we can detect bizarreness: */

    sysHeader.cfgSize = sizeof cfg;
    sysHeader.logSize = sizeof(*logTab) * cfg.MAXLOGTAB;
    sysHeader.roomSize= sizeof roomTab;
    sysHeader.evtSize = cfg.evtCount;

    totwrote  = dwrite(table, &sysHeader, sizeof(sysHeader));
    totwrote += dwrite(table, &cfg,       sizeof(cfg));
    totwrote += dwrite(table, logTab,     sizeof(*logTab) * cfg.MAXLOGTAB);
    totwrote += dwrite(table, roomTab,    sizeof(roomTab));

    if (cfg.netSize > 0)
    {
        totwrote += dwrite(table, netTab, (sizeof(*netTab) * cfg.netSize));
    }

    if (cfg.evtCount > 0)
    {
        totwrote += dwrite(table, evtTab, (sizeof(*evtTab) * cfg.evtCount));
    }

    if (cfg.poll_count > 0)
    {
        totwrote += dwrite(table, pollTab, (sizeof(*pollTab) * cfg.poll_count));
    }

    if (cfg.zap_count > 0)
    {
        totwrote += dwrite(table, zap, (sizeof(*zap) * cfg.zap_count));
    }

    dclose(table);

    return totwrote;
}
