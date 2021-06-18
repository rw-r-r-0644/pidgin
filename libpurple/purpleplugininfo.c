/*
 * Purple - Internet Messaging Library
 * Copyright (C) Pidgin Developers <devel@pidgin.im>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <https://www.gnu.org/licenses/>.
 */
#include <glib/gi18n-lib.h>

#include "internal.h"

#include "core.h"
#include "debug.h"
#include "enums.h"
#include "util.h"

#include "purpleplugininfo.h"

typedef struct {
	gchar *ui_requirement;  /* ID of UI that is required to load the plugin */
	gchar *error;           /* Why a plugin is not loadable                 */

	PurplePluginInfoFlags flags; /* Flags for the plugin */

	/* Callback that returns a list of actions the plugin can perform */
	PurplePluginActionsCb actions_cb;

	/* Callback that returns extra information about a plugin */
	PurplePluginExtraCb extra_cb;

	/* Callback that returns a preferences frame for a plugin */
	PurplePluginPrefFrameCb pref_frame_cb;

	/* Callback that returns a preferences request handle for a plugin */
	PurplePluginPrefRequestCb pref_request_cb;

	/* TRUE if a plugin has been unloaded at least once. Auto-load
	 * plugins that have been unloaded once will not be auto-loaded again. */
	gboolean unloaded;
} PurplePluginInfoPrivate;

