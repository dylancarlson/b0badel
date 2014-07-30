/*{ $Workfile:   libdata.c  $
 *  $Revision:   1.0  $
 * 
 *  GLOBAL Data for b0badel's library functions
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
 
#define LIBDATA_C

#include "ctdl.h"

char Debug = NO;                /* debugging switch */

int thisRoom = LOBBY;           /* room currently in roomBuf    */
int roomfl;                     /* room file                    */

struct aRoom  roomBuf;          /* room buffer                  */

int thisFloor = LOBBYFLOOR;     /* current floor                */
int floorfl;                    /* floor file                   */

struct flTab *floorTab;         /* floor table                  */

char *CRfill = NULL;            
char onConsole = YES;           /* input coming from the console?   */

int thisNet;
int netfl;

char *_day[] = 
{
    "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
};
