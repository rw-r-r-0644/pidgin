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

#ifndef _PIDGIN_CONVERSATION_H_
#define _PIDGIN_CONVERSATION_H_

typedef struct _PidginImPane       PidginImPane;
typedef struct _PidginChatPane     PidginChatPane;
typedef struct _PidginConversation PidginConversation;

/**
 * PidginUnseenState:
 * @PIDGIN_UNSEEN_NONE:   No unseen text in the conversation.
 * @PIDGIN_UNSEEN_EVENT:  Unseen events in the conversation.
 * @PIDGIN_UNSEEN_NO_LOG: Unseen text with NO_LOG flag.
 * @PIDGIN_UNSEEN_TEXT:   Unseen text in the conversation.
 * @PIDGIN_UNSEEN_NICK:   Unseen text and the nick was said.
 *
 * Unseen text states.
 */
typedef enum
{
	PIDGIN_UNSEEN_NONE,
	PIDGIN_UNSEEN_EVENT,
	PIDGIN_UNSEEN_NO_LOG,
	PIDGIN_UNSEEN_TEXT,
	PIDGIN_UNSEEN_NICK
} PidginUnseenState;

enum {
	CHAT_USERS_ICON_COLUMN,
	CHAT_USERS_ALIAS_COLUMN,
	CHAT_USERS_ALIAS_KEY_COLUMN,
	CHAT_USERS_NAME_COLUMN,
	CHAT_USERS_FLAGS_COLUMN,
	CHAT_USERS_COLOR_COLUMN,
	CHAT_USERS_WEIGHT_COLUMN,
	CHAT_USERS_ICON_NAME_COLUMN,
	CHAT_USERS_COLUMNS
};

#define PIDGIN_CONVERSATION(conv) \
	((PidginConversation *)g_object_get_data(G_OBJECT(conv), "pidgin"))

#define PIDGIN_IS_PIDGIN_CONVERSATION(conv) \
	(purple_conversation_get_ui_ops(conv) == \
	 pidgin_conversations_get_conv_ui_ops())

#include <purple.h>

/**************************************************************************
 * Structures
 **************************************************************************/

/**
 * PidginConversation:
 *
 * A GTK conversation pane.
 */
struct _PidginConversation
{
	PurpleConversation *active_conv;
	GList *convs;
	GList *send_history;

	GtkWidget *tab_cont;

	PurpleMessageFlags last_flags;
	GtkWidget *history_sw;
	GtkWidget *history;

	GtkWidget *editor;
	GtkWidget *entry;

	PidginUnseenState unseen_state;
	guint unseen_count;

	union
	{
		PidginImPane   *im;
		PidginChatPane *chat;

	} u;

	time_t newday;
	GtkWidget *infopane;

	/* Used when attaching a PidginConversation to a PurpleConversation
	 * with message history */
	int attach_timer;
	GList *attach_current;
};

G_BEGIN_DECLS

/**************************************************************************
 * GTK Conversation API
 **************************************************************************/

/**
 * pidgin_conversations_get_conv_ui_ops:
 *
 * Returns the UI operations structure for GTK conversations.
 *
 * Returns: The GTK conversation operations structure.
 */
PurpleConversationUiOps *pidgin_conversations_get_conv_ui_ops(void);

/**
 * pidgin_conv_switch_active_conversation:
 * @conv: The conversation
 *
 * Sets the active conversation within a GTK-conversation.
 */
void pidgin_conv_switch_active_conversation(PurpleConversation *conv);

/**
 * pidgin_conv_update_buttons_by_protocol:
 * @conv: The conversation.
 *
 * Updates conversation buttons by protocol.
 */
void pidgin_conv_update_buttons_by_protocol(PurpleConversation *conv);

/**
 * pidgin_conversations_get_unseen_all:
 * @min_state:    The minimum unseen state.
 * @max_count:    Maximum number of conversations to return, or 0 for
 *                no maximum.
 *
 * Returns a list of conversations of any type which have an unseen
 * state greater than or equal to the specified minimum state. Using the
 * hidden_only parameter, this search can be limited to hidden
 * conversations. The max_count parameter will limit the total number of
 * converations returned if greater than zero. The returned list should
 * be freed by the caller.
 *
 * Returns: (transfer container) (element-type PurpleConversation): List of PurpleConversation matching criteria, or %NULL.
 */
GList *
pidgin_conversations_get_unseen_all(PidginUnseenState min_state, guint max_count);

/**
 * pidgin_conv_present_conversation:
 * @conv: The conversation.
 *
 * Presents a purple conversation to the user.
 */
void pidgin_conv_present_conversation(PurpleConversation *conv);

/**
 * pidgin_conv_attach_to_conversation:
 * @conv:  The conversation.
 *
 * Reattach Pidgin UI to a conversation.
 *
 * Returns: Whether Pidgin UI was successfully attached.
 */
gboolean pidgin_conv_attach_to_conversation(PurpleConversation *conv);

/**
 * pidgin_conv_new:
 * @conv: The conversation.
 *
 * Creates a new GTK conversation for a given #PurpleConversation.
 */
void pidgin_conv_new(PurpleConversation *conv);

/**************************************************************************/
/* GTK Conversations Subsystem                                            */
/**************************************************************************/

/**
 * pidgin_conversations_get_handle:
 *
 * Returns the gtk conversations subsystem handle.
 *
 * Returns: The conversations subsystem handle.
 */
void *pidgin_conversations_get_handle(void);

/**
 * pidgin_conversations_init:
 *
 * Initializes the GTK conversations subsystem.
 */
void pidgin_conversations_init(void);

/**
 * pidgin_conversations_uninit:
 *
 * Uninitialized the GTK conversation subsystem.
 */
void pidgin_conversations_uninit(void);

void pidgin_conversation_detach(PurpleConversation *conv);

G_END_DECLS

#endif /* _PIDGIN_CONVERSATION_H_ */
