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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#include <math.h>

#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <purple.h>

#include "pidginprefs.h"
#include "pidginprefsinternal.h"
#include "gtkutils.h"
#include "pidgincore.h"
#ifdef USE_VV
#include "pidginvvprefs.h"
#endif

struct _PidginPrefsWindow {
	GtkDialog parent;

	/* Stack */
	GtkWidget *stack;
};

/* Main dialog */
static PidginPrefsWindow *prefs = NULL;

G_DEFINE_TYPE(PidginPrefsWindow, pidgin_prefs_window, GTK_TYPE_DIALOG);

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
update_spin_value(GtkWidget *w, GtkWidget *spin)
{
	const char *key = g_object_get_data(G_OBJECT(spin), "val");
	int value;

	value = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin));

	purple_prefs_set_int(key, value);
}

GtkWidget *
pidgin_prefs_labeled_spin_button(GtkWidget *box, const gchar *title,
		const char *key, int min, int max, GtkSizeGroup *sg)
{
	GtkWidget *spin;
	GtkAdjustment *adjust;
	int val;

	val = purple_prefs_get_int(key);

	adjust = GTK_ADJUSTMENT(gtk_adjustment_new(val, min, max, 1, 1, 0));
	spin = gtk_spin_button_new(adjust, 1, 0);
	g_object_set_data(G_OBJECT(spin), "val", (char *)key);
	if (max < 10000)
		gtk_widget_set_size_request(spin, 50, -1);
	else
		gtk_widget_set_size_request(spin, 60, -1);
	g_signal_connect(G_OBJECT(adjust), "value-changed",
					 G_CALLBACK(update_spin_value), GTK_WIDGET(spin));
	gtk_widget_show(spin);

	return pidgin_add_widget_to_vbox(GTK_BOX(box), title, sg, spin, FALSE, NULL);
}

void
pidgin_prefs_bind_spin_button(const char *key, GtkWidget *spin)
{
	GtkAdjustment *adjust;
	int val;

	val = purple_prefs_get_int(key);

	adjust = gtk_spin_button_get_adjustment(GTK_SPIN_BUTTON(spin));
	gtk_adjustment_set_value(adjust, val);
	g_object_set_data(G_OBJECT(spin), "val", (char *)key);
	g_signal_connect(G_OBJECT(adjust), "value-changed",
			G_CALLBACK(update_spin_value), GTK_WIDGET(spin));
}

static void
entry_set(GtkEntry *entry, gpointer data)
{
	const char *key = (const char*)data;

	purple_prefs_set_string(key, gtk_entry_get_text(entry));
}

void
pidgin_prefs_bind_entry(const char *key, GtkWidget *entry)
{
	const gchar *value;

	value = purple_prefs_get_string(key);

	gtk_entry_set_text(GTK_ENTRY(entry), value);
	g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(entry_set),
			(char*)key);
}

static void
dropdown_set(GtkComboBox *combo_box, G_GNUC_UNUSED gpointer data)
{
	GtkTreeIter iter;
	GtkTreeModel *tree_model;
	PurplePrefType type;
	const char *key;

	tree_model = gtk_combo_box_get_model(combo_box);
	if (!gtk_combo_box_get_active_iter(combo_box, &iter)) {
		return;
	}

	type = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(combo_box), "type"));
	key = g_object_get_data(G_OBJECT(combo_box), "key");
	if (type == PURPLE_PREF_INT) {
		gint value;
		gtk_tree_model_get(tree_model, &iter, PIDGIN_PREF_COMBO_VALUE, &value, -1);
		purple_prefs_set_int(key, value);
	} else if (type == PURPLE_PREF_STRING) {
		gchar *value;
		gtk_tree_model_get(tree_model, &iter, PIDGIN_PREF_COMBO_VALUE, &value, -1);
		purple_prefs_set_string(key, value);
		g_free(value);
	} else if (type == PURPLE_PREF_BOOLEAN) {
		gboolean value;
		gtk_tree_model_get(tree_model, &iter, PIDGIN_PREF_COMBO_VALUE, &value, -1);
		purple_prefs_set_bool(key, value);
	} else {
		g_return_if_reached();
	}
}

