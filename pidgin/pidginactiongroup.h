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
#ifndef PIDGIN_ACTION_GROUP_H
#define PIDGIN_ACTION_GROUP_H

#include <glib.h>

#include <gio/gio.h>

/**
 * PIDGIN_ACTION_NEW_MESSAGE:
 *
 * A constant that represents the new message action.
 */
#define PIDGIN_ACTION_NEW_MESSAGE ("new-message")

/**
 * PIDGIN_ACTION_ONLINE_HELP:
 *
 * A constant that represents the online help action.
 */
#define PIDGIN_ACTION_ONLINE_HELP ("online-help")

G_BEGIN_DECLS

#define PIDGIN_TYPE_ACTION_GROUP (pidgin_action_group_get_type())
G_DECLARE_FINAL_TYPE(PidginActionGroup, pidgin_action_group, PIDGIN,
                     ACTION_GROUP, GSimpleActionGroup)

/**
 * pidgin_action_group_new:
 *
 * Creates a new #PidginActionGroup instance that contains all of the
 * #GAction's in Pidgin.
 *
 * Returns: (transfer full): The new #PidginActionGroup instance.
 */
GSimpleActionGroup *pidgin_action_group_new(void);

G_END_DECLS

#endif /* PIDGIN_ACTION_GROUP_H */
