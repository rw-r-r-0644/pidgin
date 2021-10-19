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
 * pidgin_conversation_window_get_vbox:
 * @window: The #PidginConversationWindow instance.
 *
 * Gets the main vertical box of @window.
 *
 * Returns: (transfer none): The main vertical box of @window.
 *
 * Since: 3.0.0
 * Deprecated: 3.0.0
 */
GtkWidget *pidgin_conversation_window_get_vbox(PidginConversationWindow *window);

G_END_DECLS

#endif /* PIDGIN_CONVERSATION_WINDOW_H */
