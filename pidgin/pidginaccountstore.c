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

#include <glib/gi18n-lib.h>

#include "pidgin/pidginaccountstore.h"

#include "gtkutils.h"

struct _PidginAccountStore {
	GtkListStore parent;
};

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
pidgin_account_store_add_account(PidginAccountStore *store,
                                 PurpleAccount *account)
{
	GtkTreeIter iter;
	GdkPixbuf *pixbuf = NULL;
	gchar *markup = NULL;
	const gchar *alias = NULL;

	pixbuf = pidgin_create_protocol_icon(account, PIDGIN_PROTOCOL_ICON_SMALL);

	alias = purple_account_get_private_alias(account);
	if(alias != NULL) {
		markup = g_strdup_printf(_("%s (%s) (%s)"),
		                         purple_account_get_username(account),
		                         alias,
		                         purple_account_get_protocol_name(account));
	} else {
		markup = g_strdup_printf(_("%s (%s)"),
		                         purple_account_get_username(account),
		                         purple_account_get_protocol_name(account));
	}

	gtk_list_store_append(GTK_LIST_STORE(store), &iter);
	gtk_list_store_set(
		GTK_LIST_STORE(store),
		&iter,
		PIDGIN_ACCOUNT_STORE_COLUMN_ACCOUNT, account,
		PIDGIN_ACCOUNT_STORE_COLUMN_MARKUP, markup,
		PIDGIN_ACCOUNT_STORE_COLUMN_ICON, pixbuf,
		-1
	);

	g_clear_object(&pixbuf);
	g_free(markup);
}

static void
pidgin_account_store_add_account_helper(PurpleAccount *account, gpointer data) {
	if(PURPLE_IS_ACCOUNT(account)) {
		pidgin_account_store_add_account(PIDGIN_ACCOUNT_STORE(data),
		                                 PURPLE_ACCOUNT(account));
	}
}

static void
pidgin_account_store_add_accounts(PidginAccountStore *store) {
	PurpleAccountManager *manager = NULL;

	manager = purple_account_manager_get_default();
	purple_account_manager_foreach(manager,
	                               pidgin_account_store_add_account_helper,
	                               store);
}

static void
pidgin_account_store_remove_account(PidginAccountStore *store,
                                    PurpleAccount *account)
{
	GtkTreeIter iter;

	if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter) == FALSE) {
		purple_debug_warning("account-store",
		                     "account %s was removed but not in the store",
		                     purple_account_get_username(account));
		return;
	}

	/* walk through the store and look for the account to remove */
	do {
		PurpleAccount *found = NULL;

		gtk_tree_model_get(
			GTK_TREE_MODEL(store),
			&iter,
			PIDGIN_ACCOUNT_STORE_COLUMN_ACCOUNT, &found,
			-1
		);

		if(found == account) {
			g_object_unref(G_OBJECT(found));

			gtk_list_store_remove(GTK_LIST_STORE(store), &iter);

			return;
		}
	} while(gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter));
}

/******************************************************************************
 * Callbacks
 *****************************************************************************/
static void
pidgin_account_store_account_added_cb(PurpleAccount *account,
                                      gpointer data)
{
	pidgin_account_store_add_account(PIDGIN_ACCOUNT_STORE(data), account);
}

static void
pidgin_account_store_account_removed_cb(PurpleAccount *account,
                                        gpointer data)
{
	pidgin_account_store_remove_account(PIDGIN_ACCOUNT_STORE(data), account);
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
G_DEFINE_TYPE(PidginAccountStore, pidgin_account_store, GTK_TYPE_LIST_STORE)

static void
pidgin_account_store_init(PidginAccountStore *store) {
	gpointer accounts_handle = NULL;
	GType types[PIDGIN_ACCOUNT_STORE_N_COLUMNS] = {
		PURPLE_TYPE_ACCOUNT,
		G_TYPE_STRING,
		GDK_TYPE_PIXBUF,
	};

	gtk_list_store_set_column_types(
		GTK_LIST_STORE(store),
		PIDGIN_ACCOUNT_STORE_N_COLUMNS,
		types
	);

	/* add the known accounts */
	pidgin_account_store_add_accounts(store);

	/* add the signal handlers to dynamically add/remove accounts */
	accounts_handle = purple_accounts_get_handle();
	purple_signal_connect(accounts_handle, "account-added", store,
	                      G_CALLBACK(pidgin_account_store_account_added_cb),
	                      store);
	purple_signal_connect(accounts_handle, "account-removed", store,
	                      G_CALLBACK(pidgin_account_store_account_removed_cb),
	                      store);
}

static void
pidgin_account_store_finalize(GObject *obj) {
	purple_signals_disconnect_by_handle(obj);

	G_OBJECT_CLASS(pidgin_account_store_parent_class)->finalize(obj);
}

static void
pidgin_account_store_class_init(PidginAccountStoreClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->finalize = pidgin_account_store_finalize;
}

/******************************************************************************
 * API
 *****************************************************************************/
GtkListStore *
pidgin_account_store_new(void) {
	return GTK_LIST_STORE(g_object_new(PIDGIN_TYPE_ACCOUNT_STORE, NULL));
}
