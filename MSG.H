/*{ $Workfile:   msg.h  $
 *  $Revision:   1.11  $
 * 
 *  messagebase sector buffer
}*/

struct mBuf 
{
    int  thisChar;              /* offset in this sector*/
    int  thisSector;            /* this sector..        */
    int  oldChar;               /* offset in old sector */
    int  oldSector;             /* old sector..         */
    char sectBuf[BLKSIZE];      /* data buffer          */
};



#ifndef CTDLMSG_C

extern struct mBuf msgs;

#endif
