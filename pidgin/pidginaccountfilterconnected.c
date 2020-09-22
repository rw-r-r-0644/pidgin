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

#include "pidgin/pidginaccountfilterconnected.h"

#include "pidgin/pidginaccountstore.h"

#include <purple.h>

struct _PidginAccountFilterConnected {
	GtkTreeModelFilter parent;
};

/******************************************************************************
 * Callbacks
 *****************************************************************************/
static void
pidgin_account_filter_connected_changed(PurpleConnection *connection,
                                        gpointer data)
{
	PidginAccountFilterConnected *filter = NULL;

	filter = PIDGIN_ACCOUNT_FILTER_CONNECTED(data);

	gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(filter));
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
static gboolean
pidgin_account_filter_connected_func(GtkTreeModel *model, GtkTreeIter *iter,
                                     gpointer data)
{
	PurpleAccount *account = NULL;
	gboolean ret = FALSE;

	g_return_val_if_fail(GTK_IS_TREE_MODEL(model), FALSE);
	g_return_val_if_fail(iter != NULL, FALSE);

	gtk_tree_model_get(model, iter, PIDGIN_ACCOUNT_STORE_COLUMN_ACCOUNT,
	                   &account, -1);

	if(!PURPLE_IS_ACCOUNT(account)) {
		return FALSE;
	}

	ret = purple_account_is_connected(account);

	g_object_unref(G_OBJECT(account));

	return ret;
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
G_DEFINE_TYPE(PidginAccountFilterConnected, pidgin_account_filter_connected,
              GTK_TYPE_TREE_MODEL_FILTER)

static void
pidgin_account_filter_connected_init(PidginAccountFilterConnected *filter) {
	gpointer connections_handle = NULL;

	gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(filter),
	                                       pidgin_account_filter_connected_func,
	                                       NULL, NULL);

	/* we connect to the connections signals to force a refresh of the filter */
	connections_handle = purple_connections_get_handle();
	purple_signal_connect(connections_handle, "signed-on", filter,
	                      G_CALLBACK(pidgin_account_filter_connected_changed),
	                      filter);
	purple_signal_connect(connections_handle, "signed-off", filter,
	                      G_CALLBACK(pidgin_account_filter_connected_changed),
	                      filter);
}

static void
pidgin_account_filter_connected_finalize(GObject *obj) {
	purple_signals_disconnect_by_handle(obj);

	G_OBJECT_CLASS(pidgin_account_filter_connected_parent_class)->finalize(obj);
}

static void
pidgin_account_filter_connected_class_init(PidginAccountFilterConnectedClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->finalize = pidgin_account_filter_connected_finalize;
}

/******************************************************************************
 * API
 *****************************************************************************/
GtkTreeModel *
pidgin_account_filter_connected_new(GtkTreeModel *child_model,
                                    GtkTreePath *root)
{
	g_return_val_if_fail(GTK_IS_TREE_MODEL(child_model), NULL);

	return g_object_new(PIDGIN_TYPE_ACCOUNT_FILTER_CONNECTED, "child-model",
	                    child_model, "virtual-root", root, NULL);
}
