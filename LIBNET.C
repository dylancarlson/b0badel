/*{ $Workfile:   libnet.c  $
 *  $Revision:   1.12  $
 * 
 *  Net functions used by everybody
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
 
#define LIBNET_C

#include <io.h>
#include <string.h>
#include "ctdl.h"
#include "externs.h"
#include "ctdlnet.h"

/* GLOBAL Contents:
**
**  getNet()    gets a node from CTDLNET.SYS
**  putNet()    puts a node to CTDLNET.SYS
**  netnmidx()  Search net for given node name
*/

struct netBuffer netBuf;

/*
** getNet() - load netBuf
*/
void getNet (int n)
{
    lseek(netfl, (long)n * (long)sizeof(netBuf), 0);

    if (read(netfl, &netBuf, sizeof netBuf) != sizeof netBuf)
    {
        crashout("getNet(%d) -read failed", n);
    }
    crypte(&netBuf, sizeof netBuf, thisNet=n);
}


/*
** putNet() - store netBuf
*/
void putNet (register int n)
{
    thisNet = n;

    copy_struct(netBuf.nbflags, netTab[n].ntflags);
    copy_array(netBuf.shared, netTab[n].Tshared);

    crypte(&netBuf, sizeof netBuf, n);
    lseek(netfl, (long)n * (long)sizeof(netBuf), 0);

    if (write(netfl, &netBuf, sizeof(netBuf)) != sizeof(netBuf))
    {
        crashout("putNet-write failed");
    }
    crypte(&netBuf, sizeof netBuf, n);
}


/*
**  netnmidx() - Search net for a name
*/
int netnmidx (char *name)
{
    register unsigned i, h;

    h = hash(name);

    for (i = 0; i < cfg.netSize; ++i) 
    {
        if ((h == netTab[i].ntnmhash) && netTab[i].ntflags.in_use) 
        {
            getNet(i);
            if (!labelcmp(netBuf.netName, name))
            {
                return i;
            }
        }
    }
    return ERROR;
}


/*
** NNisok() - is this netname ok?
*/
int NNisok (register char *node)
{
    while (*node)
    {
        if (isalnum(*node) || strchr("* _-.", *node))
        {
            ++node;
        }
        else
        {
            return 0;
        }
    }
    return 1;
}
