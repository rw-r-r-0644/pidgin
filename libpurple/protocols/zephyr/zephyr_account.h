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

#ifndef PURPLE_ZEPHYR_ZEPHYR_ACCOUNT_H
#define PURPLE_ZEPHYR_ZEPHYR_ACCOUNT_H

#include <glib.h>

#include <purple.h>

#include "internal.h"

#ifndef HOST_NAME_MAX
# define HOST_NAME_MAX 255
#endif

typedef struct _zephyr_account zephyr_account;

typedef enum {
	PURPLE_ZEPHYR_NONE, /* Non-kerberized ZEPH0.2 */
	PURPLE_ZEPHYR_KRB4, /* ZEPH0.2 w/ KRB4 support */
	PURPLE_ZEPHYR_TZC,  /* tzc executable proxy */
	PURPLE_ZEPHYR_INTERGALACTIC_KRB4 /* Kerberized ZEPH0.3 */
} zephyr_connection_type;

typedef gboolean (*ZephyrSubscribeToFunc)(zephyr_account *zephyr, ZSubscription_t *sub);
typedef gboolean (*ZephyrRequestLocationsFunc)(zephyr_account *zephyr, gchar *who);
typedef gboolean (*ZephyrSendMessageFunc)(zephyr_account *zephyr, gchar *zclass, gchar *instance, gchar *recipient,
                                          const gchar *html_buf, const gchar *sig, const gchar *opcode);
typedef void (*ZephyrSetLocationFunc)(zephyr_account *zephyr, char *exposure);
typedef void (*ZephyrGetSubsFromServerFunc)(zephyr_account *zephyr, PurpleConnection *gc);
typedef void (*ZephyrCloseFunc)(zephyr_account *zephyr);

struct _zephyr_account {
	PurpleAccount* account;
	char *username;
	char *realm;
	char *encoding;
	char* galaxy; /* not yet useful */
	char* krbtkfile; /* not yet useful */
	guint32 nottimer;
	guint32 loctimer;
	GList *pending_zloc_names;
	GSList *subscrips;
	int last_id;
	unsigned short port;
	char ourhost[HOST_NAME_MAX + 1];
	char ourhostcanon[HOST_NAME_MAX + 1];
	zephyr_connection_type connection_type;
	char *exposure;
	GSubprocess *tzc_proc;
	GOutputStream *tzc_stdin;
	GInputStream *tzc_stdout;
	gchar *away;
	ZephyrSubscribeToFunc subscribe_to;
	ZephyrRequestLocationsFunc request_locations;
	ZephyrSendMessageFunc send_message;
	ZephyrSetLocationFunc set_location;
	ZephyrGetSubsFromServerFunc get_subs_from_server;
	ZephyrCloseFunc close;
};

gchar *get_zephyr_realm(PurpleAccount *account, const gchar *local_realm);
void handle_message(PurpleConnection *gc, ZNotice_t *notice);
void handle_locations(PurpleConnection *gc, const gchar *user, int nlocs, const ZLocations_t *zloc);
char *zephyr_normalize_local_realm(const zephyr_account *zephyr, const char *orig);

#endif /* PURPLE_ZEPHYR_ZEPHYR_ACCOUNT_H */
