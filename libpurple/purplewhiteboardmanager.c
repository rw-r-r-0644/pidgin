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

#include "purplewhiteboardmanager.h"
#include "purpleprivate.h"

enum {
	SIG_REGISTERED,
	SIG_UNREGISTERED,
	N_SIGNALS,
};
static guint signals[N_SIGNALS] = {0, };

typedef struct {
	GListStore *store;
} PurpleWhiteboardManagerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(PurpleWhiteboardManager, purple_whiteboard_manager,
                           G_TYPE_OBJECT);

static PurpleWhiteboardManager *default_manager = NULL;

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
static void
purple_whiteboard_manager_finalize(GObject *obj) {
	PurpleWhiteboardManager *manager = NULL;
	PurpleWhiteboardManagerPrivate *priv = NULL;

	manager = PURPLE_WHITEBOARD_MANAGER(obj);
	priv = purple_whiteboard_manager_get_instance_private(manager);

	g_clear_object(&priv->store);

	G_OBJECT_CLASS(purple_whiteboard_manager_parent_class)->finalize(obj);
}

static void
purple_whiteboard_manager_init(PurpleWhiteboardManager *manager) {
	PurpleWhiteboardManagerPrivate *priv = NULL;

	priv = purple_whiteboard_manager_get_instance_private(manager);

	priv->store = g_list_store_new(PURPLE_TYPE_WHITEBOARD);
}

static void
purple_whiteboard_manager_class_init(PurpleWhiteboardManagerClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->finalize = purple_whiteboard_manager_finalize;

	/**
	 * PurpleWhiteboardManager::registered:
	 * @manager: The #PurpleWhiteboardManager instance.
	 * @whiteboard: The #PurpleWhiteboard that was registered.
	 *
	 * Emitted after @whiteboard has been registered in @manager.
	 *
	 * Since: 3.0.0
	 */
	signals[SIG_REGISTERED] = g_signal_new(
		"registered",
		G_OBJECT_CLASS_TYPE(klass),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET(PurpleWhiteboardManagerClass, registered),
		NULL,
		NULL,
		NULL,
		G_TYPE_NONE,
		1,
		PURPLE_TYPE_WHITEBOARD);

	/**
	 * PurpleWhiteboardManager::unregistered:
	 * @manager: The #PurpleWhiteboardManager instance.
	 * @whiteboard: The #PurpleWhiteboard that was unregistered.
	 *
	 * Emitted after @whiteboard has been unregistered from @manager.
	 *
	 * Since: 3.0.0
	 */
	signals[SIG_UNREGISTERED] = g_signal_new(
		"unregistered",
		G_OBJECT_CLASS_TYPE(klass),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET(PurpleWhiteboardManagerClass, unregistered),
		NULL,
		NULL,
		NULL,
		G_TYPE_NONE,
		1,
		PURPLE_TYPE_WHITEBOARD);
}

/******************************************************************************
 * Private API
 *****************************************************************************/
void
purple_whiteboard_manager_startup(void) {
	if(default_manager == NULL) {
		default_manager = g_object_new(PURPLE_TYPE_WHITEBOARD_MANAGER, NULL);
	}
}

void
purple_whiteboard_manager_shutdown(void) {
	g_clear_object(&default_manager);
}

/******************************************************************************
 * Public API
 *****************************************************************************/
PurpleWhiteboardManager *
purple_whiteboard_manager_get_default(void) {
	return default_manager;
}

gboolean
purple_whiteboard_manager_register(PurpleWhiteboardManager *manager,
                                   PurpleWhiteboard *whiteboard,
                                   GError **error)
{
	PurpleWhiteboardManagerPrivate *priv = NULL;
	gboolean found = FALSE;

	g_return_val_if_fail(PURPLE_IS_WHITEBOARD_MANAGER(manager), FALSE);
	g_return_val_if_fail(PURPLE_IS_WHITEBOARD(whiteboard), FALSE);

	priv = purple_whiteboard_manager_get_instance_private(manager);

	found = g_list_store_find_with_equal_func(priv->store, whiteboard,
	                                          (GEqualFunc)purple_whiteboard_equal,
	                                          NULL);

	if(found) {
		g_set_error(error, PURPLE_WHITEBOARD_MANAGER_DOMAIN, 0,
		            _("whiteboard %s is already registered"),
		            purple_whiteboard_get_id(whiteboard));

		return FALSE;
	}

	g_list_store_insert(priv->store, 0, whiteboard);

	g_signal_emit(G_OBJECT(manager), signals[SIG_REGISTERED], 0, whiteboard);

	return TRUE;
}

