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

#include "pidginscrollbook.h"

struct _PidginScrollBook {
	GtkVBox parent;

	GtkWidget *notebook;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *left_arrow;
	GtkWidget *right_arrow;
	GList *children;
};

G_DEFINE_TYPE(PidginScrollBook, pidgin_scroll_book, GTK_TYPE_BOX)

/******************************************************************************
 * Helpers
 *****************************************************************************/
static gboolean
scroll_left_cb(PidginScrollBook *scroll_book, GdkEventButton *event)
{
	int index;

	if (event->type != GDK_BUTTON_PRESS)
		return FALSE;

	index = gtk_notebook_get_current_page(GTK_NOTEBOOK(scroll_book->notebook));

	if (index > 0)
		gtk_notebook_set_current_page(GTK_NOTEBOOK(scroll_book->notebook), index - 1);
	return TRUE;
}

static gboolean
scroll_right_cb(PidginScrollBook *scroll_book, GdkEventButton *event)
{
	int index, count;

	if (event->type != GDK_BUTTON_PRESS)
		return FALSE;

	index = gtk_notebook_get_current_page(GTK_NOTEBOOK(scroll_book->notebook));
	count = gtk_notebook_get_n_pages(GTK_NOTEBOOK(scroll_book->notebook));

	if (index + 1 < count)
		gtk_notebook_set_current_page(GTK_NOTEBOOK(scroll_book->notebook), index + 1);
	return TRUE;
}

static void
refresh_scroll_box(PidginScrollBook *scroll_book, int index, int count)
{
	char *label;

	gtk_widget_show_all(GTK_WIDGET(scroll_book));
	if (count < 1)
		gtk_widget_hide(scroll_book->hbox);
	else {
		gtk_widget_show_all(scroll_book->hbox);
		if (count == 1) {
			gtk_widget_hide(scroll_book->label);
			gtk_widget_hide(scroll_book->left_arrow);
			gtk_widget_hide(scroll_book->right_arrow);
		}
	}

	label = g_strdup_printf("<span size='smaller' weight='bold'>(%d/%d)</span>", index+1, count);
	gtk_label_set_markup(GTK_LABEL(scroll_book->label), label);
	g_free(label);

	if (index == 0)
		gtk_widget_set_sensitive(scroll_book->left_arrow, FALSE);
	else
		gtk_widget_set_sensitive(scroll_book->left_arrow, TRUE);


	if (index + 1 == count)
		gtk_widget_set_sensitive(scroll_book->right_arrow, FALSE);
	else
		gtk_widget_set_sensitive(scroll_book->right_arrow, TRUE);
}


static void
page_count_change_cb(PidginScrollBook *scroll_book)
{
	int count;
	int index = gtk_notebook_get_current_page(GTK_NOTEBOOK(scroll_book->notebook));
	count = gtk_notebook_get_n_pages(GTK_NOTEBOOK(scroll_book->notebook));
	refresh_scroll_box(scroll_book, index, count);
}

static gboolean
scroll_close_cb(PidginScrollBook *scroll_book, GdkEventButton *event)
{
	if (event->type == GDK_BUTTON_PRESS)
		gtk_widget_destroy(gtk_notebook_get_nth_page(GTK_NOTEBOOK(scroll_book->notebook), gtk_notebook_get_current_page(GTK_NOTEBOOK(scroll_book->notebook))));
	return FALSE;
}

static void
switch_page_cb(GtkNotebook *notebook, GtkWidget *page, guint page_num, PidginScrollBook *scroll_book)
{
	int count;
	count = gtk_notebook_get_n_pages(GTK_NOTEBOOK(scroll_book->notebook));
	refresh_scroll_box(scroll_book, page_num, count);
}

static gboolean
close_button_left_cb(GtkWidget *widget, GdkEventCrossing *event, GtkLabel *label)
{
	static GdkCursor *ptr = NULL;
	if (ptr == NULL) {
		GdkDisplay *display = gtk_widget_get_display(widget);
		ptr = gdk_cursor_new_for_display(display, GDK_LEFT_PTR);
	}

	gtk_label_set_markup(label, "×");
	gdk_window_set_cursor(event->window, ptr);
	return FALSE;
}

static gboolean
close_button_entered_cb(GtkWidget *widget, GdkEventCrossing *event, GtkLabel *label)
{
	static GdkCursor *hand = NULL;
	if (hand == NULL) {
		GdkDisplay *display = gtk_widget_get_display(widget);
		hand = gdk_cursor_new_for_display(display, GDK_HAND2);
	}

	gtk_label_set_markup(label, "<u>×</u>");
	gdk_window_set_cursor(event->window, hand);
	return FALSE;
}

/******************************************************************************
 * GtkContainer Implementation
 *****************************************************************************/
static void
pidgin_scroll_book_add(GtkContainer *container, GtkWidget *widget)
{
	PidginScrollBook *scroll_book;

	g_return_if_fail(GTK_IS_WIDGET (widget));
	g_return_if_fail(gtk_widget_get_parent(widget) == NULL);

	scroll_book = PIDGIN_SCROLL_BOOK(container);
	scroll_book->children = g_list_append(scroll_book->children, widget);
	gtk_widget_show(widget);
	gtk_notebook_append_page(GTK_NOTEBOOK(scroll_book->notebook), widget, NULL);
	page_count_change_cb(PIDGIN_SCROLL_BOOK(container));
}

