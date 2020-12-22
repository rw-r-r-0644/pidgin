/*
 * purple
 *
 * Copyright (C) 2002-2003, Herman Bloggs <hermanator12002@yahoo.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */

#include <config.h>

#include <glib/gi18n-lib.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <io.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/timeb.h>
#include <sys/stat.h>
#include <time.h>
#include <glib.h>
#include "debug.h"
#include "libc_internal.h"
#include "util.h"
#include <glib/gstdio.h>
#include "util.h"

#ifndef S_ISDIR
# define S_ISDIR(m) (((m)&S_IFDIR)==S_IFDIR)
#endif

/* socket.h */
int wpurple_getsockname(int socket, struct sockaddr *addr, socklen_t *lenptr) {
        if(getsockname(socket, addr, lenptr) == SOCKET_ERROR) {
                errno = WSAGetLastError();
                return -1;
        }
        return 0;
}

/* fcntl.h */
/* This is not a full implementation of fcntl. Update as needed.. */
int wpurple_fcntl(int socket, int command, ...) {

	switch( command ) {
	case F_GETFL:
		return 0;

	case F_SETFL:
	{
		va_list args;
		int val;
		int ret=0;

		va_start(args, command);
		val = va_arg(args, int);
		va_end(args);

		switch( val ) {
		case O_NONBLOCK:
		{
			u_long imode=1;
			ret = ioctlsocket(socket, FIONBIO, &imode);
			break;
		}
		case 0:
		{
			u_long imode=0;
			ret = ioctlsocket(socket, FIONBIO, &imode);
			break;
		}
		default:
			errno = EINVAL;
			return -1;
		}/*end switch*/
		if( ret == SOCKET_ERROR ) {
			errno = WSAGetLastError();
			return -1;
		}
		return 0;
	}
	default:
                purple_debug(PURPLE_DEBUG_WARNING, "wpurple", "wpurple_fcntl: Unsupported command\n");
		return -1;
	}/*end switch*/
}

/* sys/ioctl.h */
int wpurple_ioctl(int fd, int command, void* val) {
	switch( command ) {
	case FIONBIO:
	{
		if (ioctlsocket(fd, FIONBIO, (unsigned long *)val) == SOCKET_ERROR) {
			errno = WSAGetLastError();
			return -1;
		}
		return 0;
	}
	case SIOCGIFCONF:
	{
		INTERFACE_INFO InterfaceList[20];
		unsigned long nBytesReturned;
		if (WSAIoctl(fd, SIO_GET_INTERFACE_LIST,
				0, 0, &InterfaceList,
				sizeof(InterfaceList), &nBytesReturned,
				0, 0) == SOCKET_ERROR) {
			errno = WSAGetLastError();
			return -1;
		} else {
			int i;
			struct ifconf *ifc = val;
			char *tmp = ifc->ifc_buf;
			int nNumInterfaces =
				nBytesReturned / sizeof(INTERFACE_INFO);
			for (i = 0; i < nNumInterfaces; i++) {
				INTERFACE_INFO ii = InterfaceList[i];
				struct ifreq *ifr = (struct ifreq *) tmp;
				struct sockaddr_in *sa = (struct sockaddr_in *) &ifr->ifr_addr;

				sa->sin_family = ii.iiAddress.AddressIn.sin_family;
				sa->sin_port = ii.iiAddress.AddressIn.sin_port;
				sa->sin_addr.s_addr = ii.iiAddress.AddressIn.sin_addr.s_addr;
				tmp += sizeof(struct ifreq);

				/* Make sure that we can fit in the original buffer */
				if (tmp >= (ifc->ifc_buf + ifc->ifc_len + sizeof(struct ifreq))) {
					break;
				}
			}
			/* Replace the length with the actually used length */
			ifc->ifc_len = ifc->ifc_len - (ifc->ifc_buf - tmp);
			return 0;
		}
	}
	default:
		errno = EINVAL;
		return -1;
	}/*end switch*/
}

/* netdb.h */
struct hostent* wpurple_gethostbyname(const char *name) {
	struct hostent *hp;

	if((hp = gethostbyname(name)) == NULL) {
		errno = WSAGetLastError();
		return NULL;
	}
	return hp;
}

/* unistd.h */

int wpurple_gethostname(char *name, size_t size) {
        if(gethostname(name, size) == SOCKET_ERROR) {
                errno = WSAGetLastError();
			return -1;
        }
        return 0;
}
