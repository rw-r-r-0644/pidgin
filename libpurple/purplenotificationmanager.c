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

#include "purplenotificationmanager.h"

#include "purpleprivate.h"

enum {
	PROP_ZERO,
	PROP_UNREAD_COUNT,
	N_PROPERTIES,
};
static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
	SIG_ADDED,
	SIG_REMOVED,
	SIG_READ,
	SIG_UNREAD,
	N_SIGNALS,
};
static guint signals[N_SIGNALS] = { 0, };

struct _PurpleNotificationManager {
	GObject parent;

	GHashTable *notifications;

	guint unread_count;
};

G_DEFINE_TYPE(PurpleNotificationManager, purple_notification_manager,
              G_TYPE_OBJECT);

static PurpleNotificationManager *default_manager = NULL;

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
purple_notification_manager_set_unread_count(PurpleNotificationManager *manager,
                                             guint unread_count)
{
	if(manager->unread_count != unread_count) {
		manager->unread_count = unread_count;

		g_object_notify_by_pspec(G_OBJECT(manager),
		                         properties[PROP_UNREAD_COUNT]);
	}
}

static inline void
purple_notification_manager_increment_unread_count(PurpleNotificationManager *manager)
{
	if(manager->unread_count < G_MAXUINT) {
		purple_notification_manager_set_unread_count(manager,
		                                             manager->unread_count + 1);
	}
}

static inline void
purple_notification_manager_decrement_unread_count(PurpleNotificationManager *manager)
{
	if(manager->unread_count > 0) {
		purple_notification_manager_set_unread_count(manager,
		                                             manager->unread_count - 1);
	}
}

/******************************************************************************
 * Callbacks
 *****************************************************************************/
