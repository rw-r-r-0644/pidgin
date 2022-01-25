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

#include "pidginconversationwindow.h"

#include "gtkconv.h"

enum {
	PIDGIN_CONVERSATION_WINDOW_COLUMN_OBJECT,
	PIDGIN_CONVERSATION_WINDOW_COLUMN_ICON,
	PIDGIN_CONVERSATION_WINDOW_COLUMN_MARKUP,
};

enum {
	SIG_CONVERSATION_SWITCHED,
	N_SIGNALS,
};
static guint signals[N_SIGNALS] = {0, };

struct _PidginConversationWindow {
	GtkApplicationWindow parent;

	GtkWidget *vbox;

	GtkWidget *view;
	GtkTreeStore *model;

	GtkWidget *stack;
};

G_DEFINE_TYPE(PidginConversationWindow, pidgin_conversation_window,
              GTK_TYPE_APPLICATION_WINDOW)

static GtkWidget *default_window = NULL;

/******************************************************************************
 * Callbacks
 *****************************************************************************/
static void
pidgin_conversation_window_selection_changed(GtkTreeSelection *selection,
                                             gpointer data)
{
	PidginConversationWindow *window = PIDGIN_CONVERSATION_WINDOW(data);
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	gboolean changed = FALSE;

	if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
		gchar *markup = NULL;

		gtk_tree_model_get(model, &iter,
		                   PIDGIN_CONVERSATION_WINDOW_COLUMN_MARKUP, &markup,
		                   -1);

		gtk_stack_set_visible_child_name(GTK_STACK(window->stack), markup);
		g_free(markup);

		changed = TRUE;
	}

	if(!changed) {
		gtk_stack_set_visible_child_name(GTK_STACK(window->stack), "__empty__");
	}
}

static gboolean
pidgin_conversation_window_foreach_destroy(GtkTreeModel *model,
                                           GtkTreePath *path,
                                           GtkTreeIter *iter,
                                           gpointer data)
{
	PurpleConversation *conversation = NULL;

	gtk_tree_model_get(model, iter,
	                   PIDGIN_CONVERSATION_WINDOW_COLUMN_OBJECT, &conversation,
	                   -1);

	if(conversation != NULL) {
		pidgin_conversation_detach(conversation);

		gtk_list_store_remove(GTK_LIST_STORE(model), iter);
	}

	return FALSE;
}

static gboolean
pidgin_conversation_window_key_pressed_cb(GtkEventControllerKey *controller,
                                          guint keyval,
                                          G_GNUC_UNUSED guint keycode,
                                          GdkModifierType state,
                                          gpointer data)
{
	PidginConversationWindow *window = data;

	/* If CTRL was held down... */
	if (state & GDK_CONTROL_MASK) {
		switch (keyval) {
			case GDK_KEY_Page_Down:
			case GDK_KEY_KP_Page_Down:
			case ']':
				pidgin_conversation_window_select_next(window);
				return TRUE;
				break;

			case GDK_KEY_Page_Up:
			case GDK_KEY_KP_Page_Up:
			case '[':
				pidgin_conversation_window_select_previous(window);
				return TRUE;
				break;
		} /* End of switch */
	}

	/* If ALT (or whatever) was held down... */
	else if (state & GDK_MOD1_MASK) {
		if ('1' <= keyval && keyval <= '9') {
			guint switchto = keyval - '1';
			pidgin_conversation_window_select_nth(window, switchto);

			return TRUE;
		}
	}

	return FALSE;
}

/******************************************************************************
 * GObjectImplementation
 *****************************************************************************/
static void
pidgin_conversation_window_dispose(GObject *obj) {
	PidginConversationWindow *window = PIDGIN_CONVERSATION_WINDOW(obj);

	if(GTK_IS_TREE_MODEL(window->model)) {
		gtk_tree_model_foreach(GTK_TREE_MODEL(window->model),
		                       pidgin_conversation_window_foreach_destroy, window);
	}

	G_OBJECT_CLASS(pidgin_conversation_window_parent_class)->dispose(obj);
}

static void
pidgin_conversation_window_init(PidginConversationWindow *window) {
	GtkEventController *key = NULL;

	gtk_widget_init_template(GTK_WIDGET(window));

	gtk_window_set_application(GTK_WINDOW(window),
	                           GTK_APPLICATION(g_application_get_default()));

	key = gtk_event_controller_key_new(GTK_WIDGET(window));
	gtk_event_controller_set_propagation_phase(key, GTK_PHASE_CAPTURE);
	g_signal_connect(G_OBJECT(key), "key-pressed",
	                 G_CALLBACK(pidgin_conversation_window_key_pressed_cb),
	                 window);
	g_object_set_data_full(G_OBJECT(window), "key-press-controller", key,
	                       g_object_unref);
}