gboolean
purple_whiteboard_manager_unregister(PurpleWhiteboardManager *manager,
                                     PurpleWhiteboard *whiteboard,
                                     GError **error)
{
	PurpleWhiteboardManagerPrivate *priv = NULL;
	guint position = 0;
	gboolean found = FALSE;

	g_return_val_if_fail(PURPLE_IS_WHITEBOARD_MANAGER(manager), FALSE);
	g_return_val_if_fail(PURPLE_IS_WHITEBOARD(whiteboard), FALSE);

	priv = purple_whiteboard_manager_get_instance_private(manager);

	found = g_list_store_find_with_equal_func(priv->store, whiteboard,
	                                          (GEqualFunc)purple_whiteboard_equal,
	                                          &position);

	if(!found) {
		g_set_error(error, PURPLE_WHITEBOARD_MANAGER_DOMAIN, 0,
		            _("whiteboard %s is not registered"),
		            purple_whiteboard_get_id(whiteboard));

		return FALSE;
	}

	/* Temporarily ref whiteboard so we can pass it along to the signal
	 * callbacks.
	 */
	g_object_ref(G_OBJECT(whiteboard));

	g_list_store_remove(priv->store, position);

	g_signal_emit(G_OBJECT(manager), signals[SIG_UNREGISTERED], 0, whiteboard);

	g_object_unref(G_OBJECT(whiteboard));

	return TRUE;
}

PurpleWhiteboard *
purple_whiteboard_manager_find(PurpleWhiteboardManager *manager,
                               const gchar *id)
{
	PurpleWhiteboardManagerPrivate *priv = NULL;
	guint idx, n;

	g_return_val_if_fail(PURPLE_IS_WHITEBOARD_MANAGER(manager), NULL);
	g_return_val_if_fail(id != NULL, NULL);

	priv = purple_whiteboard_manager_get_instance_private(manager);

	n = g_list_model_get_n_items(G_LIST_MODEL(priv->store));
	for(idx = 0; idx < n; idx++) {
		GObject *obj = NULL;

		obj = g_list_model_get_object(G_LIST_MODEL(priv->store), idx);
		if(PURPLE_IS_WHITEBOARD(obj)) {
			PurpleWhiteboard *whiteboard = PURPLE_WHITEBOARD(obj);

			if(purple_strequal(id, purple_whiteboard_get_id(whiteboard))) {
				return whiteboard;
			}
		}
		g_clear_object(&obj);
	}

	return NULL;
}

void
purple_whiteboard_manager_foreach(PurpleWhiteboardManager *manager,
                                  PurpleWhiteboardManagerForeachFunc func,
                                  gpointer data)
{
	PurpleWhiteboardManagerPrivate *priv = NULL;
	guint idx, n;

	g_return_if_fail(PURPLE_IS_WHITEBOARD_MANAGER(manager));
	g_return_if_fail(func != NULL);

	priv = purple_whiteboard_manager_get_instance_private(manager);

	n = g_list_model_get_n_items(G_LIST_MODEL(priv->store));
	for(idx = 0; idx < n; idx++) {
		gpointer item = g_list_model_get_item(G_LIST_MODEL(priv->store), idx);
		func(item, data);
		g_object_unref(item);
	}
}

GListModel *
purple_whiteboard_manager_get_model(PurpleWhiteboardManager *manager) {
	PurpleWhiteboardManagerPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_WHITEBOARD_MANAGER(manager), NULL);

	priv = purple_whiteboard_manager_get_instance_private(manager);

	return G_LIST_MODEL(priv->store);
}
