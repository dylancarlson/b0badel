/*{ $Workfile:   libroute.c  $
 *  $Revision:   1.13  $
 *
 *  routines to check mail routing & do intellegent routing
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
#include "ctdl.h"
#include "externs.h"
#include "ctdlnet.h"

/* GLOBAL Contents:
**
** parsepath()             Tell caller whether it's a net address
** findnode()              Figure access & directions to a node
** changeto()              Modify to: address
*/

static NA   route;             /* if path aliased, use this route */
static char howto;             /* how the node was found       */

/* static function prototypes */

static int netlook (char *node, int everything);
static int ckpath  (int *cost);
static int ckalias (char *node, int *cost, int *idx);
static int cknode  (char *node);


/* netlook() - look up a nodename in ctdlnet.sys
**             Unless you tell it to, it will not find the
**             netaddresses of systems that are not on a net.
*/
static int netlook (char *node, int everything)
{
    int x;

    if ((x = netnmidx(node)) != ERROR
      && (everything || netBuf.nbflags.what_net))
    {
        return x;
    }
    return ERROR;
}

/*
** ckpath() - see if the leading node of `route' is okay.
*/
static int ckpath (int *cost)
{
    label node;
    char *q, *p;
    int i, x;

    if (p = strchr(route, '!'))           /* only do bang-path aliasing */
    {
        *p++ = '\0';
    }

    copystring(node, route, NAMESIZE);
    normalise(node);

    if ((x = netlook(node, NO)) != ERROR) 
    {
        if (p)
        {
            strcpy(route, p);
        }
    }
    else if (cfg.hub && (x = netlook(&cfg.codeBuf[cfg.hub], YES)) != ERROR)
    {
        *cost += cfg.hubcost;
    }

    if (netBuf.nbflags.ld)
    {
        *cost += cfg.ld_cost;
    }

    return x;
}

/*
** ckalias() - look up a name in ctdlpath.sys
*/
static int ckalias (char *node, int *cost, int *idx)
{
    pathbuf temp;
    label name;
    FILE *pa;

    *idx = ERROR;
    ctdlfile(temp, cfg.netdir, "ctdlpath.sys");

    if (cfg.pathalias && (pa=safeopen(temp,"r"))) 
    {
        while (fgets(temp, 100, pa)) 
        {
            route[0] = 0;
            *cost = 0;
            sscanf(temp, "%s %s %d", name, route, cost);

            if (labelcmp(name, node) == 0) 
            {
                fclose(pa);
                howto = iALIAS;
                return (route[0] && (*idx=ckpath(cost)) == ERROR) ? NO : YES;
            }
        }
        fclose(pa);
    }
    return NO;
}

/*
** findnode() - get access & cost for a node
*/
int findnode (char *node, int *cost)
{
    int x;
    char *p;

    normalise(node);

restart:

    if (p = strrchr(node, '.'))
    {
        if (ckalias(p, cost, &x))
        {
            if (x == ERROR) 
            {
                *p = 0;
                goto restart;
            }
            else    /* pass off to another domain */
            {
                howto = iDOMAIN;
                return x;
            }
        }
    }

    if (NNisok(node)) 
    {
        route[0] = 0;

        if ((x=netlook(node, NO)) != ERROR) 
        {
            *cost = netBuf.nbflags.ld ? cfg.ld_cost : 0;
            howto = iDIRECT;
            return x;
        }

        if (ckalias(node, cost, &x))
        {
            return x;
        }

        if (cfg.hub) 
        {
            strcpy(route, node);
            x = netlook(&cfg.codeBuf[cfg.hub], YES);
            *cost = cfg.hubcost;
            howto = iHUBBED;
            return x;
        }
    }
    return ERROR;
}

/*
** cknode() - cut apart a domain address and find out if it's us.                                     *
*/
static int cknode (char *node)
{
    char *p;
    FILE *f;
    label domain;
    NA path, temp;
    int cost, x;
    char found;

    while (p = strrchr(node, '.')) 
    {
        if ( !labelcmp(p, ".citadel")
          || (ckalias(p, &cost, &x) && x == ERROR))
        {
            *p = 0;
        }
        else
        {
            return NO;
        }
    }

    /* if it can't be resolved as a domain, treat it like a vanilla address */

    return (labelcmp(node, &cfg.codeBuf[cfg.nodeName]) == 0);
}

/* parsepath() - take and parse a possible netpath. return
**               YES if it's a netpath & put first node
**               into `node'.  If `edit' is true, cut
**               the offending node off `netpath'.
*/
int parsepath (char *netpath, char *node, int edit)
{
    NA temp;
    char *p;
    int self;

    if (netpath == NULL || node == NULL)        /* be paranoid... */
        return NO;

restart:

    if (p = strrchr(netpath,'@'))   /* check name@system first */
    {
        copystring(node, 1+p, NAMESIZE);
        normalise(node);
        if ((self = cknode(node)) || edit)
        {
            *p = 0;                 /* replace @ with NUL terminator   */
            normalise(netpath);     /* cut off trailing spaces in name */
        }

        if (self)
        {
            goto restart;
        }

        return YES;
    }
    strcpy(temp, netpath);

    if (p = strchr(temp,'!'))       /* then bangpaths */
    {
        *p++ = 0;
        copystring(node, temp, NAMESIZE);       /* pull out the node */
        normalise(node);
        if ((self = cknode(node)) || edit)
        {
            strcpy(netpath, p);
        }
        if (self)
        {
            goto restart;
        }
        return YES;
    }

    return NO;
}


/*
** changeto() - modify netBuf.mbto for proper routing
*/
void changeto (char *oldto, char *firstnode)
{
    switch (howto) 
    {
    case iALIAS:
        sprintf(msgBuf.mbto, route, firstnode);
        sprintf(EOS(msgBuf.mbto), "!%s", oldto);
        return;

    case iHUBBED:
        sprintf(msgBuf.mbto, "%s!%s", firstnode, oldto);
        return;

    case iDOMAIN:
        sprintf(msgBuf.mbto, "%s@%s", oldto, firstnode);
        return;

    default:
        strcpy(msgBuf.mbto, oldto);
    }
}
