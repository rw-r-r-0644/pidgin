/*
 * Purple - Internet Messaging Library
 * Copyright (C) Pidgin Developers <devel@pidgin.im>
 *
 * Purple is the legal property of its developers, whose names are too numerous
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

#if !defined(PURPLE_GLOBAL_HEADER_INSIDE) && !defined(PURPLE_COMPILATION)
# error "only <purple.h> may be included directly"
#endif

#ifndef PURPLE_PROTOCOL_PRIVACY_H
#define PURPLE_PROTOCOL_PRIVACY_H

#include <glib.h>
#include <glib-object.h>

#include <libpurple/connection.h>
#include <libpurple/purpleprotocol.h>

/**
 * PURPLE_TYPE_PROTOCOL_PRIVACY:
 *
 * The standard _get_type method for #PurpleProtocolPrivacy.
 */
#define PURPLE_TYPE_PROTOCOL_PRIVACY (purple_protocol_privacy_get_type())

/**
 * PurpleProtocolPrivacy:
 *
 * #PurpleProtocolPrivacy describes the privacy API available to protocols.
 */
G_DECLARE_INTERFACE(PurpleProtocolPrivacy, purple_protocol_privacy, PURPLE,
                    PROTOCOL_PRIVACY, PurpleProtocol)

/**
 * PurpleProtocolPrivacyInterface:
 * @add_permit: Add the buddy on the required authorized list.
 * @add_deny: Add the buddy on the required blocked list.
 * @remove_permit: Remove the buddy from the required authorized list.
 * @remove_deny: Remove the buddy from the required blocked list.
 * @set_permit_deny: Update the server with the privacy information on the
 *                   permit and deny lists.
 *
 * The protocol privacy interface.
 *
 * This interface provides privacy callbacks such as to permit/deny users.
 */
struct _PurpleProtocolPrivacyInterface {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	void (*add_permit)(PurpleProtocolPrivacy *privacy, PurpleConnection *connection, const gchar *name);

	void (*add_deny)(PurpleProtocolPrivacy *privacy, PurpleConnection *connection, const gchar *name);

	void (*remove_permit)(PurpleProtocolPrivacy *privacy, PurpleConnection *connection, const gchar *name);

	void (*remove_deny)(PurpleProtocolPrivacy *privacy, PurpleConnection *connection, const gchar *name);

	void (*set_permit_deny)(PurpleProtocolPrivacy *privacy, PurpleConnection *connection);

	/*< private >*/
	gpointer reserved[4];
};

G_BEGIN_DECLS

/**
 * purple_protocol_privacy_add_permit:
 * @privacy: The #PurpleProtocolPrivacy instance.
 * @connection: The #PurpleConnection instance.
 * @name: The username to permit.
 *
 * Adds a permit to the privacy settings for @connection to allow @name to
 * contact the user.
 *
 * Since: 3.0.0
 */
void purple_protocol_privacy_add_permit(PurpleProtocolPrivacy *privacy, PurpleConnection *connection, const gchar *name);

/**
 * purple_protocol_privacy_add_deny:
 * @privacy: The #PurpleProtocolPrivacy instance.
 * @connection: The #PurpleConnection instance.
 * @name: The username to deny.
 *
 * Adds a deny to the privacy settings for @connection to deny @name from
 * contacting the user.
 *
 * Since: 3.0.0
 */
void purple_protocol_privacy_add_deny(PurpleProtocolPrivacy *privacy, PurpleConnection *connection, const gchar *name);

/**
 * purple_protocol_privacy_remove_permit:
 * @privacy: The #PurpleProtocolPrivacy instance.
 * @connection: The #PurpleConnection instance.
 * @name: The username to remove from the permit privacy settings.
 *
 * Removes an existing permit for @name.
 *
 * Since: 3.0.0
 */
void purple_protocol_privacy_remove_permit(PurpleProtocolPrivacy *privacy, PurpleConnection *connection, const gchar *name);

/**
 * purple_protocol_privacy_remove_deny:
 * @privacy: The #PurpleProtocolPrivacy instance.
 * @connection: The #PurpleConnection instance.
 * @name: The username to remove from the deny privacy settings.
 *
 * Removes an existing deny for @name.
 *
 * Since: 3.0.0
 */
void purple_protocol_privacy_remove_deny(PurpleProtocolPrivacy *privacy, PurpleConnection *connection, const gchar *name);

/**
 * purple_protocol_privacy_set_permit_deny:
 * @privacy: The #PurpleProtocolPrivacy instance.
 * @connection: The #PurpleConnection instance.
 *
 * Forces a sync of the privacy settings with server.
 *
 * Since: 3.0.0
 */
void purple_protocol_privacy_set_permit_deny(PurpleProtocolPrivacy *privacy, PurpleConnection *connection);

G_END_DECLS

#endif /* PURPLE_PROTOCOL_PRIVACY_H */