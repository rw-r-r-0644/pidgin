/*
 * pidgin
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
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#include "glibcompat.h"

#if !GLIB_CHECK_VERSION(2, 62, 0)
gchar *
purple_compat_date_time_format_iso8601(GDateTime *datetime) {
	GString *outstr = NULL;
	gchar *main_date = NULL;
	gint64 offset;

	/* Main date and time. */
	main_date = g_date_time_format(datetime, "%Y-%m-%dT%H:%M:%S");
	outstr = g_string_new(main_date);
	g_free(main_date);

	/* Timezone. Format it as `%:::z` unless the offset is zero, in which case
	* we can simply use `Z`. */
	offset = g_date_time_get_utc_offset(datetime);

	if (offset == 0) {
		g_string_append_c(outstr, 'Z');
	} else {
		gchar *time_zone = g_date_time_format(datetime, "%:::z");
		g_string_append(outstr, time_zone);
		g_free(time_zone);
	}

	return g_string_free(outstr, FALSE);
}
#endif /* !GLIB_CHECK_VERSION(2, 62, 0) */
