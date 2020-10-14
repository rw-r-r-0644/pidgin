/*
 * purple
 *
 * File: libc_internal.h
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
#ifndef PURPLE_WIN32_LIBC_INTERNAL
#define PURPLE_WIN32_LIBC_INTERNAL
#include <glib.h>

G_BEGIN_DECLS

/* sys/socket.h */
int wpurple_socket(int domain, int style, int protocol);
int wpurple_connect(int socket, struct sockaddr *addr, u_long length);
int wpurple_getsockopt(int socket, int level, int optname, void *optval, socklen_t *optlenptr);
int wpurple_setsockopt(int socket, int level, int optname, const void *optval, socklen_t optlen);
int wpurple_getsockname (int socket, struct sockaddr *addr, socklen_t *lenptr);
int wpurple_bind(int socket, struct sockaddr *addr, socklen_t length);
int wpurple_listen(int socket, unsigned int n);
int wpurple_sendto(int socket, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen);
int wpurple_recv(int fd, void *buf, size_t len, int flags);
int wpurple_send(int fd, const void *buf, unsigned int size, int flags);

/* arpa/inet.h */
int wpurple_inet_aton(const char *name, struct in_addr *addr);
const char *
wpurple_inet_ntop (int af, const void *src, char *dst, socklen_t cnt);
int wpurple_inet_pton(int af, const char *src, void *dst);

/* netdb.h */
struct hostent* wpurple_gethostbyname(const char *name);

/* fcntl.h */
int wpurple_fcntl(int socket, int command, ...);
#define F_GETFL 3
#define F_SETFL 4
#define O_NONBLOCK 04000

/* sys/ioctl.h */
#define SIOCGIFCONF 0x8912 /* get iface list */
int wpurple_ioctl(int fd, int command, void* opt);

/* net/if.h */
struct ifreq
{
	union
	{
		char ifrn_name[6];	/* Interface name, e.g. "en0".  */
	} ifr_ifrn;

	union
	{
		struct sockaddr ifru_addr;
		char *ifru_data;
	} ifr_ifru;
};
# define ifr_name	ifr_ifrn.ifrn_name	/* interface name       */
# define ifr_addr	ifr_ifru.ifru_addr      /* address              */
# define ifr_data	ifr_ifru.ifru_data	/* for use by interface */

struct ifconf
{
	int ifc_len;			/* Size of buffer.  */
	union
	{
		char *ifcu_buf;
		struct ifreq *ifcu_req;
	} ifc_ifcu;
};
# define ifc_buf ifc_ifcu.ifcu_buf /* Buffer address.  */
# define ifc_req ifc_ifcu.ifcu_req /* Array of structures.  */

/* sys/time.h */
#if __MINGW32_MAJOR_VERSION < 3 || (__MINGW32_MAJOR_VERSION == 3 && __MINGW32_MINOR_VERSION < 10)
struct timezone {
	int tz_minuteswest;
	int tz_dsttime;
};
#else
#    include <sys/time.h>
#endif
int wpurple_gettimeofday(struct timeval *p, struct timezone *z);

/* unistd.h */
int wpurple_read(int fd, void *buf, unsigned int size);
int wpurple_write(int fd, const void *buf, unsigned int size);
int wpurple_close(int fd);
int wpurple_gethostname(char *name, size_t size);

G_END_DECLS

#endif /* PURPLE_WIN32_LIBC_INTERNAL */
