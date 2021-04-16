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

#ifndef PURPLE_CONVERSATION_H
#define PURPLE_CONVERSATION_H

/**
 * SECTION:purpleconversation
 * @section_id: libpurple-purpleconversation
 * @short_description: Conversation base class
 * @title: Conversation Base Class
 *
 * This is the base class for the conversation abstraction on Libpurple.
 *
 * The present API manages the common functionallity for both kinds of conversations
 * @PurpleIMConversation and @PurpleChatConversation.
 */

#include <glib.h>
#include <glib-object.h>

#include <purplemessage.h>

#define PURPLE_TYPE_CONVERSATION (purple_conversation_get_type())
G_DECLARE_DERIVABLE_TYPE(PurpleConversation, purple_conversation, PURPLE,
                         CONVERSATION, GObject)

/**
 * PurpleConversationUpdateType:
 * @PURPLE_CONVERSATION_UPDATE_ADD: The buddy associated with the conversation
 *                                  was added.
 * @PURPLE_CONVERSATION_UPDATE_REMOVE: The buddy associated with the
 *                                     conversation was removed.
 * @PURPLE_CONVERSATION_UPDATE_ACCOUNT: The purple_account was changed.
 * @PURPLE_CONVERSATION_UPDATE_TYPING: The typing state was updated.
 * @PURPLE_CONVERSATION_UPDATE_UNSEEN: The unseen state was updated.
 * @PURPLE_CONVERSATION_UPDATE_LOGGING: Logging for this conversation was
 *                                      enabled or disabled.
 * @PURPLE_CONVERSATION_UPDATE_TOPIC: The topic for a chat was updated.
 * @PURPLE_CONVERSATION_ACCOUNT_ONLINE: One of the user's accounts went online.
 * @PURPLE_CONVERSATION_ACCOUNT_OFFLINE: One of the user's accounts went
 *                                       offline.
 * @PURPLE_CONVERSATION_UPDATE_AWAY: The other user went away.
 * @PURPLE_CONVERSATION_UPDATE_ICON: The other user's buddy icon changed.
 * @PURPLE_CONVERSATION_UPDATE_NAME: The name of the conversation was changed.
 * @PURPLE_CONVERSATION_UPDATE_TITLE: The title of the conversation was
 *                                    updated.
 * @PURPLE_CONVERSATION_UPDATE_CHATLEFT: The user left a chat.
 * @PURPLE_CONVERSATION_UPDATE_FEATURES: The features for a chat have changed.
 *
 * Conversation update type.
 */
typedef enum
{
	PURPLE_CONVERSATION_UPDATE_ADD = 0,
	PURPLE_CONVERSATION_UPDATE_REMOVE,
	PURPLE_CONVERSATION_UPDATE_ACCOUNT,
	PURPLE_CONVERSATION_UPDATE_TYPING,
	PURPLE_CONVERSATION_UPDATE_UNSEEN,
	PURPLE_CONVERSATION_UPDATE_LOGGING,
	PURPLE_CONVERSATION_UPDATE_TOPIC,

	/*
	 * XXX These need to go when we implement a more generic core/UI event
	 * system.
	 */
	PURPLE_CONVERSATION_ACCOUNT_ONLINE,
	PURPLE_CONVERSATION_ACCOUNT_OFFLINE,
	PURPLE_CONVERSATION_UPDATE_AWAY,
	PURPLE_CONVERSATION_UPDATE_ICON,
	PURPLE_CONVERSATION_UPDATE_NAME,
	PURPLE_CONVERSATION_UPDATE_TITLE,
	PURPLE_CONVERSATION_UPDATE_CHATLEFT,

	PURPLE_CONVERSATION_UPDATE_FEATURES

} PurpleConversationUpdateType;

