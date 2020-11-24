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

	GMenu *send_to_menu;

	GtkWidget *vbox;
};

G_DEFINE_TYPE(PidginConversationWindow, pidgin_conversation_window,
              GTK_TYPE_APPLICATION_WINDOW)

/******************************************************************************
 * GObjectImplementation
 *****************************************************************************/
static void
pidgin_conversation_window_init(PidginConversationWindow *window) {
	GtkBuilder *builder = NULL;
	GtkWidget *menubar = NULL;
	GMenuModel *model = NULL;

	gtk_widget_init_template(GTK_WIDGET(window));

	gtk_window_set_application(GTK_WINDOW(window),
	                           GTK_APPLICATION(g_application_get_default()));

	/* setup our menu */
	builder = gtk_builder_new_from_resource("/im/pidgin/Pidgin/Conversations/menu.ui");

	model = (GMenuModel *)gtk_builder_get_object(builder, "conversation");
	menubar = gtk_menu_bar_new_from_model(model);
	gtk_box_pack_start(GTK_BOX(window->vbox), menubar, FALSE, FALSE, 0);
	gtk_widget_show(menubar);

	window->send_to_menu = (GMenu *)gtk_builder_get_object(builder, "send-to");

	g_object_unref(G_OBJECT(builder));
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
