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
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#include <math.h>

#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>
#include <nice.h>
#include <talkatu.h>

#include <purple.h>

#include "gtkblist.h"
#include "gtkconv.h"
#include "gtkdialogs.h"
#include "gtkutils.h"
#include "pidgincore.h"
#include "pidgindebug.h"
#include "pidginprefs.h"
#include "pidginprefsinternal.h"
#include <libsoup/soup.h>

#define PREFS_OPTIMAL_ICON_SIZE 32

/* 25MB */
#define PREFS_MAX_DOWNLOADED_THEME_SIZE 26214400

struct _PidginPrefsWindow {
	GtkDialog parent;

	/* Stack */
	GtkWidget *stack;

#ifdef USE_VV
	/* Voice/Video page */
	struct {
		struct {
			PidginPrefCombo input;
			PidginPrefCombo output;
			GtkWidget *level;
			GtkWidget *threshold;
			GtkWidget *volume;
			GtkWidget *test;
			GstElement *pipeline;
		} voice;

		struct {
			PidginPrefCombo input;
			PidginPrefCombo output;
			GtkWidget *frame;
			GtkWidget *sink_widget;
			GtkWidget *test;
			GstElement *pipeline;
		} video;
	} vv;
#endif
};

/* Main dialog */
static PidginPrefsWindow *prefs = NULL;

/*
 * PROTOTYPES
 */
G_DEFINE_TYPE(PidginPrefsWindow, pidgin_prefs_window, GTK_TYPE_DIALOG);
static void delete_prefs(GtkWidget *, void *);

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

GtkWidget *
pidgin_prefs_labeled_entry(GtkWidget *page, const gchar *title,
							 const char *key, GtkSizeGroup *sg)
{
	GtkWidget *entry;
	const gchar *value;

	value = purple_prefs_get_string(key);

	entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(entry), value);
	g_signal_connect(G_OBJECT(entry), "changed",
					 G_CALLBACK(entry_set), (char*)key);
	gtk_widget_show(entry);

	return pidgin_add_widget_to_vbox(GTK_BOX(page), title, sg, entry, TRUE, NULL);
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

GtkWidget *
pidgin_prefs_labeled_password(GtkWidget *page, const gchar *title,
							 const char *key, GtkSizeGroup *sg)
{
	GtkWidget *entry;
	const gchar *value;

	value = purple_prefs_get_string(key);

	entry = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
	gtk_entry_set_text(GTK_ENTRY(entry), value);
	g_signal_connect(G_OBJECT(entry), "changed",
					 G_CALLBACK(entry_set), (char*)key);
	gtk_widget_show(entry);

	return pidgin_add_widget_to_vbox(GTK_BOX(page), title, sg, entry, TRUE, NULL);
}

/* TODO: Maybe move this up somewheres... */
enum {
	PREF_DROPDOWN_TEXT,
	PREF_DROPDOWN_VALUE,
	PREF_DROPDOWN_COUNT
};

typedef struct
{
	PurplePrefType type;
	union {
		const char *string;
		int integer;
		gboolean boolean;
	} value;
} PidginPrefValue;

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
		gtk_tree_model_get(tree_model, &iter, PREF_DROPDOWN_VALUE, &value, -1);
		purple_prefs_set_int(key, value);
	} else if (type == PURPLE_PREF_STRING) {
		const char *value;
		gtk_tree_model_get(tree_model, &iter, PREF_DROPDOWN_VALUE, &value, -1);
		purple_prefs_set_string(key, value);
	} else if (type == PURPLE_PREF_BOOLEAN) {
		gboolean value;
		gtk_tree_model_get(tree_model, &iter, PREF_DROPDOWN_VALUE, &value, -1);
		purple_prefs_set_bool(key, value);
	} else {
		g_return_if_reached();
	}
}

