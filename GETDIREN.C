/*
 * return a direntry
 */
#include <stdio.h>
#include "dirlist.h"
#include <dos.h>

static struct dirList tmp;
static struct find_t buffer;

struct dirList *getdirentry(char *pattern)
{
    unsigned day, mon, yr;
    unsigned status;

    if (pattern)
        status = _dos_findfirst(pattern, _A_NORMAL|_A_RDONLY, &buffer);
    else
        status = _dos_findnext(&buffer);

    if (status == 0) 
    {
        strcpy(tmp.fd_name, buffer.name);
        day = (buffer.wr_date & 0x001f);
        mon = (buffer.wr_date & 0x01e0) >> 5;
        yr  = (buffer.wr_date & 0xfe00) >> 9;
        tmp.fd_size       = buffer.size;
        tmp.fd_date._day  = day;
        tmp.fd_date._year = 80+yr;
        tmp.fd_date._month= (mon > 12) ? 0 : mon;
        return &tmp;
    }
    return NULL;
}
