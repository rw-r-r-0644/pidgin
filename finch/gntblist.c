/**
 * @file gntblist.c GNT BuddyList API
 * @ingroup finch
 */

/* finch
 *
 * Finch is the legal property of its developers, whose names are too numerous
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
#include "finch.h"

#include <account.h>
#include <blist.h>
#include <notify.h>
#include <request.h>
#include <savedstatuses.h>
#include <server.h>
#include <signal.h>
#include <status.h>
#include <util.h>
#include "debug.h"

#include "gntbox.h"
#include "gntcolors.h"
#include "gntcombobox.h"
#include "gntentry.h"
#include "gntft.h"
#include "gntlabel.h"
#include "gntline.h"
#include "gntmenu.h"
#include "gntmenuitem.h"
#include "gntmenuitemcheck.h"
#include "gntpounce.h"
#include "gntstyle.h"
#include "gnttree.h"
#include "gntutils.h"
#include "gntwindow.h"

#include "gntblist.h"
#include "gntconv.h"
#include "gntstatus.h"
#include <string.h>

#define PREF_ROOT "/finch/blist"
#define TYPING_TIMEOUT 4000

typedef struct
{
	GntWidget *window;
	GntWidget *tree;

	GntWidget *tooltip;
	PurpleBlistNode *tnode;		/* Who is the tooltip being displayed for? */
	GList *tagged;          /* A list of tagged blistnodes */

	GntWidget *context;
	PurpleBlistNode *cnode;

	/* XXX: I am KISSing */
	GntWidget *status;          /* Dropdown with the statuses  */
	GntWidget *statustext;      /* Status message */
	int typing;

	GntWidget *menu;
	/* These are the menuitems that get regenerated */
	GntMenuItem *accounts;
	GntMenuItem *plugins;
} FinchBlist;

typedef enum
{
	STATUS_PRIMITIVE = 0,
	STATUS_SAVED_POPULAR,
	STATUS_SAVED_ALL,
	STATUS_SAVED_NEW
} StatusType;

typedef struct
{
	StatusType type;
	union
	{
		PurpleStatusPrimitive prim;
		PurpleSavedStatus *saved;
	} u;
} StatusBoxItem;

static FinchBlist *ggblist;

static void add_buddy(PurpleBuddy *buddy, FinchBlist *ggblist);
static void add_contact(PurpleContact *contact, FinchBlist *ggblist);
static void add_group(PurpleGroup *group, FinchBlist *ggblist);
static void add_chat(PurpleChat *chat, FinchBlist *ggblist);
static void add_node(PurpleBlistNode *node, FinchBlist *ggblist);
static void node_update(PurpleBuddyList *list, PurpleBlistNode *node);
static void draw_tooltip(FinchBlist *ggblist);
static gboolean remove_typing_cb(gpointer null);
static void remove_peripherals(FinchBlist *ggblist);
static const char * get_display_name(PurpleBlistNode *node);
static void savedstatus_changed(PurpleSavedStatus *now, PurpleSavedStatus *old);
static void blist_show(PurpleBuddyList *list);
static void update_node_display(PurpleBlistNode *buddy, FinchBlist *ggblist);
static void update_buddy_display(PurpleBuddy *buddy, FinchBlist *ggblist);
static void account_signed_on_cb(PurpleConnection *pc, gpointer null);
static void finch_request_add_buddy(PurpleAccount *account, const char *username, const char *grp, const char *alias);

/* Sort functions */
static int blist_node_compare_position(PurpleBlistNode *n1, PurpleBlistNode *n2);
static int blist_node_compare_text(PurpleBlistNode *n1, PurpleBlistNode *n2);
static int blist_node_compare_status(PurpleBlistNode *n1, PurpleBlistNode *n2);
static int blist_node_compare_log(PurpleBlistNode *n1, PurpleBlistNode *n2);

static int color_available;
static int color_away;
static int color_offline;
static int color_idle;

static gboolean
is_contact_online(PurpleContact *contact)
{
	PurpleBlistNode *node;
	for (node = ((PurpleBlistNode*)contact)->child; node; node = node->next) {
		if (PURPLE_BUDDY_IS_ONLINE((PurpleBuddy*)node))
			return TRUE;
	}
	return FALSE;
}

static gboolean
is_group_online(PurpleGroup *group)
{
	PurpleBlistNode *node;
	for (node = ((PurpleBlistNode*)group)->child; node; node = node->next) {
		if (PURPLE_BLIST_NODE_IS_CHAT(node) &&
				purple_account_is_connected(((PurpleChat *)node)->account))
			return TRUE;
		else if (is_contact_online((PurpleContact*)node))
			return TRUE;
	}
	return FALSE;
}

static void
new_node(PurpleBlistNode *node)
{
}

static void add_node(PurpleBlistNode *node, FinchBlist *ggblist)
{
	if (PURPLE_BLIST_NODE_IS_BUDDY(node))
		add_buddy((PurpleBuddy*)node, ggblist);
	else if (PURPLE_BLIST_NODE_IS_CONTACT(node))
		add_contact((PurpleContact*)node, ggblist);
	else if (PURPLE_BLIST_NODE_IS_GROUP(node))
		add_group((PurpleGroup*)node, ggblist);
	else if (PURPLE_BLIST_NODE_IS_CHAT(node))
		add_chat((PurpleChat *)node, ggblist);
	draw_tooltip(ggblist);
}

static void
remove_tooltip(FinchBlist *ggblist)
{
	gnt_widget_destroy(ggblist->tooltip);
	ggblist->tooltip = NULL;
	ggblist->tnode = NULL;
}

static void
node_remove(PurpleBuddyList *list, PurpleBlistNode *node)
{
	FinchBlist *ggblist = list->ui_data;

	if (ggblist == NULL || node->ui_data == NULL)
		return;

	gnt_tree_remove(GNT_TREE(ggblist->tree), node);
	node->ui_data = NULL;
	if (ggblist->tagged)
		ggblist->tagged = g_list_remove(ggblist->tagged, node);

	if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
		PurpleContact *contact = (PurpleContact*)node->parent;
		if ((!purple_prefs_get_bool(PREF_ROOT "/showoffline") && !is_contact_online(contact)) ||
				contact->currentsize < 1)
			node_remove(list, (PurpleBlistNode*)contact);
		else
			node_update(list, (PurpleBlistNode*)contact);
	} else if (!PURPLE_BLIST_NODE_IS_GROUP(node)) {
		PurpleGroup *group = (PurpleGroup*)node->parent;
		if ((group->currentsize < 1 && !purple_prefs_get_bool(PREF_ROOT "/emptygroups")) ||
				(!purple_prefs_get_bool(PREF_ROOT "/showoffline") && !is_group_online(group)))
			node_remove(list, node->parent);
		for (node = node->child; node; node = node->next)
			node->ui_data = NULL;
	} else {
		for (node = node->child; node; node = node->next)
			node_remove(list, node);
	}

	draw_tooltip(ggblist);
}

static void
node_update(PurpleBuddyList *list, PurpleBlistNode *node)
{
	/* It really looks like this should never happen ... but it does.
           This will at least emit a warning to the log when it
           happens, so maybe someone will figure it out. */
	g_return_if_fail(node != NULL);

	if (list->ui_data == NULL)
		return;   /* XXX: this is probably the place to auto-join chats */

	if (ggblist->window == NULL)
		return;

	if (node->ui_data != NULL) {
		gnt_tree_change_text(GNT_TREE(ggblist->tree), node,
				0, get_display_name(node));
		gnt_tree_sort_row(GNT_TREE(ggblist->tree), node);
	}

	if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
		PurpleBuddy *buddy = (PurpleBuddy*)node;
		if (purple_account_is_connected(buddy->account) &&
				(PURPLE_BUDDY_IS_ONLINE(buddy) || purple_prefs_get_bool(PREF_ROOT "/showoffline")))
			add_node((PurpleBlistNode*)buddy, list->ui_data);
		else
			node_remove(purple_get_blist(), node);

		node_update(list, node->parent);
	} else if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
		add_chat((PurpleChat *)node, list->ui_data);
	} else if (PURPLE_BLIST_NODE_IS_CONTACT(node)) {
		PurpleContact *contact = (PurpleContact*)node;
		if ((!purple_prefs_get_bool(PREF_ROOT "/showoffline") && !is_contact_online(contact)) ||
				contact->currentsize < 1)
			node_remove(purple_get_blist(), node);
		else {
			if (node->ui_data == NULL) {
				/* The core seems to expect the UI to add the buddies. */
				for (node = node->child; node; node = node->next)
					add_node(node, list->ui_data);
			}
		}
	} else if (PURPLE_BLIST_NODE_IS_GROUP(node)) {
		PurpleGroup *group = (PurpleGroup*)node;
		if (!purple_prefs_get_bool(PREF_ROOT "/emptygroups") &&
				((!purple_prefs_get_bool(PREF_ROOT "/showoffline") && !is_group_online(group)) ||
				 group->currentsize < 1))
			node_remove(list, node);
		else
			add_node(node, list->ui_data);
	}
}

static void
new_list(PurpleBuddyList *list)
{
	if (ggblist)
		return;

	ggblist = g_new0(FinchBlist, 1);
	list->ui_data = ggblist;
}

static void
add_buddy_cb(void *data, PurpleRequestFields *allfields)
{
	const char *username = purple_request_fields_get_string(allfields, "screenname");
	const char *alias = purple_request_fields_get_string(allfields, "alias");
	const char *group = purple_request_fields_get_string(allfields, "group");
	PurpleAccount *account = purple_request_fields_get_account(allfields, "account");
	const char *error = NULL;
	PurpleGroup *grp;
	PurpleBuddy *buddy;

	if (!username)
		error = _("You must provide a screename for the buddy.");
	else if (!group)
		error = _("You must provide a group.");
	else if (!account)
		error = _("You must select an account.");
	else if (!purple_account_is_connected(account))
		error = _("The selected account is not online.");

	if (error)
	{
		finch_request_add_buddy(account, username, group, alias);
		purple_notify_error(NULL, _("Error"), _("Error adding buddy"), error);
		return;
	}

	grp = purple_find_group(group);
	if (!grp)
	{
		grp = purple_group_new(group);
		purple_blist_add_group(grp, NULL);
	}

	buddy = purple_buddy_new(account, username, alias);
	purple_blist_add_buddy(buddy, NULL, grp, NULL);
	purple_account_add_buddy(account, buddy);
}

static void
finch_request_add_buddy(PurpleAccount *account, const char *username, const char *grp, const char *alias)
{
	PurpleRequestFields *fields = purple_request_fields_new();
	PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
	PurpleRequestField *field;

	purple_request_fields_add_group(fields, group);

	field = purple_request_field_string_new("screenname", _("Screen Name"), username, FALSE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_string_new("alias", _("Alias"), alias, FALSE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_string_new("group", _("Group"), grp, FALSE);
	purple_request_field_group_add_field(group, field);
	purple_request_field_set_type_hint(field, "group");

	field = purple_request_field_account_new("account", _("Account"), NULL);
	purple_request_field_account_set_show_all(field, FALSE);
	if (account)
		purple_request_field_account_set_value(field, account);
	purple_request_field_group_add_field(group, field);

	purple_request_fields(NULL, _("Add Buddy"), NULL, _("Please enter buddy information."),
			fields,
			_("Add"), G_CALLBACK(add_buddy_cb),
			_("Cancel"), NULL,
			account, NULL, NULL,
			NULL);
}

static void
add_chat_cb(void *data, PurpleRequestFields *allfields)
{
	PurpleAccount *account;
	const char *alias, *name, *group;
	PurpleChat *chat;
	PurpleGroup *grp;
	GHashTable *hash = NULL;
	PurpleConnection *gc;
	gboolean autojoin;

	account = purple_request_fields_get_account(allfields, "account");
	name = purple_request_fields_get_string(allfields, "name");
	alias = purple_request_fields_get_string(allfields, "alias");
	group = purple_request_fields_get_string(allfields, "group");
	autojoin = purple_request_fields_get_bool(allfields, "autojoin");

	if (!purple_account_is_connected(account) || !name || !*name)
		return;
	
	if (!group || !*group)
		group = _("Chats");

	gc = purple_account_get_connection(account);

	if (PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl)->chat_info_defaults != NULL)
		hash = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl)->chat_info_defaults(gc, name);
	
	chat = purple_chat_new(account, name, hash);

	if (chat != NULL) {
		if ((grp = purple_find_group(group)) == NULL) {
			grp = purple_group_new(group);
			purple_blist_add_group(grp, NULL);
		}
		purple_blist_add_chat(chat, grp, NULL);
		purple_blist_alias_chat(chat, alias);
		purple_blist_node_set_bool((PurpleBlistNode*)chat, "gnt-autojoin", autojoin);
		if (autojoin)
			serv_join_chat(chat->account->gc, chat->components);
	}
}

