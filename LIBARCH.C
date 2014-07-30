/*{ $Workfile:   libarch.c  $
 *  $Revision:   1.12  $
 * 
 *  Archive handling for b0badel
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
 
#define LIBARCH_C

#include <stdlib.h>
#include <string.h>
#include "ctdl.h"
#include "library.h"

/* GLOBAL Contents
**
**  addArchiveList()    add new archive to room archive list
**  findArchiveName()   gets requested archive name
**  initArchiveList()   eat archive list
*/

static struct any_list 
{
    int    any_room;
    char   *any_name;
    struct any_list *any_next;      /* link to next struct */

} roomarchive;


static char archive[] = "ctdlarch.sys";

static int addanylist   (struct any_list *base, int room, char *fn);
static int initList     (char *fileName, struct any_list *base);

/*
** addanylist() - Adds filename to archive list
*/
static int addanylist (struct any_list *base, int room, char *fn)
{
    int found;
    struct any_list *p = base;      /* beginning of linked list */

    while (p->any_next && (p->any_next->any_room != room))
    {
        p = p->any_next;
    }

    if (p->any_next)    /* pointer valid, changing name */ 
    {
        p = p->any_next;
        free(p->any_name);  /* free existing name */
        found = YES;
    }
    else
    {
        p->any_next = xmalloc(sizeof(roomarchive));
        p = p->any_next;
        p->any_next = NULL;
        found = NO;
    }

    p->any_room = room;
    p->any_name = xstrdup(fn);

    return found;
}


/*
** findArchiveName() - get the archive filename for a room
*/
char *findArchiveName (int room)
{
    struct any_list *p;

    for (p = roomarchive.any_next; p; p = p->any_next)
    {
        if (p->any_room == room)
        {
            return p->any_name;
        }
    }
    return NULL;
}


/*
** initArchiveList() - set up archive list
*/
void initArchiveList (void)
{
    struct any_list *p;
    pathbuf temp;

    ctdlfile(temp, cfg.sysdir, archive);
    initList(temp, &roomarchive);

    for (p = roomarchive.any_next; p; p = p->any_next) 
    {
        char *runner;
        char *hold = p->any_name;

        p->any_room = atoi(hold);

        if ((runner = strchr(hold, ' ')) != NULL)
        {
            ++runner;
        }
        else
        {
            runner = "";
        }

        p->any_name = xstrdup(runner);
        free(hold);
    }
}


/*
** initList() - set up list
*/
static int initList (char *fileName, struct any_list *base)
{
    int room;
    FILE *fd;
    char name[120];

    base->any_next = NULL;

    if ((fd = safeopen(fileName, "r")) != NULL) 
    {
        room = 0;

        while (fgets(name, 118, fd) != NULL) 
        {
            strtok(name,"\n");
            addanylist(base, room++, name);
        }
        fclose(fd);
        return YES;
    }
    return NO;
}


/*
** addArchiveList() - add to archive list
*/
int addArchiveList (int room, char *fn)
{
    FILE  *arch;
    struct any_list *p;
    char  replace;
    pathbuf filename;
    static char format[] = "%d %s\n";

    ctdlfile(filename, cfg.sysdir, archive);

    replace = addanylist(&roomarchive, room, fn);

    if ((arch = safeopen(filename, replace ? "w" : "a")) != NULL) 
    {
        if (replace) 
        {
            for (p = roomarchive.any_next; p; p = p->any_next)
            {
                fprintf(arch, format, p->any_room, p->any_name);
            }
        }
        else
        {
            fprintf(arch, format, room, fn);
        }
        fclose(arch);
        return YES;
    }
    mPrintf("Can't open %s\n ", filename);
    return NO;
}
