/* pidgin
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#if !defined(PIDGIN_GLOBAL_HEADER_INSIDE) && !defined(PIDGIN_COMPILATION)
# error "only <pidgin.h> may be included directly"
#endif

#ifndef PIDGIN_PROTOCOL_STORE_H
#define PIDGIN_PROTOCOL_STORE_H

/**
 * PidginProtocolStore:
 *
 * #PidginProtocolStore is a #GtkListStore that automatically keeps track of
 * what protocols are currently available in libpurple.  It's intended to be
 * used any time that you need to present a protocol selection to the user.
 *
 * Since: 3.0.0
 */

#include <gtk/gtk.h>

#include <purple.h>

/**
 * PidginProtocolStoreColumn:
 * @PIDGIN_PROTOCOL_STORE_COLUMN_PROTOCOL: This column holds a reference to a
 *                                         #PurpleProtocol.
 * @PIDGIN_PROTOCOL_STORE_COLUMN_ID: This column holds the id of the protocol.
 * @PIDGIN_PROTOCOL_STORE_COLUMN_NAME: This column holds the name of the
 *                                     protocol which is used for display.
 * @PIDGIN_PROTOCOL_STORE_COLUMN_ICON: This column holds a #GdkPixbuf of the
 *                                     logo of the protocol.
 *
 * Constants for accessing columns in a #PidginProtocolStore.
 *
 * Since: 3.0.0
 */
typedef enum {
	PIDGIN_PROTOCOL_STORE_COLUMN_PROTOCOL,
	PIDGIN_PROTOCOL_STORE_COLUMN_ID,
	PIDGIN_PROTOCOL_STORE_COLUMN_NAME,
	PIDGIN_PROTOCOL_STORE_COLUMN_ICON,
	/*< private >*/
	PIDGIN_PROTOCOL_STORE_N_COLUMNS,
} PidginProtocolStoreColumn;

G_BEGIN_DECLS

#define PIDGIN_TYPE_PROTOCOL_STORE pidgin_protocol_store_get_type()

G_DECLARE_FINAL_TYPE(PidginProtocolStore, pidgin_protocol_store, PIDGIN,
                     PROTOCOL_STORE, GtkListStore)

/**
 * pidgin_protocol_store_new:
 *
 * Creates a new #PidginProtocolStore that can be used anywhere a #GtkListStore
 * can be used.
 *
 * Returns: (transfer full): The new #PidginProtocolStore instance.
 *
 * Since: 3.0.0
 */
GtkListStore *pidgin_protocol_store_new(void);

G_END_DECLS

#endif /* PIDGIN_PROTOCOL_STORE_H */
