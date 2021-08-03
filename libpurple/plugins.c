/*
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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

#include "internal.h"

#include "core.h"
#include "debug.h"
#include "plugins.h"
#include "purpleenums.h"
#include "signals.h"
#include "util.h"

/**************************************************************************
 * Globals
 **************************************************************************/
static GList *loaded_plugins     = NULL;
static GList *plugins_to_disable = NULL;

/**************************************************************************
 * Plugin API
 **************************************************************************/
static gboolean
plugin_loading_cb(GObject *manager, PurplePlugin *plugin, GError **error,
                  gpointer data)
{
	PurplePluginInfo *info;
	const gchar *info_error = NULL;

	g_return_val_if_fail(PURPLE_IS_PLUGIN(plugin), FALSE);

	info = purple_plugin_get_info(plugin);
	if (!info)
		return TRUE; /* a GPlugin internal plugin */

	info_error = purple_plugin_info_get_error(info);
	if(info_error != NULL) {
		gchar *filename = gplugin_plugin_get_filename(plugin);
		purple_debug_error("plugins", "Failed to load plugin %s: %s",
		                   filename,
		                   info_error);

		g_set_error(error, PURPLE_PLUGINS_DOMAIN, 0,
				    "Plugin is not loadable: %s", info_error);

		g_free(filename);
		return FALSE;
	}

	return TRUE;
}

static void
plugin_loaded_cb(GObject *manager, PurplePlugin *plugin)
{
	PurplePluginInfo *info;
	gchar *filename;

	g_return_if_fail(PURPLE_IS_PLUGIN(plugin));

	info = purple_plugin_get_info(plugin);
	if (!info)
		return; /* a GPlugin internal plugin */

	loaded_plugins = g_list_prepend(loaded_plugins, plugin);
	filename = gplugin_plugin_get_filename(plugin);

	purple_debug_info("plugins", "Loaded plugin %s\n", filename);

	g_free(filename);
}

static gboolean
plugin_unloading_cb(GObject *manager, PurplePlugin *plugin, GError **error,
                    gpointer data)
{
	PurplePluginInfo *info;
	gchar *filename;

	g_return_val_if_fail(PURPLE_IS_PLUGIN(plugin), FALSE);

	info = purple_plugin_get_info(plugin);
	if (info) {
		filename = gplugin_plugin_get_filename(plugin);
		purple_debug_info("plugins", "Unloading plugin %s\n",
		                  filename);
		g_free(filename);
	}

	return TRUE;
}

static void
plugin_unloaded_cb(GObject *manager, PurplePlugin *plugin)
{
	PurplePluginInfo *info;

	g_return_if_fail(PURPLE_IS_PLUGIN(plugin));

	info = purple_plugin_get_info(plugin);
	if (!info)
		return; /* a GPlugin internal plugin */

	/* cancel any pending dialogs the plugin has */
	purple_request_close_with_handle(plugin);
	purple_notify_close_with_handle(plugin);

	purple_signals_disconnect_by_handle(plugin);
	purple_signals_unregister_by_instance(plugin);

	purple_plugin_info_set_unloaded(info, TRUE);

	loaded_plugins     = g_list_remove(loaded_plugins, plugin);
	plugins_to_disable = g_list_remove(plugins_to_disable, plugin);

	purple_prefs_disconnect_by_handle(plugin);
}

gboolean
purple_plugin_load(PurplePlugin *plugin, GError **error)
{
	GError *err = NULL;
	gchar *filename;

	g_return_val_if_fail(plugin != NULL, FALSE);

	if (purple_plugin_is_loaded(plugin))
		return TRUE;

	if (!gplugin_manager_load_plugin(plugin, &err)) {
	        filename = gplugin_plugin_get_filename(plugin);
		purple_debug_error("plugins", "Failed to load plugin %s: %s",
		                   filename,
		                   err ? err->message : "Unknown reason");

		if (error)
			*error = g_error_copy(err);
		g_error_free(err);
		g_free(filename);
		return FALSE;
	}

	return TRUE;
}

gboolean
purple_plugin_unload(PurplePlugin *plugin, GError **error)
{
	GError *err = NULL;
	gchar *filename;

	g_return_val_if_fail(plugin != NULL, FALSE);

	if (!purple_plugin_is_loaded(plugin))
		return TRUE;

	if (!gplugin_manager_unload_plugin(plugin, &err)) {
	        filename = gplugin_plugin_get_filename(plugin);
		purple_debug_error("plugins", "Failed to unload plugin %s: %s",
		                   filename,
		                   err ? err->message : "Unknown reason");

		if (error)
			*error = g_error_copy(err);
		g_error_free(err);
		g_free(filename);

		return FALSE;
	}

	return TRUE;
}

