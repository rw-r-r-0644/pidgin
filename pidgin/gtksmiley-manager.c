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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#include "internal.h"
#include "pidgin.h"

#include "debug.h"
#include "http.h"
#include "notify.h"
#include "smiley.h"
#include "smiley-custom.h"
#include "smiley-list.h"

#include "gtksmiley-manager.h"
#include "gtkutils.h"
#include "gtkwebview.h"
#include "pidginstock.h"

#include "gtk3compat.h"

#define PIDGIN_RESPONSE_MODIFY 1000

#if 0
typedef struct _PidginSmiley PidginSmiley;
struct _PidginSmiley
{
	PurpleSmiley *smiley;
	GtkWidget *parent;
	GtkWidget *smile;
	GtkWidget *smiley_image;
	gchar *filename;
	GdkPixbuf *custom_pixbuf;
	gpointer data;
	gsize datasize;
	gint entry_len;
};
#endif

typedef struct
{
	PurpleSmiley *smiley;

	gchar *filename;

	GtkDialog *window;
	GtkImage *thumbnail;
	GtkEntry *shortcut;
} SmileyEditDialog;

typedef struct
{
	GtkDialog *window;
	GtkListStore *model;
	GtkTreeView *tree;

	PurpleHttpConnection *running_request;
} SmileyManager;

enum
{
	SMILEY_LIST_MODEL_ICON,
	SMILEY_LIST_MODEL_SHORTCUT,
	SMILEY_LIST_MODEL_PURPLESMILEY,
	SMILEY_LIST_MODEL_N_COL
};

static SmileyManager *smiley_manager = NULL;

/******************************************************************************
 * New routines (TODO)
 *****************************************************************************/

static void
edit_dialog_destroy(GtkWidget *window, gpointer _edit_dialog)
{
	SmileyEditDialog *edit_dialog = _edit_dialog;

	g_object_set_data(G_OBJECT(edit_dialog->smiley),
		"pidgin-smiley-manager-edit-dialog", NULL);
	g_object_unref(edit_dialog->smiley);

	g_free(edit_dialog->filename);
	g_free(edit_dialog);
}

static void
edit_dialog_update_thumb(SmileyEditDialog *edit_dialog)
{
	GdkPixbuf *pixbuf = NULL;

	if (edit_dialog->filename) {
		pixbuf = pidgin_pixbuf_new_from_file_at_scale(
			edit_dialog->filename, 64, 64, TRUE);
		if (!pixbuf) {
			g_free(edit_dialog->filename);
			edit_dialog->filename = NULL;
		}
	}
	if (!pixbuf) {
		GtkIconSize icon_size =
			gtk_icon_size_from_name(PIDGIN_ICON_SIZE_TANGO_SMALL);
		pixbuf = gtk_widget_render_icon(GTK_WIDGET(edit_dialog->window),
			PIDGIN_STOCK_TOOLBAR_SELECT_AVATAR, icon_size,
			"PidginSmiley");
	}
	g_return_if_fail(pixbuf != NULL);

	gtk_image_set_from_pixbuf(GTK_IMAGE(edit_dialog->thumbnail), pixbuf);

	g_object_unref(G_OBJECT(pixbuf));
}

static void
edit_dialog_update_buttons(SmileyEditDialog *edit_dialog)
{
	gboolean shortcut_ok, image_ok;

	shortcut_ok = (gtk_entry_get_text_length(edit_dialog->shortcut) > 0);
	image_ok = (edit_dialog->filename != NULL);

	gtk_dialog_set_response_sensitive(edit_dialog->window,
		GTK_RESPONSE_ACCEPT, shortcut_ok && image_ok);
}


/******************************************************************************
 * Manager stuff
 *****************************************************************************/

#if 0
static void refresh_list(void);
#endif

/******************************************************************************
 * The Add dialog
 ******************************************************************************/

