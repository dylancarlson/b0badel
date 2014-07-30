/*{ $Workfile:   libfloor.c  $
 *  $Revision:   1.11  $
 * 
 *  Floor library functions
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
 
#define LIBFLOOR_C

#include <io.h>
#include "ctdl.h"
#include "externs.h"

/* GLOBAL Contents:
**
** putFloor()       Write floor to disk.
** cleanFloor()     Discard unused floors.
** loadFloor()      Try to load up the floor table.
*/

#define FLSIZE  (sizeof floorTab[0])

/*
** putFloor() - write out a floor.
*/
void putFloor (int i)
{
    long size;

    dseek(floorfl, i*FLSIZE, 0);

    if (dwrite(floorfl, &floorTab[i], FLSIZE) != FLSIZE)
    {
        crashout("cannot write floor %d", i);
    }
}


/*
** cleanFloor() - clean out unused floors.
*/
void cleanFloor (void)
{
    int rover, i;
    int flUsed[MAXROOMS];

    for (i = 0; i < cfg.floorCount; ++i)
    {
        flUsed[i] = 0;
    }

    for (rover = 0; rover < MAXROOMS; ++rover)
    {
        for (i = 0; i < cfg.floorCount; ++i)
        {
            if ( roomTab[rover].rtflags.INUSE
              && roomTab[rover].rtflags.floorGen == floorTab[i].flGen) 
            {
                flUsed[i]++;
                break;
            }
        }
    }

    for (rover = 0; rover < cfg.floorCount; ++rover)
    {
        /* write out only changed floors */
        if (flUsed[rover] != floorTab[i].flInUse) 
        {
            floorTab[rover].flInUse = flUsed[rover];
            putFloor(rover);
        }
    }
}


/*
** loadFloor() - try to load up the floor table.
*/
void loadFloor (void)
{
    register i;

    floorTab = xmalloc(cfg.floorCount * FLSIZE);

    for (i = 0; i < cfg.floorCount; ++i) 
    {
        dseek(floorfl, i * FLSIZE, 0);

        if (dread(floorfl, &floorTab[i], FLSIZE) != FLSIZE)
        {
            crashout("cannot read floor %d", i);
        }
    }
    cleanFloor();               /* clean out empty floors */
}