static void
pidgin_conversation_window_class_init(PidginConversationWindowClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	obj_class->dispose = pidgin_conversation_window_dispose;

	signals[SIG_CONVERSATION_SWITCHED] = g_signal_new_class_handler(
		"conversation-switched",
		G_OBJECT_CLASS_TYPE(obj_class),
		G_SIGNAL_RUN_LAST,
		NULL,
		NULL,
		NULL,
		NULL,
		G_TYPE_NONE,
		1,
		PURPLE_TYPE_CONVERSATION
	);

	gtk_widget_class_set_template_from_resource(
	    widget_class,
	    "/im/pidgin/Pidgin3/Conversations/window.ui"
	);

	gtk_widget_class_bind_template_child(widget_class, PidginConversationWindow,
	                                     vbox);

	gtk_widget_class_bind_template_child(widget_class, PidginConversationWindow,
	                                     model);
	gtk_widget_class_bind_template_child(widget_class, PidginConversationWindow,
	                                     view);

	gtk_widget_class_bind_template_child(widget_class, PidginConversationWindow,
	                                     stack);

	gtk_widget_class_bind_template_callback(widget_class,
	                                        pidgin_conversation_window_selection_changed);
}

/******************************************************************************
 * API
 *****************************************************************************/
GtkWidget *
pidgin_conversation_window_get_default(void) {
	if(!GTK_IS_WIDGET(default_window)) {
		default_window = pidgin_conversation_window_new();
		g_object_add_weak_pointer(G_OBJECT(default_window),
		                          (gpointer)&default_window);
	}

	return default_window;
}

GtkWidget *
pidgin_conversation_window_new(void) {
	return GTK_WIDGET(g_object_new(PIDGIN_TYPE_CONVERSATION_WINDOW, NULL));
}

void
pidgin_conversation_window_add(PidginConversationWindow *window,
                               PurpleConversation *conversation)
{
	PidginConversation *gtkconv = NULL;
	GtkTreeIter iter;
	const gchar *markup = NULL;

	g_return_if_fail(PIDGIN_IS_CONVERSATION_WINDOW(window));
	g_return_if_fail(PURPLE_IS_CONVERSATION(conversation));

	markup = purple_conversation_get_name(conversation);

	gtkconv = PIDGIN_CONVERSATION(conversation);
	if(gtkconv != NULL) {
		GtkWidget *parent = gtk_widget_get_parent(gtkconv->tab_cont);

		if(GTK_IS_WIDGET(parent)) {
			g_object_ref(gtkconv->tab_cont);
			gtk_container_remove(GTK_CONTAINER(parent), gtkconv->tab_cont);
		}

		gtk_stack_add_named(GTK_STACK(window->stack), gtkconv->tab_cont, markup);
		gtk_widget_show_all(gtkconv->tab_cont);

		if(GTK_IS_WIDGET(parent)) {
			g_object_unref(gtkconv->tab_cont);
		}
	}

	gtk_tree_store_prepend(window->model, &iter, NULL);
	gtk_tree_store_set(window->model, &iter,
	                   PIDGIN_CONVERSATION_WINDOW_COLUMN_OBJECT, conversation,
	                   PIDGIN_CONVERSATION_WINDOW_COLUMN_MARKUP, markup,
	                   -1);

	if(!gtk_widget_is_visible(GTK_WIDGET(window))) {
		gtk_widget_show_all(GTK_WIDGET(window));
	}
}

void
pidgin_conversation_window_remove(PidginConversationWindow *window,
                                  PurpleConversation *conversation)
{
	GtkTreeIter iter;
	GObject *obj = NULL;

	g_return_if_fail(PIDGIN_IS_CONVERSATION_WINDOW(window));
	g_return_if_fail(PURPLE_IS_CONVERSATION(conversation));

	if(!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(window->model), &iter)) {
		/* The tree is empty. */
		return;
	}

	do {
		gtk_tree_model_get(GTK_TREE_MODEL(window->model), &iter,
		                   PIDGIN_CONVERSATION_WINDOW_COLUMN_OBJECT, &obj,
		                   -1);

		if(PURPLE_CONVERSATION(obj) == conversation) {
			gtk_tree_store_remove(window->model, &iter);

			g_clear_object(&obj);

			break;
		}

		g_clear_object(&obj);
	} while(gtk_tree_model_iter_next(GTK_TREE_MODEL(window->model), &iter));
}

