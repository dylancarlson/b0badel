/************************************************************************
 *                              dirlist.h                               *
 ************************************************************************/

#define DNAMSIZ 12              /* needs to be different for Unix       */

typedef struct date_t {         /* packed into a work, just like dos    */
    unsigned _day   : 5;        /* 1..31        */
    unsigned _month : 4;        /* 1..12        */
    unsigned _year  : 7;        /* 80..127      */
} ;

struct dirList {                /* For listing files on disk.           */
   long fd_size;                /* file size.                           */
   struct date_t fd_date;       /* date created/modified.               */
   char fd_name[DNAMSIZ+1];     /* file name.                           */
};
