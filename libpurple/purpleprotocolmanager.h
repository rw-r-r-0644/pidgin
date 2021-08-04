/*
 * Purple - Internet Messaging Library
 * Copyright (C) Pidgin Developers <devel@pidgin.im>
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

#if !defined(PURPLE_GLOBAL_HEADER_INSIDE) && !defined(PURPLE_COMPILATION)
# error "only <pidgin.h> may be included directly"
#endif

#ifndef PURPLE_PROTOCOL_MANAGER_H
#define PURPLE_PROTOCOL_MANAGER_H

#include <glib.h>
#include <glib-object.h>

#include "protocol.h"

G_BEGIN_DECLS

/**
 * SECTION:purpleprotocolmanager
 * @section_id: libpurple-purpleprotocolmanager
 * @title: Purple Protocol Manager
 * @short_description: Management of protocols.
 *
 * #PurpleProtocolManager keeps track of all protocols and emits signals when
 * protocols are registered and unregistered.
 */

/**
 * PURPLE_PROTOCOL_MANAGER_DOMAIN:
 *
 * A #GError domain for errors from #PurpleProtocolManager.
 *
 * Since: 3.0.0
 */
#define PURPLE_PROTOCOL_MANAGER_DOMAIN (g_quark_from_static_string("purple-protocol-manager"))

/**
 * PURPLE_TYPE_PROTOCOL_MANAGER:
 *
 * The standard _get_type macro for #PurpleProtocolManager.
 */
#define PURPLE_TYPE_PROTOCOL_MANAGER (purple_protocol_manager_get_type())
G_DECLARE_DERIVABLE_TYPE(PurpleProtocolManager, purple_protocol_manager,
                         PURPLE, PROTOCOL_MANAGER, GObject)

/**
 * PurpleProtocolManager:
 *
 * An opaque data structure that manages protocols.
 *
 * Since: 3.0.0
 */

/**
 * PurpleProtocolManagerClass:
 * @registered: The default signal handler for when a protocol is registered.
 * @unregistered: The default signal handler for when a protocol is
 *                unregistered.
 *
 * The class structure for #PurpleProtocolManager.
 *
 * Since: 3.0.0
 */
struct _PurpleProtocolManagerClass {
	/*< private >*/
	GObjectClass parent;

	/*< public >*/
	void (*registered)(PurpleProtocolManager *manager, PurpleProtocol *protocol);
	void (*unregistered)(PurpleProtocolManager *manager, PurpleProtocol *protocol);

	/*< private >*/
	gpointer reserved[4];
};

/**
 * PurpleProtocolManagerForeachFunc:
 * @protocol: The #PurpleProtocol instance.
 * @data: User supplied data.
 *
 * A function to be used as a callback with purple_protocol_manager_foreach().
 *
 * Since: 3.0.0
 */
typedef void (*PurpleProtocolManagerForeachFunc)(PurpleProtocol *protocol, gpointer data);

/**
 * purple_protocol_manager_get_default:
 *
 * Gets the default #PurpleProtocolManager instance.
 *
 * Returns: (transfer none): The default #PurpleProtocolManager instance.
 *
 * Since: 3.0.0
 */
PurpleProtocolManager *purple_protocol_manager_get_default(void);

/**
 * purple_protocol_manager_register:
 * @manager: The #PurpleProtocolManager instance.
 * @protocol: The #PurpleProtocol to register.
 * @error: (out) (optional) (nullable): A return address for a #GError.
 *
 * Registers @protocol with @manager.
 *
 * Returns: %TRUE if @protocol was successfully registered with @manager,
 *          %FALSE otherwise.
 *
 * Since: 3.0.0
 */
gboolean purple_protocol_manager_register(PurpleProtocolManager *manager, PurpleProtocol *protocol, GError **error);

/**
 * purple_protocol_manager_unregister:
 * @manager: The #PurpleProtocolManager instance.
 * @protocol: The #PurpleProtocol to unregister.
 * @error: (out) (optional) (nullable): A return address for a #GError.
 *
 * Unregisters @protocol from @manager.
 *
 * Returns: %TRUE if @protocol was successfully unregistered from @manager,
 *          %FALSE otherwise.
 *
 * Since: 3.0.0
 */
gboolean purple_protocol_manager_unregister(PurpleProtocolManager *manager, PurpleProtocol *protocol, GError **error);

/**
 * purple_protocol_manager_find:
 * @manager: The #PurpleProtocolManager instance.
 * @id: The id of the #PurpleProtocol to find.
 *
 * Gets the #PurpleProtocol identified by @id if found, otherwise %NULL.
 *
 * Returns: (transfer none): The #PurpleProtocol identified by @id or %NULL.
 *
 * Since: 3.0.0
 */
PurpleProtocol *purple_protocol_manager_find(PurpleProtocolManager *manager, const gchar *id);

/**
 * purple_protocol_manager_foreach:
 * @manager: The #PurpleProtocolManager instance.
 * @func: (scope call): The #PurpleProtocolManagerForeachFunc to call.
 * @data: User data to pass to @func.
 *
 * Calls @func for each #PurpleProtocol that @manager knows about.
 *
 * Since: 3.0.0
 */
void purple_protocol_manager_foreach(PurpleProtocolManager *manager, PurpleProtocolManagerForeachFunc func, gpointer data);

/**
 * purple_protocol_manager_get_all:
 * @manager: The #PurpleProtocolManager instance.
 *
 * Gets a sorted list of all #PurpleProtocols that are currently registered in
 * @manager.
 *
 * Returns: (transfer container) (element-type PurpleProtocol): The list
 *          containing all of the #PurpleProtocols registered with @manager.
 *
 * Since: 3.0.0
 */
GList *purple_protocol_manager_get_all(PurpleProtocolManager *manager);

G_END_DECLS

#endif /* PURPLE_PROTOCOL_MANAGER_H */
