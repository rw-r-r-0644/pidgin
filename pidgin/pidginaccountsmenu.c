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

#include "pidginaccountsmenu.h"

#include <purple.h>

#include "pidginaccountactionsmenu.h"

#include "pidgincore.h"

struct _PidginAccountsMenu {
	GtkMenu parent;

	GtkWidget *enable_account;
	GtkWidget *disabled_menu;
	GtkWidget *separator;

	GHashTable *account_items;
	GHashTable *disabled_items;
};

/******************************************************************************
 * GSignal Handlers
 *****************************************************************************/
static void
pidgin_accounts_menu_enable_account(GtkMenuItem *item, gpointer data) {
	PurpleAccount *account = PURPLE_ACCOUNT(data);

	purple_account_set_enabled(account, TRUE);
}

/******************************************************************************
 * Helpers
 *****************************************************************************/
static GtkWidget *
pidgin_accounts_menu_create_account_menu_item(PidginAccountsMenu *menu,
                                              PurpleAccount *account)
{
	GtkWidget *item = NULL;
	const gchar *account_name = purple_account_get_username(account);
	const gchar *protocol_name = purple_account_get_protocol_name(account);
	gchar *label = g_strdup_printf("%s (%s)", account_name, protocol_name);

	item = gtk_menu_item_new_with_label(label);
	g_free(label);
	gtk_widget_show(item);

	return item;
}

static void
pidgin_accounts_menu_add_enabled_account(PidginAccountsMenu *menu,
                                         PurpleAccount *account)
{
	GtkWidget *item = NULL, *submenu = NULL;
	gpointer data = NULL;
	gboolean found = FALSE;

	/* if the account is in the disabled list, delete its widget */
	found = g_hash_table_lookup_extended(menu->disabled_items, account, NULL,
	                                     &data);
	if(found) {
		g_clear_pointer(&data, gtk_widget_destroy);
		g_hash_table_remove(menu->disabled_items, account);

		if(g_hash_table_size(menu->disabled_items) == 0) {
			gtk_widget_set_sensitive(menu->enable_account, FALSE);
		}
	}

	item = pidgin_accounts_menu_create_account_menu_item(menu, account);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_hash_table_insert(menu->account_items,
	                    g_object_ref(G_OBJECT(account)),
	                    item);

	/* create the submenu and attach it to item right away, this allows us to
	 * reuse item for the submenu items.
	 */
	submenu = pidgin_account_actions_menu_new(account);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
}

static void
pidgin_accounts_menu_add_disabled_account(PidginAccountsMenu *menu,
                                          PurpleAccount *account)
{
	GtkWidget *item = NULL;
	gpointer data = NULL;
	gboolean found = FALSE;

	/* if the account is in the enabled list, delete its widget */
	found = g_hash_table_lookup_extended(menu->account_items, account, NULL,
	                                     &data);
	if(found) {
		g_clear_pointer(&data, gtk_widget_destroy);
		g_hash_table_remove(menu->account_items, account);
	}

	item = pidgin_accounts_menu_create_account_menu_item(menu, account);
	g_signal_connect(G_OBJECT(item), "activate",
	                 G_CALLBACK(pidgin_accounts_menu_enable_account), account);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu->disabled_menu), item);

	g_hash_table_insert(menu->disabled_items,
	                    g_object_ref(G_OBJECT(account)),
	                    item);

	/* We know there's at least one item in the menu, so make sure it is
	 * sensitive.
	 */
	gtk_widget_set_sensitive(menu->enable_account, TRUE);
}

static void
pidgin_accounts_menu_foreach_cb(PurpleAccount *account, gpointer data) {
	PidginAccountsMenu *menu = PIDGIN_ACCOUNTS_MENU(data);

	if(purple_account_get_enabled(account)) {
		pidgin_accounts_menu_add_enabled_account(menu, account);
	} else {
		pidgin_accounts_menu_add_disabled_account(menu, account);
	}
}

