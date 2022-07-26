/*
 * Pidgin - Transparency plugin
 *
 * Copyright (C) 1998-2002, Rob Flynn <rob@marko.net>
 * Copyright (C) 2002-2003, Herman Bloggs <hermanator12002@yahoo.com>
 * Copyright (C) 2005,      Daniel Atallah <daniel_atallah@yahoo.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 *
 */

#include <glib.h>
#include <glib/gi18n-lib.h>

#include <purple.h>

#include <gplugin.h>
#include <gplugin-native.h>

#include <pidgin.h>

/*
 *  MACROS & DEFINES
 */
/* The plugin name is left unchanged from its WinAPI days in order to keep it
 * loading for users who were using it. */
#define WINTRANS_PLUGIN_ID	"gtk-win-trans"

/*
 *  DATA STRUCTS
 */
typedef struct {
	GtkWidget *win;
	GtkWidget *slider;
} slider_win;

/*
 *  LOCALS
 */
static const char *OPT_WINTRANS_IM_ENABLED= "/plugins/gtk/transparency/im_enabled";
static const char *OPT_WINTRANS_IM_ALPHA  = "/plugins/gtk/transparency/im_alpha";
static const char *OPT_WINTRANS_IM_SLIDER = "/plugins/gtk/transparency/im_slider";
static const char *OPT_WINTRANS_IM_ONFOCUS= "/plugins/gtk/transparency/im_solid_onfocus";
static const char *OPT_WINTRANS_IM_ONTOP  = "/plugins/gtk/transparency/im_always_on_top";
static const char *OPT_WINTRANS_BL_ENABLED= "/plugins/gtk/transparency/bl_enabled";
static const char *OPT_WINTRANS_BL_ALPHA  = "/plugins/gtk/transparency/bl_alpha";
static const char *OPT_WINTRANS_BL_ONFOCUS= "/plugins/gtk/transparency/bl_solid_onfocus";
static const char *OPT_WINTRANS_BL_ONTOP  = "/plugins/gtk/transparency/bl_always_on_top";
static GSList *window_list = NULL;

/*
 *  CODE
 */

static GtkWidget *
get_buddy_list_window(void) {
	PurpleBuddyList *purple_blist = NULL;

	purple_blist = purple_blist_get_default();
	if(PIDGIN_IS_BUDDY_LIST(purple_blist)) {
		PidginBuddyList *pidgin_blist = PIDGIN_BUDDY_LIST(purple_blist);

		return pidgin_blist->window;
	}

	return NULL;
}

/* Set window transparency level */
static void set_wintrans(GtkWidget *window, int alpha, gboolean enabled,
		gboolean always_on_top) {
	GdkWindow *gdk_window = gtk_widget_get_window(window);

	if (enabled) {
		gdk_window_set_opacity(gdk_window, alpha / 255.0);
		gdk_window_set_keep_above(gdk_window, always_on_top);
	} else {
		gdk_window_set_opacity(gdk_window, 1);
		gdk_window_set_keep_above(gdk_window, 0);
	}
}

/* When a conv window is focused, if we're only transparent when unfocused,
 * deal with transparency */
static gboolean focus_conv_win_cb(GtkWidget *w, GdkEventFocus *e, gpointer d) {
	if (purple_prefs_get_bool(OPT_WINTRANS_IM_ENABLED)
			&& purple_prefs_get_bool(OPT_WINTRANS_IM_ONFOCUS)) {
		GtkWidget *window = (GtkWidget *) d;
		if (e->in) { /* Focused */
			set_wintrans(window, 0, FALSE,
				purple_prefs_get_bool(OPT_WINTRANS_IM_ONTOP));
		} else {
			set_wintrans(window,
				purple_prefs_get_int(OPT_WINTRANS_IM_ALPHA),
				TRUE,
				purple_prefs_get_bool(OPT_WINTRANS_IM_ONTOP));
		}
	}
	return FALSE;
}

/* When buddy list window is focused,
 * if we're only transparent when unfocused, deal with transparency */
