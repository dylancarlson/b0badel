/*{ $Workfile:   driver.c  $
 *  $Revision:   1.11  $
 * 
 *  Comm port routines for the IBM PC 
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

#define DRIVER_C
 
#include "ctdl.h"
#include "externs.h"

/* GLOBAL Contents:
**
** setmodem()              Set up the modem the way citadel wants
** getMod()                Get a seven-bit character from modem
*/

/* GLOBAL Contents of IO.ASM (for reference)
**
** gotcarrier()            Does the modem have carrier?
** dobreak()               Set or clear a break.
** setBaud()               Set the system baud rate.
** modemOpen()             Let the modem accept calls
** modemClose()            Do not let the modem accept calls
** setmodem()              Set up the modem the way citadel wants
** fixmodem()              Restore the modem to original state
** MIReady()               Are characters waiting on the modem?
** getraw()                Get a character from the modem
** mflush()                clear i/o on the modem.
** modout()                put a character to the modem
*/

#define FOSSIL 1

/*
** setmodem() condition the modem for the BBS.
*/
void setmodem (void)
{
    int retval = enablecom(cfg.com_port);

    if (!retval)
    {
        crashout("Cannot init COM%d", 1 + cfg.com_port);
    }

    fossil = (retval == FOSSIL);

    return;
}


/*
** getMod() get a 7 bit char from the modem
*/
int getMod (void)
{
    return getraw() & 0x7f;
}
