/* pidgin
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include <glib/gi18n-lib.h>

#include <talkatu.h>

#include <purple.h>

#include "gtkblist.h"
#include "gtksavedstatuses.h"
#include "gtkutils.h"
#include "pidgincore.h"
#include "pidgindialog.h"
#include "pidginiconname.h"

#include "pidginstatusprimitivechooser.h"
#include "pidginstatusprimitivestore.h"

/*
 * TODO: Should attach to the account-deleted and account-added signals
 *       and update the GtkListStores in any StatusEditor windows that
 *       may be open.
 */

/*
 * These are used for the GtkTreeView when you're scrolling through
 * all your saved statuses.
 */
enum
{
	STATUS_WINDOW_COLUMN_TITLE,
	STATUS_WINDOW_COLUMN_TYPE,
	STATUS_WINDOW_COLUMN_MESSAGE,
	/** A hidden column containing a pointer to the editor for this saved status. */
	STATUS_WINDOW_COLUMN_WINDOW,
	STATUS_WINDOW_COLUMN_ICON,
	STATUS_WINDOW_NUM_COLUMNS
};

enum {
	STATUS_RESPONSE_USE = 1,
	STATUS_RESPONSE_ADD,
	STATUS_RESPONSE_MODIFY,
	STATUS_RESPONSE_DELETE,
	STATUS_RESPONSE_SAVE,
	STATUS_RESPONSE_SAVE_AND_USE,
};

/*
 * These are used for the GtkTreeView containing the list of accounts
 * at the bottom of the window when you're editing a particular
 * saved status.
 */
enum
{
	/* A hidden column containing a pointer to the PurpleAccount. */
	STATUS_EDITOR_COLUMN_ACCOUNT,
	/* A hidden column containing a pointer to the editor for this substatus. */
	STATUS_EDITOR_COLUMN_WINDOW,
	STATUS_EDITOR_COLUMN_ENABLE_SUBSTATUS,
	STATUS_EDITOR_COLUMN_ICON,
	STATUS_EDITOR_COLUMN_USERNAME,
	/* A hidden column containing the ID of this PurpleStatusType. */
	STATUS_EDITOR_COLUMN_STATUS_ID,
	STATUS_EDITOR_COLUMN_STATUS_NAME,
	STATUS_EDITOR_COLUMN_STATUS_MESSAGE,
	STATUS_EDITOR_COLUMN_STATUS_ICON,
	STATUS_EDITOR_NUM_COLUMNS
};

/*
 * These are used in the GtkComboBox to select the specific PurpleStatusType
 * when setting a (sub)status for a particular saved status.
 */
enum
{
	STATUS_COLUMN_ICON,
	/** A hidden column containing the ID of this PurpleStatusType. */
	STATUS_COLUMN_STATUS_ID,
	STATUS_COLUMN_STATUS_NAME,
	STATUS_NUM_COLUMNS
};

typedef struct
{
	GtkWidget *window;
	GtkListStore *model;
	GtkWidget *treeview;
	GtkWidget *use_button;
	GtkWidget *modify_button;
	GtkWidget *delete_button;
} StatusWindow;

typedef struct
{
	GtkWidget *window;
	GtkListStore *model;
	GtkWidget *treeview;
	GtkWidget *saveanduse_button;
	GtkWidget *save_button;

	gchar *original_title;
	GtkEntry *title;
	GtkComboBox *type;
	GtkWidget *message_view;
	GtkTextBuffer *message_buffer;
} StatusEditor;

typedef struct
{
	StatusEditor *status_editor;
	PurpleAccount *account;

	GtkWidget *window;
	GtkComboBox *box;
	GtkWidget *message_view;
	GtkTextBuffer *message_buffer;
} SubStatusEditor;

static StatusWindow *status_window = NULL;


/**************************************************************************
* Status window
**************************************************************************/

static gboolean
status_window_find_savedstatus(GtkTreeIter *iter, const char *title)
{
	GtkTreeModel *model;
	char *cur;

	if ((status_window == NULL) || (title == NULL))
		return FALSE;

	model = GTK_TREE_MODEL(status_window->model);

	if (!gtk_tree_model_get_iter_first(model, iter))
		return FALSE;

	do {
		gtk_tree_model_get(model, iter, STATUS_WINDOW_COLUMN_TITLE, &cur, -1);
		if (purple_strequal(title, cur))
		{
			g_free(cur);
			return TRUE;
		}
		g_free(cur);
	} while (gtk_tree_model_iter_next(model, iter));

	return FALSE;
}

static gboolean
status_window_destroy_cb(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	StatusWindow *dialog = user_data;

	dialog->window = NULL;
	pidgin_status_window_hide();

	return FALSE;
}

static void
status_window_use(StatusWindow *dialog)
{
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GList *list = NULL;
	int num_selected = 0;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->treeview));

	num_selected = gtk_tree_selection_count_selected_rows(selection);
	if (num_selected != 1)
		/*
		 * This shouldn't happen because the "Use" button should have
		 * been grayed out.  Oh well.
		 */
		return;

	list = gtk_tree_selection_get_selected_rows(selection, NULL);

	if (gtk_tree_model_get_iter(GTK_TREE_MODEL(dialog->model),
								&iter, list->data))
	{
		gchar *title;
		PurpleSavedStatus *saved_status;
		gtk_tree_model_get(GTK_TREE_MODEL(dialog->model), &iter,
						   STATUS_WINDOW_COLUMN_TITLE, &title,
						   -1);
		saved_status = purple_savedstatus_find(title);
		g_free(title);
		purple_savedstatus_activate(saved_status);
	}

	g_list_free_full(list, (GDestroyNotify)gtk_tree_path_free);
}

static void
status_window_modify_foreach(GtkTreeModel *model, GtkTreePath *path,
							 GtkTreeIter *iter, gpointer user_data)
{
	gchar *title;
	PurpleSavedStatus *saved_status;

	gtk_tree_model_get(model, iter, STATUS_WINDOW_COLUMN_TITLE, &title, -1);
	saved_status = purple_savedstatus_find(title);
	g_free(title);
	pidgin_status_editor_show(TRUE, saved_status);
}

static void
status_window_modify(StatusWindow *dialog) {
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->treeview));

	gtk_tree_selection_selected_foreach(selection,
	                                    status_window_modify_foreach, NULL);
}

static void
status_window_delete_cancel_cb(gpointer data)
{
	GList *sel_titles = data;
	g_list_free_full(sel_titles, g_free);
}

static void
status_window_delete_confirm_cb(gpointer data)
{
	GtkTreeIter iter;
	GList *sel_titles = data, *l;
	char *title;

	for (l = sel_titles; l != NULL; l = l->next) {
		title = l->data;
		if (purple_savedstatus_find(title) != purple_savedstatus_get_current()) {
			if (status_window_find_savedstatus(&iter, title))
				gtk_list_store_remove(status_window->model, &iter);
			purple_savedstatus_delete(title);
		}
		g_free(title);
	}
	g_list_free(sel_titles);
}

