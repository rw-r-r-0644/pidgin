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

#ifndef PIDGIN_ACCOUNT_FILTER_CONNECTED_H
#define PIDGIN_ACCOUNT_FILTER_CONNECTED_H

/**
 * SECTION: pidginaccountfilterconnected
 * @section_id: pidgin-account-filter-connected
 * @short_description: A GtkTreeModelFilter that displays connected accounts.
 * @title: Account Connected Filter
 *
 * #PidginAccountFilterConnected is a #GtkTreeModelFilter that will only show
 * accounts that are connected.  It's intended to be used with
 * #PidginAccountStore.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PIDGIN_TYPE_ACCOUNT_FILTER_CONNECTED pidgin_account_filter_connected_get_type()
G_DECLARE_FINAL_TYPE(PidginAccountFilterConnected,
                     pidgin_account_filter_connected, PIDGIN,
                     ACCOUNT_FILTER_CONNECTED, GtkTreeModelFilter)

/**
 * pidgin_account_filter_connected_new:
 * @child_model: The #GtkTreeModel instance to filter.
 * @root: The #GtkTreePath to start at or %NULL.
 *
 * Creates a new #PidginAccountFilterConnected that should be used to filter
 * only online accounts in a #PidginAccountStore.
 *
 * Returns: (transfer full): The new #PidginAccountFilterConnected instance.
 *
 * Since: 3.0.0
 */
GtkTreeModel *pidgin_account_filter_connected_new(GtkTreeModel *child_model, GtkTreePath *root);

G_END_DECLS

#endif /* PIDGIN_ACCOUNT_FILTER_CONNECTED_H */