static void
pidgin_accounts_menu_add_current(PidginAccountsMenu *menu) {
	PurpleAccountManager *manager = NULL;

	manager = purple_account_manager_get_default();
	purple_account_manager_foreach(manager, pidgin_accounts_menu_foreach_cb,
	                               menu);
}

/******************************************************************************
 * Purple Signal Callbacks
 *****************************************************************************/
static void
pidgin_accounts_menu_account_status_changed(PurpleAccount *account,
                                            gpointer d)
{
	PidginAccountsMenu *menu = PIDGIN_ACCOUNTS_MENU(d);
	gpointer data = NULL;

	data = g_hash_table_lookup(menu->account_items, account);
	if(GTK_IS_WIDGET(data)) {
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(data),
		                          pidgin_account_actions_menu_new(account));
	}
}

static void
pidgin_accounts_menu_account_enabled(PurpleAccount *account, gpointer data) {
	pidgin_accounts_menu_add_enabled_account(PIDGIN_ACCOUNTS_MENU(data),
	                                         account);
}

static void
pidgin_accounts_menu_account_disabled(PurpleAccount *account, gpointer data) {
	pidgin_accounts_menu_add_disabled_account(PIDGIN_ACCOUNTS_MENU(data),
	                                          account);
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
G_DEFINE_TYPE(PidginAccountsMenu, pidgin_accounts_menu, GTK_TYPE_MENU)

static void
pidgin_accounts_menu_init(PidginAccountsMenu *menu) {
	gpointer handle;

	/* initialize our template */
	gtk_widget_init_template(GTK_WIDGET(menu));

	/* create our storage for the items */
	menu->account_items = g_hash_table_new_full(g_direct_hash, g_direct_equal,
	                                            g_object_unref, NULL);
	menu->disabled_items = g_hash_table_new_full(g_direct_hash, g_direct_equal,
	                                             g_object_unref, NULL);

	/* add all of the existing accounts */
	pidgin_accounts_menu_add_current(menu);

	/* finally connect to the purple signals to stay up to date */
	handle = purple_accounts_get_handle();
	purple_signal_connect(handle, "account-signed-on", menu,
	                      G_CALLBACK(pidgin_accounts_menu_account_status_changed),
	                      menu);
	purple_signal_connect(handle, "account-signed-off", menu,
	                      G_CALLBACK(pidgin_accounts_menu_account_status_changed),
	                      menu);
	purple_signal_connect(handle, "account-actions-changed", menu,
	                      G_CALLBACK(pidgin_accounts_menu_account_status_changed),
	                      menu);
	purple_signal_connect(handle, "account-enabled", menu,
	                      G_CALLBACK(pidgin_accounts_menu_account_enabled),
	                      menu);
	purple_signal_connect(handle, "account-disabled", menu,
	                      G_CALLBACK(pidgin_accounts_menu_account_disabled),
	                      menu);
};

static void
pidgin_accounts_menu_finalize(GObject *obj) {
	PidginAccountsMenu *menu = PIDGIN_ACCOUNTS_MENU(obj);

	purple_signals_disconnect_by_handle(obj);

	g_hash_table_destroy(menu->account_items);
	g_hash_table_destroy(menu->disabled_items);

	G_OBJECT_CLASS(pidgin_accounts_menu_parent_class)->finalize(obj);
}

static void
pidgin_accounts_menu_class_init(PidginAccountsMenuClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	obj_class->finalize = pidgin_accounts_menu_finalize;

	gtk_widget_class_set_template_from_resource(
	    widget_class,
	    "/im/pidgin/Pidgin3/Accounts/menu.ui"
	);

	gtk_widget_class_bind_template_child(widget_class, PidginAccountsMenu,
	                                     enable_account);
	gtk_widget_class_bind_template_child(widget_class, PidginAccountsMenu,
	                                     disabled_menu);
	gtk_widget_class_bind_template_child(widget_class, PidginAccountsMenu,
	                                     separator);
}

/******************************************************************************
 * Public API
 *****************************************************************************/
GtkWidget *
pidgin_accounts_menu_new(void) {
	return GTK_WIDGET(g_object_new(PIDGIN_TYPE_ACCOUNTS_MENU, NULL));
}

