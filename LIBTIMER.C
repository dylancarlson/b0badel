/*{ $Workfile:   libtimer.c  $
 *  $Revision:   1.12  $
 * 
 *  General timer functions
}*/

#include <time.h>
#include "ctdl.h"
#include "library.h"
#include "clock.h"

static long myclock;

/*
** startTimer() - Initialize the general timer
*/
void startTimer (void)
{
    myclock = clock();
}

/*
** chkTimeSince() -
**      A call to startTimer() must have preceded calls to this function.
**      RETURNS: Time in seconds since last call to startTimer().
*/
long chkTimeSince (void)
{
    return timeSince(myclock);
}
