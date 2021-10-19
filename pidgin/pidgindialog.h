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

#ifndef PIDGIN_DIALOG_H
#define PIDGIN_DIALOG_H

#include <glib.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * PidginDialog:
 *
 * #PidginDialog is a transitional widget as we slowly migrate everything to
 * glade.
 */

#define PIDGIN_TYPE_DIALOG (pidgin_dialog_get_type())
G_DECLARE_FINAL_TYPE(PidginDialog, pidgin_dialog, PIDGIN,
                     DIALOG, GtkDialog)

/**
 * pidgin_dialog_new:
 * @title: The dialog title, or %NULL.
 * @border_width: The dialog's desired border width.
 * @role: A string indicating what the dialog is responsible for doing, or
 *        %NULL.
 * @resizable: Whether the dialog should be resizable.
 *
 * Creates a new #PidginDialog instance.
 *
 * Returns: (transfer full): The new #PidginDialog instance.
 *
 * Since: 3.0.0
 */
GtkWidget *pidgin_dialog_new(const gchar *title, guint border_width, const gchar *role, gboolean resizable);

G_END_DECLS

#endif /* PIDGIN_DIALOG_H */