#if 0
static void do_add(GtkWidget *widget, PidginSmiley *s)
{
	const gchar *entry;
	PurpleSmiley *emoticon;

	entry = gtk_entry_get_text(GTK_ENTRY(s->smile));

	emoticon = purple_smileys_find_by_shortcut(entry);
	if (emoticon && emoticon != s->smiley) {
		gchar *msg;
		msg = g_strdup_printf(_("A custom smiley for '%s' already exists.  "
				"Please use a different shortcut."), entry);
		purple_notify_error(s, _("Custom Smiley"),
				_("Duplicate Shortcut"), msg, NULL);
		g_free(msg);
		return;
	}

	if (s->smiley) {
		if (s->filename) {
			gchar *data = NULL;
			size_t len;
			GError *err = NULL;

			if (!g_file_get_contents(s->filename, &data, &len, &err)) {
				purple_debug_error("gtksmiley", "Error reading %s: %s\n",
						s->filename, err->message);
				g_error_free(err);

				return;
			}
			purple_smiley_set_data(s->smiley, (guchar*)data, len);
		}
		purple_smiley_set_shortcut(s->smiley, entry);
	} else {
		purple_debug_info("gtksmiley", "adding a new smiley\n");

		if (s->filename == NULL) {
			gchar *buffer = NULL;
			gsize size = 0;
			gchar *filename;
			const gchar *dirname = purple_smileys_get_storing_dir();

			/* since this may be called before purple_smiley_new_* has ever been
			 called, we create the storing dir, if it doesn't exist yet, to be
			 able to save the pixbuf before adding the smiley */
			if (!g_file_test(dirname, G_FILE_TEST_IS_DIR)) {
				purple_debug_info("gtksmiley", "Creating smileys directory.\n");

				if (g_mkdir(dirname, S_IRUSR | S_IWUSR | S_IXUSR) < 0) {
					purple_debug_error("gtksmiley",
			                   "Unable to create directory %s: %s\n",
			                   dirname, g_strerror(errno));
				}
			}

			if (s->data && s->datasize) {
				/* Cached data & size in memory */
				buffer = s->data;
				size = s->datasize;
			}
			else {
				/* Get the smiley from the custom pixbuf */
				gdk_pixbuf_save_to_buffer(s->custom_pixbuf, &buffer, &size,
					"png", NULL, "compression", "9", NULL, NULL);
			}
			filename = purple_util_get_image_filename(buffer, size);
			s->filename = g_build_filename(dirname, filename, NULL);
			purple_util_write_data_to_file_absolute(s->filename, buffer, size);
			g_free(filename);
			g_free(buffer);
		}
		emoticon = purple_smiley_new_from_file(entry, s->filename);
		if (emoticon)
			pidgin_smiley_add_to_list(emoticon);
	}

	if (smiley_manager != NULL)
		refresh_list();

	gtk_widget_destroy(s->parent);
}
#endif

static void
pidgin_smiley_edit_response(GtkDialog *window, gint response_id,
	gpointer _edit_dialog)
{
	SmileyEditDialog *edit_dialog = _edit_dialog;

	switch (response_id) {
#if 0
		case GTK_RESPONSE_ACCEPT:
			do_add(widget, s);
			break;
#endif
		case GTK_RESPONSE_DELETE_EVENT:
		case GTK_RESPONSE_CANCEL:
			gtk_widget_destroy(GTK_WIDGET(edit_dialog->window));
			break;
		default:
			g_warn_if_reached();
	}
}

static void
image_choosen(const char *filename, gpointer _edit_dialog)
{
	SmileyEditDialog *edit_dialog = _edit_dialog;

	if (!filename)
		return;

	g_free(edit_dialog->filename);
	edit_dialog->filename = g_strdup(filename);

	edit_dialog_update_thumb(edit_dialog);
	edit_dialog_update_buttons(edit_dialog);

	gtk_widget_grab_focus(GTK_WIDGET(edit_dialog->shortcut));
}

static void
open_image_selector(GtkWidget *widget, gpointer _edit_dialog)
{
	GtkWidget *file_chooser;
	file_chooser = pidgin_buddy_icon_chooser_new(
		GTK_WINDOW(gtk_widget_get_toplevel(widget)),
		image_choosen, _edit_dialog);
	gtk_window_set_title(GTK_WINDOW(file_chooser), _("Custom Smiley"));
	gtk_window_set_role(GTK_WINDOW(file_chooser),
		"file-selector-custom-smiley");
	gtk_widget_show_all(file_chooser);
}