static GtkWidget *
pidgin_prefs_dropdown_from_list_with_cb(GtkWidget *box, const gchar *title,
	GtkComboBox **dropdown_out, GList *menuitems,
	PidginPrefValue initial)
{
	GtkWidget *dropdown;
	GtkWidget *label = NULL;
	GtkListStore *store = NULL;
	GtkTreeIter iter;
	GtkTreeIter active;
	GtkCellRenderer *renderer;

	g_return_val_if_fail(menuitems != NULL, NULL);

	if (initial.type == PURPLE_PREF_INT) {
		store = gtk_list_store_new(PREF_DROPDOWN_COUNT, G_TYPE_STRING, G_TYPE_INT);
	} else if (initial.type == PURPLE_PREF_STRING) {
		store = gtk_list_store_new(PREF_DROPDOWN_COUNT, G_TYPE_STRING, G_TYPE_STRING);
	} else if (initial.type == PURPLE_PREF_BOOLEAN) {
		store = gtk_list_store_new(PREF_DROPDOWN_COUNT, G_TYPE_STRING, G_TYPE_BOOLEAN);
	} else {
		g_warn_if_reached();
		return NULL;
	}

	dropdown = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
	if (dropdown_out != NULL)
		*dropdown_out = GTK_COMBO_BOX(dropdown);
	g_object_set_data(G_OBJECT(dropdown), "type", GINT_TO_POINTER(initial.type));

	for (; menuitems != NULL; menuitems = g_list_next(menuitems)) {
		const PurpleKeyValuePair *menu_item = menuitems->data;
		int int_value  = 0;
		const char *str_value  = NULL;
		gboolean bool_value = FALSE;

		if (menu_item->key == NULL) {
			break;
		}

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				   PREF_DROPDOWN_TEXT, menu_item->key,
		                   -1);

		if (initial.type == PURPLE_PREF_INT) {
			int_value = GPOINTER_TO_INT(menu_item->value);
			gtk_list_store_set(store, &iter,
			                   PREF_DROPDOWN_VALUE, int_value,
			                   -1);
		}
		else if (initial.type == PURPLE_PREF_STRING) {
			str_value = (const char *)menu_item->value;
			gtk_list_store_set(store, &iter,
			                   PREF_DROPDOWN_VALUE, str_value,
			                   -1);
		}
		else if (initial.type == PURPLE_PREF_BOOLEAN) {
			bool_value = (gboolean)GPOINTER_TO_INT(menu_item->value);
			gtk_list_store_set(store, &iter,
			                   PREF_DROPDOWN_VALUE, bool_value,
			                   -1);
		}

		if ((initial.type == PURPLE_PREF_INT &&
			initial.value.integer == int_value) ||
			(initial.type == PURPLE_PREF_STRING &&
			purple_strequal(initial.value.string, str_value)) ||
			(initial.type == PURPLE_PREF_BOOLEAN &&
			(initial.value.boolean == bool_value))) {

			active = iter;
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

	return label;
}

GtkWidget *
pidgin_prefs_dropdown_from_list(GtkWidget *box, const gchar *title,
		PurplePrefType type, const char *key, GList *menuitems)
{
	PidginPrefValue initial;
	GtkComboBox *dropdown = NULL;
	GtkWidget *label;

	initial.type = type;
	if (type == PURPLE_PREF_INT) {
		initial.value.integer = purple_prefs_get_int(key);
	} else if (type == PURPLE_PREF_STRING) {
		initial.value.string = purple_prefs_get_string(key);
	} else if (type == PURPLE_PREF_BOOLEAN) {
		initial.value.boolean = purple_prefs_get_bool(key);
	} else {
		g_return_val_if_reached(NULL);
	}

	label = pidgin_prefs_dropdown_from_list_with_cb(box, title, &dropdown,
	                                                menuitems, initial);

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
		gtk_tree_model_get(tree_model, &iter, PREF_DROPDOWN_VALUE,
			&combo->value.integer, -1);
		purple_prefs_set_int(combo->key, combo->value.integer);
	} else if (combo->type == PURPLE_PREF_STRING) {
		gtk_tree_model_get(tree_model, &iter, PREF_DROPDOWN_VALUE,
			&combo->value.string, -1);
		purple_prefs_set_string(combo->key, combo->value.string);
	} else if (combo->type == PURPLE_PREF_BOOLEAN) {
		gtk_tree_model_get(tree_model, &iter, PREF_DROPDOWN_VALUE,
			&combo->value.boolean, -1);
		purple_prefs_set_bool(combo->key, combo->value.boolean);
	} else {
		g_return_if_reached();
	}
}

