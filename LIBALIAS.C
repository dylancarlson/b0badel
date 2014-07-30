/*{ $Workfile:   libalias.c  $
 *  $Revision:   1.12  $
 * 
 *  network roomname aliasing
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

#include <string.h> 
#include "ctdl.h"
#include "library.h"

/* GLOBAL Contents:
**
**  load_alias() Load aliases from a file
**  chk_name()   return the alias for a name
**  chk_alias()  return the name for an alias
*/

#define MATCH_ALL 0xb0b

/*
** load_alias() - create a linked list of aliases
*/
struct alias *load_alias (char *filename)
{
    struct alias *base = NULL;
    FILE *file = safeopen(filename, "r");

    if (file)
    {
        char line[120];

        while (fgets(line, 120, file)) 
        {
            int index;
            char *system    = strtok(line, "\t\n");
            char *ourname   = strtok(NULL, "\t\n");
            char *theirname = strtok(NULL, "\t\n");

            if (system == NULL || ourname == NULL || theirname == NULL)
            {
                continue;
            }

            normalise(system);
            normalise(ourname);
            normalise(theirname);

            /*** find netTab index ***/

            index = stricmp(system, "%all") ? netnmidx(system) : MATCH_ALL;

            if (index == ERROR)
            {
                printf("\n bad alias system <%s>", system);
                continue;
            }
            else
            {
                struct alias *newone = xmalloc(sizeof(struct alias));

                newone->a_sys   = index;
                newone->a_name  = xstrdup(ourname);
                newone->a_alias = xstrdup(theirname);
                newone->a_next  = base;
                base = newone;
            }
        }

        fclose(file);
    }
    return base;
}


/*
** chk_alias() - return the alias for a name (or the name)
*/
char *chk_alias (struct alias *tab, int sys, char *name)
{
    struct alias *blind = NULL;

    while (tab) 
    {
        if (stricmp(tab->a_name, name) == 0) 
        {
            if (tab->a_sys == sys)
            {
                return tab->a_alias;
            }
            else if (tab->a_sys == MATCH_ALL)
            {
                blind = tab;
            }
        }
        tab = tab->a_next;
    }
    return blind ? blind->a_alias : name;
}


/*
** chk_name() - return the name for an alias
*/
char *chk_name (struct alias *tab, int sys, char *alias)
{
    struct alias *blind = NULL;

    while (tab) 
    {
        if (stricmp(tab->a_alias, alias) == 0) 
        {
            if (tab->a_sys == sys)
            {
                return tab->a_name;
            }
            else if (tab->a_sys == MATCH_ALL)
            {
                blind = tab;
            }
        }
        tab = tab->a_next;
    }
    return blind ? blind->a_name : alias;
}
