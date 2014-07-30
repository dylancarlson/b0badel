/*{ $Workfile:   hash.c  $
 *  $Revision:   1.12  $
 * 
 *  simple functions used by most Citadels
}*/

#define HASH_C

#include "ctdl.h"
#include "library.h"

/* GLOBAL Contents:
**
** hash()     - hashes a string to an integer
** labelcmp() - compare strings, equating '_' with ' '
*/


/*
** hash() - hashes a string to an integer
*/
int hash (char *str)
{
    int accum = 0, shift = 0, c;

    for (; (c = *str) != 0; (shift = (1 + shift) & 7), str++)
    {
        accum ^= ((c == '_') ? ' ' : toupper(c)) << shift;
    }
    return accum;
}


/*
** labelcmp() - compare strings, equating '_' with ' '
**              Returns 0 if strings match
*/
int labelcmp (char *p1, char *p2)
{
    char d1 = 0, d2 = 0;

    while (d1 == d2) 
    {
        if (!*p1)
        {
            break;
        }
        d1 = (*p1 == '_') ? ' ' : tolower(*p1);
        d2 = (*p2 == '_') ? ' ' : tolower(*p2);
        ++p1, ++p2;
    }
    return d1 - d2;
}
