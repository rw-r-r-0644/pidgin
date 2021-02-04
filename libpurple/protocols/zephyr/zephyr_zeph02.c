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

#include <glib/gi18n-lib.h>

#include "zephyr_zeph02.h"

gboolean
zeph02_login(zephyr_account *zephyr)
{
	PurpleConnection *pc = purple_account_get_connection(zephyr->account);

	/* XXX Should actually try to report the com_err determined error */
	if (ZInitialize() != ZERR_NONE) {
		purple_connection_error(pc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
		                        "Couldn't initialize zephyr");
		return FALSE;
	}

	if (ZOpenPort(&(zephyr->port)) != ZERR_NONE) {
		purple_connection_error(pc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
		                        "Couldn't open port");
		return FALSE;
	}

	if (ZSetLocation(zephyr->exposure) != ZERR_NONE) {
		purple_connection_error(pc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
		                        "Couldn't set location");
		return FALSE;
	}

	zephyr->username = g_strdup(ZGetSender());
	zephyr->realm = get_zephyr_realm(zephyr->account, ZGetRealm());

	return TRUE;
}

/* Called when the server notifies us a message couldn't get sent */
static void
message_failed(PurpleConnection *gc, ZNotice_t *notice)
{
	const gchar *title;
	gchar *notify_msg;

	if (g_ascii_strcasecmp(notice->z_class, "message")) {
		title = "";
		notify_msg = g_strdup_printf(_("Unable to send to chat %s,%s,%s"),
		                             notice->z_class, notice->z_class_inst, notice->z_recipient);
	} else {
		title = notice->z_recipient;
		notify_msg = g_strdup(_("User is offline"));
	}
	purple_notify_error(gc, title, notify_msg, NULL, purple_request_cpar_from_connection(gc));
	g_free(notify_msg);
}

/* just for debugging */
static void
handle_unknown(ZNotice_t *notice)
{
	purple_debug_error("zephyr", "z_packet: %s\n", notice->z_packet);
	purple_debug_error("zephyr", "z_version: %s\n", notice->z_version);
	purple_debug_error("zephyr", "z_kind: %d\n", (int)(notice->z_kind));
	purple_debug_error("zephyr", "z_class: %s\n", notice->z_class);
	purple_debug_error("zephyr", "z_class_inst: %s\n", notice->z_class_inst);
	purple_debug_error("zephyr", "z_opcode: %s\n", notice->z_opcode);
	purple_debug_error("zephyr", "z_sender: %s\n", notice->z_sender);
	purple_debug_error("zephyr", "z_recipient: %s\n", notice->z_recipient);
	purple_debug_error("zephyr", "z_message: %s\n", notice->z_message);
	purple_debug_error("zephyr", "z_message_len: %d\n", notice->z_message_len);
}

gint
zeph02_check_notify(gpointer data)
{
	/* XXX add real error reporting */
	PurpleConnection *gc = (PurpleConnection*) data;
	while (ZPending()) {
		ZNotice_t notice;
		/* XXX add real error reporting */

		if (ZReceiveNotice(&notice, NULL) != ZERR_NONE) {
			return TRUE;
		}

		switch (notice.z_kind) {
		case UNSAFE:
		case UNACKED:
		case ACKED:
			handle_message(gc, &notice);
			break;
		case SERVACK:
			if (!(g_ascii_strcasecmp(notice.z_message, ZSRVACK_NOTSENT))) {
				message_failed(gc, &notice);
			}
			break;
		case CLIENTACK:
			purple_debug_error("zephyr", "Client ack received\n");
			handle_unknown(&notice); /* XXX: is it really unknown? */
			break;
		default:
			/* we'll just ignore things for now */
			handle_unknown(&notice);
			purple_debug_error("zephyr", "Unhandled notice.\n");
			break;
		}
		/* XXX add real error reporting */
		ZFreeNotice(&notice);
	}

	return TRUE;
}

gboolean
zeph02_subscribe_to(G_GNUC_UNUSED zephyr_account *zephyr, ZSubscription_t *sub)
{
	return ZSubscribeTo(sub, 1, 0) == ZERR_NONE;
}

gboolean
zeph02_request_locations(G_GNUC_UNUSED zephyr_account *zephyr, gchar *who)
{
	ZAsyncLocateData_t ald;
	Code_t zerr;

	zerr = ZRequestLocations(who, &ald, UNACKED, ZAUTH);
	g_free(ald.user);
	g_free(ald.version);
	return zerr == ZERR_NONE;
}

gboolean
zeph02_send_message(G_GNUC_UNUSED zephyr_account *zephyr, gchar *zclass, gchar *instance, gchar *recipient,
                    const gchar *html_buf, const gchar *sig, const gchar *opcode)
{
	ZNotice_t notice;
	char *buf = g_strdup_printf("%s%c%s", sig, '\0', html_buf);
	gboolean result = TRUE;

	memset((char *)&notice, 0, sizeof(notice));
	notice.z_kind = ACKED;
	notice.z_port = 0;
	notice.z_class = zclass;
	notice.z_class_inst = instance;
	notice.z_recipient = recipient;
	notice.z_sender = 0;
	notice.z_default_format = "Class $class, Instance $instance:\n" "To: @bold($recipient) at $time $date\n" "From: @bold($1) <$sender>\n\n$2";
	notice.z_message_len = strlen(sig) + 1 + strlen(html_buf) + 1;
	notice.z_message = buf;
	notice.z_opcode = g_strdup(opcode);

	purple_debug_info("zephyr","About to send notice\n");
	if (ZSendNotice(&notice, ZAUTH) != ZERR_NONE) {
		/* XXX handle errors here */
		result = FALSE;
	} else {
		purple_debug_info("zephyr", "notice sent\n");
	}
	g_free(buf);
	return result;
}

void
zeph02_set_location(G_GNUC_UNUSED zephyr_account *zephyr, char *exposure)
{
	ZSetLocation(exposure);
}

void
zeph02_get_subs_from_server(zephyr_account *zephyr, PurpleConnection *gc)
{
	Code_t retval;
	int nsubs;
	GString *subout;

	if (zephyr->port == 0) {
		purple_debug_error("zephyr", "error while retrieving port\n");
		return;
	}
	if ((retval = ZRetrieveSubscriptions(zephyr->port, &nsubs)) != ZERR_NONE) {
		/* XXX better error handling */
		purple_debug_error("zephyr", "error while retrieving subscriptions from server\n");
		return;
	}

	subout = g_string_new("Subscription list<br>");
	for (int i = 0; i < nsubs; i++) {
		ZSubscription_t subs;
		int one = 1;

		if ((retval = ZGetSubscriptions(&subs, &one)) != ZERR_NONE) {
			break;
		}
		g_string_append_printf(subout, "Class %s Instance %s Recipient %s<br>",
		                       subs.zsub_class, subs.zsub_classinst, subs.zsub_recipient);
	}

	if (retval == ZERR_NONE) {
		gchar *title = g_strdup_printf("Server subscriptions for %s", zephyr->username);
		purple_notify_formatted(gc, title, title, NULL, subout->str, NULL, NULL);
		g_free(title);
	} else {
		/* XXX better error handling */
		purple_debug_error("zephyr", "error while retrieving individual subscription\n");
	}

	g_string_free(subout, TRUE);
}

void
zeph02_close(G_GNUC_UNUSED zephyr_account *zephyr)
{
	if (ZCancelSubscriptions(0) != ZERR_NONE) {
		return;
	}
	if (ZUnsetLocation() != ZERR_NONE) {
		return;
	}
	if (ZClosePort() != ZERR_NONE) {
		return;
	}
}