GtkWidget *
pidgin_prefs_dropdown_from_list(GtkWidget *box, const gchar *title,
		PurplePrefType type, const char *key, GList *menuitems)
{
	GtkWidget *dropdown = NULL;
	GtkWidget *label = NULL;
	GtkListStore *store = NULL;
	GtkTreeIter active;
	GtkCellRenderer *renderer;
	gint pref_int_value = 0;
	const gchar *pref_str_value = NULL;
	gboolean pref_bool_value = FALSE;

	g_return_val_if_fail(menuitems != NULL, NULL);

	if (type == PURPLE_PREF_INT) {
		pref_int_value = purple_prefs_get_int(key);
		store = gtk_list_store_new(PIDGIN_PREF_COMBO_N_COLUMNS, G_TYPE_STRING,
		                           G_TYPE_INT);
	} else if (type == PURPLE_PREF_STRING) {
		pref_str_value = purple_prefs_get_string(key);
		store = gtk_list_store_new(PIDGIN_PREF_COMBO_N_COLUMNS, G_TYPE_STRING,
		                           G_TYPE_STRING);
	} else if (type == PURPLE_PREF_BOOLEAN) {
		pref_bool_value = purple_prefs_get_bool(key);
		store = gtk_list_store_new(PIDGIN_PREF_COMBO_N_COLUMNS, G_TYPE_STRING,
		                           G_TYPE_BOOLEAN);
	} else {
		g_return_val_if_reached(NULL);
	}

	dropdown = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
	g_object_set_data(G_OBJECT(dropdown), "type", GINT_TO_POINTER(type));

	for (; menuitems != NULL; menuitems = g_list_next(menuitems)) {
		const PurpleKeyValuePair *menu_item = menuitems->data;
		GtkTreeIter iter;

		if (menu_item->key == NULL) {
			break;
		}

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
		                   PIDGIN_PREF_COMBO_TEXT, menu_item->key,
		                   -1);

		if (type == PURPLE_PREF_INT) {
			gint value = GPOINTER_TO_INT(menu_item->value);
			gtk_list_store_set(store, &iter, PIDGIN_PREF_COMBO_VALUE, value, -1);
			if (pref_int_value == value) {
				active = iter;
			}

		} else if (type == PURPLE_PREF_STRING) {
			const gchar *value = (const gchar *)menu_item->value;
			gtk_list_store_set(store, &iter, PIDGIN_PREF_COMBO_VALUE, value, -1);
			if (purple_strequal(pref_str_value, value)) {
				active = iter;
			}

		} else if (type == PURPLE_PREF_BOOLEAN) {
			gboolean value = (gboolean)GPOINTER_TO_INT(menu_item->value);
			gtk_list_store_set(store, &iter, PIDGIN_PREF_COMBO_VALUE, value, -1);
			if (pref_bool_value == value) {
				active = iter;
			}
		}
	}

	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(dropdown), renderer, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(dropdown), renderer,
	                               "text", 0,
	                               NULL);

	gtk_combo_box_set_active_iter(GTK_COMBO_BOX(dropdown), &active);

	g_signal_connect(dropdown, "changed", G_CALLBACK(dropdown_set), NULL);

	pidgin_add_widget_to_vbox(GTK_BOX(box), title, NULL, dropdown, FALSE, &label);

	g_object_set_data(G_OBJECT(dropdown), "key", (gpointer)key);

	return label;
}

GtkWidget *
pidgin_prefs_dropdown(GtkWidget *box, const gchar *title, PurplePrefType type,
			   const char *key, ...)
{
	va_list ap;
	GList *menuitems = NULL;
	GtkWidget *dropdown = NULL;
	char *name;

	g_return_val_if_fail(type == PURPLE_PREF_BOOLEAN || type == PURPLE_PREF_INT ||
			type == PURPLE_PREF_STRING, NULL);

	va_start(ap, key);
	while ((name = va_arg(ap, char *)) != NULL) {
		PurpleKeyValuePair *kvp;

		if (type == PURPLE_PREF_INT || type == PURPLE_PREF_BOOLEAN) {
			kvp = purple_key_value_pair_new(name, GINT_TO_POINTER(va_arg(ap, int)));
		} else {
			kvp = purple_key_value_pair_new(name, va_arg(ap, char *));
		}
		menuitems = g_list_prepend(menuitems, kvp);
	}
	va_end(ap);

	g_return_val_if_fail(menuitems != NULL, NULL);

	menuitems = g_list_reverse(menuitems);

	dropdown = pidgin_prefs_dropdown_from_list(box, title, type, key,
			menuitems);

	g_list_free_full(menuitems, (GDestroyNotify)purple_key_value_pair_free);

	return dropdown;
}

