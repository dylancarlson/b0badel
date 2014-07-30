/************************************************************************
 *                              nfs.c                                   *
 *              Network send/requestfile functions                      *
 *                                                                      *
 *                Copyright 1988 by David L. Parsons                    *
 *                       All Rights Reserved                            *
 *                                                                      *
 *             Permission is granted to use these sources               *
 *          for non-commercial purposes ONLY.  You may not              *
 *          include them in whole or in part in any commercial          *
 *          or shareware software; you may not redistribute             *
 *          these sources  unless you have the express written          *
 *          consent of David L. Parsons.                                *
 *                                                                      *
 * 88Jul16 orc  Created.                                                *
 *                                                                      *
 ************************************************************************/

#include "ctdl.h"
#include "ctdlnet.h"
#include "dirlist.h"

/************************************************************************
 *                                                                      *
 *      nfs_put()               spool a file request                    *
 *      nfs_process()           process file requests                   *
 *      nf_request()            ask for file(s) from caller             *
 *      * netsendsingle()       send a file                             *
 *      nf_send()               fling files across the net              *
 *                                                                      *
 ************************************************************************/

extern struct config cfg;
extern struct netBuffer netBuf;
extern struct msgB msgBuf, tempMess;
extern FILE *netLog;

FILE *safeopen();

/*
 ************************************************************************
 *      nfs_put() - spool a file request for the specified netnode      *
 ************************************************************************
 */
nfs_put(place, cmd, file, dir, room)
char *file, *dir, *room;
{
    FILE *nfs;
    char *filename = msgBuf.mbtext;

    ctdlfile(filename, cfg.netdir, "%d.nfs", place);
    if (nfs=safeopen(filename, "a")) 
    {
        switch (cmd) 
        {
        case FILE_REQUEST:
            fprintf(nfs, "REQUEST %s %s %s\n", file, dir, room);
            break;
        case SEND_FILE:
            fprintf(nfs, "SEND %s\n", file);
            break;
        }
        getNet(place);
        if (!netBuf.nbflags.filepending) 
        {
            netBuf.nbflags.filepending = YES;
            putNet(place);
        }
        fclose(nfs);
    }
    else
        mPrintf("cannot open %s\n ", filename);
}


/*
 ************************************************
 *      nfs_process() - process file requests   *
 ************************************************
 */
nfs_process()
{
    FILE *nfs;
    FILE *ffs;
    char thisline[160];
    char *cmd, *file, *dir, *room, *strtok();
    char *filename = tempMess.mbtext;
    char *failname = &tempMess.mbtext[200];
    extern int rmtslot;

    ctdlfile(filename, cfg.netdir, "%d.nfs", rmtslot);
    if (nfs=safeopen(filename,"r")) 
    {
        while (fgets(thisline, 158, nfs) && gotcarrier())
            if (cmd = strtok(thisline, "\t "))
                switch (*cmd) 
                {
                case 'S':
                    if (file=strtok(NULL,"\t\n "))
                        nf_send(file);
                    break;
                case 'R':
                    file = strtok(NULL, "\t ");
                    dir  = strtok(NULL, "\t ");
                    room = strtok(NULL, "\n");
                    if (room)
                        nf_request(room, file, dir);
                    break;
                }
        if (gotcarrier()) 
        {
            netBuf.nbflags.filepending = NO;
            fclose(nfs);
            dunlink(filename);
        }
        else
        {
            /*
             * lost carrier -- respool the rest of the requests.
             */
            ctdlfile(failname, cfg.netdir, "$pending");
            if (ffs = safeopen(failname, "w")) 
            {
                while (fgets(thisline,158,nfs))
                    fputs(thisline, ffs);
                fclose(nfs);
                fclose(ffs);
                dunlink(filename);
                drename(failname, filename);
            }
        }
    }
}


/*
 ********************************************************
 *      nf_request() - ask for file(s) from caller      *
 ********************************************************
 */
nf_request(room, file, direct)
char *room;
char *file;
char *direct;
{
    int    sendARchar();
    int    rBstatus;
    extern label rYfile;
    extern char batchWC;
    extern FILE *upfd;

    if (netchdir(direct)) 
    {
        if (netcommand(BATCH_REQUEST, room, file, NULL)) 
        {
            splitF(netLog, "Request %s (%s)\n", file, room);
            batchWC = YES;
            while ((rBstatus=recXfile(sendARchar)) == 0)
                ;
            batchWC = NO;
        }
        homeSpace();
    }
}


/*
 ****************************************
 *      netsendsingle() - send a file   *
 ****************************************
 */
static
netsendsingle(fn)
struct dirList *fn;
{
    FILE *fd;
    label size;
    int status=1;
    char *plural();

    if (fd = safeopen(fn->fd_name, "rb")) 
    {
        sprintf(size, "%ld", fn->fd_size);
        splitF(netLog, "sending %s (%s)\n",
            fn->fd_name, plural("byte", fn->fd_size));
        status = netcommand(SEND_FILE, fn->fd_name, "1", size, NULL)
            && typeWC(fd);
        fclose(fd);
    }
    return status;
}


/*
 ************************************************
 *      nf_send() - fling files across the net  *
 ************************************************
 */
nf_send(pathspec)
char *pathspec;
{
    char *p, *strrchr();
    label filename;

#ifdef ATARIST
    if (p=strrchr(pathspec,'\\')) 
    {
        *p++ = 0;
        copystring(filename, p, NAMESIZE);
    }
    else if (p = strrchr(pathspec, ':')) 
    {
        copystring(filename, ++p, NAMESIZE);
        *p = 0;
    }
    else
    {
        copystring(filename, pathspec, NAMESIZE);
        *pathspec = 0;
    }
#endif
#ifdef MSDOS
    int i;

    for (i=strlen(pathspec); i>=0; --i)
        if (pathspec[i] == '\\' || pathspec[i] == '/')
            break;
    if (i>=0) 
    {
        pathspec[i++] = 0;
        copystring(filename, &pathspec[i], NAMESIZE);
    }
    else if (p = strrchr(pathspec, ':')) 
    {
        copystring(filename, ++p, NAMESIZE);
        *p = 0;
    }
    else
    {
        copystring(filename, pathspec, NAMESIZE);
        *pathspec = 0;
    }
#endif
    if (netchdir(pathspec)) 
    {
        netwildcard(netsendsingle, filename);
        homeSpace();
    }
}
