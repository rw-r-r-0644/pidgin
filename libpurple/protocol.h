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

#define PURPLE_TYPE_PROTOCOL_SERVER (purple_protocol_server_iface_get_type())

typedef struct _PurpleProtocolServerInterface PurpleProtocolServerInterface;

/**
 * PurpleProtocolServerInterface:
 * @register_user:   New user registration
 * @unregister_user: Remove the user from the server. The account can either be
 *                   connected or disconnected. After the removal is finished,
 *                   the connection will stay open and has to be closed!
 * @get_info:        Should arrange for purple_notify_userinfo() to be called
 *                   with @who's user info.
 * @add_buddy:       Add a buddy to a group on the server.
 *                   <sbr/>This protocol function may be called in situations in
 *                   which the buddy is already in the specified group. If the
 *                   protocol supports authorization and the user is not already
 *                   authorized to see the status of @buddy, @add_buddy should
 *                   request authorization.
 *                   <sbr/>If authorization is required, then use the supplied
 *                   invite message.
 * @keepalive:       If implemented, this will be called regularly for this
 *                   protocol's active connections. You'd want to do this if you
 *                   need to repeatedly send some kind of keepalive packet to
 *                   the server to avoid being disconnected. ("Regularly" is
 *                   defined to be 30 unless @get_keepalive_interval is
 *                   implemented to override it).
 *                   <filename>libpurple/connection.c</filename>.)
 * @get_keepalive_interval: If implemented, this will override the default
 *                          keepalive interval.
 * @alias_buddy:     Save/store buddy's alias on server list/roster
 * @group_buddy:     Change a buddy's group on a server list/roster
 * @rename_group:    Rename a group on a server list/roster
 * @set_buddy_icon:  Set the buddy icon for the given connection to @img. The
 *                   protocol does <emphasis>NOT</emphasis> own a reference to
 *                   @img; if it needs one, it must #g_object_ref(@img)
 *                   itself.
 * @send_raw:        For use in plugins that may understand the underlying
 *                   protocol
 * @set_public_alias: Set the user's "friendly name" (or alias or nickname or
 *                    whatever term you want to call it) on the server. The
 *                    protocol should call @success_cb or @failure_cb
 *                    <emphasis>asynchronously</emphasis> (if it knows
 *                    immediately that the set will fail, call one of the
 *                    callbacks from an idle/0-second timeout) depending on if
 *                    the nickname is set successfully. See
 *                    purple_account_set_public_alias().
 *                    <sbr/>@gc:    The connection for which to set an alias
 *                    <sbr/>@alias: The new server-side alias/nickname for this
 *                                  account, or %NULL to unset the
 *                                  alias/nickname (or return it to a
 *                                  protocol-specific "default").
 *                    <sbr/>@success_cb: Callback to be called if the public
 *                                       alias is set
 *                    <sbr/>@failure_cb: Callback to be called if setting the
 *                                       public alias fails
 * @get_public_alias: Retrieve the user's "friendly name" as set on the server.
 *                    The protocol should call @success_cb or @failure_cb
 *                    <emphasis>asynchronously</emphasis> (even if it knows
 *                    immediately that the get will fail, call one of the
 *                    callbacks from an idle/0-second timeout) depending on if
 *                    the nickname is retrieved. See
 *                    purple_account_get_public_alias().
 *                    <sbr/>@gc:         The connection for which to retireve
 *                                       the alias
 *                    <sbr/>@success_cb: Callback to be called with the
 *                                       retrieved alias
 *                    <sbr/>@failure_cb: Callback to be called if the protocol
 *                                       is unable to retrieve the alias
 *
 * The protocol server interface.
 *
 * This interface provides a gateway between purple and the protocol's server.
 */
struct _PurpleProtocolServerInterface
{
	/*< private >*/
	GTypeInterface parent_iface;

	/*< public >*/
	void (*register_user)(PurpleAccount *account);

	void (*unregister_user)(PurpleAccount *account, PurpleAccountUnregistrationCb cb,
							void *user_data);

	void (*set_info)(PurpleConnection *connection, const char *info);

	void (*get_info)(PurpleConnection *connection, const char *who);

	void (*set_status)(PurpleAccount *account, PurpleStatus *status);

	void (*set_idle)(PurpleConnection *connection, int idletime);

	void (*change_passwd)(PurpleConnection *connection, const char *old_pass,
						  const char *new_pass);

	void (*add_buddy)(PurpleConnection *pc, PurpleBuddy *buddy,
					  PurpleGroup *group, const char *message);

	void (*add_buddies)(PurpleConnection *pc, GList *buddies, GList *groups,
						const char *message);

	void (*remove_buddy)(PurpleConnection *connection, PurpleBuddy *buddy,
						 PurpleGroup *group);

	void (*remove_buddies)(PurpleConnection *connection, GList *buddies, GList *groups);

	void (*keepalive)(PurpleConnection *connection);

	int (*get_keepalive_interval)(void);

	void (*alias_buddy)(PurpleConnection *connection, const char *who,
						const char *alias);

	void (*group_buddy)(PurpleConnection *connection, const char *who,
					const char *old_group, const char *new_group);

