/************************************************************************
 *                              ctdlnet.h                               *
 *              #include file for the Citadel networker.                *
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
 * 88Aug10 orc  removed old-style netmail structures                    *
 * 88Jul16 orc  removed old-style net file req/sendfile structures      *
 * 88Jul12 orc  remote-command byte changed to avoid conflict with c-86 *
 * 87Dec23 orc  Strip out NeedsProcessing() macros and handle on the fly*
 * 87Dec18 orc  Datastructure support for external autodialers          *
 * 87Nov08 orc  network roomshare routing                               *
 * 87Jul24 orc  net password data structure support, autonet support.   *
 * 87May17 orc  extracted from ctdl.h                                   *
 *                                                                      *
 ************************************************************************/

#ifndef CTDLNET_H
#define CTDLNET_H
                    /* Network request codes */
#define HANGUP          0       /* Terminate networking                 */
#define LOGIN           255     /* login to other side                  */
#define SEND_MAIL       1       /* Send mail                            */
#define FILE_REQUEST    3       /* Request a number of files            */
#define NET_ROOM        5       /* Send a shared room                   */
#define CHECK_MAIL      6       /* Check for recipient validity         */
#define SEND_FILE       7       /* Send a file to another system        */
#define ROLE_REVERSAL   201     /* Reverse roles                        */

#define REMOTE_CMD      10      /* execute command on remote system     */
#define OPTIONS         11      /* send options string                  */
#define BATCH_SEND      12      /* ymodem batch files during networking */
#define BATCH_REQUEST   13      /* ymodem batch files during networking */


#define ROUTE_SIG       '@'     /* mbroute[0]                           */
#define ROUTE_ORIG      'O'     /* message originated here              */
#define ROUTE_LOCAL     'L'     /* message came from a local system     */
#define ROUTE_HUB       'H'     /* message came from a hub              */

/************************************************************************
 * How message routing goes:                                            *
 * all* netmessages are given a mbroute id.                             *
 * two classes of link:  local & hub.                                   *
 * a hub link mails all messages (@O, @L, @H) and addresses them to @H  *
 * a local link mails all @O and @H messages, addressing them to @L     *
 *                                                                      *
 *      [Local ->@L][-> Hub ->@H][-> Hub ->@L][-> Local]                *
 *                                                                      *
 ************************************************************************/

#define ROUTEOK(x)      (((x)->mbroute[0]) == ROUTE_SIG)
#define ROUTECHAR(x)    ((x)->mbroute[1])
#define ROUTEFROM(x)    (&((x)->mbroute[2]))

#define CALL_OK         0       /* callout went ok                      */
#define NO_ID           1       /* couldn't exchange network ids        */
#define NOT_STABILISED  2       /* callout not stabilised               */

#define NO_ERROR        0       /* No error (ends transmission)         */
#define NO_RECIPIENT    1       /* No recipient found                   */
#define BAD_FORM        2       /* Something's wrong                    */
#define UNKNOWN         99      /* Something's REALLY wrong (eek!)      */

#define iDIRECT 0               /* mail directly to system              */
#define iALIAS  1               /* mail route found in netalias.sys     */
#define iHUBBED 2               /* mail forwarded to a #hub             */
#define iDOMAIN 3               /* mail passed to another domain        */

struct nflags {                 /* Any and all reasons to call this node*/
    char in_use;                /* Is this record even in use?          */
    char mailpending;           /* Outgoing mail?                       */
    char filepending;           /* Any file requests?                   */
    char expand0;               /* files to be sent?                    */

    char ld;                    /* long-distance system (also pollcnt)  */
    char rec_only;              /* can we call this L-D system?         */
    char dialer;                /* external dialer #n?                  */
    char ld_rr;                 /* allow l-d role reversal?             */

    long what_net;              /* which networks this node is in       */
    char poll_day;              /* days we must poll this node          */
    char refuse;                /* don't route netmail through this node*/

    short wwiv_id;              /* node id for WWIV systems (else 0)    */  
};

struct netroom {
    long     NRlast;            /* Highest net message in this room     */
    unsigned NRgen;
    int      NRidx;             /* roomTab[] position of shared room    */
    int      NRhub;             /* backboned with the other system?     */
};

struct netBuffer {
    label  netId;               /* Node id                      */
    label  netName;             /* Node name                    */
    label  myPasswd;            /* password I expect from other node */
    label  herPasswd;           /* password other node expects from me */
    struct nflags nbflags;      /* Flags                        */
    char   baudCode;            /* Baud code for this node      */
    char   dummy;               /* for alignment */
    struct netroom shared[SHARED_ROOMS];
    char   access[40];          /* For alternative access       */
};

#define sharing(i)  \
   ( (netBuf.shared[i].NRidx >= 0)   \
  && (roomTab[netBuf.shared[i].NRidx].rtflags.SHARED) \
  && (roomTab[netBuf.shared[i].NRidx].rtgen == netBuf.shared[i].NRgen) )

struct netTable {
    int           ntnmhash;
    int           ntidhash;
    struct nflags ntflags ;
    struct netroom Tshared[SHARED_ROOMS];
};

#ifndef LIBTABL_C
extern struct netTable *netTab;
#endif

#ifndef LIBNET_C
extern struct netBuffer netBuf;
#endif

int  mesgpending  (struct netroom *nr);

#endif /* end ifndef CTDLNET_H */
