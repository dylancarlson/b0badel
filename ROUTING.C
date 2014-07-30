/************************************************************************
 *                              routing.c                               *
 *      routines to check mail routing & do intellegent routing         *
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
 * 89Mar09 b0b  added prtAlias()                                        *
 *                                                                      *
 ************************************************************************/

#include "ctdl.h"
#include "ctdlnet.h"

extern struct msgB msgBuf;
extern struct logBuffer logBuf;
extern char loggedIn;

extern struct config cfg;               /* various variables we should  */
NA   route;                             /* if path aliased, use this route */

/*
 ****************************************************************
 *      verifynettable() - check if a user can [N]etwork mail   *
 ****************************************************************
 */
verifynettable(node)
char *node;
{
    char *plural();
    int cost;

    msgBuf.mbcost = 0;

    if (!(loggedIn && logBuf.lbflags.NET_PRIVS)) 
    {
        mPrintf("- You don't have net privileges\n ");
        return NO;
    }
    if (!node[0]) 
    {   /* enter a new nodename */
        doCR();
        retry:  getString(" System", node, NAMESIZE, '?', YES);
        if (node[0] == '?') 
        {
            mCR();
            listnodes(NO);
            prtAlias();         /* print names from the alias list */
            goto retry;
        }
        if (strlen(node) < 1)
            return NO;
    }
    if (findnode(node, &cost) == ERROR) 
    {
        mPrintf("No system `%s'\n ", node);
        return NO;
    }
    if (cost > logBuf.credit) 
    {
        mPrintf("You can't pay the postage (%s)\n ",
            plural("credit", (long)(cost)));
        return NO;
    }
    msgBuf.mbcost = cost;
    return YES;
}


/*
 ********************************************************
 * prtAlias() - print aliases from name in ctdlpath.sys *
 ********************************************************
 */

prtAlias()
{
    pathbuf temp;
    label name;
    FILE *pa, *safeopen();

    ctdlfile(temp, cfg.netdir, "ctdlpath.sys");

    if (cfg.pathalias && (pa=safeopen(temp,"r"))) 
    {

        mPrintf("---------");
        doCR();

        while (fgets(temp, 100, pa)) 
        {
            route[0] = 0;
            sscanf(temp, "%s", name);
            mPrintf(" %s", name);
            doCR();
        }
        fclose(pa);
    }
    else
        doCR();
}