static void
status_window_delete(StatusWindow *dialog) {
	GtkTreeIter iter;
	GtkTreeSelection *selection;
	GList *sel_paths, *l, *sel_titles = NULL;
	GtkTreeModel *model = GTK_TREE_MODEL(dialog->model);
	char *title;
	gpointer handle;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->treeview));
	sel_paths = gtk_tree_selection_get_selected_rows(selection, NULL);

	/* This is ugly because we're not allowed to modify the model from within
	 * gtk_tree_selection_selected_foreach() and the GtkTreePaths can become invalid
	 * when something is removed from the model.  The selection can also change while
	 * the request dialog is displayed, so we need to capture the selected rows at this time. */

	for (l = sel_paths; l != NULL; l = l->next) {
		if (gtk_tree_model_get_iter(model, &iter, l->data)) {
			gtk_tree_model_get(model, &iter, STATUS_WINDOW_COLUMN_TITLE, &title, -1);
			sel_titles = g_list_prepend(sel_titles, title);
		}
		gtk_tree_path_free(l->data);
	}
	g_list_free(sel_paths);

	g_return_if_fail(sel_titles != NULL);
	if (!sel_titles->next) {
		title = g_strdup_printf(_("Are you sure you want to delete %s?"),
				(const gchar *)sel_titles->data);
		handle = purple_savedstatus_find(sel_titles->data);
	} else {
		title = g_strdup(_("Are you sure you want to delete the selected saved statuses?"));
		handle = dialog;
	}

	purple_request_action(handle, NULL, title, NULL, 0,
		 NULL,
		 sel_titles, 2,
		_("Delete"), status_window_delete_confirm_cb,
		_("Cancel"), status_window_delete_cancel_cb);

	g_free(title);
}

static void
status_selected_cb(GtkTreeSelection *sel, gpointer user_data)
{
	StatusWindow *dialog = user_data;
	GList *sel_paths, *tmp;
	gboolean can_use = TRUE, can_delete = TRUE;
	int num_selected;
	GtkTreeModel *model = GTK_TREE_MODEL(dialog->model);

	sel_paths = gtk_tree_selection_get_selected_rows(sel, NULL);

	for (tmp = sel_paths, num_selected = 0; tmp; tmp = tmp->next, num_selected++) {
		GtkTreeIter iter;
		char *title;

		if (gtk_tree_model_get_iter(model, &iter, tmp->data)) {
			gtk_tree_model_get(model, &iter,
					STATUS_WINDOW_COLUMN_TITLE, &title, -1);
			if (purple_savedstatus_find(title) == purple_savedstatus_get_current()) {
				can_use = can_delete = FALSE;
			}

			g_free(title);
		}

		gtk_tree_path_free(tmp->data);
	}

	gtk_widget_set_sensitive(dialog->use_button, (num_selected == 1) && can_use);
	gtk_widget_set_sensitive(dialog->modify_button, (num_selected > 0));
	gtk_widget_set_sensitive(dialog->delete_button, num_selected > 0 && can_delete);

    g_list_free(sel_paths);
}

static void
add_status_to_saved_status_list(GtkListStore *model, PurpleSavedStatus *saved_status)
{
	GtkTreeIter iter;
	const char *title;
	const char *type;
	const gchar *icon;
	char *message;

	if (purple_savedstatus_is_transient(saved_status))
		return;

	title = purple_savedstatus_get_title(saved_status);
	type = purple_primitive_get_name_from_type(purple_savedstatus_get_primitive_type(saved_status));
	message = purple_markup_strip_html(purple_savedstatus_get_message(saved_status));
	icon = pidgin_icon_name_from_status_primitive(purple_savedstatus_get_primitive_type(saved_status), NULL);

	gtk_list_store_append(model, &iter);
	gtk_list_store_set(model, &iter,
					   STATUS_WINDOW_COLUMN_ICON, icon,
					   STATUS_WINDOW_COLUMN_TITLE, title,
					   STATUS_WINDOW_COLUMN_TYPE, type,
					   STATUS_WINDOW_COLUMN_MESSAGE, message,
					   -1);
	g_free(message);
}

static void
populate_saved_status_list(StatusWindow *dialog)
{
	GList *saved_statuses;

	gtk_list_store_clear(dialog->model);

	for (saved_statuses = purple_savedstatuses_get_all(); saved_statuses != NULL;
			saved_statuses = g_list_next(saved_statuses))
	{
	  add_status_to_saved_status_list(dialog->model, saved_statuses->data);
	}
}

static gboolean
search_func(GtkTreeModel *model, gint column, const gchar *key, GtkTreeIter *iter, gpointer search_data)
{
	gboolean result;
	char *haystack;

	gtk_tree_model_get(model, iter, column, &haystack, -1);

	result = (purple_strcasestr(haystack, key) == NULL);

	g_free(haystack);

	return result;
}

static void
savedstatus_activated_cb(GtkTreeView *view, GtkTreePath *path, GtkTreeViewColumn *column, StatusWindow *dialog)
{
	status_window_use(dialog);
	pidgin_status_window_hide();
}

static void
saved_status_updated_cb(PurpleSavedStatus *status, StatusWindow *sw)
{
	populate_saved_status_list(sw);
}

static GtkWidget *
create_saved_status_list(StatusWindow *dialog)
{
	GtkWidget *treeview;
	GtkTreeSelection *sel;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	/* Create the list model */
	dialog->model = gtk_list_store_new(STATUS_WINDOW_NUM_COLUMNS,
									   G_TYPE_STRING,
									   G_TYPE_STRING,
									   G_TYPE_STRING,
									   G_TYPE_POINTER,
									   G_TYPE_STRING);

	/* Create the treeview */
	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(dialog->model));
	dialog->treeview = treeview;
	g_signal_connect(G_OBJECT(treeview), "row-activated",
						G_CALLBACK(savedstatus_activated_cb), dialog);

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	gtk_tree_selection_set_mode(sel, GTK_SELECTION_MULTIPLE);
	g_signal_connect(G_OBJECT(sel), "changed",
					 G_CALLBACK(status_selected_cb), dialog);

	/* Add columns */
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Title"));
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_min_width(column, 100);
	gtk_tree_view_column_set_sort_column_id(column,
											STATUS_WINDOW_COLUMN_TITLE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text",
									   STATUS_WINDOW_COLUMN_TITLE);
	g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Type"));
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column,
											STATUS_WINDOW_COLUMN_TYPE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "icon-name",
	                                   STATUS_WINDOW_COLUMN_ICON);
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text",
									   STATUS_WINDOW_COLUMN_TYPE);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Message"));
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_sort_column_id(column,
											STATUS_WINDOW_COLUMN_MESSAGE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text",
									   STATUS_WINDOW_COLUMN_MESSAGE);
	g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	/* Enable CTRL+F searching */
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(treeview), STATUS_WINDOW_COLUMN_TITLE);
	gtk_tree_view_set_search_equal_func(GTK_TREE_VIEW(treeview), search_func, NULL, NULL);

	/* Sort the title column by default */
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(dialog->model),
										 STATUS_WINDOW_COLUMN_TITLE,
										 GTK_SORT_ASCENDING);

	/* Populate list */
	populate_saved_status_list(dialog);

	gtk_widget_show_all(treeview);

	return pidgin_make_scrollable(treeview, GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS, GTK_SHADOW_IN, -1, -1);
}

