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

#ifndef PURPLE_ZEPHYR_ZEPHYR_ZEPH02_H
#define PURPLE_ZEPHYR_ZEPHYR_ZEPH02_H

#include "zephyr_account.h"

gboolean zeph02_login(zephyr_account *zephyr);
gint zeph02_check_notify(gpointer data);
gboolean zeph02_subscribe_to(zephyr_account *zephyr, ZSubscription_t *sub);
gboolean zeph02_request_locations(zephyr_account *zephyr, gchar *who);
gboolean zeph02_send_message(zephyr_account *zephyr, gchar *zclass, gchar *instance, gchar *recipient,
                             const gchar *html_buf, const gchar *sig, const gchar *opcode);
void zeph02_set_location(zephyr_account *zephyr, char *exposure);
void zeph02_get_subs_from_server(zephyr_account *zephyr, PurpleConnection *gc);
void zeph02_close(zephyr_account *zephyr);

#endif /* PURPLE_ZEPHYR_ZEPHYR_ZEPH02_H */
