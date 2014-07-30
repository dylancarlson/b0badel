/*{ $Workfile:   libtag.c  $
 *  $Revision:   1.12  $
 * 
 *  File description tag handling for b0badel
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

#include <io.h>
#include <string.h> 
#include "ctdl.h"
#include "externs.h"

/* GLOBAL Contents:
**
** tagSetup()  Set up the description file.
** tagClose()  Close the description file.
** getTag()    Get the tag for a file.
** putTag()    Write a file tag into the descr file.
*/

static char *tagfilename = "$dir";
static char *tmpfilename = "$$$$";

static int preload;
static FILE *tagfile = NULL;

static int tagcmp (char *fname, char *tag);

/*
** tagcmp() - compare a filetag with a filename
*/
static int tagcmp (char *fname, char *tag)
{
    while (toupper(*fname) == toupper(*tag))
    {
        ++fname;
        ++tag;
    }

    if (*fname == 0 && (*tag == '\t' || *tag == ' '))
    {
        return 0;
    }

    if (*tag == 0 || (toupper(*fname) < toupper(*tag)))
    {
        return -1;
    }

    return 1;
}


/*
** tagSetup() -- opens up tagfile.
*/
int tagSetup (void)
{
    preload = YES;
    return xchdir(roomBuf.rbdirname) && (tagfile = safeopen(tagfilename,"r"));
}


/*
** tagClose() -- shuts down the tagfile
*/
void tagClose (void)
{
    if (tagfile != NULL)
    {
        fclose(tagfile);
        tagfile = NULL;
    }
}


/*
** putTag() -- updates a file tag
*/
void putTag (char *fname, char *desc)
{
    FILE *temp;
    char *flag;
    char line[500];

    if (tagfile = safeopen(tagfilename, "r")) 
    {
        temp = safeopen(tmpfilename, "w");

        while ((flag = fgets(line, 500, tagfile)) != NULL) 
        {
            if (tagcmp(fname, line) <= 0)
            {
                break;
            }
            fputs(line, temp);
        }
        fprintf(temp, "%s\t%s\n", fname, desc);

        if (flag != NULL && tagcmp(fname, line) != 0)
        {
            fputs(line, temp);
        }

        while (fgets(line, 500, tagfile))
        {
            fputs(line, temp);
        }

        tagClose();
        fclose(temp);

        dunlink(tagfilename);
        drename(tmpfilename, tagfilename);
    }
    else if (tagfile = safeopen(tagfilename, "w")) 
    {
        fprintf(tagfile, "%s\t%s\n", fname, desc);
        tagClose();
    }
    else
    {
        mPrintf("Can't write file tag!\n ");
    }
}


/*
** getTag() - returns the tag for a file.
*/
char *getTag (char *fname)
{
    if (tagfile != NULL) 
    {
        if (preload) 
        {
            msgBuf.mbtext[0] = 0;
            fgets(msgBuf.mbtext, MAXTEXT-10, tagfile);
            preload = NO;
        }

        while (tagcmp(fname, msgBuf.mbtext) > 0)
        {
            if (!fgets(msgBuf.mbtext, MAXTEXT-10, tagfile)) 
            {
                rewind(tagfile);
                return NULL;
            }
        }

        if (tagcmp(fname, msgBuf.mbtext) == 0) 
        {
            preload = YES;
            strtok(msgBuf.mbtext, "\n");
            return 1 + strpbrk(msgBuf.mbtext, "\t ");
        }
    }
    return NULL;
}