enum {
	PROP_0,
	PROP_UI_REQUIREMENT,
	PROP_ACTIONS_CB,
	PROP_EXTRA_CB,
	PROP_PREF_FRAME_CB,
	PROP_PREF_REQUEST_CB,
	PROP_FLAGS,
	N_PROPERTIES,
};
static GParamSpec *properties[N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE(PurplePluginInfo, purple_plugin_info,
                           GPLUGIN_TYPE_PLUGIN_INFO);

/**************************************************************************
 * GObject Implementation
 **************************************************************************/
static void
purple_plugin_info_init(PurplePluginInfo *info) {
}

static void
purple_plugin_info_set_property(GObject *obj, guint param_id,
                                const GValue *value, GParamSpec *pspec)
{
	PurplePluginInfo *info = PURPLE_PLUGIN_INFO(obj);
	PurplePluginInfoPrivate *priv = NULL;

	priv = purple_plugin_info_get_instance_private(info);

	switch (param_id) {
		case PROP_UI_REQUIREMENT:
			priv->ui_requirement = g_value_dup_string(value);
			break;
		case PROP_ACTIONS_CB:
			priv->actions_cb = g_value_get_pointer(value);
			break;
		case PROP_EXTRA_CB:
			priv->extra_cb = g_value_get_pointer(value);
			break;
		case PROP_PREF_FRAME_CB:
			priv->pref_frame_cb = g_value_get_pointer(value);
			break;
		case PROP_PREF_REQUEST_CB:
			priv->pref_request_cb = g_value_get_pointer(value);
			break;
		case PROP_FLAGS:
			priv->flags = g_value_get_flags(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_plugin_info_get_property(GObject *obj, guint param_id, GValue *value,
                                GParamSpec *pspec)
{
	PurplePluginInfo *info = PURPLE_PLUGIN_INFO(obj);

	switch (param_id) {
		case PROP_ACTIONS_CB:
			g_value_set_pointer(value,
					purple_plugin_info_get_actions_cb(info));
			break;
		case PROP_EXTRA_CB:
			g_value_set_pointer(value,
					purple_plugin_info_get_extra_cb(info));
			break;
		case PROP_PREF_FRAME_CB:
			g_value_set_pointer(value,
					purple_plugin_info_get_pref_frame_cb(info));
			break;
		case PROP_PREF_REQUEST_CB:
			g_value_set_pointer(value,
					purple_plugin_info_get_pref_request_cb(info));
			break;
		case PROP_FLAGS:
			g_value_set_flags(value, purple_plugin_info_get_flags(info));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_plugin_info_constructed(GObject *object) {
	PurplePluginInfo *info = PURPLE_PLUGIN_INFO(object);
	GPluginPluginInfo *ginfo = GPLUGIN_PLUGIN_INFO(info);
	PurplePluginInfoPrivate *priv = NULL;
	const gchar *id = gplugin_plugin_info_get_id(ginfo);
	guint32 version;

	priv = purple_plugin_info_get_instance_private(info);

	G_OBJECT_CLASS(purple_plugin_info_parent_class)->constructed(object);

	if(id == NULL || *id == '\0') {
		priv->error = g_strdup(_("This plugin has not defined an ID."));
	}

	if(priv->ui_requirement != NULL) {
		if(!purple_strequal(priv->ui_requirement, purple_core_get_ui())) {
			priv->error = g_strdup_printf(_("You are using %s, but this plugin "
			                                "requires %s."),
			                              purple_core_get_ui(),
			                              priv->ui_requirement);
			purple_debug_error("plugins",
			                   "%s is not loadable: The UI requirement is not "
			                   "met. (%s)\n",
			                   id, priv->error);
		}
	}

	version = gplugin_plugin_info_get_abi_version(ginfo);
	if (PURPLE_PLUGIN_ABI_MAJOR_VERSION(version) != PURPLE_MAJOR_VERSION ||
		PURPLE_PLUGIN_ABI_MINOR_VERSION(version) > PURPLE_MINOR_VERSION)
	{
		priv->error = g_strdup_printf(_("Your libpurple version is %d.%d.x "
		                                "(need %d.%d.x)"),
		                              PURPLE_MAJOR_VERSION,
		                              PURPLE_MINOR_VERSION,
		                              PURPLE_PLUGIN_ABI_MAJOR_VERSION(version),
		                              PURPLE_PLUGIN_ABI_MINOR_VERSION(version));
		purple_debug_error("plugins",
		                   "%s is not loadable: libpurple version is %d.%d.x "
		                   "(need %d.%d.x)\n",
		                   id, PURPLE_MAJOR_VERSION, PURPLE_MINOR_VERSION,
		                   PURPLE_PLUGIN_ABI_MAJOR_VERSION(version),
		                   PURPLE_PLUGIN_ABI_MINOR_VERSION(version));
	}
}

static void
purple_plugin_info_finalize(GObject *object) {
	PurplePluginInfoPrivate *priv = NULL;

	priv = purple_plugin_info_get_instance_private(PURPLE_PLUGIN_INFO(object));

	g_free(priv->ui_requirement);
	g_free(priv->error);

	G_OBJECT_CLASS(purple_plugin_info_parent_class)->finalize(object);
}

static void
purple_plugin_info_class_init(PurplePluginInfoClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->constructed = purple_plugin_info_constructed;
	obj_class->finalize    = purple_plugin_info_finalize;

	obj_class->get_property = purple_plugin_info_get_property;
	obj_class->set_property = purple_plugin_info_set_property;

	properties[PROP_UI_REQUIREMENT] = g_param_spec_string(
		"ui-requirement", "UI Requirement",
		"ID of UI that is required by this plugin",
		NULL,
		G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);

	properties[PROP_ACTIONS_CB] = g_param_spec_pointer(
		"actions-cb", "Plugin actions",
		"Callback that returns list of plugin's actions",
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

	properties[PROP_EXTRA_CB] = g_param_spec_pointer(
		"extra-cb", "Extra info callback",
		"Callback that returns extra info about the plugin",
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

	properties[PROP_PREF_FRAME_CB] = g_param_spec_pointer(
		"pref-frame-cb", "Preferences frame callback",
		"The callback that returns the preferences frame",
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

	properties[PROP_PREF_REQUEST_CB] = g_param_spec_pointer(
		"pref-request-cb", "Preferences request callback",
		"Callback that returns preferences request handle",
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

	properties[PROP_FLAGS] = g_param_spec_flags(
		"flags", "Plugin flags",
		"The flags for the plugin",
		PURPLE_TYPE_PLUGIN_INFO_FLAGS,
		0,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, N_PROPERTIES, properties);
}

/**************************************************************************
 * Public API
 **************************************************************************/
GPluginPluginInfo *
purple_plugin_info_new(const char *first_property, ...) {
	GObject *info;
	va_list var_args;

	/* at least ID is required */
	if (!first_property) {
		return NULL;
	}

	va_start(var_args, first_property);
	info = g_object_new_valist(PURPLE_TYPE_PLUGIN_INFO, first_property,
	                           var_args);
	va_end(var_args);

	return GPLUGIN_PLUGIN_INFO(info);
}

PurplePluginActionsCb
purple_plugin_info_get_actions_cb(PurplePluginInfo *info) {
	PurplePluginInfoPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_PLUGIN_INFO(info), NULL);

	priv = purple_plugin_info_get_instance_private(info);

	return priv->actions_cb;
}

PurplePluginExtraCb
purple_plugin_info_get_extra_cb(PurplePluginInfo *info) {
	PurplePluginInfoPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_PLUGIN_INFO(info), NULL);

	priv = purple_plugin_info_get_instance_private(info);

	return priv->extra_cb;
}

PurplePluginPrefFrameCb
purple_plugin_info_get_pref_frame_cb(PurplePluginInfo *info) {
	PurplePluginInfoPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_PLUGIN_INFO(info), NULL);

	priv = purple_plugin_info_get_instance_private(info);

	return priv->pref_frame_cb;
}

PurplePluginPrefRequestCb
purple_plugin_info_get_pref_request_cb(PurplePluginInfo *info) {
	PurplePluginInfoPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_PLUGIN_INFO(info), NULL);

	priv = purple_plugin_info_get_instance_private(info);

	return priv->pref_request_cb;
}

PurplePluginInfoFlags
purple_plugin_info_get_flags(PurplePluginInfo *info) {
	PurplePluginInfoPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_PLUGIN_INFO(info), 0);

	priv = purple_plugin_info_get_instance_private(info);

	return priv->flags;
}

const gchar *
purple_plugin_info_get_error(PurplePluginInfo *info) {
	PurplePluginInfoPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_PLUGIN_INFO(info), NULL);

	priv = purple_plugin_info_get_instance_private(info);

	return priv->error;
}

gboolean
purple_plugin_info_get_unloaded(PurplePluginInfo *info) {
	PurplePluginInfoPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_PLUGIN_INFO(info), FALSE);

	priv = purple_plugin_info_get_instance_private(info);

	return priv->unloaded;
}

void
purple_plugin_info_set_unloaded(PurplePluginInfo *info, gboolean unloaded) {
	PurplePluginInfoPrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_PLUGIN_INFO(info));

	priv = purple_plugin_info_get_instance_private(info);

	priv->unloaded = unloaded;
}
