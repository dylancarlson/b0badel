/*{ header.h
**
** read and display Citadel message headers
**
** author: b0b
**
}*/

#ifndef HEADER_H
#define HEADER_H

typedef struct 
{
    label id;                 /* local number of message              */
    label date;               /* creation date                        */
    label time;               /* creation time                        */
    label room;               /* creation room                        */
    NA    auth;               /* name of author                       */
    NA    to;                 /* private message to                   */
    label oname;              /* short human name for origin system   */
    char  subject [ORGSIZE];  /* subject field     -b0b-              */

} HEADER_DATA;

#endif