gboolean
purple_plugin_is_loaded(PurplePlugin *plugin)
{
	g_return_val_if_fail(plugin != NULL, FALSE);

	return (gplugin_plugin_get_state(plugin) == GPLUGIN_PLUGIN_STATE_LOADED);
}

PurplePluginInfo *
purple_plugin_get_info(PurplePlugin *plugin)
{
	GPluginPluginInfo *info;

	g_return_val_if_fail(plugin != NULL, NULL);

	info = gplugin_plugin_get_info(plugin);

	/* GPlugin refs the plugin info object before returning it. This workaround
	 * is to avoid managing the reference counts everywhere in our codebase
	 * where we use the plugin info. The plugin info instance is guaranteed to
	 * exist as long as the plugin exists. */
	g_object_unref(info);

	if (PURPLE_IS_PLUGIN_INFO(info))
		return PURPLE_PLUGIN_INFO(info);
	else
		return NULL;
}

void
purple_plugin_disable(PurplePlugin *plugin)
{
	g_return_if_fail(plugin != NULL);

	if (!g_list_find(plugins_to_disable, plugin))
		plugins_to_disable = g_list_prepend(plugins_to_disable, plugin);
}

gboolean
purple_plugin_is_internal(PurplePlugin *plugin)
{
	PurplePluginInfo *info;

	g_return_val_if_fail(plugin != NULL, FALSE);

	info = purple_plugin_get_info(plugin);
	if (!info)
		return TRUE;

	return (purple_plugin_info_get_flags(info) &
	        PURPLE_PLUGIN_INFO_FLAGS_INTERNAL);
}

GSList *
purple_plugin_get_dependent_plugins(PurplePlugin *plugin)
{
#warning TODO: Implement this when GPlugin can return dependent plugins.
	return NULL;
}

/**************************************************************************
 * PluginAction API
 **************************************************************************/
PurplePluginAction *
purple_plugin_action_new(const char* label, PurplePluginActionCb callback)
{
	PurplePluginAction *action;

	g_return_val_if_fail(label != NULL && callback != NULL, NULL);

	action = g_new0(PurplePluginAction, 1);

	action->label    = g_strdup(label);
	action->callback = callback;

	return action;
}

void
purple_plugin_action_free(PurplePluginAction *action)
{
	g_return_if_fail(action != NULL);

	g_free(action->label);
	g_free(action);
}

static PurplePluginAction *
purple_plugin_action_copy(PurplePluginAction *action)
{
	g_return_val_if_fail(action != NULL, NULL);

	return purple_plugin_action_new(action->label, action->callback);
}

G_DEFINE_BOXED_TYPE(PurplePluginAction, purple_plugin_action,
                    purple_plugin_action_copy, purple_plugin_action_free)

/**************************************************************************
 * Plugins API
 **************************************************************************/
GList *
purple_plugins_find_all(void)
{
	GList *ret = NULL, *ids, *l;
	GSList *plugins, *ll;

	ids = gplugin_manager_list_plugins();

	for (l = ids; l; l = l->next) {
		plugins = gplugin_manager_find_plugins(l->data);

		for (ll = plugins; ll; ll = ll->next) {
			PurplePlugin *plugin = PURPLE_PLUGIN(ll->data);
			if (purple_plugin_get_info(plugin))
				ret = g_list_append(ret, plugin);
		}

		g_slist_free_full(plugins, g_object_unref);
	}
	g_list_free(ids);

	return ret;
}

GList *
purple_plugins_get_loaded(void)
{
	return loaded_plugins;
}

void
purple_plugins_add_search_path(const gchar *path)
{
	gplugin_manager_append_path(path);
}

void
purple_plugins_refresh(void)
{
	GList *plugins, *l;

	gplugin_manager_refresh();

	plugins = purple_plugins_find_all();
	for (l = plugins; l != NULL; l = l->next) {
		PurplePlugin *plugin = PURPLE_PLUGIN(l->data);
		PurplePluginInfo *info;
		PurplePluginInfoFlags flags;
		gboolean unloaded;

		if (purple_plugin_is_loaded(plugin))
			continue;

		info = purple_plugin_get_info(plugin);

		unloaded = purple_plugin_info_get_unloaded(info);
		flags = purple_plugin_info_get_flags(info);
		if (!unloaded && flags & PURPLE_PLUGIN_INFO_FLAGS_AUTO_LOAD) {
			gchar *filename = gplugin_plugin_get_filename(plugin);
			purple_debug_info("plugins", "Auto-loading plugin %s\n",
			                  filename);
			purple_plugin_load(plugin, NULL);
			g_free(filename);
		}
	}

	g_list_free(plugins);
}

