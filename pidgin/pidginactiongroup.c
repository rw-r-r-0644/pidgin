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

#include "pidginactiongroup.h"

#include "internal.h"
#include "gtkdialogs.h"

struct _PidginActionGroup {
	GSimpleActionGroup parent;
};

/******************************************************************************
 * Action Callbacks
 *****************************************************************************/
static void
pidgin_action_group_new_message(GSimpleAction *simple, GVariant *parameter,
                                gpointer data)
{
	pidgin_dialogs_im();
}

static void
pidgin_action_group_online_help(GSimpleAction *simple, GVariant *parameter,
                                gpointer data)
{
	purple_notify_uri(NULL, PURPLE_WEBSITE "documentation");
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
G_DEFINE_TYPE(PidginActionGroup, pidgin_action_group,
              G_TYPE_SIMPLE_ACTION_GROUP)

static void
pidgin_action_group_init(PidginActionGroup *group) {
	GActionEntry entries[] = {
		{
			.name = PIDGIN_ACTION_NEW_MESSAGE,
			.activate = pidgin_action_group_new_message,
		}, {
			.name = PIDGIN_ACTION_ONLINE_HELP,
			.activate = pidgin_action_group_online_help,
		},
	};

	g_action_map_add_action_entries(G_ACTION_MAP(group), entries,
	                                G_N_ELEMENTS(entries), NULL);
};

static void
pidgin_action_group_class_init(PidginActionGroupClass *klass) {
}

/******************************************************************************
 * Public API
 *****************************************************************************/
GSimpleActionGroup *
pidgin_action_group_new(void) {
	return G_SIMPLE_ACTION_GROUP(g_object_new(PIDGIN_TYPE_ACTION_GROUP, NULL));
}
