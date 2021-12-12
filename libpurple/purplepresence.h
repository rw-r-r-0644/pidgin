/*
 * Purple - Internet Messaging Library
 * Copyright (C) Pidgin Developers <devel@pidgin.im>
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
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#if !defined(PURPLE_GLOBAL_HEADER_INSIDE) && !defined(PURPLE_COMPILATION)
# error "only <purple.h> may be included directly"
#endif

#ifndef PURPLE_PRESENCE_H
#define PURPLE_PRESENCE_H

/**
 * PurplePresence:
 *
 * A PurplePresence is like a collection of PurpleStatuses (plus some other
 * random info).  For any buddy, or for any one of your accounts, or for any
 * person with which you're chatting, you may know various amounts of
 * information.  This information is all contained in one PurplePresence.  If
 * one of your buddies is away and idle, then the presence contains the
 * #PurpleStatus for their awayness, and it contains their current idle time.
 * #PurplePresence's are never saved to disk.  The information they contain is
 * only relevant for the current Purple session.
 *
 * Note: When a presence is destroyed with the last g_object_unref(), all
 * statuses added to this list will be destroyed along with the presence.
 */

typedef struct _PurplePresence PurplePresence;

#include "status.h"

G_BEGIN_DECLS

/**
 * PurplePresenceClass:
 * @update_idle: Updates the logs and the UI when the idle state or time of the
 *               presence changes.
 *
 * The base class for all #PurplePresence's.
 */
struct _PurplePresenceClass {
	/*< private >*/
	GObjectClass parent;

	/*< public >*/
	void (*update_idle)(PurplePresence *presence, gboolean old_idle);
	GList *(*get_statuses)(PurplePresence *presence);

	/*< private >*/
	gpointer reserved[4];
};

/**
 * PURPLE_TYPE_PRESENCE:
 *
 * The standard _get_type macro for #PurplePresence.
 */
#define PURPLE_TYPE_PRESENCE purple_presence_get_type()

/**
 * purple_presence_get_type:
 *
 * Returns: The #GType for the #PurplePresence object.
 */

G_DECLARE_DERIVABLE_TYPE(PurplePresence, purple_presence, PURPLE, PRESENCE,
                         GObject)

/**
 * purple_presence_set_status_active:
 * @presence: The #PurplePresence instance.
 * @status_id: The ID of the status.
 * @active: The active state.
 *
 * Sets the active state of a status in a presence.
 *
 * Only independent statuses can be set inactive. Normal statuses can only
 * be set active, so if you wish to disable a status, set another
 * non-independent status to active, or use purple_presence_switch_status().
 */
void purple_presence_set_status_active(PurplePresence *presence, const gchar *status_id, gboolean active);

/**
 * purple_presence_switch_status:
 * @presence: The #PurplePresence instance.
 * @status_id: The status ID to switch to.
 *
 * Switches the active status in a presence.
 *
 * This is similar to purple_presence_set_status_active(), except it won't
 * activate independent statuses.
 */
void purple_presence_switch_status(PurplePresence *presence, const gchar *status_id);

/**
 * purple_presence_set_idle:
 * @presence: The #PurplePresence instance.
 * @idle: The idle state.
 * @idle_time: The idle time, if @idle is %TRUE.  This is the time at which the
 *             user became idle, in seconds since the epoch.  If this value is
 *             unknown then 0 should be used.
 *
 * Sets the idle state and time of @presence.
 */
void purple_presence_set_idle(PurplePresence *presence, gboolean idle, time_t idle_time);

/**
 * purple_presence_set_login_time:
 * @presence: The #PurplePresence instance.
 * @login_time: The login time.
 *
 * Sets the login time on a presence.
 */
void purple_presence_set_login_time(PurplePresence *presence, time_t login_time);

/**
 * purple_presence_get_statuses:
 * @presence: The #PurplePresence instance.
 *
 * Gets a list of all the statuses in @presence.
 *
 * Returns: (element-type PurpleStatus) (transfer none): The statuses.
 */
GList *purple_presence_get_statuses(PurplePresence *presence);

/**
 * purple_presence_get_status:
 * @presence: The #PurplePresence instance.
 * @status_id: The ID of the status.
 *
 * Gets the status with the specified ID from @presence.
 *
 * Returns: (transfer none): The #PurpleStatus if found, or %NULL.
 */
PurpleStatus *purple_presence_get_status(PurplePresence *presence, const gchar *status_id);

/**
 * purple_presence_get_active_status:
 * @presence: The #PurplePresence instance.
 *
 * Gets the active exclusive status from @presence.
 *
 * Returns: (transfer none): The active exclusive status.
 */
PurpleStatus *purple_presence_get_active_status(PurplePresence *presence);

/**
 * purple_presence_is_available:
 * @presence: The #PurplePresence instance.
 *
 * Gets whether or not @presence is available.
 *
 * Available presences are online and possibly invisible, but not away or idle.
 *
 * Returns: %TRUE if the presence is available, or %FALSE otherwise.
 */
gboolean purple_presence_is_available(PurplePresence *presence);

/**
 * purple_presence_is_online:
 * @presence: The #PurplePresence instance.
 *
 * Gets whether or not @presence is online.
 *
 * Returns: %TRUE if the presence is online, or %FALSE otherwise.
 */
gboolean purple_presence_is_online(PurplePresence *presence);

/**
 * purple_presence_is_status_active:
 * @presence: The #PurplePresence instance.
 * @status_id: The ID of the status.
 *
 * Gets whether or not a status in @presence is active.
 *
 * A status is active if itself or any of its sub-statuses are active.
 *
 * Returns: %TRUE if the status is active, or %FALSE.
 */
gboolean purple_presence_is_status_active(PurplePresence *presence, const gchar *status_id);

/**
 * purple_presence_is_status_primitive_active:
 * @presence: The #PurplePresence instance.
 * @primitive: The status primitive.
 *
 * Gets whether or not a status with the specified primitive type in @presence
 * is active.
 *
 * A status is active if itself or any of its sub-statuses are active.
 *
 * Returns: %TRUE if the status is active, or %FALSE.
 */
gboolean purple_presence_is_status_primitive_active(PurplePresence *presence, PurpleStatusPrimitive primitive);

/**
 * purple_presence_is_idle:
 * @presence: The #PurplePresence instance.
 *
 * Gets whether or not @presence is idle.
 *
 * Returns: %TRUE if the presence is idle, or %FALSE otherwise.  If the
 *          presence is offline (purple_presence_is_online() returns %FALSE)
 *          then %FALSE is returned.
 */
gboolean purple_presence_is_idle(PurplePresence *presence);

/**
 * purple_presence_get_idle_time:
 * @presence: The #PurplePresence instance.
 *
 * Gets the idle time of @presence.
 *
 * Returns: The idle time of @presence.
 */
time_t purple_presence_get_idle_time(PurplePresence *presence);

/**
 * purple_presence_get_login_time:
 * @presence: The #PurplePresence instance.
 *
 * Gets the login time of @presence.
 *
 * Returns: The login time of @presence.
 */
time_t purple_presence_get_login_time(PurplePresence *presence);

G_END_DECLS

#endif /* PURPLE_PRESENCE_H */