PurplePlugin *
purple_plugins_find_plugin(const gchar *id)
{
	PurplePlugin *plugin;

	g_return_val_if_fail(id != NULL && *id != '\0', NULL);

	plugin = gplugin_manager_find_plugin(id);

	if (!plugin)
		return NULL;

	/* GPlugin refs the plugin object before returning it. This workaround is
	 * to avoid managing the reference counts everywhere in our codebase where
	 * we use plugin instances. A plugin object will exist till the plugins
	 * subsystem is uninitialized. */
	g_object_unref(plugin);

	return plugin;
}

PurplePlugin *
purple_plugins_find_by_filename(const char *filename)
{
	GList *plugins, *l;

	g_return_val_if_fail(filename != NULL && *filename != '\0', NULL);

	plugins = purple_plugins_find_all();

	for (l = plugins; l != NULL; l = l->next) {
		PurplePlugin *plugin = PURPLE_PLUGIN(l->data);
		gchar *plugin_filename = gplugin_plugin_get_filename(plugin);

		if (purple_strequal(plugin_filename, filename)) {
			g_list_free(plugins);
			g_free(plugin_filename);
			return plugin;
		}
		g_free(plugin_filename);
	}
	g_list_free(plugins);

	return NULL;
}

void
purple_plugins_save_loaded(const char *key)
{
	GList *pl;
	GList *files = NULL;

	g_return_if_fail(key != NULL && *key != '\0');

	for (pl = purple_plugins_get_loaded(); pl != NULL; pl = pl->next) {
		PurplePlugin *plugin = PURPLE_PLUGIN(pl->data);
		PurplePluginInfo *info = purple_plugin_get_info(plugin);
		if (!info)
			continue;

		if (purple_plugin_info_get_flags(info) &
				PURPLE_PLUGIN_INFO_FLAGS_AUTO_LOAD)
			continue;

		if (!g_list_find(plugins_to_disable, plugin))
			files = g_list_append(
			        files,
			        (gchar *)gplugin_plugin_get_filename(plugin));
	}

	purple_prefs_set_path_list(key, files);
	g_list_free_full(files, g_free);
}

void
purple_plugins_load_saved(const char *key)
{
	GList *l, *files;

	g_return_if_fail(key != NULL && *key != '\0');

	files = purple_prefs_get_path_list(key);

	for (l = files; l; l = l->next)
	{
		char *file;
		PurplePlugin *plugin;

		if (l->data == NULL)
			continue;

		file = l->data;
		plugin = purple_plugins_find_by_filename(file);

		if (plugin) {
			purple_debug_info("plugins", "Loading saved plugin %s\n", file);
			purple_plugin_load(plugin, NULL);
		} else {
			purple_debug_error("plugins", "Unable to find saved plugin %s\n", file);
		}

		g_free(l->data);
	}

	g_list_free(files);
}

/**************************************************************************
 * Plugins Subsystem API
 **************************************************************************/
void
purple_plugins_init(void)
{
	GPluginManager *manager = NULL;
	const gchar *search_path;

	gplugin_init(GPLUGIN_CORE_FLAGS_NONE);

	search_path = g_getenv("PURPLE_PLUGIN_PATH");
	if (search_path) {
		gchar **paths;
		int i;

		paths = g_strsplit(search_path, G_SEARCHPATH_SEPARATOR_S, 0);
		for (i = 0; paths[i]; ++i) {
			purple_plugins_add_search_path(paths[i]);
		}

		g_strfreev(paths);
	}

	gplugin_manager_add_default_paths();

	if(!g_getenv("PURPLE_PLUGINS_SKIP")) {
		purple_plugins_add_search_path(PURPLE_LIBDIR);
	} else {
		purple_debug_info("plugins", "PURPLE_PLUGINS_SKIP environment variable set, skipping normal plugin paths");
	}

	manager = gplugin_manager_get_instance();
	g_signal_connect(manager, "loading-plugin", G_CALLBACK(plugin_loading_cb),
	                 NULL);
	g_signal_connect(manager, "loaded-plugin", G_CALLBACK(plugin_loaded_cb),
	                 NULL);
	g_signal_connect(manager, "unloading-plugin",
	                 G_CALLBACK(plugin_unloading_cb), NULL);
	g_signal_connect(manager, "unloaded-plugin",
	                 G_CALLBACK(plugin_unloaded_cb), NULL);

	purple_plugins_refresh();
}

void
purple_plugins_uninit(void)
{
	purple_debug_info("plugins", "Unloading all plugins\n");
	while (loaded_plugins != NULL)
		purple_plugin_unload(loaded_plugins->data, NULL);

	gplugin_uninit();
}
