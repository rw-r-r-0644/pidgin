/* purple
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#if !defined(PURPLE_GLOBAL_HEADER_INSIDE) && !defined(PURPLE_COMPILATION)
# error "only <purple.h> may be included directly"
#endif

#ifndef PURPLE_CONVERSATIONS_H
#define PURPLE_CONVERSATIONS_H
/**
 * SECTION:conversations
 * @section_id: libpurple-conversations
 * @short_description: <filename>conversations.h</filename>
 * @title: Conversations Subsystem API
 * @see_also: <link linkend="chapter-signals-conversation">Conversation signals</link>
 */

#include <purpleconversation.h>
#include "server.h"

G_BEGIN_DECLS

/**************************************************************************/
/* Conversations Subsystem                                                */
/**************************************************************************/

/**
 * purple_conversations_add:
 * @conv: The conversation.
 *
 * Adds a conversation to the list of conversations.
 */
G_DEPRECATED_FOR(purple_conversation_manager_register)
void purple_conversations_add(PurpleConversation *conv);

/**
 * purple_conversations_remove:
 * @conv: The conversation.
 *
 * Removes a conversation from the list of conversations.
 */
G_DEPRECATED_FOR(purple_conversation_manager_unregister)
void purple_conversations_remove(PurpleConversation *conv);

/**
 * purple_conversations_get_all:
 *
 * Returns a list of all conversations.
 *
 * This list includes both IMs and chats.
 *
 * Returns: (element-type PurpleConversation) (transfer none): A GList of all conversations.
 */
G_DEPRECATED_FOR(purple_conversation_manager_get_all)
GList *purple_conversations_get_all(void);

/**
 * purple_conversations_find_with_account:
 * @name: The name of the conversation.
 * @account: The account associated with the conversation.
 *
 * Finds a conversation of any type with the specified name and Purple account.
 *
 * Returns: (transfer none): The conversation if found, or %NULL otherwise.
 */
G_DEPRECATED_FOR(purple_conversation_manager_find)
PurpleConversation *purple_conversations_find_with_account(const char *name,
		PurpleAccount *account);

/**
 * purple_conversations_find_im_with_account:
 * @name: The name of the conversation.
 * @account: The account associated with the conversation.
 *
 * Finds an IM with the specified name and Purple account.
 *
 * Returns: (transfer none): The conversation if found, or %NULL otherwise.
 */
G_DEPRECATED_FOR(purple_conversation_manager_find_im)
PurpleConversation *purple_conversations_find_im_with_account(const char *name, PurpleAccount *account);

/**
 * purple_conversations_find_chat_with_account:
 * @name: The name of the conversation.
 * @account: The account associated with the conversation.
 *
 * Finds a chat with the specified name and Purple account.
 *
 * Returns: (transfer none): The conversation if found, or %NULL otherwise.
 */
G_DEPRECATED_FOR(purple_conversation_manager_find_chat)
PurpleConversation *purple_conversations_find_chat_with_account(const char *name, PurpleAccount *account);

/**
 * purple_conversations_find_chat:
 * @gc: The purple_connection.
 * @id: The chat ID.
 *
 * Finds a chat with the specified chat ID.
 *
 * Returns: (transfer none): The chat conversation.
 */
G_DEPRECATED_FOR(purple_conversation_manager_find_chat_by_id)
PurpleConversation *purple_conversations_find_chat(PurpleConnection *gc, int id);

/**
 * purple_conversations_set_ui_ops:
 * @ops:  The UI conversation operations structure.
 *
 * Sets the default conversation UI operations structure.
 */
void purple_conversations_set_ui_ops(PurpleConversationUiOps *ops);

/**
 * purple_conversations_get_ui_ops:
 *
 * Gets the default conversation UI operations structure.
 *
 * Returns:  The UI conversation operations structure.
 */
PurpleConversationUiOps *purple_conversations_get_ui_ops(void);

/**
 * purple_conversations_get_handle:
 *
 * Returns the conversation subsystem handle.
 *
 * Returns: The conversation subsystem handle.
 */
void *purple_conversations_get_handle(void);

/**
 * purple_conversations_init:
 *
 * Initializes the conversation subsystem.
 */
void purple_conversations_init(void);

/**
 * purple_conversations_uninit:
 *
 * Uninitializes the conversation subsystem.
 */
void purple_conversations_uninit(void);

G_END_DECLS

#endif /* PURPLE_CONVERSATIONS_H */