static void
response_cb(GtkDialog *dialog, gint response_id, gpointer data) {
	StatusWindow *window = data;

	switch(response_id) {
		case STATUS_RESPONSE_USE:
			status_window_use(window);
			break;
		case STATUS_RESPONSE_ADD:
			pidgin_status_editor_show(FALSE, NULL);
			break;
		case STATUS_RESPONSE_MODIFY:
			status_window_modify(window);
			break;
		case STATUS_RESPONSE_DELETE:
			status_window_delete(window);
			break;
		case GTK_RESPONSE_CLOSE:
			pidgin_status_window_hide();
			break;
		default:
			break;
	}
}

static void
current_status_changed(PurpleSavedStatus *old, PurpleSavedStatus *new_status,
		StatusWindow *dialog)
{
	status_selected_cb(gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->treeview)), dialog);
}

void
pidgin_status_window_show(void)
{
	StatusWindow *dialog;
	GtkWidget *list;
	GtkWidget *vbox;
	GtkWidget *win;

	if (status_window != NULL)
	{
		gtk_window_present(GTK_WINDOW(status_window->window));
		return;
	}

	status_window = dialog = g_new0(StatusWindow, 1);

	dialog->window = win = pidgin_dialog_new(_("Saved Statuses"), 12, "statuses", TRUE);
	gtk_window_set_default_size(GTK_WINDOW(win), 550, 250);

	g_signal_connect(win, "delete_event", G_CALLBACK(status_window_destroy_cb),
	                 dialog);
	g_signal_connect(win, "response", G_CALLBACK(response_cb), dialog);

	/* Setup the vbox */
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(win));
	gtk_box_set_spacing(GTK_BOX(vbox), 12);

	/* List of saved status states */
	list = create_saved_status_list(dialog);
	gtk_box_pack_start(GTK_BOX(vbox), list, TRUE, TRUE, 0);

	/* Add our buttons */
	dialog->use_button = gtk_dialog_add_button(GTK_DIALOG(win), _("_Use"),
	                                           STATUS_RESPONSE_USE);
	gtk_widget_set_sensitive(dialog->use_button, FALSE);

	gtk_dialog_add_button(GTK_DIALOG(win), _("_Add"), STATUS_RESPONSE_ADD);

	dialog->modify_button = gtk_dialog_add_button(GTK_DIALOG(win),
	                                              _("_Modify"),
	                                              STATUS_RESPONSE_MODIFY);
	gtk_widget_set_sensitive(dialog->modify_button, FALSE);

	dialog->delete_button = gtk_dialog_add_button(GTK_DIALOG(win), _("Delete"),
	                                              STATUS_RESPONSE_DELETE);
	gtk_widget_set_sensitive(dialog->delete_button, FALSE);

	gtk_dialog_add_button(GTK_DIALOG(win), _("Close"), GTK_RESPONSE_CLOSE);

	purple_signal_connect(purple_savedstatuses_get_handle(),
			"savedstatus-changed", status_window,
			G_CALLBACK(current_status_changed), dialog);
	purple_signal_connect(purple_savedstatuses_get_handle(),
			"savedstatus-added", status_window,
			G_CALLBACK(saved_status_updated_cb), dialog);
	purple_signal_connect(purple_savedstatuses_get_handle(),
			"savedstatus-deleted", status_window,
			G_CALLBACK(saved_status_updated_cb), dialog);
	purple_signal_connect(purple_savedstatuses_get_handle(),
			"savedstatus-modified", status_window,
			G_CALLBACK(saved_status_updated_cb), dialog);

	gtk_widget_show_all(win);
}

void
pidgin_status_window_hide(void)
{
	if (status_window == NULL)
		return;

	if (status_window->window != NULL)
		gtk_widget_destroy(status_window->window);

	purple_request_close_with_handle(status_window);
	purple_notify_close_with_handle(status_window);
	purple_signals_disconnect_by_handle(status_window);
	g_object_unref(G_OBJECT(status_window->model));
	g_free(status_window);
	status_window = NULL;
}


/**************************************************************************
* Status editor
**************************************************************************/

static void substatus_editor_cancel(SubStatusEditor *dialog);

static void
status_editor_remove_dialog(StatusEditor *dialog)
{
	GtkTreeModel *model;
	GtkTreeIter iter;

	/* Remove the reference to this dialog from our parent's list store */
	if (status_window_find_savedstatus(&iter, dialog->original_title))
	{
		gtk_list_store_set(status_window->model, &iter,
							STATUS_WINDOW_COLUMN_WINDOW, NULL,
							-1);
	}

	/* Close any substatus editors that may be open */
	model = GTK_TREE_MODEL(dialog->model);
	if (gtk_tree_model_get_iter_first(model, &iter))
	{
		do {
			SubStatusEditor *substatus_dialog;

			gtk_tree_model_get(model, &iter,
							   STATUS_EDITOR_COLUMN_WINDOW, &substatus_dialog,
							   -1);
			if (substatus_dialog != NULL)
			{
				gtk_list_store_set(dialog->model, &iter,
								   STATUS_EDITOR_COLUMN_WINDOW, NULL,
								   -1);
				substatus_editor_cancel(substatus_dialog);
			}
		} while (gtk_tree_model_iter_next(model, &iter));
	}
}


static void
status_editor_destroy_cb(GtkWidget *widget, gpointer user_data)
{
	StatusEditor *dialog = user_data;

	status_editor_remove_dialog(dialog);
	g_free(dialog->original_title);
	g_object_unref(G_OBJECT(dialog->model));
	g_free(dialog);
}

static void
status_editor_ok(StatusEditor *dialog, gboolean use, gboolean save) {
	const char *title;
	PurpleStatusPrimitive type;
	char *message, *unformatted;
	PurpleSavedStatus *saved_status = NULL;
	GtkTreeModel *model;
	GtkTreeIter iter;

	title = gtk_entry_get_text(dialog->title);

	/*
	 * If we're saving this status, and the title is already taken
	 * then show an error dialog and don't do anything.
	 */
	if ((save) && (purple_savedstatus_find(title) != NULL) &&
		((dialog->original_title == NULL) || (!purple_strequal(title, dialog->original_title))))
	{
		purple_notify_error(status_window, NULL, _("Title already in use.  You must "
						  "choose a unique title."), NULL, NULL);
		return;
	}

	type = gtk_combo_box_get_active(dialog->type) + (PURPLE_STATUS_UNSET + 1);
	message = talkatu_markup_get_html(dialog->message_buffer, NULL);
	unformatted = purple_markup_strip_html(message);

	/*
	 * If we're editing an old status, then lookup the old status.
	 * Note: It is possible that it has been deleted or renamed
	 *       or something, and no longer exists.
	 */
	if (dialog->original_title != NULL)
	{
		GtkTreeIter iter;

		saved_status = purple_savedstatus_find(dialog->original_title);

		if (status_window_find_savedstatus(&iter, dialog->original_title))
			gtk_list_store_remove(status_window->model, &iter);
	}

	if(saved_status == NULL) {
		/* This is a new status */
		if(save) {
			saved_status = purple_savedstatus_new(title, type);
		} else {
			saved_status = purple_savedstatus_new(NULL, type);
		}
	} else {
		/* Modify the old status */
		if(!purple_strequal(title, dialog->original_title)) {
			purple_savedstatus_set_title(saved_status, title);
		}
		purple_savedstatus_set_primitive_type(saved_status, type);
	}

	if (*unformatted == '\0')
		purple_savedstatus_set_message(saved_status, NULL);
	else
		purple_savedstatus_set_message(saved_status, message);

	/* Set any substatuses */
	model = GTK_TREE_MODEL(dialog->model);
	if (gtk_tree_model_get_iter_first(model, &iter))
	{
		do {
			PurpleAccount *account;
			gboolean enabled;
			char *id;
			char *message;
			PurpleStatusType *type;

			gtk_tree_model_get(model, &iter,
							   STATUS_EDITOR_COLUMN_ACCOUNT, &account,
							   STATUS_EDITOR_COLUMN_ENABLE_SUBSTATUS, &enabled,
							   STATUS_EDITOR_COLUMN_STATUS_ID, &id,
							   STATUS_EDITOR_COLUMN_STATUS_MESSAGE, &message,
							   -1);
			if (enabled)
			{
				type = purple_account_get_status_type(account, id);
				purple_savedstatus_set_substatus(saved_status, account, type, message);
			}
			else
			{
				purple_savedstatus_unset_substatus(saved_status, account);
			}
			g_free(id);
			g_free(message);
		} while (gtk_tree_model_iter_next(model, &iter));
	}

	g_free(message);
	g_free(unformatted);

	/* If they clicked on "Save and Use" or "Use," then activate the status */
	if(use) {
		purple_savedstatus_activate(saved_status);
	}

	gtk_widget_destroy(dialog->window);
}


