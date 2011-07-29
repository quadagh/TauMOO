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

/*
 * This describes the complete set of procedures that a network implementation
 * must provide.  See 'server.h' for the complete set of procedures such an
 * implementation is expected to use from the rest of the server.
 */

#ifndef Network_H
#define Network_H 1

#include "config.h"
#include "options.h"
#include "structures.h"

typedef struct {		/* Network's handle on a connection */
    void *ptr;
} network_handle;

typedef struct {		/* Network's handle on a listening point */
    void *ptr;
} network_listener;

#include "server.h"		/* Include this *after* defining the types */

extern const char *network_protocol_name(void);
				/* Returns a string naming the networking
				 * protocol in use.
				 */

extern const char *network_usage_string(void);
				/* Returns a string describing any extra
				 * network-specific command-line arguments,
				 * such as a port number, etc.
				 */

extern int network_initialize(int argc, char **argv,
			      Var * desc);
				/* ARGC and ARGV refer to just the network-
				 * specific arguments, if any, which always
				 * come after any network-independent args.
				 * Returns true iff those arguments were valid.
				 * On success, *DESC should be a MOO value to
				 * pass to network_make_listener() in order to
				 * create the server's initial listening point.
				 */

extern enum error network_make_listener(server_listener sl, Var desc,
					network_listener * nl,
					Var * canon, const char **name);
				/* DESC is the second argument in a call to the
				 * built-in MOO function `listen()'; it should
				 * be used as a specification of a new local
				 * point on which to listen for connections.
				 * If DESC is OK and a listening point is
				 * successfully made, then *NL should be the
				 * network's handle on the new listening point,
				 * *CANON a canonicalized version of DESC
				 * (reflecting any defaulting or aliasing),
				 * *NAME a human-readable name for the
				 * listening point, and E_NONE returned.
				 * Otherwise, an appropriate error should be
				 * returned.  By this call, the network and
				 * server exchange tokens representing the
				 * listening point for use in later calls on
				 * each other.
				 *
				 * NOTE: It is more than okay for the server
				 * still to be refusing connections.  The
				 * server's call to network_listen() marks the
				 * time by which the server must start
				 * accepting connections.
				 */

extern int network_listen(network_listener nl);
				/* The network should begin accepting
				 * connections on the given listening point,
				 * returning true iff this is now possible.
				 */

extern int network_send_line(network_handle nh, const char *line,
			     int flush_ok);
				/* The given line should be queued for output
				 * on the specified connection.  'line' does
				 * NOT end in a newline; it is up to the
				 * network code to add the appropriate bytes,
				 * if any.  If FLUSH_OK is true, then the
				 * network module may, if necessary, discard
				 * some other pending output to make room for
				 * this new output.  If FLUSH_OK is false, then
				 * no output may be discarded in this way.
				 * Returns true iff the given line was
				 * successfully queued for output; it can only
				 * fail if FLUSH_OK is false.
				 */

extern int network_send_bytes(network_handle nh,
			      const char *buffer, int buflen,
			      int flush_ok);
				/* The first BUFLEN bytes in the given BUFFER
				 * should be queued for output on the specified
				 * connection.  If FLUSH_OK is true, then the
				 * network module may, if necessary, discard
				 * some other pending output to make room for
				 * this new output.  If FLUSH_OK is false, then
				 * no output may be discarded in this way.
				 * Returns true iff the given bytes were
				 * successfully queued for output; it can only
				 * fail if FLUSH_OK is false.
				 */

extern int network_buffered_output_length(network_handle nh);
				/* Returns the number of bytes of output
				 * currently queued up on the given connection.
				 */

extern void network_suspend_input(network_handle nh);
				/* The network module is strongly encouraged,
				 * though not strictly required, to temporarily
				 * stop calling `server_receive_line()' for
				 * the given connection.
				 */

extern void network_resume_input(network_handle nh);
				/* The network module may once again feel free
				 * to call `server_receive_line()' for the
				 * given connection.
				 */

extern void network_set_connection_binary(network_handle, int);
				/* Set the given connection into or out of
				 * `binary input mode'.
				 */

extern int network_process_io(int timeout);
				/* This is called at intervals to allow the
				 * network to flush pending output, receive
				 * pending input, and handle requests for new
				 * connections.  It is acceptable for the
				 * network to block for up to 'timeout'
				 * seconds.  Returns true iff it found some I/O
				 * to do (i.e., it didn't use up all of the
				 * timeout).
				 */

extern const char *network_connection_name(network_handle nh);
				/* Return some human-readable identification
				 * for the specified connection.  It should fit
				 * into the phrase 'Connection accepted: %s'.
				 */

