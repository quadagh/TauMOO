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

/* Multi-user networking protocol implementation for TCP/IP on BSD UNIX */

#include "my-inet.h"		/* inet_addr() */
#include <errno.h>		/* EMFILE, EADDRNOTAVAIL, ECONNREFUSED,
				   * ENETUNREACH, ETIMEOUT */
#include "my-in.h"		/* struct sockaddr_in, INADDR_ANY, htons(),
				   * htonl(), ntohl(), struct in_addr */
#include "my-socket.h"		/* socket(), AF_INET, SOCK_STREAM,
				   * setsockopt(), SOL_SOCKET, SO_REUSEADDR,
				   * bind(), struct sockaddr, accept(),
				   * connect() */
#include "my-stdlib.h"		/* strtoul() */
#include "my-string.h"		/* memcpy() */
#include "my-unistd.h"		/* close() */

#include "config.h"
#include "list.h"
#include "log.h"
#include "name_lookup.h"
#include "net_proto.h"
#include "options.h"
#include "server.h"
#include "streams.h"
#include "timers.h"
#include "utils.h"

#include "net_tcp.c"

const char *
proto_name(void)
{
    return "BSD/TCP";
}

int
proto_initialize(struct proto *proto, Var * desc, int argc, char **argv)
{
    int port = DEFAULT_PORT;

    proto->pocket_size = 1;
    proto->believe_eof = 1;
    proto->eol_out_string = "\r\n";

    if (!tcp_arguments(argc, argv, &port))
	return 0;
	
	initialize_name_lookup();
	
    desc->type = TYPE_INT;
    desc->v.num = port;
    return 1;
}

enum error
proto_make_listener(Var desc, int *fd, Var * canon, const char **name)
{
    struct sockaddr_in address;
    int s, port, option = 1;
    static Stream *st = 0;

    if (!st)
	st = new_stream(20);

    if (desc.type != TYPE_INT)
	return E_TYPE;

    port = desc.v.num;
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
	log_perror("Creating listening socket");
	return E_QUOTA;
    }
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
		   (char *) &option, sizeof(option)) < 0) {
	log_perror("Setting listening socket options");
	close(s);
	return E_QUOTA;
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = bind_local_ip;
    address.sin_port = htons(port);
    if (bind(s, (struct sockaddr *) &address, sizeof(address)) < 0) {
	enum error e = E_QUOTA;

	log_perror("Binding listening socket");
	if (errno == EACCES)
	    e = E_PERM;
	close(s);
	return e;
    }
    if (port == 0) {
	int length = sizeof(address);

	if (getsockname(s, (struct sockaddr *) &address, &length) < 0) {
	    log_perror("Discovering local port number");
	    close(s);
	    return E_QUOTA;
	}
	canon->type = TYPE_INT;
	canon->v.num = ntohs(address.sin_port);
    } else
	*canon = var_ref(desc);

    stream_printf(st, "port %d", canon->v.num);
    *name = reset_stream(st);

    *fd = s;
    return E_NONE;
}

int
proto_listen(int fd)
{
    listen(fd, 5);
    return 1;
}

enum proto_accept_error
proto_accept_connection(int listener_fd, int *read_fd, int *write_fd,
			const char **name)
{
    int timeout = server_int_option("name_lookup_timeout", 5);
    int fd;
    struct sockaddr_in address;
    int addr_length = sizeof(address);
    static Stream *s = 0;

    if (!s)
	s = new_stream(100);

    fd = accept(listener_fd, (struct sockaddr *) &address, &addr_length);
    if (fd < 0) {
	if (errno == EMFILE)
	    return PA_FULL;
	else {
	    log_perror("Accepting new network connection");
	    return PA_OTHER;
	}
    }
    *read_fd = *write_fd = fd;
    stream_printf(s, "%s, port %d",
		  lookup_name_from_addr(&address, timeout),
		  (int) ntohs(address.sin_port));
    *name = reset_stream(s);
    return PA_OKAY;
}

void
proto_close_connection(int read_fd, int write_fd)
{
    /* read_fd and write_fd are the same, so we only need to deal with one. */
    close(read_fd);
}

void
proto_close_listener(int fd)
{
    close(fd);
}

#ifdef OUTBOUND_NETWORK

#include "exceptions.h"
#include "structures.h"

static Exception timeout_exception;

static void
timeout_proc(Timer_ID id, Timer_Data data)
{
    RAISE(timeout_exception, 0);
}

