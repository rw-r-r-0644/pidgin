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

/**
 * SECTION:pidginactiongroup
 * @section_id: pidgin-action-group
 * @short_description: An action group for Pidgin
 * @title: Action Group
 *
 * A #GSimpleActionGroup containing most of our actions.  A lot of this will
 * need to be added to the #GtkApplication, but I didn't want to do that part
 * quite yet, so I created this instead.
 */

#include <glib.h>

#include <gio/gio.h>

/**
 * PIDGIN_ACTION_ABOUT:
 *
 * A constant that represents the about action that shows the about window.
 */
#define PIDGIN_ACTION_ABOUT ("about")

/**
 * PIDGIN_ACTION_ADD_BUDDY:
 *
 * A constant that represents the add-buddy action to add a buddy to the
 * contact list.
 */
#define PIDGIN_ACTION_ADD_BUDDY ("add-buddy")

/**
 * PIDGIN_ACTION_ADD_CHAT:
 *
 * A constant that represents the add-chat action to add a chat to the
 * contact list.
 */
#define PIDGIN_ACTION_ADD_CHAT ("add-chat")

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
 * A constant that represents the custom-smiley action to toggle the visibility
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
 * A constant that represents the get-user-info action.
 */
#define PIDGIN_ACTION_GET_USER_INFO ("get-user-info")

/**
 * PIDGIN_ACTION_JOIN_CHAT:
 *
 * A constant that represents the join-chat action.
 */
#define PIDGIN_ACTION_JOIN_CHAT ("join-chat")

/**
 * PIDGIN_ACTION_MUTE_SOUNDS:
 *
 * A constant that represents the mute-sounds action.
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
 * PIDGIN_ACTION_SET_MOOD:
 *
 * A constant that represents the set-mood action.
 */
#define PIDGIN_ACTION_SET_MOOD ("set-mood")

/**
 * PIDGIN_ACTION_SHOW_BUDDY_ICONS:
 *
 * A constant that represents the show-buddy-icons action.
 */
#define PIDGIN_ACTION_SHOW_BUDDY_ICONS ("show-buddy-icons")

/**
 * PIDGIN_ACTION_SHOW_EMPTY_GROUPS:
 *
 * A constant that represents the show-empty-groups action.
 */
#define PIDGIN_ACTION_SHOW_EMPTY_GROUPS ("show-empty-groups")

/**
 * PIDGIN_ACTION_SHOW_IDLE_TIMES:
 *
 * A constant that represents the show-idle-times action.
 */
#define PIDGIN_ACTION_SHOW_IDLE_TIMES ("show-idle-times")

/**
 * PIDGIN_ACTION_SHOW_OFFLINE_BUDDIES:
 *
 * A constant that represents the show-offline-buddies action.
 */
#define PIDGIN_ACTION_SHOW_OFFLINE_BUDDIES ("show-offline-buddies")

/**
 * PIDGIN_ACTION_SHOW_PROTOCOL_ICONS:
 *
 * A constant that represents the show-protocol-icons action.
 */
#define PIDGIN_ACTION_SHOW_PROTOCOL_ICONS ("show-protocol-icons")

/**
 * PIDGIN_ACTION_SORT_METHOD:
 *
 * A constant that represents the sort-method action to change the sorting
 * method of the buddy list.
 */
#define PIDGIN_ACTION_SORT_METHOD ("sort-method")

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
