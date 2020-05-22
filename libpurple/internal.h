/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
 */

#ifndef PURPLE_INTERNAL_H
#define PURPLE_INTERNAL_H
/*
 * SECTION:internal
 * @section_id: libpurple-internal
 * @short_description: <filename>internal.h</filename>
 * @title: Internal definitions and includes
 */

#if !defined(PURPLE_COMPILATION)
#error "internal.h included outside of libpurple"
#endif

#ifndef GLIB_VERSION_MIN_REQUIRED
#define GLIB_VERSION_MIN_REQUIRED (GLIB_VERSION_2_28)
#endif

#ifdef HAVE_CONFIG_H
# ifdef GETTEXT_PACKAGE
#  undef GETTEXT_PACKAGE
# endif
# include <config.h>
#endif

#ifdef HAVE_ENDIAN_H
# include <endian.h>
#endif

#define MSG_LEN 2048
/* The above should normally be the same as BUF_LEN,
 * but just so we're explicitly asking for the max message
 * length. */
#define BUF_LEN MSG_LEN
#define BUF_LONG BUF_LEN * 2

#include <sys/types.h>

#ifndef _WIN32
# include <sys/time.h>
# include <sys/wait.h>
# include <sys/time.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef HAVE_ICONV
# include <iconv.h>
#endif

#ifdef HAVE_LANGINFO_CODESET
# include <langinfo.h>
#endif

#ifndef _WIN32
# include <netinet/in.h>
# include <sys/socket.h>
# include <arpa/inet.h>
# include <sys/un.h>
# include <sys/utsname.h>
# include <netdb.h>
# include <signal.h>
# include <unistd.h>
#endif

#ifndef HOST_NAME_MAX
# define HOST_NAME_MAX 255
#endif

#ifdef _WIN32
# include "win32/win32dep.h"
#endif

#ifdef HAVE_CONFIG_H
#if SIZEOF_TIME_T == 4
#	define PURPLE_TIME_T_MODIFIER "lu"
#elif SIZEOF_TIME_T == 8
#	define PURPLE_TIME_T_MODIFIER "zu"
#else
#error Unknown size of time_t
#endif
#endif

#ifdef __clang__

#define PURPLE_BEGIN_IGNORE_CAST_ALIGN \
	_Pragma ("clang diagnostic push") \
	_Pragma ("clang diagnostic ignored \"-Wcast-align\"")

#define PURPLE_END_IGNORE_CAST_ALIGN \
	_Pragma ("clang diagnostic pop")

#else

#define PURPLE_BEGIN_IGNORE_CAST_ALIGN
#define PURPLE_END_IGNORE_CAST_ALIGN

#endif /* __clang__ */

#include <glib-object.h>

#ifdef __COVERITY__

/* avoid TAINTED_SCALAR warning */
#undef g_utf8_next_char
#define g_utf8_next_char(p) (char *)((p) + 1)

#endif

#endif /* PURPLE_INTERNAL_H */