static void
finch_request_add_chat(PurpleAccount *account, PurpleGroup *grp, const char *alias, const char *name)
{
	PurpleRequestFields *fields = purple_request_fields_new();
	PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
	PurpleRequestField *field;

	purple_request_fields_add_group(fields, group);

	field = purple_request_field_account_new("account", _("Account"), NULL);
	purple_request_field_account_set_show_all(field, FALSE);
	if (account)
		purple_request_field_account_set_value(field, account);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_string_new("name", _("Name"), name, FALSE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_string_new("alias", _("Alias"), alias, FALSE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_string_new("group", _("Group"), grp ? grp->name : NULL, FALSE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_bool_new("autojoin", _("Auto-join"), FALSE);
	purple_request_field_group_add_field(group, field);

	purple_request_fields(NULL, _("Add Chat"), NULL,
			_("You can edit more information from the context menu later."),
			fields, _("Add"), G_CALLBACK(add_chat_cb), _("Cancel"), NULL,
			NULL, NULL, NULL,
			NULL);
}

static void
add_group_cb(gpointer null, const char *group)
{
	PurpleGroup *grp;

	if (!group || !*group)
	{
		purple_notify_error(NULL, _("Error"), _("Error adding group"),
				_("You must give a name for the group to add."));
		return;
	}

	grp = purple_find_group(group);
	if (!grp)
	{
		grp = purple_group_new(group);
		purple_blist_add_group(grp, NULL);
	}
	else
	{
		purple_notify_error(NULL, _("Error"), _("Error adding group"),
				_("A group with the name already exists."));
	}
}

static void
finch_request_add_group()
{
	purple_request_input(NULL, _("Add Group"), NULL, _("Enter the name of the group"),
			NULL, FALSE, FALSE, NULL,
			_("Add"), G_CALLBACK(add_group_cb), _("Cancel"), NULL,
			NULL, NULL, NULL,
			NULL);
}

static PurpleBlistUiOps blist_ui_ops =
{
	new_list,
	new_node,
	blist_show,
	node_update,
	node_remove,
	NULL,
	NULL,
	finch_request_add_buddy,
	finch_request_add_chat,
	finch_request_add_group,
	NULL,
	NULL,
	NULL,
	NULL
};

static gpointer
finch_blist_get_handle()
{
	static int handle;

	return &handle;
}

static void
add_group(PurpleGroup *group, FinchBlist *ggblist)
{
	PurpleBlistNode *node = (PurpleBlistNode *)group;
	if (node->ui_data)
		return;
	node->ui_data = gnt_tree_add_row_after(GNT_TREE(ggblist->tree), group,
			gnt_tree_create_row(GNT_TREE(ggblist->tree), get_display_name(node)), NULL, NULL);
	gnt_tree_set_expanded(GNT_TREE(ggblist->tree), node,
		!purple_blist_node_get_bool(node, "collapsed"));
}

static const char *
get_display_name(PurpleBlistNode *node)
{
	static char text[2096];
	char status[8] = " ";
	const char *name = NULL;

	if (PURPLE_BLIST_NODE_IS_CONTACT(node))
		node = (PurpleBlistNode*)purple_contact_get_priority_buddy((PurpleContact*)node);  /* XXX: this can return NULL?! */
	
	if (node == NULL)
		return NULL;

	if (PURPLE_BLIST_NODE_IS_BUDDY(node))
	{
		PurpleBuddy *buddy = (PurpleBuddy *)node;
		PurpleStatusPrimitive prim;
		PurplePresence *presence;
		PurpleStatus *now;
		gboolean ascii = gnt_ascii_only();
		
		presence = purple_buddy_get_presence(buddy);
		if (purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_MOBILE))
			strncpy(status, ascii ? ":" : "☎", sizeof(status) - 1);
		else {
			now = purple_presence_get_active_status(presence);

			prim = purple_status_type_get_primitive(purple_status_get_type(now));

			switch(prim) {
				case PURPLE_STATUS_OFFLINE:
					strncpy(status, ascii ? "x" : "⊗", sizeof(status) - 1);
					break;
				case PURPLE_STATUS_AVAILABLE:
					strncpy(status, ascii ? "o" : "◯", sizeof(status) - 1);
					break;
				default:
					strncpy(status, ascii ? "." : "⊖", sizeof(status) - 1);
					break;
			}
		}
		name = purple_buddy_get_alias(buddy);
	}
	else if (PURPLE_BLIST_NODE_IS_CHAT(node))
	{
		PurpleChat *chat = (PurpleChat*)node;
		name = purple_chat_get_name(chat);

		strncpy(status, "~", sizeof(status) - 1);
	}
	else if (PURPLE_BLIST_NODE_IS_GROUP(node))
		return ((PurpleGroup*)node)->name;

	snprintf(text, sizeof(text) - 1, "%s %s", status, name);

	return text;
}

static void
add_chat(PurpleChat *chat, FinchBlist *ggblist)
{
	PurpleGroup *group;
	PurpleBlistNode *node = (PurpleBlistNode *)chat;
	if (node->ui_data)
		return;
	if (!purple_account_is_connected(chat->account))
		return;

	group = purple_chat_get_group(chat);
	add_node((PurpleBlistNode*)group, ggblist);

	node->ui_data = gnt_tree_add_row_after(GNT_TREE(ggblist->tree), chat,
				gnt_tree_create_row(GNT_TREE(ggblist->tree), get_display_name(node)),
				group, NULL);
}

static void
add_contact(PurpleContact *contact, FinchBlist *ggblist)
{
	PurpleGroup *group;
	PurpleBlistNode *node = (PurpleBlistNode*)contact;
	const char *name;

	if (node->ui_data)
		return;
	
	name = get_display_name(node);
	if (name == NULL)
		return;
	
	group = (PurpleGroup*)node->parent;
	add_node((PurpleBlistNode*)group, ggblist);

	node->ui_data = gnt_tree_add_row_after(GNT_TREE(ggblist->tree), contact,
				gnt_tree_create_row(GNT_TREE(ggblist->tree), name),
				group, NULL);

	gnt_tree_set_expanded(GNT_TREE(ggblist->tree), contact, FALSE);
}

static void
add_buddy(PurpleBuddy *buddy, FinchBlist *ggblist)
{
	PurpleContact *contact;
	PurpleBlistNode *node = (PurpleBlistNode *)buddy;
	if (node->ui_data)
		return;

	if (!purple_account_is_connected(buddy->account))
		return;

	contact = (PurpleContact*)node->parent;
	if (!contact)   /* When a new buddy is added and show-offline is set */
		return;
	add_node((PurpleBlistNode*)contact, ggblist);

	node->ui_data = gnt_tree_add_row_after(GNT_TREE(ggblist->tree), buddy,
				gnt_tree_create_row(GNT_TREE(ggblist->tree), get_display_name(node)),
				contact, NULL);

	if (purple_presence_is_idle(purple_buddy_get_presence(buddy))) {
		gnt_tree_set_row_color(GNT_TREE(ggblist->tree), buddy, color_idle);
		gnt_tree_set_row_color(GNT_TREE(ggblist->tree), contact, color_idle);
	} else if (purple_presence_is_available(purple_buddy_get_presence(buddy)) && color_available) {
		gnt_tree_set_row_color(GNT_TREE(ggblist->tree), buddy, color_available);
		gnt_tree_set_row_color(GNT_TREE(ggblist->tree), contact, color_available);
	} else if (purple_presence_is_online(purple_buddy_get_presence(buddy)) &&
			!purple_presence_is_available(purple_buddy_get_presence(buddy)) && color_away) {
		gnt_tree_set_row_color(GNT_TREE(ggblist->tree), buddy, color_away);
		gnt_tree_set_row_color(GNT_TREE(ggblist->tree), contact, color_away);
	} else if (!purple_presence_is_online(purple_buddy_get_presence(buddy)) && color_offline) {
		gnt_tree_set_row_color(GNT_TREE(ggblist->tree), buddy, color_offline);
		gnt_tree_set_row_color(GNT_TREE(ggblist->tree), contact, color_offline);
	}
}

#if 0
static void
buddy_signed_on(PurpleBuddy *buddy, FinchBlist *ggblist)
{
	add_node((PurpleBlistNode*)buddy, ggblist);
}

static void
buddy_signed_off(PurpleBuddy *buddy, FinchBlist *ggblist)
{
	node_remove(purple_get_blist(), (PurpleBlistNode*)buddy);
}
#endif

PurpleBlistUiOps *finch_blist_get_ui_ops()
{
	return &blist_ui_ops;
}

static void
selection_activate(GntWidget *widget, FinchBlist *ggblist)
{
	GntTree *tree = GNT_TREE(ggblist->tree);
	PurpleBlistNode *node = gnt_tree_get_selection_data(tree);

	if (!node)
		return;
	
	if (PURPLE_BLIST_NODE_IS_CONTACT(node))
		node = (PurpleBlistNode*)purple_contact_get_priority_buddy((PurpleContact*)node);

	if (PURPLE_BLIST_NODE_IS_BUDDY(node))
	{
		PurpleBuddy *buddy = (PurpleBuddy *)node;
		PurpleConversation *conv;
		conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM,
					purple_buddy_get_name(buddy),
					purple_buddy_get_account(buddy));
		if (!conv) {
			conv =  purple_conversation_new(PURPLE_CONV_TYPE_IM,
						purple_buddy_get_account(buddy),
						purple_buddy_get_name(buddy));
		} else {
			FinchConv *ggconv = conv->ui_data;
			gnt_window_present(ggconv->window);
		}
		finch_conversation_set_active(conv);
	}
	else if (PURPLE_BLIST_NODE_IS_CHAT(node))
	{
		PurpleChat *chat = (PurpleChat*)node;
		serv_join_chat(chat->account->gc, chat->components);
	}
}

static void
context_menu_callback(GntMenuItem *item, gpointer data)
{
	PurpleMenuAction *action = data;
	PurpleBlistNode *node = ggblist->cnode;
	if (action) {
		void (*callback)(PurpleBlistNode *, gpointer);
		callback = (void (*)(PurpleBlistNode *, gpointer))action->callback;
		if (callback)
			callback(node, action->data);
		else
			return;
	}
}

static void
gnt_append_menu_action(GntMenu *menu, PurpleMenuAction *action, gpointer parent)
{
	GList *list;
	GntMenuItem *item;

	if (action == NULL)
		return;

	item = gnt_menuitem_new(action->label);
	if (action->callback)
		gnt_menuitem_set_callback(GNT_MENU_ITEM(item), context_menu_callback, action);
	gnt_menu_add_item(menu, GNT_MENU_ITEM(item));

	if (action->children) {
		GntWidget *sub = gnt_menu_new(GNT_MENU_POPUP);
		gnt_menuitem_set_submenu(item, GNT_MENU(sub));
		for (list = action->children; list; list = list->next)
			gnt_append_menu_action(GNT_MENU(sub), list->data, action);
	}
}

static void
append_proto_menu(GntMenu *menu, PurpleConnection *gc, PurpleBlistNode *node)
{
	GList *list;
	PurplePluginProtocolInfo *prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl);

	if(!prpl_info || !prpl_info->blist_node_menu)
		return;

	for(list = prpl_info->blist_node_menu(node); list;
			list = g_list_delete_link(list, list))
	{
		PurpleMenuAction *act = (PurpleMenuAction *) list->data;
		act->data = node;
		gnt_append_menu_action(menu, act, NULL);
	}
}

