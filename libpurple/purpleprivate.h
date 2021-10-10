/*
 * purple
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined(PURPLE_GLOBAL_HEADER_INSIDE) && !defined(PURPLE_COMPILATION)
# error "only <purple.h> may be included directly"
#endif

#ifndef PURPLE_PRIVATE_H
#define PURPLE_PRIVATE_H

#include <glib.h>
#include <glib/gstdio.h>

#include "accounts.h"
#include "connection.h"
#include "purplecredentialprovider.h"

#define PURPLE_STATIC_ASSERT(condition, message) \
	{ typedef char static_assertion_failed_ ## message \
	[(condition) ? 1 : -1]; static_assertion_failed_ ## message dummy; \
	(void)dummy; }

G_BEGIN_DECLS

/**
 * _purple_account_set_current_error:
 * @account:  The account to set the error for.
 * @new_err:  The #PurpleConnectionErrorInfo instance representing the
 *                 error.
 *
 * Sets an error for an account.
 */
void _purple_account_set_current_error(PurpleAccount *account,
                                       PurpleConnectionErrorInfo *new_err);

/**
 * _purple_account_to_xmlnode:
 * @account:  The account
 *
 * Get an XML description of an account.
 *
 * Returns:  The XML description of the account.
 */
PurpleXmlNode *_purple_account_to_xmlnode(PurpleAccount *account);

/**
 * _purple_blist_get_last_child:
 * @node:  The node whose last child is to be retrieved.
 *
 * Returns the last child of a particular node.
 *
 * Returns: The last child of the node.
 */
PurpleBlistNode *_purple_blist_get_last_child(PurpleBlistNode *node);

/* This is for the accounts code to notify the buddy icon code that
 * it's done loading.  We may want to replace this with a signal. */
void
_purple_buddy_icons_account_loaded_cb(void);

/* This is for the buddy list to notify the buddy icon code that
 * it's done loading.  We may want to replace this with a signal. */
void
_purple_buddy_icons_blist_loaded_cb(void);

/**
 * _purple_connection_new:
 * @account:  The account the connection should be connecting to.
 * @is_registration: Whether we are registering a new account or just trying to
 *                   do a normal signon.
 * @password: The password to use.
 *
 * Creates a connection to the specified account and either connects
 * or attempts to register a new account.  If you are logging in,
 * the connection uses the current active status for this account.
 * So if you want to sign on as "away," for example, you need to
 * have called purple_account_set_status(account, "away").
 * (And this will call purple_account_connect() automatically).
 *
 * Note: This function should only be called by purple_account_connect()
 *       in account.c.  If you're trying to sign on an account, use that
 *       function instead.
 */
void _purple_connection_new(PurpleAccount *account, gboolean is_registration,
                            const gchar *password);
/**
 * _purple_connection_new_unregister:
 * @account:  The account to unregister
 * @password: The password to use.
 * @cb: Optional callback to be called when unregistration is complete
 * @user_data: user data to pass to the callback
 *
 * Tries to unregister the account on the server. If the account is not
 * connected, also creates a new connection.
 *
 * Note: This function should only be called by purple_account_unregister()
 *       in account.c.
 */
void _purple_connection_new_unregister(PurpleAccount *account, const char *password,
                                       PurpleAccountUnregistrationCb cb, void *user_data);
/**
 * _purple_connection_wants_to_die:
 * @gc:  The connection to check
 *
 * Checks if a connection is disconnecting, and should not attempt to reconnect.
 *
 * Note: This function should only be called by purple_account_set_enabled()
 *       in account.c.
 */
gboolean _purple_connection_wants_to_die(PurpleConnection *gc);

/**
 * _purple_connection_add_active_chat:
 * @gc:    The connection
 * @chat:  The chat conversation to add
 *
 * Adds a chat to the active chats list of a connection
 *
 * Note: This function should only be called by purple_serv_got_joined_chat()
 *       in server.c.
 */
void _purple_connection_add_active_chat(PurpleConnection *gc,
                                        PurpleChatConversation *chat);
/**
 * _purple_connection_remove_active_chat:
 * @gc:    The connection
 * @chat:  The chat conversation to remove
 *
 * Removes a chat from the active chats list of a connection
 *
 * Note: This function should only be called by purple_serv_got_chat_left()
 *       in server.c.
 */
void _purple_connection_remove_active_chat(PurpleConnection *gc,
                                           PurpleChatConversation *chat);

/**
 * _purple_statuses_get_primitive_scores:
 *
 * Note: This function should only be called by
 *       purple_buddy_presence_compute_score() in presence.c.
 *
 * Returns: The primitive scores array from status.c.
 */
int *_purple_statuses_get_primitive_scores(void);

/**
 * _purple_conversation_write_common:
 * @conv:    The conversation.
 * @msg:     The message.
 *
 * Writes to a conversation window.
 *
 * This function should not be used to write IM or chat messages. Use
 * purple_conversation_write_message() instead. This function will
 * most likely call this anyway, but it may do it's own formatting,
 * sound playback, etc. depending on whether the conversation is a chat or an
 * IM.
 *
 * See purple_conversation_write_message().
 */
void
_purple_conversation_write_common(PurpleConversation *conv, PurpleMessage *msg);

/**
 * purple_conversation_manager_startup:
 *
 * Starts up the conversation manager by creating the default instance.
 *
 * Since: 3.0.0
 */
void purple_conversation_manager_startup(void);

/**
 * purple_conversation_manager_shutdown:
 *
 * Shuts down the conversation manager by destroying the default instance.
 *
 * Since: 3.0.0
 */
void purple_conversation_manager_shutdown(void);

/**
 * purple_credential_manager_startup:
 *
 * Starts up the credential manager by creating the default instance.
 *
 * Since: 3.0.0
 */
void purple_credential_manager_startup(void);

/**
 * purple_credential_manager_shutdown:
 *
 * Shuts down the credential manager by destroying the default instance.
 *
 * Since: 3.0.0
 */
void purple_credential_manager_shutdown(void);

/**
 * purple_protocol_manager_startup:
 *
 * Starts up the protocol manager by creating the default instance.
 *
 * Since: 3.0.0
 */
void purple_protocol_manager_startup(void);

/**
 * purple_protocol_manager_shutdown:
 *
 * Shuts down the protocol manager by destroying the default instance.
 *
 * Since: 3.0.0
 */
void purple_protocol_manager_shutdown(void);

/**
 * purple_credential_provider_activate:
 * @provider: The #PurpleCredentialProvider instance.
 *
 * Tells a @provider that it has become the active provider.
 *
 * Since: 3.0.0
 */
void purple_credential_provider_activate(PurpleCredentialProvider *provider);

/**
 * purple_credential_provider_deactivate:
 * @provider: The #PurpleCredentialProvider instance.
 *
 * Tells @provider that another #PurpleCredentialProvider has become the active
 * provider.
 *
 * Since: 3.0.0
 */
void purple_credential_provider_deactivate(PurpleCredentialProvider *provider);

/**
 * purple_whiteboard_manager_startup:
 *
 * Starts up the whiteboard manager by creating the default instance.
 *
 * Since: 3.0.0
 */
void purple_whiteboard_manager_startup(void);

/**
 * purple_whiteboard_manager_shutdown:
 *
 * Shuts down the whiteboard manager by destroying the default instance.
 *
 * Since: 3.0.0
 */
void purple_whiteboard_manager_shutdown(void);


G_END_DECLS

#endif /* PURPLE_PRIVATE_H */