void
pidgin_prefs_bind_dropdown_from_list(PidginPrefCombo *combo, GList *menuitems)
{
	gchar *text;
	GtkListStore *store = NULL;
	GtkTreeIter iter;
	GtkTreeIter active;

	g_return_if_fail(menuitems != NULL);

	if (combo->type == PURPLE_PREF_INT) {
		combo->value.integer = purple_prefs_get_int(combo->key);
	} else if (combo->type == PURPLE_PREF_STRING) {
		combo->value.string = purple_prefs_get_string(combo->key);
	} else if (combo->type == PURPLE_PREF_BOOLEAN) {
		combo->value.boolean = purple_prefs_get_bool(combo->key);
	} else {
		g_return_if_reached();
	}

	store = GTK_LIST_STORE(
			gtk_combo_box_get_model(GTK_COMBO_BOX(combo->combo)));

	while (menuitems != NULL && (text = (char *)menuitems->data) != NULL) {
		int int_value = 0;
		const char *str_value = NULL;
		gboolean bool_value = FALSE;

		menuitems = g_list_next(menuitems);
		g_return_if_fail(menuitems != NULL);

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
		                   PREF_DROPDOWN_TEXT, text,
		                   -1);

		if (combo->type == PURPLE_PREF_INT) {
			int_value = GPOINTER_TO_INT(menuitems->data);
			gtk_list_store_set(store, &iter,
			                   PREF_DROPDOWN_VALUE, int_value,
			                   -1);
		}
		else if (combo->type == PURPLE_PREF_STRING) {
			str_value = (const char *)menuitems->data;
			gtk_list_store_set(store, &iter,
			                   PREF_DROPDOWN_VALUE, str_value,
			                   -1);
		}
		else if (combo->type == PURPLE_PREF_BOOLEAN) {
			bool_value = (gboolean)GPOINTER_TO_INT(menuitems->data);
			gtk_list_store_set(store, &iter,
			                   PREF_DROPDOWN_VALUE, bool_value,
			                   -1);
		}

		if ((combo->type == PURPLE_PREF_INT &&
			combo->value.integer == int_value) ||
			(combo->type == PURPLE_PREF_STRING &&
			purple_strequal(combo->value.string, str_value)) ||
			(combo->type == PURPLE_PREF_BOOLEAN &&
			(combo->value.boolean == bool_value))) {

			active = iter;
		}

		menuitems = g_list_next(menuitems);
	}

	gtk_combo_box_set_active_iter(GTK_COMBO_BOX(combo->combo), &active);

	g_signal_connect(G_OBJECT(combo->combo), "changed",
			G_CALLBACK(bind_dropdown_set), combo);
}

