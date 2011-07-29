/******************************************************************************
  Copyright (c) 1994 Xerox Corporation.  All rights reserved.
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

/*
 * This module provides IP host name lookup with timeouts.  Because
 * many DNS servers are flaky and the normal UNIX name-lookup facilities just
 * hang in such situations, this interface comes in very handy.
 */

#ifndef Name_Lookup_H
#define Name_Lookup_H 1

#include "config.h"

extern int initialize_name_lookup(void);
				/* Initialize the module, returning true iff
				 * this succeeds.
				 */

extern unsigned32 lookup_addr_from_name(const char *name,
					unsigned timeout);
				/* Translate a host name to a 32-bit
				 * internet address in host byte order.  If
				 * anything goes wrong, return 0.  Dotted
				 * decimal address are translated properly.
				 */

extern const char *lookup_name_from_addr(struct sockaddr_in *addr,
					 unsigned timeout);
				/* Translate an internet address, contained
				 * in the sockaddr_in, to a host name.  If
				 * the translation cannot be done, the
				 * address is returned in dotted decimal
				 * form.
				 */

#endif				/* Name_Lookup_H */

/* 
 * $Log: name_lookup.h,v $
 * Revision 1.3  1998/12/14 13:18:26  nop
 * Merge UNSAFE_OPTS (ref fixups); fix Log tag placement to fit CVS whims
 *
 * Revision 1.2  1997/03/03 04:19:01  nop
 * GNU Indent normalization
 *
 * Revision 1.1.1.1  1997/03/03 03:45:04  nop
 * LambdaMOO 1.8.0p5
 *
 * Revision 2.0  1995/11/30  05:07:03  pavel
 * New baseline version, corresponding to release 1.8.0alpha1.
 *
 * Revision 1.1  1995/11/30  05:06:56  pavel
 * Initial revision
 */