static void
bind_dropdown_set(GtkComboBox *combo_box, gpointer data)
{
	PidginPrefCombo *combo = data;
	GtkTreeIter iter;
	GtkTreeModel *tree_model;

	tree_model = gtk_combo_box_get_model(combo_box);
	if (!gtk_combo_box_get_active_iter(combo_box, &iter))
		return;

	if (combo->type == PURPLE_PREF_INT) {
		gint value;
		gtk_tree_model_get(tree_model, &iter, PIDGIN_PREF_COMBO_VALUE, &value, -1);
		purple_prefs_set_int(combo->key, value);
	} else if (combo->type == PURPLE_PREF_STRING) {
		gchar *value;
		gtk_tree_model_get(tree_model, &iter, PIDGIN_PREF_COMBO_VALUE, &value, -1);
		purple_prefs_set_string(combo->key, value);
		g_free(value);
	} else if (combo->type == PURPLE_PREF_BOOLEAN) {
		gboolean value;
		gtk_tree_model_get(tree_model, &iter, PIDGIN_PREF_COMBO_VALUE, &value, -1);
		purple_prefs_set_bool(combo->key, value);
	} else {
		g_return_if_reached();
	}
}

void
pidgin_prefs_bind_dropdown(PidginPrefCombo *combo)
{
	GtkTreeModel *store = NULL;
	GtkTreeIter iter;
	GtkTreeIter active;
	int pref_int_value = 0;
	const char *pref_str_value = NULL;
	gboolean pref_bool_value = FALSE;

	if (combo->type == PURPLE_PREF_INT) {
		pref_int_value = purple_prefs_get_int(combo->key);
	} else if (combo->type == PURPLE_PREF_STRING) {
		pref_str_value = purple_prefs_get_string(combo->key);
	} else if (combo->type == PURPLE_PREF_BOOLEAN) {
		pref_bool_value = purple_prefs_get_bool(combo->key);
	} else {
		g_return_if_reached();
	}

	store = gtk_combo_box_get_model(GTK_COMBO_BOX(combo->combo));

	if (!gtk_tree_model_get_iter_first(store, &iter)) {
		g_return_if_reached();
	}

	do {
		if (combo->type == PURPLE_PREF_INT) {
			gint value = 0;
			gtk_tree_model_get(store, &iter, PIDGIN_PREF_COMBO_VALUE, &value, -1);
			if (pref_int_value == value) {
				active = iter;
				break;
			}

		} else if (combo->type == PURPLE_PREF_STRING) {
			gchar *value = NULL;
			gtk_tree_model_get(store, &iter, PIDGIN_PREF_COMBO_VALUE, &value, -1);
			if (purple_strequal(pref_str_value, value)) {
				active = iter;
				g_free(value);
				break;
			}
			g_free(value);

		} else if (combo->type == PURPLE_PREF_BOOLEAN) {
			gboolean value = FALSE;
			gtk_tree_model_get(store, &iter, PIDGIN_PREF_COMBO_VALUE, &value, -1);
			if (pref_bool_value == value) {
				active = iter;
				break;
			}
		}
	} while (gtk_tree_model_iter_next(store, &iter));

	gtk_combo_box_set_active_iter(GTK_COMBO_BOX(combo->combo), &active);

	g_signal_connect(G_OBJECT(combo->combo), "changed",
			G_CALLBACK(bind_dropdown_set), combo);
}

static void
set_bool_pref(GtkWidget *w, const char *key)
{
	purple_prefs_set_bool(key,
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)));
}

GtkWidget *
pidgin_prefs_checkbox(const char *text, const char *key, GtkWidget *page)
{
	GtkWidget *button;

	button = gtk_check_button_new_with_mnemonic(text);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
			purple_prefs_get_bool(key));

	gtk_box_pack_start(GTK_BOX(page), button, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(button), "clicked",
			G_CALLBACK(set_bool_pref), (char *)key);

	gtk_widget_show(button);

	return button;
}

