/************************************************************************
 *                              sysdep.h                                *
 *              #include file for all system-dependant                  *
 *              things found in STadel.                                 *
 *                                                                      *
 * 88Jul21 orc  Stripped down to get rid of lurking system dependencies *
 * 88Apr08 orc  #ifdefs put in for MS-DOS port                          *
 * 87May19 orc  created.                                                *
 *                                                                      *
 ************************************************************************/

#undef toupper
#undef tolower

#ifdef MSDOS

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

/*
** #defines for GEMDOS routines
*/
#define dcreat(f)       open(f,O_CREAT|O_EXCL|O_RDWR|O_BINARY,S_IREAD|S_IWRITE)
#define dopen(f,attr)   open(f,(attr)|O_BINARY)
#define dclose          close
#define dread           read
#define dwrite          write
#define dseek(f,l,w)    lseek(f, (long)(l), w)

#define dunlink         unlink
#define drename         rename

/*
 * MS-DOS/MS-C specific
 */
#define blkcpy          memcpy
#define blkfill         memset
#define xprintf         printf

/*
** our fast, clean, asm function calls int 21, function 2
*/
#undef putchar
#define putchar(n)      putch(n)

#endif

#ifdef ATARIST

#include <fcntl.h>

extern long gemdos(), xbios();

#define getch() (char)gemdos(0x7)

/*
** #defines for GEMDOS routines
*/
#define dcreat(f)       (int)gemdos(0x3c,f,0)
#define dopen(f,m)      (int)gemdos(0x3d,f,m)
#define dclose(f)       gemdos(0x3e,f)
#define dread(f,p,s)    gemdos(0x3f,f,(long)(s),(char*)(p))
#define dwrite(f,p,s)   gemdos(0x40,f,(long)(s),(char*)(p))
#define dseek(f,l,w)    gemdos(0x42,(long)(l), f, w)

#define dunlink(f)      gemdos(0x41,f)
#define drename(b,c)    gemdos(0x56,0,b,c)

#define dup(handle)     gemdos(0x45,handle)
/*
 * notice flipped arguments to be compatable with DOS (Unix?)
 */
#define dup2(src,dest)  gemdos(0x46,dest,src)

#define safeopen        fopen

#define CLK_TCK         200

#endif

