/*{ $Workfile:   event.h  $
 *  $Revision:   1.13  $
 *
 *  b0badel event handler definitions
}*/

#ifndef EVENT_H
#define EVENT_H

typedef enum 
{
    EVENT_NETWORK,                  /* invoke citadel networker */
    EVENT_PREEMPTIVE,               /* preemptive event         */
    EVENT_TIMEOUT,                  /* nonpreemptive event      */
    EVENT_OFFLINE,                  /* take system offline      */
    EVENT_ONLINE,                   /* put system online        */
    EVENT_EOL                       /* always the last one      */
    
} EVENT_TYPE;

/* events are set in b0badel.cfg using the following syntax:
** #event type [days] time "name" flags
*/

struct evt_type 
{
    int     evtTime;    /* minutes after midnight.              */

    char    evtDay;     /* what days the event can happen       */
                        /* (character bitfield: bit 8 unused)   */
    char    padding;    /* for alignment */

    EVENT_TYPE evtType; /* type of event                        */

    int     evtLen;     /* How long this event will last.       */
    label   evtMsg;     /* message citadel gives you before it  */
                        /* punts you into never-never land.     */
                        /* It will be printed in the form       */
                        /* "system going down at %d:%d for %s". */
    int     evtRel;     /* relative event flag for timeouts     */
                        /* if this flag is set, citadel will    */
                        /* find evtTime by adding evtRel to     */
                        /* curTime()                            */
    int     evtFlags;   /* special event flags for the net:     */
                        /* return code for everything else      */
};

#define TODAY(x, d)     ((x).evtDay & (0x01 << (d)) )
#define isNetwork(x)    ((x)->evtType == EVENT_NETWORK)
#define isPreemptive(x) (  (x)->evtType == EVENT_NETWORK    \
                        || (x)->evtType == EVENT_PREEMPTIVE \
                        || (x)->evtType == EVENT_OFFLINE    \
                        || (x)->evtType == EVENT_ONLINE)

#ifndef LIBTABL_C
extern struct evt_type *evtTab;        /* event table */
#endif

#ifndef EVENT_C
extern struct evt_type *nextEvt;       /* next event to execute */
#endif

#endif /* end ifndef EVENT_H */
