/*
 * pidgin
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

#include "pidginbuddylistmenu.h"

struct _PidginBuddyListMenu {
	GtkMenuBar parent;

	GtkWidget *sort_buddies;
};

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
G_DEFINE_TYPE(PidginBuddyListMenu, pidgin_buddy_list_menu, GTK_TYPE_MENU_BAR)

static void
pidgin_buddy_list_menu_init(PidginBuddyListMenu *menu) {
	gtk_widget_init_template(GTK_WIDGET(menu));
}

static void
pidgin_buddy_list_menu_class_init(PidginBuddyListMenuClass *klass) {
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

    gtk_widget_class_set_template_from_resource(
        widget_class,
        "/im/pidgin/Pidgin/BuddyList/menu.ui"
    );

   	gtk_widget_class_bind_template_child(widget_class, PidginBuddyListMenu,
   	                                     sort_buddies);
}

/******************************************************************************
 * Public API
 *****************************************************************************/
GtkWidget *
pidgin_buddy_list_menu_new(void) {
	return GTK_WIDGET(g_object_new(PIDGIN_TYPE_BUDDY_LIST_MENU, NULL));
}

GtkWidget *
pidgin_buddy_list_menu_get_sort_item(PidginBuddyListMenu *menu) {
	g_return_val_if_fail(PIDGIN_IS_BUDDY_LIST_MENU(menu), NULL);

	return menu->sort_buddies;
}
