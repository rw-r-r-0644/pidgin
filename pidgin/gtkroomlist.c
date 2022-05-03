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

#include <gtk/gtk.h>

#include <purple.h>

#include "gtkroomlist.h"

#include "gtkutils.h"
#include "pidginaccountchooser.h"

#define PIDGIN_TYPE_ROOMLIST_DIALOG (pidgin_roomlist_dialog_get_type())
G_DECLARE_FINAL_TYPE(PidginRoomlistDialog, pidgin_roomlist_dialog, PIDGIN,
                     ROOMLIST_DIALOG, GtkDialog)

#define PIDGIN_ROOMLIST_UI_DATA "pidgin-ui"

struct _PidginRoomlistDialog {
	GtkDialog parent;

	GtkWidget *account_widget;
	GtkWidget *progress;
	GtkWidget *tree;

	GtkWidget *stop_button;
	GtkWidget *list_button;
	GtkWidget *add_button;
	GtkWidget *join_button;
	GtkWidget *close_button;

	PurpleAccount *account;
	PurpleRoomlist *roomlist;

	gboolean pg_needs_pulse;
	guint pg_update_to;
};

G_DEFINE_TYPE(PidginRoomlistDialog, pidgin_roomlist_dialog, GTK_TYPE_DIALOG)

typedef struct {
	PidginRoomlistDialog *dialog;
	GtkTreeStore *model;
} PidginRoomlist;

enum {
	ROOM_COLUMN = 0,
	NAME_COLUMN,
	DESCRIPTION_COLUMN,
	NUM_OF_COLUMNS,
};

static gboolean
_search_func(GtkTreeModel *model, gint column, const gchar *key,
             GtkTreeIter *iter, gpointer search_data)
{
	gboolean result;
	gchar *name, *fold, *fkey;

	gtk_tree_model_get(model, iter, column, &name, -1);
	fold = g_utf8_casefold(name, -1);
	fkey = g_utf8_casefold(key, -1);

	result = (g_strstr_len(fold, strlen(fold), fkey) == NULL);

	g_free(fold);
	g_free(fkey);
	g_free(name);

	return result;
}

static gint delete_win_cb(GtkWidget *w, GdkEventAny *e, gpointer d)
{
	PidginRoomlistDialog *dialog = PIDGIN_ROOMLIST_DIALOG(w);

	if (dialog->roomlist && purple_roomlist_get_in_progress(dialog->roomlist))
		purple_roomlist_cancel_get_list(dialog->roomlist);

	if (dialog->pg_update_to > 0)
		g_source_remove(dialog->pg_update_to);

	if (dialog->roomlist) {
		PidginRoomlist *rl = NULL;

		rl = g_object_get_data(G_OBJECT(dialog->roomlist),
		                       PIDGIN_ROOMLIST_UI_DATA);

		if (dialog->pg_update_to > 0)
			/* yes, that's right, unref it twice. */
			g_object_unref(dialog->roomlist);

		if (rl)
			rl->dialog = NULL;
		g_object_unref(dialog->roomlist);
	}

	dialog->progress = NULL;

	return FALSE;
}

static void
dialog_select_account_cb(GtkWidget *w, PidginRoomlistDialog *dialog) {
	PidginAccountChooser *chooser = PIDGIN_ACCOUNT_CHOOSER(w);
	PurpleAccount *account = pidgin_account_chooser_get_selected(chooser);
	gboolean change = (account != dialog->account);
	dialog->account = account;

	if (change && dialog->roomlist) {
		PidginRoomlist *rl = NULL;

		rl = g_object_get_data(G_OBJECT(dialog->roomlist),
		                       PIDGIN_ROOMLIST_UI_DATA);

		g_clear_object(&rl->model);
		g_object_unref(dialog->roomlist);
		dialog->roomlist = NULL;
	}
}

