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

#include <purple.h>

#include "pidginaccountmanager.h"

#include "gtkaccount.h"
#include "pidgincore.h"

struct _PidginAccountManager {
	GtkDialog parent;

	GtkListStore *model;
	GtkTreeSelection *selection;

	GtkWidget *modify_button;
	GtkWidget *remove_button;
};

enum {
	RESPONSE_ADD,
	RESPONSE_MODIFY,
	RESPONSE_REMOVE
};

enum {
	COLUMN_ENABLED,
	COLUMN_AVATAR,
	COLUMN_USERNAME,
	COLUMN_PROTOCOL_ICON,
	COLUMN_PROTOCOL_NAME,
	COLUMN_ACCOUNT
};

G_DEFINE_TYPE(PidginAccountManager, pidgin_account_manager, GTK_TYPE_DIALOG)

static void pidgin_account_manager_account_notify_cb(GObject *obj, GParamSpec *pspec, gpointer data);

/******************************************************************************
 * Helpers
 *****************************************************************************/
static gboolean
pidgin_account_manager_find_account(PidginAccountManager *manager,
                                    PurpleAccount *account, GtkTreeIter *iter)
{
	GtkTreeModel *model = GTK_TREE_MODEL(manager->model);

	if(!gtk_tree_model_get_iter_first(model, iter)) {
		return FALSE;
	}

	do {
		PurpleAccount *current = NULL;

		gtk_tree_model_get(model, iter,
		                   COLUMN_ACCOUNT, &current,
		                   -1);

		if(current == account) {
			g_clear_object(&current);

			return TRUE;
		}

		g_clear_object(&current);
	} while(gtk_tree_model_iter_next(model, iter));

	return FALSE;
}

static PurpleAccount *
pidgin_account_manager_get_selected_account(PidginAccountManager *manager) {
	PurpleAccount *account = NULL;
	GtkTreeIter iter;

	if(gtk_tree_selection_count_selected_rows(manager->selection) == 0) {
		return NULL;
	}

	gtk_tree_selection_get_selected(manager->selection, NULL, &iter);

	gtk_tree_model_get(GTK_TREE_MODEL(manager->model), &iter,
	                   COLUMN_ACCOUNT, &account,
	                   -1);

	return account;
}

static void
pidgin_account_manager_refresh_account(PidginAccountManager *manager,
                                       PurpleAccount *account,
                                       GtkTreeIter *iter)
{
	PurpleImage *image = NULL;
	PurpleProtocol *protocol = NULL;
	GdkPixbuf *avatar = NULL;
	const gchar *protocol_icon = NULL, *protocol_name = NULL;

	/* Try to find the avatar for the account. */
	image = purple_buddy_icons_find_account_icon(account);
	if(image != NULL) {
		GdkPixbuf *raw = NULL;

		raw = purple_gdk_pixbuf_from_image(image);
		g_object_unref(image);

		avatar = gdk_pixbuf_scale_simple(raw, 22, 22, GDK_INTERP_HYPER);
		g_clear_object(&raw);
	}

	/* Get the protocol fields. */
	protocol = purple_account_get_protocol(account);
	if(PURPLE_IS_PROTOCOL(protocol)) {
		protocol_name = purple_protocol_get_name(protocol);
		protocol_icon = purple_protocol_get_icon_name(protocol);
	} else {
		protocol_name = _("Unknown");
	}

	gtk_list_store_set(manager->model, iter,
	                   COLUMN_ENABLED, purple_account_get_enabled(account),
	                   COLUMN_AVATAR, avatar,
	                   COLUMN_USERNAME, purple_account_get_username(account),
	                   COLUMN_PROTOCOL_ICON, protocol_icon,
	                   COLUMN_PROTOCOL_NAME, protocol_name,
	                   COLUMN_ACCOUNT, account,
	                   -1);

	g_clear_object(&avatar);
}

static void
pidgin_account_manager_update_account(PidginAccountManager *manager,
                                      PurpleAccount *account)
{
	GtkTreeIter iter;

	if(pidgin_account_manager_find_account(manager, account, &iter)) {
		pidgin_account_manager_refresh_account(manager, account, &iter);
	}
}

static void
pidgin_account_manager_add_account(PidginAccountManager *manager,
                                   PurpleAccount *account)
{
	GtkTreeIter iter;

	gtk_list_store_append(manager->model, &iter);

	pidgin_account_manager_refresh_account(manager, account, &iter);

	g_signal_connect_object(account, "notify",
	                        G_CALLBACK(pidgin_account_manager_account_notify_cb),
	                        manager, 0);
}

static void
pidgin_account_manager_populate_helper(PurpleAccount *account, gpointer data) {
	pidgin_account_manager_add_account(PIDGIN_ACCOUNT_MANAGER(data), account);
}

static void
pidgin_account_manager_populate(PidginAccountManager *manager) {
	purple_account_manager_foreach(purple_account_manager_get_default(),
	                               pidgin_account_manager_populate_helper,
	                               manager);
}

/******************************************************************************
 * Callbacks
 *****************************************************************************/
static void
pidgin_account_manager_account_notify_cb(GObject *obj,
                                         G_GNUC_UNUSED GParamSpec *pspec,
                                         gpointer data)
{
	PidginAccountManager *manager = PIDGIN_ACCOUNT_MANAGER(data);
	PurpleAccount *account = PURPLE_ACCOUNT(obj);

	pidgin_account_manager_update_account(manager, account);
}

