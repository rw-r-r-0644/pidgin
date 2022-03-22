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

#include "pidginstatusmanager.h"

#include "gtksavedstatuses.h"
#include "pidginiconname.h"

enum {
	RESPONSE_USE,
	RESPONSE_ADD,
	RESPONSE_MODIFY,
	RESPONSE_REMOVE
};

enum {
	COLUMN_TITLE,
	COLUMN_ICON_NAME,
	COLUMN_TYPE,
	COLUMN_MESSAGE,
	COLUMN_STATUS
};

struct _PidginStatusManager {
	GtkDialog parent;

	GtkListStore *model;
	GtkTreeSelection *selection;

	GtkWidget *use_button;
	GtkWidget *modify_button;
	GtkWidget *remove_button;
};

G_DEFINE_TYPE(PidginStatusManager, pidgin_status_manager, GTK_TYPE_DIALOG)

/******************************************************************************
 * Helpers
 *****************************************************************************/
static PurpleSavedStatus *
pidgin_status_manager_get_selected_status(PidginStatusManager *manager) {
	PurpleSavedStatus *status = NULL;
	GtkTreeIter iter;

	if(gtk_tree_selection_count_selected_rows(manager->selection) == 0) {
		return NULL;
	}

	gtk_tree_selection_get_selected(manager->selection, NULL, &iter);
	gtk_tree_model_get(GTK_TREE_MODEL(manager->model), &iter,
	                   COLUMN_STATUS, &status,
	                   -1);

	return status;
}

static void
pidgin_status_manager_add(PidginStatusManager *manager,
                          PurpleSavedStatus *status)
{
	PurpleStatusPrimitive primitive;
	GtkTreeIter iter;
	gchar *message = NULL;
	const gchar *icon_name = NULL, *type = NULL;

	message = purple_markup_strip_html(purple_savedstatus_get_message(status));

	primitive = purple_savedstatus_get_primitive_type(status);
	icon_name = pidgin_icon_name_from_status_primitive(primitive, NULL);
	type = purple_primitive_get_name_from_type(primitive);

	gtk_list_store_append(manager->model, &iter);
	gtk_list_store_set(manager->model, &iter,
	                   COLUMN_TITLE, purple_savedstatus_get_title(status),
	                   COLUMN_ICON_NAME, icon_name,
	                   COLUMN_TYPE, type,
	                   COLUMN_MESSAGE, message,
	                   COLUMN_STATUS, status,
	                   -1);

	g_free(message);
}

static void
pidgin_status_manager_populate_helper(gpointer data, gpointer user_data) {
	PidginStatusManager *manager = user_data;
	PurpleSavedStatus *status = data;

	if(!purple_savedstatus_is_transient(status)) {
		pidgin_status_manager_add(manager, status);
	}
}

static void
pidgin_status_manager_refresh(PidginStatusManager *manager) {
	GList *statuses = NULL;

	gtk_list_store_clear(manager->model);

	statuses = purple_savedstatuses_get_all();
	g_list_foreach(statuses, pidgin_status_manager_populate_helper, manager);
}

/******************************************************************************
 * Callbacks
 *****************************************************************************/
static void
pidgin_status_manager_response_cb(GtkDialog *dialog, gint response_id,
                                  gpointer data)
{
	PidginStatusManager *manager = data;
	PurpleSavedStatus *status = NULL;

	switch(response_id) {
		case RESPONSE_USE:
			status = pidgin_status_manager_get_selected_status(manager);

			purple_savedstatus_activate(status);

			break;
		case RESPONSE_ADD:
			pidgin_status_editor_show(FALSE, NULL);
			break;
		case RESPONSE_MODIFY:
			status = pidgin_status_manager_get_selected_status(manager);

			pidgin_status_editor_show(TRUE, status);

			break;
		case RESPONSE_REMOVE:
			status = pidgin_status_manager_get_selected_status(manager);

			purple_savedstatus_delete_by_status(status);

			break;
		case GTK_RESPONSE_CLOSE:
			gtk_widget_destroy(GTK_WIDGET(dialog));
			break;
	}
}

static void
pidgin_status_manager_row_activated_cb(G_GNUC_UNUSED GtkTreeView *tree_view,
                                       GtkTreePath *path,
                                       G_GNUC_UNUSED GtkTreeViewColumn *column,
                                       gpointer data)
{
	PidginStatusManager *manager = data;
	GtkTreeIter iter;

	if(gtk_tree_model_get_iter(GTK_TREE_MODEL(manager->model), &iter, path)) {
		PurpleSavedStatus *status = NULL;

		gtk_tree_model_get(GTK_TREE_MODEL(manager->model), &iter,
		                   COLUMN_STATUS, &status,
		                   -1);

		pidgin_status_editor_show(TRUE, status);
	}
}

