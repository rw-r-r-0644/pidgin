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

#ifndef PIDGIN_CONTACT_LIST_H
#define PIDGIN_CONTACT_LIST_H

/**
 * SECTION:pidgincontactlist
 * @section_id: pidgin-contactlist
 * @short_description: The contact list widget.
 * @title: Contact list widget
 *
 * #PidginContactList is a transitional widget as we slowly migrate it into the
 * conversation window to make a single window application.
 */

#include <glib.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * PIDGIN_TYPE_CONTACT_LIST:
 *
 * The standard _get_type method for #PidginContactList.
 *
 * Since: 3.0.0
 */

/**
 * PidginContactList:
 *
 * A #GtkWindow that contains the main contact list.
 *
 * Since: 3.0.0
 */

#define PIDGIN_TYPE_CONTACT_LIST (pidgin_contact_list_get_type())
G_DECLARE_FINAL_TYPE(PidginContactList, pidgin_contact_list, PIDGIN,
                     CONTACT_LIST, GtkApplicationWindow)

/**
 * pidgin_contact_list_new:
 *
 * Creates a new #PidginContactList instance.
 *
 * Returns: (transfer full): The new #PidginContactList instance.
 *
 * Since: 3.0.0
 */
GtkWidget *pidgin_contact_list_new(void);

/**
 * pidgin_contact_list_get_vbox:
 * @contact_list: The #PidginContactList instance.
 *
 * Gets the main vbox for @contact_list.
 *
 * Returns: (transfer none): The main vbox of @contact_list.
 *
 * Since: 3.0.0
 */
GtkWidget *pidgin_contact_list_get_vbox(PidginContactList *contact_list);

/**
 * pidgin_contact_list_get_menu_sort_item:
 * @contact_list: The #PidginContactList instance.
 *
 * Returns the sort menu item from the menu of @contact_list.
 *
 * Returns: (transfer none): The sort menu item from the menu of @contact_list.
 *
 * Since: 3.0.0
 */
GtkWidget *pidgin_contact_list_get_menu_sort_item(PidginContactList *contact_list);

/**
 * pidgin_contact_list_get_menu_tray:
 * @contact_list: The #PidginContactList instance.
 *
 * Gets the #PidginMenuTray instance from the menu of @contact_list.
 *
 * Returns: (transfer none): The #PidginMenuTray from the menu of
 *          @contact_list.
 *
 * Since: 3.0.0
 */
GtkWidget *pidgin_contact_list_get_menu_tray(PidginContactList *contact_list);

G_END_DECLS

#endif /* PIDGIN_CONTACT_LIST_H */