static void
smiley_shortcut_changed(GtkEditable *shortcut, gpointer _edit_dialog)
{
	SmileyEditDialog *edit_dialog = _edit_dialog;

	edit_dialog_update_buttons(edit_dialog);
}

/* TODO: maybe replace "GtkWindow *parent" with "SmileyManager *manager"? */
static void
pidgin_smiley_edit(GtkWindow *parent, PurpleSmiley *smiley)
{
	SmileyEditDialog *edit_dialog;
	GtkWidget *vbox, *hbox;
	GtkLabel *label;
	GtkButton *filech;

	edit_dialog = g_object_get_data(G_OBJECT(smiley),
		"pidgin-smiley-manager-edit-dialog");
	if (edit_dialog) {
		gtk_window_present(GTK_WINDOW(edit_dialog->window));
		return;
	}

	edit_dialog = g_new0(SmileyEditDialog, 1);

	edit_dialog->window = GTK_DIALOG(gtk_dialog_new_with_buttons(
		smiley ? _("Edit Smiley") : _("Add Smiley"),
		parent, GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		smiley ? GTK_STOCK_SAVE : GTK_STOCK_ADD, GTK_RESPONSE_ACCEPT,
		NULL));
	gtk_dialog_set_default_response(
		edit_dialog->window, GTK_RESPONSE_ACCEPT);

	edit_dialog->smiley = smiley;
	g_object_set_data(G_OBJECT(smiley),
		"pidgin-smiley-manager-edit-dialog", edit_dialog);
	g_object_ref(smiley);

#if !GTK_CHECK_VERSION(3,0,0)
	gtk_container_set_border_width(
		GTK_CONTAINER(edit_dialog->window), PIDGIN_HIG_BORDER);
#endif

	/* The vbox */
#if GTK_CHECK_VERSION(3,0,0)
	vbox = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(vbox), PIDGIN_HIG_BORDER);
#else
	vbox = gtk_vbox_new(FALSE, PIDGIN_HIG_BORDER);
#endif
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(
		edit_dialog->window)), vbox);
	gtk_widget_show(vbox);

	/* The hbox */
#if GTK_CHECK_VERSION(3,0,0)
	hbox = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(hbox), PIDGIN_HIG_BORDER);
	gtk_grid_attach(GTK_GRID(vbox), hbox, 0, 0, 1, 1);
#else
	hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(GTK_VBOX(vbox)), hbox);
#endif

	label = GTK_LABEL(gtk_label_new_with_mnemonic(_("_Image:")));
#if GTK_CHECK_VERSION(3,0,0)
	gtk_grid_attach(GTK_GRID(hbox), GTK_WIDGET(label), 0, 0, 1, 1);
#else
	gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(label), FALSE, FALSE, 0);
#endif
	gtk_widget_show(GTK_WIDGET(label));

	filech = GTK_BUTTON(gtk_button_new());
#if GTK_CHECK_VERSION(3,0,0)
	gtk_grid_attach_next_to(GTK_GRID(hbox), GTK_WIDGET(filech), NULL,
		GTK_POS_RIGHT, 1, 1);
#else
	gtk_box_pack_end(GTK_BOX(hbox), GTK_WIDGET(filech), FALSE, FALSE, 0);
#endif
	pidgin_set_accessible_label(GTK_WIDGET(filech), label);

	edit_dialog->thumbnail = GTK_IMAGE(gtk_image_new());
	gtk_container_add(GTK_CONTAINER(filech),
		GTK_WIDGET(edit_dialog->thumbnail));

	gtk_widget_show_all(hbox);

	/* info */
#if GTK_CHECK_VERSION(3,0,0)
	hbox = gtk_grid_new();
	gtk_grid_set_column_spacing(GTK_GRID(hbox), PIDGIN_HIG_BORDER);

	gtk_grid_attach_next_to(GTK_GRID(vbox), hbox, NULL,
		GTK_POS_BOTTOM, 1, 1);
#else
	hbox = gtk_hbox_new(FALSE, PIDGIN_HIG_BORDER);
	gtk_container_add(GTK_CONTAINER(GTK_VBOX(vbox)),hbox);
#endif

	/* Shortcut text */
	label = GTK_LABEL(gtk_label_new_with_mnemonic(_("S_hortcut text:")));
