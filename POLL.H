/*{ $Workfile:   poll.h  $
 *  $Revision:   1.11  $
 * 
 *  data structures for net polling
}*/

#ifndef POLL_H
#define POLL_H

struct poll_t {
    int p_net;                  /* net to poll */
    int p_start;                /* minutes after midnight */
    int p_end;                  /* minutes after midnight */
    int p_days;                 /* days to poll this system */
} ;

#define poll_today(x, d)        ((x) & (1 << (d)))

#ifndef LIBTABL_C
extern struct poll_t *pollTab;
#endif

#endif /* end ifndef POOL_H */

