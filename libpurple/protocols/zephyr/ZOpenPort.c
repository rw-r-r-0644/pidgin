/* This file is part of the Project Athena Zephyr Notification System.
 * It contains source for the ZOpenPort function.
 *
 *	Created by:	Robert French
 *
 *	Copyright (c) 1987 by the Massachusetts Institute of Technology.
 *	For copying and distribution information, see the file
 *	"mit-copyright.h".
 */

#include "internal.h"
#include <purple.h>
#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

Code_t
ZOpenPort(unsigned short *port)
{
	GSocketAddress *bindin = NULL;
	GError *error = NULL;

	ZClosePort();

	__Zephyr_socket = g_socket_new(G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_DATAGRAM,
	                               G_SOCKET_PROTOCOL_DEFAULT, &error);
	if (__Zephyr_socket == NULL) {
		purple_debug_error("zephyr", "Failed to open socket: %s",
		                   error->message);
		g_error_free(error);
		return ZERR_INTERNAL;
	}

	bindin = g_inet_socket_address_new_from_string("0.0.0.0", port ? *port : 0);
	if (!g_socket_bind(__Zephyr_socket, bindin, FALSE, &error)) {
		gint err = ZERR_INTERNAL;
		if (error->code == G_IO_ERROR_ADDRESS_IN_USE && port && *port) {
			err = ZERR_PORTINUSE;
		}
		purple_debug_error("zephyr", "Failed to bind socket to port %d: %s",
		                   port ? *port : 0, error->message);
		g_error_free(error);
		g_object_unref(bindin);
		g_clear_object(&__Zephyr_socket);
		return err;
	}
	g_object_unref(bindin);

	bindin = g_socket_get_local_address(__Zephyr_socket, &error);
	if (bindin == NULL) {
		purple_debug_error("zephyr", "Failed to get bound socket port: %s",
		                   error->message);
		g_error_free(error);
		g_clear_object(&__Zephyr_socket);
		return ZERR_INTERNAL;
	}
	__Zephyr_port =
	        g_inet_socket_address_get_port(G_INET_SOCKET_ADDRESS(bindin));
	if (port) {
		*port = __Zephyr_port;
	}
	g_object_unref(bindin);

	return ZERR_NONE;
}