static void list_button_cb(GtkButton *button, PidginRoomlistDialog *dialog)
{
	PurpleConnection *gc;
	PidginRoomlist *rl;

	gc = purple_account_get_connection(dialog->account);
	if (!gc)
		return;

	if (dialog->roomlist != NULL) {
		rl = g_object_get_data(G_OBJECT(dialog->roomlist),
		                       PIDGIN_ROOMLIST_UI_DATA);

		g_clear_object(&rl->model);
		g_object_unref(dialog->roomlist);
	}

	dialog->roomlist = purple_roomlist_get_list(gc);
	if (!dialog->roomlist)
		return;
	g_object_ref(dialog->roomlist);

	rl = g_object_get_data(G_OBJECT(dialog->roomlist),
	                       PIDGIN_ROOMLIST_UI_DATA);
	rl->dialog = dialog;

	gtk_widget_set_sensitive(dialog->account_widget, FALSE);

	gtk_tree_view_set_model(GTK_TREE_VIEW(dialog->tree),
	                        GTK_TREE_MODEL(rl->model));

	/* some protocols (not bundled with libpurple) finish getting their
	 * room list immediately */
	if(purple_roomlist_get_in_progress(dialog->roomlist)) {
		gtk_widget_set_sensitive(dialog->stop_button, TRUE);
		gtk_widget_set_sensitive(dialog->list_button, FALSE);
	} else {
		gtk_widget_set_sensitive(dialog->stop_button, FALSE);
		gtk_widget_set_sensitive(dialog->list_button, TRUE);
	}
	gtk_widget_set_sensitive(dialog->add_button, FALSE);
	gtk_widget_set_sensitive(dialog->join_button, FALSE);
}

static void stop_button_cb(GtkButton *button, PidginRoomlistDialog *dialog)
{
	purple_roomlist_cancel_get_list(dialog->roomlist);

	gtk_widget_set_sensitive(dialog->account_widget, TRUE);

	gtk_widget_set_sensitive(dialog->stop_button, FALSE);
	gtk_widget_set_sensitive(dialog->list_button, TRUE);
	gtk_widget_set_sensitive(dialog->add_button, FALSE);
	gtk_widget_set_sensitive(dialog->join_button, FALSE);
}

struct _menu_cb_info {
	PurpleRoomlist *list;
	PurpleRoomlistRoom *room;
};

static void
selection_changed_cb(GtkTreeSelection *selection,
                     PidginRoomlistDialog *dialog)
{
	GtkTreeIter iter;
	PurpleRoomlistRoom *room;
	static struct _menu_cb_info *info;
	PidginRoomlist *grl = NULL;

	grl = g_object_get_data(G_OBJECT(dialog->roomlist),
	                        PIDGIN_ROOMLIST_UI_DATA);

	if (gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		gtk_tree_model_get(GTK_TREE_MODEL(grl->model), &iter,
		                   ROOM_COLUMN, &room,
		                   -1);

		info = g_new0(struct _menu_cb_info, 1);
		info->list = dialog->roomlist;
		info->room = room;

		g_object_set_data_full(G_OBJECT(dialog->join_button), "room-info",
							   info, g_free);
		g_object_set_data(G_OBJECT(dialog->add_button), "room-info", info);

		gtk_widget_set_sensitive(dialog->add_button, TRUE);
		gtk_widget_set_sensitive(dialog->join_button, TRUE);

		g_object_unref(room);
	} else {
		gtk_widget_set_sensitive(dialog->add_button, FALSE);
		gtk_widget_set_sensitive(dialog->join_button, FALSE);
	}
}

static void
do_add_room_cb(G_GNUC_UNUSED GtkWidget *w, struct _menu_cb_info *info)
{
	char *name;
	PurpleAccount *account = purple_roomlist_get_account(info->list);
	PurpleConnection *gc = purple_account_get_connection(account);
	PurpleProtocol *protocol = NULL;

	if(gc != NULL) {
		protocol = purple_connection_get_protocol(gc);
	}

	if(protocol != NULL && PURPLE_PROTOCOL_IMPLEMENTS(protocol, ROOMLIST, room_serialize)) {
		name = purple_protocol_roomlist_room_serialize(PURPLE_PROTOCOL_ROOMLIST(protocol),
		                                               info->room);
	} else {
		name = g_strdup(purple_roomlist_room_get_name(info->room));
	}

	purple_blist_request_add_chat(account, NULL, NULL, name);

	g_free(name);
}

static void
add_room_to_blist_cb(GtkButton *button, G_GNUC_UNUSED gpointer data)
{
	struct _menu_cb_info *info = g_object_get_data(G_OBJECT(button), "room-info");

	if(info != NULL) {
		do_add_room_cb(NULL, info);
	}
}

static void
do_join_cb(G_GNUC_UNUSED GtkWidget *w, struct _menu_cb_info *info) {
	purple_roomlist_join_room(info->list, info->room);
}

