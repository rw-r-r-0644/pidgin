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

#include <glib/gi18n.h>

#include "pidginproxyoptions.h"

#include "gtkaccount.h"
#include "pidgincore.h"

struct _PidginProxyOptions {
	GtkBox parent;

	gboolean show_global;
	PurpleProxyInfo *info;

	GtkListStore *model;
	GtkTreeModelFilter *filter;

	GtkWidget *proxy_type;

	GtkWidget *options;
	GtkWidget *hostname;
	GtkWidget *port;
	GtkWidget *username;
	GtkWidget *password;
};

enum {
	PROP_0,
	PROP_SHOW_GLOBAL,
	PROP_INFO,
	N_PROPERTIES
};
static GParamSpec *properties[N_PROPERTIES] = {NULL, };

enum {
	COLUMN_TYPE,
	COLUMN_TITLE,
};

G_DEFINE_TYPE(PidginProxyOptions, pidgin_proxy_options, GTK_TYPE_BOX)

/******************************************************************************
 * Helpers
 *****************************************************************************/
static gboolean
pidgin_proxy_options_null_to_empty_str(G_GNUC_UNUSED GBinding *binding,
                                       const GValue *from_value,
                                       GValue *to_value,
                                       G_GNUC_UNUSED gpointer data)
{
	const gchar *str_val = g_value_get_string(from_value);

	if(str_val == NULL) {
		str_val = "";
	}

	g_value_set_string(to_value, str_val);

	return TRUE;
}

static gboolean
pidgin_proxy_options_empty_str_to_null(G_GNUC_UNUSED GBinding *binding,
                                       const GValue *from_value,
                                       GValue *to_value,
                                       G_GNUC_UNUSED gpointer data)
{
	const gchar *str_val = g_value_get_string(from_value);

	if(str_val != NULL && *str_val == '\0') {
		str_val = NULL;
	}

	g_value_set_string(to_value, str_val);

	return TRUE;
}

static gboolean
pidgin_proxy_options_gint_to_double(G_GNUC_UNUSED GBinding *binding,
                                    const GValue *from_value, GValue *to_value,
                                    G_GNUC_UNUSED gpointer data)
{
	g_value_set_double(to_value, (gdouble)g_value_get_int(from_value));

	return TRUE;
}

static gboolean
pidgin_proxy_options_double_to_gint(G_GNUC_UNUSED GBinding *binding,
                                    const GValue *from_value, GValue *to_value,
                                    G_GNUC_UNUSED gpointer user_data)
{
	g_value_set_int(to_value, (gint)g_value_get_double(from_value));

	return TRUE;
}

static gboolean
pidgin_proxy_options_filter_visible(GtkTreeModel *model, GtkTreeIter *iter,
                                    gpointer data)
{
	PidginProxyOptions *options = data;
	PurpleProxyType type;

	gtk_tree_model_get(model, iter, COLUMN_TYPE, &type, -1);

	if(type == PURPLE_PROXY_TYPE_USE_GLOBAL) {
		return options->show_global;
	}

	return TRUE;
}

/******************************************************************************
 * Callbacks
 *****************************************************************************/
static void
pidgin_proxy_options_proxy_type_changed_cb(GtkComboBox *box, gpointer data) {
	PidginProxyOptions *options = data;
	PurpleProxyType type;
	GtkTreeIter iter;
	gboolean sensitive = TRUE;

	if(!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(options->proxy_type), &iter)) {
		return;
	}

	gtk_tree_model_get(GTK_TREE_MODEL(options->filter), &iter,
	                   COLUMN_TYPE, &type,
	                   -1);

	purple_proxy_info_set_proxy_type(options->info, type);

	switch(type) {
		case PURPLE_PROXY_TYPE_USE_GLOBAL:
		case PURPLE_PROXY_TYPE_NONE:
		case PURPLE_PROXY_TYPE_USE_ENVVAR:
			sensitive = FALSE;
			break;
		default:
			break;
	}

	gtk_widget_set_sensitive(options->options, sensitive);
}

