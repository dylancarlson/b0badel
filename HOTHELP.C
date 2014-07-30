/*{ $Workfile:   hothelp.c  $
 *  $Revision:   1.15  $
 *
 *  Menu-driven help functions for b0bdel
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
 
#include "ctdl.h"
#include "externs.h"
#include <string.h>
#include <ctype.h>

/* Original implementation notes by Paul Gunthier:
** 
** In CTDL.C instead of have the doHelp() call tutorial() it should now
** call hothelp(filename).
** 
** In the HeLP files, you insert lines containing % sign followed by the
** topic (read: filename) of the entries you want to have displayed in
** the menu. Add a space and then enter text to describe it.
** For example, an excerpt from a help file:
** 
** %FILES This menu item will display FILES.HLP
** %DOHELP This entry will re-show the main help file
** %FOO Help for idiots... :-)
** 
** etc...
** 
** The file name will be padded out to 8 characters and a letter inside
** square brackets will be added. The above will format into:
** 
** [a] FILES     This menu item will display FILES.HLP
** [b] DOHELP    This entry will reshow the main help file
** [c] FOO       Help for idiots... :-)
** 
** And then the prompt asking for a choice will appear. Every help file
** can contain these entries, and there is no limit to the depth that
** this routine can display menus.  If there are no % signs in the help
** file then no prompt for a choice is printed (cause no choices were
** displayed, right?).
*/


#define FN_WIDTH 14

/*** prototypes ***/

static void bFormat (char *string);
static int letter_help (FILE *helpfile);
static int getword (char *dest, char *source, int offset, int lim);


typedef char Key_Filename[FN_WIDTH]; 

static Key_Filename keyfile[26];

/*
** letterhelp() - called by hotblurb() 
*/
static int letter_help (FILE *helpfile)
{
    char        buf[MAXWORD];
    int         count = 0;      /* # items found */
    char        *selection;
    char        *menutext;
    int         index;
    int         flag = NO;

    mCR();

    /*** first, clear the keyfile array ***/

    for (index = 0; index < 26; index++)
    {
        *keyfile[index] = 0;
    }

    while ((fgets(buf, MAXWORD-1, helpfile)) && (outFlag != OUTSKIP)) 
    {
        /*** '%' signals a menu line ***/

        if (*buf == '%') 
        {
            selection = strtok(1 + buf,"\t ");

            menutext = strtok(NULL, "\n");

            if (menutext)
            {
                int leader = toupper(*selection);

                if (leader >= 'A' && leader <= 'Z')
                {
                    index = leader - 'A';

                    copystring(keyfile[index], selection, FN_WIDTH);

                    if (flag == NO)
                    {
                        flag = YES;
                    }

                    mPrintf(" %c) %c", leader, leader);
                    ++selection;

                    count = 8;

                    while (count--)
                    {
                        if (*selection >= 'A')
                        {
                            oChar(*selection++);
                        }
                        else
                        {
                            oChar(' ');
                        }
                    }
                    oChar(' ');

                    while (*menutext == ' ' || *menutext == '\t')
                    {
                        ++menutext;
                    }

                    bFormat(menutext);
                    doCR();
                }
            }
        }
        else 
        {
            bFormat(buf);
        }
    }

    if (!flag)
    {
        mCR();
    }
    return flag;
}


/*
** hotblurb() expects a complete filename as input AND in the %FILE.BLB
** strings.  It still looks for the file in HelpDir.  It acts on the first
** letter of the filename, rather than the sequencial a) b) c) d).
** More intuitive than the old hothelp().
*/
int hotblurb(char *filename)
{
    FILE    *blurbfile;
    pathbuf fn;
    char    nextfile[FN_WIDTH];
    int     more = YES;

    strcpy (nextfile, filename);

    while (more) 
    {
        more = NO;

        ctdlfile(fn, cfg.helpdir, "%s", nextfile);

        blurbfile = safeopen(fn, "r");

        if (blurbfile) 
        {
            if (outFlag != IMPERVIOUS)
            {
                outFlag = OUTOK;
            }

            if (letter_help(blurbfile) && outFlag != OUTSKIP) 
            {
                char key;

                outFlag = OUTOK;

                if (expert)
                {
                    mPrintf("\n Select: ");
                }
                else
                {
                    mPrintf("\n Select one, or press <Enter> ");
                }

                while ((key = getnoecho()) != '\n' && onLine())
                {
                    int i = toupper(key) - 'A';

                    if (i >= 0 && i < ESC && *keyfile[i]) 
                    {
                        oChar(key);
                        strcpy(nextfile, keyfile[i]);
                        more = YES;
                        break;
                    }
                    else
                    {
                        oChar(7);
                    }
                }
                mCR();
            }
            fclose(blurbfile);
        }
        else 
        {
            mPrintf("\n %s not found\n ", nextfile);
            return NO;
        }

    }
    return YES;
}