static void
pidgin_account_manager_response_cb(GtkDialog *dialog, gint response_id,
                                   G_GNUC_UNUSED gpointer data)
{
	PidginAccountManager *manager = PIDGIN_ACCOUNT_MANAGER(dialog);
	PurpleAccount *account = NULL;

	switch(response_id) {
		case RESPONSE_ADD:
			pidgin_account_dialog_show(PIDGIN_ADD_ACCOUNT_DIALOG, NULL);
			break;
		case RESPONSE_MODIFY:
			account = pidgin_account_manager_get_selected_account(manager);

			pidgin_account_dialog_show(PIDGIN_MODIFY_ACCOUNT_DIALOG, account);

			g_clear_object(&account);

			break;
		case RESPONSE_REMOVE:
			account = pidgin_account_manager_get_selected_account(manager);

			purple_accounts_delete(account);

			g_clear_object(&account);

			break;
		case GTK_RESPONSE_CLOSE:
			gtk_widget_destroy(GTK_WIDGET(dialog));
			break;
		default:
			g_warning("not sure how you got here...");
	}
}

static void
pidgin_account_manager_selection_changed_cb(GtkTreeSelection *selection,
                                            gpointer data)
{
	PidginAccountManager *manager = data;
	gboolean sensitive = TRUE;

	if(gtk_tree_selection_count_selected_rows(selection) == 0) {
		sensitive = FALSE;
	}

	gtk_widget_set_sensitive(manager->modify_button, sensitive);
	gtk_widget_set_sensitive(manager->remove_button, sensitive);
}

static void
pidgin_account_manager_row_activated_cb(G_GNUC_UNUSED GtkTreeView *tree_view,
                                        GtkTreePath *path,
                                        G_GNUC_UNUSED GtkTreeViewColumn *column,
                                        gpointer data)
{
	PidginAccountManager *manager = data;
	GtkTreeIter iter;

	if(gtk_tree_model_get_iter(GTK_TREE_MODEL(manager->model), &iter, path)) {
		PurpleAccount *account = NULL;

		gtk_tree_model_get(GTK_TREE_MODEL(manager->model), &iter,
		                   COLUMN_ACCOUNT, &account,
		                   -1);

		pidgin_account_dialog_show(PIDGIN_MODIFY_ACCOUNT_DIALOG, account);

		g_clear_object(&account);
	}
}

static void
pidgin_account_manager_enable_toggled_cb(G_GNUC_UNUSED GtkCellRendererToggle *renderer,
                                         gchar *path, gpointer data)
{
	PidginAccountManager *manager = data;
	GtkTreeModel *model = GTK_TREE_MODEL(manager->model);
	GtkTreeIter iter;

	if(gtk_tree_model_get_iter_from_string(model, &iter, path)) {
		PurpleAccount *account = NULL;
		gboolean enabled = FALSE;

		/* The value of enabled in the model is the old value, so if enabled
		 * is currently set to TRUE, we are disabling the account and vice
		 * versa.
		 */
		gtk_tree_model_get(model, &iter,
		                   COLUMN_ENABLED, &enabled,
		                   COLUMN_ACCOUNT, &account,
		                   -1);

		/* The account was just enabled, so set its status. */
		if(!enabled) {
			PurpleSavedStatus *status = purple_savedstatus_get_current();
			purple_savedstatus_activate_for_account(status, account);
		}

		purple_account_set_enabled(account, !enabled);

		/* We don't update the model here, as it's updated via the notify
		 * signal.
		 */
	}
}

static void
pidgin_account_manager_account_added_cb(G_GNUC_UNUSED PurpleAccountManager *purple_manager,
                                        PurpleAccount *account,
                                        gpointer data)
{
	PidginAccountManager *manager = data;

	pidgin_account_manager_add_account(manager, account);
}

static void
pidgin_account_manager_account_removed_cb(G_GNUC_UNUSED PurpleAccountManager *purple_manager,
                                          PurpleAccount *account,
                                          gpointer data)
{
	PidginAccountManager *manager = data;
	GtkTreeIter iter;

	if(pidgin_account_manager_find_account(manager, account, &iter)) {
		gtk_list_store_remove(manager->model, &iter);
	}
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
static void
pidgin_account_manager_init(PidginAccountManager *manager) {
	PurpleAccountManager *purple_manager = NULL;

	gtk_widget_init_template(GTK_WIDGET(manager));

	pidgin_account_manager_populate(manager);

	purple_manager = purple_account_manager_get_default();
	g_signal_connect_object(purple_manager, "added",
	                        G_CALLBACK(pidgin_account_manager_account_added_cb),
	                        manager, 0);
	g_signal_connect_object(purple_manager, "removed",
	                        G_CALLBACK(pidgin_account_manager_account_removed_cb),
	                        manager, 0);
}

static void
pidgin_account_manager_class_init(PidginAccountManagerClass *klass) {
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(
		widget_class,
		"/im/pidgin/Pidgin3/Accounts/manager.ui"
	);

	gtk_widget_class_bind_template_child(widget_class, PidginAccountManager,
	                                     model);
	gtk_widget_class_bind_template_child(widget_class, PidginAccountManager,
	                                     selection);

	gtk_widget_class_bind_template_child(widget_class, PidginAccountManager,
	                                     modify_button);
	gtk_widget_class_bind_template_child(widget_class, PidginAccountManager,
	                                     remove_button);

	gtk_widget_class_bind_template_callback(widget_class,
	                                        pidgin_account_manager_enable_toggled_cb);
	gtk_widget_class_bind_template_callback(widget_class,
	                                        pidgin_account_manager_response_cb);
	gtk_widget_class_bind_template_callback(widget_class,
	                                        pidgin_account_manager_row_activated_cb);
	gtk_widget_class_bind_template_callback(widget_class,
	                                        pidgin_account_manager_selection_changed_cb);
}

/******************************************************************************
 * Public API
 *****************************************************************************/
GtkWidget *
pidgin_account_manager_new(void) {
	return g_object_new(PIDGIN_TYPE_ACCOUNT_MANAGER, NULL);
}