/**
 * PurpleConversation:
 *
 * A core representation of a conversation between two or more people.
 *
 * The conversation can be an IM or a chat.
 *
 * Note: When a conversation is destroyed with the last g_object_unref(), the
 *       specified conversation is removed from the parent window. If this
 *       conversation is the only one contained in the parent window, that
 *       window is also destroyed.
 */

/**
 * PurpleConversationClass:
 * @write_message: Writes a message to a chat or IM conversation. See
 *                 purple_conversation_write_message().
 *
 * Base class for all #PurpleConversation's
 */
struct _PurpleConversationClass {
	/*< private >*/
	GObjectClass parent_class;

	/*< public >*/
	void (*write_message)(PurpleConversation *conv, PurpleMessage *msg);

	/*< private >*/
	gpointer reserved[4];
};

#include <purpleconversationuiops.h>
#include "account.h"
#include "buddyicon.h"
#include "log.h"
#include "smiley-list.h"

G_BEGIN_DECLS

/**
 * purple_conversation_get_type:
 *
 * Returns: The #GType for the Conversation object.
 */

/**
 * purple_conversation_present:
 * @conv: The conversation to present
 *
 * Present a conversation to the user. This allows core code to initiate a
 * conversation by displaying the IM dialog.
 */
void purple_conversation_present(PurpleConversation *conv);

/**
 * purple_conversation_set_ui_ops:
 * @conv: The conversation.
 * @ops:  The UI conversation operations structure.
 *
 * Sets the specified conversation's UI operations structure.
 */
void purple_conversation_set_ui_ops(PurpleConversation *conv, PurpleConversationUiOps *ops);

/**
 * purple_conversation_get_ui_ops:
 * @conv: The conversation.
 *
 * Returns the specified conversation's UI operations structure.
 *
 * Returns: The operations structure.
 */
PurpleConversationUiOps *purple_conversation_get_ui_ops(PurpleConversation *conv);

/**
 * purple_conversation_set_account:
 * @conv: The conversation.
 * @account: The purple_account.
 *
 * Sets the specified conversation's purple_account.
 *
 * This purple_account represents the user using purple, not the person the user
 * is having a conversation/chat/flame with.
 */
void purple_conversation_set_account(PurpleConversation *conv, PurpleAccount *account);

/**
 * purple_conversation_get_account:
 * @conv: The conversation.
 *
 * Returns the specified conversation's purple_account.
 *
 * This purple_account represents the user using purple, not the person the user
 * is having a conversation/chat/flame with.
 *
 * Returns: (transfer none): The conversation's purple_account.
 */
PurpleAccount *purple_conversation_get_account(PurpleConversation *conv);

/**
 * purple_conversation_get_connection:
 * @conv: The conversation.
 *
 * Returns the specified conversation's purple_connection.
 *
 * Returns: (transfer none): The conversation's purple_connection.
 */
PurpleConnection *purple_conversation_get_connection(PurpleConversation *conv);

/**
 * purple_conversation_set_title:
 * @conv:  The conversation.
 * @title: The title.
 *
 * Sets the specified conversation's title.
 */
void purple_conversation_set_title(PurpleConversation *conv, const gchar *title);

/**
 * purple_conversation_get_title:
 * @conv: The conversation.
 *
 * Returns the specified conversation's title.
 *
 * Returns: The title.
 */
const char *purple_conversation_get_title(PurpleConversation *conv);

/**
 * purple_conversation_autoset_title:
 * @conv: The conversation.
 *
 * Automatically sets the specified conversation's title.
 *
 * This function takes OPT_IM_ALIAS_TAB into account, as well as the
 * user's alias.
 */
void purple_conversation_autoset_title(PurpleConversation *conv);

/**
 * purple_conversation_set_name:
 * @conv: The conversation.
 * @name: The conversation's name.
 *
 * Sets the specified conversation's name.
 */
void purple_conversation_set_name(PurpleConversation *conv, const gchar *name);