#if GTK_CHECK_VERSION(3,0,0)
	gtk_grid_attach(GTK_GRID(hbox), GTK_WIDGET(label), 0, 0, 1, 1);
#else
	gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(label), FALSE, FALSE, 0);
#endif
	gtk_widget_show(GTK_WIDGET(label));

	edit_dialog->shortcut = GTK_ENTRY(gtk_entry_new());
	gtk_entry_set_activates_default(edit_dialog->shortcut, TRUE);
	pidgin_set_accessible_label(GTK_WIDGET(edit_dialog->shortcut), label);

#if GTK_CHECK_VERSION(3,0,0)
	gtk_grid_attach_next_to(GTK_GRID(hbox), GTK_WIDGET(edit_dialog->shortcut), NULL, GTK_POS_RIGHT, 1, 1);
#else
	gtk_box_pack_end(GTK_BOX(hbox), GTK_WIDGET(edit_dialog->shortcut), FALSE, FALSE, 0);
#endif

	gtk_widget_show(GTK_WIDGET(edit_dialog->shortcut));
	gtk_widget_show(hbox);
	gtk_widget_show(GTK_WIDGET(edit_dialog->window));

	if (smiley) {
		edit_dialog->filename =
			g_strdup(purple_smiley_get_path(smiley));
		gtk_entry_set_text(edit_dialog->shortcut,
			purple_smiley_get_shortcut(smiley));
	}

	edit_dialog_update_thumb(edit_dialog);
	edit_dialog_update_buttons(edit_dialog);

	g_signal_connect(edit_dialog->window, "response",
		G_CALLBACK(pidgin_smiley_edit_response), edit_dialog);
	g_signal_connect(filech, "clicked",
		G_CALLBACK(open_image_selector), edit_dialog);
	g_signal_connect(edit_dialog->shortcut, "changed",
		G_CALLBACK(smiley_shortcut_changed), edit_dialog);

	g_signal_connect(edit_dialog->window, "destroy",
		G_CALLBACK(edit_dialog_destroy), edit_dialog);
	g_signal_connect(edit_dialog->window, "destroy",
		G_CALLBACK(purple_notify_close_with_handle), edit_dialog);
}

#if 0
static void
pidgin_smiley_editor_set_shortcut(PidginSmiley *editor, const gchar *shortcut)
{
	gtk_entry_set_text(GTK_ENTRY(editor->smile), shortcut ? shortcut : "");
}
#endif

#if 0
static void
pidgin_smiley_editor_set_image(PidginSmiley *editor, GdkPixbuf *image)
{
	if (editor->custom_pixbuf)
		g_object_unref(G_OBJECT(editor->custom_pixbuf));
	editor->custom_pixbuf = image ? g_object_ref(G_OBJECT(image)) : NULL;
	if (image) {
		gtk_image_set_from_pixbuf(GTK_IMAGE(editor->smiley_image), image);
		if (editor->entry_len > 0)
			gtk_dialog_set_response_sensitive(GTK_DIALOG(editor->parent),
			                                  GTK_RESPONSE_ACCEPT, TRUE);
	}
	else
		gtk_dialog_set_response_sensitive(GTK_DIALOG(editor->parent),
		                                  GTK_RESPONSE_ACCEPT, FALSE);

	edit_dialog_update_buttons(...);
}
#endif

#if 0
static void
pidgin_smiley_editor_set_data(PidginSmiley *editor, gpointer data, gsize datasize)
{
	editor->data = data;
	editor->datasize = datasize;
}
#endif

/******************************************************************************
 * Delete smiley
 *****************************************************************************/
#if 0
static void delete_foreach(GtkTreeModel *model, GtkTreePath *path,
		GtkTreeIter *iter, gpointer data)
{
	PurpleSmiley *smiley = NULL;

	gtk_tree_model_get(model, iter,
			SMILEY, &smiley,
			-1);

	if(smiley != NULL) {
		g_object_unref(G_OBJECT(smiley));
		pidgin_smiley_del_from_list(smiley);
		purple_smiley_delete(smiley);
	}
}
#endif

#if 0
static void append_to_list(GtkTreeModel *model, GtkTreePath *path,
		GtkTreeIter *iter, gpointer data)
{
	GList **list = data;
	*list = g_list_prepend(*list, gtk_tree_path_copy(path));
}
#endif

