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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "pidgindialog.h"

#include "pidgincore.h"

struct _PidginDialog {
	GtkDialog parent;
};

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
G_DEFINE_TYPE(PidginDialog, pidgin_dialog, GTK_TYPE_DIALOG)

static void
pidgin_dialog_init(PidginDialog *button) {
}

static void
pidgin_dialog_class_init(PidginDialogClass *klass) {
}

/******************************************************************************
 * Public API
 *****************************************************************************/
GtkWidget *
pidgin_dialog_new(const gchar *title, guint border_width, const gchar *role,
                  gboolean resizable)
{
	if(title == NULL) {
		title = PIDGIN_ALERT_TITLE;
	}

	return GTK_WIDGET(g_object_new(
		PIDGIN_TYPE_DIALOG,
		"title", title,
		"border-width", border_width,
		"role", role,
		"resizable", resizable,
		NULL));
}
