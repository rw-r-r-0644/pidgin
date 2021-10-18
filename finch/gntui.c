/*
 * finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
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

#include <config.h>

#include <glib/gi18n-lib.h>

#include <purple.h>

#include "gntui.h"

#include "gntaccount.h"
#include "gntblist.h"
#include "gntconn.h"
#include "gntconv.h"
#include "gntdebug.h"
#include "gntxfer.h"
#include "gntmedia.h"
#include "gntnotify.h"
#include "gntplugin.h"
#include "gntprefs.h"
#include "gntrequest.h"
#include "gntroomlist.h"
#include "gntstatus.h"

static gboolean
finch_history_init(GError **error) {
	PurpleHistoryManager *manager = NULL;
	PurpleHistoryAdapter *adapter = NULL;
	gchar *filename = NULL;
	const gchar *id = NULL;

	manager = purple_history_manager_get_default();

	/* Attempt to create the config directory. */
	g_mkdir_with_parents(purple_config_dir(), 0700);

	filename = g_build_filename(purple_config_dir(), "history.db", NULL);
	adapter = purple_sqlite_history_adapter_new(filename);
	g_free(filename);

	id = purple_history_adapter_get_id(adapter);
	if(!purple_history_manager_register(manager, adapter, error)) {
		g_clear_object(&adapter);

		return FALSE;
	}

	/* The manager adds a ref to the adapter on registration, so we can remove
	 * our reference.
	 */
	g_clear_object(&adapter);

	return purple_history_manager_set_active(manager, id, error);
}

void finch_ui_init()
{
	GError *error = NULL;

#ifdef STANDALONE
#ifdef _WIN32 /* TODO: don't change it when using FHS under win32 */
	gnt_set_config_dir(purple_user_dir());
#endif /* _WIN32 */

	gnt_init();
#endif /* STANDALONE */

	if(!finch_history_init(&error)) {
		g_critical("failed to initialize the history api: %s",
		           error != NULL ? error->message : "unknown");
		g_clear_error(&error);
	}

	purple_prefs_add_none("/purple/gnt");

	/* Accounts */
	finch_accounts_init();
	purple_accounts_set_ui_ops(finch_accounts_get_ui_ops());

	/* Connections */
	finch_connections_init();
	purple_connections_set_ui_ops(finch_connections_get_ui_ops());

	/* Initialize the buddy list */
	finch_blist_init();
	purple_blist_set_ui(FINCH_TYPE_BUDDY_LIST);

	/* Now the conversations */
	finch_conversation_init();
	purple_conversations_set_ui_ops(finch_conv_get_ui_ops());

	/* Notify */
	finch_notify_init();
	purple_notify_set_ui_ops(finch_notify_get_ui_ops());

	/* Request */
	finch_request_init();
	purple_request_set_ui_ops(finch_request_get_ui_ops());

	/* File transfer */
	finch_xfers_init();
	purple_xfers_set_ui_ops(finch_xfers_get_ui_ops());

	/* Roomlist */
	finch_roomlist_init();
	purple_roomlist_set_ui_ops(finch_roomlist_get_ui_ops());

	/* Media */
	finch_media_manager_init();

	gnt_register_action(_("Accounts"), finch_accounts_show_all);
	gnt_register_action(_("Buddy List"), finch_blist_show);
	gnt_register_action(_("Debug Window"), finch_debug_window_show);
	gnt_register_action(_("File Transfers"), finch_xfer_dialog_show);
	gnt_register_action(_("Plugins"), finch_plugins_show_all);
	gnt_register_action(_("Room List"), finch_roomlist_show_all);
	gnt_register_action(_("Preferences"), finch_prefs_show_all);
	gnt_register_action(_("Statuses"), finch_savedstatus_show_all);

#ifdef STANDALONE
}

void finch_ui_uninit()
{
	purple_accounts_set_ui_ops(NULL);
	finch_accounts_uninit();

	purple_connections_set_ui_ops(NULL);
	finch_connections_uninit();

	purple_blist_set_ui(G_TYPE_INVALID);
	finch_blist_uninit();

	purple_conversations_set_ui_ops(NULL);
	finch_conversation_uninit();

	purple_notify_set_ui_ops(NULL);
	finch_notify_uninit();

	purple_request_set_ui_ops(NULL);
	finch_request_uninit();

	finch_xfers_uninit();
	purple_xfers_set_ui_ops(NULL);

	finch_roomlist_uninit();
	purple_roomlist_set_ui_ops(NULL);

#ifdef USE_VV
	finch_media_manager_uninit();
#endif

	gnt_quit();

#ifdef _WIN32
	gnt_set_config_dir(NULL);
#endif /* _WIN32 */
#endif /* STANDALONE */
}
