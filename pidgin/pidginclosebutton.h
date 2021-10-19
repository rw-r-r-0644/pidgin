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

#ifndef PIDGIN_CLOSE_BUTTON_H
#define PIDGIN_CLOSE_BUTTON_H

#include <glib.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * PidginCloseButton:
 *
 * #PidginCloseButton is a transitional widget as we slowly migrate the
 * conversation window to glade.
 *
 * Since: 3.0.0
 */

#define PIDGIN_TYPE_CLOSE_BUTTON (pidgin_close_button_get_type())
G_DECLARE_FINAL_TYPE(PidginCloseButton, pidgin_close_button, PIDGIN,
                     CLOSE_BUTTON, GtkButton)

/**
 * pidgin_close_button_new:
 *
 * Creates a new #PidginCloseButton instance.
 *
 * Returns: (transfer full): The new #PidginCloseButton instance.
 *
 * Since: 3.0.0
 */
GtkWidget *pidgin_close_button_new(void);

G_END_DECLS

#endif /* PIDGIN_CLOSE_BUTTON_H */
