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

#include "pidginconversationwindow.h"

struct _PidginConversationWindow {
	GtkApplicationWindow parent;

	GtkWidget *vbox;
};

G_DEFINE_TYPE(PidginConversationWindow, pidgin_conversation_window,
              GTK_TYPE_APPLICATION_WINDOW)

/******************************************************************************
 * GObjectImplementation
 *****************************************************************************/
static void
pidgin_conversation_window_init(PidginConversationWindow *window) {
	gtk_widget_init_template(GTK_WIDGET(window));

	gtk_window_set_application(GTK_WINDOW(window),
	                           GTK_APPLICATION(g_application_get_default()));
}

static void
pidgin_conversation_window_class_init(PidginConversationWindowClass *klass) {
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(
	    widget_class,
	    "/im/pidgin/Pidgin/Conversations/window.ui"
	);

	gtk_widget_class_bind_template_child(widget_class, PidginConversationWindow,
	                                     vbox);
}

/******************************************************************************
 * API
 *****************************************************************************/
GtkWidget *
pidgin_conversation_window_new(void) {
	return GTK_WIDGET(g_object_new(PIDGIN_TYPE_CONVERSATION_WINDOW, NULL));
}

GtkWidget *
pidgin_conversation_window_get_vbox(PidginConversationWindow *window) {
	g_return_val_if_fail(PIDGIN_IS_CONVERSATION_WINDOW(window), NULL);

	return window->vbox;
}