extern Var network_connection_options(network_handle nh,
				      Var list);
				/* Add the current option settings for the
				 * given connection onto the end of LIST and
				 * return the new list.  Each entry on LIST
				 * should be a {NAME, VALUE} pair.
				 */

extern int network_connection_option(network_handle nh,
				     const char *option,
				     Var * value);
				/* Return true iff the given option name
				 * is valid for the given connection, storing
				 * the current setting into *VALUE if valid.
				 */

extern int network_set_connection_option(network_handle nh,
					 const char *option,
					 Var value);
				/* Return true iff the given option/value pair
				 * is valid for the given connection, applying
				 * the given setting if valid.
				 */

#ifdef OUTBOUND_NETWORK
#include "structures.h"

extern enum error network_open_connection(Var arglist);
				/* The given MOO arguments should be used as a
				 * specification of a remote network connection
				 * to be made.  If the arguments are OK and the
				 * connection is successfully made, it should
				 * be treated as if it were a normal connection
				 * accepted by the server (e.g., a network
				 * handle should be created for it, the
				 * function server_new_connection should be
				 * called, etc.) and E_NONE should be returned.
				 * Otherwise, some other appropriate error
				 * value should be returned.  The caller of
				 * this function is responsible for freeing the
				 * MOO value in `arglist'.  This function need
				 * not be supplied if OUTBOUND_NETWORK is not
				 * defined.
				 */

#endif

extern void network_close(network_handle nh);
				/* The specified connection should be closed
				 * immediately, after flushing as much pending
				 * output as possible.  Effective immediately,
				 * the given network_handle will not be used
				 * by the server and the corresponding
				 * server_handle should not be used by the
				 * network.
				 */

extern void network_close_listener(network_listener nl);
				/* The specified listening point should be
				 * closed immediately.  Effective immediately,
				 * the given network_listener will not be used
				 * by the server and the corresponding
				 * server_listener should not be used by the
				 * network.
				 */

extern void network_shutdown(void);
				/* All network connections should be closed
				 * after flushing as much pending output as
				 * possible, and all listening points should be
				 * closed.  Effective immediately, no
				 * server_handles or server_listeners should be
				 * used by the network and no network_handles
				 * or network_listeners will be used by the
				 * server.  After this call, the server will
				 * never make another call on the network.
				 */

#endif				/* Network_H */

/* 
 * $Log: network.h,v $
 * Revision 1.3  1998/12/14 13:18:36  nop
 * Merge UNSAFE_OPTS (ref fixups); fix Log tag placement to fit CVS whims
 *
 * Revision 1.2  1997/03/03 04:19:10  nop
 * GNU Indent normalization
 *
 * Revision 1.1.1.1  1997/03/03 03:45:04  nop
 * LambdaMOO 1.8.0p5
 *
 * Revision 2.4  1996/03/10  01:15:13  pavel
 * Added support for `connection_option()'.  Release 1.8.0.
 *
 * Revision 2.3  1996/02/08  06:19:13  pavel
 * Added network_set_connection_binary() and network_connection_options().
 * Updated copyright notice for 1996.  Release 1.8.0beta1.
 *
 * Revision 2.2  1996/01/11  07:45:43  pavel
 * Added network_send_bytes() and network_buffered_output_length().  Removed
 * one more `unsigned' from the server.  Release 1.8.0alpha5.
 *
 * Revision 2.1  1995/12/30  23:57:35  pavel
 * Added support for the new multiple-listening-points interface.
 * Release 1.8.0alpha4.
 *
 * Revision 2.0  1995/11/30  04:52:44  pavel
 * New baseline version, corresponding to release 1.8.0alpha1.
 *
 * Revision 1.6  1992/10/23  23:03:47  pavel
 * Added copyright notice.
 *
 * Revision 1.5  1992/10/21  03:02:35  pavel
 * Converted to use new automatic configuration system.
 *
 * Revision 1.4  1992/09/26  02:26:30  pavel
 * Added support for printing the network protocol name on server start-up.
 *
 * Revision 1.3  1992/09/13  00:30:36  pavel
 * Added missing #include of config.h, for OUTBOUND_NETWORK option.
 *
 * Revision 1.2  1992/09/11  21:22:49  pavel
 * Added return value from network_process_io() indicating whether or not any
 * I/O was done (i.e., whether or not the entire timeout was used up).
 *
 * Revision 1.1  1992/07/20  23:23:12  pavel
 * Initial RCS-controlled version.
 */
