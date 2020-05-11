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
#ifndef PIDGIN_BUDDY_LIST_MENU_H
#define PIDGIN_BUDDY_LIST_MENU_H

/**
 * SECTION:pidginbuddylistmenu
 * @section_id: pidgin-buddylist-menu
 * @short_description: A widget to display the menubar in the buddy list window.
 * @title: Buddylist Menu
 *
 * #PidginBuddyListMenu is a transitional widget as we slowly migrate the
 * buddylist window to glade.
 */

#include <glib.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PIDGIN_TYPE_BUDDY_LIST_MENU (pidgin_buddy_list_menu_get_type())
G_DECLARE_FINAL_TYPE(PidginBuddyListMenu, pidgin_buddy_list_menu, PIDGIN,
                     BUDDY_LIST_MENU, GtkMenuBar)

/**
 * pidgin_buddy_list_menu_new:
 *
 * Creates a new #PidginBuddyListMenu instance.
 *
 * Returns: (transfer full): The new #PidginBuddyListMenu instance.
 */
GtkWidget *pidgin_buddy_list_menu_new(void);

/**
 * pidgin_buddy_list_menu_get_sort_item:
 * @menu: The #PidginBuddyList instance.
 *
 * Returns the sort menu item from the buddies menu.
 *
 * Returns: (transfer none): The sort menu item from the buddies menu.
 */
GtkWidget *pidgin_buddy_list_menu_get_sort_item(PidginBuddyListMenu *menu);

/**
 * pidgin_buddy_list_menu_get_menu_tray:
 * @menu: The #PidginBuddyList instance.
 *
 * Gets the #PidginMenuTray instance from @menu.
 *
 * Returns: (transfer none): The #PidginMenuTray from @menu.
 */
GtkWidget *pidgin_buddy_list_menu_get_menu_tray(PidginBuddyListMenu *menu);

G_END_DECLS

#endif /* PIDGIN_BUDDY_LIST_MENU_H */
