/*{ $Workfile:   libcryp.c  $
 *  $Revision:   1.12  $
 * 
 *  typical Citadel encrypter/decrypter (public domain)
}*/

#include "ctdl.h"
#include "library.h"    /* contains our prototype */

/*
** crypte() -- encrypt/decrypt data
*/
void crypte (void *buffer, register unsigned count, register unsigned seed)
{
    register char *buf = buffer;

    seed = (seed + cfg.cryptSeed) & 0xFF;

    while (count--) 
    {
        *buf++ ^= seed;
        seed = (seed + CRYPTADD) & 0xFF;
    }
}
