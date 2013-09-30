/*
 * Copyright (c) 2010, The Android Open Source Project.
 * Copyright (c) 2013, Canonical Ltd
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of The Android Open Source Project nor the names
 *    of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Send notification to Upstart (via a bridge already listening
 * on the other end of UPSTART_BRIDGE_SOCKET) when services change state
 * to allow the bridge to emit Upstart events which jobs on the host
 * side can react to.
 *
 * This code is essentially a minor rework of the standard system
 * utility 'watchprops.c'.
 *
 * Author: James Hunt <james.hunt@ubuntu.com>
 *
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>

#include <cutils/properties.h>

#include <sys/atomics.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#undef LOG_TAG
#define LOG_TAG "UpstartPropertyWatcher"
#include <utils/Log.h>

#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>

#define MAX_WATCHES 1024

/* Use the "special" /dev/socket/ directory which is configured to
 * "bleed through" from the host.
 */
#define UPSTART_BRIDGE_SOCKET "/dev/socket/upstart-text-bridge"

extern prop_area *__system_property_area__;

typedef struct pwatch pwatch;

struct pwatch
{
	const prop_info  *pi;
	unsigned          serial;
};

static pwatch watchlist[MAX_WATCHES];

static int socket_fd = -1;

static void
notify_upstart (const prop_info *pi)
{
	char     name[PROP_NAME_MAX];
	char     value[PROP_VALUE_MAX];
	char    *x;
	int      bytes;
	ssize_t  ret;
	int      saved;

	/* '<name>=<value>\n\0' */
	char buffer[PROP_NAME_MAX + 1 + PROP_VALUE_MAX + 1 + 1];

	assert (pi);

	__system_property_read (pi, name, value);

	for (x = value; *x; x++) {
		/* Make all values printable */
		if (! isprint (*x))
			*x = '.';
	}

	assert (name);

	bytes = sprintf (buffer, "%s=%s\n", name, value);
	ALOGI ("Property changed: %s", buffer);
	if (bytes <= 0) {
		ALOGE ("Failed to format buffer");
		exit (1);
	}

	if (write (socket_fd, buffer, bytes) < 0) {
		saved = errno;
		ALOGE ("Failed to write %lu bytes to socket '%s' on fd %d (%d [%s])",
				(unsigned long int)bytes,
				UPSTART_BRIDGE_SOCKET,
				socket_fd,
				saved, strerror (saved));
		exit (1);
	}
}

static void
setup_upstart_socket (void)
{
	struct sockaddr_un   sun_addr;
	socklen_t            addrlen;
	int                  ret;
	int                  saved;
	const char          *path = UPSTART_BRIDGE_SOCKET;
	size_t               len;

	memset (&sun_addr, 0, sizeof (struct sockaddr_un));
	sun_addr.sun_family = AF_UNIX;

	len = strlen (path);

	if (len > sizeof (sun_addr.sun_path)) {
		ALOGE ("Path too long '%s'", UPSTART_BRIDGE_SOCKET);
		exit (1);
	}

	addrlen = sizeof (sun_addr.sun_family) + len;

	strncpy (sun_addr.sun_path, path, sizeof (sun_addr.sun_path));

	/* Handle abstract names */
	if (sun_addr.sun_path[0] == '@')
		 sun_addr.sun_path[0] = '\0';

	socket_fd = socket (AF_UNIX, SOCK_STREAM, 0);

	if (socket_fd < 0) {
		saved = errno;
		ALOGE ("Failed to create socket for '%s' (%d [%s])",
				UPSTART_BRIDGE_SOCKET,
				saved, strerror (saved));
		exit (1);
	}

	ret = connect (socket_fd, (struct sockaddr *)&sun_addr, sizeof (struct sockaddr_un));
	if (ret < 0) {
		saved = errno;
		ALOGE ("Failed to connect socket for '%s' on fd %d (%d [%s])",
				UPSTART_BRIDGE_SOCKET, socket_fd,
				saved, strerror (saved));
		exit (1);
	}

	ALOGI ("Upstart property watcher connected to socket '%s'",
			UPSTART_BRIDGE_SOCKET);
}

static void
signal_handler (int signum)
{
	assert (socket_fd);

	if (signum == SIGTERM || signum == SIGINT)
		close (socket_fd);
}

int
main (int argc, char *argv[])
{
	prop_area    *pa;
	unsigned      serial;
	unsigned      count;
	unsigned      n;

	pa = __system_property_area__;
	assert (pa);
	serial = pa->serial;
	count = pa->count;

	ALOGI ("Starting upstart property watcher");

	ALOGI ("Property count %d", count);

	if (count >= MAX_WATCHES) exit (1);

	/* Connect to Upstart bridge running on host */
	setup_upstart_socket ();

	signal (SIGTERM, signal_handler);
	signal (SIGINT, signal_handler);

	for (n = 0; n < count; n++) {
		watchlist[n].pi = __system_property_find_nth (n);
		watchlist[n].serial = watchlist[n].pi->serial;
	}

	for (;;) {

		do {
			__futex_wait (&pa->serial, serial, 0);
		} while (pa->serial == serial);

		while (count < pa->count){
			watchlist[count].pi = __system_property_find_nth (count);
			watchlist[count].serial = watchlist[n].pi->serial;
			notify_upstart (watchlist[n].pi);
			count++;

			if (count == MAX_WATCHES) exit (1);
		}

		for (n = 0; n < count; n++) {
			unsigned tmp = watchlist[n].pi->serial;
			if (watchlist[n].serial != tmp) {
				notify_upstart (watchlist[n].pi);
				watchlist[n].serial = tmp;
			}
		}
	}

	return 0;
}
