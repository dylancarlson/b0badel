/*{ $Workfile:   libroom.c  $
 *  $Revision:   1.11  $
 * 
 *  Typical Citadel room code (public domain)
}*/

#include <io.h>
#include <string.h>
#include "ctdl.h"
#include "externs.h"

/* GLOBAL contents:
**
** getRoom()        load given room into RAM                *
** putRoom()        store room to given disk slot           *
** noteRoom()       enter room into RAM index               *
** update_mail()    update message pointers for mail>       *
*/

#define RBSIZE  (sizeof roomBuf)

/*
** getRoom()
*/
void getRoom (int rm)
{
    long position = ((long)rm) * ((long)RBSIZE);
    int  actual;

    /* load room #rm into memory starting at buf */

    thisRoom = rm;
    dseek(roomfl, position, 0);

    if ((actual = dread(roomfl, &roomBuf, RBSIZE)) != RBSIZE)
    {
        crashout("getRoom(%d): read %d/expected %d", rm, actual, RBSIZE);
    }

    crypte(&roomBuf, RBSIZE, rm);

    if (thisRoom == MAILROOM)
    {
        update_mail();
    }
}


/*
** putRoom()
*/
void putRoom (int rm)
{
    long position = ((long)rm) * ((long)RBSIZE);
    int actual;

    crypte(&roomBuf, RBSIZE, rm);       /* encrypt the room? */

    dseek(roomfl, position, 0);
    if ((actual = dwrite(roomfl, &roomBuf, RBSIZE)) != RBSIZE)
    {
        crashout("putRoom(%d): wrote %d/expected %d", rm, actual, RBSIZE);
    }

    crypte(&roomBuf, RBSIZE, rm);       /* decrypt the room? */
}


/*
** noteRoom() -- enter room into RAM index array.
*/
void noteRoom (void)
{
    int i;
    long last = 0L;

    for (i = 0; i < MSGSPERRM; ++i)
    {
        if (roomBuf.msg[i].rbmsgNo > last)
        {
            last = roomBuf.msg[i].rbmsgNo;
        }
    }

    roomBuf.rblastMessage = last;
    roomTab[thisRoom].rtlastMessage = last;
    roomTab[thisRoom].rtlastLocal = roomBuf.rblastLocal;
    roomTab[thisRoom].rtlastNet   = roomBuf.rblastNet;
    strcpy(roomTab[thisRoom].rtname, roomBuf.rbname);
    roomTab[thisRoom].rtgen = roomBuf.rbgen;
    copy_struct(roomBuf.rbflags, roomTab[thisRoom].rtflags);
}


/*
** update_mail() - update message pointers for mail
*/
void update_mail (void)
{
    int i;

    for (i = 0; i < MSGSPERRM; i++) 
    {
        roomBuf.msg[i].rbmsgLoc = logBuf.lbslot[i];
        roomBuf.msg[i].rbmsgNo  = logBuf.lbId  [i];
    }
    noteRoom();
}