static void
add_custom_action(GntMenu *menu, const char *label, PurpleCallback callback,
		gpointer data)
{
	PurpleMenuAction *action = purple_menu_action_new(label, callback, data, NULL);
	gnt_append_menu_action(menu, action, NULL);
	g_signal_connect_swapped(G_OBJECT(menu), "destroy",
			G_CALLBACK(purple_menu_action_free), action);
}

static void
chat_components_edit_ok(PurpleChat *chat, PurpleRequestFields *allfields)
{
	GList *groups, *fields;

	for (groups = purple_request_fields_get_groups(allfields); groups; groups = groups->next) {
		fields = purple_request_field_group_get_fields(groups->data);
		for (; fields; fields = fields->next) {
			PurpleRequestField *field = fields->data;
			const char *id;
			char *val;

			id = purple_request_field_get_id(field);
			if (purple_request_field_get_type(field) == PURPLE_REQUEST_FIELD_INTEGER)
				val = g_strdup_printf("%d", purple_request_field_int_get_value(field));
			else
				val = g_strdup(purple_request_field_string_get_value(field));

			g_hash_table_replace(chat->components, g_strdup(id), val);  /* val should not be free'd */
		}
	}
}

static void
chat_components_edit(PurpleBlistNode *selected, PurpleChat *chat)
{
	PurpleRequestFields *fields = purple_request_fields_new();
	PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
	PurpleRequestField *field;
	GList *parts, *iter;
	struct proto_chat_entry *pce;

	purple_request_fields_add_group(fields, group);

	parts = PURPLE_PLUGIN_PROTOCOL_INFO(chat->account->gc->prpl)->chat_info(chat->account->gc);

	for (iter = parts; iter; iter = iter->next) {
		pce = iter->data;
		if (pce->is_int) {
			int val;
			const char *str = g_hash_table_lookup(chat->components, pce->identifier);
			if (!str || sscanf(str, "%d", &val) != 1)
				val = pce->min;
			field = purple_request_field_int_new(pce->identifier, pce->label, val);
		} else {
			field = purple_request_field_string_new(pce->identifier, pce->label,
					g_hash_table_lookup(chat->components, pce->identifier), FALSE);
		}

		purple_request_field_group_add_field(group, field);
		g_free(pce);
	}

	g_list_free(parts);

	purple_request_fields(NULL, _("Edit Chat"), NULL, _("Please Update the necessary fields."),
			fields, _("Edit"), G_CALLBACK(chat_components_edit_ok), _("Cancel"), NULL,
			NULL, NULL, NULL,
			chat);
}

static void
autojoin_toggled(GntMenuItem *item, gpointer data)
{
	PurpleMenuAction *action = data;
	purple_blist_node_set_bool(action->data, "gnt-autojoin",
				gnt_menuitem_check_get_checked(GNT_MENU_ITEM_CHECK(item)));
}

static void
create_chat_menu(GntMenu *menu, PurpleChat *chat)
{
	PurpleMenuAction *action = purple_menu_action_new(_("Auto-join"), NULL, chat, NULL);
	GntMenuItem *check = gnt_menuitem_check_new(action->label);
	gnt_menuitem_check_set_checked(GNT_MENU_ITEM_CHECK(check),
				purple_blist_node_get_bool((PurpleBlistNode*)chat, "gnt-autojoin"));
	gnt_menu_add_item(menu, check);
	gnt_menuitem_set_callback(check, autojoin_toggled, action);
	g_signal_connect_swapped(G_OBJECT(menu), "destroy",
			G_CALLBACK(purple_menu_action_free), action);

	add_custom_action(menu, _("Edit Settings"), (PurpleCallback)chat_components_edit, chat);
}

static void
finch_add_buddy(PurpleBlistNode *selected, PurpleGroup *grp)
{
	purple_blist_request_add_buddy(NULL, NULL, grp ? grp->name : NULL, NULL);
}

static void
finch_add_group(PurpleBlistNode *selected, PurpleGroup *grp)
{
	purple_blist_request_add_group();
}

static void
finch_add_chat(PurpleBlistNode *selected, PurpleGroup *grp)
{
	purple_blist_request_add_chat(NULL, grp, NULL, NULL);
}

static void
create_group_menu(GntMenu *menu, PurpleGroup *group)
{
	add_custom_action(menu, _("Add Buddy"),
			PURPLE_CALLBACK(finch_add_buddy), group);
	add_custom_action(menu, _("Add Chat"),
			PURPLE_CALLBACK(finch_add_chat), group);
	add_custom_action(menu, _("Add Group"),
			PURPLE_CALLBACK(finch_add_group), group);
}

gpointer finch_retrieve_user_info(PurpleConnection *conn, const char *name)
{
	PurpleNotifyUserInfo *info = purple_notify_user_info_new();
	gpointer uihandle;
	purple_notify_user_info_add_pair(info, _("Information"), _("Retrieving..."));
	uihandle = purple_notify_userinfo(conn, name, info, NULL, NULL);
	purple_notify_user_info_destroy(info);

	serv_get_info(conn, name);
	return uihandle;
}

static void
finch_blist_get_buddy_info_cb(PurpleBlistNode *selected, PurpleBuddy *buddy)
{
	finch_retrieve_user_info(buddy->account->gc, purple_buddy_get_name(buddy));
}

static void
finch_blist_menu_send_file_cb(PurpleBlistNode *selected, PurpleBuddy *buddy)
{
	serv_send_file(buddy->account->gc, buddy->name, NULL);
}

static void
finch_blist_pounce_node_cb(PurpleBlistNode *selected, PurpleBlistNode *node)
{
	PurpleBuddy *b;
	if (PURPLE_BLIST_NODE_IS_CONTACT(node))
		b = purple_contact_get_priority_buddy((PurpleContact *)node);
	else
		b = (PurpleBuddy *)node;
	finch_pounce_editor_show(b->account, b->name, NULL);
}


static void
create_buddy_menu(GntMenu *menu, PurpleBuddy *buddy)
{
	PurplePluginProtocolInfo *prpl_info;

	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(buddy->account->gc->prpl);
	if (prpl_info && prpl_info->get_info)
	{
		add_custom_action(menu, _("Get Info"),
				PURPLE_CALLBACK(finch_blist_get_buddy_info_cb), buddy);
	}

	add_custom_action(menu, _("Add Buddy Pounce"),
			PURPLE_CALLBACK(finch_blist_pounce_node_cb), buddy);

	if (prpl_info && prpl_info->send_file)
	{
		if (!prpl_info->can_receive_file ||
			prpl_info->can_receive_file(buddy->account->gc, buddy->name))
			add_custom_action(menu, _("Send File"),
					PURPLE_CALLBACK(finch_blist_menu_send_file_cb), buddy);
	}
#if 0
	add_custom_action(tree, _("View Log"),
			PURPLE_CALLBACK(finch_blist_view_log_cb)), buddy);
#endif

	/* Protocol actions */
	append_proto_menu(menu,
			purple_account_get_connection(purple_buddy_get_account(buddy)),
			(PurpleBlistNode*)buddy);
}

static void
append_extended_menu(GntMenu *menu, PurpleBlistNode *node)
{
	GList *iter;

	for (iter = purple_blist_node_get_extended_menu(node);
			iter; iter = g_list_delete_link(iter, iter))
	{
		gnt_append_menu_action(menu, iter->data, NULL);
	}
}

/* Xerox'd from gtkdialogs.c:purple_gtkdialogs_remove_contact_cb */
static void
remove_contact(PurpleContact *contact)
{
	PurpleBlistNode *bnode, *cnode;
	PurpleGroup *group;

	cnode = (PurpleBlistNode *)contact;
	group = (PurpleGroup*)cnode->parent;
	for (bnode = cnode->child; bnode; bnode = bnode->next) {
		PurpleBuddy *buddy = (PurpleBuddy*)bnode;
		if (purple_account_is_connected(buddy->account))
			purple_account_remove_buddy(buddy->account, buddy, group);
	}
	purple_blist_remove_contact(contact);
}

static void
rename_blist_node(PurpleBlistNode *node, const char *newname)
{
	const char *name = newname;
	if (name && !*name)
		name = NULL;

	if (PURPLE_BLIST_NODE_IS_CONTACT(node)) {
		PurpleContact *contact = (PurpleContact*)node;
		PurpleBuddy *buddy = purple_contact_get_priority_buddy(contact);
		purple_blist_alias_contact(contact, name);
		purple_blist_alias_buddy(buddy, name);
		serv_alias_buddy(buddy);
	} else if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
		purple_blist_alias_buddy((PurpleBuddy*)node, name);
		serv_alias_buddy((PurpleBuddy*)node);
	} else if (PURPLE_BLIST_NODE_IS_CHAT(node))
		purple_blist_alias_chat((PurpleChat*)node, name);
	else if (PURPLE_BLIST_NODE_IS_GROUP(node) && (name != NULL))
		purple_blist_rename_group((PurpleGroup*)node, name);
	else
		g_return_if_reached();
}

static void
finch_blist_rename_node_cb(PurpleBlistNode *selected, PurpleBlistNode *node)
{
	const char *name = NULL;
	char *prompt;
	const char *text;

	if (PURPLE_BLIST_NODE_IS_CONTACT(node))
		name = purple_contact_get_alias((PurpleContact*)node);
	else if (PURPLE_BLIST_NODE_IS_BUDDY(node))
		name = purple_buddy_get_contact_alias((PurpleBuddy*)node);
	else if (PURPLE_BLIST_NODE_IS_CHAT(node))
		name = purple_chat_get_name((PurpleChat*)node);
	else if (PURPLE_BLIST_NODE_IS_GROUP(node))
		name = ((PurpleGroup*)node)->name;
	else
		g_return_if_reached();

	prompt = g_strdup_printf(_("Please enter the new name for %s"), name);

	text = PURPLE_BLIST_NODE_IS_GROUP(node) ? _("Rename") : _("Set Alias");
	purple_request_input(node, text, prompt, _("Enter empty string to reset the name."),
			name, FALSE, FALSE, NULL, text, G_CALLBACK(rename_blist_node),
			_("Cancel"), NULL,
			NULL, NULL, NULL,
			node);

	g_free(prompt);
}

/* Xeroxed from gtkdialogs.c:purple_gtkdialogs_remove_group_cb*/
static void
remove_group(PurpleGroup *group)
{
	PurpleBlistNode *cnode, *bnode;

	cnode = ((PurpleBlistNode*)group)->child;

	while (cnode) {
		if (PURPLE_BLIST_NODE_IS_CONTACT(cnode)) {
			bnode = cnode->child;
			cnode = cnode->next;
			while (bnode) {
				PurpleBuddy *buddy;
				if (PURPLE_BLIST_NODE_IS_BUDDY(bnode)) {
					buddy = (PurpleBuddy*)bnode;
					bnode = bnode->next;
					if (purple_account_is_connected(buddy->account)) {
						purple_account_remove_buddy(buddy->account, buddy, group);
						purple_blist_remove_buddy(buddy);
					}
				} else {
					bnode = bnode->next;
				}
			}
		} else if (PURPLE_BLIST_NODE_IS_CHAT(cnode)) {
			PurpleChat *chat = (PurpleChat *)cnode;
			cnode = cnode->next;
			if (purple_account_is_connected(chat->account))
				purple_blist_remove_chat(chat);
		} else {
			cnode = cnode->next;
		}
	}

	purple_blist_remove_group(group);
}

static void
finch_blist_remove_node(PurpleBlistNode *node)
{
	if (PURPLE_BLIST_NODE_IS_CONTACT(node)) {
		remove_contact((PurpleContact*)node);
	} else if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
		PurpleBuddy *buddy = (PurpleBuddy*)node;
		PurpleGroup *group = purple_buddy_get_group(buddy);
		purple_account_remove_buddy(purple_buddy_get_account(buddy), buddy, group);
		purple_blist_remove_buddy(buddy);
	} else if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
		purple_blist_remove_chat((PurpleChat*)node);
	} else if (PURPLE_BLIST_NODE_IS_GROUP(node)) {
		remove_group((PurpleGroup*)node);
	}
}

