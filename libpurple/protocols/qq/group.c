/**
 * @file group.c
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

#include "internal.h"

#include "debug.h"
#include "prpl.h"
#include "request.h"

#include "group_internal.h"
#include "group_info.h"
#include "group_search.h"
#include "utils.h"
#include "qq_network.h"
#include "header_info.h"
#include "group.h"

static void _qq_group_search_callback(PurpleConnection *gc, const gchar *input)
{
	guint32 ext_id;

	g_return_if_fail(input != NULL);
	ext_id = qq_string_to_dec_value(input);
	/* 0x00000000 means search for demo group */
	qq_send_cmd_group_search_group(gc, ext_id);
}

static void _qq_group_search_cancel_callback(PurpleConnection *gc, const gchar *input)
{
	qq_data *qd;

	qd = (qq_data *) gc->proto_data;
	purple_roomlist_set_in_progress(qd->roomlist, FALSE);
}

/* This is needed for PurpleChat node to be valid */
GList *qq_chat_info(PurpleConnection *gc)
{
	GList *m;
	struct proto_chat_entry *pce;

	m = NULL;

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("ID: ");
	pce->identifier = QQ_ROOM_KEY_EXTERNAL_ID;
	m = g_list_append(m, pce);

	return m;
}

GHashTable *qq_chat_info_defaults(PurpleConnection *gc, const gchar *chat_name)
{
	GHashTable *defaults;

	defaults = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);

	if (chat_name != NULL)
		g_hash_table_insert(defaults, QQ_ROOM_KEY_EXTERNAL_ID, g_strdup(chat_name));

	return defaults;
}

/*  get a list of qq groups */
PurpleRoomlist *qq_roomlist_get_list(PurpleConnection *gc)
{
	GList *fields;
	qq_data *qd;
	PurpleRoomlist *rl;
	PurpleRoomlistField *f;

	qd = (qq_data *) gc->proto_data;

	fields = NULL;
	rl = purple_roomlist_new(purple_connection_get_account(gc));
	qd->roomlist = rl;

	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, _("Group ID"), QQ_ROOM_KEY_EXTERNAL_ID, FALSE);
	fields = g_list_append(fields, f);
	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, _("Creator"), QQ_ROOM_KEY_CREATOR_UID, FALSE);
	fields = g_list_append(fields, f);
	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING,
				    _("Group Description"), QQ_ROOM_KEY_DESC_UTF8, FALSE);
	fields = g_list_append(fields, f);
	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, "", QQ_ROOM_KEY_INTERNAL_ID, TRUE);
	fields = g_list_append(fields, f);
	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, "", QQ_ROOM_KEY_TYPE, TRUE);
	fields = g_list_append(fields, f);
	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, _("Auth"), QQ_ROOM_KEY_AUTH_TYPE, TRUE);
	fields = g_list_append(fields, f);
	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, "", QQ_ROOM_KEY_CATEGORY, TRUE);
	fields = g_list_append(fields, f);
	f = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, "", QQ_ROOM_KEY_TITLE_UTF8, TRUE);

	fields = g_list_append(fields, f);
	purple_roomlist_set_fields(rl, fields);
	purple_roomlist_set_in_progress(qd->roomlist, TRUE);

	purple_request_input(gc, _("QQ Qun"),
			   _("Please enter Qun number"),
			   _("You can only search for permanent Qun\n"),
			   NULL, FALSE, FALSE, NULL,
			   _("Search"), G_CALLBACK(_qq_group_search_callback),
			   _("Cancel"), G_CALLBACK(_qq_group_search_cancel_callback),
			   purple_connection_get_account(gc), NULL, NULL,
			   gc);

	return qd->roomlist;
}

/* free roomlist space, I have no idea when this one is called ... */
void qq_roomlist_cancel(PurpleRoomlist *list)
{
	qq_data *qd;
	PurpleConnection *gc;

	g_return_if_fail(list != NULL);
	gc = purple_account_get_connection(list->account);

	qd = (qq_data *) gc->proto_data;
	purple_roomlist_set_in_progress(list, FALSE);
	purple_roomlist_unref(list);
}

/* this should be called upon signin, even when we did not open group chat window */
void qq_group_init(PurpleConnection *gc)
{
	PurpleAccount *account;
	PurpleChat *chat;
	PurpleGroup *purple_group;
	PurpleBlistNode *node;
	qq_group *group;
	gint count;

	account = purple_connection_get_account(gc);

	purple_group = purple_find_group(PURPLE_GROUP_QQ_QUN);
	if (purple_group == NULL) {
		purple_debug_info("QQ", "We have no QQ Qun\n");
		return;
	}

	count = 0;
	for (node = ((PurpleBlistNode *) purple_group)->child; node != NULL; node = node->next) {
		if ( !PURPLE_BLIST_NODE_IS_CHAT(node)) {
			continue;
		}
		/* got one */
		chat = (PurpleChat *) node;
		if (account != chat->account)	/* not qq account*/
			continue;
		group = qq_room_create_by_hashtable(gc, chat->components);
		if (group == NULL)
			continue;

		if (group->id <= 0)
			continue;

		count++;
	}

	purple_debug_info("QQ", "Load %d QQ Qun configurations\n", count);
}
