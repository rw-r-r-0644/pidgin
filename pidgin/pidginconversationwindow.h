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

#ifndef PIDGIN_CONVERSATION_WINDOW_H
#define PIDGIN_CONVERSATION_WINDOW_H

#include <glib.h>

#include <gtk/gtk.h>

#include <purple.h>

G_BEGIN_DECLS

/**
 * PidginConversationWindow:
 *
 * #PidginConversationWindow is a widget that contains #PidginConversations.
 *
 * Since: 3.0.0
 */

#define PIDGIN_TYPE_CONVERSATION_WINDOW (pidgin_conversation_window_get_type())
G_DECLARE_FINAL_TYPE(PidginConversationWindow, pidgin_conversation_window,
                     PIDGIN, CONVERSATION_WINDOW, GtkApplicationWindow)

/**
 * pidgin_conversation_window_new:
 *
 * Creates a new #PidginConversationWindow instance.
 *
 * Returns: (transfer full): The new #PidginConversationWindow instance.
 */
GtkWidget *pidgin_conversation_window_new(void);

/**
 * pidgin_conversation_window_get_default:
 *
 * Gets or creates the default conversation window.
 *
 * Returns: (transfer none): The default #PidginConversationWindow.
 *
 * Since: 3.0.0
 */
GtkWidget *pidgin_conversation_window_get_default(void);

/**
 * pidgin_conversation_window_add:
 * @window: The #PidginConversationWindow instance.
 * @conversation: The #PurpleConversation to add to @window.
 *
 * Adds @conversation to @window. If @conversation is already in @window, this
 * does nothing.
 *
 * Since: 3.0.0
 */
void pidgin_conversation_window_add(PidginConversationWindow *window, PurpleConversation *conversation);

/**
 * pidgin_conversation_window_remove:
 * @window: The #PidginConversationWindow instance.
 * @conversation: The #PurpleConversation to remove from @window.
 *
 * Removes @conversation from @window. If @conversation is not in @window, this
 * does nothing.
 *
 * Since: 3.0.0
 */
void pidgin_conversation_window_remove(PidginConversationWindow *window, PurpleConversation *conversation);

/**
 * pidgin_conversation_window_get_count:
 * @window: The conversation window instance.
 *
 * Gets the number of conversations that @window contains.
 *
 * Returns: The number of conversations that @window contains.
 *
 * Since: 3.0.0
 */
guint pidgin_conversation_window_get_count(PidginConversationWindow *window);

/**
 * pidgin_conversation_window_get_selected:
 * @window: The conversation window instance.
 *
 * Gets the currently selected PurpleConversation or %NULL if there is no
 * selection.
 *
 * Returns: (transfer full): The selected PurpleConversation or %NULL.
 *
 * Since: 3.0.0
 */
PurpleConversation *pidgin_conversation_window_get_selected(PidginConversationWindow *window);

/**
 * pidgin_conversation_window_select:
 * @window: The conversation window instance.
 * @conversation: The conversation to select.
 *
 * Selects @conversation, making it the active conversation.
 *
 * Since: 3.0.0
 */
void pidgin_conversation_window_select(PidginConversationWindow *window, PurpleConversation *conversation);

/**
 * pidgin_conversation_window_select_previous:
 * @window: The conversation window instance.
 *
 * Switches to the conversation previous to the currently selected
 * conversation.
 *
 * If no conversation is selected, the last conversation will be selected.
 *
 * Since: 3.0.0
 */
void pidgin_conversation_window_select_previous(PidginConversationWindow *window);

/**
 * pidgin_conversation_window_select_next:
 * @window: The conversation window instance.
 *
 * Switches to the conversation next of the currently selected conversation.
 *
 * If no conversation is selected, the first conversation will be selected.
 *
 * Since: 3.0.0
 */
void pidgin_conversation_window_select_next(PidginConversationWindow *window);

/**
 * pidgin_conversation_window_select_first:
 * @window: The conversation window instance.
 *
 * Selects the first conversation in @window. If there are no conversations in
 * @window this does nothing.
 *
 * Since: 3.0.0
 */
void pidgin_conversation_window_select_first(PidginConversationWindow *window);

/**
 * pidgin_conversation_window_select_last:
 * @window: The conversation window instance.
 *
 * Selects the last conversation in @window. If there are no conversations in
 * @window this does nothing.
 *
 * Since: 3.0.0
 */
void pidgin_conversation_window_select_last(PidginConversationWindow *window);

/**
 * pidgin_conversation_window_select_nth:
 * @window: The conversation window instance.
 * @nth: The index of the conversation to switch to.
 *
 * Switches to the @nth conversation. @nth is a 0 based index, so the first
 * conversation is at index 0.
 *
 * Since: 3.0.0
 */
void pidgin_conversation_window_select_nth(PidginConversationWindow *window, guint nth);

/**
 * pidgin_conversation_window_conversation_is_selected:
 * @window: The conversation window instance.
 * @conversation: The conversation instance.
 *
 * Checks whether @conversation is the active conversation in @window.
 *
 * Returns: %TRUE if @conversation is active.
 *
 * Since: 3.0.0
 */
gboolean pidgin_conversation_window_conversation_is_selected(PidginConversationWindow *window, PurpleConversation *conversation);

G_END_DECLS

#endif /* PIDGIN_CONVERSATION_WINDOW_H */
