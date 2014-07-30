/*
 *
 * zaploop.h:  structures used by the loop zapper
 *
 */

struct zaploop {
    label lxaddr;       /* node id for the offending system     */
    long  lxlast;       /* newest message here                  */
    int   lxroom;       /* room on our system for this message  */
    int   lxchain;      /* hash chain for next room             */
} ;

struct zaphash {
    int zhash;
    int zbucket;
} ;

extern struct zaphash *zap;

