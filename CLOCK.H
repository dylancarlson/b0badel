/*{ $Workfile:   clock.h  $
 *  $Revision:   1.11  $
 * 
 *  clock structure used by get_time() & set_time() 
}*/

struct clock {
    unsigned yr;
    unsigned mo;
    unsigned dy;
    unsigned hr;
    unsigned mm;
    unsigned ss;
} ;


#ifndef LIBMISC_C

extern struct clock now;   /* declared in libmisc.c */

#endif


/* clock related prototypes */

long julian_date (struct clock *time);

struct clock *timeis   (struct clock *clk);
struct clock *get_time (void);

char *day_of_week (struct clock *clk);