static void
finch_blist_remove_node_cb(PurpleBlistNode *selected, PurpleBlistNode *node)
{
	PurpleAccount *account = NULL;
	char *primary;
	const char *name, *sec = NULL;

	/* XXX: could be a contact */
	if (PURPLE_BLIST_NODE_IS_CONTACT(node)) {
		PurpleContact *c = (PurpleContact*)node;
		name = purple_contact_get_alias(c);
		if (c->totalsize > 1)
			sec = _("Removing this contact will also remove all the buddies in the contact");
	} else if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
		name = purple_buddy_get_name((PurpleBuddy*)node);
		account = purple_buddy_get_account((PurpleBuddy*)node);
	} else if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
		name = purple_chat_get_name((PurpleChat*)node);
	} else if (PURPLE_BLIST_NODE_IS_GROUP(node)) {
		name = ((PurpleGroup*)node)->name;
		sec = _("Removing this group will also remove all the buddies in the group");
	}
	else
		return;

	primary = g_strdup_printf(_("Are you sure you want to remove %s?"), name);

	/* XXX: anything to do with the returned ui-handle? */
	purple_request_action(node, _("Confirm Remove"),
			primary, sec,
			1,
			account, name, NULL,
			node, 2,
			_("Remove"), finch_blist_remove_node,
			_("Cancel"), NULL);
	g_free(primary);
}

static void
finch_blist_toggle_tag_buddy(PurpleBlistNode *node)
{
	GList *iter;
	if (node == NULL)
		return;
	if (ggblist->tagged && (iter = g_list_find(ggblist->tagged, node)) != NULL) {
		ggblist->tagged = g_list_delete_link(ggblist->tagged, iter);
	} else {
		ggblist->tagged = g_list_prepend(ggblist->tagged, node);
	}
	if (PURPLE_BLIST_NODE_IS_CONTACT(node))
		node = (PurpleBlistNode*)purple_contact_get_priority_buddy((PurpleContact*)node);
	if (PURPLE_BLIST_NODE_IS_BUDDY(node))
		update_buddy_display((PurpleBuddy*)node, ggblist);
	else
		update_node_display(node, ggblist);
}

static void
finch_blist_place_tagged(PurpleBlistNode *target)
{
	PurpleGroup *tg = NULL;
	PurpleContact *tc = NULL;

	if (target == NULL)
		return;

	if (PURPLE_BLIST_NODE_IS_GROUP(target))
		tg = (PurpleGroup*)target;
	else if (PURPLE_BLIST_NODE_IS_BUDDY(target)) {
		tc = (PurpleContact*)target->parent;
		tg = (PurpleGroup*)target->parent->parent;
	} else {
		if (PURPLE_BLIST_NODE_IS_CONTACT(target))
			tc = (PurpleContact*)target;
		tg = (PurpleGroup*)target->parent;
	}

	if (ggblist->tagged) {
		GList *list = ggblist->tagged;
		ggblist->tagged = NULL;
		while (list) {
			PurpleBlistNode *node = list->data;
			list = g_list_delete_link(list, list);

			if (PURPLE_BLIST_NODE_IS_GROUP(node)) {
				update_node_display(node, ggblist);
				/* Add the group after the current group */
				purple_blist_add_group((PurpleGroup*)node, (PurpleBlistNode*)tg);
			} else if (PURPLE_BLIST_NODE_IS_CONTACT(node)) {
				update_buddy_display(purple_contact_get_priority_buddy((PurpleContact*)node), ggblist);
				if ((PurpleBlistNode*)tg == target) {
					/* The target is a group, just add the contact to the group. */
					purple_blist_add_contact((PurpleContact*)node, tg, NULL);
				} else if (tc) {
					/* The target is either a buddy, or a contact. Merge with that contact. */
					purple_blist_merge_contact((PurpleContact*)node, (PurpleBlistNode*)tc);
				} else {
					/* The target is a chat. Add the contact to the group after this chat. */
					purple_blist_add_contact((PurpleContact*)node, NULL, target);
				}
			} else if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
				update_buddy_display((PurpleBuddy*)node, ggblist);
				if ((PurpleBlistNode*)tg == target) {
					/* The target is a group. Add this buddy in a new contact under this group. */
					purple_blist_add_buddy((PurpleBuddy*)node, NULL, tg, NULL);
				} else if (PURPLE_BLIST_NODE_IS_CONTACT(target)) {
					/* Add to the contact. */
					purple_blist_add_buddy((PurpleBuddy*)node, tc, NULL, NULL);
				} else if (PURPLE_BLIST_NODE_IS_BUDDY(target)) {
					/* Add to the contact after the selected buddy. */
					purple_blist_add_buddy((PurpleBuddy*)node, NULL, NULL, target);
				} else if (PURPLE_BLIST_NODE_IS_CHAT(target)) {
					/* Add to the selected chat's group. */
					purple_blist_add_buddy((PurpleBuddy*)node, NULL, tg, NULL);
				}
			} else if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
				update_node_display(node, ggblist);
				if ((PurpleBlistNode*)tg == target)
					purple_blist_add_chat((PurpleChat*)node, tg, NULL);
				else
					purple_blist_add_chat((PurpleChat*)node, NULL, target);
			}
		}
	}
}

static void
context_menu_destroyed(GntWidget *widget, FinchBlist *ggblist)
{
	ggblist->context = NULL;
}

static void
draw_context_menu(FinchBlist *ggblist)
{
	PurpleBlistNode *node = NULL;
	GntWidget *context = NULL;
	GntTree *tree = NULL;
	int x, y, top, width;
	char *title = NULL;

	if (ggblist->context)
		return;

	tree = GNT_TREE(ggblist->tree);

	node = gnt_tree_get_selection_data(tree);

	if (ggblist->tooltip)
		remove_tooltip(ggblist);

	ggblist->cnode = node;

	ggblist->context = context = gnt_menu_new(GNT_MENU_POPUP);
	g_signal_connect(G_OBJECT(context), "destroy", G_CALLBACK(context_menu_destroyed), ggblist);
	g_signal_connect(G_OBJECT(context), "hide", G_CALLBACK(gnt_widget_destroy), NULL);

	if (!node) {
		create_group_menu(GNT_MENU(context), NULL);
		title = g_strdup(_("Buddy List"));
	} else if (PURPLE_BLIST_NODE_IS_CONTACT(node)) {
		ggblist->cnode = (PurpleBlistNode*)purple_contact_get_priority_buddy((PurpleContact*)node);
		create_buddy_menu(GNT_MENU(context), (PurpleBuddy*)ggblist->cnode);
		title = g_strdup(purple_contact_get_alias((PurpleContact*)node));
	} else if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
		PurpleBuddy *buddy = (PurpleBuddy *)node;
		create_buddy_menu(GNT_MENU(context), buddy);
		title = g_strdup(purple_buddy_get_name(buddy));
	} else if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
		PurpleChat *chat = (PurpleChat*)node;
		create_chat_menu(GNT_MENU(context), chat);
		title = g_strdup(purple_chat_get_name(chat));
	} else if (PURPLE_BLIST_NODE_IS_GROUP(node)) {
		PurpleGroup *group = (PurpleGroup *)node;
		create_group_menu(GNT_MENU(context), group);
		title = g_strdup(group->name);
	}

	append_extended_menu(GNT_MENU(context), node);

	/* These are common for everything */
	if (node) {
		add_custom_action(GNT_MENU(context),
				PURPLE_BLIST_NODE_IS_GROUP(node) ? _("Rename") : _("Alias"),
				PURPLE_CALLBACK(finch_blist_rename_node_cb), node);
		add_custom_action(GNT_MENU(context), _("Remove"),
				PURPLE_CALLBACK(finch_blist_remove_node_cb), node);

		if (ggblist->tagged && (PURPLE_BLIST_NODE_IS_CONTACT(node)
				|| PURPLE_BLIST_NODE_IS_GROUP(node))) {
			add_custom_action(GNT_MENU(context), _("Place tagged"),
					PURPLE_CALLBACK(finch_blist_place_tagged), node);
		}

		if (PURPLE_BLIST_NODE_IS_BUDDY(node) || PURPLE_BLIST_NODE_IS_CONTACT(node)) {
			add_custom_action(GNT_MENU(context), _("Toggle Tag"),
					PURPLE_CALLBACK(finch_blist_toggle_tag_buddy), node);
		}
	}

	/* Set the position for the popup */
	gnt_widget_get_position(GNT_WIDGET(tree), &x, &y);
	gnt_widget_get_size(GNT_WIDGET(tree), &width, NULL);
	top = gnt_tree_get_selection_visible_line(tree);

	x += width;
	y += top - 1;

	gnt_widget_set_position(context, x, y);
	gnt_screen_menu_show(GNT_MENU(context));
	g_free(title);
}

static void
tooltip_for_buddy(PurpleBuddy *buddy, GString *str, gboolean full)
{
	PurplePlugin *prpl;
	PurplePluginProtocolInfo *prpl_info;
	PurpleAccount *account;
	PurpleNotifyUserInfo *user_info;
	PurplePresence *presence;
	const char *alias = purple_buddy_get_alias(buddy);
	char *tmp, *strip;

	user_info = purple_notify_user_info_new();

	account = purple_buddy_get_account(buddy);
	presence = purple_buddy_get_presence(buddy);

	if (!full || g_utf8_collate(purple_buddy_get_name(buddy), alias)) {
		char *esc = g_markup_escape_text(alias, -1);
		purple_notify_user_info_add_pair(user_info, _("Nickname"), esc);
		g_free(esc);
	}

	tmp = g_strdup_printf("%s (%s)",
			purple_account_get_username(account),
			purple_account_get_protocol_name(account));
	purple_notify_user_info_add_pair(user_info, _("Account"), tmp);
	g_free(tmp);
	
	prpl = purple_find_prpl(purple_account_get_protocol_id(account));
	prpl_info = PURPLE_PLUGIN_PROTOCOL_INFO(prpl);
	if (prpl_info && prpl_info->tooltip_text) {
		prpl_info->tooltip_text(buddy, user_info, full);
	}

	if (purple_prefs_get_bool("/finch/blist/idletime")) {
		PurplePresence *pre = purple_buddy_get_presence(buddy);
		if (purple_presence_is_idle(pre)) {
			time_t idle = purple_presence_get_idle_time(pre);
			if (idle > 0) {
				char *st = purple_str_seconds_to_string(time(NULL) - idle);
				purple_notify_user_info_add_pair(user_info, _("Idle"), st);
				g_free(st);
			}
		}
	}
	
	tmp = purple_notify_user_info_get_text_with_newline(user_info, "<BR>");
	purple_notify_user_info_destroy(user_info);

	strip = purple_markup_strip_html(tmp);
	g_string_append(str, strip);

	if (purple_presence_is_status_primitive_active(presence, PURPLE_STATUS_MOBILE)) {
		g_string_append(str, "\n");
		g_string_append(str, _("On Mobile"));
	}

	g_free(strip);
	g_free(tmp);
}

static GString*
make_sure_text_fits(GString *string)
{
	int maxw = getmaxx(stdscr) - 3;
	char *str = gnt_util_onscreen_fit_string(string->str, maxw);
	string = g_string_assign(string, str);
	g_free(str);
	return string;
}

