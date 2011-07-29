/******************************************************************************
  Copyright (c) 1992, 1995, 1996 Xerox Corporation.  All rights reserved.
  Portions of this code were written by Stephen White, aka ghond.
  Use and copying of this software and preparation of derivative works based
  upon this software are permitted.  Any distribution of this software or
  derivative works must comply with all applicable United States export
  control laws.  This software is made available AS IS, and Xerox Corporation
  makes no warranty about the software, its performance or its conformity to
  any specification.  Any person obtaining a copy of this software is requested
  to send their name and post office or electronic mail address to:
    Pavel Curtis
    Xerox PARC
    3333 Coyote Hill Rd.
    Palo Alto, CA 94304
    Pavel@Xerox.Com
 *****************************************************************************/

#include "config.h"

#if SYS_STAT_H_NEEDS_HELP
#  include "my-types.h"
#endif

#include <sys/stat.h>

#if NDECL_FSTAT
#include "my-types.h"

extern int stat(const char *, struct stat *);
extern int fstat(int, struct stat *);
extern int mkfifo(const char *, mode_t);
#endif

#if !HAVE_MKFIFO
extern int mknod(const char *file, int mode, int dev);
#define mkfifo(path, mode)	mknod(path, S_IFIFO | (mode), 0)
#endif

/* 
 * $Log: my-stat.h,v $
 * Revision 1.3  1998/12/14 13:18:15  nop
 * Merge UNSAFE_OPTS (ref fixups); fix Log tag placement to fit CVS whims
 *
 * Revision 1.2  1997/03/03 04:18:55  nop
 * GNU Indent normalization
 *
 * Revision 1.1.1.1  1997/03/03 03:45:05  nop
 * LambdaMOO 1.8.0p5
 *
 * Revision 2.3  1996/02/08  06:02:22  pavel
 * Updated copyright notice for 1996.  Release 1.8.0beta1.
 *
 * Revision 2.2  1995/12/31  03:25:59  pavel
 * Added test for <sys/stat.h> needing help.  Release 1.8.0alpha4.
 *
 * Revision 2.1  1995/12/28  00:36:26  pavel
 * Added declaration of stat().  Release 1.8.0alpha3.
 *
 * Revision 2.0  1995/11/30  04:58:21  pavel
 * New baseline version, corresponding to release 1.8.0alpha1.
 *
 * Revision 1.2  1992/10/23  23:03:47  pavel
 * Added copyright notice.
 *
 * Revision 1.1  1992/10/23  22:25:00  pavel
 * Initial RCS-controlled version.
 */
