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
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#if !defined(PIDGIN_GLOBAL_HEADER_INSIDE) && !defined(PIDGIN_COMPILATION)
# error "only <pidgin.h> may be included directly"
#endif

#ifndef PIDGIN_WINDOW_H
#define PIDGIN_WINDOW_H

/**
 * SECTION:pidginwindow
 * @section_id: pidgin-window
 * @short_description: A base window widget.
 * @title: Window
 *
 * #PidginWindow is a transitional widget as we slowly migrate everything to
 * glade.
 */

#include <glib.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PIDGIN_TYPE_WINDOW (pidgin_window_get_type())
G_DECLARE_FINAL_TYPE(PidginWindow, pidgin_window, PIDGIN,
                     WINDOW, GtkWindow)

/**
 * pidgin_window_new:
 * @title: The window title, or %NULL.
 * @border_width: The window's desired border width.
 * @role: A string indicating what the window is responsible for doing, or
 *        %NULL.
 * @resizable: Whether the window should be resizable.
 *
 * Creates a new #PidginWindow instance.
 *
 * Returns: (transfer full): The new #PidginWindow instance.
 *
 * Since: 3.0.0
 */
GtkWidget *pidgin_window_new(const gchar *title, guint border_width, const gchar *role, gboolean resizable);

G_END_DECLS

#endif /* PIDGIN_WINDOW_H */
