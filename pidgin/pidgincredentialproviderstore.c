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

#include "pidgin/pidgincredentialproviderstore.h"

struct _PidginCredentialProviderStore {
	GtkListStore parent;
};

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
pidgin_credential_provider_store_add(PidginCredentialProviderStore *store,
                                     PurpleCredentialProvider *provider)
{
	GtkTreeIter iter;
	gchar *markup = NULL;
	const gchar *id = NULL, *name = NULL, *description = NULL;

	id = purple_credential_provider_get_id(provider);
	name = purple_credential_provider_get_name(provider);
	description = purple_credential_provider_get_description(provider);

	markup = g_strdup_printf("<b>%s</b>\n%s", name, description);

	gtk_list_store_append(GTK_LIST_STORE(store), &iter);
	gtk_list_store_set(
		GTK_LIST_STORE(store),
		&iter,
		PIDGIN_CREDENTIAL_PROVIDER_STORE_COLUMN_ID, id,
		PIDGIN_CREDENTIAL_PROVIDER_STORE_COLUMN_MARKUP, markup,
		-1
	);

	g_free(markup);
}

static void
pidgin_credential_provider_store_add_helper(PurpleCredentialProvider *provider,
                                            gpointer data)
{
	pidgin_credential_provider_store_add(PIDGIN_CREDENTIAL_PROVIDER_STORE(data),
	                                     provider);
}

static void
pidgin_credential_provider_store_add_providers(PidginCredentialProviderStore *store)
{
	PurpleCredentialManager *manager = NULL;

	manager = purple_credential_manager_get_default();

	purple_credential_manager_foreach_provider(manager,
	                                           pidgin_credential_provider_store_add_helper,
	                                           store);
}

static void
pidgin_credential_provider_store_remove(PidginCredentialProviderStore *store,
                                        PurpleCredentialProvider *provider)
{
	GtkTreeIter iter;
	const gchar *id = NULL;

	id = purple_credential_provider_get_id(provider);

	if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter) == FALSE) {
		purple_debug_warning("credential-provider-store",
		                     "asked to remove provider %s but the store is "
		                     "empty",
		                     id);
		return;
	}

	/* walk through the store and look for the provider to remove */
	do {
		gchar *found = NULL;

		gtk_tree_model_get(
			GTK_TREE_MODEL(store),
			&iter,
			PIDGIN_CREDENTIAL_PROVIDER_STORE_COLUMN_ID, &found,
			-1
		);

		if(purple_strequal(found, id)) {
			g_free(found);

			gtk_list_store_remove(GTK_LIST_STORE(store), &iter);

			return;
		}

		g_free(found);
	} while(gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter));
}

/******************************************************************************
 * Callbacks
 *****************************************************************************/
static void
purple_credential_provider_store_registered_cb(PurpleCredentialManager *manager,
                                               PurpleCredentialProvider *provider,
                                               gpointer data)
{
	pidgin_credential_provider_store_add(PIDGIN_CREDENTIAL_PROVIDER_STORE(data),
	                                     provider);
}

static void
purple_credential_provider_store_unregistered_cb(PurpleCredentialManager *manager,
                                                 PurpleCredentialProvider *provider,
                                                 gpointer data)
{
	pidgin_credential_provider_store_remove(PIDGIN_CREDENTIAL_PROVIDER_STORE(data),
	                                        provider);
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
G_DEFINE_TYPE(PidginCredentialProviderStore, pidgin_credential_provider_store, GTK_TYPE_LIST_STORE)

static void
pidgin_credential_provider_store_init(PidginCredentialProviderStore *store) {
	PurpleCredentialManager *manager = NULL;
	GType types[PIDGIN_CREDENTIAL_PROVIDER_STORE_N_COLUMNS] = {
		G_TYPE_STRING,
		G_TYPE_STRING,
	};

	gtk_list_store_set_column_types(
		GTK_LIST_STORE(store),
		PIDGIN_CREDENTIAL_PROVIDER_STORE_N_COLUMNS,
		types
	);

	/* add the known providers */
	pidgin_credential_provider_store_add_providers(store);

	manager = purple_credential_manager_get_default();

	g_signal_connect_object(G_OBJECT(manager), "provider-registered",
	                        G_CALLBACK(purple_credential_provider_store_registered_cb),
	                        store, 0);
	g_signal_connect_object(G_OBJECT(manager), "provider-unregistered",
	                        G_CALLBACK(purple_credential_provider_store_unregistered_cb),
	                        store, 0);
}

static void
pidgin_credential_provider_store_class_init(PidginCredentialProviderStoreClass *klass) {
}

/******************************************************************************
 * API
 *****************************************************************************/
GtkListStore *
pidgin_credential_provider_store_new(void) {
	return GTK_LIST_STORE(g_object_new(PIDGIN_TYPE_CREDENTIAL_PROVIDER_STORE,
	                      NULL));
}