void
pidgin_prefs_bind_checkbox(const char *key, GtkWidget *button)
{
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button),
			purple_prefs_get_bool(key));
	g_signal_connect(G_OBJECT(button), "toggled",
			G_CALLBACK(set_bool_pref), (char *)key);
}

static void
delete_prefs(GtkWidget *asdf, void *gdsa)
{
	/* Close any request dialogs */
	purple_request_close_with_handle(prefs);
	purple_notify_close_with_handle(prefs);

	/* Unregister callbacks. */
	purple_prefs_disconnect_by_handle(prefs);

	prefs = NULL;
}

#ifdef USE_VV
static void
vv_test_switch_page_cb(GtkStack *stack, G_GNUC_UNUSED GParamSpec *pspec,
                       gpointer data)
{
	PidginVVPrefs *vv_prefs = data;

	if (!g_str_equal(gtk_stack_get_visible_child_name(stack), "vv")) {
		/* Disable any running test pipelines. */
		pidgin_vv_prefs_disable_test_pipelines(vv_prefs);
	}
}
#endif

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
static void
pidgin_prefs_window_class_init(PidginPrefsWindowClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(
		widget_class,
		"/im/pidgin/Pidgin3/Prefs/prefs.ui"
	);

	/* Main window */
	gtk_widget_class_bind_template_child(widget_class, PidginPrefsWindow,
	                                     stack);
	gtk_widget_class_bind_template_callback(widget_class, delete_prefs);
}

static void
pidgin_prefs_window_init(PidginPrefsWindow *win)
{
#ifdef USE_VV
	GtkWidget *vv = NULL;
#endif

	gtk_widget_init_template(GTK_WIDGET(win));

#ifdef USE_VV
	vv = pidgin_vv_prefs_new();
	gtk_stack_add_titled(GTK_STACK(win->stack), vv, "vv", _("Voice/Video"));
	g_signal_connect(win->stack, "notify::visible-child",
	                 G_CALLBACK(vv_test_switch_page_cb), vv);
#endif
}

/******************************************************************************
 * API
 *****************************************************************************/
void
pidgin_prefs_show(void)
{
	if (prefs == NULL) {
		prefs = PIDGIN_PREFS_WINDOW(g_object_new(
					pidgin_prefs_window_get_type(), NULL));
	}

	gtk_window_present(GTK_WINDOW(prefs));
}

void
pidgin_prefs_init(void)
{
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "");
	purple_prefs_add_none("/plugins/gtk");

	/* Plugins */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/plugins");
	purple_prefs_add_path_list(PIDGIN_PREFS_ROOT "/plugins/loaded", NULL);

	/* File locations */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/filelocations");
	purple_prefs_add_path(PIDGIN_PREFS_ROOT "/filelocations/last_save_folder", "");
	purple_prefs_add_path(PIDGIN_PREFS_ROOT "/filelocations/last_open_folder", "");
	purple_prefs_add_path(PIDGIN_PREFS_ROOT "/filelocations/last_icon_folder", "");

#ifdef USE_VV
	/* Voice/Video */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/vvconfig");
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/vvconfig/audio");
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/vvconfig/audio/src");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/vvconfig/audio/src/device", "");
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/vvconfig/audio/sink");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/vvconfig/audio/sink/device", "");
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/vvconfig/video");
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/vvconfig/video/src");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/vvconfig/video/src/device", "");
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/vvconfig/video");
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/vvconfig/video/sink");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/vvconfig/video/sink/device", "");
#endif

	pidgin_prefs_update_old();
}