static void
pidgin_scroll_book_remove(GtkContainer *container, GtkWidget *widget)
{
	int page;
	PidginScrollBook *scroll_book;
	g_return_if_fail(GTK_IS_WIDGET(widget));

	scroll_book = PIDGIN_SCROLL_BOOK(container);
	scroll_book->children = g_list_remove(scroll_book->children, widget);
	/* gtk_widget_unparent(widget); */

	page = gtk_notebook_page_num(GTK_NOTEBOOK(PIDGIN_SCROLL_BOOK(container)->notebook), widget);
	if (page >= 0) {
		gtk_notebook_remove_page(GTK_NOTEBOOK(PIDGIN_SCROLL_BOOK(container)->notebook), page);
	}
}

static void
pidgin_scroll_book_forall(GtkContainer *container,
			   gboolean include_internals,
			   GtkCallback callback,
			   gpointer callback_data)
{
	PidginScrollBook *scroll_book;

	g_return_if_fail(GTK_IS_CONTAINER(container));

	scroll_book = PIDGIN_SCROLL_BOOK(container);

	if (include_internals) {
		(*callback)(scroll_book->hbox, callback_data);
		(*callback)(scroll_book->notebook, callback_data);
	}
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
static void
pidgin_scroll_book_class_init(PidginScrollBookClass *klass) {
	GtkContainerClass *container_class = GTK_CONTAINER_CLASS(klass);

	container_class->add = pidgin_scroll_book_add;
	container_class->remove = pidgin_scroll_book_remove;
	container_class->forall = pidgin_scroll_book_forall;
}

static void
pidgin_scroll_book_init(PidginScrollBook *scroll_book) {
	GIcon *icon;
	GtkWidget *eb;
	GtkWidget *close_button;
	const gchar *left_arrow_icon_names[] = {
	        "pan-start-symbolic",
	        "pan-left-symbolic",
	};
	const gchar *right_arrow_icon_names[] = {
	        "pan-end-symbolic",
	        "pan-right-symbolic",
	};

	gtk_orientable_set_orientation(GTK_ORIENTABLE(scroll_book), GTK_ORIENTATION_VERTICAL);

	scroll_book->hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

	/* Close */
	eb = gtk_event_box_new();
	gtk_box_pack_end(GTK_BOX(scroll_book->hbox), eb, FALSE, FALSE, 0);
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(eb), FALSE);
	gtk_widget_set_events(eb, GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK);
	close_button = gtk_label_new("×");
	g_signal_connect(G_OBJECT(eb), "enter-notify-event", G_CALLBACK(close_button_entered_cb), close_button);
	g_signal_connect(G_OBJECT(eb), "leave-notify-event", G_CALLBACK(close_button_left_cb), close_button);
	gtk_container_add(GTK_CONTAINER(eb), close_button);
	g_signal_connect_swapped(G_OBJECT(eb), "button-press-event", G_CALLBACK(scroll_close_cb), scroll_book);

	/* Right arrow */
	eb = gtk_event_box_new();
	gtk_box_pack_end(GTK_BOX(scroll_book->hbox), eb, FALSE, FALSE, 0);
	icon = g_themed_icon_new_from_names((char **)right_arrow_icon_names,
	                                    G_N_ELEMENTS(right_arrow_icon_names));
	scroll_book->right_arrow =
	        gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_BUTTON);
	g_object_unref(icon);
	gtk_container_add(GTK_CONTAINER(eb), scroll_book->right_arrow);
	g_signal_connect_swapped(G_OBJECT(eb), "button-press-event", G_CALLBACK(scroll_right_cb), scroll_book);

	/* Count */
	scroll_book->label = gtk_label_new(NULL);
	gtk_box_pack_end(GTK_BOX(scroll_book->hbox), scroll_book->label, FALSE, FALSE, 0);

	/* Left arrow */
	eb = gtk_event_box_new();
	gtk_box_pack_end(GTK_BOX(scroll_book->hbox), eb, FALSE, FALSE, 0);
	icon = g_themed_icon_new_from_names((char **)left_arrow_icon_names,
	                                    G_N_ELEMENTS(left_arrow_icon_names));
	scroll_book->left_arrow =
	        gtk_image_new_from_gicon(icon, GTK_ICON_SIZE_BUTTON);
	g_object_unref(icon);
	gtk_container_add(GTK_CONTAINER(eb), scroll_book->left_arrow);
	g_signal_connect_swapped(G_OBJECT(eb), "button-press-event", G_CALLBACK(scroll_left_cb), scroll_book);

	gtk_box_pack_start(GTK_BOX(scroll_book), scroll_book->hbox, FALSE, FALSE, 0);

	scroll_book->notebook = gtk_notebook_new();
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(scroll_book->notebook), FALSE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(scroll_book->notebook), FALSE);

	gtk_box_pack_start(GTK_BOX(scroll_book), scroll_book->notebook, TRUE, TRUE, 0);

	g_signal_connect_swapped(G_OBJECT(scroll_book->notebook), "remove", G_CALLBACK(page_count_change_cb), scroll_book);
	g_signal_connect(G_OBJECT(scroll_book->notebook), "switch-page", G_CALLBACK(switch_page_cb), scroll_book);
	gtk_widget_show_all(scroll_book->notebook);
}

/******************************************************************************
 * Public API
 *****************************************************************************/
GtkWidget *
pidgin_scroll_book_new() {
	return g_object_new(
		PIDGIN_TYPE_SCROLL_BOOK,
		"orientation", GTK_ORIENTATION_VERTICAL,
		NULL);
}

GtkWidget *
pidgin_scroll_book_get_notebook(PidginScrollBook *scroll_book) {
	g_return_val_if_fail(PIDGIN_IS_SCROLL_BOOK(scroll_book), NULL);

	return scroll_book->notebook;
}
