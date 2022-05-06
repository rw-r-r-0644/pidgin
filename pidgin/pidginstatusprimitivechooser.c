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

PurpleStatusPrimitive
pidgin_status_primitive_chooser_get_selected(PidginStatusPrimitiveChooser *chooser) {
    const gchar *active_id = NULL;

    g_return_val_if_fail(PIDGIN_IS_STATUS_PRIMITIVE_CHOOSER(chooser),
                         PURPLE_STATUS_UNSET);

    active_id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(chooser));

    return purple_primitive_get_type_from_id(active_id);
}

void
pidgin_status_primitive_chooser_set_selected(PidginStatusPrimitiveChooser *chooser,
                                             PurpleStatusPrimitive primitive)
{
    const gchar *active_id = NULL;

    g_return_if_fail(PIDGIN_IS_STATUS_PRIMITIVE_CHOOSER(chooser));

    active_id = purple_primitive_get_id_from_type(primitive);

    gtk_combo_box_set_active_id(GTK_COMBO_BOX(chooser), active_id);
}
