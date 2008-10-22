/**
 * @file group_find.h
 *
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

#ifndef _QQ_GROUP_FIND_H_
#define _QQ_GROUP_FIND_H_

#include <glib.h>
#include "connection.h"
#include "group.h"

qq_buddy_data *qq_group_find_member_by_uid(qq_group *group, guint32 uid);
void qq_group_remove_member_by_uid(qq_group *group, guint32 uid);
qq_buddy_data *qq_group_find_or_add_member(PurpleConnection *gc, qq_group *group, guint32 member_uid);

qq_group *qq_room_search_id(PurpleConnection *gc, guint32 room_id);

qq_group *qq_room_get_next(PurpleConnection *gc, guint32 room_id);
qq_group *qq_room_get_next_conv(PurpleConnection *gc, guint32 room_id);

#endif