static gboolean
draw_tooltip_real(FinchBlist *ggblist)
{
	PurpleBlistNode *node;
	int x, y, top, width, w, h;
	GString *str;
	GntTree *tree;
	GntWidget *widget, *box, *tv;
	char *title = NULL;
	int lastseen = 0;

	widget = ggblist->tree;
	tree = GNT_TREE(widget);

	if (!gnt_widget_has_focus(ggblist->tree) || 
			(ggblist->context && !GNT_WIDGET_IS_FLAG_SET(ggblist->context, GNT_WIDGET_INVISIBLE)))
		return FALSE;

	if (ggblist->tooltip)
	{
		/* XXX: Once we can properly redraw on expose events, this can be removed at the end
		 * to avoid the blinking*/
		remove_tooltip(ggblist);
	}

	node = gnt_tree_get_selection_data(tree);
	if (!node)
		return FALSE;

	str = g_string_new("");

	if (PURPLE_BLIST_NODE_IS_CONTACT(node)) {
		PurpleBuddy *pr = purple_contact_get_priority_buddy((PurpleContact*)node);
		gboolean offline = !PURPLE_BUDDY_IS_ONLINE(pr);
		gboolean showoffline = purple_prefs_get_bool(PREF_ROOT "/showoffline");
		const char *name = purple_buddy_get_name(pr);

		title = g_strdup(name);
		tooltip_for_buddy(pr, str, TRUE);
		for (node = node->child; node; node = node->next) {
			PurpleBuddy *buddy = (PurpleBuddy*)node;
			if (offline) {
				int value = purple_blist_node_get_int(node, "last_seen");
				if (value > lastseen)
					lastseen = value;
			}
			if (node == (PurpleBlistNode*)pr)
				continue;
			if (!purple_account_is_connected(buddy->account))
				continue;
			if (!showoffline && !PURPLE_BUDDY_IS_ONLINE(buddy))
				continue;
			str = g_string_append(str, "\n----------\n");
			tooltip_for_buddy(buddy, str, FALSE);
		}
	} else if (PURPLE_BLIST_NODE_IS_BUDDY(node)) {
		PurpleBuddy *buddy = (PurpleBuddy *)node;
		tooltip_for_buddy(buddy, str, TRUE);
		title = g_strdup(purple_buddy_get_name(buddy));
		if (!PURPLE_BUDDY_IS_ONLINE((PurpleBuddy*)node))
			lastseen = purple_blist_node_get_int(node, "last_seen");
	} else if (PURPLE_BLIST_NODE_IS_GROUP(node)) {
		PurpleGroup *group = (PurpleGroup *)node;

		g_string_append_printf(str, _("Online: %d\nTotal: %d"),
						purple_blist_get_group_online_count(group),
						purple_blist_get_group_size(group, FALSE));

		title = g_strdup(group->name);
	} else if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
		PurpleChat *chat = (PurpleChat *)node;
		PurpleAccount *account = chat->account;

		g_string_append_printf(str, _("Account: %s (%s)"),
				purple_account_get_username(account),
				purple_account_get_protocol_name(account));

		title = g_strdup(purple_chat_get_name(chat));
	} else {
		g_string_free(str, TRUE);
		return FALSE;
	}

	if (lastseen > 0) {
		char *tmp = purple_str_seconds_to_string(time(NULL) - lastseen);
		g_string_append_printf(str, _("\nLast Seen: %s ago"), tmp);
		g_free(tmp);
	}

	gnt_widget_get_position(widget, &x, &y);
	gnt_widget_get_size(widget, &width, NULL);
	top = gnt_tree_get_selection_visible_line(tree);

	x += width;
	y += top - 1;

	box = gnt_box_new(FALSE, FALSE);
	gnt_box_set_toplevel(GNT_BOX(box), TRUE);
	GNT_WIDGET_SET_FLAGS(box, GNT_WIDGET_NO_SHADOW);
	gnt_box_set_title(GNT_BOX(box), title);

	str = make_sure_text_fits(str);
	gnt_util_get_text_bound(str->str, &w, &h);
	h = MAX(1, h);
	tv = gnt_text_view_new();
	gnt_widget_set_size(tv, w + 1, h);
	gnt_text_view_set_flag(GNT_TEXT_VIEW(tv), GNT_TEXT_VIEW_NO_SCROLL);
	gnt_box_add_widget(GNT_BOX(box), tv);

	gnt_widget_set_position(box, x, y);
	GNT_WIDGET_UNSET_FLAGS(box, GNT_WIDGET_CAN_TAKE_FOCUS);
	GNT_WIDGET_SET_FLAGS(box, GNT_WIDGET_TRANSIENT);
	gnt_widget_draw(box);

	gnt_text_view_append_text_with_flags(GNT_TEXT_VIEW(tv), str->str, GNT_TEXT_FLAG_NORMAL);
	gnt_text_view_scroll(GNT_TEXT_VIEW(tv), 0);

	g_free(title);
	g_string_free(str, TRUE);
	ggblist->tooltip = box;
	ggblist->tnode = node;

	gnt_widget_set_name(ggblist->tooltip, "tooltip");
	return FALSE;
}

static void
draw_tooltip(FinchBlist *ggblist)
{
	/* When an account has signed off, it removes one buddy at a time.
	 * Drawing the tooltip after removing each buddy is expensive. On
	 * top of that, if the selected buddy belongs to the disconnected
	 * account, then retreiving the tooltip for that causes crash. So
	 * let's make sure we wait for all the buddies to be removed first.*/
	int id = g_timeout_add(0, (GSourceFunc)draw_tooltip_real, ggblist);
	g_object_set_data_full(G_OBJECT(ggblist->window), "draw_tooltip_calback",
				GINT_TO_POINTER(id), (GDestroyNotify)g_source_remove);
}

static void
selection_changed(GntWidget *widget, gpointer old, gpointer current, FinchBlist *ggblist)
{
	remove_peripherals(ggblist);
	draw_tooltip(ggblist);
}

static gboolean
context_menu(GntWidget *widget, FinchBlist *ggblist)
{
	draw_context_menu(ggblist);
	return TRUE;
}

static gboolean
key_pressed(GntWidget *widget, const char *text, FinchBlist *ggblist)
{
	if (text[0] == 27 && text[1] == 0) {
		/* Escape was pressed */
		if (gnt_tree_is_searching(GNT_TREE(ggblist->tree)))
			gnt_bindable_perform_action_named(GNT_BINDABLE(ggblist->tree), "end-search", NULL);
		remove_peripherals(ggblist);
	} else if (strcmp(text, GNT_KEY_CTRL_O) == 0) {
		purple_prefs_set_bool(PREF_ROOT "/showoffline",
				!purple_prefs_get_bool(PREF_ROOT "/showoffline"));
	} else if (strcmp(text, GNT_KEY_INS) == 0) {
		purple_blist_request_add_buddy(NULL, NULL, NULL, NULL);
	} else if (!gnt_tree_is_searching(GNT_TREE(ggblist->tree))) {
		if (strcmp(text, "t") == 0) {
			finch_blist_toggle_tag_buddy(gnt_tree_get_selection_data(GNT_TREE(ggblist->tree)));
			gnt_bindable_perform_action_named(GNT_BINDABLE(ggblist->tree), "move-down");
		} else if (strcmp(text, "a") == 0) {
			finch_blist_place_tagged(gnt_tree_get_selection_data(GNT_TREE(ggblist->tree)));
		} else
			return FALSE;
	} else
		return FALSE;

	return TRUE;
}

static void
update_node_display(PurpleBlistNode *node, FinchBlist *ggblist)
{
	GntTextFormatFlags flag = 0;
	if (ggblist->tagged && g_list_find(ggblist->tagged, node))
		flag |= GNT_TEXT_FLAG_BOLD;
	gnt_tree_set_row_flags(GNT_TREE(ggblist->tree), node, flag);
}

static void
update_buddy_display(PurpleBuddy *buddy, FinchBlist *ggblist)
{
	PurpleContact *contact;
	GntTextFormatFlags bflag = 0, cflag = 0;
	
	contact = purple_buddy_get_contact(buddy);

	gnt_tree_change_text(GNT_TREE(ggblist->tree), buddy, 0, get_display_name((PurpleBlistNode*)buddy));
	gnt_tree_change_text(GNT_TREE(ggblist->tree), contact, 0, get_display_name((PurpleBlistNode*)contact));

	if (ggblist->tagged && g_list_find(ggblist->tagged, buddy))
		bflag |= GNT_TEXT_FLAG_BOLD;
	if (ggblist->tagged && g_list_find(ggblist->tagged, contact))
		cflag |= GNT_TEXT_FLAG_BOLD;

	if (ggblist->tnode == (PurpleBlistNode*)buddy)
		draw_tooltip(ggblist);

	if (purple_presence_is_idle(purple_buddy_get_presence(buddy))) {
		gnt_tree_set_row_color(GNT_TREE(ggblist->tree), buddy, color_idle);
		gnt_tree_set_row_color(GNT_TREE(ggblist->tree), contact, color_idle);
		if (buddy == purple_contact_get_priority_buddy(contact))
			gnt_tree_set_row_color(GNT_TREE(ggblist->tree), contact, color_idle);
	} else if (purple_presence_is_available(purple_buddy_get_presence(buddy)) && color_available) {
		gnt_tree_set_row_color(GNT_TREE(ggblist->tree), buddy, color_available);
		gnt_tree_set_row_color(GNT_TREE(ggblist->tree), contact, color_available);
		if (buddy == purple_contact_get_priority_buddy(contact))
			gnt_tree_set_row_color(GNT_TREE(ggblist->tree), contact, color_available);
	} else if (purple_presence_is_online(purple_buddy_get_presence(buddy)) &&
			!purple_presence_is_available(purple_buddy_get_presence(buddy)) && color_away) {
		gnt_tree_set_row_color(GNT_TREE(ggblist->tree), buddy, color_away);
		gnt_tree_set_row_color(GNT_TREE(ggblist->tree), contact, color_away);
		if (buddy == purple_contact_get_priority_buddy(contact))
			gnt_tree_set_row_color(GNT_TREE(ggblist->tree), contact, color_away);
	} else if (!purple_presence_is_online(purple_buddy_get_presence(buddy)) && color_offline) {
		gnt_tree_set_row_color(GNT_TREE(ggblist->tree), buddy, color_offline);
		gnt_tree_set_row_color(GNT_TREE(ggblist->tree), contact, color_offline);
		if (buddy == purple_contact_get_priority_buddy(contact))
			gnt_tree_set_row_color(GNT_TREE(ggblist->tree), contact, color_offline);
	}
	gnt_tree_set_row_flags(GNT_TREE(ggblist->tree), buddy, bflag);
	if (buddy == purple_contact_get_priority_buddy(contact))
		gnt_tree_set_row_flags(GNT_TREE(ggblist->tree), contact, cflag);

	if (buddy != purple_contact_get_priority_buddy(contact))
			update_buddy_display(purple_contact_get_priority_buddy(contact), ggblist);
}

static void
buddy_status_changed(PurpleBuddy *buddy, PurpleStatus *old, PurpleStatus *now, FinchBlist *ggblist)
{
	update_buddy_display(buddy, ggblist);
}

static void
buddy_idle_changed(PurpleBuddy *buddy, int old, int new, FinchBlist *ggblist)
{
	update_buddy_display(buddy, ggblist);
}

static void
remove_peripherals(FinchBlist *ggblist)
{
	if (ggblist->tooltip)
		remove_tooltip(ggblist);
	else if (ggblist->context)
		gnt_widget_destroy(ggblist->context);
}

static void
size_changed_cb(GntWidget *w, int wi, int h)
{
	int width, height;
	gnt_widget_get_size(w, &width, &height);
	purple_prefs_set_int(PREF_ROOT "/size/width", width);
	purple_prefs_set_int(PREF_ROOT "/size/height", height);
}

static void
save_position_cb(GntWidget *w, int x, int y)
{
	purple_prefs_set_int(PREF_ROOT "/position/x", x);
	purple_prefs_set_int(PREF_ROOT "/position/y", y);
}

static void
reset_blist_window(GntWidget *window, gpointer null)
{
	PurpleBlistNode *node;
	purple_signals_disconnect_by_handle(finch_blist_get_handle());
	purple_get_blist()->ui_data = NULL;

	node = purple_blist_get_root();
	while (node) {
		node->ui_data = NULL;
		node = purple_blist_node_next(node, TRUE);
	}

	if (ggblist->typing)
		g_source_remove(ggblist->typing);
	remove_peripherals(ggblist);
	if (ggblist->tagged)
		g_list_free(ggblist->tagged);
	g_free(ggblist);
	ggblist = NULL;
}