	void (*rename_group)(PurpleConnection *connection, const char *old_name,
					 PurpleGroup *group, GList *moved_buddies);

	void (*set_buddy_icon)(PurpleConnection *connection, PurpleImage *img);

	void (*remove_group)(PurpleConnection *gc, PurpleGroup *group);

	int (*send_raw)(PurpleConnection *gc, const char *buf, int len);

	void (*set_public_alias)(PurpleConnection *gc, const char *alias,
	                         PurpleSetPublicAliasSuccessCallback success_cb,
	                         PurpleSetPublicAliasFailureCallback failure_cb);

	void (*get_public_alias)(PurpleConnection *gc,
	                         PurpleGetPublicAliasSuccessCallback success_cb,
	                         PurpleGetPublicAliasFailureCallback failure_cb);
};

#define PURPLE_IS_PROTOCOL_SERVER(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), PURPLE_TYPE_PROTOCOL_SERVER))
#define PURPLE_PROTOCOL_SERVER_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE((obj), PURPLE_TYPE_PROTOCOL_SERVER, \
                                               PurpleProtocolServerInterface))

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

/**************************************************************************/
/* Protocol Server Interface API                                          */
/**************************************************************************/

/**
 * purple_protocol_server_iface_get_type:
 *
 * Returns: The #GType for the protocol server interface.
 */
GType purple_protocol_server_iface_get_type(void);

void purple_protocol_server_iface_register_user(PurpleProtocol *protocol,
		PurpleAccount *account);

/**
 * purple_protocol_server_iface_unregister_user:
 * @cb: (scope call):
 */
void purple_protocol_server_iface_unregister_user(PurpleProtocol *protocol,
		PurpleAccount *account, PurpleAccountUnregistrationCb cb, void *user_data);

void purple_protocol_server_iface_set_info(PurpleProtocol *protocol, PurpleConnection *connection,
		const char *info);

void purple_protocol_server_iface_get_info(PurpleProtocol *protocol, PurpleConnection *connection,
		const char *who);

void purple_protocol_server_iface_set_status(PurpleProtocol *protocol,
		PurpleAccount *account, PurpleStatus *status);

void purple_protocol_server_iface_set_idle(PurpleProtocol *protocol, PurpleConnection *connection,
		int idletime);

void purple_protocol_server_iface_change_passwd(PurpleProtocol *protocol,
		PurpleConnection *connection, const char *old_pass, const char *new_pass);

void purple_protocol_server_iface_add_buddy(PurpleProtocol *protocol,
		PurpleConnection *pc, PurpleBuddy *buddy, PurpleGroup *group,
		const char *message);

void purple_protocol_server_iface_add_buddies(PurpleProtocol *protocol,
		PurpleConnection *pc, GList *buddies, GList *groups,
		const char *message);

void purple_protocol_server_iface_remove_buddy(PurpleProtocol *protocol,
		PurpleConnection *connection, PurpleBuddy *buddy, PurpleGroup *group);

void purple_protocol_server_iface_remove_buddies(PurpleProtocol *protocol,
		PurpleConnection *connection, GList *buddies, GList *groups);

void purple_protocol_server_iface_keepalive(PurpleProtocol *protocol,
		PurpleConnection *connection);

int purple_protocol_server_iface_get_keepalive_interval(PurpleProtocol *protocol);

void purple_protocol_server_iface_alias_buddy(PurpleProtocol *protocol,
		PurpleConnection *connection, const char *who, const char *alias);

void purple_protocol_server_iface_group_buddy(PurpleProtocol *protocol,
		PurpleConnection *connection, const char *who, const char *old_group,
		const char *new_group);

void purple_protocol_server_iface_rename_group(PurpleProtocol *protocol,
		PurpleConnection *connection, const char *old_name, PurpleGroup *group,
		GList *moved_buddies);

void purple_protocol_server_iface_set_buddy_icon(PurpleProtocol *protocol,
		PurpleConnection *connection, PurpleImage *img);

void purple_protocol_server_iface_remove_group(PurpleProtocol *protocol,
		PurpleConnection *gc, PurpleGroup *group);

int purple_protocol_server_iface_send_raw(PurpleProtocol *protocol,
		PurpleConnection *gc, const char *buf, int len);

/**
 * purple_protocol_server_iface_set_public_alias:
 * @success_cb: (scope call):
 * @failure_cb: (scope call):
 */
void purple_protocol_server_iface_set_public_alias(PurpleProtocol *protocol,
		PurpleConnection *gc, const char *alias,
		PurpleSetPublicAliasSuccessCallback success_cb,
		PurpleSetPublicAliasFailureCallback failure_cb);

/**
 * purple_protocol_server_iface_get_public_alias:
 * @success_cb: (scope call):
 * @failure_cb: (scope call):
 */
void purple_protocol_server_iface_get_public_alias(PurpleProtocol *protocol,
		PurpleConnection *gc, PurpleGetPublicAliasSuccessCallback success_cb,
		PurpleGetPublicAliasFailureCallback failure_cb);

/**************************************************************************/
/* Protocol Chat Interface API                                            */
/**************************************************************************/

G_END_DECLS

#endif /* PURPLE_PROTOCOL_H */