static void
pidgin_status_manager_selection_changed_cb(GtkTreeSelection *selection,
                                           gpointer data)
{
	PidginStatusManager *manager = data;
	gboolean sensitive = TRUE;

	if(gtk_tree_selection_count_selected_rows(selection) == 0) {
		sensitive = FALSE;
	}

	gtk_widget_set_sensitive(manager->use_button, sensitive);
	gtk_widget_set_sensitive(manager->modify_button, sensitive);

	/* Only enable the remove button if the currently selected row is not the
	 * currently active status.
	 */
	if(sensitive) {
		PurpleSavedStatus *status = NULL;

		status = pidgin_status_manager_get_selected_status(manager);

		sensitive = status != purple_savedstatus_get_current();
	}

	gtk_widget_set_sensitive(manager->remove_button, sensitive);
}

static void
pidgin_status_manager_savedstatus_changed_cb(PurpleSavedStatus *new_status,
                                             G_GNUC_UNUSED PurpleSavedStatus *old_status,
                                             gpointer data)
{
	PidginStatusManager *manager = data;
	PurpleSavedStatus *selected = NULL;

	/* Disable the remove button if the selected status is the currently active
	 * status.
	 */
	selected = pidgin_status_manager_get_selected_status(manager);
	if(selected != NULL) {
		gboolean sensitive = selected != new_status;

		gtk_widget_set_sensitive(manager->remove_button, sensitive);
	}
}

static void
pidgin_status_manager_savedstatus_updated_cb(G_GNUC_UNUSED PurpleSavedStatus *status,
                                             gpointer data)
{
	PidginStatusManager *manager = data;

	pidgin_status_manager_refresh(manager);
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
static void
pidgin_status_manager_finalize(GObject *obj) {
	purple_signals_disconnect_by_handle(obj);

	G_OBJECT_CLASS(pidgin_status_manager_parent_class)->finalize(obj);
}

static void
pidgin_status_manager_init(PidginStatusManager *manager) {
	gpointer handle = NULL;

	gtk_widget_init_template(GTK_WIDGET(manager));

	pidgin_status_manager_refresh(manager);

	handle = purple_savedstatuses_get_handle();
	purple_signal_connect(handle, "savedstatus-changed", manager,
	                      PURPLE_CALLBACK(pidgin_status_manager_savedstatus_changed_cb),
	                      manager);
	purple_signal_connect(handle, "savedstatus-added", manager,
	                      PURPLE_CALLBACK(pidgin_status_manager_savedstatus_updated_cb),
	                      manager);
	purple_signal_connect(handle, "savedstatus-deleted", manager,
	                      PURPLE_CALLBACK(pidgin_status_manager_savedstatus_updated_cb),
	                      manager);
	purple_signal_connect(handle, "savedstatus-modified", manager,
	                      PURPLE_CALLBACK(pidgin_status_manager_savedstatus_updated_cb),
	                      manager);
}

static void
pidgin_status_manager_class_init(PidginStatusManagerClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	obj_class->finalize = pidgin_status_manager_finalize;

	gtk_widget_class_set_template_from_resource(
		widget_class,
		"/im/pidgin/Pidgin3/Status/manager.ui"
	);

	gtk_widget_class_bind_template_child(widget_class, PidginStatusManager,
	                                     model);
	gtk_widget_class_bind_template_child(widget_class, PidginStatusManager,
	                                     selection);
	gtk_widget_class_bind_template_child(widget_class, PidginStatusManager,
	                                     use_button);
	gtk_widget_class_bind_template_child(widget_class, PidginStatusManager,
	                                     modify_button);
	gtk_widget_class_bind_template_child(widget_class, PidginStatusManager,
	                                     remove_button);

	gtk_widget_class_bind_template_callback(widget_class,
	                                        pidgin_status_manager_response_cb);
	gtk_widget_class_bind_template_callback(widget_class,
	                                        pidgin_status_manager_row_activated_cb);
	gtk_widget_class_bind_template_callback(widget_class,
	                                        pidgin_status_manager_selection_changed_cb);
}

/******************************************************************************
 * Public API
 *****************************************************************************/
GtkWidget *
pidgin_status_manager_new(void) {
	return g_object_new(PIDGIN_TYPE_STATUS_MANAGER, NULL);
}
