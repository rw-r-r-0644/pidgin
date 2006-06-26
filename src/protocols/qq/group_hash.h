/**
* The QQ2003C protocol plugin
 *
 * for gaim
 *
 * Copyright (C) 2004 Puzzlebird
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// START OF FILE
/*****************************************************************************/
#ifndef _QQ_GROUP_HASH_H_
#define _QQ_GROUP_HASH_H_

#include <glib.h>		// GHashTable
#include "group.h"		// qq_group

#define QQ_GROUP_KEY_MEMBER_STATUS      "my_status_code"
#define QQ_GROUP_KEY_MEMBER_STATUS_DESC "my_status_desc"
#define QQ_GROUP_KEY_INTERNAL_ID        "internal_group_id"
#define QQ_GROUP_KEY_EXTERNAL_ID        "external_group_id"
#define QQ_GROUP_KEY_GROUP_TYPE         "group_type"
#define QQ_GROUP_KEY_CREATOR_UID        "creator_uid"
#define QQ_GROUP_KEY_GROUP_CATEGORY     "group_category"
#define QQ_GROUP_KEY_AUTH_TYPE          "auth_type"
#define QQ_GROUP_KEY_GROUP_NAME_UTF8    "group_name_utf8"
#define QQ_GROUP_KEY_GROUP_DESC_UTF8    "group_desc_utf8"

qq_group *qq_group_create_by_id(GaimConnection * gc, guint32 internal_id, guint32 external_id);
GHashTable *qq_group_to_hashtable(qq_group * group);

qq_group *qq_group_from_hashtable(GaimConnection * gc, GHashTable * data);
void qq_group_refresh(GaimConnection * gc, qq_group * group);

#endif
/*****************************************************************************/
// END OF FILE