static void
status_editor_response_cb(GtkDialog *dialog, gint response, gpointer data) {
	StatusEditor *window = data;

	switch(response) {
		case GTK_RESPONSE_CLOSE:
			gtk_widget_destroy(window->window);
			break;
		case STATUS_RESPONSE_USE:
			status_editor_ok(window, TRUE, FALSE);
			break;
		case STATUS_RESPONSE_SAVE_AND_USE:
			status_editor_ok(window, TRUE, TRUE);
			break;
		case STATUS_RESPONSE_SAVE:
			status_editor_ok(window, FALSE, TRUE);
			break;
		default:
			break;
	}
}

static void
editor_title_changed_cb(GtkWidget *widget, gpointer user_data)
{
	StatusEditor *dialog = user_data;
	const gchar *text;

	text = gtk_entry_get_text(dialog->title);

	gtk_widget_set_sensitive(GTK_WIDGET(dialog->saveanduse_button), (*text != '\0'));
	gtk_widget_set_sensitive(GTK_WIDGET(dialog->save_button), (*text != '\0'));
}

static void edit_substatus(StatusEditor *status_editor, PurpleAccount *account);

static void
edit_substatus_cb(GtkTreeView *tv, GtkTreePath *path, GtkTreeViewColumn *col, gpointer user_data)
{
	StatusEditor *dialog = user_data;
	GtkTreeIter iter;
	PurpleAccount *account;

	gtk_tree_model_get_iter(GTK_TREE_MODEL(dialog->model), &iter, path);
	gtk_tree_model_get(GTK_TREE_MODEL(dialog->model), &iter,
					   STATUS_EDITOR_COLUMN_ACCOUNT, &account,
					   -1);

	edit_substatus(dialog, account);
}

static void
status_editor_substatus_cb(GtkCellRendererToggle *renderer, gchar *path_str, gpointer data)
{
	StatusEditor *dialog = (StatusEditor *)data;
	GtkTreeIter iter;
	gboolean enabled;
	PurpleAccount *account;

	gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(dialog->model), &iter, path_str);
	gtk_tree_model_get(GTK_TREE_MODEL(dialog->model), &iter,
					STATUS_EDITOR_COLUMN_ACCOUNT, &account,
					STATUS_EDITOR_COLUMN_ENABLE_SUBSTATUS, &enabled,
					-1);

	enabled = !enabled;

	if (enabled)
	{
		edit_substatus(dialog, account);
	}
	else
	{
		/* Remove the substatus */
		gtk_list_store_set(dialog->model, &iter,
						   STATUS_EDITOR_COLUMN_ENABLE_SUBSTATUS, enabled,
						   STATUS_EDITOR_COLUMN_STATUS_ID, NULL,
						   STATUS_EDITOR_COLUMN_STATUS_NAME, NULL,
						   STATUS_EDITOR_COLUMN_STATUS_MESSAGE, NULL,
						   STATUS_EDITOR_COLUMN_STATUS_ICON, NULL,
						   -1);
	}
}

static void
status_editor_add_columns(StatusEditor *dialog)
{
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	/* Enable Different status column */
	renderer = gtk_cell_renderer_toggle_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(dialog->treeview),
						    -1, _("Different"),
						    renderer,
						    "active", STATUS_EDITOR_COLUMN_ENABLE_SUBSTATUS,
						    NULL);
	g_signal_connect(G_OBJECT(renderer), "toggled",
			 G_CALLBACK(status_editor_substatus_cb), dialog);

	/* Username column */
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_title(column, _("Username"));
	gtk_tree_view_insert_column(GTK_TREE_VIEW(dialog->treeview), column, -1);
	gtk_tree_view_column_set_resizable(column, TRUE);

	/* Icon */
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_add_attribute(column, renderer, "pixbuf",
									   STATUS_EDITOR_COLUMN_ICON);

	/* Username */
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text",
									   STATUS_EDITOR_COLUMN_USERNAME);

	/* Status column */
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_title(column, _("Status"));
	gtk_tree_view_insert_column(GTK_TREE_VIEW(dialog->treeview), column, -1);
	gtk_tree_view_column_set_resizable(column, TRUE);
	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, renderer, FALSE);
	gtk_tree_view_column_add_attribute(column, renderer, "stock-id",
			STATUS_EDITOR_COLUMN_STATUS_ICON);
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text",
									   STATUS_EDITOR_COLUMN_STATUS_NAME);

	/* Message column */
	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_column_set_title(column, _("Message"));
	gtk_tree_view_insert_column(GTK_TREE_VIEW(dialog->treeview), column, -1);
	gtk_tree_view_column_set_resizable(column, TRUE);
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, renderer, TRUE);
	gtk_tree_view_column_add_attribute(column, renderer, "text",
									   STATUS_EDITOR_COLUMN_STATUS_MESSAGE);

	g_signal_connect(G_OBJECT(dialog->treeview), "row-activated",
					G_CALLBACK(edit_substatus_cb), dialog);
}