static void
purple_notification_manager_notify_cb(GObject *obj,
                                      G_GNUC_UNUSED GParamSpec *pspec,
                                      gpointer data)
{
	PurpleNotification *notification = PURPLE_NOTIFICATION(obj);
	PurpleNotificationManager *manager = data;
	guint signal_id = 0;

	/* This function is called after the property is changed. So we need to
	 * get the new value to determine how the state changed.
	 */

	if(purple_notification_get_read(notification)) {
		purple_notification_manager_decrement_unread_count(manager);

		signal_id = signals[SIG_READ];
	} else {
		purple_notification_manager_increment_unread_count(manager);

		signal_id = signals[SIG_UNREAD];
	}

	g_signal_emit(manager, signal_id, 0, notification);
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
static void
purple_notification_manager_get_property(GObject *obj, guint param_id,
                                         GValue *value, GParamSpec *pspec)
{
	PurpleNotificationManager *manager = PURPLE_NOTIFICATION_MANAGER(obj);

	switch(param_id) {
		case PROP_UNREAD_COUNT:
			g_value_set_uint(value,
			                 purple_notification_manager_get_unread_count(manager));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_notification_manager_finalize(GObject *obj) {
	PurpleNotificationManager *manager = NULL;

	manager = PURPLE_NOTIFICATION_MANAGER(obj);

	g_clear_pointer(&manager->notifications, g_hash_table_destroy);

	G_OBJECT_CLASS(purple_notification_manager_parent_class)->finalize(obj);
}

static void
purple_notification_manager_init(PurpleNotificationManager *manager) {
	manager->notifications = g_hash_table_new_full(g_str_hash, g_str_equal,
	                                               NULL, g_object_unref);
}

static void
purple_notification_manager_class_init(PurpleNotificationManagerClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->get_property = purple_notification_manager_get_property;
	obj_class->finalize = purple_notification_manager_finalize;

	/* Properties */

	/**
	 * PurpleNotificationManager:unread-count:
	 *
	 * The number of unread notifications in the manager.
	 *
	 * Since: 3.0.0
	 */
	properties[PROP_UNREAD_COUNT] = g_param_spec_uint(
		"unread-count", "unread-count",
		"The number of unread messages in the manager.",
		0, G_MAXUINT, 0,
		G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, N_PROPERTIES, properties);

	/* Signals */

	/**
	 * PurpleNotificationManager::added:
	 * @manager: The instance.
	 * @notification: The [class@Notification] that was added.
	 *
	 * Emitted after @notification has been added to @manager.
	 *
	 * Since: 3.0.0
	 */
	signals[SIG_ADDED] = g_signal_new_class_handler(
		"added",
		G_OBJECT_CLASS_TYPE(klass),
		G_SIGNAL_RUN_LAST,
		NULL,
		NULL,
		NULL,
		NULL,
		G_TYPE_NONE,
		1,
		PURPLE_TYPE_NOTIFICATION);

	/**
	 * PurpleNotificationManager::removed:
	 * @manager: The instance.
	 * @notification: The [class@Notification] that was removed.
	 *
	 * Emitted after @notification has been removed from @manager.
	 *
	 * Since: 3.0.0
	 */
	signals[SIG_REMOVED] = g_signal_new_class_handler(
		"removed",
		G_OBJECT_CLASS_TYPE(klass),
		G_SIGNAL_RUN_LAST,
		NULL,
		NULL,
		NULL,
		NULL,
		G_TYPE_NONE,
		1,
		PURPLE_TYPE_NOTIFICATION);

	/**
	 * PurpleNotificationManager::read:
	 * @manager: The instance.
	 * @notification: The [class@Notification].
	 *
	 * Emitted after @notification has been marked as read.
	 *
	 * Since: 3.0.0
	 */
	signals[SIG_READ] = g_signal_new_class_handler(
		"read",
		G_OBJECT_CLASS_TYPE(klass),
		G_SIGNAL_RUN_LAST,
		NULL,
		NULL,
		NULL,
		NULL,
		G_TYPE_NONE,
		1,
		PURPLE_TYPE_NOTIFICATION);

	/**
	 * PurpleNotificationManager::unread:
	 * @manager: The instance.
	 * @notification: The [class@Notification].
	 *
	 * Emitted after @notification has been marked as unread.
	 *
	 * Since: 3.0.0
	 */
	signals[SIG_UNREAD] = g_signal_new_class_handler(
		"unread",
		G_OBJECT_CLASS_TYPE(klass),
		G_SIGNAL_RUN_LAST,
		NULL,
		NULL,
		NULL,
		NULL,
		G_TYPE_NONE,
		1,
		PURPLE_TYPE_NOTIFICATION);
}

/******************************************************************************
 * Private API
 *****************************************************************************/
void
purple_notification_manager_startup(void) {
	if(default_manager == NULL) {
		default_manager = g_object_new(PURPLE_TYPE_NOTIFICATION_MANAGER, NULL);
	}
}

void
purple_notification_manager_shutdown(void) {
	g_clear_object(&default_manager);
}

/******************************************************************************
 * Public API
 *****************************************************************************/
PurpleNotificationManager *
purple_notification_manager_get_default(void) {
	return default_manager;
}

void
purple_notification_manager_add(PurpleNotificationManager *manager,
                                PurpleNotification *notification)
{
	const gchar *id = NULL;

	g_return_if_fail(PURPLE_IS_NOTIFICATION_MANAGER(manager));
	g_return_if_fail(PURPLE_IS_NOTIFICATION(notification));

	id = purple_notification_get_id(notification);

	if(g_hash_table_lookup(manager->notifications, (gpointer)id) != NULL) {
		g_warning("double add detected for notification %s", id);

		return;
	}

	g_hash_table_insert(manager->notifications, (gpointer)id, notification);

	/* Connect to the notify signal for the read property only so we can
	 * propagate out changes for any notification.
	 */
	g_signal_connect_object(notification, "notify::read",
	                        G_CALLBACK(purple_notification_manager_notify_cb),
	                        manager, 0);

	/* If the notification is not read, we need to increment the unread count.
	 */
	if(!purple_notification_get_read(notification)) {
		purple_notification_manager_increment_unread_count(manager);
	}

	g_signal_emit(G_OBJECT(manager), signals[SIG_ADDED], 0, notification);
}

gboolean
purple_notification_manager_remove(PurpleNotificationManager *manager,
                                   const gchar *id)
{
	gpointer data = NULL;
	gboolean ret = FALSE;

	g_return_val_if_fail(PURPLE_IS_NOTIFICATION_MANAGER(manager), FALSE);
	g_return_val_if_fail(id != NULL, FALSE);

	data = g_hash_table_lookup(manager->notifications, id);
	if(PURPLE_IS_NOTIFICATION(data)) {
		/* Reference the notification so we can emit the signal after it's been
		 * removed from the hash table.
		 */
		g_object_ref(G_OBJECT(data));

		if(g_hash_table_remove(manager->notifications, id)) {
			g_signal_emit(G_OBJECT(manager), signals[SIG_REMOVED], 0,
			              data);

			ret = TRUE;
		}

		/* Remove the notify signal handler for the read state incase someone
		 * else added a reference to the notification which would then mess
		 * with our unread count accounting.
		 */
		g_signal_handlers_disconnect_by_func(data,
		                                     G_CALLBACK(purple_notification_manager_notify_cb),
		                                     manager);

		/* If the notification is not read, we need to decrement the unread
		 * count.
		 */
		if(!purple_notification_get_read(PURPLE_NOTIFICATION(data))) {
			purple_notification_manager_decrement_unread_count(manager);
		}

		g_object_unref(G_OBJECT(data));
	}

	return ret;
}

guint
purple_notification_manager_get_unread_count(PurpleNotificationManager *manager) {
	g_return_val_if_fail(PURPLE_IS_NOTIFICATION_MANAGER(manager), 0);

	return manager->unread_count;
}
