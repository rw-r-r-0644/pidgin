/*
 * Pidgin - Internet Messenger
 * Copyright (C) Pidgin Developers <devel@pidgin.im>
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

#include <glib/gi18n.h>

#include <gtk/gtk.h>

#include "pidginaccountsenabledmenu.h"

#include "pidgincore.h"

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
pidgin_accounts_enabled_menu_refresh_helper(PurpleAccount *account,
                                            gpointer data)
{
	GApplication *application = g_application_get_default();
	GMenu *menu = data;

	if(purple_account_get_enabled(account)) {
		PurpleConnection *connection = purple_account_get_connection(account);
		GMenu *submenu = NULL;
		gchar *label = NULL;
		const gchar *account_name = purple_account_get_username(account);
		const gchar *protocol_name = purple_account_get_protocol_name(account);
		const gchar *account_id = purple_account_get_id(account);
		const gchar *connection_id = NULL;

		submenu = gtk_application_get_menu_by_id(GTK_APPLICATION(application),
		                                         "enabled-account");

		if(PURPLE_IS_CONNECTION(connection)) {
			connection_id = purple_connection_get_id(connection);
		}

		purple_menu_populate_dynamic_targets(submenu,
		                                     "account", account_id,
		                                     "connection", connection_id,
		                                     NULL);

		/* translators: This format string is intended to contain the account
		 * name followed by the protocol name to uniquely identify a specific
		 * account.
		 */
		label = g_strdup_printf(_("%s (%s)"), account_name, protocol_name);

		g_menu_append_submenu(menu, label, G_MENU_MODEL(submenu));

		g_free(label);
	}
}

static void
pidgin_accounts_enabled_menu_refresh(GMenu *menu) {
	PurpleAccountManager *manager = NULL;

	g_menu_remove_all(menu);

	manager = purple_account_manager_get_default();
	purple_account_manager_foreach(manager,
	                               pidgin_accounts_enabled_menu_refresh_helper,
	                               menu);
}

/******************************************************************************
 * Callbacks
 *****************************************************************************/
static void
pidgin_accounts_enabled_menu_enabled_cb(G_GNUC_UNUSED PurpleAccount *account,
                                        gpointer data)
{
	pidgin_accounts_enabled_menu_refresh(data);
}

static void
pidgin_accounts_enabled_menu_disabled_cb(G_GNUC_UNUSED PurpleAccount *account,
                                         gpointer data)
{
	pidgin_accounts_enabled_menu_refresh(data);
}

static void
pidgin_accounts_enabled_menu_connected_cb(G_GNUC_UNUSED PurpleAccount *account,
                                          gpointer data)
{
	pidgin_accounts_enabled_menu_refresh(data);
}

static void
pidgin_accounts_enabled_menu_disconnected_cb(G_GNUC_UNUSED PurpleAccount *account,
                                             gpointer data)
{
	pidgin_accounts_enabled_menu_refresh(data);
}

static void
pidgin_accounts_enabled_menu_weak_notify_cb(G_GNUC_UNUSED gpointer data,
                                            GObject *obj)
{
	purple_signals_disconnect_by_handle(obj);
}

/******************************************************************************
 * Public API
 *****************************************************************************/
GMenu *
pidgin_accounts_enabled_menu_new(void) {
	GMenu *menu = NULL;
	gpointer handle = NULL;

	/* Create the menu and set our instance as data on it so it'll be freed
	 * when the menu is destroyed.
	 */
	menu = g_menu_new();
	g_object_weak_ref(G_OBJECT(menu),
	                  pidgin_accounts_enabled_menu_weak_notify_cb, NULL);

	/* Populate ourselves with any accounts that are already enabled. */
	pidgin_accounts_enabled_menu_refresh(menu);

	/* Wire up the purple signals we care about. */
	handle = purple_accounts_get_handle();
	purple_signal_connect(handle, "account-enabled", menu,
	                      G_CALLBACK(pidgin_accounts_enabled_menu_enabled_cb),
	                      menu);
	purple_signal_connect(handle, "account-disabled", menu,
	                      G_CALLBACK(pidgin_accounts_enabled_menu_disabled_cb),
	                      menu);

	/* For the account actions, we also need to know when an account is online
	 * or offline.
	 */
	purple_signal_connect(handle, "account-signed-on", menu,
	                      G_CALLBACK(pidgin_accounts_enabled_menu_connected_cb),
	                      menu);
	purple_signal_connect(handle, "account-signed-off", menu,
	                      G_CALLBACK(pidgin_accounts_enabled_menu_disconnected_cb),
	                      menu);

	return menu;
}