static void
status_editor_set_account(GtkListStore *store, PurpleAccount *account,
						  GtkTreeIter *iter, PurpleSavedStatusSub *substatus)
{
	GdkPixbuf *pixbuf;
	const char *id = NULL, *name = NULL, *message = NULL;
	PurpleStatusPrimitive prim = PURPLE_STATUS_UNSET;

	pixbuf = pidgin_create_protocol_icon(account, PIDGIN_PROTOCOL_ICON_MEDIUM);
	if ((pixbuf != NULL) && !purple_account_is_connected(account))
	{
		gdk_pixbuf_saturate_and_pixelate(pixbuf, pixbuf, 0.0, FALSE);
	}

	if (substatus != NULL)
	{
		const PurpleStatusType *type;

		type = purple_savedstatus_substatus_get_status_type(substatus);
		id = purple_status_type_get_id(type);
		name = purple_status_type_get_name(type);
		prim = purple_status_type_get_primitive(type);
		if (purple_status_type_get_attr(type, "message"))
			message = purple_savedstatus_substatus_get_message(substatus);
	}

	gtk_list_store_set(store, iter,
			STATUS_EDITOR_COLUMN_ACCOUNT, account,
			STATUS_EDITOR_COLUMN_ENABLE_SUBSTATUS, (substatus != NULL),
			STATUS_EDITOR_COLUMN_ICON, pixbuf,
			STATUS_EDITOR_COLUMN_USERNAME, purple_account_get_username(account),
			STATUS_EDITOR_COLUMN_STATUS_ID, id,
			STATUS_EDITOR_COLUMN_STATUS_NAME, name,
			STATUS_EDITOR_COLUMN_STATUS_MESSAGE, message,
			STATUS_EDITOR_COLUMN_STATUS_ICON, pidgin_icon_name_from_status_primitive(prim, NULL),
			-1);

	if (pixbuf != NULL)
		g_object_unref(G_OBJECT(pixbuf));
}

static void
status_editor_add_account(StatusEditor *dialog, PurpleAccount *account,
						  PurpleSavedStatusSub *substatus)
{
	GtkTreeIter iter;

	gtk_list_store_append(dialog->model, &iter);

	status_editor_set_account(dialog->model, account, &iter, substatus);
}

static void
status_editor_populate_list(StatusEditor *dialog, PurpleSavedStatus *saved_status)
{
	GList *iter;
	PurpleAccountManager *manager = NULL;
	PurpleSavedStatusSub *substatus;

	gtk_list_store_clear(dialog->model);

	manager = purple_account_manager_get_default();
	iter = purple_account_manager_get_all(manager);

	for(; iter != NULL; iter = iter->next) {
		PurpleAccount *account = (PurpleAccount *)iter->data;

		if (saved_status != NULL) {
			substatus = purple_savedstatus_get_substatus(saved_status, account);
		} else {
			substatus = NULL;
		}

		status_editor_add_account(dialog, account, substatus);
	}
}

void
pidgin_status_editor_show(gboolean edit, PurpleSavedStatus *saved_status)
{
	GtkTreeIter iter;
	StatusEditor *dialog;
	GtkSizeGroup *sg;
	GtkWidget *dbox;
	GtkWidget *expander;
	GtkWidget *dropdown;
	GtkWidget *entry;
	GtkWidget *editor;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *win;
	GtkListStore *store;
	PurpleStatusPrimitive primitive = PURPLE_STATUS_AWAY;

	if (edit)
	{
		g_return_if_fail(saved_status != NULL);
		g_return_if_fail(!purple_savedstatus_is_transient(saved_status));
	}

	/* Find a possible window for this saved status and present it */
	if (edit && status_window_find_savedstatus(&iter, purple_savedstatus_get_title(saved_status)))
	{
		gtk_tree_model_get(GTK_TREE_MODEL(status_window->model), &iter,
							STATUS_WINDOW_COLUMN_WINDOW, &dialog,
							-1);
		if (dialog != NULL)
		{
			gtk_window_present(GTK_WINDOW(dialog->window));
			return;
		}
	}

	dialog = g_new0(StatusEditor, 1);
	if (edit && status_window_find_savedstatus(&iter, purple_savedstatus_get_title(saved_status)))
	{
		gtk_list_store_set(status_window->model, &iter,
							STATUS_WINDOW_COLUMN_WINDOW, dialog,
							-1);
	}

	if (edit)
		dialog->original_title = g_strdup(purple_savedstatus_get_title(saved_status));

	dialog->window = win = pidgin_dialog_new(_("Status"), 12, "status", TRUE);

	g_signal_connect(win, "destroy", G_CALLBACK(status_editor_destroy_cb),
	                 dialog);
	g_signal_connect(win, "response", G_CALLBACK(status_editor_response_cb),
	                 dialog);

	/* Setup the vbox */
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(win));
	gtk_box_set_spacing(GTK_BOX(vbox), 12);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	/* Title */
	entry = gtk_entry_new();
	dialog->title = GTK_ENTRY(entry);
	if ((saved_status != NULL)
			&& !purple_savedstatus_is_transient(saved_status)
			&& (purple_savedstatus_get_title(saved_status) != NULL))
		gtk_entry_set_text(GTK_ENTRY(entry), purple_savedstatus_get_title(saved_status));
	g_signal_connect(G_OBJECT(entry), "changed",
					 G_CALLBACK(editor_title_changed_cb), dialog);
	pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("_Title:"), sg, entry, TRUE, NULL);

	/* Status type */
	if (saved_status != NULL) {
		primitive = purple_savedstatus_get_primitive_type(saved_status);
	}

	store = pidgin_status_primitive_store_new();
	dropdown = pidgin_status_primitive_chooser_new();
	dialog->type = GTK_COMBO_BOX(dropdown);
	gtk_combo_box_set_model(GTK_COMBO_BOX(dropdown), GTK_TREE_MODEL(store));
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(dropdown),
	                            purple_primitive_get_id_from_type(primitive));
	pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("_Status:"), sg, dropdown, TRUE, NULL);

	/* Status message */
	editor = talkatu_editor_new();

	dialog->message_view = talkatu_editor_get_input(TALKATU_EDITOR(editor));
	hbox = pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("_Message:"), sg, editor, TRUE, NULL);

	dialog->message_buffer = talkatu_html_buffer_new();
	gtk_text_view_set_buffer(GTK_TEXT_VIEW(dialog->message_view), dialog->message_buffer);

	gtk_container_child_set(GTK_CONTAINER(vbox), hbox, "expand", TRUE, "fill", TRUE, NULL);

	if ((saved_status != NULL) && (purple_savedstatus_get_message(saved_status) != NULL)) {
		talkatu_markup_set_html(
			TALKATU_BUFFER(dialog->message_buffer),
		    purple_savedstatus_get_message(saved_status),
		    -1
		);
	}

	/* Different status message expander */
	expander = gtk_expander_new_with_mnemonic(_("Use a _different status for some accounts"));
	gtk_box_pack_start(GTK_BOX(vbox), expander, FALSE, FALSE, 0);

	/* Setup the box that the expander will cover */
	dbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 18);
	gtk_container_add(GTK_CONTAINER(expander), dbox);

	/* Create the list model */
	dialog->model = gtk_list_store_new(STATUS_EDITOR_NUM_COLUMNS,
									   G_TYPE_POINTER,
									   G_TYPE_POINTER,
									   G_TYPE_BOOLEAN,
									   GDK_TYPE_PIXBUF,
									   G_TYPE_STRING,
									   G_TYPE_STRING,
									   G_TYPE_STRING,
									   G_TYPE_STRING,
									   G_TYPE_STRING);

	/* Create the treeview */
	dialog->treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(dialog->model));
	gtk_widget_set_size_request(dialog->treeview, -1, 150);
	gtk_box_pack_start(GTK_BOX(dbox),
		pidgin_make_scrollable(dialog->treeview, GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS, GTK_SHADOW_IN, -1, -1),
		TRUE, TRUE, 0);

	/* Add columns */
	status_editor_add_columns(dialog);

	/* Populate list */
	status_editor_populate_list(dialog, saved_status);

	/* Expand the treeview if we have substatuses */
	gtk_expander_set_expanded(GTK_EXPANDER(expander),
		(saved_status != NULL) && purple_savedstatus_has_substatuses(saved_status));

	/* Buttons */
	gtk_dialog_add_button(GTK_DIALOG(win), _("Cancel"), GTK_RESPONSE_CLOSE);
	gtk_dialog_add_button(GTK_DIALOG(win), _("_Use"), STATUS_RESPONSE_USE);

	dialog->saveanduse_button = gtk_dialog_add_button(GTK_DIALOG(win),
	                                                  _("Sa_ve and Use"),
	                                                  STATUS_RESPONSE_SAVE_AND_USE);
	if(dialog->original_title == NULL) {
		gtk_widget_set_sensitive(dialog->saveanduse_button, FALSE);
	}

	dialog->save_button = gtk_dialog_add_button(GTK_DIALOG(win), _("Save"),
	                                            STATUS_RESPONSE_SAVE);
	if(dialog->original_title == NULL) {
		gtk_widget_set_sensitive(dialog->save_button, FALSE);
	}

	gtk_widget_show_all(win);
	g_object_unref(sg);
}


