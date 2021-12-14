/*
 * Purple - Internet Messaging Library
 * Copyright (C) Pidgin Developers <devel@pidgin.im>
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
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#ifndef PURPLE_SOUPCOMPAT_H
#define PURPLE_SOUPCOMPAT_H
/*
 * This file is internal to libpurple. Do not use!
 * Also, any public API should not depend on this file.
 */

#include <libsoup/soup.h>

#if SOUP_MAJOR_VERSION < 3

static inline const char *
soup_message_get_reason_phrase(SoupMessage *msg) {
	return msg->reason_phrase;
}

static inline SoupMessageHeaders *
soup_message_get_response_headers(SoupMessage *msg) {
	return msg->response_headers;
}

static inline SoupMessageHeaders *
soup_message_get_request_headers(SoupMessage *msg) {
	return msg->request_headers;
}

static inline SoupStatus
soup_message_get_status(SoupMessage *msg) {
	return msg->status_code;
}

static inline SoupMessage *
soup_message_new_from_encoded_form(const gchar *method,
                                   const gchar *uri_string,
                                   gchar *encoded_form)
{
	SoupMessage *msg = NULL;
	SoupURI *uri;

	g_return_val_if_fail(method != NULL, NULL);
	g_return_val_if_fail(uri_string != NULL, NULL);
	g_return_val_if_fail(encoded_form != NULL, NULL);

	uri = soup_uri_new(uri_string);
	if (!uri || !uri->host) {
		g_free(encoded_form);
		soup_uri_free(uri);
		return NULL;
	}

	if (strcmp(method, "GET") == 0) {
		g_free(uri->query);
		uri->query = encoded_form;
		msg = soup_message_new_from_uri(method, uri);
	} else if (strcmp (method, "POST") == 0 || strcmp (method, "PUT") == 0) {
		msg = soup_message_new_from_uri(method, uri);
		soup_message_body_append_take(msg->request_body,
		                              (guchar *)encoded_form,
		                              strlen(encoded_form));
	} else {
		g_free(encoded_form);
	}

	soup_uri_free(uri);

	return msg;
}

#endif /* SOUP_MAJOR_VERSION < 3 */

#endif /* PURPLE_SOUPCOMPAT_H */
