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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301 USA
 */

#include "pidginprotocolchooser.h"

#include "pidginprotocolstore.h"

/******************************************************************************
 * Structs
 *****************************************************************************/
struct _PidginProtocolChooser {
	GtkComboBox parent;

	GtkTreeModel *model;
};

/******************************************************************************
 * Callbacks
 *****************************************************************************/
static void
pidgin_protocol_chooser_model_changed_cb(GObject *obj, GParamSpec *pspec,
                                         gpointer data)
{
	GtkComboBox *combo = GTK_COMBO_BOX(obj);
	GtkTreeModel *model = gtk_combo_box_get_model(combo);
	GtkTreeIter iter;

	/* When the model for the combobox changes, select the first item. */
	if(gtk_tree_model_get_iter_first(model, &iter)) {
		gtk_combo_box_set_active_iter(combo, &iter);
	}
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
G_DEFINE_TYPE(PidginProtocolChooser, pidgin_protocol_chooser,
              GTK_TYPE_COMBO_BOX)

static void
pidgin_protocol_chooser_class_init(PidginProtocolChooserClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(widget_class,
	                                            "/im/pidgin/Pidgin3/Protocols/chooser.ui");

	gtk_widget_class_bind_template_child(widget_class, PidginProtocolChooser,
	                                     model);
}

static void
pidgin_protocol_chooser_init(PidginProtocolChooser *chooser) {
	g_signal_connect_object(G_OBJECT(chooser), "notify::model",
	                        G_CALLBACK(pidgin_protocol_chooser_model_changed_cb),
	                        chooser, G_CONNECT_AFTER);

	gtk_widget_init_template(GTK_WIDGET(chooser));

	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(chooser->model),
	                                     PIDGIN_PROTOCOL_STORE_COLUMN_NAME,
	                                     GTK_SORT_ASCENDING);

	gtk_combo_box_set_id_column(GTK_COMBO_BOX(chooser),
	                            PIDGIN_PROTOCOL_STORE_COLUMN_ID);
}

/******************************************************************************
 * Public API
 *****************************************************************************/
GtkWidget *
pidgin_protocol_chooser_new(void) {
	return g_object_new(PIDGIN_TYPE_PROTOCOL_CHOOSER, NULL);
}

PurpleProtocol *
pidgin_protocol_chooser_get_selected(PidginProtocolChooser *chooser) {
	GtkTreeIter iter;
	PurpleProtocol *protocol = NULL;

	g_return_val_if_fail(PIDGIN_IS_PROTOCOL_CHOOSER(chooser), NULL);

	if(gtk_combo_box_get_active_iter(GTK_COMBO_BOX(chooser), &iter)) {
		gtk_tree_model_get(GTK_TREE_MODEL(chooser->model), &iter,
		                   PIDGIN_PROTOCOL_STORE_COLUMN_PROTOCOL, &protocol,
		                   -1);
	}

	return protocol;
}

const gchar *
pidgin_protocol_chooser_get_selected_id(PidginProtocolChooser *chooser) {
	g_return_val_if_fail(PIDGIN_IS_PROTOCOL_CHOOSER(chooser), NULL);

	return gtk_combo_box_get_active_id(GTK_COMBO_BOX(chooser));
}

void
pidgin_protocol_chooser_set_selected_id(PidginProtocolChooser *chooser,
                                        const gchar *id)
{
	g_return_if_fail(PIDGIN_IS_PROTOCOL_CHOOSER(chooser));

	if(id != NULL) {
		gtk_combo_box_set_active_id(GTK_COMBO_BOX(chooser), id);
	} else {
		GtkTreeIter first;

		gtk_tree_model_get_iter_first(chooser->model, &first);
		gtk_combo_box_set_active_iter(GTK_COMBO_BOX(chooser), &first);
	}

}