static gboolean focus_blist_win_cb(GtkWidget *w, GdkEventFocus *e, gpointer d) {
	if (purple_prefs_get_bool(OPT_WINTRANS_BL_ENABLED)
			&& purple_prefs_get_bool(OPT_WINTRANS_BL_ONFOCUS)) {
		GtkWidget *window = (GtkWidget *) d;
		if (e->in) { /* Focused */
			set_wintrans(window, 0, FALSE,
				purple_prefs_get_bool(OPT_WINTRANS_BL_ONTOP));
		} else {
			set_wintrans(window,
				purple_prefs_get_int(OPT_WINTRANS_BL_ALPHA),
				TRUE,
				purple_prefs_get_bool(OPT_WINTRANS_BL_ONTOP));
		}
	}
	return FALSE;
}

static void change_alpha(GtkWidget *w, gpointer data) {
	int alpha = gtk_range_get_value(GTK_RANGE(w));
	purple_prefs_set_int(OPT_WINTRANS_IM_ALPHA, alpha);

	/* If we're in no-transparency on focus mode,
	 * don't take effect immediately */
	if (!purple_prefs_get_bool(OPT_WINTRANS_IM_ONFOCUS))
		set_wintrans(GTK_WIDGET(data), alpha, TRUE,
			purple_prefs_get_bool(OPT_WINTRANS_IM_ONTOP));
}


static GtkWidget *wintrans_slider(GtkWidget *win) {
	GtkWidget *hbox;
	GtkWidget *label, *slider;
	GtkWidget *frame;

	int imalpha = purple_prefs_get_int(OPT_WINTRANS_IM_ALPHA);

	frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
	gtk_widget_show(frame);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_container_add(GTK_CONTAINER(frame), hbox);

	label = gtk_label_new(_("Opacity:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 50, 255, 1);
	gtk_range_set_value(GTK_RANGE(slider), imalpha);

	/* On slider val change, update window's transparency level */
	g_signal_connect(G_OBJECT(slider), "value-changed",
		G_CALLBACK(change_alpha), win);

	gtk_box_pack_start(GTK_BOX(hbox), slider, FALSE, TRUE, 5);

	/* Set the initial transparency level */
	set_wintrans(win, imalpha, TRUE,
		purple_prefs_get_bool(OPT_WINTRANS_IM_ONTOP));

	gtk_widget_show_all(hbox);

	return frame;
}

static slider_win* find_slidwin(GtkWidget *win) {
	GSList *tmp = window_list;

	while (tmp) {
		if (((slider_win*) (tmp->data))->win == win)
			return (slider_win*) tmp->data;
		tmp = tmp->next;
	}
	return NULL;
}

/* Clean up transparency stuff for the conv window */
static void cleanup_conv_window(PidginConvWindow *win) {
	GtkWidget *window = win->window;
	slider_win *slidwin = NULL;

	/* Remove window from the window list */
	purple_debug_info(WINTRANS_PLUGIN_ID,
		"Conv window destroyed... removing from list\n");

	if ((slidwin = find_slidwin(window))) {
		window_list = g_slist_remove(window_list, slidwin);
		g_free(slidwin);
	}

	/* Remove the focus cbs */
	g_signal_handlers_disconnect_by_func(G_OBJECT(window),
		G_CALLBACK(focus_conv_win_cb), window);
}

static void
conversation_delete_cb(PurpleConversation *conv) {
	PidginConvWindow *win = pidgin_conv_get_window(PIDGIN_CONVERSATION(conv));
	/* If it is the last conversation in the window, cleanup */
	if (win != NULL && pidgin_conv_window_get_gtkconv_count(win) == 1)
		cleanup_conv_window(win);
}

static void set_blist_trans(GtkWidget *w, const char *pref) {
	GtkWidget *window = get_buddy_list_window();
	gboolean enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w));
	purple_prefs_set_bool(pref, enabled);
	if (window != NULL) {
		set_wintrans(window, purple_prefs_get_int(OPT_WINTRANS_BL_ALPHA),
			purple_prefs_get_bool(OPT_WINTRANS_BL_ENABLED),
			purple_prefs_get_bool(OPT_WINTRANS_IM_ONTOP));
	}
}

