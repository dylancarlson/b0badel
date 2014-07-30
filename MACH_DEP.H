/************************************************************************
 *                              mach_dep.h                              *
 *      processor/compiler specific things used ONLY* by sysdep.c,      *
 *      libdep.c, io.c/io.s                                             *
 *                                                                      *
 *      88Apr08 orc     Created                                         *
 *                                                                      *
 ************************************************************************/

#ifdef ATARIST

#include <osbind.h>

#define mkdir   Dcreate

#define MFP     ((char*)0xfffffa01L)            /* rs232 status register */
#define CLOCK   ((long*)0x4baL)                 /* 200hz system clock */

#endif

#ifdef MSDOS

#define _NOPROTO
#include <time.h>
#include <bios.h>
#include <dos.h>
#include <signal.h>

#endif
