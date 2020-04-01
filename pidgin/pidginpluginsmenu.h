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
#ifndef PIDGIN_PLUGINS_MENU_H
#define PIDGIN_PLUGINS_MENU_H

/**
 * SECTION:pidginpluginsmenu
 * @section_id: pidgin-plugins-menu
 * @short_description: A menu for managing plugins and their actions
 * @title: Plugins Menu
 *
 * #PidginPluginsMenu is a #GtkMenu that provides an interface to users to open
 * the plugin manager as well as activate plugin actions.
 *
 * It manages itself as plugins are loaded and unloaded and can be added as a
 * submenu to any #GtkMenuItem.
 */


#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PIDGIN_TYPE_PLUGINS_MENU (pidgin_plugins_menu_get_type())
G_DECLARE_FINAL_TYPE(PidginPluginsMenu, pidgin_plugins_menu, PIDGIN,
                     PLUGINS_MENU, GtkMenu)

/**
 * pidgin_plugins_menu_new:
 *
 * Creates a new #PidginPluginsMenu instance that keeps itself up to date.
 *
 * Returns: (transfer full): The new #PidginPluginsMenu instance.
 */
GtkWidget *pidgin_plugins_menu_new(void);

G_END_DECLS

#endif /* PIDGIN_PLUGINS_MENU_H */