static void add_slider(GtkWidget *win) {
	GList *wl, *wl1;
	GtkWidget *vbox = NULL;

	/* Look up this window to see if it already has a slider */
	if (!find_slidwin(win)) {
		GtkWidget *slider_box = NULL;
		slider_win *slidwin = NULL;

		/* Get top vbox */
		for (wl1 = wl = gtk_container_get_children(
					GTK_CONTAINER(win));
				wl != NULL;
				wl = wl->next) {
			if (GTK_IS_BOX(G_OBJECT(wl->data)))
				vbox = GTK_WIDGET(wl->data);
			else {
				purple_debug_error(WINTRANS_PLUGIN_ID,
					"no vbox found\n");
				return;
			}
		}
		g_list_free(wl1);

		slider_box = wintrans_slider(win);
		gtk_box_pack_start(GTK_BOX(vbox), slider_box, FALSE, FALSE, 0);

		/* Add window to list, to track that it has a slider */
		slidwin = g_new0(slider_win, 1);
		slidwin->win = win;
		slidwin->slider = slider_box;
		window_list = g_slist_append(window_list, slidwin);
	}
}

static void remove_sliders() {
	if (window_list) {
		GSList *tmp = window_list;
		while (tmp) {
			slider_win *slidwin = (slider_win*) tmp->data;
			if (slidwin != NULL &&
					GTK_IS_WINDOW(slidwin->win)) {
				gtk_widget_destroy(slidwin->slider);
			}
			g_free(slidwin);
			tmp = tmp->next;
		}
		g_slist_free(window_list);
		window_list = NULL;
	}
}

/* Remove all transparency related aspects from conversation windows */
static void remove_convs_wintrans(gboolean remove_signal) {
	GList *wins;

	for (wins = pidgin_conv_windows_get_list(); wins; wins = wins->next) {
		PidginConvWindow *win = wins->data;
		GtkWidget *window = win->window;

		if (purple_prefs_get_bool(OPT_WINTRANS_IM_ENABLED))
			set_wintrans(window, 0, FALSE, FALSE);

		/* Remove the focus cbs */
		if (remove_signal)
			g_signal_handlers_disconnect_by_func(G_OBJECT(window),
				G_CALLBACK(focus_conv_win_cb), window);
	}

	remove_sliders();
}

static void set_conv_window_trans(PidginConvWindow *oldwin, PidginConvWindow *newwin) {
	GtkWidget *win = newwin->window;

	/* check prefs to see if we want trans */
	if (purple_prefs_get_bool(OPT_WINTRANS_IM_ENABLED)) {
		set_wintrans(win, purple_prefs_get_int(OPT_WINTRANS_IM_ALPHA),
			TRUE, purple_prefs_get_bool(OPT_WINTRANS_IM_ONTOP));

		if (purple_prefs_get_bool(OPT_WINTRANS_IM_SLIDER)) {
			add_slider(win);
		}
	}

	/* If we're moving from one window to another,
	 * add the focus listeners to the new window if not already there */
	if (oldwin != NULL && oldwin != newwin) {
		if (pidgin_conv_window_get_gtkconv_count(newwin) == 0) {
			g_signal_connect(G_OBJECT(win), "focus_in_event",
				G_CALLBACK(focus_conv_win_cb), win);
			g_signal_connect(G_OBJECT(win), "focus_out_event",
				G_CALLBACK(focus_conv_win_cb), win);
		}

		/* If we've moved the last conversation, cleanup the window */
		if (pidgin_conv_window_get_gtkconv_count(oldwin) == 1)
			cleanup_conv_window(oldwin);
	}
}

static void update_convs_wintrans(GtkWidget *toggle_btn, const char *pref) {
	purple_prefs_set_bool(pref, gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(toggle_btn)));

	if (purple_prefs_get_bool(OPT_WINTRANS_IM_ENABLED)) {
		GList *wins;

		for (wins = pidgin_conv_windows_get_list(); wins; wins = wins->next) {
			PidginConvWindow *win = wins->data;
			set_conv_window_trans(NULL, win);
		}

		if (!purple_prefs_get_bool(OPT_WINTRANS_IM_SLIDER))
			remove_sliders();
	}
	else
		remove_convs_wintrans(FALSE);
}