static void
join_button_cb(GtkButton *button, G_GNUC_UNUSED gpointer data) {
	struct _menu_cb_info *info = g_object_get_data(G_OBJECT(button), "room-info");

	purple_roomlist_join_room(info->list, info->room);
}

static void
row_activated_cb(GtkTreeView *tv, GtkTreePath *path, GtkTreeViewColumn *arg2,
                 gpointer data)
{
	PidginRoomlistDialog *dialog = data;
	PidginRoomlist *grl = NULL;
	GtkTreeIter iter;
	PurpleRoomlistRoom *room;
	struct _menu_cb_info info;

	grl = g_object_get_data(G_OBJECT(dialog->roomlist), PIDGIN_ROOMLIST_UI_DATA);

	gtk_tree_model_get_iter(GTK_TREE_MODEL(grl->model), &iter, path);
	gtk_tree_model_get(GTK_TREE_MODEL(grl->model), &iter, ROOM_COLUMN, &room, -1);

	info.list = dialog->roomlist;
	info.room = room;

	do_join_cb(NULL, &info);

	g_clear_object(&room);
}

static gboolean
room_click_cb(GtkWidget *tv, GdkEventButton *event, gpointer data) {
	PidginRoomlistDialog *dialog = data;
	GtkTreePath *path;
	PidginRoomlist *grl = NULL;
	PurpleRoomlistRoom *room;
	GtkTreeIter iter;
	GtkWidget *menu;
	GtkWidget *menuitem;
	static struct _menu_cb_info info; /* XXX? */

	if (!gdk_event_triggers_context_menu((GdkEvent *)event))
		return FALSE;

	grl = g_object_get_data(G_OBJECT(dialog->roomlist), PIDGIN_ROOMLIST_UI_DATA);

	/* Here we figure out which room was clicked */
	if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(tv), event->x, event->y, &path, NULL, NULL, NULL))
		return FALSE;
	gtk_tree_model_get_iter(GTK_TREE_MODEL(grl->model), &iter, path);
	gtk_tree_path_free(path);
	gtk_tree_model_get(GTK_TREE_MODEL(grl->model), &iter, ROOM_COLUMN, &room, -1);

	info.list = dialog->roomlist;
	info.room = room;

	/* The current implementation isn't expecting a ref to unref the one we got
	 * when we pulled the room out of the model.
	 */
	g_clear_object(&room);

	menu = gtk_menu_new();

	menuitem = gtk_menu_item_new_with_mnemonic(_("_Join"));
	g_signal_connect(G_OBJECT(menuitem), "activate",
	                 G_CALLBACK(do_join_cb), &info);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	menuitem = gtk_menu_item_new_with_mnemonic(_("_Add"));
	g_signal_connect(G_OBJECT(menuitem), "activate",
	                 G_CALLBACK(do_add_room_cb), &info);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

	gtk_widget_show_all(menu);
	gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);

	return FALSE;
}

#define SMALL_SPACE 6

static gboolean
pidgin_roomlist_query_tooltip(GtkWidget *widget, int x, int y,
                              gboolean keyboard_mode, GtkTooltip *tooltip,
                              gpointer data)
{
	PidginRoomlistDialog *dialog = data;
	PidginRoomlist *grl = NULL;
	GtkTreePath *path = NULL;
	GtkTreeIter iter;
	gchar *name, *tmp;
	GString *tooltip_text = NULL;

	grl = g_object_get_data(G_OBJECT(dialog->roomlist), PIDGIN_ROOMLIST_UI_DATA);

	if (keyboard_mode) {
		GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
		if (!gtk_tree_selection_get_selected(selection, NULL, &iter)) {
			return FALSE;
		}
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(grl->model), &iter);
	} else {
		gint bx, by;

		gtk_tree_view_convert_widget_to_bin_window_coords(GTK_TREE_VIEW(widget),
		                                                  x, y, &bx, &by);
		gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), bx, by, &path,
		                              NULL, NULL, NULL);
		if (path == NULL) {
			return FALSE;
		}

		if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(grl->model), &iter, path)) {
			gtk_tree_path_free(path);
			return FALSE;
		}
	}

	tooltip_text = g_string_new("");

	gtk_tree_model_get(GTK_TREE_MODEL(grl->model), &iter, NAME_COLUMN, &name, -1);
	tmp = g_markup_escape_text(name, -1);
	g_free(name);
	g_string_append_printf(
	    tooltip_text, "<span size='x-large' weight='bold'>%s</span>\n", tmp);
	g_free(tmp);

	gtk_tooltip_set_markup(tooltip, tooltip_text->str);
	gtk_tree_view_set_tooltip_row(GTK_TREE_VIEW(widget), tooltip, path);
	g_string_free(tooltip_text, TRUE);
	gtk_tree_path_free(path);

	return TRUE;
}

