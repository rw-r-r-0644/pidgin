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

#ifndef _FACEBOOK_HTTP_H_
#define _FACEBOOK_HTTP_H_

#include <glib.h>
#include <libpurple/http.h>

#define FB_HTTP_ERROR fb_http_error_quark()

typedef GHashTable FbHttpParams;

GQuark
fb_http_error_quark(void);

gboolean
fb_http_error_chk(PurpleHttpResponse *res, GError **error);

FbHttpParams *
fb_http_params_new(void);

gchar *
fb_http_params_close(FbHttpParams *params, gsize *size);

void
fb_http_params_set_bool(FbHttpParams *params, const gchar *name,
		        gboolean value);

void
fb_http_params_set_dbl(FbHttpParams *params, const gchar *name, gdouble value);

void
fb_http_params_set_int(FbHttpParams *params, const gchar *name, gint64 value);

void
fb_http_params_set_str(FbHttpParams *params, const gchar *name,
                       const gchar *value);

void
fb_http_params_set_strf(FbHttpParams *params, const gchar *name,
                        const gchar *format, ...)
                        G_GNUC_PRINTF(3, 4);

#endif /* _FACEBOOK_HTTP_H_ */
