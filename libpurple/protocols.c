/*
 * purple
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
 *
 */

#include <glib/gi18n-lib.h>

#include "internal.h"
#include "debug.h"
#include "network.h"
#include "notify.h"
#include "purpleaccountoption.h"
#include "purpleconversation.h"
#include "purpleconversationmanager.h"
#include "purplecredentialmanager.h"
#include "purpleprotocol.h"
#include "purpleprotocolmanager.h"
#include "purpleprotocolmedia.h"
#include "purpleprotocolserver.h"
#include "request.h"
#include "server.h"
#include "util.h"

/**************************************************************************
 * GBoxed code for PurpleProtocolChatEntry
 **************************************************************************/

static PurpleProtocolChatEntry *
purple_protocol_chat_entry_copy(PurpleProtocolChatEntry *pce)
{
	PurpleProtocolChatEntry *pce_copy;

	g_return_val_if_fail(pce != NULL, NULL);

	pce_copy  = g_new(PurpleProtocolChatEntry, 1);
	*pce_copy = *pce;

	return pce_copy;
}

GType
purple_protocol_chat_entry_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		type = g_boxed_type_register_static("PurpleProtocolChatEntry",
				(GBoxedCopyFunc)purple_protocol_chat_entry_copy,
				(GBoxedFreeFunc)g_free);
	}

	return type;
}

/**************************************************************************/
/* Protocol API                                                           */
/**************************************************************************/
void
purple_protocol_got_account_idle(PurpleAccount *account, gboolean idle,
						   time_t idle_time)
{
	g_return_if_fail(account != NULL);
	g_return_if_fail(purple_account_is_connected(account));

	purple_presence_set_idle(purple_account_get_presence(account),
						   idle, idle_time);
}

void
purple_protocol_got_account_login_time(PurpleAccount *account, time_t login_time)
{
	PurplePresence *presence;

	g_return_if_fail(account != NULL);
	g_return_if_fail(purple_account_is_connected(account));

	if (login_time == 0)
		login_time = time(NULL);

	presence = purple_account_get_presence(account);

	purple_presence_set_login_time(presence, login_time);
}

void
purple_protocol_got_account_status(PurpleAccount *account, const char *status_id, ...)
{
	PurplePresence *presence;
	PurpleStatus *status;
	va_list args;

	g_return_if_fail(account   != NULL);
	g_return_if_fail(status_id != NULL);
	g_return_if_fail(purple_account_is_connected(account));

	presence = purple_account_get_presence(account);
	status   = purple_presence_get_status(presence, status_id);

	g_return_if_fail(status != NULL);

	va_start(args, status_id);
	purple_status_set_active_with_attrs(status, TRUE, args);
	va_end(args);
}

void
purple_protocol_got_account_actions(PurpleAccount *account)
{

	g_return_if_fail(account != NULL);
	g_return_if_fail(purple_account_is_connected(account));

	purple_signal_emit(purple_accounts_get_handle(), "account-actions-changed",
	                   account);
}

void
purple_protocol_got_user_idle(PurpleAccount *account, const char *name,
		gboolean idle, time_t idle_time)
{
	PurplePresence *presence;
	GSList *list;

	g_return_if_fail(account != NULL);
	g_return_if_fail(name    != NULL);
	g_return_if_fail(purple_account_is_connected(account) || purple_account_is_connecting(account));

	if ((list = purple_blist_find_buddies(account, name)) == NULL)
		return;

	while (list) {
		presence = purple_buddy_get_presence(list->data);
		list = g_slist_delete_link(list, list);
		purple_presence_set_idle(presence, idle, idle_time);
	}
}

