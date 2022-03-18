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

#include "pidginaccountsenabledmenu.h"

#include "pidgincore.h"

/******************************************************************************
 * Helpers
 *****************************************************************************/
static GMenu *
pidgin_accounts_enabled_menu_build_submenu(PurpleAccount *account) {
	GMenu *menu = NULL, *section = NULL;
	gchar *action_id = NULL;
	const gchar *account_id = NULL;

	menu = g_menu_new();
	account_id = purple_account_get_id(account);

	/* Add the "Edit Account" section. */
	section = g_menu_new();
	g_menu_append_section(menu, NULL, G_MENU_MODEL(section));

	action_id = g_strdup_printf("app.edit-account::%s", account_id);
	g_menu_append(section, _("Edit Account"), action_id);
	g_free(action_id);

	/* Add the "Disable Account" section. */
	section = g_menu_new();
	g_menu_append_section(menu, NULL, G_MENU_MODEL(section));

	action_id = g_strdup_printf("app.disable-account::%s", account_id);
	g_menu_append(section, _("Disable"), action_id);
	g_free(action_id);

	return menu;
}

static void
pidgin_accounts_enabled_menu_refresh_helper(PurpleAccount *account,
                                            gpointer data)
{
	GMenu *menu = data;

	if(purple_account_get_enabled(account, PIDGIN_UI)) {
		GMenu *submenu = NULL;
		gchar *label = NULL;
		const gchar *account_name = purple_account_get_username(account);
		const gchar *protocol_name = purple_account_get_protocol_name(account);

		submenu = pidgin_accounts_enabled_menu_build_submenu(account);

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

	return menu;
}
