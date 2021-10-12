/*
 * Purple - Internet Messaging Library
 * Copyright (C) Pidgin Developers <devel@pidgin.im>
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

#include <glib/gi18n-lib.h>

#include "purplehistorymanager.h"
#include "purplehistoryadapter.h"
#include "purplesqlitehistoryadapter.h"

#include "purpleprivate.h"
#include "debug.h"
#include "util.h"

enum {
	SIG_ACTIVE_CHANGED,
	SIG_REGISTERED,
	SIG_UNREGISTERED,
	N_SIGNALS,
};
static guint signals[N_SIGNALS] = {0, };

typedef struct {
	GHashTable *adapters;
	PurpleHistoryAdapter *active_adapter;
} PurpleHistoryManagerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(PurpleHistoryManager, purple_history_manager,
                           G_TYPE_OBJECT);

static PurpleHistoryManager *default_manager = NULL;

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
static void
purple_history_manager_finalize(GObject *obj) {
	PurpleHistoryManager *manager = NULL;
	PurpleHistoryManagerPrivate *priv = NULL;

	manager = PURPLE_HISTORY_MANAGER(obj);
	priv = purple_history_manager_get_instance_private(manager);

	g_clear_pointer(&priv->adapters, g_hash_table_destroy);

	G_OBJECT_CLASS(purple_history_manager_parent_class)->finalize(obj);
}

static void
purple_history_manager_init(PurpleHistoryManager *manager) {
	PurpleHistoryManagerPrivate *priv = NULL;

	priv = purple_history_manager_get_instance_private(manager);

	priv->adapters = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
	                                       g_object_unref);
}

static void
purple_history_manager_class_init(PurpleHistoryManagerClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->finalize = purple_history_manager_finalize;

	/**
	* PurpleHistoryManager::adapter-changed:
	* @manager: The #PurpleHistoryManager instance.
	* @old: The old #PurpleHistoryAdapter.
	* @current: The new activated #PurpleHistoryAdapter.
	*
	* Emitted after @adapter has been changed for @manager.
	*
	* Since: 3.0.0
	*/
	signals[SIG_ACTIVE_CHANGED] = g_signal_new(
		"active-changed",
		G_OBJECT_CLASS_TYPE(klass),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET(PurpleHistoryManagerClass, active_changed),
		NULL,
		NULL,
		NULL,
		G_TYPE_NONE,
		2,
		PURPLE_TYPE_HISTORY_ADAPTER,
		PURPLE_TYPE_HISTORY_ADAPTER);

	/**
	 * PurpleHistoryManager::registered:
	 * @manager: The #PurpleHistoryManager instance.
	 * @adapter: The #PurpleHistoryAdapter that was registered.
	 *
	 * Emitted after @adapter has been registered in @manager.
	 *
	 * Since: 3.0.0
	 */
	signals[SIG_REGISTERED] = g_signal_new(
		"registered",
		G_OBJECT_CLASS_TYPE(klass),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET(PurpleHistoryManagerClass, registered),
		NULL,
		NULL,
		NULL,
		G_TYPE_NONE,
		1,
		PURPLE_TYPE_HISTORY_ADAPTER);

	/**
	 * PurpleHistoryManager::unregistered:
	 * @manager: The #PurpleHistoryManager instance.
	 * @adapter: The #PurpleHistoryAdapter that was unregistered.
	 *
	 * Emitted after @adapter has been unregistered for @manager.
	 *
	 * Since: 3.0.0
	 */
	signals[SIG_UNREGISTERED] = g_signal_new(
		"unregistered",
		G_OBJECT_CLASS_TYPE(klass),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET(PurpleHistoryManagerClass, unregistered),
		NULL,
		NULL,
		NULL,
		G_TYPE_NONE,
		1,
		PURPLE_TYPE_HISTORY_ADAPTER);
}

/******************************************************************************
 * Private API
 *****************************************************************************/
void
purple_history_manager_startup(void) {
	if(default_manager == NULL) {
		PurpleHistoryAdapter *adapter = NULL;
		GError *error = NULL;
		gchar *filename = NULL;

		filename = g_build_filename(purple_config_dir(), "history.db", NULL);
		adapter = purple_sqlite_history_adapter_new(filename);
		g_free(filename);

		default_manager = g_object_new(PURPLE_TYPE_HISTORY_MANAGER, NULL);
		if(!purple_history_manager_register(default_manager, adapter, &error)) {
			if(error != NULL) {
				g_warning("Failed to register sqlite history adapter: %s", error->message);
				g_clear_error(&error);
			} else {
				g_warning("Failed to register sqlite history adapter: Unknown reason");
			}

			g_clear_object(&adapter);

			return;
		}

		purple_history_manager_set_active(default_manager,
		                                  purple_history_adapter_get_id(adapter),
		                                  &error);

		if(error != NULL) {
			g_warning("Failed to activate %s: %s",
			          purple_history_adapter_get_id(adapter), error->message);

			g_clear_error(&error);
		}

		g_clear_object(&adapter);
	}
}