void
purple_protocol_got_user_login_time(PurpleAccount *account, const char *name,
		time_t login_time)
{
	GSList *list;
	PurplePresence *presence;

	g_return_if_fail(account != NULL);
	g_return_if_fail(name    != NULL);

	if ((list = purple_blist_find_buddies(account, name)) == NULL)
		return;

	if (login_time == 0)
		login_time = time(NULL);

	while (list) {
		PurpleBuddy *buddy = list->data;
		presence = purple_buddy_get_presence(buddy);
		list = g_slist_delete_link(list, list);

		if (purple_presence_get_login_time(presence) != login_time)
		{
			purple_presence_set_login_time(presence, login_time);

			purple_signal_emit(purple_blist_get_handle(), "buddy-got-login-time", buddy);
		}
	}
}

void
purple_protocol_got_user_status(PurpleAccount *account, const char *name,
		const char *status_id, ...)
{
	GSList *list, *l;
	PurpleBuddy *buddy;
	PurplePresence *presence;
	PurpleStatus *status;
	PurpleStatus *old_status;
	va_list args;

	g_return_if_fail(account   != NULL);
	g_return_if_fail(name      != NULL);
	g_return_if_fail(status_id != NULL);
	g_return_if_fail(purple_account_is_connected(account) || purple_account_is_connecting(account));

	if((list = purple_blist_find_buddies(account, name)) == NULL)
		return;

	for(l = list; l != NULL; l = l->next) {
		buddy = l->data;

		presence = purple_buddy_get_presence(buddy);
		status   = purple_presence_get_status(presence, status_id);

		if(NULL == status)
			/*
			 * TODO: This should never happen, right?  We should call
			 *       g_warning() or something.
			 */
			continue;

		old_status = purple_presence_get_active_status(presence);

		va_start(args, status_id);
		purple_status_set_active_with_attrs(status, TRUE, args);
		va_end(args);

		purple_buddy_update_status(buddy, old_status);
	}

	g_slist_free(list);

	/* The buddy is no longer online, they are therefore by definition not
	 * still typing to us. */
	if (!purple_status_is_online(status)) {
		purple_serv_got_typing_stopped(purple_account_get_connection(account), name);
		purple_protocol_got_media_caps(account, name);
	}
}

void purple_protocol_got_user_status_deactive(PurpleAccount *account, const char *name,
					const char *status_id)
{
	GSList *list, *l;
	PurpleBuddy *buddy;
	PurplePresence *presence;
	PurpleStatus *status;

	g_return_if_fail(account   != NULL);
	g_return_if_fail(name      != NULL);
	g_return_if_fail(status_id != NULL);
	g_return_if_fail(purple_account_is_connected(account) || purple_account_is_connecting(account));

	if((list = purple_blist_find_buddies(account, name)) == NULL)
		return;

	for(l = list; l != NULL; l = l->next) {
		buddy = l->data;

		presence = purple_buddy_get_presence(buddy);
		status   = purple_presence_get_status(presence, status_id);

		if(NULL == status)
			continue;

		if (purple_status_is_active(status)) {
			purple_status_set_active(status, FALSE);
			purple_buddy_update_status(buddy, status);
		}
	}

	g_slist_free(list);
}

static void
do_protocol_change_account_status(PurpleAccount *account,
								PurpleStatus *old_status, PurpleStatus *new_status)
{
	PurpleProtocol *protocol;
	PurpleProtocolManager *manager;

	if (purple_status_is_online(new_status) &&
		purple_account_is_disconnected(account) &&
		purple_network_is_available())
	{
		purple_account_connect(account);
		return;
	}

	if (!purple_status_is_online(new_status))
	{
		if (!purple_account_is_disconnected(account))
			purple_account_disconnect(account);
		/* Clear out the unsaved password if we switch to offline status */
		if (!purple_account_get_remember_password(account)) {
			PurpleCredentialManager *manager = NULL;

			manager = purple_credential_manager_get_default();

			purple_credential_manager_clear_password_async(manager, account,
			                                               NULL, NULL, NULL);
		}

		return;
	}

	if (purple_account_is_connecting(account))
		/*
		 * We don't need to call the set_status protocol function because
		 * the protocol will take care of setting its status during the
		 * connection process.
		 */
		return;

	manager = purple_protocol_manager_get_default();
	protocol = purple_protocol_manager_find(manager,
	                                        purple_account_get_protocol_id(account));

	if (protocol == NULL)
		return;

	if(!purple_account_is_disconnected(account)) {
		purple_protocol_server_set_status(PURPLE_PROTOCOL_SERVER(protocol),
		                                  account, new_status);
	}
}

