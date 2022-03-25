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

#include "pidgininactiveaccountsmenu.h"

struct _PidginInactiveAccountsMenu {
	GMenuModel parent;

	GList *accounts;
};

G_DEFINE_TYPE(PidginInactiveAccountsMenu, pidgin_inactive_accounts_menu,
              G_TYPE_MENU_MODEL)

/******************************************************************************
 * Callbacks
 *****************************************************************************/
static void
pidgin_inactive_accounts_menu_refresh(PidginInactiveAccountsMenu *menu) {
	PurpleAccountManager *manager = NULL;
	gint removed = 0, added = 0;

	/* When refreshing we're always removing at least 1 item because of the
	 * "no disabled accounts" item that we put in place when all accounts
	 * are enabled.
	 */
	removed = MAX(1, g_list_length(menu->accounts));

	/* Grab the manager and get all the disabled accounts. */
	manager = purple_account_manager_get_default();
	g_list_free(menu->accounts);
	menu->accounts = purple_account_manager_get_inactive(manager);

	/* Similar to the aboved note about removed items, if every account is
	 * enabled, we add an item saying "no disabled accounts".
	 */
	added = MAX(1, g_list_length(menu->accounts));

	/* Tell any listeners that our menu has changed. */
	g_menu_model_items_changed(G_MENU_MODEL(menu), 0, removed, added);
}

static void
pidgin_inactive_accounts_menu_changed_cb(G_GNUC_UNUSED PurpleAccount *account,
                                         gpointer data)
{
	PidginInactiveAccountsMenu *menu = data;

	pidgin_inactive_accounts_menu_refresh(menu);
}

/******************************************************************************
 * GMenuModel Implementation
 *****************************************************************************/
static gboolean
pidgin_inactive_accounts_menu_is_mutable(GMenuModel *model) {
	return TRUE;
}

static gboolean
pidgin_inactive_accounts_menu_get_n_items(GMenuModel *model) {
	PidginInactiveAccountsMenu *menu = NULL;

	menu = PIDGIN_INACTIVE_ACCOUNTS_MENU(model);

	if(menu->accounts == NULL) {
		return 1;
	}

	return g_list_length(menu->accounts);
}

static void
pidgin_inactive_accounts_menu_get_item_attributes(GMenuModel *model,
                                                  gint index,
                                                  GHashTable **attributes)
{
	PidginInactiveAccountsMenu *menu = NULL;
	PurpleAccount *account = NULL;
	PurpleProtocol *protocol = NULL;
	GVariant *value = NULL;
	const gchar *account_name = NULL, *protocol_name = NULL;

	menu = PIDGIN_INACTIVE_ACCOUNTS_MENU(model);

	/* Create our hash table of attributes to return. */
	*attributes = g_hash_table_new_full(g_str_hash, g_str_equal, NULL,
	                                    (GDestroyNotify)g_variant_unref);

	/* If we don't have any disabled accounts, just return a single item,
	 * stating as much.
	 */
	if(menu->accounts == NULL) {
		value = g_variant_new_string(_("No disabled accounts"));
		g_hash_table_insert(*attributes, G_MENU_ATTRIBUTE_LABEL,
		                    g_variant_ref_sink(value));

		value = g_variant_new_string("disabled");
		g_hash_table_insert(*attributes, G_MENU_ATTRIBUTE_ACTION,
		                    g_variant_ref_sink(value));

		return;
	}

	account = g_list_nth_data(menu->accounts, index);
	if(account == NULL) {
		return;
	}

	account_name = purple_account_get_username(account);
	protocol_name = purple_account_get_protocol_name(account);

	/* translators: This format string is intended to contain the account
	 * name followed by the protocol name to uniquely identify a specific
	 * account.
	 */
	value = g_variant_new_printf(_("%s (%s)"), account_name, protocol_name);
	g_hash_table_insert(*attributes, G_MENU_ATTRIBUTE_LABEL,
	                    g_variant_ref_sink(value));

	value = g_variant_new_string("app.enable-account");
	g_hash_table_insert(*attributes, G_MENU_ATTRIBUTE_ACTION,
	                    g_variant_ref_sink(value));

	value = g_variant_new_printf("%s", purple_account_get_id(account));
	g_hash_table_insert(*attributes, G_MENU_ATTRIBUTE_TARGET,
	                    g_variant_ref_sink(value));

	protocol = purple_account_get_protocol(account);
	if(protocol != NULL) {
		value = g_variant_new_printf("%s", purple_protocol_get_icon_name(protocol));
		g_hash_table_insert(*attributes, G_MENU_ATTRIBUTE_ICON,
		                    g_variant_ref_sink(value));
	}
}

static void
pidgin_inactive_accounts_menu_get_item_links(GMenuModel *model, gint index,
                                             GHashTable **links)
{
	*links = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
	                               g_object_unref);
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
static void
pidgin_inactive_accounts_menu_dispose(GObject *obj) {
	purple_signals_disconnect_by_handle(obj);

	G_OBJECT_CLASS(pidgin_inactive_accounts_menu_parent_class)->dispose(obj);
}

static void
pidgin_inactive_accounts_menu_constructed(GObject *obj) {
	G_OBJECT_CLASS(pidgin_inactive_accounts_menu_parent_class)->constructed(obj);

	pidgin_inactive_accounts_menu_refresh(PIDGIN_INACTIVE_ACCOUNTS_MENU(obj));
}

static void
pidgin_inactive_accounts_menu_init(PidginInactiveAccountsMenu *menu) {
	gpointer handle = NULL;

	/* Wire up the purple signals we care about. */
	handle = purple_accounts_get_handle();
	purple_signal_connect(handle, "account-enabled", menu,
	                      G_CALLBACK(pidgin_inactive_accounts_menu_changed_cb),
	                      menu);
	purple_signal_connect(handle, "account-disabled", menu,
	                      G_CALLBACK(pidgin_inactive_accounts_menu_changed_cb),
	                      menu);
}

static void
pidgin_inactive_accounts_menu_class_init(PidginInactiveAccountsMenuClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	GMenuModelClass *model_class = G_MENU_MODEL_CLASS(klass);

	obj_class->constructed = pidgin_inactive_accounts_menu_constructed;
	obj_class->dispose = pidgin_inactive_accounts_menu_dispose;

	model_class->is_mutable = pidgin_inactive_accounts_menu_is_mutable;
	model_class->get_n_items = pidgin_inactive_accounts_menu_get_n_items;
	model_class->get_item_attributes = pidgin_inactive_accounts_menu_get_item_attributes;
	model_class->get_item_links = pidgin_inactive_accounts_menu_get_item_links;
}

/******************************************************************************
 * Public API
 *****************************************************************************/
GMenuModel *
pidgin_inactive_accounts_menu_new(void) {
	return g_object_new(PIDGIN_TYPE_INACTIVE_ACCOUNTS_MENU, NULL);
}
