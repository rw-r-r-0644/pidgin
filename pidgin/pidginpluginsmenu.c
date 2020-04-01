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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include "pidginpluginsmenu.h"

#include <gplugin.h>

#include <purple.h>

#include "internal.h"

#include "pidgin/pidginpluginsdialog.h"

struct _PidginPluginsMenu {
	GtkMenu parent;

	GtkWidget *separator;

	GSimpleActionGroup *action_group;

	GHashTable *plugin_items;
};

#define PIDGIN_PLUGINS_MENU_ACTION_PREFIX "plugins-menu"

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
pidgin_plugins_menu_action_activated(GSimpleAction *simple, GVariant *parameter,
                                     gpointer data)
{
	PurplePluginAction *action = (PurplePluginAction *)data;

	if(action != NULL && action->callback != NULL) {
		action->callback(action);
	}
}

static void
pidgin_plugins_menu_add_plugin_actions(PidginPluginsMenu *menu,
                                       PurplePlugin *plugin)
{
	GPluginPluginInfo *info = NULL;
	PurplePluginActionsCb actions_cb = NULL;
	GList *actions = NULL, *l = NULL;
	GtkWidget *submenu = NULL, *item = NULL;
	gint i = 0;

	info = gplugin_plugin_get_info(GPLUGIN_PLUGIN(plugin));

	actions_cb = purple_plugin_info_get_actions_cb(PURPLE_PLUGIN_INFO(info));
	if(actions_cb == NULL) {
		g_object_unref(G_OBJECT(info));

		return;
	}

	actions = actions_cb(plugin);
	if(actions == NULL) {
		g_object_unref(G_OBJECT(info));

		return;
	}

	submenu = gtk_menu_new();

	for(l = actions, i = 0; l != NULL; l = l->next, i++) {
		PurplePluginAction *action = (PurplePluginAction *)l->data;
		GSimpleAction *gaction = NULL;
		GtkWidget *action_item = NULL;
		gchar *action_base_name = NULL;
		gchar *action_full_name = NULL;

		if(action->label == NULL) {
			continue;
		}

		action_base_name = g_strdup_printf("%s-%d",
		                                   gplugin_plugin_info_get_id(info),
		                                   i);
		action_full_name = g_strdup_printf("%s.%s",
		                                   PIDGIN_PLUGINS_MENU_ACTION_PREFIX,
		                                   action_base_name);

		/* create the menu item with the full action name */
		action_item = gtk_menu_item_new_with_label(action->label);
		gtk_actionable_set_action_name(GTK_ACTIONABLE(action_item),
		                               action_full_name);
		gtk_widget_show(action_item);
		g_free(action_full_name);

		/* add our action item to the menu */
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), action_item);

		/* now Creation the gaction with the base name */
		gaction = g_simple_action_new(action_base_name, NULL);
		g_free(action_base_name);

		/* now connect to the activate signal of the action using
		 * g_signal_connect_data with a destory notify to free the plugin action
		 * when the signal handler is removed.
		 */
		g_signal_connect_data(G_OBJECT(gaction), "activate",
		                      G_CALLBACK(pidgin_plugins_menu_action_activated),
		                      action,
		                      (GClosureNotify)purple_plugin_action_free,
		                      0);

		/* finally add the action to the action group and remove our ref */
		g_action_map_add_action(G_ACTION_MAP(menu->action_group),
		                        G_ACTION(gaction));
		g_object_unref(G_OBJECT(gaction));
	}

	item = gtk_menu_item_new_with_label(gplugin_plugin_info_get_name(info));
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
	gtk_widget_show(item);

	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

	g_hash_table_insert(menu->plugin_items,
	                    g_object_ref(G_OBJECT(plugin)),
	                    item);

	g_object_unref(G_OBJECT(info));

	/* make sure that our separator is visible */
	gtk_widget_show(menu->separator);
}

