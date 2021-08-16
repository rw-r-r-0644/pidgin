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
#include <gtk/gtk.h>

#include "pidginaccountchooser.h"

#include "pidginaccountstore.h"

struct _PidginAccountChooser {
	GtkComboBox parent;

	PurpleFilterAccountFunc filter_func;
};

enum
{
	PROP_0,
	PROP_ACCOUNT,
	PROP_LAST
};

static GParamSpec *properties[PROP_LAST] = {NULL};

/******************************************************************************
 * Callbacks
 *****************************************************************************/
static void
pidgin_account_chooser_changed_cb(GtkComboBox *widget, gpointer user_data) {
	g_object_notify_by_pspec(G_OBJECT(widget), properties[PROP_ACCOUNT]);
}

/******************************************************************************
 * GObject implementation
 *****************************************************************************/
G_DEFINE_TYPE(PidginAccountChooser, pidgin_account_chooser, GTK_TYPE_COMBO_BOX)

static void
pidgin_account_chooser_get_property(GObject *object, guint prop_id,
                                    GValue *value, GParamSpec *pspec)
{
	PidginAccountChooser *chooser = PIDGIN_ACCOUNT_CHOOSER(object);

	switch (prop_id) {
		case PROP_ACCOUNT:
			g_value_set_object(value, pidgin_account_chooser_get_selected(chooser));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}

static void
pidgin_account_chooser_set_property(GObject *object, guint prop_id,
                                    const GValue *value, GParamSpec *pspec)
{
	PidginAccountChooser *chooser = PIDGIN_ACCOUNT_CHOOSER(object);

	switch (prop_id) {
		case PROP_ACCOUNT:
			pidgin_account_chooser_set_selected(chooser,
			                                    g_value_get_object(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}

static void
pidgin_account_chooser_class_init(PidginAccountChooserClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	/* Properties */
	obj_class->get_property = pidgin_account_chooser_get_property;
	obj_class->set_property = pidgin_account_chooser_set_property;

	properties[PROP_ACCOUNT] = g_param_spec_object(
	        "account", "Account", "The account that is currently selected.",
	        PURPLE_TYPE_ACCOUNT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, PROP_LAST, properties);

	/* Widget template */
	gtk_widget_class_set_template_from_resource(
	        widget_class, "/im/pidgin/Pidgin3/Accounts/chooser.ui");
}

static void
pidgin_account_chooser_init(PidginAccountChooser *chooser) {
	gtk_widget_init_template(GTK_WIDGET(chooser));

	/* this callback emits the notify for the account property */
	g_signal_connect(chooser, "changed",
	                 G_CALLBACK(pidgin_account_chooser_changed_cb), NULL);
}

/******************************************************************************
 * Public API
 *****************************************************************************/
GtkWidget *
pidgin_account_chooser_new(void) {
	PidginAccountChooser *chooser = NULL;

	chooser = g_object_new(PIDGIN_TYPE_ACCOUNT_CHOOSER, NULL);

	return GTK_WIDGET(chooser);
}

void
pidgin_account_chooser_set_filter_func(PidginAccountChooser *chooser,
                                       PurpleFilterAccountFunc filter_func)
{
	g_return_if_fail(PIDGIN_IS_ACCOUNT_CHOOSER(chooser));

	chooser->filter_func = filter_func;
}

PurpleAccount *
pidgin_account_chooser_get_selected(PidginAccountChooser *chooser) {
	GtkTreeIter iter;
	gpointer account = NULL;

	g_return_val_if_fail(PIDGIN_IS_ACCOUNT_CHOOSER(chooser), NULL);

	if(gtk_combo_box_get_active_iter(GTK_COMBO_BOX(chooser), &iter)) {
		GtkTreeModel *model = gtk_combo_box_get_model(GTK_COMBO_BOX(chooser));
		gtk_tree_model_get(model, &iter,
		                   PIDGIN_ACCOUNT_STORE_COLUMN_ACCOUNT, &account, -1);
	}

	return account;
}

void
pidgin_account_chooser_set_selected(PidginAccountChooser *chooser,
                                    PurpleAccount *account)
{
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	PurpleAccount *acc = NULL;

	g_return_if_fail(PIDGIN_IS_ACCOUNT_CHOOSER(chooser));

	model = gtk_combo_box_get_model(GTK_COMBO_BOX(chooser));
	g_return_if_fail(GTK_IS_TREE_MODEL(model));

	if(gtk_tree_model_get_iter_first(model, &iter)) {
		do {
			gtk_tree_model_get(model, &iter,
			                   PIDGIN_ACCOUNT_STORE_COLUMN_ACCOUNT, &acc, -1);
			if(acc == account) {
				/* NOTE: Property notification occurs in 'changed' signal
				 * callback.
				 */
				gtk_combo_box_set_active_iter(GTK_COMBO_BOX(chooser), &iter);

				g_object_unref(G_OBJECT(acc));

				return;
			}
			g_object_unref(G_OBJECT(acc));
		} while(gtk_tree_model_iter_next(model, &iter));
	}
}