/**************************************************************************
* SubStatus editor
**************************************************************************/

static void
substatus_selection_changed_cb(GtkComboBox *box, gpointer user_data)
{
	SubStatusEditor *select = user_data;
	PurpleStatusType *type;
	PurpleStatusPrimitive primitive;
	const gchar *id;

	id = gtk_combo_box_get_active_id(box);
	if(id == NULL) {
		return;
	}

	primitive = purple_primitive_get_type_from_id(id);
	if(primitive == PURPLE_STATUS_UNSET) {
		return;
	}

	type = purple_account_get_status_type_with_primitive(select->account,
	                                                     primitive);

	if(purple_status_type_get_attr(type, "message") == NULL) {
		gtk_widget_set_sensitive(select->message_view, FALSE);
	} else {
		gtk_widget_set_sensitive(select->message_view, TRUE);
	}
}

static gboolean
status_editor_find_account_in_treemodel(GtkTreeIter *iter,
										StatusEditor *status_editor,
										PurpleAccount *account)
{
	GtkTreeModel *model;
	PurpleAccount *cur;

	g_return_val_if_fail(status_editor != NULL, FALSE);
	g_return_val_if_fail(account       != NULL, FALSE);

	model = GTK_TREE_MODEL(status_editor->model);

	if (!gtk_tree_model_get_iter_first(model, iter))
		return FALSE;

	do {
		gtk_tree_model_get(model, iter, STATUS_EDITOR_COLUMN_ACCOUNT, &cur, -1);
		if (cur == account)
			return TRUE;
	} while (gtk_tree_model_iter_next(model, iter));

	return FALSE;
}

static void
substatus_editor_remove_dialog(SubStatusEditor *dialog)
{
	GtkTreeIter iter;

	if (status_editor_find_account_in_treemodel(&iter, dialog->status_editor, dialog->account))
	{
		/* Remove the reference to this dialog from our parent's list store */
		gtk_list_store_set(dialog->status_editor->model, &iter,
						   STATUS_EDITOR_COLUMN_WINDOW, NULL,
						   -1);
	}
}

static void
substatus_editor_destroy_cb(GtkWidget *widget, gpointer user_data)
{
	SubStatusEditor *dialog = user_data;

	substatus_editor_remove_dialog(dialog);
	g_free(dialog);
}

static void
substatus_editor_cancel(SubStatusEditor *dialog)
{
	gtk_widget_destroy(dialog->window);
}


static void
substatus_editor_ok(SubStatusEditor *dialog)
{
	StatusEditor *status_editor;
	GtkTreeIter iter;
	PurpleStatusType *type;
	PurpleStatusPrimitive primitive;
	const char *id = NULL;
	char *message = NULL;
	const gchar *name = NULL, *icon_name = NULL;

	id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(dialog->box));
	if(id == NULL) {
		gtk_widget_destroy(dialog->window);
		return;
	}

	primitive = purple_primitive_get_type_from_id(id);
	if(primitive == PURPLE_STATUS_UNSET) {
		gtk_widget_destroy(dialog->window);
		return;
	}

	type = purple_account_get_status_type_with_primitive(dialog->account,
	                                                     primitive);
	if (purple_status_type_get_attr(type, "message") != NULL) {
		message = talkatu_markup_get_html(dialog->message_buffer, NULL);
	}
	name = purple_status_type_get_name(type);
	icon_name = pidgin_icon_name_from_status_primitive(primitive, NULL);

	status_editor = dialog->status_editor;

	if (status_editor_find_account_in_treemodel(&iter, status_editor, dialog->account))
	{
		gtk_list_store_set(status_editor->model, &iter,
		                   STATUS_EDITOR_COLUMN_ENABLE_SUBSTATUS, TRUE,
		                   STATUS_EDITOR_COLUMN_STATUS_ID, id,
		                   STATUS_EDITOR_COLUMN_STATUS_NAME, name,
		                   STATUS_EDITOR_COLUMN_STATUS_MESSAGE, message,
		                   STATUS_EDITOR_COLUMN_WINDOW, NULL,
		                   STATUS_EDITOR_COLUMN_STATUS_ICON, icon_name,
		                   -1);
	}

	gtk_widget_destroy(dialog->window);
	g_free(message);
}

static void
substatus_editor_response_cb(GtkDialog *dialog, gint response_id,
                             gpointer data)
{
	SubStatusEditor *editor = data;

	switch(response_id) {
		case GTK_RESPONSE_CANCEL:
			substatus_editor_cancel(editor);
			break;
		case GTK_RESPONSE_OK:
			substatus_editor_ok(editor);
			break;
		default:
			break;
	}
}

