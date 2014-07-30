/*{ $Workfile:   protocol.h  $
 *  $Revision:   1.12  $
}*/

/************************************************************************
 *                      Transfer protocol constants                     *
 ************************************************************************/

/* take care to keep this in sync with the protocol[] array in roomb.c */

enum PRoToCoLs          /* goofy case avoids compiler warning */
{
    ASCII = 0,
    VANILLA,            /* "vanilla ascii" -- terminated by CAN-CAN */
    XMODEM,
    YMODEM,
    CAPTURE,            /* capture to held message buffer */
    TODISK,             /* archive to disk */
    TOPRINTER
};


#if 0

/* orc's old code */

#define ASCII   0
#define VANILLA 1       /* "vanilla ascii" -- terminated by CAN-CAN */
#define XMODEM  2
#define YMODEM  3
#define WXMODEM 4
#define KERMIT  5       /* Maybe */
#define ZMODEM  6       /* Maybe */
#define CAPTURE 7       /* capture to held message buffer */
#define TODISK  8       /* archive to disk */

#endif