static void
populate_buddylist()
{
	PurpleBlistNode *node;
	PurpleBuddyList *list;

	if (strcmp(purple_prefs_get_string(PREF_ROOT "/sort_type"), "text") == 0) {
		gnt_tree_set_compare_func(GNT_TREE(ggblist->tree),
			(GCompareFunc)blist_node_compare_text);
	} else if (strcmp(purple_prefs_get_string(PREF_ROOT "/sort_type"), "status") == 0) {
		gnt_tree_set_compare_func(GNT_TREE(ggblist->tree),
			(GCompareFunc)blist_node_compare_status);
	} else if (strcmp(purple_prefs_get_string(PREF_ROOT "/sort_type"), "log") == 0) {
		gnt_tree_set_compare_func(GNT_TREE(ggblist->tree),
			(GCompareFunc)blist_node_compare_log);
	}
	
	list = purple_get_blist();
	node = purple_blist_get_root();
	while (node)
	{
		node_update(list, node);
		node = purple_blist_node_next(node, FALSE);
	}
}

static void
destroy_status_list(GList *list)
{
	g_list_foreach(list, (GFunc)g_free, NULL);
	g_list_free(list);
}

static void
populate_status_dropdown()
{
	int i;
	GList *iter;
	GList *items = NULL;
	StatusBoxItem *item = NULL;

	/* First the primitives */
	PurpleStatusPrimitive prims[] = {PURPLE_STATUS_AVAILABLE, PURPLE_STATUS_AWAY,
			PURPLE_STATUS_INVISIBLE, PURPLE_STATUS_OFFLINE, PURPLE_STATUS_UNSET};

	gnt_combo_box_remove_all(GNT_COMBO_BOX(ggblist->status));

	for (i = 0; prims[i] != PURPLE_STATUS_UNSET; i++)
	{
		item = g_new0(StatusBoxItem, 1);
		item->type = STATUS_PRIMITIVE;
		item->u.prim = prims[i];
		items = g_list_prepend(items, item);
		gnt_combo_box_add_data(GNT_COMBO_BOX(ggblist->status), item,
				purple_primitive_get_name_from_type(prims[i]));
	}

	/* Now the popular statuses */
	for (iter = purple_savedstatuses_get_popular(6); iter; iter = g_list_delete_link(iter, iter))
	{
		item = g_new0(StatusBoxItem, 1);
		item->type = STATUS_SAVED_POPULAR;
		item->u.saved = iter->data;
		items = g_list_prepend(items, item);
		gnt_combo_box_add_data(GNT_COMBO_BOX(ggblist->status), item,
				purple_savedstatus_get_title(iter->data));
	}

	/* New savedstatus */
	item = g_new0(StatusBoxItem, 1);
	item->type = STATUS_SAVED_NEW;
	items = g_list_prepend(items, item);
	gnt_combo_box_add_data(GNT_COMBO_BOX(ggblist->status), item,
			_("New..."));

	/* More savedstatuses */
	item = g_new0(StatusBoxItem, 1);
	item->type = STATUS_SAVED_ALL;
	items = g_list_prepend(items, item);
	gnt_combo_box_add_data(GNT_COMBO_BOX(ggblist->status), item,
			_("Saved..."));

	/* The keys for the combobox are created here, and never used
	 * anywhere else. So make sure the keys are freed when the widget
	 * is destroyed. */
	g_object_set_data_full(G_OBJECT(ggblist->status), "list of statuses",
			items, (GDestroyNotify)destroy_status_list);
}

static void
redraw_blist(const char *name, PurplePrefType type, gconstpointer val, gpointer data)
{
	PurpleBlistNode *node, *sel;
	if (ggblist == NULL || ggblist->window == NULL)
		return;

	sel = gnt_tree_get_selection_data(GNT_TREE(ggblist->tree));
	gnt_tree_remove_all(GNT_TREE(ggblist->tree));
	node = purple_blist_get_root();
	for (; node; node = purple_blist_node_next(node, TRUE))
		node->ui_data = NULL;
	populate_buddylist();
	gnt_tree_set_selected(GNT_TREE(ggblist->tree), sel);
	draw_tooltip(ggblist);
}

static int
get_color(char *key)
{
#if GLIB_CHECK_VERSION(2,6,0)
	int fg = 0, bg = 0;
	gsize n;
	char **vals;
	vals = gnt_style_get_string_list(NULL, key, &n);
	if (vals && n == 2) {
		fg = gnt_colors_get_color(vals[0]);
		bg = gnt_colors_get_color(vals[1]);
		return gnt_color_add_pair(fg, bg);
	}
	return 0;
#else
	return 0;
#endif
}

void finch_blist_init()
{
	color_available = get_color("color-available");
	if (!color_available)
		color_available = gnt_color_add_pair(COLOR_GREEN, -1);
	color_away = get_color("color-away");
	if (!color_away)
		color_away = gnt_color_add_pair(COLOR_BLUE, -1);
	color_idle = get_color("color-idle");
	if (!color_idle)
		color_idle = gnt_color_add_pair(COLOR_CYAN, -1);
	color_offline = get_color("color-offline");
	if (!color_offline)
		color_offline = gnt_color_add_pair(COLOR_RED, -1);

	purple_prefs_add_none(PREF_ROOT);
	purple_prefs_add_none(PREF_ROOT "/size");
	purple_prefs_add_int(PREF_ROOT "/size/width", 20);
	purple_prefs_add_int(PREF_ROOT "/size/height", 17);
	purple_prefs_add_none(PREF_ROOT "/position");
	purple_prefs_add_int(PREF_ROOT "/position/x", 0);
	purple_prefs_add_int(PREF_ROOT "/position/y", 0);
	purple_prefs_add_bool(PREF_ROOT "/idletime", TRUE);
	purple_prefs_add_bool(PREF_ROOT "/showoffline", FALSE);
	purple_prefs_add_bool(PREF_ROOT "/emptygroups", FALSE);
	purple_prefs_add_string(PREF_ROOT "/sort_type", "text");

	purple_prefs_connect_callback(finch_blist_get_handle(),
			PREF_ROOT "/emptygroups", redraw_blist, NULL);
	purple_prefs_connect_callback(finch_blist_get_handle(),
			PREF_ROOT "/showoffline", redraw_blist, NULL);
	purple_prefs_connect_callback(finch_blist_get_handle(),
			PREF_ROOT "/sort_type", redraw_blist, NULL);

	purple_signal_connect(purple_connections_get_handle(), "signed-on", purple_blist_get_handle(),
			G_CALLBACK(account_signed_on_cb), NULL);
	return;
}

static gboolean
remove_typing_cb(gpointer null)
{
	PurpleSavedStatus *current;
	const char *message, *newmessage;
	PurpleStatusPrimitive prim, newprim;
	StatusBoxItem *item;

	current = purple_savedstatus_get_current();
	message = purple_savedstatus_get_message(current);
	prim = purple_savedstatus_get_type(current);

	newmessage = gnt_entry_get_text(GNT_ENTRY(ggblist->statustext));
	item = gnt_combo_box_get_selected_data(GNT_COMBO_BOX(ggblist->status));

	switch (item->type) {
		case STATUS_PRIMITIVE:
			newprim = item->u.prim;
			break;
		case STATUS_SAVED_POPULAR:
			newprim = purple_savedstatus_get_type(item->u.saved);
			break;
		default:
			goto end;  /* 'New' or 'Saved' is selected, but this should never happen. */
	}

	if (newprim != prim || ((message && !newmessage) ||
				(!message && newmessage) ||
				(message && newmessage && g_utf8_collate(message, newmessage) != 0)))
	{
		PurpleSavedStatus *status = purple_savedstatus_find_transient_by_type_and_message(newprim, newmessage);
									/* Holy Crap! That's a LAWNG function name */
		if (status == NULL)
		{
			status = purple_savedstatus_new(NULL, newprim);
			purple_savedstatus_set_message(status, newmessage);
		}

		purple_savedstatus_activate(status);
	}

	gnt_box_give_focus_to_child(GNT_BOX(ggblist->window), ggblist->tree);
end:
	if (ggblist->typing)
		g_source_remove(ggblist->typing);
	ggblist->typing = 0;
	return FALSE;
}

static void
status_selection_changed(GntComboBox *box, StatusBoxItem *old, StatusBoxItem *now, gpointer null)
{
	gnt_entry_set_text(GNT_ENTRY(ggblist->statustext), NULL);
	if (now->type == STATUS_SAVED_POPULAR)
	{
		/* Set the status immediately */
		purple_savedstatus_activate(now->u.saved);
	}
	else if (now->type == STATUS_PRIMITIVE)
	{
		/* Move the focus to the entry box */
		/* XXX: Make sure the selected status can have a message */
		gnt_box_move_focus(GNT_BOX(ggblist->window), 1);
		ggblist->typing = g_timeout_add(TYPING_TIMEOUT, (GSourceFunc)remove_typing_cb, NULL);
	}
	else if (now->type == STATUS_SAVED_ALL)
	{
		/* Restore the selection to reflect current status. */
		savedstatus_changed(purple_savedstatus_get_current(), NULL);
		gnt_box_give_focus_to_child(GNT_BOX(ggblist->window), ggblist->tree);
		finch_savedstatus_show_all();
	}
	else if (now->type == STATUS_SAVED_NEW)
	{
		savedstatus_changed(purple_savedstatus_get_current(), NULL);
		gnt_box_give_focus_to_child(GNT_BOX(ggblist->window), ggblist->tree);
		finch_savedstatus_edit(NULL);
	}
	else
		g_return_if_reached();
}

static gboolean
status_text_changed(GntEntry *entry, const char *text, gpointer null)
{
	if ((text[0] == 27 || (text[0] == '\t' && text[1] == '\0')) && ggblist->typing == 0)
		return FALSE;

	if (ggblist->typing)
		g_source_remove(ggblist->typing);
	ggblist->typing = 0;

	if (text[0] == '\r' && text[1] == 0)
	{
		/* Set the status only after you press 'Enter' */
		remove_typing_cb(NULL);
		return TRUE;
	}

	ggblist->typing = g_timeout_add(TYPING_TIMEOUT, (GSourceFunc)remove_typing_cb, NULL);
	return FALSE;
}

static void
savedstatus_changed(PurpleSavedStatus *now, PurpleSavedStatus *old)
{
	GList *list;
	PurpleStatusPrimitive prim;
	const char *message;
	gboolean found = FALSE, saved = TRUE;

	if (!ggblist)
		return;

	/* Block the signals we don't want to emit */
	g_signal_handlers_block_matched(ggblist->status, G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, status_selection_changed, NULL);
	g_signal_handlers_block_matched(ggblist->statustext, G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, status_text_changed, NULL);

	prim = purple_savedstatus_get_type(now);
	message = purple_savedstatus_get_message(now);

	/* Rebuild the status dropdown */
	populate_status_dropdown();

	while (!found) {
		list = g_object_get_data(G_OBJECT(ggblist->status), "list of statuses");
		for (; list; list = list->next)
		{
			StatusBoxItem *item = list->data;
			if ((saved && item->type != STATUS_PRIMITIVE && item->u.saved == now) ||
					(!saved && item->type == STATUS_PRIMITIVE && item->u.prim == prim))
			{
				char *mess = purple_unescape_html(message);
				gnt_combo_box_set_selected(GNT_COMBO_BOX(ggblist->status), item);
				gnt_entry_set_text(GNT_ENTRY(ggblist->statustext), mess);
				gnt_widget_draw(ggblist->status);
				g_free(mess);
				found = TRUE;
				break;
			}
		}
		if (!saved)
			break;
		saved = FALSE;
	}

	g_signal_handlers_unblock_matched(ggblist->status, G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, status_selection_changed, NULL);
	g_signal_handlers_unblock_matched(ggblist->statustext, G_SIGNAL_MATCH_FUNC,
			0, 0, NULL, status_text_changed, NULL);
}

