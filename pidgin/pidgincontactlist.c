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

#include "pidgincontactlist.h"

#include "pidginactiongroup.h"

struct _PidginContactList {
	GtkApplicationWindow parent;

	GtkWidget *vbox;

	GtkWidget *menu_bar;
	GtkWidget *sort_buddies;

	GtkWidget *accounts;
	GtkWidget *accounts_menu;
};

G_DEFINE_TYPE(PidginContactList, pidgin_contact_list,
              GTK_TYPE_APPLICATION_WINDOW)

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
static void
pidgin_contact_list_init(PidginContactList *contact_list) {
	GSimpleActionGroup *group = NULL;

	gtk_widget_init_template(GTK_WIDGET(contact_list));

	gtk_window_set_application(GTK_WINDOW(contact_list),
	                           GTK_APPLICATION(g_application_get_default()));

	group = pidgin_action_group_new();
	gtk_widget_insert_action_group(GTK_WIDGET(contact_list), "blist",
	                               G_ACTION_GROUP(group));

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(contact_list->accounts),
	                          contact_list->accounts_menu);
}

static void
pidgin_contact_list_class_init(PidginContactListClass *klass) {
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(
	    widget_class,
	    "/im/pidgin/Pidgin3/BuddyList/window.ui"
	);

	gtk_widget_class_bind_template_child(widget_class, PidginContactList, vbox);

	gtk_widget_class_bind_template_child(widget_class, PidginContactList,
	                                     menu_bar);
	gtk_widget_class_bind_template_child(widget_class, PidginContactList,
	                                     sort_buddies);
	gtk_widget_class_bind_template_child(widget_class, PidginContactList,
	                                     accounts);
	gtk_widget_class_bind_template_child(widget_class, PidginContactList,
	                                     accounts_menu);
}

/******************************************************************************
 * Public API
 *****************************************************************************/
GtkWidget *
pidgin_contact_list_new(void) {
	return GTK_WIDGET(g_object_new(PIDGIN_TYPE_CONTACT_LIST, NULL));
}

GtkWidget *
pidgin_contact_list_get_vbox(PidginContactList *contact_list) {
	g_return_val_if_fail(PIDGIN_IS_CONTACT_LIST(contact_list), NULL);

	return contact_list->vbox;
}

GtkWidget *
pidgin_contact_list_get_menu_sort_item(PidginContactList *contact_list) {
	g_return_val_if_fail(PIDGIN_IS_CONTACT_LIST(contact_list), NULL);

	return contact_list->sort_buddies;
}