#if 0
static void smiley_delete(SmileyManager *dialog)
{
	GtkTreeSelection *selection;
	GList *list = NULL;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->treeview));
	gtk_tree_selection_selected_foreach(selection, delete_foreach, dialog);
	gtk_tree_selection_selected_foreach(selection, append_to_list, &list);

	while (list) {
		GtkTreeIter iter;
		if (gtk_tree_model_get_iter(GTK_TREE_MODEL(dialog->model), &iter, list->data))
			gtk_list_store_remove(GTK_LIST_STORE(dialog->model), &iter);
		gtk_tree_path_free(list->data);
		list = g_list_delete_link(list, list);
	}
}
#endif


/******************************************************************************
 * The Smiley Manager
 *****************************************************************************/

#if 0
static void
smiley_got_url(PurpleHttpConnection *http_conn, PurpleHttpResponse *response,
	gpointer _dialog)
{
	SmileyManager *dialog = _dialog;
	PidginSmiley *ps;
	GdkPixbuf *image;
	const gchar *smileydata;
	size_t len;

	g_assert(http_conn == smiley_manager->running_request);
	smiley_manager->running_request = NULL;

	if (!purple_http_response_is_successful(response))
		return;

	smileydata = purple_http_response_get_data(response, &len);
	image = pidgin_pixbuf_from_data((const guchar *)smileydata, len);
	if (!image)
		return;

	ps = pidgin_smiley_edit(GTK_WIDGET(dialog->window), NULL);
	pidgin_smiley_editor_set_image(ps, image);
	pidgin_smiley_editor_set_data(ps, g_memdup(smileydata, len), len);
}
#endif

#if 0
static void
smiley_dnd_recv(GtkWidget *widget, GdkDragContext *dc, guint x, guint y,
		GtkSelectionData *sd, guint info, guint t, gpointer user_data)
{
	SmileyManager *dialog = user_data;
	gchar *name = g_strchomp((gchar *) gtk_selection_data_get_data(sd));

	if ((gtk_selection_data_get_length(sd) >= 0)
      && (gtk_selection_data_get_format(sd) == 8)) {
		/* Well, it looks like the drag event was cool.
		 * Let's do something with it */

		if (!g_ascii_strncasecmp(name, "file://", 7)) {
			GError *converr = NULL;
			gchar *tmp;
			PidginSmiley *ps;
			/* It looks like we're dealing with a local file. Let's
			 * just try and read it */
			if(!(tmp = g_filename_from_uri(name, NULL, &converr))) {
				purple_debug_error("smiley dnd", "%s\n",
						   (converr ? converr->message :
							"g_filename_from_uri error"));
				return;
			}
			ps = pidgin_smiley_edit(GTK_WIDGET(dialog->window), NULL);
			image_choosen(tmp, ps);
			if (gtk_image_get_pixbuf(GTK_IMAGE(ps->smiley_image)) == NULL)
				gtk_dialog_response(GTK_DIALOG(ps->parent), GTK_RESPONSE_CANCEL);
			g_free(tmp);
		} else if (!g_ascii_strncasecmp(name, "http://", 7) ||
			!g_ascii_strncasecmp(name, "https://", 8))
		{
			/* Oo, a web drag and drop. This is where things
			 * will start to get interesting */
			purple_http_conn_cancel(smiley_manager->
				running_request);
			smiley_manager->running_request = purple_http_get(NULL,
				smiley_got_url, dialog, name);
		}

		gtk_drag_finish(dc, TRUE, FALSE, t);
	}

	gtk_drag_finish(dc, FALSE, FALSE, t);
}
#endif

static void
smiley_list_selected(GtkTreeSelection *sel, gpointer _manager)
{
	SmileyManager *manager = _manager;
	gboolean sens;

	sens = (gtk_tree_selection_count_selected_rows(sel) > 0);

	gtk_dialog_set_response_sensitive(manager->window,
		GTK_RESPONSE_NO, sens);
	gtk_dialog_set_response_sensitive(manager->window,
		PIDGIN_RESPONSE_MODIFY, sens);
}

