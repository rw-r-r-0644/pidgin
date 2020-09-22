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

#include "pidginclosebutton.h"

struct _PidginCloseButton {
	GtkButton parent;
};

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
G_DEFINE_TYPE(PidginCloseButton, pidgin_close_button, GTK_TYPE_BUTTON)

static void
pidgin_close_button_init(PidginCloseButton *button) {
	gtk_widget_init_template(GTK_WIDGET(button));
}

static void
pidgin_close_button_class_init(PidginCloseButtonClass *klass) {
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(
	    widget_class,
	    "/im/pidgin/Pidgin/closebutton.ui"
	);
}

/******************************************************************************
 * Public API
 *****************************************************************************/
GtkWidget *
pidgin_close_button_new(void) {
	return GTK_WIDGET(g_object_new(PIDGIN_TYPE_CLOSE_BUTTON, NULL));
}