static void
conv_updated_cb(PurpleConversation *conv, PurpleConversationUpdateType type) {
	PidginConversation *pconv = PIDGIN_CONVERSATION(conv);
	PidginConvWindow *win = pidgin_conv_get_window(pconv);

	if(type == PURPLE_CONVERSATION_UPDATE_UNSEEN
			&& pconv->unseen_state == PIDGIN_UNSEEN_NONE
			&& pidgin_conv_window_get_gtkconv_count(win) == 1) {
		GtkWidget *window = win->window;
		gboolean has_focus;

		g_object_get(G_OBJECT(window), "has-toplevel-focus", &has_focus, NULL);

		if(!has_focus || !purple_prefs_get_bool(OPT_WINTRANS_IM_ONFOCUS)) {
			set_conv_window_trans(NULL, win);
		}

		if(g_signal_handler_find(G_OBJECT(window), G_SIGNAL_MATCH_FUNC,
				0, 0, NULL, G_CALLBACK(focus_conv_win_cb), NULL) == 0) {
			g_signal_connect(G_OBJECT(window), "focus_in_event",
				G_CALLBACK(focus_conv_win_cb), window);
			g_signal_connect(G_OBJECT(window), "focus_out_event",
				G_CALLBACK(focus_conv_win_cb), window);
		}
	}
}

static void
new_conversation_cb(PurpleConversation *conv) {
	PidginConvWindow *win = pidgin_conv_get_window(PIDGIN_CONVERSATION(conv));

	/* If it is the first conversation in the window,
	 * add the sliders, and set transparency */
	if(pidgin_conv_window_get_gtkconv_count(win) == 1) {
		GtkWidget *window = win->window;

		set_conv_window_trans(NULL, win);

		g_signal_connect(G_OBJECT(window), "focus_in_event",
			G_CALLBACK(focus_conv_win_cb), window);
		g_signal_connect(G_OBJECT(window), "focus_out_event",
			G_CALLBACK(focus_conv_win_cb), window);
	}
}

static void
blist_created_cb(PurpleBuddyList *purple_blist, gpointer data) {
	GtkWidget *window = get_buddy_list_window();
	if (window != NULL) {
		if (purple_prefs_get_bool(OPT_WINTRANS_BL_ENABLED)) {
			set_wintrans(window,
				purple_prefs_get_int(OPT_WINTRANS_BL_ALPHA),
				TRUE,
				purple_prefs_get_bool(OPT_WINTRANS_BL_ONTOP));
		}

		g_signal_connect(G_OBJECT(window), "focus_in_event",
			G_CALLBACK(focus_blist_win_cb), window);
		g_signal_connect(G_OBJECT(window), "focus_out_event",
			G_CALLBACK(focus_blist_win_cb), window);
	}
}

static void alpha_change(GtkWidget *w, gpointer data) {
	GList *wins;
	int imalpha = gtk_range_get_value(GTK_RANGE(w));

	for (wins = pidgin_conv_windows_get_list(); wins; wins = wins->next) {
		PidginConvWindow *win = wins->data;
		set_wintrans(win->window, imalpha, TRUE,
			purple_prefs_get_bool(OPT_WINTRANS_IM_ONTOP));
	}
}

static void alpha_pref_set_int (GtkWidget *w, GdkEventFocus *e, const char *pref)
{
	int alpha = gtk_range_get_value(GTK_RANGE(w));
	purple_prefs_set_int(pref, alpha);
}

static void bl_alpha_change(GtkWidget *w, gpointer data) {
	GtkWidget *window = get_buddy_list_window();
	if (window != NULL) {
		change_alpha(w, window);
	}
}

static void update_existing_convs() {
	GList *wins;

	for (wins = pidgin_conv_windows_get_list(); wins; wins = wins->next) {
		PidginConvWindow *win = wins->data;
		GtkWidget *window = win->window;

		set_conv_window_trans(NULL, win);

		g_signal_connect(G_OBJECT(window), "focus_in_event",
			G_CALLBACK(focus_conv_win_cb), window);
		g_signal_connect(G_OBJECT(window), "focus_out_event",
			G_CALLBACK(focus_conv_win_cb), window);
	}
}