static void
edit_substatus(StatusEditor *status_editor, PurpleAccount *account)
{
	char *tmp;
	SubStatusEditor *dialog;
	GtkSizeGroup *sg;
	GtkWidget *combo;
	GtkWidget *hbox;
	GtkWidget *editor;
	GtkWidget *label;
	GtkWidget *vbox;
	GtkWidget *win;
	GtkTreeIter iter;
	char *status_id = NULL;
	char *message = NULL;
	gboolean parent_dialog_has_substatus = FALSE;
	GtkListStore *store;
	PurpleStatusPrimitive primitive = PURPLE_STATUS_AWAY;

	g_return_if_fail(status_editor != NULL);
	g_return_if_fail(account       != NULL);

	status_editor_find_account_in_treemodel(&iter, status_editor, account);
	gtk_tree_model_get(GTK_TREE_MODEL(status_editor->model), &iter,
						STATUS_EDITOR_COLUMN_WINDOW, &dialog,
						-1);
	if (dialog != NULL)
	{
		gtk_window_present(GTK_WINDOW(dialog->window));
		return;
	}

	dialog = g_new0(SubStatusEditor, 1);
	gtk_list_store_set(status_editor->model, &iter,
						STATUS_EDITOR_COLUMN_WINDOW, dialog,
						-1);
	dialog->status_editor = status_editor;
	dialog->account = account;

	tmp = g_strdup_printf(_("Status for %s"), purple_account_get_username(account));
	dialog->window = win = pidgin_dialog_new(tmp, 12, "substatus", TRUE);
	g_free(tmp);

	g_signal_connect(G_OBJECT(win), "destroy",
					 G_CALLBACK(substatus_editor_destroy_cb), dialog);
	g_signal_connect(win, "response", G_CALLBACK(substatus_editor_response_cb),
	                 dialog);

	/* Setup the vbox */
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(win));
	gtk_box_set_spacing(GTK_BOX(vbox), 12);

	sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	/* Status type */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new_with_mnemonic(_("_Status:"));
	gtk_label_set_xalign(GTK_LABEL(label), 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_size_group_add_widget(sg, label);

	store = pidgin_status_primitive_store_new();
	pidgin_status_primitive_store_set_account(PIDGIN_STATUS_PRIMITIVE_STORE(store),
	                                          account);
	combo = pidgin_status_primitive_chooser_new();
	gtk_combo_box_set_model(GTK_COMBO_BOX(combo), GTK_TREE_MODEL(store));
	dialog->box = GTK_COMBO_BOX(combo);

	g_signal_connect(G_OBJECT(combo), "changed",
					 G_CALLBACK(substatus_selection_changed_cb), dialog);

	gtk_box_pack_start(GTK_BOX(hbox), combo, FALSE, FALSE, 0);

	/* Status message */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

	label = gtk_label_new_with_mnemonic(_("_Message:"));
	gtk_label_set_xalign(GTK_LABEL(label), 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_size_group_add_widget(sg, label);

	editor = talkatu_editor_new();
	gtk_box_pack_start(GTK_BOX(hbox), editor, TRUE, TRUE, 0);

	dialog->message_view = talkatu_editor_get_input(TALKATU_EDITOR(editor));
	dialog->message_buffer = talkatu_html_buffer_new();
	gtk_text_view_set_buffer(GTK_TEXT_VIEW(dialog->message_view), dialog->message_buffer);

	/* Add the buttons */
	gtk_dialog_add_button(GTK_DIALOG(win), _("Cancel"), GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button(GTK_DIALOG(win), _("Okay"), GTK_RESPONSE_OK);

	/* Seed the input widgets with the current values */

	/* Only look at the saved status if we can't find it in the parent status dialog's substatuses model */
	gtk_tree_model_get(GTK_TREE_MODEL(status_editor->model), &iter,
		STATUS_EDITOR_COLUMN_ENABLE_SUBSTATUS, &parent_dialog_has_substatus, -1);
	if (parent_dialog_has_substatus) {
		gtk_tree_model_get(GTK_TREE_MODEL(status_editor->model), &iter,
			STATUS_EDITOR_COLUMN_STATUS_ID, &status_id,
			STATUS_EDITOR_COLUMN_STATUS_MESSAGE, &message, -1);
	} else if (status_editor->original_title != NULL) {
		PurpleSavedStatus *saved_status = NULL;
		PurpleSavedStatusSub *substatus = NULL;

		if ((saved_status = purple_savedstatus_find(status_editor->original_title)) != NULL) {
			if ((substatus = purple_savedstatus_get_substatus(saved_status, account)) != NULL) {
				message = (char *)purple_savedstatus_substatus_get_message(substatus);
				status_id = (char *)purple_status_type_get_id(
						purple_savedstatus_substatus_get_status_type(substatus));
			}
		}
	}
	/* TODO: Else get the generic status type from our parent */

	if(status_id != NULL) {
		PurpleStatus *status = purple_account_get_status(account, status_id);
		PurpleStatusType *type = purple_status_get_status_type(status);

		primitive = purple_status_type_get_primitive(type);
	}

	gtk_combo_box_set_active_id(GTK_COMBO_BOX(combo),
	                            purple_primitive_get_id_from_type(primitive));

	if (message) {
		talkatu_markup_set_html(TALKATU_BUFFER(dialog->message_buffer), message, -1);
	}

	if (parent_dialog_has_substatus) {
		/* These two were gotten from the parent tree model, so they need to be freed */
		g_free(status_id);
		g_free(message);
	}

	gtk_widget_show_all(win);
	g_object_unref(sg);
}


/**************************************************************************
 * Utilities                                                              *
 **************************************************************************/

enum {
	SS_MENU_ENTRY_TYPE_PRIMITIVE,
	SS_MENU_ENTRY_TYPE_SAVEDSTATUS
};

enum {
	/* _SSMenuEntryType */
	SS_MENU_TYPE_COLUMN,

	/*
	 * This is a GdkPixbuf (the other columns are strings).
	 * This column is visible.
	 */
	SS_MENU_ICON_COLUMN,

	/* The text displayed on the status box.  This column is visible. */
	SS_MENU_TEXT_COLUMN,

	/*
	 * This value depends on SS_MENU_TYPE_COLUMN.  For _SAVEDSTATUS types,
	 * this is the creation time.  For _PRIMITIVE types,
	 * this is the PurpleStatusPrimitive.
	 */
	SS_MENU_DATA_COLUMN,

	/*
	 * This is the emblem to use for this status
	 */
	SS_MENU_EMBLEM_COLUMN,

	/*
	 * And whether or not that emblem is visible
	 */
	SS_MENU_EMBLEM_VISIBLE_COLUMN,

	SS_MENU_NUM_COLUMNS
};

static void
status_menu_cb(GtkComboBox *widget, void(*callback)(PurpleSavedStatus*))
{
	GtkTreeIter iter;
	int type;
	gpointer data;
	PurpleSavedStatus *status = NULL;

	if (!gtk_combo_box_get_active_iter(widget, &iter))
		return;

	gtk_tree_model_get(gtk_combo_box_get_model(widget), &iter,
			   SS_MENU_TYPE_COLUMN, &type,
			   SS_MENU_DATA_COLUMN, &data,
			   -1);

	if (type == SS_MENU_ENTRY_TYPE_PRIMITIVE)
	{
		PurpleStatusPrimitive primitive = GPOINTER_TO_INT(data);
		status = purple_savedstatus_find_transient_by_type_and_message(primitive, NULL);
		if (status == NULL)
			status = purple_savedstatus_new(NULL, primitive);
	}
	else if (type == SS_MENU_ENTRY_TYPE_SAVEDSTATUS)
		status = purple_savedstatus_find_by_creation_time(GPOINTER_TO_INT(data));

	callback(status);
}

static gint
saved_status_sort_alphabetically_func(gconstpointer a, gconstpointer b)
{
	const PurpleSavedStatus *saved_status_a = a;
	const PurpleSavedStatus *saved_status_b = b;
	return g_utf8_collate(purple_savedstatus_get_title(saved_status_a),
				  purple_savedstatus_get_title(saved_status_b));
}

static gboolean pidgin_status_menu_add_primitive(GtkListStore *model, GtkWidget *w, PurpleStatusPrimitive primitive,
	PurpleSavedStatus *current_status)
{
	GtkTreeIter iter;
	gboolean currently_selected = FALSE;

	gtk_list_store_append(model, &iter);
	gtk_list_store_set(model, &iter,
			   SS_MENU_TYPE_COLUMN, SS_MENU_ENTRY_TYPE_PRIMITIVE,
			   SS_MENU_ICON_COLUMN, pidgin_icon_name_from_status_primitive(primitive, NULL),
			   SS_MENU_TEXT_COLUMN, purple_primitive_get_name_from_type(primitive),
			   SS_MENU_DATA_COLUMN, GINT_TO_POINTER(primitive),
			   SS_MENU_EMBLEM_VISIBLE_COLUMN, FALSE,
			   -1);

	if (purple_savedstatus_is_transient(current_status)
			&& !purple_savedstatus_has_substatuses(current_status)
			&& purple_savedstatus_get_primitive_type(current_status) == primitive)
		currently_selected = TRUE;

	return currently_selected;
}

static void
pidgin_status_menu_update_iter(GtkWidget *combobox, GtkListStore *store, GtkTreeIter *iter,
		PurpleSavedStatus *status)
{
	PurpleStatusPrimitive primitive;

	if (store == NULL)
		store = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(combobox)));

	primitive = purple_savedstatus_get_primitive_type(status);
	gtk_list_store_set(store, iter,
			SS_MENU_TYPE_COLUMN, SS_MENU_ENTRY_TYPE_SAVEDSTATUS,
			SS_MENU_ICON_COLUMN, pidgin_icon_name_from_status_primitive(primitive, NULL),
			SS_MENU_TEXT_COLUMN, purple_savedstatus_get_title(status),
			SS_MENU_DATA_COLUMN, GINT_TO_POINTER(purple_savedstatus_get_creation_time(status)),
			SS_MENU_EMBLEM_COLUMN, "document-save",
			SS_MENU_EMBLEM_VISIBLE_COLUMN, TRUE,
			-1);
}

