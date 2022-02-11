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

#include <pidgin/pidginstatusprimitivechooser.h>

struct _PidginStatusPrimitiveChooser {
	GtkComboBox parent;
};

G_DEFINE_TYPE(PidginStatusPrimitiveChooser, pidgin_status_primitive_chooser,
              GTK_TYPE_COMBO_BOX)

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/

static void
pidgin_status_primitive_chooser_init(PidginStatusPrimitiveChooser *chooser) {
	gtk_widget_init_template(GTK_WIDGET(chooser));
}

static void
pidgin_status_primitive_chooser_class_init(PidginStatusPrimitiveChooserClass *klass) {
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	/* Widget template */
	gtk_widget_class_set_template_from_resource(
	        widget_class, "/im/pidgin/Pidgin3/statusprimitivechooser.ui");
}

/******************************************************************************
 * Public API
 *****************************************************************************/
GtkWidget *
pidgin_status_primitive_chooser_new(void) {
	return g_object_new(PIDGIN_TYPE_STATUS_PRIMITIVE_CHOOSER, NULL);
}
