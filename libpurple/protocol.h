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

#ifndef PURPLE_PROTOCOL_H
#define PURPLE_PROTOCOL_H

/**
 * SECTION:protocol
 * @section_id: libpurple-protocol
 * @short_description: <filename>protocol.h</filename>
 * @title: Protocol Object and Interfaces
 *
 * #PurpleProtocol is the base type for all protocols in libpurple.
 */

#define PURPLE_TYPE_PROTOCOL            (purple_protocol_get_type())
#define PURPLE_PROTOCOL(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), PURPLE_TYPE_PROTOCOL, PurpleProtocol))
#define PURPLE_PROTOCOL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), PURPLE_TYPE_PROTOCOL, PurpleProtocolClass))
#define PURPLE_IS_PROTOCOL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_PROTOCOL))
#define PURPLE_IS_PROTOCOL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), PURPLE_TYPE_PROTOCOL))
#define PURPLE_PROTOCOL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), PURPLE_TYPE_PROTOCOL, PurpleProtocolClass))

typedef struct _PurpleProtocol PurpleProtocol;
typedef struct _PurpleProtocolClass PurpleProtocolClass;

#include "account.h"
#include "buddyicon.h"
#include "buddylist.h"
#include "chat.h"
#include "connection.h"
#include "conversations.h"
#include "debug.h"
#include "xfer.h"
#include "image.h"
#include "notify.h"
#include "plugins.h"
#include "purpleaccountoption.h"
#include "purpleaccountusersplit.h"
#include "purplemessage.h"
#include "purplewhiteboard.h"
#include "purplewhiteboardops.h"
#include "roomlist.h"
#include "status.h"

/**
 * PurpleProtocol:
 * @id:              Protocol ID
 * @name:            Translated name of the protocol
 * @options:         Protocol options
 * @user_splits:     A GList of PurpleAccountUserSplit
 * @account_options: A GList of PurpleAccountOption
 * @icon_spec:       The icon spec.
 * @whiteboard_ops:  Whiteboard operations
 *
 * Represents an instance of a protocol registered with the protocols
 * subsystem. Protocols must initialize the members to appropriate values.
 */
struct _PurpleProtocol
{
	GObject gparent;

	/*< public >*/
	const char *id;
	const char *name;

	PurpleProtocolOptions options;

	GList *user_splits;
	GList *account_options;

	PurpleBuddyIconSpec *icon_spec;
	PurpleWhiteboardOps *whiteboard_ops;

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * PurpleProtocolClass:
 * @login:        Log in to the server.
 * @close:        Close connection with the server.
 * @status_types: Returns a list of #PurpleStatusType which exist for this
 *                account; and must add at least the offline and online states.
 * @list_icon:    Returns the base icon name for the given buddy and account. If
 *                buddy is %NULL and the account is non-%NULL, it will return
 *                the name to use for the account's icon. If both are %NULL, it
 *                will return the name to use for the protocol's icon.
 *
 * The base class for all protocols.
 *
 * All protocol types must implement the methods in this class.
 */
/* If adding new methods to this class, ensure you add checks for them in
   purple_protocols_add().
*/
struct _PurpleProtocolClass
{
	GObjectClass parent_class;

	void (*login)(PurpleAccount *account);

	void (*close)(PurpleConnection *connection);

	GList *(*status_types)(PurpleAccount *account);

	const char *(*list_icon)(PurpleAccount *account, PurpleBuddy *buddy);

	/*< private >*/
	void (*_purple_reserved1)(void);
	void (*_purple_reserved2)(void);
	void (*_purple_reserved3)(void);
	void (*_purple_reserved4)(void);
};

/**
 * PURPLE_PROTOCOL_IMPLEMENTS:
 * @protocol: The protocol in which to check
 * @IFACE:    The interface name in caps. e.g. <literal>CLIENT</literal>
 * @func:     The function to check
 *
 * Returns: %TRUE if a protocol implements a function in an interface,
 *          %FALSE otherwise.
 */
#define PURPLE_PROTOCOL_IMPLEMENTS(protocol, IFACE, func) \
	(PURPLE_IS_PROTOCOL_##IFACE(protocol) && \
	 PURPLE_PROTOCOL_##IFACE##_GET_IFACE(protocol)->func != NULL)

G_BEGIN_DECLS

/**************************************************************************/
/* Protocol Object API                                                    */
/**************************************************************************/

/**
 * purple_protocol_get_type:
 *
 * Returns: The #GType for #PurpleProtocol.
 */
GType purple_protocol_get_type(void);

/**
 * purple_protocol_get_id:
 * @protocol: The protocol.
 *
 * Returns the ID of a protocol.
 *
 * Returns: The ID of the protocol.
 */
const char *purple_protocol_get_id(const PurpleProtocol *protocol);

/**
 * purple_protocol_get_name:
 * @protocol: The protocol.
 *
 * Returns the translated name of a protocol.
 *
 * Returns: The translated name of the protocol.
 */
const char *purple_protocol_get_name(const PurpleProtocol *protocol);

/**
 * purple_protocol_get_options:
 * @protocol: The protocol.
 *
 * Returns the options of a protocol.
 *
 * Returns: The options of the protocol.
 */
PurpleProtocolOptions purple_protocol_get_options(const PurpleProtocol *protocol);

/**
 * purple_protocol_get_user_splits:
 * @protocol: The protocol.
 *
 * Returns the user splits of a protocol.
 *
 * Returns: (element-type PurpleAccountUserSplit) (transfer none): The user
 *          splits of the protocol.
 */
GList *purple_protocol_get_user_splits(const PurpleProtocol *protocol);

/**
 * purple_protocol_get_account_options:
 * @protocol: The protocol.
 *
 * Returns the account options for a protocol.
 *
 * Returns: (element-type PurpleAccountOption) (transfer none): The account
 *          options for the protocol.
 */
GList *purple_protocol_get_account_options(const PurpleProtocol *protocol);

/**
 * purple_protocol_get_icon_spec:
 * @protocol: The protocol.
 *
 * Returns the icon spec of a protocol.
 *
 * Returns: The icon spec of the protocol.
 */
PurpleBuddyIconSpec *purple_protocol_get_icon_spec(const PurpleProtocol *protocol);

/**
 * purple_protocol_get_whiteboard_ops:
 * @protocol: The protocol.
 *
 * Returns the whiteboard ops of a protocol.
 *
 * Returns: (transfer none): The whiteboard ops of the protocol.
 */
PurpleWhiteboardOps *purple_protocol_get_whiteboard_ops(const PurpleProtocol *protocol);

/**************************************************************************/
/* Protocol Class API                                                     */
/**************************************************************************/

void purple_protocol_class_login(PurpleProtocol *protocol, PurpleAccount *account);

void purple_protocol_class_close(PurpleProtocol *protocol, PurpleConnection *connection);

GList *purple_protocol_class_status_types(PurpleProtocol *protocol,
		PurpleAccount *account);

const char *purple_protocol_class_list_icon(PurpleProtocol *protocol,
		PurpleAccount *account, PurpleBuddy *buddy);

G_END_DECLS

#endif /* PURPLE_PROTOCOL_H */
