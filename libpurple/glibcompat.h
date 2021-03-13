/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#ifndef PURPLE_GLIBCOMPAT_H
#define PURPLE_GLIBCOMPAT_H
/*
 * SECTION:glibcompat
 * @section_id: libpurple-glibcompat
 * @short_description: <filename>glibcompat.h</filename>
 * @title: GLib version-dependent definitions
 *
 * This file is internal to libpurple. Do not use!
 * Also, any public API should not depend on this file.
 */

#include <glib.h>

/* glib's definition of g_stat+GStatBuf seems to be broken on mingw64-w32 (and
 * possibly other 32-bit windows), so instead of relying on it,
 * we'll define our own.
 */
#if defined(_WIN32) && !defined(_MSC_VER) && !defined(_WIN64)
#  include <glib/gstdio.h>
typedef struct _stat GStatBufW32;
static inline int
purple_g_stat(const gchar *filename, GStatBufW32 *buf)
{
	return g_stat(filename, (GStatBuf*)buf);
}
#  define GStatBuf GStatBufW32
#  define g_stat purple_g_stat
#endif

#if !GLIB_CHECK_VERSION(2, 58, 0)
#define G_SOURCE_FUNC(f) ((GSourceFunc) (void (*)(void)) (f))

static inline GTimeZone *
g_time_zone_new_offset(gint32 seconds) {
	GTimeZone *tz = NULL;
	gchar *identifier = NULL;

	/* Seemingly, we should be using @seconds directly to set the
	 * #TransitionInfo.gmt_offset to avoid all this string building and
	 * parsing. However, we always need to set the #GTimeZone.name to a
	 * constructed string anyway, so we might as well reuse its code.
	 */
	identifier = g_strdup_printf("%c%02u:%02u:%02u",
	                             (seconds >= 0) ? '+' : '-',
	                             (ABS (seconds) / 60) / 60,
	                             (ABS (seconds) / 60) % 60,
	                             ABS (seconds) % 60);
	tz = g_time_zone_new(identifier);
	g_free(identifier);

	g_assert(g_time_zone_get_offset(tz, 0) == seconds);

	return tz;
}

#endif /* !GLIB_CHECK_VERSION(2, 58, 0) */

#if !GLIB_CHECK_VERSION(2, 62, 0)
#define g_date_time_format_iso8601(dt) (purple_compat_date_time_format_iso8601((dt)))
gchar *purple_compat_date_time_format_iso8601(GDateTime *datetime);
#endif /* GLIB_CHECK_VERSION(2, 62, 0) */

/* Backport the static inline version of g_memdup2 if we don't have g_memdup2.
 * see https://mail.gnome.org/archives/desktop-devel-list/2021-February/msg00000.html
 * for more information.
 */
#if !GLIB_CHECK_VERSION(2, 67, 3)
static inline gpointer
g_memdup2(gconstpointer mem, gsize byte_size) {
	gpointer new_mem = NULL;

	if(mem && byte_size != 0) {
		new_mem = g_malloc (byte_size);
		memcpy (new_mem, mem, byte_size);
	}

	return new_mem;
}
#endif /* !GLIB_CHECK_VERSION(2, 67, 3) */

#endif /* PURPLE_GLIBCOMPAT_H */
