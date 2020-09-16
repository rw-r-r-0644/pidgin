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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PIDGIN_ACCOUNT_FILTER_PROTOCOL_H
#define PIDGIN_ACCOUNT_FILTER_PROTOCOL_H

/**
 * SECTION:pidginaccountfilterprotocol
 * @section_id: pidgin-account-filter-protocol
 * @short_description: A GtkTreeModelFilter that displays the given protocol.
 * @title: Account Protocol Filter
 *
 * #PidginAccountFilterProtocol is a #GtkTreeModelFilter that will only show
 * accounts for the given protocol.  It's intended to be used with
 * #PidginAccountStore.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PIDGIN_TYPE_ACCOUNT_FILTER_PROTOCOL pidgin_account_filter_protocol_get_type()
G_DECLARE_FINAL_TYPE(PidginAccountFilterProtocol,
                     pidgin_account_filter_protocol, PIDGIN,
                     ACCOUNT_FILTER_PROTOCOL, GtkTreeModelFilter)

/**
 * pidgin_account_filter_protocol_new:
 * @protocol_id: The ID of the protocol to filter for.
 * @child_model: The #GtkTreeModel that should be filtered.
 * @root: The root path that should be filtered.
 *
 * Creates a new #PidginAccountFilterProtocol that should be used to filter
 * only online accounts in a #PidginAccountStore.
 *
 * See gtk_tree_model_filter_new() for more information.
 *
 * Returns: (transfer full): The new #PidginAccountFilterProtocol instance.
 *
 * Since: 3.0.0
 */
GtkTreeModel *pidgin_account_filter_protocol_new(const gchar *protocol_id, GtkTreeModel *child_model, GtkTreePath *root);

/**
 * pidgin_account_filter_protocol_get_protocol_id:
 * @filter: The #PidginAccountFilterProtocol instance.
 *
 * Gets the ID of the protocol that @filter is filtering for.
 *
 * Returns: The Protocol ID that @filter is filtering for.
 */
const gchar *pidgin_account_filter_protocol_get_protocol_id(PidginAccountFilterProtocol *filter);

G_END_DECLS

#endif /* PIDGIN_ACCOUNT_FILTER_PROTOCOL_H */