static void
smiley_list_activated(GtkTreeView *tree, GtkTreePath *path,
	GtkTreeViewColumn *col, gpointer _manager)
{
	SmileyManager *manager = _manager;
	GtkTreeIter iter;
	PurpleSmiley *smiley = NULL;

	if (!gtk_tree_model_get_iter(
		GTK_TREE_MODEL(manager->model), &iter, path))
	{
		return;
	}

	gtk_tree_model_get(GTK_TREE_MODEL(manager->model), &iter,
		SMILEY_LIST_MODEL_PURPLESMILEY, &smiley, -1);
	g_return_if_fail(PURPLE_IS_SMILEY(smiley));

	pidgin_smiley_edit(GTK_WINDOW(manager->window), smiley);
}

static void
pidgin_smiley_manager_list_add(SmileyManager *manager, PurpleSmiley *smiley)
{
	GdkPixbuf *smiley_image;
	GtkTreeIter iter;

	/* TODO: maybe some cache? */
	smiley_image = pidgin_pixbuf_new_from_file_at_scale(
			purple_smiley_get_path(smiley), 22, 22, TRUE);

	gtk_list_store_append(manager->model, &iter);
	gtk_list_store_set(manager->model, &iter,
		SMILEY_LIST_MODEL_ICON, smiley_image,
		SMILEY_LIST_MODEL_SHORTCUT, purple_smiley_get_shortcut(smiley),
		SMILEY_LIST_MODEL_PURPLESMILEY, smiley,
		-1);

	if (smiley_image)
		g_object_unref(smiley_image);
}

static void
pidgin_smiley_manager_list_fill(SmileyManager *manager)
{
	GList *custom_smileys, *it;
	gtk_list_store_clear(manager->model);

	custom_smileys = purple_smiley_list_get_unique(
		purple_smiley_custom_get_list());

	for (it = custom_smileys; it; it = g_list_next(it)) {
		PurpleSmiley *smiley = it->data;

		pidgin_smiley_manager_list_add(manager, smiley);
	}
	g_list_free(custom_smileys);
}

static GtkWidget *
pidgin_smiley_manager_list_create(SmileyManager *manager)
{
	GtkTreeView *tree;
	GtkTreeSelection *sel;
	GtkCellRenderer *cellrend;
	GtkTreeViewColumn *column;
#if 0
	GtkTargetEntry targets[3] = {
		{"text/plain", 0, 0},
		{"text/uri-list", 0, 1},
		{"STRING", 0, 2}
	};
#endif

	manager->model = gtk_list_store_new(SMILEY_LIST_MODEL_N_COL,
		GDK_TYPE_PIXBUF, /* icon */
		G_TYPE_STRING, /* shortcut */
		G_TYPE_OBJECT /* PurpleSmiley */
		);

	manager->tree = tree = GTK_TREE_VIEW(gtk_tree_view_new_with_model(
		GTK_TREE_MODEL(manager->model)));

	gtk_tree_view_set_rules_hint(tree, TRUE);
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(manager->model),
		SMILEY_LIST_MODEL_SHORTCUT, GTK_SORT_ASCENDING);

	g_object_unref(manager->model);

	sel = gtk_tree_view_get_selection(tree);
	gtk_tree_selection_set_mode(sel, GTK_SELECTION_MULTIPLE);

	g_signal_connect(sel, "changed",
		G_CALLBACK(smiley_list_selected), manager);
	g_signal_connect(tree, "row-activated",
		G_CALLBACK(smiley_list_activated), manager);

#if 0
	gtk_drag_dest_set(GTK_WIDGET(tree), GTK_DEST_DEFAULT_MOTION |
		GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP,
		targets, G_N_ELEMENTS(targets),
		GDK_ACTION_COPY | GDK_ACTION_MOVE);
	g_signal_connect(tree, "drag_data_received",
		G_CALLBACK(smiley_dnd_recv), manager);
#endif

	gtk_widget_show(GTK_WIDGET(tree));

	/* setting up columns */

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Smiley"));
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(tree, column);
	cellrend = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, cellrend, FALSE);
	gtk_tree_view_column_add_attribute(column, cellrend,
		"pixbuf", SMILEY_LIST_MODEL_ICON);

	column = gtk_tree_view_column_new();
	gtk_tree_view_column_set_title(column, _("Shortcut Text"));
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(tree, column);
	cellrend = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, cellrend, TRUE);
	gtk_tree_view_column_add_attribute(column, cellrend,
		"text", SMILEY_LIST_MODEL_SHORTCUT);

	pidgin_smiley_manager_list_fill(manager);

	return pidgin_make_scrollable(GTK_WIDGET(tree), GTK_POLICY_AUTOMATIC,
		GTK_POLICY_AUTOMATIC, GTK_SHADOW_IN, -1, -1);
}