guint
pidgin_conversation_window_get_count(PidginConversationWindow *window) {
	GList *children = NULL;
	guint count = 0;

	g_return_val_if_fail(PIDGIN_IS_CONVERSATION_WINDOW(window), 0);

	children = gtk_container_get_children(GTK_CONTAINER(window));
	while(children != NULL) {
		children = g_list_delete_link(children, children);
		count++;
	}

	return count;
}

PurpleConversation *
pidgin_conversation_window_get_selected(PidginConversationWindow *window) {
	PurpleConversation *conversation = NULL;
	GtkTreeSelection *selection = NULL;
	GtkTreeIter iter;

	g_return_val_if_fail(PIDGIN_IS_CONVERSATION_WINDOW(window), NULL);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(window->view));
	if(gtk_tree_selection_get_selected(selection, NULL, &iter)) {

		gtk_tree_model_get(GTK_TREE_MODEL(window->model), &iter,
		                   PIDGIN_CONVERSATION_WINDOW_COLUMN_OBJECT, &conversation,
		                   -1);
	}

	return conversation;
}

void
pidgin_conversation_window_select(PidginConversationWindow *window,
                                  PurpleConversation *conversation)
{
	const gchar *name = NULL;

	g_return_if_fail(PIDGIN_IS_CONVERSATION_WINDOW(window));
	g_return_if_fail(PURPLE_IS_CONVERSATION(conversation));

	name = purple_conversation_get_name(conversation);
	gtk_stack_set_visible_child_name(GTK_STACK(window->stack), name);
}

void
pidgin_conversation_window_select_previous(PidginConversationWindow *window) {
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;
	GtkTreeSelection *selection = NULL;
	gboolean set = FALSE;

	g_return_if_fail(PIDGIN_IS_CONVERSATION_WINDOW(window));

	model = GTK_TREE_MODEL(window->model);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(window->view));
	if(gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		if(gtk_tree_model_iter_previous(model, &iter)) {
			gtk_tree_selection_select_iter(selection, &iter);
			set = TRUE;
		}
	}

	if(!set) {
		pidgin_conversation_window_select_last(window);
	}
}


void
pidgin_conversation_window_select_next(PidginConversationWindow *window) {
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;
	GtkTreeSelection *selection = NULL;
	gboolean set = FALSE;

	g_return_if_fail(PIDGIN_IS_CONVERSATION_WINDOW(window));

	model = GTK_TREE_MODEL(window->model);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(window->view));
	if(gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		if(gtk_tree_model_iter_next(model, &iter)) {
			gtk_tree_selection_select_iter(selection, &iter);
			set = TRUE;
		}
	}

	if(!set) {
		pidgin_conversation_window_select_first(window);
	}
}

void
pidgin_conversation_window_select_first(PidginConversationWindow *window) {
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;

	g_return_if_fail(PIDGIN_IS_CONVERSATION_WINDOW(window));

	model = GTK_TREE_MODEL(window->model);

	if(gtk_tree_model_get_iter_first(model, &iter)) {
		GtkTreeSelection *selection = NULL;

		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(window->view));
		gtk_tree_selection_select_iter(selection, &iter);
	}
}

void
pidgin_conversation_window_select_last(PidginConversationWindow *window) {
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;
	gint count = 0;

	g_return_if_fail(PIDGIN_IS_CONVERSATION_WINDOW(window));

	model = GTK_TREE_MODEL(window->model);
	count = gtk_tree_model_iter_n_children(model, NULL);

	if(gtk_tree_model_iter_nth_child(model, &iter, NULL, count - 1)) {
		GtkTreeSelection *selection = NULL;

		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(window->view));
		gtk_tree_selection_select_iter(selection, &iter);
	}
}

void
pidgin_conversation_window_select_nth(PidginConversationWindow *window,
                                      guint nth)
{
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;

	g_return_if_fail(PIDGIN_IS_CONVERSATION_WINDOW(window));

	model = GTK_TREE_MODEL(window->model);

	if(gtk_tree_model_iter_nth_child(model, &iter, NULL, nth)) {
		GtkTreeSelection *selection = NULL;

		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(window->view));
		gtk_tree_selection_select_iter(selection, &iter);
	}
}

gboolean
pidgin_conversation_window_conversation_is_selected(PidginConversationWindow *window,
                                                    PurpleConversation *conversation)
{
	const gchar *name = NULL, *visible = NULL;

	g_return_val_if_fail(PIDGIN_IS_CONVERSATION_WINDOW(window), FALSE);
	g_return_val_if_fail(PURPLE_IS_CONVERSATION(conversation), FALSE);

	name = purple_conversation_get_name(conversation);
	visible = gtk_stack_get_visible_child_name(GTK_STACK(window->stack));

	return purple_strequal(name, visible);
}
