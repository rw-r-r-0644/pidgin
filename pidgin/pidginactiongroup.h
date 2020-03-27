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
 * PIDGIN_ACTION_ADD_ABOUT:
 *
 * A constant that represents the about action that shows the about window.
 */
#define PIDGIN_ACTION_ABOUT ("about")

/**
 * PIDGIN_ACTION_ADD_GROUP:
 *
 * A constant that represents the add-group action to add a group to the
 * contact list.
 */
#define PIDGIN_ACTION_ADD_GROUP ("add-group")

/**
 * PIDGIN_ACTION_BUDDY_POUNCES:
 *
 * A constant that represents the pounces action.
 */
#define PIDGIN_ACTION_BUDDY_POUNCES ("buddy-pounces")

/**
 * PIDGIN_ACTION_CUSTOM_SMILEY:
 *
 * A constant that represents the smiley-action action to toggle the visibility
 * of the smiley manager.
 */
#define PIDGIN_ACTION_CUSTOM_SMILEY ("custom-smiley")

/**
 * PIDGIN_ACTION_DEBUG:
 *
 * A constant that represents the debug action to toggle the visibility of the
 * debug window.
 */
#define PIDGIN_ACTION_DEBUG ("debug")

/**
 * PIDGIN_ACTION_FILE_TRANSFERS:
 *
 * A constant that represents the file-transfers action to toggle the
 * visibility of the file transfers window.
 */
#define PIDGIN_ACTION_FILE_TRANSFERS ("file-transfers")

/**
 * PIDGIN_ACTION_GET_USER_INFO:
 *
 * A constant that represents the get-user info action.
 */
#define PIDGIN_ACTION_GET_USER_INFO ("get-user-info")

/**
 * PIDGIN_ACTION_MANAGE_ACCOUNTS:
 *
 * A constatnt that represents the manage-accounts action to displays the
 * manage accounts window.
 */
#define PIDGIN_ACTION_MANAGE_ACCOUNTS ("manage-accounts")

/**
 * PIDGIN_ACTION_MUTE_SOUNDS:
 *
 * A constatnt that represents the mute-sounds action.
 */
#define PIDGIN_ACTION_MUTE_SOUNDS ("mute-sounds")

/**
 * PIDGIN_ACTION_NEW_MESSAGE:
 *
 * A constant that represents the new-message action.
 */
#define PIDGIN_ACTION_NEW_MESSAGE ("new-message")

/**
 * PIDGIN_ACTION_ONLINE_HELP:
 *
 * A constant that represents the online-help action.
 */
#define PIDGIN_ACTION_ONLINE_HELP ("online-help")

/**
 * PIDGIN_ACTION_PLUGINS:
 *
 * A constant that represents the plugins action.
 */
#define PIDGIN_ACTION_PLUGINS ("plugins")

/**
 * PIDGIN_ACTION_PREFERENCES:
 *
 * A constant that represents the preferences action.
 */
#define PIDGIN_ACTION_PREFERENCES ("preferences")

/**
 * PIDGIN_ACTION_PRIVACY:
 *
 * A constant that represents the privacy action.
 */
#define PIDGIN_ACTION_PRIVACY ("privacy")

/**
 * PIDGIN_ACTION_QUIT:
 *
 * A constant that represents the quit action.
 */
#define PIDGIN_ACTION_QUIT ("quit")

/**
 * PIDGIN_ACTION_ROOM_LIST:
 *
 * A constant that represents the room-list action.
 */
#define PIDGIN_ACTION_ROOM_LIST ("room-list")

/**
 * PIDGIN_ACTION_SYSTEM_LOG:
 *
 * A constant that represents the system-log action.
 */
#define PIDGIN_ACTION_SYSTEM_LOG ("system-log")

/**
 * PIDGIN_ACTION_VIEW_USER_LOG:
 *
 * A constant that represents the view-user-log action.
 */
#define PIDGIN_ACTION_VIEW_USER_LOG ("view-user-log")

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