static gboolean
pidgin_status_menu_find_iter(GtkListStore *store, GtkTreeIter *iter, PurpleSavedStatus *find)
{
	int type;
	gpointer data;
	time_t creation_time = purple_savedstatus_get_creation_time(find);
	GtkTreeModel *model = GTK_TREE_MODEL(store);

	if (!gtk_tree_model_get_iter_first(model, iter))
		return FALSE;

	do {
		gtk_tree_model_get(model, iter,
				SS_MENU_TYPE_COLUMN, &type,
				SS_MENU_DATA_COLUMN, &data,
				-1);
		if (type == SS_MENU_ENTRY_TYPE_PRIMITIVE)
			continue;
		if (GPOINTER_TO_INT(data) == creation_time)
			return TRUE;
	} while (gtk_tree_model_iter_next(model, iter));

	return FALSE;
}

static void
savedstatus_added_cb(PurpleSavedStatus *status, GtkWidget *combobox)
{
	GtkListStore *store;
	GtkTreeIter iter;

	if (purple_savedstatus_is_transient(status))
		return;

	store = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(combobox)));
	gtk_list_store_append(store, &iter);
	pidgin_status_menu_update_iter(combobox, store, &iter, status);
}

static void
savedstatus_deleted_cb(PurpleSavedStatus *status, GtkWidget *combobox)
{
	GtkListStore *store;
	GtkTreeIter iter;

	if (purple_savedstatus_is_transient(status))
		return;

	store = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(combobox)));
	if (pidgin_status_menu_find_iter(store, &iter, status))
		gtk_list_store_remove(store, &iter);
}

static void
savedstatus_modified_cb(PurpleSavedStatus *status, GtkWidget *combobox)
{
	GtkListStore *store;
	GtkTreeIter iter;

	if (purple_savedstatus_is_transient(status))
		return;

	store = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(combobox)));
	if (pidgin_status_menu_find_iter(store, &iter, status))
		pidgin_status_menu_update_iter(combobox, store, &iter, status);
}

GtkWidget *pidgin_status_menu(PurpleSavedStatus *current_status, GCallback callback)
{
	GtkWidget *combobox;
	GtkListStore *model;
	GList *sorted, *cur;
	int i = 0;
	int index = -1;
	GtkTreeIter iter;
	GtkCellRenderer *text_rend;
	GtkCellRenderer *icon_rend;
	GtkCellRenderer *emblem_rend;

	model = gtk_list_store_new(SS_MENU_NUM_COLUMNS, G_TYPE_INT, G_TYPE_STRING,
				   G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_STRING, G_TYPE_BOOLEAN);

	combobox = gtk_combo_box_new();

	if (pidgin_status_menu_add_primitive(model, combobox, PURPLE_STATUS_AVAILABLE, current_status))
		index = i;
	i++;

	if (pidgin_status_menu_add_primitive(model, combobox, PURPLE_STATUS_AWAY, current_status))
		index = i;
	i++;

	if (pidgin_status_menu_add_primitive(model, combobox, PURPLE_STATUS_INVISIBLE, current_status))
		index = i;
	i++;

	if (pidgin_status_menu_add_primitive(model, combobox, PURPLE_STATUS_OFFLINE, current_status))
		index = i;
	i++;

	sorted = g_list_copy((GList *)purple_savedstatuses_get_all());
	sorted = g_list_sort(sorted, saved_status_sort_alphabetically_func);
	for (cur = sorted; cur; cur = cur->next)
	{
		PurpleSavedStatus *status = (PurpleSavedStatus *) cur->data;
		if (!purple_savedstatus_is_transient(status))
		{
			gtk_list_store_append(model, &iter);

			pidgin_status_menu_update_iter(combobox, model, &iter, status);

			if (status == current_status)
				index = i;
			i++;
		}
	}
	g_list_free(sorted);

	gtk_combo_box_set_model(GTK_COMBO_BOX(combobox), GTK_TREE_MODEL(model));

	text_rend = gtk_cell_renderer_text_new();
	icon_rend = gtk_cell_renderer_pixbuf_new();
	emblem_rend = gtk_cell_renderer_pixbuf_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combobox), icon_rend, FALSE);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combobox), text_rend, TRUE);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combobox), emblem_rend, FALSE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combobox), icon_rend, "icon-name", SS_MENU_ICON_COLUMN, NULL);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combobox), text_rend, "markup", SS_MENU_TEXT_COLUMN, NULL);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combobox), emblem_rend,
					"icon-name", SS_MENU_EMBLEM_COLUMN, "visible", SS_MENU_EMBLEM_VISIBLE_COLUMN, NULL);
	g_object_set(text_rend, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), index);
	g_signal_connect(G_OBJECT(combobox), "changed", G_CALLBACK(status_menu_cb), callback);

	/* Make sure the list is updated dynamically when a substatus is changed/deleted
	 * or a new one is added. */
	purple_signal_connect(purple_savedstatuses_get_handle(), "savedstatus-added",
			combobox, G_CALLBACK(savedstatus_added_cb), combobox);
	purple_signal_connect(purple_savedstatuses_get_handle(), "savedstatus-deleted",
			combobox, G_CALLBACK(savedstatus_deleted_cb), combobox);
	purple_signal_connect(purple_savedstatuses_get_handle(), "savedstatus-modified",
			combobox, G_CALLBACK(savedstatus_modified_cb), combobox);
	g_signal_connect(G_OBJECT(combobox), "destroy",
			G_CALLBACK(purple_signals_disconnect_by_handle), NULL);

	return combobox;
}


/**************************************************************************
* GTK saved status glue
**************************************************************************/

void *
pidgin_status_get_handle(void)
{
	static int handle;

	return &handle;
}

void
pidgin_status_init(void)
{
}

void
pidgin_status_uninit(void)
{
	pidgin_status_window_hide();
}