/**
 * purple_conversation_get_name:
 * @conv: The conversation.
 *
 * Returns the specified conversation's name.
 *
 * Returns: The conversation's name. If the conversation is an IM with a
 *          PurpleBuddy, then it's the name of the PurpleBuddy.
 */
const char *purple_conversation_get_name(PurpleConversation *conv);

/**
 * purple_conversation_set_logging:
 * @conv: The conversation.
 * @log:  %TRUE if logging should be enabled, or %FALSE otherwise.
 *
 * Enables or disables logging for this conversation.
 */
void purple_conversation_set_logging(PurpleConversation *conv, gboolean log);

/**
 * purple_conversation_is_logging:
 * @conv: The conversation.
 *
 * Returns whether or not logging is enabled for this conversation.
 *
 * Returns: %TRUE if logging is enabled, or %FALSE otherwise.
 */
gboolean purple_conversation_is_logging(PurpleConversation *conv);

/**
 * purple_conversation_close_logs:
 * @conv: The conversation.
 *
 * Closes any open logs for this conversation.
 *
 * Note that new logs will be opened as necessary (e.g. upon receipt of a
 * message, if the conversation has logging enabled. To disable logging for
 * the remainder of the conversation, use purple_conversation_set_logging().
 */
void purple_conversation_close_logs(PurpleConversation *conv);

/**
 * purple_conversation_write_message:
 * @conv: The conversation.
 * @msg:  The message to write.
 *
 * Writes to a chat or an IM.
 */
void purple_conversation_write_message(PurpleConversation *conv, PurpleMessage *msg);

/**
 * purple_conversation_write_system_message:
 * @conv:    The conversation.
 * @message: The message to write.
 * @flags:   The message flags (you don't need to set %PURPLE_MESSAGE_SYSTEM.
 *
 * Wites a system message to a chat or an IM.
 */
void purple_conversation_write_system_message(PurpleConversation *conv, const gchar *message, PurpleMessageFlags flags);

/**
 * purple_conversation_send:
 * @conv:    The conversation.
 * @message: The message to send.
 *
 * Sends a message to this conversation. This function calls
 * purple_conversation_send_with_flags() with no additional flags.
 */
void purple_conversation_send(PurpleConversation *conv, const gchar *message);

/**
 * purple_conversation_send_with_flags:
 * @conv:    The conversation.
 * @message: The message to send.
 * @flags:   The PurpleMessageFlags flags to use in addition to
 *           PURPLE_MESSAGE_SEND.
 *
 * Sends a message to this conversation with specified flags.
 */
void purple_conversation_send_with_flags(PurpleConversation *conv, const gchar *message, PurpleMessageFlags flags);

/**
 * purple_conversation_set_features:
 * @conv:      The conversation
 * @features:  Bitset defining supported features
 *
 * Set the features as supported for the given conversation.
 */
void purple_conversation_set_features(PurpleConversation *conv, PurpleConnectionFlags features);

/**
 * purple_conversation_get_features:
 * @conv:  The conversation
 *
 * Get the features supported by the given conversation.
 */
PurpleConnectionFlags purple_conversation_get_features(PurpleConversation *conv);

/**
 * purple_conversation_has_focus:
 * @conv:    The conversation.
 *
 * Determines if a conversation has focus
 *
 * Returns: %TRUE if the conversation has focus, %FALSE if
 * it does not or the UI does not have a concept of conversation focus
 */
gboolean purple_conversation_has_focus(PurpleConversation *conv);

/**
 * purple_conversation_update:
 * @conv: The conversation.
 * @type: The update type.
 *
 * Updates the visual status and UI of a conversation.
 */
void purple_conversation_update(PurpleConversation *conv, PurpleConversationUpdateType type);

/**
 * purple_conversation_get_message_history:
 * @conv:   The conversation
 *
 * Retrieve the message history of a conversation.
 *
 * Returns: (element-type PurpleMessage) (transfer none):
 *          A GList of PurpleMessage's. You must not modify the
 *          list or the data within. The list contains the newest message at
 *          the beginning, and the oldest message at the end.
 */