void
purple_protocol_change_account_status(PurpleAccount *account,
								PurpleStatus *old_status, PurpleStatus *new_status)
{
	g_return_if_fail(account    != NULL);
	g_return_if_fail(new_status != NULL);
	g_return_if_fail(!purple_status_is_exclusive(new_status) || old_status != NULL);

	purple_signal_emit(purple_accounts_get_handle(), "account-status-changing",
					account, old_status, new_status);

	do_protocol_change_account_status(account, old_status, new_status);

	purple_signal_emit(purple_accounts_get_handle(), "account-status-changed",
					account, old_status, new_status);
}

GList *
purple_protocol_get_statuses(PurpleAccount *account, PurplePresence *presence)
{
	g_return_val_if_fail(account  != NULL, NULL);
	g_return_val_if_fail(presence != NULL, NULL);

	return g_list_copy_deep(purple_account_get_status_types(account),
	                        (GCopyFunc)purple_status_new, presence);
}

gboolean
purple_protocol_initiate_media(PurpleAccount *account,
			   const char *who,
			   PurpleMediaSessionType type)
{
#ifdef USE_VV
	PurpleConnection *gc = NULL;
	PurpleProtocol *protocol = NULL;

	if(account) {
		gc = purple_account_get_connection(account);
	}
	if(gc) {
		protocol = purple_connection_get_protocol(gc);
	}

	if(PURPLE_IS_PROTOCOL_MEDIA(protocol)) {
		PurpleProtocolMedia *media = PURPLE_PROTOCOL_MEDIA(protocol);

		return purple_protocol_media_initiate_session(media, account, who,
		                                              type);
	} else {
		return FALSE;
	}
#else
	return FALSE;
#endif
}

PurpleMediaCaps
purple_protocol_get_media_caps(PurpleAccount *account, const char *who)
{
#ifdef USE_VV
	PurpleConnection *gc = NULL;
	PurpleProtocol *protocol = NULL;

	if(account) {
		gc = purple_account_get_connection(account);
	}
	if(gc) {
		protocol = purple_connection_get_protocol(gc);
	}

	if(PURPLE_IS_PROTOCOL_MEDIA(protocol)) {
		return purple_protocol_media_get_caps(PURPLE_PROTOCOL_MEDIA(protocol),
		                                      account, who);
	} else {
		return PURPLE_MEDIA_CAPS_NONE;
	}
#else
	return PURPLE_MEDIA_CAPS_NONE;
#endif
}

void
purple_protocol_got_media_caps(PurpleAccount *account, const char *name)
{
#ifdef USE_VV
	GSList *list;

	g_return_if_fail(account != NULL);
	g_return_if_fail(name    != NULL);

	if ((list = purple_blist_find_buddies(account, name)) == NULL)
		return;

	while (list) {
		PurpleBuddy *buddy = list->data;
		PurpleMediaCaps oldcaps = purple_buddy_get_media_caps(buddy);
		PurpleMediaCaps newcaps = 0;
		const gchar *bname = purple_buddy_get_name(buddy);
		list = g_slist_delete_link(list, list);


		newcaps = purple_protocol_get_media_caps(account, bname);
		purple_buddy_set_media_caps(buddy, newcaps);

		if (oldcaps == newcaps)
			continue;

		purple_signal_emit(purple_blist_get_handle(),
				"buddy-caps-changed", buddy,
				newcaps, oldcaps);
	}
#endif
}