static gboolean account_filter_func(PurpleAccount *account)
{
	PurpleConnection *conn = purple_account_get_connection(account);
	PurpleProtocol *protocol = NULL;

	if (conn && PURPLE_CONNECTION_IS_CONNECTED(conn))
		protocol = purple_connection_get_protocol(conn);

	return (protocol && PURPLE_PROTOCOL_IMPLEMENTS(protocol, ROOMLIST, get_list));
}

gboolean
pidgin_roomlist_is_showable()
{
	GList *c;
	PurpleConnection *gc;

	for (c = purple_connections_get_all(); c != NULL; c = c->next) {
		gc = c->data;

		if (account_filter_func(purple_connection_get_account(gc)))
			return TRUE;
	}

	return FALSE;
}

static void
pidgin_roomlist_dialog_class_init(PidginRoomlistDialogClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(
	        widget_class, "/im/pidgin/Pidgin3/Roomlist/roomlist.ui");

	gtk_widget_class_bind_template_child(widget_class, PidginRoomlistDialog,
	                                     account_widget);
	gtk_widget_class_bind_template_child(widget_class, PidginRoomlistDialog,
	                                     tree);
	gtk_widget_class_bind_template_child(widget_class, PidginRoomlistDialog,
	                                     add_button);
	gtk_widget_class_bind_template_child(widget_class, PidginRoomlistDialog,
	                                     close_button);
	gtk_widget_class_bind_template_child(widget_class, PidginRoomlistDialog,
	                                     join_button);
	gtk_widget_class_bind_template_child(widget_class, PidginRoomlistDialog,
	                                     list_button);
	gtk_widget_class_bind_template_child(widget_class, PidginRoomlistDialog,
	                                     progress);
	gtk_widget_class_bind_template_child(widget_class, PidginRoomlistDialog,
	                                     stop_button);

	gtk_widget_class_bind_template_callback(widget_class,
	                                        add_room_to_blist_cb);
	gtk_widget_class_bind_template_callback(widget_class, delete_win_cb);
	gtk_widget_class_bind_template_callback(widget_class, row_activated_cb);
	gtk_widget_class_bind_template_callback(widget_class, room_click_cb);
	gtk_widget_class_bind_template_callback(widget_class,
	                                        dialog_select_account_cb);
	gtk_widget_class_bind_template_callback(widget_class,
	                                        selection_changed_cb);
	gtk_widget_class_bind_template_callback(widget_class,
	                                        pidgin_roomlist_query_tooltip);
	gtk_widget_class_bind_template_callback(widget_class, join_button_cb);
	gtk_widget_class_bind_template_callback(widget_class, list_button_cb);
	gtk_widget_class_bind_template_callback(widget_class, stop_button_cb);
}

static void
pidgin_roomlist_dialog_init(PidginRoomlistDialog *self)
{
	gtk_widget_init_template(GTK_WIDGET(self));

	pidgin_account_chooser_set_filter_func(
	        PIDGIN_ACCOUNT_CHOOSER(self->account_widget),
	        account_filter_func);

	gtk_tree_view_set_search_equal_func(GTK_TREE_VIEW(self->tree),
	                                    _search_func, NULL, NULL);
}

static PidginRoomlistDialog *
pidgin_roomlist_dialog_new_with_account(PurpleAccount *account)
{
	PidginRoomlistDialog *dialog = NULL;
	PidginAccountChooser *chooser = NULL;

	dialog = g_object_new(PIDGIN_TYPE_ROOMLIST_DIALOG, NULL);
	dialog->account = account;

	chooser = PIDGIN_ACCOUNT_CHOOSER(dialog->account_widget);

	if (!account) {
		/* This is normally NULL, and we normally don't care what the
		 * first selected item is */
		dialog->account = pidgin_account_chooser_get_selected(chooser);
	} else {
		pidgin_account_chooser_set_selected(chooser, account);
	}

	/* show the dialog window and return the dialog */
	gtk_widget_show(GTK_WIDGET(dialog));

	return dialog;
}

