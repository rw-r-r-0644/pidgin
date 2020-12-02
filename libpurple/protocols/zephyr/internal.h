#ifndef PURPLE_ZEPHYR_INTERNAL_H
#define PURPLE_ZEPHYR_INTERNAL_H

#include <sysdep.h>

#ifdef LIBZEPHYR_EXT
#include <zephyr/zephyr.h>
#else
#include <zephyr_internal.h>
#endif

#ifdef WIN32

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 512
#endif

#define ETIMEDOUT WSAETIMEDOUT
#define EADDRINUSE WSAEADDRINUSE
#else /* !WIN32 */
#include <netdb.h>

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 4096
#endif

#endif

#ifdef ZEPHYR_USES_HESIOD
#include <hesiod.h>
#endif

#ifndef ZEPHYR_USES_KERBEROS
#define REALM_SZ	MAXHOSTNAMELEN
#define INST_SZ		0		/* no instances w/o Kerberos */
#define ANAME_SZ	9		/* size of a username + null */
#define CLOCK_SKEW	300		/* max time to cache packet ids */
#endif

#endif /* PURPLE_ZEPHYR_INTERNAL_H */
