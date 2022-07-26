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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301 USA
 */

#include "pidgin/pidginprotocolstore.h"

#include "gtkutils.h"

struct _PidginProtocolStore {
	GtkListStore parent;
};

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
pidgin_protocol_store_add_protocol(PidginProtocolStore *store,
                                   PurpleProtocol *protocol)
{
	GtkTreeIter iter;
	const gchar *icon_name = NULL;

	icon_name = purple_protocol_get_icon_name(protocol);

	gtk_list_store_append(GTK_LIST_STORE(store), &iter);
	gtk_list_store_set(
		GTK_LIST_STORE(store),
		&iter,
		PIDGIN_PROTOCOL_STORE_COLUMN_PROTOCOL, protocol,
		PIDGIN_PROTOCOL_STORE_COLUMN_ID, purple_protocol_get_id(protocol),
		PIDGIN_PROTOCOL_STORE_COLUMN_NAME, purple_protocol_get_name(protocol),
		PIDGIN_PROTOCOL_STORE_COLUMN_ICON_NAME, icon_name,
		-1
	);
}

static void
pidgin_protocol_store_add_protocol_helper(gpointer data, gpointer user_data) {
	if(PURPLE_IS_PROTOCOL(data)) {
		pidgin_protocol_store_add_protocol(PIDGIN_PROTOCOL_STORE(user_data),
		                                   PURPLE_PROTOCOL(data));
	}
}

static void
pidgin_protocol_store_add_protocols(PidginProtocolStore *store) {
	PurpleProtocolManager *manager = purple_protocol_manager_get_default();
	GList *protocols = NULL;

	protocols = purple_protocol_manager_get_all(manager);
	g_list_foreach(protocols, pidgin_protocol_store_add_protocol_helper,
	               store);
	g_list_free(protocols);
}

static void
pidgin_protocol_store_remove_protocol(PidginProtocolStore *store,
                                      PurpleProtocol *protocol)
{
	GtkTreeIter iter;

	if(gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter) == FALSE) {
		purple_debug_warning("protocol-store",
		                     "protocol %s was removed but not in the store",
		                     purple_protocol_get_name(protocol));
		return;
	}

	/* walk through the store and look for the protocol to remove */
	do {
		PurpleProtocol *prpl = NULL;

		gtk_tree_model_get(
			GTK_TREE_MODEL(store),
			&iter,
			PIDGIN_PROTOCOL_STORE_COLUMN_PROTOCOL, &prpl,
			-1
		);

		if(prpl == protocol) {
			g_object_unref(G_OBJECT(prpl));

			gtk_list_store_remove(GTK_LIST_STORE(store), &iter);

			return;
		}
	} while(gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter));
}

/******************************************************************************
 * Callbacks
 *****************************************************************************/
static void
pidgin_protocol_store_registered_cb(PurpleProtocolManager *manager,
                                    PurpleProtocol *protocol, gpointer data)
{
	pidgin_protocol_store_add_protocol(PIDGIN_PROTOCOL_STORE(data), protocol);
}

static void
pidgin_protocol_store_unregistered_cb(PurpleProtocolManager *manager,
                                      PurpleProtocol *protocol, gpointer data)
{
	pidgin_protocol_store_remove_protocol(PIDGIN_PROTOCOL_STORE(data),
	                                      protocol);
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
G_DEFINE_TYPE(PidginProtocolStore, pidgin_protocol_store, GTK_TYPE_LIST_STORE)

static void
pidgin_protocol_store_init(PidginProtocolStore *store) {
	PurpleProtocolManager *manager = NULL;
	GType types[PIDGIN_PROTOCOL_STORE_N_COLUMNS] = {
		PURPLE_TYPE_PROTOCOL,
		G_TYPE_STRING,
		G_TYPE_STRING,
		G_TYPE_STRING,
	};

	gtk_list_store_set_column_types(
		GTK_LIST_STORE(store),
		PIDGIN_PROTOCOL_STORE_N_COLUMNS,
		types
	);

	/* add the known protocols */
	pidgin_protocol_store_add_protocols(store);

	/* add the signal handlers to dynamically add/remove protocols */
	manager = purple_protocol_manager_get_default();
	g_signal_connect_object(G_OBJECT(manager), "registered",
	                        G_CALLBACK(pidgin_protocol_store_registered_cb),
	                        store, 0);
	g_signal_connect_object(G_OBJECT(manager), "unregistered",
	                        G_CALLBACK(pidgin_protocol_store_unregistered_cb),
	                        store, 0);
}

static void
pidgin_protocol_store_class_init(PidginProtocolStoreClass *klass) {
}

/******************************************************************************
 * API
 *****************************************************************************/
GtkListStore *
pidgin_protocol_store_new(void) {
	return GTK_LIST_STORE(g_object_new(PIDGIN_TYPE_PROTOCOL_STORE, NULL));
}