void
purple_history_manager_shutdown(void) {
	PurpleHistoryManagerPrivate *priv = NULL;
	GError **error = NULL;

	if(default_manager == NULL) {
		return;
	}

	priv = purple_history_manager_get_instance_private(default_manager);
	if(PURPLE_IS_HISTORY_ADAPTER(priv->active_adapter)) {
		PurpleHistoryAdapter *adapter = NULL;

		adapter = g_object_ref(priv->active_adapter);
		purple_history_manager_set_active(default_manager, NULL, NULL);
		purple_history_manager_unregister(default_manager,
		                                  adapter, error);

		g_clear_object(&adapter);
	}

	g_clear_object(&default_manager);
}

/******************************************************************************
 * Public API
 *****************************************************************************/
PurpleHistoryManager *
purple_history_manager_get_default(void) {
	return default_manager;
}

gboolean
purple_history_manager_register(PurpleHistoryManager *manager,
                                PurpleHistoryAdapter *adapter,
                                GError **error)
{
	PurpleHistoryManagerPrivate *priv = NULL;
	const gchar *id = NULL;

	g_return_val_if_fail(PURPLE_IS_HISTORY_MANAGER(manager), FALSE);
	g_return_val_if_fail(PURPLE_IS_HISTORY_ADAPTER(adapter), FALSE);

	priv = purple_history_manager_get_instance_private(manager);

	id = purple_history_adapter_get_id(adapter);
	if(g_hash_table_lookup(priv->adapters, id) != NULL) {
		g_set_error(error, PURPLE_HISTORY_MANAGER_DOMAIN, 0,
		            _("adapter %s is already registered"), id);

		return FALSE;
	}

	g_hash_table_insert(priv->adapters, g_strdup(id), g_object_ref(adapter));

	g_signal_emit(G_OBJECT(manager), signals[SIG_REGISTERED], 0, adapter);

	return TRUE;
}

gboolean
purple_history_manager_unregister(PurpleHistoryManager *manager,
                                  PurpleHistoryAdapter *adapter,
                                  GError **error)
{
	PurpleHistoryManagerPrivate *priv = NULL;
	const gchar *id = NULL;
	gboolean ret = FALSE;

	g_return_val_if_fail(PURPLE_IS_HISTORY_MANAGER(manager), FALSE);
	g_return_val_if_fail(PURPLE_IS_HISTORY_ADAPTER(adapter), FALSE);

	priv = purple_history_manager_get_instance_private(manager);

	if(adapter == priv->active_adapter) {
		g_set_error(error, PURPLE_HISTORY_MANAGER_DOMAIN, 0,
		            _("adapter %s is currently in use"), id);

		return FALSE;
	}

	g_object_ref(G_OBJECT(adapter));

	id = purple_history_adapter_get_id(adapter);

	if(g_hash_table_remove(priv->adapters, id)) {
		g_signal_emit(G_OBJECT(manager), signals[SIG_UNREGISTERED], 0,
		              adapter);

		ret = TRUE;
	} else {
		g_set_error(error, PURPLE_HISTORY_MANAGER_DOMAIN, 0,
		            _("adapter %s is not registered"), id);

		ret = FALSE;
	}

	g_object_unref(G_OBJECT(adapter));

	return ret;
}

PurpleHistoryAdapter *
purple_history_manager_find(PurpleHistoryManager *manager, const gchar *id) {
	PurpleHistoryManagerPrivate *priv = NULL;
	gpointer value = NULL;

	g_return_val_if_fail(PURPLE_IS_HISTORY_MANAGER(manager), NULL);
	g_return_val_if_fail(id != NULL, NULL);

	priv = purple_history_manager_get_instance_private(manager);

	value = g_hash_table_lookup(priv->adapters, id);
	if(value == NULL) {
		return NULL;
	}

	return PURPLE_HISTORY_ADAPTER(value);
}

GList *
purple_history_manager_get_all(PurpleHistoryManager *manager) {
	PurpleHistoryManagerPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_HISTORY_MANAGER(manager), NULL);

	priv = purple_history_manager_get_instance_private(manager);

	return g_hash_table_get_values(priv->adapters);
}

PurpleHistoryAdapter *
purple_history_manager_get_active(PurpleHistoryManager *manager) {
	PurpleHistoryManagerPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_HISTORY_MANAGER(manager), NULL);

	priv = purple_history_manager_get_instance_private(manager);

	return priv->active_adapter;
}