static GtkWidget *
get_config_frame(PurplePlugin *plugin) {
	GtkWidget *ret;
	GtkWidget *imtransbox, *bltransbox;
	GtkWidget *hbox;
	GtkWidget *label, *slider;
	GtkWidget *button;
	GtkWidget *trans_box;

	ret = gtk_box_new(GTK_ORIENTATION_VERTICAL, 18);
	gtk_container_set_border_width(GTK_CONTAINER (ret), 12);

	/* IM Convo trans options */
	imtransbox = pidgin_make_frame(ret, _("IM Conversation Windows"));
	button = pidgin_prefs_checkbox(_("_IM window transparency"),
		OPT_WINTRANS_IM_ENABLED, imtransbox);
	g_signal_connect(G_OBJECT(button), "clicked",
		G_CALLBACK(update_convs_wintrans),
		(gpointer) OPT_WINTRANS_IM_ENABLED);

	trans_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 18);
	gtk_widget_show(trans_box);
	g_object_bind_property(button, "active", trans_box, "sensitive",
			G_BINDING_SYNC_CREATE);

	button = pidgin_prefs_checkbox(_("_Show slider bar in IM window"),
		OPT_WINTRANS_IM_SLIDER, trans_box);
	g_signal_connect(G_OBJECT(button), "clicked",
		G_CALLBACK(update_convs_wintrans),
		(gpointer) OPT_WINTRANS_IM_SLIDER);

	button = pidgin_prefs_checkbox(
		_("Remove IM window transparency on focus"),
		OPT_WINTRANS_IM_ONFOCUS, trans_box);

	button = pidgin_prefs_checkbox(_("Always on top"), OPT_WINTRANS_IM_ONTOP,
		trans_box);
	g_signal_connect(G_OBJECT(button), "clicked",
		G_CALLBACK(update_convs_wintrans),
		(gpointer) OPT_WINTRANS_IM_ONTOP);

	gtk_box_pack_start(GTK_BOX(imtransbox), trans_box, FALSE, FALSE, 5);

	/* IM transparency slider */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

	label = gtk_label_new(_("Opacity:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

	slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 50, 255, 1);
	gtk_range_set_value(GTK_RANGE(slider),
		purple_prefs_get_int(OPT_WINTRANS_IM_ALPHA));

	g_signal_connect(G_OBJECT(slider), "value-changed",
		G_CALLBACK(alpha_change), NULL);
	g_signal_connect(G_OBJECT(slider), "focus-out-event",
		G_CALLBACK(alpha_pref_set_int),
		(gpointer) OPT_WINTRANS_IM_ALPHA);

	gtk_box_pack_start(GTK_BOX(hbox), slider, FALSE, TRUE, 5);

	gtk_widget_show_all(hbox);

	gtk_box_pack_start(GTK_BOX(trans_box), hbox, FALSE, FALSE, 5);

	/* Buddy List trans options */
	bltransbox = pidgin_make_frame (ret, _("Buddy List Window"));
	button = pidgin_prefs_checkbox(_("_Buddy List window transparency"),
		OPT_WINTRANS_BL_ENABLED, bltransbox);
	g_signal_connect(G_OBJECT(button), "clicked",
		G_CALLBACK(set_blist_trans),
		(gpointer) OPT_WINTRANS_BL_ENABLED);

	trans_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 18);
	gtk_widget_show(trans_box);
	g_object_bind_property(button, "active", trans_box, "sensitive",
			G_BINDING_SYNC_CREATE);
	button = pidgin_prefs_checkbox(
		_("Remove Buddy List window transparency on focus"),
		OPT_WINTRANS_BL_ONFOCUS, trans_box);
	button = pidgin_prefs_checkbox(_("Always on top"), OPT_WINTRANS_BL_ONTOP,
		trans_box);
	g_signal_connect(G_OBJECT(button), "clicked",
		G_CALLBACK(set_blist_trans),
		(gpointer) OPT_WINTRANS_BL_ONTOP);
	gtk_box_pack_start(GTK_BOX(bltransbox), trans_box, FALSE, FALSE, 5);

	/* IM transparency slider */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

	label = gtk_label_new(_("Opacity:"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

	slider = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 50, 255, 1);
	gtk_range_set_value(GTK_RANGE(slider),
		purple_prefs_get_int(OPT_WINTRANS_BL_ALPHA));

	g_signal_connect(G_OBJECT(slider), "value-changed",
		G_CALLBACK(bl_alpha_change), NULL);
	g_signal_connect(G_OBJECT(slider), "focus-out-event",
		G_CALLBACK(alpha_pref_set_int),
		(gpointer) OPT_WINTRANS_BL_ALPHA);

	gtk_box_pack_start(GTK_BOX(hbox), slider, FALSE, TRUE, 5);

	gtk_widget_show_all(hbox);

	gtk_box_pack_start(GTK_BOX(trans_box), hbox, FALSE, FALSE, 5);

	gtk_widget_show_all(ret);
	return ret;
}