enum error
proto_open_connection(Var arglist, int *read_fd, int *write_fd,
		      const char **local_name, const char **remote_name)
{
    /* These are `static' rather than `volatile' because I can't cope with
     * getting all those nasty little parameter-passing rules right.  This
     * function isn't recursive anyway, so it doesn't matter.
     */
    static const char *host_name;
    static int port;
    static Timer_ID id;
    int s, result, length;
    int timeout = server_int_option("name_lookup_timeout", 5);
    static struct sockaddr_in addr;
    static Stream *st1 = 0, *st2 = 0;

	if (!outbound_network_enabled)
		return E_PERM;
	
    if (!st1) {
	st1 = new_stream(20);
	st2 = new_stream(50);
    }
    if (arglist.v.list[0].v.num != 2)
	return E_ARGS;
    else if (arglist.v.list[1].type != TYPE_STR ||
	     arglist.v.list[2].type != TYPE_INT)
	return E_TYPE;

    host_name = arglist.v.list[1].v.str;
    port = arglist.v.list[2].v.num;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = lookup_addr_from_name(host_name, timeout);
    if (addr.sin_addr.s_addr == 0)
	return E_INVARG;

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
	if (errno != EMFILE)
	    log_perror("Making socket in proto_open_connection");
	return E_QUOTA;
    }
    
	if (bind_local_ip != INADDR_ANY) {
		static struct sockaddr_in local_addr;
		
		local_addr.sin_family = AF_INET;
		local_addr.sin_addr.s_addr = bind_local_ip;
		local_addr.sin_port = 0;
		/* In theory, if the original listen() succeeded,
		 * then this should too, but who knows, really? */
		if (bind(s, (struct sockaddr *) &local_addr, sizeof(local_addr)) < 0) {
			enum error e = E_QUOTA;
			
			log_perror("Binding local address in proto_open_connection");
			if (errno == EACCES)
				e = E_PERM;
			close(s);
			return e;
		}
	}
    TRY
	id = set_timer(server_int_option("outbound_connect_timeout", 5),
		       timeout_proc, 0);
    result = connect(s, (struct sockaddr *) &addr, sizeof(addr));
    cancel_timer(id);
    EXCEPT(timeout_exception)
	result = -1;
    errno = ETIMEDOUT;
    reenable_timers();
    ENDTRY

	if (result < 0) {
	close(s);
	if (errno == EADDRNOTAVAIL ||
	    errno == ECONNREFUSED ||
	    errno == ENETUNREACH ||
	    errno == ETIMEDOUT)
	    return E_INVARG;
	log_perror("Connecting in proto_open_connection");
	return E_QUOTA;
    }
    length = sizeof(addr);
    if (getsockname(s, (struct sockaddr *) &addr, &length) < 0) {
	close(s);
	log_perror("Getting local name in proto_open_connection");
	return E_QUOTA;
    }
    *read_fd = *write_fd = s;

    stream_printf(st1, "port %d", (int) ntohs(addr.sin_port));
    *local_name = reset_stream(st1);

    stream_printf(st2, "%s, port %d", host_name, port);
    *remote_name = reset_stream(st2);

    return E_NONE;
}
#endif				/* OUTBOUND_NETWORK */

char rcsid_net_bsd_tcp[] = "$Id: net_bsd_tcp.c,v 1.3 1998/12/14 13:18:27 nop Exp $";

/* 
 * $Log: net_bsd_tcp.c,v $
 * Revision 1.3  1998/12/14 13:18:27  nop
 * Merge UNSAFE_OPTS (ref fixups); fix Log tag placement to fit CVS whims
 *
 * Revision 1.2  1997/03/03 04:19:02  nop
 * GNU Indent normalization
 *
 * Revision 1.1.1.1  1997/03/03 03:45:02  nop
 * LambdaMOO 1.8.0p5
 *
 * Revision 2.5  1996/03/10  01:13:48  pavel
 * Moved definition of DEFAULT_PORT to options.h.  Release 1.8.0.
 *
 * Revision 2.4  1996/02/08  06:35:55  pavel
 * Renamed TYPE_NUM to TYPE_INT.  Updated copyright notice for 1996.
 * Release 1.8.0beta1.
 *
 * Revision 2.3  1995/12/31  00:02:17  pavel
 * Fixed variable name clash that led to compilation warnings.  Added support
 * for the new multiple-listening-points interface.  Release 1.8.0alpha4.
 *
 * Revision 2.2  1995/12/28  00:34:43  pavel
 * Removed old support for protocol-specific input EOL conventions.
 * Release 1.8.0alpha3.
 *
 * Revision 2.1  1995/12/11  07:42:34  pavel
 * Added support for $server_options.name_lookup_timeout and
 * $server_options.outbound_connect_timeout.
 *
 * Release 1.8.0alpha2.
 *
 * Revision 2.0  1995/11/30  04:46:40  pavel
 * New baseline version, corresponding to release 1.8.0alpha1.
 *
 * Revision 1.13  1993/08/11  03:14:12  pavel
 * -- Worked around a problem in some systems' declaration of gethostbyname().
 * -- Changed some bogus %d's to %u's in calls to *scanf() and *printf(),
 *    guided by warnings from GCC...
 *
 * Revision 1.12  1993/08/05  19:00:07  pavel
 * Made another local variable in proto_open_connection `static' to avoid
 * volatility warnings.
 *
 * Revision 1.11  1993/08/04  02:40:17  pavel
 * Sigh.  I never will learn not to check anything in until *after* it
 * compiles...
 *
 * Revision 1.10  1993/08/04  02:19:05  pavel
 * -- Added support for better logging of outbound connections.
 *
 * Revision 1.9  1993/08/04  00:25:54  pavel
 * Use strtoul() instead of strtol() for the port argument.
 *
 * Revision 1.8  1993/08/04  00:19:54  pavel
 * -- Fixed declaration of variables holding TCP ports to be unsigned.
 * -- Close server_socket if the setsockopt() fails.
 * -- Support new `network_listen()' protocol.
 * -- Set a timeout on outbound network connections.
 *
 * Revision 1.7  1992/10/23  23:03:47  pavel
 * Added copyright notice.
 *
 * Revision 1.6  1992/10/21  03:02:35  pavel
 * Converted to use new automatic configuration system.
 *
 * Revision 1.5  1992/10/17  20:43:13  pavel
 * Changed to use "my-inet.h" rather than <arpa/inet.h>.
 *
 * Revision 1.4  1992/10/06  01:36:45  pavel
 * Moved non-blocking code to net_multi.c, replacing it with code to set
 * proto->believe_eof appropriately.
 *
 * Revision 1.3  1992/10/04  04:58:03  pavel
 * Fixed #include's for declaration of inet_addr().  Also replaced INADDR_ANY
 * by htonl(INADDR_ANY), for portability.
 *
 * Revision 1.2  1992/09/26  02:22:53  pavel
 * Added support for printing the network protocol name on server start-up.
 *
 * Revision 1.1  1992/09/23  17:14:17  pavel
 * Initial RCS-controlled version.
 */