gboolean
purple_history_manager_set_active(PurpleHistoryManager *manager,
                                  const gchar *id,
                                  GError **error)
{
	PurpleHistoryManagerPrivate *priv = NULL;
	PurpleHistoryAdapter *old = NULL, *adapter = NULL;

	g_return_val_if_fail(PURPLE_IS_HISTORY_MANAGER(manager), FALSE);

	priv = purple_history_manager_get_instance_private(manager);

	/* First look up the new adapter if we're given one. */
	if(id != NULL) {
		adapter = g_hash_table_lookup(priv->adapters, id);
		if(!PURPLE_IS_HISTORY_ADAPTER(adapter)) {
			g_set_error(error, PURPLE_HISTORY_MANAGER_DOMAIN, 0,
			            "no history adapter found with id %s", id);

			return FALSE;
		}
	}

	if(PURPLE_IS_HISTORY_ADAPTER(priv->active_adapter)) {
		old = g_object_ref(priv->active_adapter);
	}

	if(g_set_object(&priv->active_adapter, adapter)) {
		if(PURPLE_IS_HISTORY_ADAPTER(old)) {
			if(!purple_history_adapter_deactivate(old, error)) {
				g_set_object(&priv->active_adapter, old);
				g_clear_object(&old);
				return FALSE;
			}
		}

		if(PURPLE_IS_HISTORY_ADAPTER(adapter)) {
			if(!purple_history_adapter_activate(adapter, error)) {
				if(PURPLE_IS_HISTORY_ADAPTER(old)) {
					purple_history_adapter_activate(old, error);
				}

				g_set_object(&priv->active_adapter, old);
				g_clear_object(&old);

				return FALSE;
			}
		}

		g_signal_emit(G_OBJECT(manager), signals[SIG_ACTIVE_CHANGED], 0, old,
		              priv->active_adapter);
	}

	g_clear_object(&old);

	purple_debug_info("history-manager", "set active adapter to '%s'", id);

	return TRUE;
}

GList *
purple_history_manager_query(PurpleHistoryManager *manager,
                             const gchar *query,
                             GError **error)
{
	PurpleHistoryManagerPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_HISTORY_MANAGER(manager), FALSE);

	priv = purple_history_manager_get_instance_private(manager);

	if(priv->active_adapter == NULL) {
		g_set_error_literal(error, PURPLE_HISTORY_MANAGER_DOMAIN, 0,
		                    _("no active history adapter"));
		return FALSE;
	}

	return purple_history_adapter_query(priv->active_adapter, query, error);
}

gboolean
purple_history_manager_remove(PurpleHistoryManager *manager,
                              const gchar *query,
                              GError **error)
{
	PurpleHistoryManagerPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_HISTORY_MANAGER(manager), FALSE);

	priv = purple_history_manager_get_instance_private(manager);

	if(priv->active_adapter == NULL) {
		g_set_error_literal(error, PURPLE_HISTORY_MANAGER_DOMAIN, 0,
		                    _("no active history adapter"));
		return FALSE;
	}

	return purple_history_adapter_remove(priv->active_adapter, query, error);
}

gboolean
purple_history_manager_write(PurpleHistoryManager *manager,
                             PurpleConversation *conversation,
                             PurpleMessage *message,
                             GError **error)
{
	PurpleHistoryManagerPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_CONVERSATION(conversation), FALSE);
	g_return_val_if_fail(PURPLE_IS_MESSAGE(message), FALSE);
	g_return_val_if_fail(PURPLE_IS_HISTORY_MANAGER(manager), FALSE);

	priv = purple_history_manager_get_instance_private(manager);

	if(priv->active_adapter == NULL) {
		g_set_error_literal(error, PURPLE_HISTORY_MANAGER_DOMAIN, 0,
		                    _("no active history adapter"));
		return FALSE;
	}

	return purple_history_adapter_write(priv->active_adapter, conversation,
	                                    message, error);
}

void
purple_history_manager_foreach(PurpleHistoryManager *manager,
                               PurpleHistoryManagerForeachFunc func,
                               gpointer data)
{
	GHashTableIter iter;
	PurpleHistoryManagerPrivate *priv = NULL;
	gpointer value;

	g_return_if_fail(PURPLE_IS_HISTORY_MANAGER(manager));
	g_return_if_fail(func != NULL);

	priv = purple_history_manager_get_instance_private(manager);

	g_hash_table_iter_init(&iter, priv->adapters);
	while (g_hash_table_iter_next(&iter, NULL, &value)) {
		func(PURPLE_HISTORY_ADAPTER(value), data);
	}
}