static int
blist_node_compare_position(PurpleBlistNode *n1, PurpleBlistNode *n2)
{
	while ((n1 = n1->prev) != NULL)
		if (n1 == n2)
			return 1;
	return -1;
}

static int
blist_node_compare_text(PurpleBlistNode *n1, PurpleBlistNode *n2)
{
	const char *s1, *s2;
	char *us1, *us2;
	int ret;

	if (n1->type != n2->type)
		return blist_node_compare_position(n1, n2);

	switch (n1->type)
	{
		case PURPLE_BLIST_CHAT_NODE:
			s1 = purple_chat_get_name((PurpleChat*)n1);
			s2 = purple_chat_get_name((PurpleChat*)n2);
			break;
		case PURPLE_BLIST_BUDDY_NODE:
			return purple_presence_compare(purple_buddy_get_presence((PurpleBuddy*)n1),
					purple_buddy_get_presence((PurpleBuddy*)n2));
			break;
		case PURPLE_BLIST_CONTACT_NODE:
			s1 = purple_contact_get_alias((PurpleContact*)n1);
			s2 = purple_contact_get_alias((PurpleContact*)n2);
			break;
		default:
			return blist_node_compare_position(n1, n2);
	}

	us1 = g_utf8_strup(s1, -1);
	us2 = g_utf8_strup(s2, -1);
	ret = g_utf8_collate(us1, us2);
	g_free(us1);
	g_free(us2);

	return ret;
}

static int
blist_node_compare_status(PurpleBlistNode *n1, PurpleBlistNode *n2)
{
	int ret;

	if (n1->type != n2->type)
		return blist_node_compare_position(n1, n2);

	switch (n1->type) {
		case PURPLE_BLIST_CONTACT_NODE:
			n1 = (PurpleBlistNode*)purple_contact_get_priority_buddy((PurpleContact*)n1);
			n2 = (PurpleBlistNode*)purple_contact_get_priority_buddy((PurpleContact*)n2);
			/* now compare the presence of the priority buddies */
		case PURPLE_BLIST_BUDDY_NODE:
			ret = purple_presence_compare(purple_buddy_get_presence((PurpleBuddy*)n1),
					purple_buddy_get_presence((PurpleBuddy*)n2));
			if (ret != 0)
				return ret;
			break;
		default:
			return blist_node_compare_position(n1, n2);
			break;
	}

	/* Sort alphabetically if presence is not comparable */
	ret = blist_node_compare_text(n1, n2);

	return ret;
}

static int
get_contact_log_size(PurpleBlistNode *c)
{
	int log = 0;
	PurpleBlistNode *node;

	for (node = c->child; node; node = node->next) {
		PurpleBuddy *b = (PurpleBuddy*)node;
		log += purple_log_get_total_size(PURPLE_LOG_IM, b->name, b->account);
	}

	return log;
}

static int
blist_node_compare_log(PurpleBlistNode *n1, PurpleBlistNode *n2)
{
	int ret;
	PurpleBuddy *b1, *b2;

	if (n1->type != n2->type)
		return blist_node_compare_position(n1, n2);

	switch (n1->type) {
		case PURPLE_BLIST_BUDDY_NODE:
			b1 = (PurpleBuddy*)n1;
			b2 = (PurpleBuddy*)n2;
			ret = purple_log_get_total_size(PURPLE_LOG_IM, b2->name, b2->account) - 
					purple_log_get_total_size(PURPLE_LOG_IM, b1->name, b1->account);
			if (ret != 0)
				return ret;
			break;
		case PURPLE_BLIST_CONTACT_NODE:
			ret = get_contact_log_size(n2) - get_contact_log_size(n1);
			if (ret != 0)
				return ret;
			break;
		default:
			return blist_node_compare_position(n1, n2);
	}
	ret = blist_node_compare_text(n1, n2);
	return ret;
}

static gboolean
blist_clicked(GntTree *tree, GntMouseEvent event, int x, int y, gpointer ggblist)
{
	if (event == GNT_RIGHT_MOUSE_DOWN) {
		draw_context_menu(ggblist);
	}
	return FALSE;
}

static void
plugin_action(GntMenuItem *item, gpointer data)
{
	PurplePluginAction *action = data;
	if (action && action->callback)
		action->callback(action);
}

static void
build_plugin_actions(GntMenuItem *item, PurplePlugin *plugin, gpointer context)
{
	GntWidget *sub = gnt_menu_new(GNT_MENU_POPUP);
	GList *actions;
	GntMenuItem *menuitem;

	gnt_menuitem_set_submenu(item, GNT_MENU(sub));
	for (actions = PURPLE_PLUGIN_ACTIONS(plugin, context); actions;
			actions = g_list_delete_link(actions, actions)) {
		if (actions->data) {
			PurplePluginAction *action = actions->data;
			action->plugin = plugin;
			action->context = context;
			menuitem = gnt_menuitem_new(action->label);
			gnt_menu_add_item(GNT_MENU(sub), menuitem);

			gnt_menuitem_set_callback(menuitem, plugin_action, action);
			g_object_set_data_full(G_OBJECT(menuitem), "plugin_action",
								   action, (GDestroyNotify)purple_plugin_action_free);
		}
	}
}

static void
reconstruct_plugins_menu()
{
	GntWidget *sub;
	GntMenuItem *plg;
	GList *iter;

	if (!ggblist)
		return;

	if (ggblist->plugins == NULL)
		ggblist->plugins = gnt_menuitem_new(_("Plugins"));

	plg = ggblist->plugins;
	sub = gnt_menu_new(GNT_MENU_POPUP);
	gnt_menuitem_set_submenu(plg, GNT_MENU(sub));

	for (iter = purple_plugins_get_loaded(); iter; iter = iter->next) {
		PurplePlugin *plugin = iter->data;
		GntMenuItem *item;
		if (PURPLE_IS_PROTOCOL_PLUGIN(plugin))
			continue;

		if (!PURPLE_PLUGIN_HAS_ACTIONS(plugin))
			continue;

		item = gnt_menuitem_new(_(plugin->info->name));
		gnt_menu_add_item(GNT_MENU(sub), item);
		build_plugin_actions(item, plugin, NULL);
	}
}

static void
reconstruct_accounts_menu()
{
	GntWidget *sub;
	GntMenuItem *acc, *item;
	GList *iter;

	if (!ggblist)
		return;

	if (ggblist->accounts == NULL)
		ggblist->accounts = gnt_menuitem_new(_("Accounts"));

	acc = ggblist->accounts;
	sub = gnt_menu_new(GNT_MENU_POPUP);
	gnt_menuitem_set_submenu(acc, GNT_MENU(sub));

	for (iter = purple_accounts_get_all_active(); iter;
			iter = g_list_delete_link(iter, iter)) {
		PurpleAccount *account = iter->data;
		PurpleConnection *gc = purple_account_get_connection(account);
		PurplePlugin *prpl;
		
		if (!gc || !PURPLE_CONNECTION_IS_CONNECTED(gc))
			continue;
		prpl = gc->prpl;

		if (PURPLE_PLUGIN_HAS_ACTIONS(prpl)) {
			item = gnt_menuitem_new(purple_account_get_username(account));
			gnt_menu_add_item(GNT_MENU(sub), item);
			build_plugin_actions(item, prpl, gc);
		}
	}
}

static gboolean
auto_join_chats(gpointer data)
{
	PurpleBlistNode *node;
	PurpleConnection *pc = data;
	PurpleAccount *account = purple_connection_get_account(pc);

	for (node = purple_blist_get_root(); node;
			node = purple_blist_node_next(node, FALSE)) {
		if (PURPLE_BLIST_NODE_IS_CHAT(node)) {
			PurpleChat *chat = (PurpleChat*)node;
			if (chat->account == account &&
					purple_blist_node_get_bool(node, "gnt-autojoin"))
				serv_join_chat(purple_account_get_connection(chat->account), chat->components);
		}
	}
	return FALSE;
}

static void
account_signed_on_cb(PurpleConnection *gc, gpointer null)
{
	g_idle_add(auto_join_chats, gc);
}

static void toggle_pref_cb(GntMenuItem *item, gpointer n)
{
	purple_prefs_set_bool(n, !purple_prefs_get_bool(n));
}

static void sort_blist_change_cb(GntMenuItem *item, gpointer n)
{
	purple_prefs_set_string(PREF_ROOT "/sort_type", n);
}

/* send_im_select* -- Xerox */
static void
send_im_select_cb(gpointer data, PurpleRequestFields *fields)
{
	PurpleAccount *account;
	const char *username;

	account  = purple_request_fields_get_account(fields, "account");
	username = purple_request_fields_get_string(fields,  "screenname");

	purple_conversation_new(PURPLE_CONV_TYPE_IM, account, username);
}

static void
send_im_select(GntMenuItem *item, gpointer n)
{
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;

	fields = purple_request_fields_new();

	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_string_new("screenname", _("Name"), NULL, FALSE);
	purple_request_field_set_type_hint(field, "screenname");
	purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_account_new("account", _("Account"), NULL);
	purple_request_field_set_type_hint(field, "account");
	purple_request_field_set_visible(field,
		(purple_connections_get_all() != NULL &&
		 purple_connections_get_all()->next != NULL));
	purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	purple_request_fields(purple_get_blist(), _("New Instant Message"),
						NULL,
						_("Please enter the screen name or alias of the person "
						  "you would like to IM."),
						fields,
						_("OK"), G_CALLBACK(send_im_select_cb),
						_("Cancel"), NULL,
						NULL, NULL, NULL,
						NULL);
}

static void
join_chat_select_cb(gpointer data, PurpleRequestFields *fields)
{
	PurpleAccount *account;
	const char *name;
	PurpleConnection *gc;
	PurpleChat *chat;
	GHashTable *hash = NULL;

	account = purple_request_fields_get_account(fields, "account");
	name = purple_request_fields_get_string(fields,  "chat");

	if (!purple_account_is_connected(account))
		return;

	gc = purple_account_get_connection(account);	
	purple_conversation_new(PURPLE_CONV_TYPE_CHAT, account, name);
	chat = purple_blist_find_chat(account, name);
	if (chat == NULL) {
		if (PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl)->chat_info_defaults != NULL)
			hash = PURPLE_PLUGIN_PROTOCOL_INFO(gc->prpl)->chat_info_defaults(gc, name);
	} else {
		hash = chat->components;
	}
	serv_join_chat(gc, hash);
	if (chat == NULL && hash != NULL)
		g_hash_table_destroy(hash);
}

static void
join_chat_select(GntMenuItem *item, gpointer n)
{
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;

	fields = purple_request_fields_new();

	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_string_new("chat", _("Channel"), NULL, FALSE);
	purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_account_new("account", _("Account"), NULL);
	purple_request_field_set_type_hint(field, "account");
	purple_request_field_set_visible(field,
		(purple_connections_get_all() != NULL &&
		 purple_connections_get_all()->next != NULL));
	purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	purple_request_fields(purple_get_blist(), _("Join a Chat"),
						NULL,
						_("Please enter the name of the chat you want to join."),
						fields,
						_("Join"), G_CALLBACK(join_chat_select_cb),
						_("Cancel"), NULL,
						NULL, NULL, NULL,
						NULL);
}

static void
menu_add_buddy_cb(GntMenuItem *item, gpointer null)
{
	purple_blist_request_add_buddy(NULL, NULL, NULL, NULL);
}

static void
menu_add_chat_cb(GntMenuItem *item, gpointer null)
{
	purple_blist_request_add_chat(NULL, NULL, NULL, NULL);
}

static void
menu_add_group_cb(GntMenuItem *item, gpointer null)
{
	purple_blist_request_add_group();
}

