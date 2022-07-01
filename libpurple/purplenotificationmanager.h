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

#if !defined(PURPLE_GLOBAL_HEADER_INSIDE) && !defined(PURPLE_COMPILATION)
# error "only <purple.h> may be included directly"
#endif

#ifndef PURPLE_NOTIFICATION_MANAGER_H
#define PURPLE_NOTIFICATION_MANAGER_H

#include <glib.h>
#include <glib-object.h>

#include "account.h"
#include <purplenotification.h>

G_BEGIN_DECLS

#define PURPLE_TYPE_NOTIFICATION_MANAGER (purple_notification_manager_get_type())
G_DECLARE_FINAL_TYPE(PurpleNotificationManager, purple_notification_manager,
                     PURPLE, NOTIFICATION_MANAGER, GObject)

/**
 * PurpleNotificationManager:
 *
 * Purple Notification Manager manages all notifications between protocols and
 * plugins and how the user interface interacts with them.
 *
 * Since: 3.0.0
 */

/**
 * purple_notification_manager_get_default:
 *
 * Gets the default [class@NotificationManager] instance.
 *
 * Returns: (transfer none): The default instance.
 *
 * Since: 3.0.0
 */
PurpleNotificationManager *purple_notification_manager_get_default(void);

/**
 * purple_notification_manager_add:
 * @manager: The instance.
 * @notification: (transfer full): The [class@Notification] to add.
 *
 * Adds @notification into @manager.
 *
 * Since: 3.0.0
 */
void purple_notification_manager_add(PurpleNotificationManager *manager, PurpleNotification *notification);

/**
 * purple_notification_manager_remove:
 * @manager: The instance.
 * @notification: The notification to remove.
 *
 * Removes @notification from @manager.
 *
 * Since: 3.0.0
 */
void purple_notification_manager_remove(PurpleNotificationManager *manager, PurpleNotification *notification);

/**
 * purple_notification_manager_get_unread_count:
 * @manager: The instance.
 *
 * Gets the number of currently unread notifications.
 *
 * Returns: The number of unread notifications.
 *
 * Since: 3.0.0
 */
guint purple_notification_manager_get_unread_count(PurpleNotificationManager *manager);

G_END_DECLS

#endif /* PURPLE_NOTIFICATION_MANAGER_H */
