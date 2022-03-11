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

#if !defined(PIDGIN_GLOBAL_HEADER_INSIDE) && !defined(PIDGIN_COMPILATION)
# error "only <pidgin.h> may be included directly"
#endif

#ifndef PIDGIN_STATUS_PRIMITIVE_STORE_H
#define PIDGIN_STATUS_PRIMITIVE_STORE_H

#include <gtk/gtk.h>

#include <purple.h>

G_BEGIN_DECLS

/**
 * PidginStatusPrimitiveStore:
 *
 * A [class@Gtk.ListStore] for use with [class@PidginStatusPrimitiveChooser].
 *
 * Since: 3.0.0
 */

#define PIDGIN_TYPE_STATUS_PRIMITIVE_STORE (pidgin_status_primitive_store_get_type())
G_DECLARE_FINAL_TYPE(PidginStatusPrimitiveStore, pidgin_status_primitive_store,
                     PIDGIN, STATUS_PRIMITIVE_STORE, GtkListStore)

/**
 * pidgin_status_primitive_store_new:
 *
 * Creates a new [class@Gtk.ListStore] that contains status primitives in
 * purple.
 *
 * Returns: (transfer full): The new list store.
 *
 * Since: 3.0.0
 */
GtkListStore *pidgin_status_primitive_store_new(void);

/**
 * pidgin_status_primitive_store_set_account:
 * @store: The status primitive store instance.
 * @account: (nullable): The account to use.
 *
 * Sets the [class@Purple.Account] whose status primitives to show. If %NULL,
 * all status primitives will be displayed.
 *
 * Since: 3.0.0
 */
void pidgin_status_primitive_store_set_account(PidginStatusPrimitiveStore *store, PurpleAccount *account);

/**
 * pidgin_status_primitive_store_get_account:
 * @store: The store instance.
 *
 * Gets the [class@Purple.Account] that's associated with @store.
 *
 * Returns: (transfer none): The account of %NULL.
 *
 * Since: 3.0.0
 */
PurpleAccount *pidgin_status_primitive_store_get_account(PidginStatusPrimitiveStore *store);

G_END_DECLS

#endif /* PIDGIN_STATUS_PRIMITIVE_STORE_H */