void
pidgin_prefs_bind_dropdown(PidginPrefCombo *combo)
{
	GtkTreeModel *store = NULL;
	GtkTreeIter iter;
	GtkTreeIter active;

	if (combo->type == PURPLE_PREF_INT) {
		combo->value.integer = purple_prefs_get_int(combo->key);
	} else if (combo->type == PURPLE_PREF_STRING) {
		combo->value.string = purple_prefs_get_string(combo->key);
	} else if (combo->type == PURPLE_PREF_BOOLEAN) {
		combo->value.boolean = purple_prefs_get_bool(combo->key);
	} else {
		g_return_if_reached();
	}

	store = gtk_combo_box_get_model(GTK_COMBO_BOX(combo->combo));

	if (!gtk_tree_model_get_iter_first(store, &iter)) {
		g_return_if_reached();
	}

	do {
		int int_value = 0;
		const char *str_value = NULL;
		gboolean bool_value = FALSE;

		if (combo->type == PURPLE_PREF_INT) {
			gtk_tree_model_get(store, &iter,
			                   PREF_DROPDOWN_VALUE, &int_value,
			                   -1);
			if (combo->value.integer == int_value) {
				active = iter;
				break;
			}
		}
		else if (combo->type == PURPLE_PREF_STRING) {
			gtk_tree_model_get(store, &iter,
			                   PREF_DROPDOWN_VALUE, &str_value,
			                   -1);
			if (purple_strequal(combo->value.string, str_value)) {
				active = iter;
				break;
			}
		}
		else if (combo->type == PURPLE_PREF_BOOLEAN) {
			gtk_tree_model_get(store, &iter,
			                   PREF_DROPDOWN_VALUE, &bool_value,
			                   -1);
			if (combo->value.boolean == bool_value) {
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
static GList *
get_vv_device_menuitems(PurpleMediaElementType type)
{
	GList *result = NULL;
	GList *i;

	i = purple_media_manager_enumerate_elements(purple_media_manager_get(),
			type);
	for (; i; i = g_list_delete_link(i, i)) {
		PurpleMediaElementInfo *info = i->data;

		result = g_list_append(result,
				purple_media_element_info_get_name(info));
		result = g_list_append(result,
				purple_media_element_info_get_id(info));
		g_object_unref(info);
	}

	return result;
}

static GstElement *
create_test_element(PurpleMediaElementType type)
{
	PurpleMediaElementInfo *element_info;

	element_info = purple_media_manager_get_active_element(purple_media_manager_get(), type);

	g_return_val_if_fail(element_info, NULL);

	return purple_media_element_info_call_create(element_info,
		NULL, NULL, NULL);
}

static void
vv_test_switch_page_cb(GtkStack *stack, G_GNUC_UNUSED GParamSpec *pspec,
                       gpointer data)
{
	PidginPrefsWindow *win = data;

	if (!g_str_equal(gtk_stack_get_visible_child_name(stack), "vv")) {
		/* Disable any running test pipelines. */
		gtk_toggle_button_set_active(
		        GTK_TOGGLE_BUTTON(win->vv.voice.test), FALSE);
		gtk_toggle_button_set_active(
		        GTK_TOGGLE_BUTTON(win->vv.video.test), FALSE);
	}
}

static GstElement *
create_voice_pipeline(void)
{
	GstElement *pipeline;
	GstElement *src, *sink;
	GstElement *volume;
	GstElement *level;
	GstElement *valve;

	pipeline = gst_pipeline_new("voicetest");

	src = create_test_element(PURPLE_MEDIA_ELEMENT_AUDIO | PURPLE_MEDIA_ELEMENT_SRC);
	sink = create_test_element(PURPLE_MEDIA_ELEMENT_AUDIO | PURPLE_MEDIA_ELEMENT_SINK);
	volume = gst_element_factory_make("volume", "volume");
	level = gst_element_factory_make("level", "level");
	valve = gst_element_factory_make("valve", "valve");

	gst_bin_add_many(GST_BIN(pipeline), src, volume, level, valve, sink, NULL);
	gst_element_link_many(src, volume, level, valve, sink, NULL);

	purple_debug_info("gtkprefs", "create_voice_pipeline: setting pipeline "
		"state to GST_STATE_PLAYING - it may hang here on win32\n");
	gst_element_set_state(GST_ELEMENT(pipeline), GST_STATE_PLAYING);
	purple_debug_info("gtkprefs", "create_voice_pipeline: state is set\n");

	return pipeline;
}

static void
on_volume_change_cb(GtkWidget *w, gdouble value, gpointer data)
{
	PidginPrefsWindow *win = PIDGIN_PREFS_WINDOW(data);
	GstElement *volume;

	if (!win->vv.voice.pipeline) {
		return;
	}

	volume = gst_bin_get_by_name(GST_BIN(win->vv.voice.pipeline), "volume");
	g_object_set(volume, "volume",
	             gtk_scale_button_get_value(GTK_SCALE_BUTTON(w)) / 100.0, NULL);
}

static gdouble
gst_msg_db_to_percent(GstMessage *msg, gchar *value_name)
{
	const GValue *list;
	const GValue *value;
	gdouble value_db;
	gdouble percent;

	list = gst_structure_get_value(gst_message_get_structure(msg), value_name);
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	value = g_value_array_get_nth(g_value_get_boxed(list), 0);
G_GNUC_END_IGNORE_DEPRECATIONS
	value_db = g_value_get_double(value);
	percent = pow(10, value_db / 20);
	return (percent > 1.0) ? 1.0 : percent;
}

static gboolean
gst_bus_cb(GstBus *bus, GstMessage *msg, gpointer data)
{
	PidginPrefsWindow *win = PIDGIN_PREFS_WINDOW(data);

	if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ELEMENT &&
		gst_structure_has_name(gst_message_get_structure(msg), "level")) {

		GstElement *src = GST_ELEMENT(GST_MESSAGE_SRC(msg));
		gchar *name = gst_element_get_name(src);

		if (purple_strequal(name, "level")) {
			gdouble percent;
			gdouble threshold;
			GstElement *valve;

			percent = gst_msg_db_to_percent(msg, "rms");
			gtk_progress_bar_set_fraction(
			        GTK_PROGRESS_BAR(win->vv.voice.level), percent);

			percent = gst_msg_db_to_percent(msg, "decay");
			threshold = gtk_range_get_value(GTK_RANGE(
			                    win->vv.voice.threshold)) /
			            100.0;
			valve = gst_bin_get_by_name(GST_BIN(GST_ELEMENT_PARENT(src)), "valve");
			g_object_set(valve, "drop", (percent < threshold), NULL);
			g_object_set(win->vv.voice.level, "text",
			             (percent < threshold) ? _("DROP") : " ",
			             NULL);
		}

		g_free(name);
	}

	return TRUE;
}

static void
voice_test_destroy_cb(GtkWidget *w, gpointer data)
{
	PidginPrefsWindow *win = PIDGIN_PREFS_WINDOW(data);

	if (!win->vv.voice.pipeline) {
		return;
	}

	gst_element_set_state(win->vv.voice.pipeline, GST_STATE_NULL);
	g_clear_pointer(&win->vv.voice.pipeline, gst_object_unref);
}

static void
enable_voice_test(PidginPrefsWindow *win)
{
	GstBus *bus;

	win->vv.voice.pipeline = create_voice_pipeline();
	bus = gst_pipeline_get_bus(GST_PIPELINE(win->vv.voice.pipeline));
	gst_bus_add_signal_watch(bus);
	g_signal_connect(bus, "message", G_CALLBACK(gst_bus_cb), win);
	gst_object_unref(bus);
}

static void
toggle_voice_test_cb(GtkToggleButton *test, gpointer data)
{
	PidginPrefsWindow *win = PIDGIN_PREFS_WINDOW(data);

	if (gtk_toggle_button_get_active(test)) {
		gtk_widget_set_sensitive(win->vv.voice.level, TRUE);
		enable_voice_test(win);

		g_signal_connect(win->vv.voice.volume, "value-changed",
		                 G_CALLBACK(on_volume_change_cb), win);
		g_signal_connect(test, "destroy",
		                 G_CALLBACK(voice_test_destroy_cb), win);
	} else {
		gtk_progress_bar_set_fraction(
		        GTK_PROGRESS_BAR(win->vv.voice.level), 0.0);
		gtk_widget_set_sensitive(win->vv.voice.level, FALSE);
		g_object_disconnect(win->vv.voice.volume,
		                    "any-signal::value-changed",
		                    G_CALLBACK(on_volume_change_cb), win, NULL);
		g_object_disconnect(test, "any-signal::destroy",
		                    G_CALLBACK(voice_test_destroy_cb), win,
		                    NULL);
		voice_test_destroy_cb(NULL, win);
	}
}

static void
volume_changed_cb(GtkScaleButton *button, gdouble value, gpointer data)
{
	purple_prefs_set_int("/purple/media/audio/volume/input", value * 100);
}

static void
threshold_value_changed_cb(GtkScale *scale, GtkWidget *label)
{
	int value;
	char *tmp;

	value = (int)gtk_range_get_value(GTK_RANGE(scale));
	tmp = g_strdup_printf(_("Silence threshold: %d%%"), value);
	gtk_label_set_label(GTK_LABEL(label), tmp);
	g_free(tmp);

	purple_prefs_set_int("/purple/media/audio/silence_threshold", value);
}

static void
bind_voice_test(PidginPrefsWindow *win, GtkBuilder *builder)
{
	GObject *test;
	GObject *label;
	GObject *volume;
	GObject *threshold;
	char *tmp;

	volume = gtk_builder_get_object(builder, "vv.voice.volume");
	win->vv.voice.volume = GTK_WIDGET(volume);
	gtk_scale_button_set_value(GTK_SCALE_BUTTON(volume),
			purple_prefs_get_int("/purple/media/audio/volume/input") / 100.0);
	g_signal_connect(volume, "value-changed",
	                 G_CALLBACK(volume_changed_cb), NULL);

	label = gtk_builder_get_object(builder, "vv.voice.threshold_label");
	tmp = g_strdup_printf(_("Silence threshold: %d%%"),
	                      purple_prefs_get_int("/purple/media/audio/silence_threshold"));
	gtk_label_set_text(GTK_LABEL(label), tmp);
	g_free(tmp);

	threshold = gtk_builder_get_object(builder, "vv.voice.threshold");
	win->vv.voice.threshold = GTK_WIDGET(threshold);
	gtk_range_set_value(GTK_RANGE(threshold),
			purple_prefs_get_int("/purple/media/audio/silence_threshold"));
	g_signal_connect(threshold, "value-changed",
	                 G_CALLBACK(threshold_value_changed_cb), label);

	win->vv.voice.level =
	        GTK_WIDGET(gtk_builder_get_object(builder, "vv.voice.level"));

	test = gtk_builder_get_object(builder, "vv.voice.test");
	g_signal_connect(test, "toggled",
	                 G_CALLBACK(toggle_voice_test_cb), win);
	win->vv.voice.test = GTK_WIDGET(test);
}

static GstElement *
create_video_pipeline(void)
{
	GstElement *pipeline;
	GstElement *src, *sink;
	GstElement *videoconvert;
	GstElement *videoscale;

	pipeline = gst_pipeline_new("videotest");
	src = create_test_element(PURPLE_MEDIA_ELEMENT_VIDEO | PURPLE_MEDIA_ELEMENT_SRC);
	sink = create_test_element(PURPLE_MEDIA_ELEMENT_VIDEO | PURPLE_MEDIA_ELEMENT_SINK);
	videoconvert = gst_element_factory_make("videoconvert", NULL);
	videoscale = gst_element_factory_make("videoscale", NULL);

	g_object_set_data(G_OBJECT(pipeline), "sink", sink);

	gst_bin_add_many(GST_BIN(pipeline), src, videoconvert, videoscale, sink,
			NULL);
	gst_element_link_many(src, videoconvert, videoscale, sink, NULL);

	return pipeline;
}

static void
video_test_destroy_cb(GtkWidget *w, gpointer data)
{
	PidginPrefsWindow *win = PIDGIN_PREFS_WINDOW(data);

	if (!win->vv.video.pipeline) {
		return;
	}

	gst_element_set_state(win->vv.video.pipeline, GST_STATE_NULL);
	g_clear_pointer(&win->vv.video.pipeline, gst_object_unref);
}

static void
enable_video_test(PidginPrefsWindow *win)
{
	GtkWidget *video = NULL;
	GstElement *sink = NULL;

	win->vv.video.pipeline = create_video_pipeline();

	sink = g_object_get_data(G_OBJECT(win->vv.video.pipeline), "sink");
	g_object_get(sink, "widget", &video, NULL);
	gtk_widget_show(video);

	g_clear_pointer(&win->vv.video.sink_widget, gtk_widget_destroy);
	gtk_container_add(GTK_CONTAINER(win->vv.video.frame), video);
	win->vv.video.sink_widget = video;

	gst_element_set_state(GST_ELEMENT(win->vv.video.pipeline),
	                      GST_STATE_PLAYING);
}

static void
toggle_video_test_cb(GtkToggleButton *test, gpointer data)
{
	PidginPrefsWindow *win = PIDGIN_PREFS_WINDOW(data);

	if (gtk_toggle_button_get_active(test)) {
		enable_video_test(win);
		g_signal_connect(test, "destroy",
		                 G_CALLBACK(video_test_destroy_cb), win);
	} else {
		g_object_disconnect(test, "any-signal::destroy",
		                    G_CALLBACK(video_test_destroy_cb), win,
		                    NULL);
		video_test_destroy_cb(NULL, win);
	}
}

static void
bind_video_test(PidginPrefsWindow *win, GtkBuilder *builder)
{
	GObject *test;

	win->vv.video.frame = GTK_WIDGET(
	        gtk_builder_get_object(builder, "vv.video.frame"));
	test = gtk_builder_get_object(builder, "vv.video.test");
	g_signal_connect(test, "toggled",
	                 G_CALLBACK(toggle_video_test_cb), win);
	win->vv.video.test = GTK_WIDGET(test);
}

static void
vv_device_changed_cb(const gchar *name, PurplePrefType type,
                     gconstpointer value, gpointer data)
{
	PidginPrefsWindow *win = PIDGIN_PREFS_WINDOW(data);

	PurpleMediaManager *manager;
	PurpleMediaElementInfo *info;

	manager = purple_media_manager_get();
	info = purple_media_manager_get_element_info(manager, value);
	purple_media_manager_set_active_element(manager, info);

	/* Refresh test viewers */
	if (strstr(name, "audio") && win->vv.voice.pipeline) {
		voice_test_destroy_cb(NULL, win);
		enable_voice_test(win);
	} else if (strstr(name, "video") && win->vv.video.pipeline) {
		video_test_destroy_cb(NULL, win);
		enable_video_test(win);
	}
}

static const char *
purple_media_type_to_preference_key(PurpleMediaElementType type)
{
	if (type & PURPLE_MEDIA_ELEMENT_AUDIO) {
		if (type & PURPLE_MEDIA_ELEMENT_SRC) {
			return PIDGIN_PREFS_ROOT "/vvconfig/audio/src/device";
		} else if (type & PURPLE_MEDIA_ELEMENT_SINK) {
			return PIDGIN_PREFS_ROOT "/vvconfig/audio/sink/device";
		}
	} else if (type & PURPLE_MEDIA_ELEMENT_VIDEO) {
		if (type & PURPLE_MEDIA_ELEMENT_SRC) {
			return PIDGIN_PREFS_ROOT "/vvconfig/video/src/device";
		} else if (type & PURPLE_MEDIA_ELEMENT_SINK) {
			return PIDGIN_PREFS_ROOT "/vvconfig/video/sink/device";
		}
	}

	return NULL;
}

static void
bind_vv_dropdown(PidginPrefCombo *combo, PurpleMediaElementType element_type)
{
	const gchar *preference_key;
	GList *devices;

	preference_key = purple_media_type_to_preference_key(element_type);
	devices = get_vv_device_menuitems(element_type);

	if (g_list_find_custom(devices, purple_prefs_get_string(preference_key),
		(GCompareFunc)strcmp) == NULL)
	{
		GList *next = g_list_next(devices);
		if (next)
			purple_prefs_set_string(preference_key, next->data);
	}

	combo->type = PURPLE_PREF_STRING;
	combo->key = preference_key;
	pidgin_prefs_bind_dropdown_from_list(combo, devices);
	g_list_free_full(devices, g_free);
}

static void
bind_vv_frame(PidginPrefsWindow *win, PidginPrefCombo *combo,
              PurpleMediaElementType type)
{
	bind_vv_dropdown(combo, type);

	purple_prefs_connect_callback(combo->combo,
	                              purple_media_type_to_preference_key(type),
	                              vv_device_changed_cb, win);
	g_signal_connect_swapped(combo->combo, "destroy",
	                         G_CALLBACK(purple_prefs_disconnect_by_handle),
	                         combo->combo);

	g_object_set_data(G_OBJECT(combo->combo), "vv_media_type",
	                  (gpointer)type);
	g_object_set_data(G_OBJECT(combo->combo), "vv_combo", combo);
}

static void
device_list_changed_cb(PurpleMediaManager *manager, GtkWidget *widget)
{
	PidginPrefCombo *combo;
	PurpleMediaElementType media_type;
	GtkTreeModel *model;

	combo = g_object_get_data(G_OBJECT(widget), "vv_combo");
	media_type = (PurpleMediaElementType)g_object_get_data(G_OBJECT(widget),
			"vv_media_type");

	/* Unbind original connections so we can repopulate the combo box. */
	g_object_disconnect(combo->combo, "any-signal::changed",
	                    G_CALLBACK(bind_dropdown_set), combo, NULL);
	model = gtk_combo_box_get_model(GTK_COMBO_BOX(combo->combo));
	gtk_list_store_clear(GTK_LIST_STORE(model));

	bind_vv_dropdown(combo, media_type);
}

static GtkWidget *
vv_page(PidginPrefsWindow *win)
{
	GtkBuilder *builder;
	GtkWidget *ret;
	PurpleMediaManager *manager;

	builder = gtk_builder_new_from_resource("/im/pidgin/Pidgin3/Prefs/vv.ui");
	gtk_builder_set_translation_domain(builder, PACKAGE);

	ret = GTK_WIDGET(gtk_builder_get_object(builder, "vv.page"));

	manager = purple_media_manager_get();

	win->vv.voice.input.combo = GTK_WIDGET(
	        gtk_builder_get_object(builder, "vv.voice.input.combo"));
	bind_vv_frame(win, &win->vv.voice.input,
	              PURPLE_MEDIA_ELEMENT_AUDIO | PURPLE_MEDIA_ELEMENT_SRC);
	g_signal_connect_object(manager, "elements-changed::audiosrc",
	                        G_CALLBACK(device_list_changed_cb),
	                        win->vv.voice.input.combo, 0);

	win->vv.voice.output.combo = GTK_WIDGET(
	        gtk_builder_get_object(builder, "vv.voice.output.combo"));
	bind_vv_frame(win, &win->vv.voice.output,
	              PURPLE_MEDIA_ELEMENT_AUDIO | PURPLE_MEDIA_ELEMENT_SINK);
	g_signal_connect_object(manager, "elements-changed::audiosink",
	                        G_CALLBACK(device_list_changed_cb),
	                        win->vv.voice.output.combo, 0);

	bind_voice_test(win, builder);

	win->vv.video.input.combo = GTK_WIDGET(
	        gtk_builder_get_object(builder, "vv.video.input.combo"));
	bind_vv_frame(win, &win->vv.video.input,
	              PURPLE_MEDIA_ELEMENT_VIDEO | PURPLE_MEDIA_ELEMENT_SRC);
	g_signal_connect_object(manager, "elements-changed::videosrc",
	                        G_CALLBACK(device_list_changed_cb),
	                        win->vv.video.input.combo, 0);

	win->vv.video.output.combo = GTK_WIDGET(
	        gtk_builder_get_object(builder, "vv.video.output.combo"));
	bind_vv_frame(win, &win->vv.video.output,
	              PURPLE_MEDIA_ELEMENT_VIDEO | PURPLE_MEDIA_ELEMENT_SINK);
	g_signal_connect_object(manager, "elements-changed::videosink",
	                        G_CALLBACK(device_list_changed_cb),
	                        win->vv.video.output.combo, 0);

	bind_video_test(win, builder);

	g_signal_connect(win->stack, "notify::visible-child",
	                 G_CALLBACK(vv_test_switch_page_cb), win);

	g_object_ref(ret);
	g_object_unref(builder);

	return ret;
}
#endif

static void
prefs_stack_init(PidginPrefsWindow *win)
{
#ifdef USE_VV
	GtkStack *stack = GTK_STACK(win->stack);
	GtkWidget *vv;
#endif

#ifdef USE_VV
	vv = vv_page(win);
	gtk_container_add_with_properties(GTK_CONTAINER(stack), vv, "name",
	                                  "vv", "title", _("Voice/Video"),
	                                  NULL);
	g_object_unref(vv);
#endif
}

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
	/* copy the preferences to tmp values...
	 * I liked "take affect immediately" Oh well :-( */
	/* (that should have been "effect," right?) */

	/* Back to instant-apply! I win!  BU-HAHAHA! */

	/* Create the window */
	gtk_widget_init_template(GTK_WIDGET(win));

	prefs_stack_init(win);
}

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