/*** the blurb macro processor ***/

/*
** bFormat() - formats a string to modem and console
*/
static void bFormat (char *string)
{
    char wordBuf[MAXWORD];
    int i = 0;

    while (string[i])
    {
        switch (outFlag)
        {
        case OUTOK:
        case IMPERVIOUS:
        case OUTPARAGRAPH:
            i = getword(wordBuf, string, i, MAXWORD - 1);
#if 0
            softWord(wordBuf);
#endif
            mFormat(wordBuf);       /* word might be multiple words! */

            if (mAbort())
            {
        default:
                return;
            }
        }
    }
    return;
}


/*
** getword() - get a word from a string
*/
static int getword (char *dest, char *source, int offset, int lim)
{
    char    *p, *first;
    int     leading_spaces;
    int     word_length;
    int     i, j, k;
    char    special = '\0';
    
    i = word_length = 0;

    source += offset;

    /* skip leading blanks if any */

    while (source[i] == ' ' && i < lim)
        i++;

    leading_spaces = i;

    first = &source[i];

    /* step over word */

    while (source[i] != ' ' && i < lim && source[i] != 0)
    {
        ++i;
        ++word_length;
    }

    if (word_length > 3 && first[1] == '!' && first[3] == '!')
    {
        /* special case for macros in parentheses (!m!) */

        special = *first++;
        --word_length;
    }

    if (word_length > 2 && first[0] == '!' && first[2] == '!')
    {
        /*** a macro was found ***/

        char *subst;

        switch (toupper(first[1]))
        {
        case 'D':
            subst = formDate();         /* !D! is Date macro */
            break;

        case 'T':
            subst = tod(YES);           /* !T! is Time macro */
            break;

        case 'N':                       /* !N! is Name macro */
            subst = loggedIn ? logBuf.lbname : NULL;
            break;

        case 'R':                       /* !R! is Realname macro */
            subst = loggedIn ? logBuf.lbrealname : NULL;
            break;

        case 'S':                       /* !S! is System name macro */
            subst = &cfg.codeBuf[cfg.nodeTitle];
            break;

        case 'L':                       /* !L! is Lobby macro */
            subst = roomTab[LOBBY].rtname;
            break;

        default:
            subst = NULL;
            break;
        }

        if (subst == NULL)
        {
            first[0] = first[1] = first[2] = '*';
        }
        else    /* macro processor */
        {
            while (leading_spaces--)
            {
                *dest++ = ' ';      /* the leading spaces */
            }

            if (special != '\0')
            {
                *dest++ = special;  /* special leading character */
            }

            strcpy(dest, subst);

            dest += strlen(subst);

            /* grab trailing chars */

            if (word_length > 3)
            {
                for (j = 3; j < word_length; ++j)
                {
                    *dest++ = first[j];
                }
            }

            /* pick up any trailing blanks */

            while (source[i] == ' ' && i < lim)
            {
                *dest++ = ' ';
                i++;
            }

            *dest = '\0';

            return offset + i;
        }
    }

    /* pick up any trailing blanks */

    while (source[i] == ' ' && i < lim)
    {
        i++;
    }

    /* copy word over */

    for (j = 0; j < i; j++)
    {
        *dest++ = *source++;
    }

    *dest = '\0';           /* null to tie off string */

    return offset+i;        /* return new offset */
}