static void
pidgin_proxy_options_ports_popup_cb(G_GNUC_UNUSED GtkEntry *entry,
                                    GtkWidget *widget,
                                    G_GNUC_UNUSED gpointer data)
{
	GtkWidget *item = NULL;

	/* This is an easter egg. The items are in reverse order because they are
	 * prepended to the menu.
	 *
	 * It means one of two things, both intended as humorous:
	 * A) your network is really slow and you have nothing better to do than
	 *    look at butterflies.
	 * B) You are looking really closely at something that shouldn't matter.
	 */

	item = gtk_menu_item_new_with_label(_("you can see the butterflies mating"));
	gtk_menu_shell_prepend(GTK_MENU_SHELL(widget), item);
	gtk_widget_show(item);

	/* This is also an easter egg, see the previous comment. */
	item = gtk_menu_item_new_with_label(_("If you look real closely"));
	gtk_menu_shell_prepend(GTK_MENU_SHELL(widget), item);
	gtk_widget_show(item);
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
static void
pidgin_proxy_options_get_property(GObject *obj, guint param_id, GValue *value,
                                  GParamSpec *pspec)
{
	PidginProxyOptions *options = PIDGIN_PROXY_OPTIONS(obj);

	switch(param_id) {
		case PROP_SHOW_GLOBAL:
			g_value_set_boolean(value,
			                    pidgin_proxy_options_get_show_global(options));
			break;
		case PROP_INFO:
			g_value_set_object(value, pidgin_proxy_options_get_info(options));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
pidgin_proxy_options_set_property(GObject *obj, guint param_id,
                                  const GValue *value, GParamSpec *pspec)
{
	PidginProxyOptions *options = PIDGIN_PROXY_OPTIONS(obj);

	switch(param_id) {
		case PROP_SHOW_GLOBAL:
			pidgin_proxy_options_set_show_global(options,
			                                     g_value_get_boolean(value));
			break;
		case PROP_INFO:
			pidgin_proxy_options_set_info(options, g_value_get_object(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
pidgin_proxy_options_init(PidginProxyOptions *options) {
	gtk_widget_init_template(GTK_WIDGET(options));

	/* Set the visible function so we can control the visibility of the "Use
	 * Global Proxy" option.
	 */
	gtk_tree_model_filter_set_visible_func(options->filter,
	                                       pidgin_proxy_options_filter_visible,
	                                       options, NULL);
}

static void
pidgin_proxy_options_constructed(GObject *obj) {
	PidginProxyOptions *options = PIDGIN_PROXY_OPTIONS(obj);

	G_OBJECT_CLASS(pidgin_proxy_options_parent_class)->constructed(obj);

	if(options->info == NULL) {
		pidgin_proxy_options_set_info(options, NULL);
	}
}

static void
pidgin_proxy_options_class_init(PidginProxyOptionsClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	obj_class->get_property = pidgin_proxy_options_get_property;
	obj_class->set_property = pidgin_proxy_options_set_property;
	obj_class->constructed = pidgin_proxy_options_constructed;

	/**
	 * PidginProxyOptions::show-global:
	 *
	 * Whether or not to show the "Use Global Proxy Settings" option. This
	 * is turned off for the preferences where we use this widget to define
	 * the global proxy settings.
	 *
	 * Since: 3.0.0
	 */
	properties[PROP_SHOW_GLOBAL] = g_param_spec_boolean(
		"show-global", "show-global",
		"Whether or not to show the global proxy settings option",
		TRUE,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	/**
	 * PidginProxyOptions::info:
	 *
	 * The [class@Purple.ProxyInfo] that this options widget is configuring. If
	 * unset, a new instance will be created.
	 *
	 * Since: 3.0.0
	 */
	properties[PROP_INFO] = g_param_spec_object(
		"info", "info",
		"The proxy info to configure",
		PURPLE_TYPE_PROXY_INFO,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, N_PROPERTIES, properties);

	gtk_widget_class_set_template_from_resource(
		widget_class,
		"/im/pidgin/Pidgin3/proxyoptions.ui"
	);

	gtk_widget_class_bind_template_child(widget_class, PidginProxyOptions,
	                                     model);
	gtk_widget_class_bind_template_child(widget_class, PidginProxyOptions,
	                                     filter);

	gtk_widget_class_bind_template_child(widget_class, PidginProxyOptions,
	                                     proxy_type);

	gtk_widget_class_bind_template_child(widget_class, PidginProxyOptions,
	                                     options);
	gtk_widget_class_bind_template_child(widget_class, PidginProxyOptions,
	                                     hostname);
	gtk_widget_class_bind_template_child(widget_class, PidginProxyOptions,
	                                     port);
	gtk_widget_class_bind_template_child(widget_class, PidginProxyOptions,
	                                     username);
	gtk_widget_class_bind_template_child(widget_class, PidginProxyOptions,
	                                     password);

	gtk_widget_class_bind_template_callback(widget_class,
	                                        pidgin_proxy_options_proxy_type_changed_cb);
	gtk_widget_class_bind_template_callback(widget_class,
	                                        pidgin_proxy_options_ports_popup_cb);
}

/******************************************************************************
 * Public API
 *****************************************************************************/
GtkWidget *
pidgin_proxy_options_new(void) {
	return g_object_new(PIDGIN_TYPE_PROXY_OPTIONS, NULL);
}

void
pidgin_proxy_options_set_show_global(PidginProxyOptions *options,
                                     gboolean show_global)
{
	PurpleProxyType proxy_type = PURPLE_PROXY_TYPE_USE_GLOBAL;

	g_return_if_fail(PIDGIN_IS_PROXY_OPTIONS(options));

	if(show_global == options->show_global) {
		return;
	}

	options->show_global = show_global;

	if(options->info == NULL) {
		g_object_notify_by_pspec(G_OBJECT(options),
		                         properties[PROP_SHOW_GLOBAL]);

		return;
	}

	proxy_type = purple_proxy_info_get_proxy_type(options->info);
	if(proxy_type == PURPLE_PROXY_TYPE_USE_GLOBAL && show_global == FALSE) {
		proxy_type = PURPLE_PROXY_TYPE_NONE;
	}

	purple_proxy_info_set_proxy_type(options->info, proxy_type);

	options->show_global = show_global;

	g_object_notify_by_pspec(G_OBJECT(options), properties[PROP_SHOW_GLOBAL]);

	/* Tell the filter to rerun. */
	gtk_tree_model_filter_refilter(options->filter);
}

gboolean
pidgin_proxy_options_get_show_global(PidginProxyOptions *options) {
	g_return_val_if_fail(PIDGIN_IS_PROXY_OPTIONS(options), FALSE);

	return options->show_global;
}

void
pidgin_proxy_options_set_info(PidginProxyOptions *options,
                              PurpleProxyInfo *info)
{
	GtkTreeIter iter;
	GtkTreeModel *model;

	g_return_if_fail(PIDGIN_IS_PROXY_OPTIONS(options));

	/* We want to always have a PurpleProxyInfo instance to make management
	 * easier. So if someone clears it, just replace it with an empty one
	 * instead.
	 */
	if(!PURPLE_IS_PROXY_INFO(info)) {
		PurpleProxyInfo *empty_info = purple_proxy_info_new();

		if(!g_set_object(&options->info, empty_info)) {
			g_assert_not_reached();
		}

		g_object_unref(empty_info);
	} else if(!g_set_object(&options->info, info)) {
		return;
	}

	model = GTK_TREE_MODEL(options->filter);
	if(gtk_tree_model_get_iter_first(model, &iter)) {
		do {
			PurpleProxyType type = purple_proxy_info_get_proxy_type(options->info);
			PurpleProxyType row_type;

			gtk_tree_model_get(model, &iter, COLUMN_TYPE, &row_type, -1);
			if(row_type == type) {
				gtk_combo_box_set_active_iter(GTK_COMBO_BOX(options->proxy_type),
				                              &iter);
				break;
			}
		} while(gtk_tree_model_iter_next(model, &iter));
	}

	g_object_bind_property_full(options->info, "hostname",
	                            options->hostname, "text",
	                            G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
	                            pidgin_proxy_options_null_to_empty_str,
	                            pidgin_proxy_options_empty_str_to_null,
	                            NULL,
	                            NULL);

	g_object_bind_property_full(options->info, "port",
	                            options->port, "value",
	                            G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
	                            pidgin_proxy_options_gint_to_double,
	                            pidgin_proxy_options_double_to_gint,
	                            NULL,
	                            NULL);

	g_object_bind_property_full(options->info, "username",
	                            options->username, "text",
	                            G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
	                            pidgin_proxy_options_null_to_empty_str,
	                            pidgin_proxy_options_empty_str_to_null,
	                            NULL,
	                            NULL);


	g_object_bind_property_full(options->info, "password",
	                            options->password, "text",
	                            G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE,
	                            pidgin_proxy_options_null_to_empty_str,
	                            pidgin_proxy_options_empty_str_to_null,
	                            NULL,
	                            NULL);

	g_object_notify_by_pspec(G_OBJECT(options), properties[PROP_INFO]);
}

PurpleProxyInfo *
pidgin_proxy_options_get_info(PidginProxyOptions *options) {
	g_return_val_if_fail(PIDGIN_IS_PROXY_OPTIONS(options), NULL);

	return options->info;
}