static GPluginPluginInfo *
transparency_query(GError **error) {
	const gchar * const authors[] = {
		"Pidgin Developers <devel@pidgin.im>",
		NULL
	};

	return pidgin_plugin_info_new(
		"id", WINTRANS_PLUGIN_ID,
		"name", N_("Transparency"),
		"version", DISPLAY_VERSION,
		"category", N_("User interface"),
		"summary", N_("Variable Transparency for the buddy list and conversations."),
		"description", N_("This plugin enables variable alpha transparency on conversation windows and the buddy list."),
		"authors", authors,
		"website", PURPLE_WEBSITE,
		"abi-version", PURPLE_ABI_VERSION,
		"gtk-config-frame-cb", get_config_frame,
		NULL
	);
}

static gboolean
transparency_load(GPluginPlugin *plugin, GError **error) {
	GtkWidget *window = NULL;
	gpointer conv_handle = NULL;

	purple_prefs_add_none("/plugins/gtk");
	purple_prefs_add_none("/plugins/gtk/transparency");
	purple_prefs_add_bool(OPT_WINTRANS_IM_ENABLED, FALSE);
	purple_prefs_add_int(OPT_WINTRANS_IM_ALPHA, 255);
	purple_prefs_add_bool(OPT_WINTRANS_IM_SLIDER, FALSE);
	purple_prefs_add_bool(OPT_WINTRANS_IM_ONFOCUS, FALSE);
	purple_prefs_add_bool(OPT_WINTRANS_IM_ONTOP, FALSE);
	purple_prefs_add_bool(OPT_WINTRANS_BL_ENABLED, FALSE);
	purple_prefs_add_int(OPT_WINTRANS_BL_ALPHA, 255);
	purple_prefs_add_bool(OPT_WINTRANS_BL_ONFOCUS, FALSE);
	purple_prefs_add_bool(OPT_WINTRANS_BL_ONTOP, FALSE);
	purple_prefs_rename("/plugins/gtk/win32/wintrans", "/plugins/gtk/transparency");

	conv_handle = purple_conversations_get_handle();
	purple_signal_connect(conv_handle, "conversation-created", plugin,
	                      G_CALLBACK(new_conversation_cb), NULL);

	/* Set callback to remove window from the list, if the window is destroyed */
	purple_signal_connect(conv_handle, "deleting-conversation", plugin,
	                      G_CALLBACK(conversation_delete_cb), NULL);

	purple_signal_connect(conv_handle, "conversation-updated", plugin,
	                      G_CALLBACK(conv_updated_cb), NULL);

	update_existing_convs();

	window = get_buddy_list_window();
	if(window != NULL) {
		blist_created_cb(NULL, NULL);
	} else {
		purple_signal_connect(pidgin_blist_get_handle(),
			"gtkblist-created", plugin,
			G_CALLBACK(blist_created_cb), NULL);
	}

	return TRUE;
}

static gboolean
transparency_unload(GPluginPlugin *plugin, gboolean shutdown, GError **error) {
	GtkWidget *window = NULL;

	purple_debug_info(WINTRANS_PLUGIN_ID, "Unloading transparency plugin\n");

	remove_convs_wintrans(TRUE);

	window = get_buddy_list_window();
	if (window != NULL) {
		if (purple_prefs_get_bool(OPT_WINTRANS_BL_ENABLED))
			set_wintrans(window, 0, FALSE, FALSE);

		/* Remove the focus cbs */
		g_signal_handlers_disconnect_by_func(G_OBJECT(window),
			G_CALLBACK(focus_blist_win_cb), window);
	}

	return TRUE;
}

GPLUGIN_NATIVE_PLUGIN_DECLARE(transparency)

