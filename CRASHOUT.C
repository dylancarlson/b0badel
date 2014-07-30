/*{ $Workfile:   crashout.c  $
 *  $Revision:   1.11  $
 * 
 *  crash exit of b0badel
}*/
/* 
**    Copyright (c) 1989, 1991 by Robert P. Lee 
**        Sebastopol, CA, U.S.A. 95473-0846 
**               All Rights Reserved 
** 
**    Permission is granted to use these files for 
**  non-commercial purposes only.  You may not include 
**  them in whole or in part in any commercial software. 
** 
**    Use of this source code or resulting executables 
**  by military organizations, or by companies holding 
**  contracts with military organizations, is expressly 
**  forbidden. 
*/

#define CRASHOUT_C
 
#include <stdarg.h>
#include <stdlib.h>
#include "ctdl.h"
#include "externs.h"

void crashout (char *format, ...)
{
    va_list arg_ptr;

    va_start(arg_ptr, format);

    fprintf(stderr, "%s: ", program);
    vfprintf(stderr, format, arg_ptr);
    fputs(".\n", stderr);

    exit(CRASH_EXIT);
}
