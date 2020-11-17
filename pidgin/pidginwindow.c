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

#include <glib/gi18n-lib.h>

#include "pidginwindow.h"

#include "pidgincore.h"

struct _PidginWindow {
	GtkWindow parent;
};

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
G_DEFINE_TYPE(PidginWindow, pidgin_window, GTK_TYPE_WINDOW)

static void
pidgin_window_init(PidginWindow *button) {
}

static void
pidgin_window_class_init(PidginWindowClass *klass) {
}

/******************************************************************************
 * Public API
 *****************************************************************************/
GtkWidget *
pidgin_window_new(const gchar *title, guint border_width, const gchar *role,
                  gboolean resizable)
{
	if(title == NULL) {
		title = PIDGIN_ALERT_TITLE;
	}

	return GTK_WIDGET(g_object_new(
		PIDGIN_TYPE_WINDOW,
		"title", title,
		"border-width", border_width,
		"role", role,
		"resizable", resizable,
		NULL));
}