static void
create_menu()
{
	GntWidget *menu, *sub, *subsub;
	GntMenuItem *item;
	GntWindow *window;

	if (!ggblist)
		return;

	window = GNT_WINDOW(ggblist->window);
	ggblist->menu = menu = gnt_menu_new(GNT_MENU_TOPLEVEL);
	gnt_window_set_menu(window, GNT_MENU(menu));

	item = gnt_menuitem_new(_("Options"));
	gnt_menu_add_item(GNT_MENU(menu), item);

	sub = gnt_menu_new(GNT_MENU_POPUP);
	gnt_menuitem_set_submenu(item, GNT_MENU(sub));

	item = gnt_menuitem_new(_("Send IM..."));
	gnt_menuitem_set_id(GNT_MENU_ITEM(item), "send-im");
	gnt_menu_add_item(GNT_MENU(sub), item);
	gnt_menuitem_set_callback(GNT_MENU_ITEM(item), send_im_select, NULL);

	item = gnt_menuitem_new(_("Join Chat..."));
	gnt_menuitem_set_id(GNT_MENU_ITEM(item), "join-chat");
	gnt_menu_add_item(GNT_MENU(sub), item);
	gnt_menuitem_set_callback(GNT_MENU_ITEM(item), join_chat_select, NULL);

	item = gnt_menuitem_new(_("Show"));
	gnt_menu_add_item(GNT_MENU(sub), item);
	subsub = gnt_menu_new(GNT_MENU_POPUP);
	gnt_menuitem_set_submenu(item, GNT_MENU(subsub));

	item = gnt_menuitem_check_new(_("Empty groups"));
	gnt_menuitem_set_id(GNT_MENU_ITEM(item), "show-empty-groups");
	gnt_menuitem_check_set_checked(GNT_MENU_ITEM_CHECK(item),
				purple_prefs_get_bool(PREF_ROOT "/emptygroups"));
	gnt_menu_add_item(GNT_MENU(subsub), item);
	gnt_menuitem_set_callback(GNT_MENU_ITEM(item), toggle_pref_cb, PREF_ROOT "/emptygroups");
	
	item = gnt_menuitem_check_new(_("Offline buddies"));
	gnt_menuitem_set_id(GNT_MENU_ITEM(item), "show-offline-buddies");
	gnt_menuitem_check_set_checked(GNT_MENU_ITEM_CHECK(item),
				purple_prefs_get_bool(PREF_ROOT "/showoffline"));
	gnt_menu_add_item(GNT_MENU(subsub), item);
	gnt_menuitem_set_callback(GNT_MENU_ITEM(item), toggle_pref_cb, PREF_ROOT "/showoffline");

	item = gnt_menuitem_new(_("Sort"));
	gnt_menu_add_item(GNT_MENU(sub), item);
	subsub = gnt_menu_new(GNT_MENU_POPUP);
	gnt_menuitem_set_submenu(item, GNT_MENU(subsub));

	item = gnt_menuitem_new(_("By Status"));
	gnt_menuitem_set_id(GNT_MENU_ITEM(item), "sort-status");
	gnt_menu_add_item(GNT_MENU(subsub), item);
	gnt_menuitem_set_callback(GNT_MENU_ITEM(item), sort_blist_change_cb, "status");

	item = gnt_menuitem_new(_("Alphabetically"));
	gnt_menuitem_set_id(GNT_MENU_ITEM(item), "sort-alpha");
	gnt_menu_add_item(GNT_MENU(subsub), item);
	gnt_menuitem_set_callback(GNT_MENU_ITEM(item), sort_blist_change_cb, "text");

	item = gnt_menuitem_new(_("By Log Size"));
	gnt_menuitem_set_id(GNT_MENU_ITEM(item), "sort-log");
	gnt_menu_add_item(GNT_MENU(subsub), item);
	gnt_menuitem_set_callback(GNT_MENU_ITEM(item), sort_blist_change_cb, "log");

	item = gnt_menuitem_new(_("Add"));
	gnt_menu_add_item(GNT_MENU(sub), item);

	subsub = gnt_menu_new(GNT_MENU_POPUP);
	gnt_menuitem_set_submenu(item, GNT_MENU(subsub));

	item = gnt_menuitem_new("Buddy");
	gnt_menuitem_set_id(GNT_MENU_ITEM(item), "add-buddy");
	gnt_menu_add_item(GNT_MENU(subsub), item);
	gnt_menuitem_set_callback(item, menu_add_buddy_cb, NULL);

	item = gnt_menuitem_new("Chat");
	gnt_menuitem_set_id(GNT_MENU_ITEM(item), "add-chat");
	gnt_menu_add_item(GNT_MENU(subsub), item);
	gnt_menuitem_set_callback(item, menu_add_chat_cb, NULL);

	item = gnt_menuitem_new("Group");
	gnt_menuitem_set_id(GNT_MENU_ITEM(item), "add-group");
	gnt_menu_add_item(GNT_MENU(subsub), item);
	gnt_menuitem_set_callback(item, menu_add_group_cb, NULL);

	reconstruct_accounts_menu();
	gnt_menu_add_item(GNT_MENU(menu), ggblist->accounts);

	reconstruct_plugins_menu();
	gnt_menu_add_item(GNT_MENU(menu), ggblist->plugins);
}

void finch_blist_show()
{
	blist_show(purple_get_blist());
}

static void
group_collapsed(GntWidget *widget, PurpleBlistNode *node, gboolean collapsed, gpointer null)
{
	if (PURPLE_BLIST_NODE_IS_GROUP(node))
		purple_blist_node_set_bool(node, "collapsed", collapsed);
}

static void
blist_show(PurpleBuddyList *list)
{
	if (ggblist == NULL)
		new_list(list);
	else if (ggblist->window) {
		gnt_window_present(ggblist->window);
		return;
	}

	ggblist->window = gnt_vwindow_new(FALSE);
	gnt_widget_set_name(ggblist->window, "buddylist");
	gnt_box_set_toplevel(GNT_BOX(ggblist->window), TRUE);
	gnt_box_set_title(GNT_BOX(ggblist->window), _("Buddy List"));
	gnt_box_set_pad(GNT_BOX(ggblist->window), 0);

	ggblist->tree = gnt_tree_new();

	GNT_WIDGET_SET_FLAGS(ggblist->tree, GNT_WIDGET_NO_BORDER);
	gnt_widget_set_size(ggblist->tree, purple_prefs_get_int(PREF_ROOT "/size/width"),
			purple_prefs_get_int(PREF_ROOT "/size/height"));
	gnt_widget_set_position(ggblist->window, purple_prefs_get_int(PREF_ROOT "/position/x"),
			purple_prefs_get_int(PREF_ROOT "/position/y"));

	gnt_tree_set_col_width(GNT_TREE(ggblist->tree), 0,
			purple_prefs_get_int(PREF_ROOT "/size/width") - 1);

	gnt_box_add_widget(GNT_BOX(ggblist->window), ggblist->tree);

	ggblist->status = gnt_combo_box_new();
	gnt_box_add_widget(GNT_BOX(ggblist->window), ggblist->status);
	ggblist->statustext = gnt_entry_new(NULL);
	gnt_box_add_widget(GNT_BOX(ggblist->window), ggblist->statustext);

	gnt_widget_show(ggblist->window);

	purple_signal_connect(purple_connections_get_handle(), "signed-on", finch_blist_get_handle(),
				PURPLE_CALLBACK(reconstruct_accounts_menu), NULL);
	purple_signal_connect(purple_connections_get_handle(), "signed-off", finch_blist_get_handle(),
				PURPLE_CALLBACK(reconstruct_accounts_menu), NULL);
	purple_signal_connect(purple_blist_get_handle(), "buddy-status-changed", finch_blist_get_handle(),
				PURPLE_CALLBACK(buddy_status_changed), ggblist);
	purple_signal_connect(purple_blist_get_handle(), "buddy-idle-changed", finch_blist_get_handle(),
				PURPLE_CALLBACK(buddy_idle_changed), ggblist);

	purple_signal_connect(purple_plugins_get_handle(), "plugin-load", finch_blist_get_handle(),
				PURPLE_CALLBACK(reconstruct_plugins_menu), NULL);
	purple_signal_connect(purple_plugins_get_handle(), "plugin-unload", finch_blist_get_handle(),
				PURPLE_CALLBACK(reconstruct_plugins_menu), NULL);

#if 0
	purple_signal_connect(purple_blist_get_handle(), "buddy-signed-on", finch_blist_get_handle(),
				PURPLE_CALLBACK(buddy_signed_on), ggblist);
	purple_signal_connect(purple_blist_get_handle(), "buddy-signed-off", finch_blist_get_handle(),
				PURPLE_CALLBACK(buddy_signed_off), ggblist);

	/* These I plan to use to indicate unread-messages etc. */
	purple_signal_connect(purple_conversations_get_handle(), "received-im-msg", finch_blist_get_handle(),
				PURPLE_CALLBACK(received_im_msg), list);
	purple_signal_connect(purple_conversations_get_handle(), "sent-im-msg", finch_blist_get_handle(),
				PURPLE_CALLBACK(sent_im_msg), NULL);

	purple_signal_connect(purple_conversations_get_handle(), "received-chat-msg", finch_blist_get_handle(),
				PURPLE_CALLBACK(received_chat_msg), list);
#endif

	g_signal_connect(G_OBJECT(ggblist->tree), "selection_changed", G_CALLBACK(selection_changed), ggblist);
	g_signal_connect(G_OBJECT(ggblist->tree), "key_pressed", G_CALLBACK(key_pressed), ggblist);
	g_signal_connect(G_OBJECT(ggblist->tree), "context-menu", G_CALLBACK(context_menu), ggblist);
	g_signal_connect(G_OBJECT(ggblist->tree), "collapse-toggled", G_CALLBACK(group_collapsed), NULL);
	g_signal_connect_after(G_OBJECT(ggblist->tree), "clicked", G_CALLBACK(blist_clicked), ggblist);
	g_signal_connect(G_OBJECT(ggblist->tree), "activate", G_CALLBACK(selection_activate), ggblist);
	g_signal_connect_data(G_OBJECT(ggblist->tree), "gained-focus", G_CALLBACK(draw_tooltip),
				ggblist, 0, G_CONNECT_AFTER | G_CONNECT_SWAPPED);
	g_signal_connect_data(G_OBJECT(ggblist->tree), "lost-focus", G_CALLBACK(remove_peripherals),
				ggblist, 0, G_CONNECT_AFTER | G_CONNECT_SWAPPED);
	g_signal_connect_data(G_OBJECT(ggblist->window), "workspace-hidden", G_CALLBACK(remove_peripherals),
				ggblist, 0, G_CONNECT_AFTER | G_CONNECT_SWAPPED);
	g_signal_connect(G_OBJECT(ggblist->tree), "size_changed", G_CALLBACK(size_changed_cb), NULL);
	g_signal_connect(G_OBJECT(ggblist->window), "position_set", G_CALLBACK(save_position_cb), NULL);
	g_signal_connect(G_OBJECT(ggblist->window), "destroy", G_CALLBACK(reset_blist_window), NULL);

	/* Status signals */
	purple_signal_connect(purple_savedstatuses_get_handle(), "savedstatus-changed", finch_blist_get_handle(),
				PURPLE_CALLBACK(savedstatus_changed), NULL);
	g_signal_connect(G_OBJECT(ggblist->status), "selection_changed",
				G_CALLBACK(status_selection_changed), NULL);
	g_signal_connect(G_OBJECT(ggblist->statustext), "key_pressed",
				G_CALLBACK(status_text_changed), NULL);

	create_menu();

	populate_buddylist();

	savedstatus_changed(purple_savedstatus_get_current(), NULL);
}

void finch_blist_uninit()
{
	if (ggblist == NULL)
		return;

	gnt_widget_destroy(ggblist->window);
	g_free(ggblist);
	ggblist = NULL;
}

gboolean finch_blist_get_position(int *x, int *y)
{
	if (!ggblist || !ggblist->window)
		return FALSE;
	gnt_widget_get_position(ggblist->window, x, y);
	return TRUE;
}

void finch_blist_set_position(int x, int y)
{
	gnt_widget_set_position(ggblist->window, x, y);
}

gboolean finch_blist_get_size(int *width, int *height)
{
	if (!ggblist || !ggblist->window)
		return FALSE;
	gnt_widget_get_size(ggblist->window, width, height);
	return TRUE;
}

void finch_blist_set_size(int width, int height)
{
	gnt_widget_set_size(ggblist->window, width, height);
}