#if 0
static void refresh_list()
{
	populate_smiley_list(smiley_manager);
}
#endif

static void
smiley_manager_select_cb(GtkWidget *widget, gint resp, SmileyManager *manager)
{
	GtkTreeSelection *selection = NULL;
	GList *selected_rows, *selected_smileys = NULL, *it;
	GtkTreeModel *model = GTK_TREE_MODEL(manager->model);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(manager->tree));
	selected_rows = gtk_tree_selection_get_selected_rows(selection, NULL);
	for (it = selected_rows; it; it = g_list_next(it)) {
		GtkTreePath *path = it->data;
		GtkTreeIter iter;
		PurpleSmiley *smiley = NULL;

		if (!gtk_tree_model_get_iter(model, &iter, path))
			continue;

		gtk_tree_model_get(model, &iter,
			SMILEY_LIST_MODEL_PURPLESMILEY, &smiley, -1);
		if (!smiley)
			continue;

		selected_smileys = g_list_prepend(selected_smileys, smiley);
	}
	g_list_free_full(selected_rows, (GDestroyNotify)gtk_tree_path_free);

	switch (resp) {
		case GTK_RESPONSE_YES:
			pidgin_smiley_edit(GTK_WINDOW(manager->window), NULL);
			break;
#if 0
		case GTK_RESPONSE_NO:
			smiley_delete(manager);
			break;
#endif
		case GTK_RESPONSE_DELETE_EVENT:
		case GTK_RESPONSE_CLOSE:
			gtk_widget_destroy(GTK_WIDGET(manager->window));
			purple_http_conn_cancel(manager->running_request);
			g_free(manager);
			smiley_manager = NULL;
			break;
		case PIDGIN_RESPONSE_MODIFY:
			for (it = selected_smileys; it; it = g_list_next(it)) {
				pidgin_smiley_edit(GTK_WINDOW(manager->window),
					it->data);
			}
			break;
		default:
			g_warn_if_reached();
	}

	g_list_free(selected_smileys);
}

void
pidgin_smiley_manager_show(void)
{
	SmileyManager *manager;
	GtkDialog *win;
	GtkWidget *sw, *vbox;

	if (smiley_manager) {
		gtk_window_present(GTK_WINDOW(smiley_manager->window));
		return;
	}

	manager = g_new0(SmileyManager, 1);
	smiley_manager = manager;

	manager->window = win = GTK_DIALOG(gtk_dialog_new_with_buttons(
		_("Custom Smiley Manager"), NULL,
		GTK_DIALOG_DESTROY_WITH_PARENT,
		PIDGIN_STOCK_ADD, GTK_RESPONSE_YES,
		PIDGIN_STOCK_MODIFY, PIDGIN_RESPONSE_MODIFY,
		GTK_STOCK_DELETE, GTK_RESPONSE_NO,
		GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
		NULL));

	gtk_window_set_default_size(GTK_WINDOW(win), 50, 400);
	gtk_window_set_role(GTK_WINDOW(win), "custom_smiley_manager");
#if !GTK_CHECK_VERSION(3,0,0)
	gtk_container_set_border_width(GTK_CONTAINER(win),PIDGIN_HIG_BORDER);
#endif
	gtk_dialog_set_response_sensitive(win, GTK_RESPONSE_NO, FALSE);
	gtk_dialog_set_response_sensitive(win, PIDGIN_RESPONSE_MODIFY, FALSE);

	g_signal_connect(win, "response",
		G_CALLBACK(smiley_manager_select_cb), manager);

	/* The vbox */
	vbox = gtk_dialog_get_content_area(win);

	/* get the scrolled window with all stuff */
	sw = pidgin_smiley_manager_list_create(manager);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);
	gtk_widget_show(sw);

	gtk_widget_show(GTK_WIDGET(win));
}