GList *purple_conversation_get_message_history(PurpleConversation *conv);

/**
 * purple_conversation_clear_message_history:
 * @conv:  The conversation
 *
 * Clear the message history of a conversation.
 */
void purple_conversation_clear_message_history(PurpleConversation *conv);

/**
 * purple_conversation_send_confirm:
 * @conv:    The conversation.
 * @message: The message to send.
 *
 * Sends a message to a conversation after confirming with
 * the user.
 *
 * This function is intended for use in cases where the user
 * hasn't explicitly and knowingly caused a message to be sent.
 * The confirmation ensures that the user isn't sending a
 * message by mistake.
 */
void purple_conversation_send_confirm(PurpleConversation *conv, const gchar *message);

/**
 * purple_conversation_get_extended_menu:
 * @conv: The conversation.
 *
 * Retrieves the extended menu items for the conversation.
 *
 * Returns: (element-type PurpleActionMenu) (transfer full): The extended menu
 *          items for a conversation, as harvested by the chat-extended-menu
 *          signal.
 */
GList * purple_conversation_get_extended_menu(PurpleConversation *conv);

/**
 * purple_conversation_do_command:
 * @conv:    The conversation.
 * @cmdline: The entire command including the arguments.
 * @markup:  %NULL, or the formatted command line.
 * @error:   If the command failed errormsg is filled in with the appropriate error
 *                message, if not %NULL. It must be freed by the caller with g_free().
 *
 * Perform a command in a conversation. Similar to purple_cmd_do_command().
 *
 * Returns:  %TRUE if the command was executed successfully, %FALSE otherwise.
 */
gboolean purple_conversation_do_command(PurpleConversation *conv, const gchar *cmdline, const gchar *markup, gchar **error);

/**
 * purple_conversation_get_max_message_size:
 * @conv: The conversation to query.
 *
 * Gets the maximum message size in bytes for the conversation.
 *
 * See #PurpleProtocolClientInterface's <literal>get_max_message_size</literal>.
 *
 * Returns: Maximum message size, 0 if unspecified, -1 for infinite.
 */
gssize purple_conversation_get_max_message_size(PurpleConversation *conv);

/**
 * purple_conversation_add_smiley:
 * @conv: The conversation that receives new smiley.
 * @smiley: The smiley.
 *
 * Adds a smiley to the list of smileys that belong to this conversation.
 */
void purple_conversation_add_smiley(PurpleConversation *conv, PurpleSmiley *smiley);

/**
 * purple_conversation_get_smiley:
 * @conv: The conversation.
 * @shortcut: The shortcut.
 *
 * Lookups for the smiley previously added to this conversation.
 *
 * Returns: (transfer none): The smiley, or %NULL if it doesn't exists.
 */
PurpleSmiley *purple_conversation_get_smiley(PurpleConversation *conv, const gchar *shortcut);

/**
 * purple_conversation_get_remote_smileys:
 * @conv: The conversation.
 *
 * Get all remote smileys previously added to this conversation.
 *
 * Returns: (transfer none): The list of remote smileys.
 */
PurpleSmileyList *purple_conversation_get_remote_smileys(PurpleConversation *conv);

/**
 * purple_conversation_present_error:
 * @who:     The user this error is about
 * @account: The account this error is on
 * @what:    The error
 *
 * Presents an IM-error to the user
 *
 * This is a helper function to find a conversation, write an error to it, and
 * raise the window.  If a conversation with this user doesn't already exist,
 * the function will return FALSE and the calling function can attempt to present
 * the error another way (purple_notify_error, most likely)
 *
 * Returns:        TRUE if the error was presented, else FALSE
 */
gboolean purple_conversation_present_error(const gchar *who, PurpleAccount *account, const gchar *what);

G_END_DECLS

#endif /* PURPLE_CONVERSATION_H */