void pidgin_roomlist_dialog_show_with_account(PurpleAccount *account)
{
	PidginRoomlistDialog *dialog = pidgin_roomlist_dialog_new_with_account(account);

	if (!dialog)
		return;

	list_button_cb(GTK_BUTTON(dialog->list_button), dialog);
}

void pidgin_roomlist_dialog_show(void)
{
	pidgin_roomlist_dialog_new_with_account(NULL);
}

static gboolean pidgin_progress_bar_pulse(gpointer data)
{
	PurpleRoomlist *list = data;
	PidginRoomlist *rl = NULL;

	rl = g_object_get_data(G_OBJECT(list), PIDGIN_ROOMLIST_UI_DATA);
	if (!rl || !rl->dialog || !rl->dialog->pg_needs_pulse) {
		if (rl && rl->dialog)
			rl->dialog->pg_update_to = 0;
		g_object_unref(list);
		return FALSE;
	}

	gtk_progress_bar_pulse(GTK_PROGRESS_BAR(rl->dialog->progress));
	rl->dialog->pg_needs_pulse = FALSE;
	return TRUE;
}

static void
pidgin_roomlist_add_room(PurpleRoomlist *list, PurpleRoomlistRoom *room) {
	PidginRoomlist *rl = NULL;
	GtkTreePath *path;
	GtkTreeIter iter;

	rl = g_object_get_data(G_OBJECT(list), PIDGIN_ROOMLIST_UI_DATA);

	if (rl->dialog) {
		if (rl->dialog->pg_update_to == 0) {
			g_object_ref(list);
			rl->dialog->pg_update_to = g_timeout_add(100, pidgin_progress_bar_pulse, list);
			gtk_progress_bar_pulse(GTK_PROGRESS_BAR(rl->dialog->progress));
		} else
			rl->dialog->pg_needs_pulse = TRUE;
	}

	gtk_tree_store_append(rl->model, &iter, NULL);

	path = gtk_tree_model_get_path(GTK_TREE_MODEL(rl->model), &iter);

	gtk_tree_path_free(path);

	gtk_tree_store_set(
		rl->model, &iter,
		ROOM_COLUMN, room,
		NAME_COLUMN, purple_roomlist_room_get_name(room),
		DESCRIPTION_COLUMN, purple_roomlist_room_get_description(room),
		-1);
}

static void
pidgin_roomlist_in_progress(GObject *obj, G_GNUC_UNUSED GParamSpec *pspec,
                            gpointer data)
{
	PurpleRoomlist *list = PURPLE_ROOMLIST(obj);
	PidginRoomlist *rl = data;

	if (purple_roomlist_get_in_progress(list)) {
		if (rl->dialog->account_widget) {
			gtk_widget_set_sensitive(rl->dialog->account_widget, FALSE);
		}
		gtk_widget_set_sensitive(rl->dialog->stop_button, TRUE);
		gtk_widget_set_sensitive(rl->dialog->list_button, FALSE);
	} else {
		rl->dialog->pg_needs_pulse = FALSE;
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(rl->dialog->progress), 0.0);
		if (rl->dialog->account_widget) {
			gtk_widget_set_sensitive(rl->dialog->account_widget, TRUE);
		}
		gtk_widget_set_sensitive(rl->dialog->stop_button, FALSE);
		gtk_widget_set_sensitive(rl->dialog->list_button, TRUE);
	}
}

static void
pidgin_roomlist_destroy(PidginRoomlist *rl)
{
	g_free(rl);
}

static void
pidgin_roomlist_new(PurpleRoomlist *list)
{
	PidginRoomlist *rl = g_new0(PidginRoomlist, 1);

	g_object_set_data_full(G_OBJECT(list), PIDGIN_ROOMLIST_UI_DATA, rl,
	                       (GDestroyNotify)pidgin_roomlist_destroy);

	rl->model = gtk_tree_store_new(3, G_TYPE_OBJECT, G_TYPE_STRING,
	                               G_TYPE_STRING);

	g_signal_connect(list, "notify::in-progress",
	                 G_CALLBACK(pidgin_roomlist_in_progress), rl);
}

static PurpleRoomlistUiOps ops = {
	pidgin_roomlist_dialog_show_with_account,
	pidgin_roomlist_new,
	NULL,
	pidgin_roomlist_add_room,
	NULL,
	NULL,
	NULL,
	NULL
};


void pidgin_roomlist_init(void)
{
	purple_roomlist_set_ui_ops(&ops);
}