void
pidgin_prefs_update_old(void)
{
	const gchar *video_sink = NULL;

	/* Rename some old prefs */
	purple_prefs_rename(PIDGIN_PREFS_ROOT "/conversations/im/raise_on_events", "/plugins/gtk/X11/notify/method_raise");

	purple_prefs_rename_boolean_toggle(PIDGIN_PREFS_ROOT "/conversations/ignore_colors",
									 PIDGIN_PREFS_ROOT "/conversations/show_incoming_formatting");

	/* Remove some no-longer-used prefs */
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/auto_expand_contacts");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/button_style");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/grey_idle_buddies");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/raise_on_events");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/show_group_count");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/show_warning_level");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/tooltip_delay");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/x");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/y");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/browsers");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/browsers/browser");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/browsers/command");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/browsers/place");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/browsers/manual_command");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/button_type");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/ctrl_enter_sends");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/enter_sends");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/escape_closes");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/html_shortcuts");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/icons_on_tabs");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/send_formatting");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/show_urls_as_links");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/use_custom_bgcolor");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/use_custom_fgcolor");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/use_custom_font");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/use_custom_size");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/chat/old_tab_complete");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/chat/tab_completion");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/im/hide_on_send");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/chat/color_nicks");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/chat/raise_on_events");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/ignore_fonts");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/ignore_font_sizes");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/passthrough_unknown_commands");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/debug/timestamps");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/idle");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/signon");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/silent_signon");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/command");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/conv_focus");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/enabled/chat_msg_recv");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/enabled/first_im_recv");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/enabled/got_attention");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/enabled/im_recv");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/enabled/join_chat");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/enabled/left_chat");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/enabled/login");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/enabled/logout");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/enabled/nick_said");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/enabled/pounce_default");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/enabled/send_chat_msg");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/enabled/send_im");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/enabled/sent_attention");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/enabled");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/file/chat_msg_recv");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/file/first_im_recv");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/file/got_attention");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/file/im_recv");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/file/join_chat");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/file/left_chat");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/file/login");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/file/logout");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/file/nick_said");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/file/pounce_default");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/file/send_chat_msg");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/file/send_im");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/file/sent_attention");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/file");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/method");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/mute");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound/theme");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/sound");

	purple_prefs_remove(PIDGIN_PREFS_ROOT "/away/queue_messages");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/away");
	purple_prefs_remove("/plugins/gtk/docklet/queue_messages");

	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/chat/default_width");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/chat/default_height");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/im/default_width");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/conversations/im/default_height");
	purple_prefs_rename(PIDGIN_PREFS_ROOT "/conversations/x",
			PIDGIN_PREFS_ROOT "/conversations/im/x");
	purple_prefs_rename(PIDGIN_PREFS_ROOT "/conversations/y",
			PIDGIN_PREFS_ROOT "/conversations/im/y");

	/* Fixup vvconfig plugin prefs */
	if (purple_prefs_exists("/plugins/core/vvconfig/audio/src/device")) {
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/vvconfig/audio/src/device",
				purple_prefs_get_string("/plugins/core/vvconfig/audio/src/device"));
	}
	if (purple_prefs_exists("/plugins/core/vvconfig/audio/sink/device")) {
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/vvconfig/audio/sink/device",
				purple_prefs_get_string("/plugins/core/vvconfig/audio/sink/device"));
	}
	if (purple_prefs_exists("/plugins/core/vvconfig/video/src/device")) {
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/vvconfig/video/src/device",
				purple_prefs_get_string("/plugins/core/vvconfig/video/src/device"));
	}
	if (purple_prefs_exists("/plugins/gtk/vvconfig/video/sink/device")) {
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/vvconfig/video/sink/device",
				purple_prefs_get_string("/plugins/gtk/vvconfig/video/sink/device"));
	}

	video_sink = purple_prefs_get_string(PIDGIN_PREFS_ROOT "/vvconfig/video/sink/device");
	if (purple_strequal(video_sink, "glimagesink") || purple_strequal(video_sink, "directdrawsink")) {
		/* Accelerated sinks move to GTK GL. */
		/* video_sink = "gtkglsink"; */
		/* FIXME: I haven't been able to get gtkglsink to work yet: */
		video_sink = "gtksink";
	} else {
		/* Everything else, including default will be moved to GTK sink. */
		video_sink = "gtksink";
	}
	purple_prefs_set_string(PIDGIN_PREFS_ROOT "/vvconfig/video/sink/device", video_sink);

	purple_prefs_remove("/plugins/core/vvconfig");
	purple_prefs_remove("/plugins/gtk/vvconfig");

	purple_prefs_remove(PIDGIN_PREFS_ROOT "/vvconfig/audio/src/plugin");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/vvconfig/audio/sink/plugin");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/vvconfig/video/src/plugin");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/vvconfig/video/sink/plugin");
}