static void
pidgin_plugins_menu_remove_plugin_actions(PidginPluginsMenu *menu,
                                          PurplePlugin *plugin)
{
	GPluginPluginInfo *info = NULL;
	PurplePluginActionsCb actions_cb = NULL;
	GList *actions = NULL, *l = NULL;
	gint i = 0;

	/* try remove the menu item from plugin from the hash table.  If we didn't
	 * remove anything, we have nothing to do so bail.
	 */
	if(!g_hash_table_remove(menu->plugin_items, plugin)) {
		return;
	}

	info = gplugin_plugin_get_info(GPLUGIN_PLUGIN(plugin));

	actions_cb = purple_plugin_info_get_actions_cb(PURPLE_PLUGIN_INFO(info));
	if(actions_cb == NULL) {
		g_object_unref(G_OBJECT(info));

		return;
	}

	actions = actions_cb(plugin);
	if(actions == NULL) {
		g_object_unref(G_OBJECT(info));

		return;
	}

	/* now walk through the actions and remove them from the action group. */
	for(l = actions, i = 0; l != NULL; l = l->next, i++) {
		gchar *name = NULL;

		name = g_strdup_printf("%s-%d", gplugin_plugin_info_get_id(info), i);

		g_action_map_remove_action(G_ACTION_MAP(menu->action_group), name);
		g_free(name);
	}

	g_object_unref(G_OBJECT(info));

	/* finally, if this was the last item in the list, hide the separator. */
	if(g_hash_table_size(menu->plugin_items) == 0) {
		gtk_widget_hide(menu->separator);
	}
}

/******************************************************************************
 * Purple Signal Callbacks
 *****************************************************************************/
static void
pidgin_plugins_menu_plugin_load_cb(PurplePlugin *plugin, gpointer data) {
	pidgin_plugins_menu_add_plugin_actions(PIDGIN_PLUGINS_MENU(data), plugin);
}

static void
pidgin_plugins_menu_plugin_unload_cb(PurplePlugin *plugin, gpointer data) {
	pidgin_plugins_menu_remove_plugin_actions(PIDGIN_PLUGINS_MENU(data),
	                                          plugin);
}

/******************************************************************************
 * Static Actions
 *****************************************************************************/
static void
pidgin_plugins_menu_show_manager(GSimpleAction *action, GVariant *parameter,
                                 gpointer data)
{
	GtkWidget *dialog = pidgin_plugins_dialog_new();

	/* fixme? */
#if 0
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
#endif

	gtk_widget_show_all(dialog);
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
G_DEFINE_TYPE(PidginPluginsMenu, pidgin_plugins_menu, GTK_TYPE_MENU)

static void
pidgin_plugins_menu_init(PidginPluginsMenu *menu) {
	GActionEntry actions[] = {
		{
			.name = "manager",
			.activate = pidgin_plugins_menu_show_manager,
		}
	};
	gpointer handle;

	/* initialize our template */
	gtk_widget_init_template(GTK_WIDGET(menu));

	/* create our internal action group and assign it to ourself */
	menu->action_group = g_simple_action_group_new();
	g_action_map_add_action_entries(G_ACTION_MAP(menu->action_group), actions,
	                                G_N_ELEMENTS(actions), NULL);
	gtk_widget_insert_action_group(GTK_WIDGET(menu),
	                               PIDGIN_PLUGINS_MENU_ACTION_PREFIX,
	                               G_ACTION_GROUP(menu->action_group));

	/* create our storage for the items */
	menu->plugin_items = g_hash_table_new_full(g_direct_hash, g_direct_equal,
	                                           g_object_unref,
	                                           (GDestroyNotify)gtk_widget_destroy);

	/* finally connect to the purple signals to stay up to date */
	handle = purple_plugins_get_handle();
	purple_signal_connect(handle, "plugin-load", menu,
	                      PURPLE_CALLBACK(pidgin_plugins_menu_plugin_load_cb),
	                      menu);
	purple_signal_connect(handle, "plugin-unload", menu,
	                      PURPLE_CALLBACK(pidgin_plugins_menu_plugin_unload_cb),
	                      menu);
};

static void
pidgin_plugins_menu_finalize(GObject *obj) {
	purple_signals_disconnect_by_handle(obj);

	G_OBJECT_CLASS(pidgin_plugins_menu_parent_class)->finalize(obj);
}

static void
pidgin_plugins_menu_class_init(PidginPluginsMenuClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	obj_class->finalize = pidgin_plugins_menu_finalize;

    gtk_widget_class_set_template_from_resource(
        widget_class,
        "/im/pidgin/Pidgin/Plugins/menu.ui"
    );

   	gtk_widget_class_bind_template_child(widget_class, PidginPluginsMenu,
   	                                     separator);
}

/******************************************************************************
 * Public API
 *****************************************************************************/
GtkWidget *
pidgin_plugins_menu_new(void) {
	return GTK_WIDGET(g_object_new(PIDGIN_TYPE_PLUGINS_MENU, NULL));
}

