/*
 * dateread.h: defines for reading from dates.
 */
#define dEVERY  0               /* read every message */
#define dBEFORE 1               /* read messages before a date */
#define dAFTER  2               /* read messages after a date */

extern char dPass;              /* what <things> to pass through */
extern long dDate;              /* what date to read from */

/*** prototypes ***/

long parsedate (char *);
long filedate (struct dirList *);

