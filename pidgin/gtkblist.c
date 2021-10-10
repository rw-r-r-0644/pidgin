/* pidgin
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
 *
 */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <glib/gi18n-lib.h>

#include <purple.h>

#include "gtkaccount.h"
#include "gtkblist.h"
#include "gtkconv.h"
#include "gtkdialogs.h"
#include "gtkxfer.h"
#include "gtkprivacy.h"
#include "gtkroomlist.h"
#include "gtkstatusbox.h"
#include "gtkutils.h"
#include "pidgin/minidialog.h"
#include "pidgin/pidginaccountchooser.h"
#include "pidgin/pidginaccountfilterconnected.h"
#include "pidgin/pidginaccountstore.h"
#include "pidgin/pidginactiongroup.h"
#include "pidgin/pidgincellrendererexpander.h"
#include "pidgin/pidginclosebutton.h"
#include "pidgin/pidgincontactlist.h"
#include "pidgin/pidgincore.h"
#include "pidgin/pidgindebug.h"
#include "pidgin/pidgingdkpixbuf.h"
#include "pidgin/pidginlog.h"
#include "pidgin/pidginmooddialog.h"
#include "pidgin/pidginplugininfo.h"
#include "pidginscrollbook.h"
#include "pidgin/pidginstylecontext.h"
#include "pidginstock.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#define UI_DATA "ui-pidgin"

typedef struct
{
	PurpleAccount *account;
	GtkWidget *window;
	GtkBox *vbox;
	GtkWidget *account_menu;
	GtkSizeGroup *sg;
} PidginBlistRequestData;

typedef struct
{
	PidginBlistRequestData rq_data;
	GtkWidget *combo;
	GtkWidget *entry;
	GtkWidget *entry_for_alias;
	GtkWidget *entry_for_invite;

} PidginAddBuddyData;

typedef struct
{
	PidginBlistRequestData rq_data;
	gchar *default_chat_name;
	GList *entries;
} PidginChatData;

typedef struct
{
	PidginChatData chat_data;

	GtkWidget *alias_entry;
	GtkWidget *group_combo;
	GtkWidget *autojoin;
	GtkWidget *persistent;
} PidginAddChatData;

typedef struct
{
	/* GBoxed reference count */
	int box_count;

	/* Used to hold error minidialogs.  Gets packed
	 * inside PidginBuddyList.error_buttons
	 */
	PidginScrollBook *error_scrollbook;

	/* Pointer to the mini-dialog about having signed on elsewhere, if one
	 * is showing; %NULL otherwise.
	 */
	PidginMiniDialog *signed_on_elsewhere;

	guint select_notebook_page_timeout;


} PidginBuddyListPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(PidginBuddyList, pidgin_buddy_list,
                           PURPLE_TYPE_BUDDY_LIST)

enum {
	STATUS_ICON_COLUMN,
	STATUS_ICON_VISIBLE_COLUMN,
	NAME_COLUMN,
	IDLE_COLUMN,
	IDLE_VISIBLE_COLUMN,
	BUDDY_ICON_COLUMN,
	BUDDY_ICON_VISIBLE_COLUMN,
	NODE_COLUMN,
	GROUP_EXPANDER_COLUMN,
	GROUP_EXPANDER_VISIBLE_COLUMN,
	CONTACT_EXPANDER_COLUMN,
	CONTACT_EXPANDER_VISIBLE_COLUMN,
	EMBLEM_COLUMN,
	EMBLEM_VISIBLE_COLUMN,
	PROTOCOL_ICON_COLUMN,
	PROTOCOL_ICON_VISIBLE_COLUMN,
	BLIST_COLUMNS
};

#define PIDGIN_WINDOW_ICONIFIED(x) \
	(gdk_window_get_state(gtk_widget_get_window(GTK_WIDGET(x))) & \
	GDK_WINDOW_STATE_ICONIFIED)

#define PIDGIN_WINDOW_MAXIMIZED(x) \
	(gdk_window_get_state(gtk_widget_get_window(GTK_WIDGET(x))) & \
	GDK_WINDOW_STATE_MAXIMIZED)

static guint visibility_manager_count = 0;
static GdkVisibilityState gtk_blist_visibility = GDK_VISIBILITY_UNOBSCURED;
static gboolean gtk_blist_focused = FALSE;
static gboolean editing_blist = FALSE;

static GList *pidgin_blist_sort_methods = NULL;
static struct _PidginBlistSortMethod *current_sort_method = NULL;
static void sort_method_none(PurpleBlistNode *node, PurpleBuddyList *blist, GtkTreeIter groupiter, GtkTreeIter *cur, GtkTreeIter *iter);

static void sort_method_alphabetical(PurpleBlistNode *node, PurpleBuddyList *blist, GtkTreeIter groupiter, GtkTreeIter *cur, GtkTreeIter *iter);
static void sort_method_status(PurpleBlistNode *node, PurpleBuddyList *blist, GtkTreeIter groupiter, GtkTreeIter *cur, GtkTreeIter *iter);
static void sort_method_log_activity(PurpleBlistNode *node, PurpleBuddyList *blist, GtkTreeIter groupiter, GtkTreeIter *cur, GtkTreeIter *iter);

static PidginBuddyList *gtkblist = NULL;

static GList *groups_tree(void);
static gboolean pidgin_blist_refresh_timer(PurpleBuddyList *list);
static void pidgin_blist_update_buddy(PurpleBuddyList *list, PurpleBlistNode *node, gboolean status_change);
static void pidgin_blist_selection_changed(GtkTreeSelection *selection, gpointer data);
static void pidgin_blist_update(PurpleBuddyList *list, PurpleBlistNode *node);
static void pidgin_blist_update_group(PurpleBuddyList *list, PurpleBlistNode *node);
static void pidgin_blist_update_contact(PurpleBuddyList *list, PurpleBlistNode *node);
static char *pidgin_get_tooltip_text(PurpleBlistNode *node, gboolean full);
static gboolean get_iter_from_node(PurpleBlistNode *node, GtkTreeIter *iter);
static gboolean buddy_is_displayable(PurpleBuddy *buddy);
static void redo_buddy_list(PurpleBuddyList *list, gboolean remove, gboolean rerender);
static void pidgin_blist_collapse_contact_cb(GtkWidget *w, PurpleBlistNode *node);
static char *pidgin_get_group_title(PurpleBlistNode *gnode, gboolean expanded);
static void pidgin_blist_expand_contact_cb(GtkWidget *w, PurpleBlistNode *node);
static void set_urgent(void);

typedef enum {
	PIDGIN_BLIST_NODE_HAS_PENDING_MESSAGE            =  1 << 0,  /* Whether there's pending message in a conversation */
	PIDGIN_BLIST_CHAT_HAS_PENDING_MESSAGE_WITH_NICK	 =  1 << 1,  /* Whether there's a pending message in a chat that mentions our nick */
} PidginBlistNodeFlags;

typedef struct {
	GtkTreeRowReference *row;
	gboolean contact_expanded;
	gboolean recent_signonoff;
	gint recent_signonoff_timer;
	struct {
		PurpleConversation *conv;
		PidginBlistNodeFlags flags;
	} conv;
} PidginBlistNode;

/***************************************************
 *              Callbacks                          *
 ***************************************************/
static gboolean gtk_blist_visibility_cb(GtkWidget *w, GdkEventVisibility *event, gpointer data)
{
	GdkVisibilityState old_state = gtk_blist_visibility;
	gtk_blist_visibility = event->state;

	if (gtk_blist_visibility == GDK_VISIBILITY_FULLY_OBSCURED &&
		old_state != GDK_VISIBILITY_FULLY_OBSCURED) {

		/* no longer fully obscured */
		pidgin_blist_refresh_timer(purple_blist_get_default());
	}

	/* continue to handle event normally */
	return FALSE;
}

static gboolean gtk_blist_window_state_cb(GtkWidget *w, GdkEventWindowState *event, gpointer data)
{
	if(event->changed_mask & GDK_WINDOW_STATE_WITHDRAWN) {
		if(event->new_window_state & GDK_WINDOW_STATE_WITHDRAWN)
			purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/blist/list_visible", FALSE);
		else {
			purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/blist/list_visible", TRUE);
			pidgin_blist_refresh_timer(purple_blist_get_default());
		}
	}

	if(event->changed_mask & GDK_WINDOW_STATE_MAXIMIZED) {
		if(event->new_window_state & GDK_WINDOW_STATE_MAXIMIZED)
			purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/blist/list_maximized", TRUE);
		else
			purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/blist/list_maximized", FALSE);
	}

	/* Refresh gtkblist if un-iconifying */
	if (event->changed_mask & GDK_WINDOW_STATE_ICONIFIED){
		if (!(event->new_window_state & GDK_WINDOW_STATE_ICONIFIED))
			pidgin_blist_refresh_timer(purple_blist_get_default());
	}

	return FALSE;
}

static gboolean gtk_blist_delete_cb(GtkWidget *w, GdkEventAny *event, gpointer data)
{
	if(visibility_manager_count)
		purple_blist_set_visible(FALSE);
	else
		purple_core_quit();

	/* we handle everything, event should not propagate further */
	return TRUE;
}

static void
gtk_blist_hide_cb(GtkWidget *widget, PidginBuddyList *gtkblist)
{
	purple_signal_emit(pidgin_blist_get_handle(),
			"gtkblist-hiding", gtkblist);
}

static void
gtk_blist_show_cb(GtkWidget *widget, PidginBuddyList *gtkblist)
{
	purple_signal_emit(pidgin_blist_get_handle(),
			"gtkblist-unhiding", gtkblist);
}

static void
gtk_blist_size_allocate_cb(GtkWidget *widget, GtkAllocation *allocation,
		gpointer data)
{
	int new_width;
	int new_height;

	/* ignore changes when maximized */
	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/list_maximized")) {
		return;
	}

	gtk_window_get_size(GTK_WINDOW(widget), &new_width, &new_height);

	/* store the size */
	purple_prefs_set_int(PIDGIN_PREFS_ROOT "/blist/width",  new_width);
	purple_prefs_set_int(PIDGIN_PREFS_ROOT "/blist/height", new_height);
}

static void gtk_blist_menu_info_cb(GtkWidget *w, PurpleBuddy *b)
{
	PurpleAccount *account = purple_buddy_get_account(b);

	pidgin_retrieve_user_info(purple_account_get_connection(account),
	                          purple_buddy_get_name(b));
}

static void gtk_blist_menu_im_cb(GtkWidget *w, PurpleBuddy *b)
{
	pidgin_dialogs_im_with_user(purple_buddy_get_account(b),
	                            purple_buddy_get_name(b));
}

#ifdef USE_VV
static void gtk_blist_menu_audio_call_cb(GtkWidget *w, PurpleBuddy *b)
{
	purple_protocol_initiate_media(purple_buddy_get_account(b),
		purple_buddy_get_name(b), PURPLE_MEDIA_AUDIO);
}

static void gtk_blist_menu_video_call_cb(GtkWidget *w, PurpleBuddy *b)
{
	/* if the buddy supports both audio and video, start a combined call,
	 otherwise start a pure video session */
	if (purple_protocol_get_media_caps(purple_buddy_get_account(b),
			purple_buddy_get_name(b)) &
			PURPLE_MEDIA_CAPS_AUDIO_VIDEO) {
		purple_protocol_initiate_media(purple_buddy_get_account(b),
			purple_buddy_get_name(b), PURPLE_MEDIA_AUDIO | PURPLE_MEDIA_VIDEO);
	} else {
		purple_protocol_initiate_media(purple_buddy_get_account(b),
			purple_buddy_get_name(b), PURPLE_MEDIA_VIDEO);
	}
}

#endif

static void gtk_blist_menu_send_file_cb(GtkWidget *w, PurpleBuddy *b)
{
	PurpleAccount *account = purple_buddy_get_account(b);

	purple_serv_send_file(purple_account_get_connection(account),
	               purple_buddy_get_name(b), NULL);
}

static void gtk_blist_menu_move_to_cb(GtkWidget *w, PurpleBlistNode *node)
{
	PurpleGroup *group = g_object_get_data(G_OBJECT(w), "groupnode");
	purple_blist_add_contact((PurpleContact *)node, group, NULL);

}

static void gtk_blist_menu_autojoin_cb(GtkWidget *w, PurpleChat *chat)
{
	purple_blist_node_set_bool(PURPLE_BLIST_NODE(chat), "gtk-autojoin",
			gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)));
}

static void gtk_blist_menu_persistent_cb(GtkWidget *w, PurpleChat *chat)
{
	purple_blist_node_set_bool(PURPLE_BLIST_NODE(chat), "gtk-persistent",
			gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(w)));
}

static PurpleConversation *
find_conversation_with_buddy(PurpleBuddy *buddy)
{
	PurpleConversationManager *manager;
	PidginBlistNode *ui = g_object_get_data(G_OBJECT(buddy), UI_DATA);

	if(ui) {
		return ui->conv.conv;
	}

	manager = purple_conversation_manager_get_default();
	return purple_conversation_manager_find_im(manager,
	                                           purple_buddy_get_account(buddy),
	                                           purple_buddy_get_name(buddy));
}

static void gtk_blist_join_chat(PurpleChat *chat)
{
	PurpleAccount *account;
	PurpleConversation *conv;
	PurpleConversationManager *manager;
	PurpleProtocol *protocol;
	GHashTable *components;
	const char *name;
	char *chat_name = NULL;

	account = purple_chat_get_account(chat);
	protocol = purple_account_get_protocol(account);

	components = purple_chat_get_components(chat);

	if(PURPLE_IS_PROTOCOL_CHAT(protocol)) {
		chat_name = purple_protocol_chat_get_name(PURPLE_PROTOCOL_CHAT(protocol),
		                                          components);
	}

	if(chat_name != NULL) {
		name = chat_name;
	} else {
		name = purple_chat_get_name(chat);
	}

	manager = purple_conversation_manager_get_default();
	conv = purple_conversation_manager_find(manager, account, name);

	if(PURPLE_IS_CONVERSATION(conv)) {
		pidgin_conv_attach_to_conversation(conv);
		purple_conversation_present(conv);
	}

	purple_serv_join_chat(purple_account_get_connection(account), components);
	g_free(chat_name);
}

static void gtk_blist_menu_join_cb(GtkWidget *w, PurpleChat *chat)
{
	gtk_blist_join_chat(chat);
}

static void gtk_blist_renderer_editing_cancelled_cb(GtkCellRenderer *renderer, PurpleBuddyList *list)
{
	editing_blist = FALSE;
	g_object_set(G_OBJECT(renderer), "editable", FALSE, NULL);
	pidgin_blist_refresh(list);
}

static void gtk_blist_renderer_editing_started_cb(GtkCellRenderer *renderer,
		GtkCellEditable *editable,
		gchar *path_str,
		gpointer user_data)
{
	PidginBuddyList *gtkblist = PIDGIN_BUDDY_LIST(user_data);
	GtkTreeIter iter;
	GtkTreePath *path = NULL;
	PurpleBlistNode *node;
	const char *text = NULL;

	path = gtk_tree_path_new_from_string (path_str);
	gtk_tree_model_get_iter (GTK_TREE_MODEL(gtkblist->treemodel), &iter, path);
	gtk_tree_path_free (path);
	gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);

	if (PURPLE_IS_CONTACT(node))
		text = purple_contact_get_alias(PURPLE_CONTACT(node));
	else if (PURPLE_IS_BUDDY(node))
		text = purple_buddy_get_alias(PURPLE_BUDDY(node));
	else if (PURPLE_IS_GROUP(node))
		text = purple_group_get_name(PURPLE_GROUP(node));
	else if (PURPLE_IS_CHAT(node))
		text = purple_chat_get_name(PURPLE_CHAT(node));
	else
		g_return_if_reached();

	if (GTK_IS_ENTRY (editable)) {
		GtkEntry *entry = GTK_ENTRY (editable);
		gtk_entry_set_text(entry, text);
	}
	editing_blist = TRUE;
}

static void
gtk_blist_do_personize(GList *merges)
{
	PurpleBlistNode *contact = NULL;
	int max = 0;
	GList *tmp;

	/* First, we find the contact to merge the rest of the buddies into.
	 * This will be the contact with the most buddies in it; ties are broken
	 * by which contact is higher in the list
	 */
	for (tmp = merges; tmp; tmp = tmp->next) {
		PurpleBlistNode *node = tmp->data;
		PurpleBlistNode *b;
		int i = 0;

		if (PURPLE_IS_BUDDY(node))
			node = purple_blist_node_get_parent(node);

		if (!PURPLE_IS_CONTACT(node))
			continue;

		for (b = purple_blist_node_get_first_child(node);
		     b;
		     b = purple_blist_node_get_sibling_next(b))
		{
			i++;
		}

		if (i > max) {
			contact = node;
			max = i;
		}
	}

	if (contact == NULL)
		return;

	/* Merge all those buddies into this contact */
	for (tmp = merges; tmp; tmp = tmp->next) {
		PurpleBlistNode *node = tmp->data;
		if (PURPLE_IS_BUDDY(node))
			node = purple_blist_node_get_parent(node);

		if (node == contact)
			continue;

		purple_contact_merge((PurpleContact *)node, contact);
	}

	/* And show the expanded contact, so the people know what's going on */
	pidgin_blist_expand_contact_cb(NULL, contact);
	g_list_free(merges);
}

static void
gtk_blist_auto_personize(PurpleBlistNode *group, const char *alias)
{
	PurpleBlistNode *contact;
	PurpleBlistNode *buddy;
	GList *merges = NULL;
	int i = 0;
	char *a = g_utf8_casefold(alias, -1);

	for (contact = purple_blist_node_get_first_child(group);
	     contact != NULL;
	     contact = purple_blist_node_get_sibling_next(contact)) {
		char *node_alias;
		if (!PURPLE_IS_CONTACT(contact))
			continue;

		node_alias = g_utf8_casefold(purple_contact_get_alias((PurpleContact *)contact), -1);
		if (node_alias && !g_utf8_collate(node_alias, a)) {
			merges = g_list_append(merges, contact);
			i++;
			g_free(node_alias);
			continue;
		}
		g_free(node_alias);

		for (buddy = purple_blist_node_get_first_child(contact);
		     buddy;
		     buddy = purple_blist_node_get_sibling_next(buddy))
		{
			if (!PURPLE_IS_BUDDY(buddy))
				continue;

			node_alias = g_utf8_casefold(purple_buddy_get_alias(PURPLE_BUDDY(buddy)), -1);
			if (node_alias && !g_utf8_collate(node_alias, a)) {
				merges = g_list_append(merges, buddy);
				i++;
				g_free(node_alias);
				break;
			}
			g_free(node_alias);
		}
	}
	g_free(a);

	if (i > 1)
	{
		char *msg = g_strdup_printf(ngettext("You have %d contact named %s. Would you like to merge them?", "You currently have %d contacts named %s. Would you like to merge them?", i), i, alias);
		purple_request_action(NULL, NULL, msg, _("Merging these contacts will cause them to share a single entry on the buddy list and use a single conversation window. "
							 "You can separate them again by choosing 'Expand' from the contact's context menu"), 0, NULL,
				      merges, 2, _("_Yes"), PURPLE_CALLBACK(gtk_blist_do_personize), _("_No"), PURPLE_CALLBACK(g_list_free));
		g_free(msg);
	} else
		g_list_free(merges);
}

static void gtk_blist_renderer_edited_cb(GtkCellRendererText *text_rend, char *arg1,
					 char *arg2, PurpleBuddyList *list)
{
	PidginBuddyList *gtkblist = PIDGIN_BUDDY_LIST(list);
	GtkTreeIter iter;
	GtkTreePath *path;
	PurpleBlistNode *node;
	PurpleGroup *dest;
	gchar *alias = NULL;

	editing_blist = FALSE;
	path = gtk_tree_path_new_from_string (arg1);
	gtk_tree_model_get_iter (GTK_TREE_MODEL(gtkblist->treemodel), &iter, path);
	gtk_tree_path_free (path);
	gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);
	gtk_tree_view_set_enable_search (GTK_TREE_VIEW(gtkblist->treeview), TRUE);
	g_object_set(G_OBJECT(gtkblist->text_rend), "editable", FALSE, NULL);

	if (PURPLE_IS_CONTACT(node)) {
		PurpleContact *contact = PURPLE_CONTACT(node);
		PidginBlistNode *gtknode = g_object_get_data(G_OBJECT(node), UI_DATA);

		/*
		 * Using purple_contact_get_alias here breaks because we
		 * specifically want to check the contact alias only (i.e. not
		 * the priority buddy, which purple_contact_get_alias does).
		 * The "alias" GObject property gives us just the alias.
		 */
		g_object_get(contact, "alias", &alias, NULL);

		if (alias || gtknode->contact_expanded) {
			purple_contact_set_alias(contact, arg2);
			gtk_blist_auto_personize(purple_blist_node_get_parent(node), arg2);
		} else {
			PurpleBuddy *buddy = purple_contact_get_priority_buddy(contact);
			purple_buddy_set_local_alias(buddy, arg2);
			purple_serv_alias_buddy(buddy);
			gtk_blist_auto_personize(purple_blist_node_get_parent(node), arg2);
		}
	} else if (PURPLE_IS_BUDDY(node)) {
		PurpleGroup *group = purple_buddy_get_group(PURPLE_BUDDY(node));

		purple_buddy_set_local_alias(PURPLE_BUDDY(node), arg2);
		purple_serv_alias_buddy(PURPLE_BUDDY(node));
		gtk_blist_auto_personize(PURPLE_BLIST_NODE(group), arg2);
	} else if (PURPLE_IS_GROUP(node)) {
		dest = purple_blist_find_group(arg2);
		if (dest != NULL && purple_utf8_strcasecmp(arg2, purple_group_get_name(PURPLE_GROUP(node)))) {
			pidgin_dialogs_merge_groups(PURPLE_GROUP(node), arg2);
		} else {
			purple_group_set_name(PURPLE_GROUP(node), arg2);
		}
	} else if (PURPLE_IS_CHAT(node)) {
		purple_chat_set_alias(PURPLE_CHAT(node), arg2);
	}

	g_free(alias);
	pidgin_blist_refresh(list);
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
			if (purple_request_field_get_field_type(field) == PURPLE_REQUEST_FIELD_INTEGER)
				val = g_strdup_printf("%d", purple_request_field_int_get_value(field));
			else
				val = g_strdup(purple_request_field_string_get_value(field));

			if (!val) {
				g_hash_table_remove(purple_chat_get_components(chat), id);
			} else {
				g_hash_table_replace(purple_chat_get_components(chat), g_strdup(id), val);  /* val should not be free'd */
			}
		}
	}
}

static void chat_components_edit(GtkWidget *w, PurpleBlistNode *node)
{
	PurpleRequestFields *fields = purple_request_fields_new();
	PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
	PurpleRequestField *field;
	GList *parts, *iter;
	PurpleProtocolChatEntry *pce;
	PurpleProtocol *protocol;
	PurpleConnection *gc;
	PurpleChat *chat = (PurpleChat*)node;

	purple_request_fields_add_group(fields, group);

	gc = purple_account_get_connection(purple_chat_get_account(chat));
	protocol = purple_connection_get_protocol(gc);
	parts = purple_protocol_chat_info(PURPLE_PROTOCOL_CHAT(protocol), gc);

	for (iter = parts; iter; iter = iter->next) {
		pce = iter->data;
		if (pce->is_int) {
			int val;
			const char *str = g_hash_table_lookup(purple_chat_get_components(chat), pce->identifier);
			if (!str || sscanf(str, "%d", &val) != 1)
				val = pce->min;
			field = purple_request_field_int_new(pce->identifier, pce->label, val, INT_MIN, INT_MAX);
		} else {
			field = purple_request_field_string_new(pce->identifier, pce->label,
					g_hash_table_lookup(purple_chat_get_components(chat), pce->identifier), FALSE);
			if (pce->secret)
				purple_request_field_string_set_masked(field, TRUE);
		}

		if (pce->required)
			purple_request_field_set_required(field, TRUE);

		purple_request_field_group_add_field(group, field);
		g_free(pce);
	}

	g_list_free(parts);

	purple_request_fields(NULL, _("Edit Chat"), NULL, _("Please update the necessary fields."),
			fields, _("Save"), G_CALLBACK(chat_components_edit_ok), _("Cancel"), NULL,
			NULL, chat);
}

static void gtk_blist_menu_alias_cb(GtkWidget *w, PurpleBlistNode *node)
{
	GtkTreeIter iter;
	GtkTreePath *path;

	if (!(get_iter_from_node(node, &iter))) {
		/* This is either a bug, or the buddy is in a collapsed contact */
		node = purple_blist_node_get_parent(node);
		if (!get_iter_from_node(node, &iter))
			/* Now it's definitely a bug */
			return;
	}

	gtk_widget_trigger_tooltip_query(gtkblist->treeview);

	path = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &iter);
	g_object_set(G_OBJECT(gtkblist->text_rend), "editable", TRUE, NULL);
	gtk_tree_view_set_enable_search (GTK_TREE_VIEW(gtkblist->treeview), FALSE);
	gtk_widget_grab_focus(gtkblist->treeview);
	gtk_tree_view_set_cursor_on_cell(GTK_TREE_VIEW(gtkblist->treeview), path,
			gtkblist->text_column, gtkblist->text_rend, TRUE);
	gtk_tree_path_free(path);
}

static void gtk_blist_menu_showlog_cb(GtkWidget *w, PurpleBlistNode *node)
{
	PurpleLogType type;
	PurpleAccount *account;
	char *name = NULL;

	pidgin_set_cursor(gtkblist->window, GDK_WATCH);

	if (PURPLE_IS_BUDDY(node)) {
		PurpleBuddy *b = (PurpleBuddy*) node;
		type = PURPLE_LOG_IM;
		name = g_strdup(purple_buddy_get_name(b));
		account = purple_buddy_get_account(b);
	} else if (PURPLE_IS_CHAT(node)) {
		PurpleChat *c = PURPLE_CHAT(node);
		PurpleProtocol *protocol = NULL;
		type = PURPLE_LOG_CHAT;
		account = purple_chat_get_account(c);
		protocol = purple_account_get_protocol(account);
		if (protocol) {
			name = purple_protocol_chat_get_name(PURPLE_PROTOCOL_CHAT(protocol),
			                                     purple_chat_get_components(c));
		}
	} else if (PURPLE_IS_CONTACT(node)) {
		pidgin_log_show_contact(PURPLE_CONTACT(node));
		pidgin_clear_cursor(gtkblist->window);
		return;
	} else {
		pidgin_clear_cursor(gtkblist->window);

		/* This callback should not have been registered for a node
		 * that doesn't match the type of one of the blocks above. */
		g_return_if_reached();
	}

	if (name && account) {
		pidgin_log_show(type, name, account);
		pidgin_clear_cursor(gtkblist->window);
	}

	g_free(name);
}

static void gtk_blist_menu_showoffline_cb(GtkWidget *w, PurpleBlistNode *node)
{
	if (PURPLE_IS_BUDDY(node))
	{
		purple_blist_node_set_bool(node, "show_offline",
		                           !purple_blist_node_get_bool(node, "show_offline"));
		pidgin_blist_update(purple_blist_get_default(), node);
	}
	else if (PURPLE_IS_CONTACT(node))
	{
		PurpleBlistNode *bnode;
		gboolean setting = !purple_blist_node_get_bool(node, "show_offline");

		purple_blist_node_set_bool(node, "show_offline", setting);
		for (bnode = purple_blist_node_get_first_child(node);
		     bnode != NULL;
		     bnode = purple_blist_node_get_sibling_next(bnode))
		{
			purple_blist_node_set_bool(bnode, "show_offline", setting);
			pidgin_blist_update(purple_blist_get_default(), bnode);
		}
	} else if (PURPLE_IS_GROUP(node)) {
		PurpleBlistNode *cnode, *bnode;
		gboolean setting = !purple_blist_node_get_bool(node, "show_offline");

		purple_blist_node_set_bool(node, "show_offline", setting);
		for (cnode = purple_blist_node_get_first_child(node);
		     cnode != NULL;
		     cnode = purple_blist_node_get_sibling_next(cnode))
		{
			purple_blist_node_set_bool(cnode, "show_offline", setting);
			for (bnode = purple_blist_node_get_first_child(cnode);
			     bnode != NULL;
			     bnode = purple_blist_node_get_sibling_next(bnode))
			{
				purple_blist_node_set_bool(bnode, "show_offline", setting);
				pidgin_blist_update(purple_blist_get_default(),
				                    bnode);
			}
		}
	}
}

static void
do_join_chat(PidginChatData *data)
{
	if (data)
	{
		GHashTable *components =
			g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
		GList *tmp;
		PurpleChat *chat;

		for (tmp = data->entries; tmp != NULL; tmp = tmp->next)
		{
			if (g_object_get_data(tmp->data, "is_spin"))
			{
				g_hash_table_replace(components,
					g_strdup(g_object_get_data(tmp->data, "identifier")),
					g_strdup_printf("%d",
							gtk_spin_button_get_value_as_int(tmp->data)));
			}
			else
			{
				g_hash_table_replace(components,
					g_strdup(g_object_get_data(tmp->data, "identifier")),
					g_strdup(gtk_entry_get_text(tmp->data)));
			}
		}

		chat = purple_chat_new(data->rq_data.account, NULL, components);
		gtk_blist_join_chat(chat);
		purple_blist_remove_chat(chat);
	}
}

static void
do_joinchat(GtkWidget *dialog, int id, PidginChatData *info)
{
	switch(id)
	{
		case GTK_RESPONSE_OK:
			do_join_chat(info);
			break;

		case 1:
			pidgin_roomlist_dialog_show_with_account(info->rq_data.account);
			return;

		break;
	}

	gtk_widget_destroy(GTK_WIDGET(dialog));
	g_list_free(info->entries);
	g_free(info);
}

/*
 * Check the values of all the text entry boxes.  If any required input
 * strings are empty then don't allow the user to click on "OK."
 */
static void
set_sensitive_if_input_chat_cb(GtkWidget *entry, gpointer user_data)
{
	PurpleProtocol *protocol;
	PurpleConnection *gc;
	PidginChatData *data;
	GList *tmp;
	const char *text;
	gboolean required;
	gboolean sensitive = TRUE;

	data = user_data;

	for (tmp = data->entries; tmp != NULL; tmp = tmp->next)
	{
		if (!g_object_get_data(tmp->data, "is_spin"))
		{
			required = GPOINTER_TO_INT(g_object_get_data(tmp->data, "required"));
			text = gtk_entry_get_text(tmp->data);
			if (required && (*text == '\0'))
				sensitive = FALSE;
		}
	}

	gtk_dialog_set_response_sensitive(GTK_DIALOG(data->rq_data.window), GTK_RESPONSE_OK, sensitive);

	gc = purple_account_get_connection(data->rq_data.account);
	protocol = (gc != NULL) ? purple_connection_get_protocol(gc) : NULL;
	sensitive = (protocol != NULL && PURPLE_PROTOCOL_IMPLEMENTS(protocol, ROOMLIST, get_list));

	gtk_dialog_set_response_sensitive(GTK_DIALOG(data->rq_data.window), 1, sensitive);
}

static void
set_sensitive_if_input_buddy_cb(GtkWidget *entry, gpointer user_data)
{
	PurpleProtocol *protocol;
	PidginAddBuddyData *data = user_data;
	const char *text;

	protocol = purple_account_get_protocol(data->rq_data.account);
	text = gtk_entry_get_text(GTK_ENTRY(entry));

	gtk_dialog_set_response_sensitive(GTK_DIALOG(data->rq_data.window),
		GTK_RESPONSE_OK, purple_validate(protocol, text));
}

static void
pidgin_blist_update_privacy_cb(PurpleBuddy *buddy)
{
	PidginBlistNode *ui_data = g_object_get_data(G_OBJECT(buddy), UI_DATA);
	if (ui_data == NULL || ui_data->row == NULL)
		return;
	pidgin_blist_update_buddy(purple_blist_get_default(),
	                          PURPLE_BLIST_NODE(buddy), TRUE);
}

static gboolean
add_buddy_account_filter_func(PurpleAccount *account)
{
	PurpleConnection *gc = purple_account_get_connection(account);
	PurpleProtocol *protocol = purple_connection_get_protocol(gc);

	return PURPLE_PROTOCOL_IMPLEMENTS(protocol, SERVER, add_buddy);
}

static gboolean
chat_account_filter_func(PurpleAccount *account)
{
	PurpleConnection *gc = purple_account_get_connection(account);
	PurpleProtocol *protocol = NULL;

	if (gc == NULL)
		return FALSE;

	protocol = purple_connection_get_protocol(gc);

	return (PURPLE_PROTOCOL_IMPLEMENTS(protocol, CHAT, info));
}

gboolean
pidgin_blist_joinchat_is_showable()
{
	GList *c;
	PurpleConnection *gc;

	for (c = purple_connections_get_all(); c != NULL; c = c->next) {
		gc = c->data;

		if (chat_account_filter_func(purple_connection_get_account(gc)))
			return TRUE;
	}

	return FALSE;
}

static GtkWidget *
make_blist_request_dialog(PidginBlistRequestData *data, PurpleAccount *account,
	const char *title, const char *window_role, const char *label_text,
	GCallback callback_func, PurpleFilterAccountFunc filter_func,
	GCallback response_cb)
{
	GtkWidget *label;
	GtkWidget *img;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWindow *blist_window;
	GtkTreeModel *model = NULL, *filter = NULL;
	PidginBuddyList *gtkblist;

	data->account = account;

	img = gtk_image_new_from_icon_name("dialog-question",
			GTK_ICON_SIZE_DIALOG);

	gtkblist = PIDGIN_BUDDY_LIST(purple_blist_get_default());
	blist_window = gtkblist ? GTK_WINDOW(gtkblist->window) : NULL;

	data->window = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(data->window), title);
	gtk_window_set_transient_for(GTK_WINDOW(data->window), blist_window);
	gtk_dialog_set_default_response(GTK_DIALOG(data->window), GTK_RESPONSE_OK);
	gtk_container_set_border_width(GTK_CONTAINER(data->window), 6);
	gtk_window_set_resizable(GTK_WINDOW(data->window), FALSE);
	gtk_box_set_spacing(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(data->window))),
	                    12);
	gtk_container_set_border_width(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(data->window))),
	                               6);
	gtk_window_set_role(GTK_WINDOW(data->window), window_role);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(data->window))),
	                  hbox);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
	gtk_widget_set_halign(img, GTK_ALIGN_START);
	gtk_widget_set_valign(img, GTK_ALIGN_START);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	gtk_container_add(GTK_CONTAINER(hbox), vbox);

	label = gtk_label_new(label_text);

	gtk_widget_set_size_request(label, 400, -1);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_xalign(GTK_LABEL(label), 0);
	gtk_label_set_yalign(GTK_LABEL(label), 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	data->sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	model = GTK_TREE_MODEL(pidgin_account_store_new());
	filter = pidgin_account_filter_connected_new(model, NULL);
	g_object_unref(G_OBJECT(model));
	data->account_menu = pidgin_account_chooser_new();
	gtk_combo_box_set_model(GTK_COMBO_BOX(data->account_menu), filter);
	g_object_unref(G_OBJECT(filter));
	if(PURPLE_IS_ACCOUNT(account)) {
		pidgin_account_chooser_set_selected(PIDGIN_ACCOUNT_CHOOSER(
			data->account_menu), account);
	} else {
		gtk_combo_box_set_active(GTK_COMBO_BOX(data->account_menu), 0);
	}

	pidgin_account_chooser_set_filter_func(
	        PIDGIN_ACCOUNT_CHOOSER(data->account_menu), filter_func);
	pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("A_ccount"), data->sg, data->account_menu, TRUE, NULL);
	g_signal_connect(data->account_menu, "changed",
	                 G_CALLBACK(callback_func), data);

	data->vbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 5));
	gtk_container_set_border_width(GTK_CONTAINER(data->vbox), 0);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(data->vbox), FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(data->window), "response", response_cb, data);

	g_object_unref(data->sg);

	return vbox;
}

static void
rebuild_chat_entries(PidginChatData *data, const char *default_chat_name)
{
	PurpleConnection *gc;
	PurpleProtocol *protocol;
	GList *list = NULL, *tmp;
	GHashTable *defaults = NULL;
	PurpleProtocolChatEntry *pce;
	gboolean focus = TRUE;

	g_return_if_fail(data->rq_data.account != NULL);

	gc = purple_account_get_connection(data->rq_data.account);
	protocol = purple_connection_get_protocol(gc);

	gtk_container_foreach(GTK_CONTAINER(data->rq_data.vbox), (GtkCallback)gtk_widget_destroy, NULL);

	g_list_free(data->entries);
	data->entries = NULL;

	list = purple_protocol_chat_info(PURPLE_PROTOCOL_CHAT(protocol), gc);
	defaults = purple_protocol_chat_info_defaults(PURPLE_PROTOCOL_CHAT(protocol),
	                                              gc, default_chat_name);

	for (tmp = list; tmp; tmp = tmp->next)
	{
		GtkWidget *input;

		pce = tmp->data;

		if (pce->is_int)
		{
			GtkAdjustment *adjust;
			adjust = GTK_ADJUSTMENT(gtk_adjustment_new(pce->min,
			                                           pce->min, pce->max,
			                                           1, 10, 10));
			input = gtk_spin_button_new(adjust, 1, 0);
			gtk_widget_set_size_request(input, 50, -1);
			pidgin_add_widget_to_vbox(GTK_BOX(data->rq_data.vbox), pce->label, data->rq_data.sg, input, FALSE, NULL);
		}
		else
		{
			char *value;
			input = gtk_entry_new();
			gtk_entry_set_activates_default(GTK_ENTRY(input), TRUE);
			value = g_hash_table_lookup(defaults, pce->identifier);
			if (value != NULL)
				gtk_entry_set_text(GTK_ENTRY(input), value);
			if (pce->secret)
			{
				gtk_entry_set_visibility(GTK_ENTRY(input), FALSE);
			}
			pidgin_add_widget_to_vbox(data->rq_data.vbox, pce->label, data->rq_data.sg, input, TRUE, NULL);
			g_signal_connect(G_OBJECT(input), "changed",
							 G_CALLBACK(set_sensitive_if_input_chat_cb), data);
		}

		/* Do the following for any type of input widget */
		if (focus)
		{
			gtk_widget_grab_focus(input);
			focus = FALSE;
		}
		g_object_set_data(G_OBJECT(input), "identifier", (gpointer)pce->identifier);
		g_object_set_data(G_OBJECT(input), "is_spin", GINT_TO_POINTER(pce->is_int));
		g_object_set_data(G_OBJECT(input), "required", GINT_TO_POINTER(pce->required));
		data->entries = g_list_append(data->entries, input);

		g_free(pce);
	}

	g_list_free(list);
	g_hash_table_destroy(defaults);

	/* Set whether the "OK" button should be clickable initially */
	set_sensitive_if_input_chat_cb(NULL, data);

	gtk_widget_show_all(GTK_WIDGET(data->rq_data.vbox));
}

static void
chat_select_account_cb(GObject *w, PidginChatData *data)
{
	PidginAccountChooser *chooser = PIDGIN_ACCOUNT_CHOOSER(w);
	PurpleAccount *account = pidgin_account_chooser_get_selected(chooser);

	g_return_if_fail(data != NULL);
	g_return_if_fail(account != NULL);

	if (purple_strequal(purple_account_get_protocol_id(data->rq_data.account),
	                    purple_account_get_protocol_id(account)))
	{
		data->rq_data.account = account;
	}
	else
	{
		data->rq_data.account = account;
		rebuild_chat_entries(data, data->default_chat_name);
	}
}

void
pidgin_blist_joinchat_show(void)
{
	PidginChatData *data = NULL;
	PidginAccountChooser *chooser = NULL;

	data = g_new0(PidginChatData, 1);

	make_blist_request_dialog((PidginBlistRequestData *)data, NULL,
		_("Join a Chat"), "join_chat",
		_("Please enter the appropriate information about the chat "
			"you would like to join.\n"),
		G_CALLBACK(chat_select_account_cb),
		chat_account_filter_func, (GCallback)do_joinchat);
	gtk_dialog_add_buttons(GTK_DIALOG(data->rq_data.window),
		_("Room _List"), 1,
		_("Cancel"), GTK_RESPONSE_CANCEL,
		_("_Join"), GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(data->rq_data.window),
		GTK_RESPONSE_OK);
	data->default_chat_name = NULL;

	chooser = PIDGIN_ACCOUNT_CHOOSER(data->rq_data.account_menu);
	data->rq_data.account = pidgin_account_chooser_get_selected(chooser);

	rebuild_chat_entries(data, NULL);

	gtk_widget_show_all(data->rq_data.window);
}

static void gtk_blist_row_expanded_cb(GtkTreeView *tv, GtkTreeIter *iter, GtkTreePath *path, gpointer user_data)
{
	PidginBuddyList *gtkblist = PIDGIN_BUDDY_LIST(user_data);
	PurpleBlistNode *node;

	gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), iter, NODE_COLUMN, &node, -1);

	if (PURPLE_IS_GROUP(node)) {
		char *title;

		title = pidgin_get_group_title(node, TRUE);

		gtk_tree_store_set(gtkblist->treemodel, iter,
		   NAME_COLUMN, title,
		   -1);

		g_free(title);

		purple_blist_node_set_bool(node, "collapsed", FALSE);
		gtk_widget_trigger_tooltip_query(gtkblist->treeview);
	}
}

static void gtk_blist_row_collapsed_cb(GtkTreeView *tv, GtkTreeIter *iter, GtkTreePath *path, gpointer user_data)
{
	PidginBuddyList *gtkblist = PIDGIN_BUDDY_LIST(user_data);
	PurpleBlistNode *node;

	gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), iter, NODE_COLUMN, &node, -1);

	if (PURPLE_IS_GROUP(node)) {
		char *title;
		PidginBlistNode *gtknode;
		PurpleBlistNode *cnode;

		title = pidgin_get_group_title(node, FALSE);

		gtk_tree_store_set(gtkblist->treemodel, iter,
		   NAME_COLUMN, title,
		   -1);

		g_free(title);

		purple_blist_node_set_bool(node, "collapsed", TRUE);

		for(cnode = purple_blist_node_get_first_child(node); cnode; cnode = purple_blist_node_get_sibling_next(cnode)) {
			if (PURPLE_IS_CONTACT(cnode)) {
				gtknode = g_object_get_data(G_OBJECT(cnode), UI_DATA);
				if (!gtknode->contact_expanded)
					continue;
				gtknode->contact_expanded = FALSE;
				pidgin_blist_update_contact(NULL, cnode);
			}
		}
		gtk_widget_trigger_tooltip_query(gtkblist->treeview);
	} else if(PURPLE_IS_CONTACT(node)) {
		pidgin_blist_collapse_contact_cb(NULL, node);
	}
}

static void gtk_blist_row_activated_cb(GtkTreeView *tv, GtkTreePath *path, GtkTreeViewColumn *col, gpointer data) {
	PidginBuddyList *gtkblist = PIDGIN_BUDDY_LIST(data);
	PurpleBlistNode *node;
	GtkTreeIter iter;

	gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &iter, path);
	gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);

	if(PURPLE_IS_CONTACT(node) || PURPLE_IS_BUDDY(node)) {
		PurpleBuddy *buddy;

		if(PURPLE_IS_CONTACT(node))
			buddy = purple_contact_get_priority_buddy((PurpleContact*)node);
		else
			buddy = (PurpleBuddy*)node;

		pidgin_dialogs_im_with_user(purple_buddy_get_account(buddy), purple_buddy_get_name(buddy));
	} else if (PURPLE_IS_CHAT(node)) {
		gtk_blist_join_chat((PurpleChat *)node);
	} else if (PURPLE_IS_GROUP(node)) {
/*		if (gtk_tree_view_row_expanded(tv, path))
			gtk_tree_view_collapse_row(tv, path);
		else
			gtk_tree_view_expand_row(tv,path,FALSE);*/
	}
}

static void pidgin_blist_add_chat_cb(void)
{
	GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkblist->treeview));
	GtkTreeIter iter;
	PurpleBlistNode *node;

	if(gtk_tree_selection_get_selected(sel, NULL, &iter)){
		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);
		if (PURPLE_IS_BUDDY(node))
			purple_blist_request_add_chat(NULL, purple_buddy_get_group(PURPLE_BUDDY(node)), NULL, NULL);
		if (PURPLE_IS_CONTACT(node) || PURPLE_IS_CHAT(node))
			purple_blist_request_add_chat(NULL, purple_contact_get_group(PURPLE_CONTACT(node)), NULL, NULL);
		else if (PURPLE_IS_GROUP(node))
			purple_blist_request_add_chat(NULL, (PurpleGroup*)node, NULL, NULL);
	}
	else {
		purple_blist_request_add_chat(NULL, NULL, NULL, NULL);
	}
}

static void pidgin_blist_add_buddy_cb(void)
{
	GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkblist->treeview));
	GtkTreeIter iter;
	PurpleBlistNode *node;

	if(gtk_tree_selection_get_selected(sel, NULL, &iter)){
		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);
		if (PURPLE_IS_BUDDY(node)) {
			PurpleGroup *group = purple_buddy_get_group(PURPLE_BUDDY(node));
			purple_blist_request_add_buddy(NULL, NULL, purple_group_get_name(group), NULL);
		} else if (PURPLE_IS_CONTACT(node) || PURPLE_IS_CHAT(node)) {
			PurpleGroup *group = purple_contact_get_group(PURPLE_CONTACT(node));
			purple_blist_request_add_buddy(NULL, NULL, purple_group_get_name(group), NULL);
		} else if (PURPLE_IS_GROUP(node)) {
			purple_blist_request_add_buddy(NULL, NULL, purple_group_get_name(PURPLE_GROUP(node)), NULL);
		}
	}
	else {
		purple_blist_request_add_buddy(NULL, NULL, NULL, NULL);
	}
}

static void
pidgin_blist_remove_cb (GtkWidget *w, PurpleBlistNode *node)
{
	if (PURPLE_IS_BUDDY(node)) {
		pidgin_dialogs_remove_buddy((PurpleBuddy*)node);
	} else if (PURPLE_IS_CHAT(node)) {
		pidgin_dialogs_remove_chat((PurpleChat*)node);
	} else if (PURPLE_IS_GROUP(node)) {
		pidgin_dialogs_remove_group((PurpleGroup*)node);
	} else if (PURPLE_IS_CONTACT(node)) {
		pidgin_dialogs_remove_contact((PurpleContact*)node);
	}
}

struct _expand {
	GtkTreeView *treeview;
	GtkTreePath *path;
	PurpleBlistNode *node;
};

static gboolean
scroll_to_expanded_cell(gpointer data)
{
	struct _expand *ex = data;
	gtk_tree_view_scroll_to_cell(ex->treeview, ex->path, NULL, FALSE, 0, 0);
	pidgin_blist_update_contact(NULL, ex->node);

	gtk_tree_path_free(ex->path);
	g_free(ex);

	return FALSE;
}

static void
pidgin_blist_expand_contact_cb(GtkWidget *w, PurpleBlistNode *node)
{
	PidginBlistNode *gtknode;
	GtkTreeIter iter, parent;
	PurpleBlistNode *bnode;
	GtkTreePath *path;

	if(!PURPLE_IS_CONTACT(node)) {
		return;
	}

	gtknode = g_object_get_data(G_OBJECT(node), UI_DATA);

	gtknode->contact_expanded = TRUE;

	for(bnode = purple_blist_node_get_first_child(node); bnode; bnode = purple_blist_node_get_sibling_next(bnode)) {
		pidgin_blist_update(NULL, bnode);
	}

	/* This ensures that the bottom buddy is visible, i.e. not scrolled off the alignment */
	if (get_iter_from_node(node, &parent)) {
		struct _expand *ex = g_new0(struct _expand, 1);

		gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(gtkblist->treemodel), &iter, &parent,
				  gtk_tree_model_iter_n_children(GTK_TREE_MODEL(gtkblist->treemodel), &parent) -1);
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &iter);

		/* Let the treeview draw so it knows where to scroll */
		ex->treeview = GTK_TREE_VIEW(gtkblist->treeview);
		ex->path = path;
		ex->node = purple_blist_node_get_first_child(node);
		g_idle_add(scroll_to_expanded_cell, ex);
	}
}

static void
pidgin_blist_collapse_contact_cb(GtkWidget *w, PurpleBlistNode *node)
{
	PurpleBlistNode *bnode;
	PidginBlistNode *gtknode;

	if(!PURPLE_IS_CONTACT(node))
		return;

	gtknode = g_object_get_data(G_OBJECT(node), UI_DATA);

	gtknode->contact_expanded = FALSE;

	for(bnode = purple_blist_node_get_first_child(node); bnode; bnode = purple_blist_node_get_sibling_next(bnode)) {
		pidgin_blist_update(NULL, bnode);
	}
}

static void
toggle_privacy(GtkWidget *widget, PurpleBlistNode *node)
{
	PurpleBuddy *buddy;
	PurpleAccount *account;
	gboolean permitted;
	const char *name;

	if (!PURPLE_IS_BUDDY(node))
		return;

	buddy = (PurpleBuddy *)node;
	account = purple_buddy_get_account(buddy);
	name = purple_buddy_get_name(buddy);

	permitted = purple_account_privacy_check(account, name);

	/* XXX: Perhaps ask whether to restore the previous lists where appropirate? */

	if (permitted)
		purple_account_privacy_deny(account, name);
	else
		purple_account_privacy_allow(account, name);

	pidgin_blist_update(purple_blist_get_default(), node);
}

void pidgin_append_blist_node_privacy_menu(GtkWidget *menu, PurpleBlistNode *node)
{
	PurpleBuddy *buddy = (PurpleBuddy *)node;
	PurpleAccount *account;
	gboolean permitted;

	account = purple_buddy_get_account(buddy);
	permitted = purple_account_privacy_check(account, purple_buddy_get_name(buddy));

	pidgin_new_menu_item(menu, permitted ? _("_Block") : _("Un_block"), NULL,
                         G_CALLBACK(toggle_privacy), node);
}

void
pidgin_append_blist_node_proto_menu(GtkWidget *menu, PurpleConnection *gc,
                                      PurpleBlistNode *node)
{
	GList *l, *ll;
	PurpleProtocol *protocol = purple_connection_get_protocol(gc);

	if(!PURPLE_IS_PROTOCOL_CLIENT(protocol)) {
		return;
	}

	ll = purple_protocol_client_blist_node_menu(PURPLE_PROTOCOL_CLIENT(protocol), node);
	for(l = ll; l; l = l->next) {
		PurpleActionMenu *act = (PurpleActionMenu *) l->data;
		pidgin_append_menu_action(menu, act, node);
	}
	g_list_free(ll);
}

void
pidgin_append_blist_node_extended_menu(GtkWidget *menu, PurpleBlistNode *node)
{
	GList *l, *ll;

	for(l = ll = purple_blist_node_get_extended_menu(node); l; l = l->next) {
		PurpleActionMenu *act = (PurpleActionMenu *) l->data;
		pidgin_append_menu_action(menu, act, node);
	}
	g_list_free(ll);
}



static void
pidgin_append_blist_node_move_to_menu(GtkWidget *menu, PurpleBlistNode *node)
{
	GtkWidget *submenu;
	GtkWidget *menuitem;
	PurpleBlistNode *group;

	menuitem = gtk_menu_item_new_with_label(_("Move to"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	gtk_widget_show(menuitem);

	submenu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);

	for (group = purple_blist_get_default_root(); group;
	     group = purple_blist_node_get_sibling_next(group)) {
		if (!PURPLE_IS_GROUP(group))
			continue;
		if (group == purple_blist_node_get_parent(node))
			continue;
		menuitem = pidgin_new_menu_item(submenu,
                                purple_group_get_name((PurpleGroup *)group), NULL,
                                G_CALLBACK(gtk_blist_menu_move_to_cb), node);
		g_object_set_data(G_OBJECT(menuitem), "groupnode", group);
	}
	gtk_widget_show_all(submenu);
}

void
pidgin_blist_make_buddy_menu(GtkWidget *menu, PurpleBuddy *buddy, gboolean sub) {
	PurpleAccount *account = NULL;
	PurpleConnection *pc = NULL;
	PurpleProtocol *protocol;
	PurpleContact *contact;
	PurpleBlistNode *node;
	gboolean contact_expanded = FALSE;

	g_return_if_fail(menu);
	g_return_if_fail(buddy);

	account = purple_buddy_get_account(buddy);
	pc = purple_account_get_connection(account);
	protocol = purple_connection_get_protocol(pc);

	node = PURPLE_BLIST_NODE(buddy);

	contact = purple_buddy_get_contact(buddy);
	if (contact) {
		PidginBlistNode *node = g_object_get_data(G_OBJECT(contact), UI_DATA);
		contact_expanded = node->contact_expanded;
	}

	if (protocol && PURPLE_PROTOCOL_IMPLEMENTS(protocol, SERVER, get_info)) {
		pidgin_new_menu_item(menu, _("Get _Info"), NULL,
		                     G_CALLBACK(gtk_blist_menu_info_cb), buddy);
	}
	pidgin_new_menu_item(menu, _("I_M"), NULL,
			G_CALLBACK(gtk_blist_menu_im_cb), buddy);

#ifdef USE_VV
	if (protocol && PURPLE_PROTOCOL_IMPLEMENTS(protocol, MEDIA, get_caps)) {
		PurpleAccount *account = purple_buddy_get_account(buddy);
		const gchar *who = purple_buddy_get_name(buddy);
		PurpleMediaCaps caps = purple_protocol_get_media_caps(account, who);
		if (caps & PURPLE_MEDIA_CAPS_AUDIO) {
			pidgin_new_menu_item(menu, _("_Audio Call"), NULL,
			                     G_CALLBACK(gtk_blist_menu_audio_call_cb),
			                     buddy);
		}
		if (caps & PURPLE_MEDIA_CAPS_AUDIO_VIDEO) {
			pidgin_new_menu_item(menu, _("Audio/_Video Call"), NULL,
			                     G_CALLBACK(gtk_blist_menu_video_call_cb),
			                     buddy);
		} else if (caps & PURPLE_MEDIA_CAPS_VIDEO) {
			pidgin_new_menu_item(menu, _("_Video Call"), NULL,
			                     G_CALLBACK(gtk_blist_menu_video_call_cb),
			                     buddy);
		}
	}

#endif

	if (protocol && PURPLE_IS_PROTOCOL_XFER(protocol)) {
		if (purple_protocol_xfer_can_receive(
			PURPLE_PROTOCOL_XFER(protocol),
			purple_account_get_connection(purple_buddy_get_account(buddy)), purple_buddy_get_name(buddy)
		)) {
			pidgin_new_menu_item(menu, _("_Send File..."), NULL,
		                         G_CALLBACK(gtk_blist_menu_send_file_cb),
		                         buddy);
		}
	}

	if (node->parent && node->parent->child->next &&
	      !sub && !contact_expanded) {
		pidgin_new_menu_item(menu, _("View _Log"), NULL,
				G_CALLBACK(gtk_blist_menu_showlog_cb), contact);
	} else if (!sub) {
		pidgin_new_menu_item(menu, _("View _Log"), NULL,
				G_CALLBACK(gtk_blist_menu_showlog_cb), buddy);
	}

	if (!purple_blist_node_is_transient(node)) {
		gboolean show_offline = purple_blist_node_get_bool(node, "show_offline");
		pidgin_new_menu_item(menu,
                                show_offline ? _("Hide When Offline") : _("Show When Offline"),
				NULL, G_CALLBACK(gtk_blist_menu_showoffline_cb), node);
	}

	pidgin_append_blist_node_proto_menu(menu, purple_account_get_connection(purple_buddy_get_account(buddy)), node);
	pidgin_append_blist_node_extended_menu(menu, node);

	if (!contact_expanded && contact != NULL)
		pidgin_append_blist_node_move_to_menu(menu, PURPLE_BLIST_NODE(contact));

	if (node->parent && node->parent->child->next &&
	    !sub && !contact_expanded) {
		pidgin_separator(menu);
		pidgin_append_blist_node_privacy_menu(menu, node);
		pidgin_new_menu_item(menu, _("_Alias..."), NULL,
		                     G_CALLBACK(gtk_blist_menu_alias_cb), contact);
		pidgin_new_menu_item(menu, _("_Remove"), NULL,
		                     G_CALLBACK(pidgin_blist_remove_cb), contact);
	} else if (!sub || contact_expanded) {
		pidgin_separator(menu);
		pidgin_append_blist_node_privacy_menu(menu, node);
		pidgin_new_menu_item(menu, _("_Alias..."), NULL,
		                     G_CALLBACK(gtk_blist_menu_alias_cb), buddy);
		pidgin_new_menu_item(menu, _("_Remove"), NULL,
		                     G_CALLBACK(pidgin_blist_remove_cb), buddy);
	}
}

static gboolean
gtk_blist_key_press_cb(GtkWidget *tv, GdkEventKey *event, gpointer data)
{
	PurpleBlistNode *node;
	GtkTreeIter iter, parent;
	GtkTreeSelection *sel;
	GtkTreePath *path;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv));
	if(!gtk_tree_selection_get_selected(sel, NULL, &iter))
		return FALSE;

	gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);

	if(event->state & GDK_CONTROL_MASK &&
			(event->keyval == 'o' || event->keyval == 'O')) {
		PurpleBuddy *buddy;

		if(PURPLE_IS_CONTACT(node)) {
			buddy = purple_contact_get_priority_buddy((PurpleContact*)node);
		} else if(PURPLE_IS_BUDDY(node)) {
			buddy = (PurpleBuddy*)node;
		} else {
			return FALSE;
		}
		if(buddy)
			pidgin_retrieve_user_info(purple_account_get_connection(purple_buddy_get_account(buddy)), purple_buddy_get_name(buddy));
	} else {
		switch (event->keyval) {
			case GDK_KEY_F2:
				gtk_blist_menu_alias_cb(tv, node);
				break;

			case GDK_KEY_Left:
				path = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &iter);
				if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(tv), path)) {
					/* Collapse the Group */
					gtk_tree_view_collapse_row(GTK_TREE_VIEW(tv), path);
					gtk_tree_path_free(path);
					return TRUE;
				} else {
					/* Select the Parent */
					if (gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &iter, path)) {
						if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(gtkblist->treemodel), &parent, &iter)) {
							gtk_tree_path_free(path);
							path = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &parent);
							gtk_tree_view_set_cursor(GTK_TREE_VIEW(tv), path, NULL, FALSE);
							gtk_tree_path_free(path);
							return TRUE;
						}
					}
				}
				gtk_tree_path_free(path);
				break;

			case GDK_KEY_Right:
				path = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &iter);
				if (!gtk_tree_view_row_expanded(GTK_TREE_VIEW(tv), path)) {
					/* Expand the Group */
					if (PURPLE_IS_CONTACT(node)) {
						pidgin_blist_expand_contact_cb(NULL, node);
						gtk_tree_path_free(path);
						return TRUE;
					} else if (!PURPLE_IS_BUDDY(node)) {
						gtk_tree_view_expand_row(GTK_TREE_VIEW(tv), path, FALSE);
						gtk_tree_path_free(path);
						return TRUE;
					}
				} else {
					/* Select the First Child */
					if (gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &parent, path)) {
						if (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(gtkblist->treemodel), &iter, &parent, 0)) {
							gtk_tree_path_free(path);
							path = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &iter);
							gtk_tree_view_set_cursor(GTK_TREE_VIEW(tv), path, NULL, FALSE);
							gtk_tree_path_free(path);
							return TRUE;
						}
					}
				}
				gtk_tree_path_free(path);
				break;
		}
	}

	return FALSE;
}

static void
set_node_custom_icon_cb(const gchar *filename, gpointer data)
{
	if (filename) {
		PurpleBlistNode *node = (PurpleBlistNode*)data;

		purple_buddy_icons_node_set_custom_icon_from_file(node,
		                                                  filename);
	}
	g_object_set_data(G_OBJECT(data), "buddy-icon-chooser", NULL);
}

static void
set_node_custom_icon(GtkWidget *w, PurpleBlistNode *node)
{
	GtkFileChooserNative *win =
	        g_object_get_data(G_OBJECT(node), "buddy-icon-chooser");
	if (win == NULL) {
		win = pidgin_buddy_icon_chooser_new(NULL, set_node_custom_icon_cb,
		                                    node);
		g_object_set_data_full(G_OBJECT(node), "buddy-icon-chooser", win,
		                       g_object_unref);
	}
	gtk_native_dialog_show(GTK_NATIVE_DIALOG(win));
}

static void
remove_node_custom_icon(GtkWidget *w, PurpleBlistNode *node)
{
	purple_buddy_icons_node_set_custom_icon(node, NULL, 0);
}

static void
add_buddy_icon_menu_items(GtkWidget *menu, PurpleBlistNode *node)
{
	GtkWidget *item;

	pidgin_new_menu_item(menu, _("Set Custom Icon"), NULL,
                        G_CALLBACK(set_node_custom_icon), node);

	item = pidgin_new_menu_item(menu, _("Remove Custom Icon"), NULL,
	                           G_CALLBACK(remove_node_custom_icon), node);
	if (!purple_buddy_icons_node_has_custom_icon(node))
		gtk_widget_set_sensitive(item, FALSE);
}

static GtkWidget *
create_group_menu (PurpleBlistNode *node, PurpleGroup *g)
{
	GtkWidget *menu;
	GtkWidget *item;

	menu = gtk_menu_new();
	item = pidgin_new_menu_item(menu, _("Add _Buddy..."), NULL,
				 G_CALLBACK(pidgin_blist_add_buddy_cb), node);
	gtk_widget_set_sensitive(item, purple_connections_get_all() != NULL);
	item = pidgin_new_menu_item(menu, _("Add C_hat..."), NULL,
				 G_CALLBACK(pidgin_blist_add_chat_cb), node);
	gtk_widget_set_sensitive(item, pidgin_blist_joinchat_is_showable());
	pidgin_new_menu_item(menu, _("_Delete Group"), NULL,
				 G_CALLBACK(pidgin_blist_remove_cb), node);
	pidgin_new_menu_item(menu, _("_Rename"), NULL,
				 G_CALLBACK(gtk_blist_menu_alias_cb), node);
	if (!purple_blist_node_is_transient(node)) {
		gboolean show_offline = purple_blist_node_get_bool(node, "show_offline");
		pidgin_new_menu_item(menu,
                                show_offline ? _("Hide When Offline") : _("Show When Offline"),
				NULL, G_CALLBACK(gtk_blist_menu_showoffline_cb), node);
	}

	add_buddy_icon_menu_items(menu, node);

	pidgin_append_blist_node_extended_menu(menu, node);

	return menu;
}

static GtkWidget *
create_chat_menu(PurpleBlistNode *node, PurpleChat *c)
{
	GtkWidget *menu;
	gboolean autojoin, persistent;

	menu = gtk_menu_new();
	autojoin = purple_blist_node_get_bool(node, "gtk-autojoin");
	persistent = purple_blist_node_get_bool(node, "gtk-persistent");

	pidgin_new_menu_item(menu, _("_Join"), NULL,
	                     G_CALLBACK(gtk_blist_menu_join_cb), node);
	pidgin_new_check_item(menu, _("Auto-Join"),
	                      G_CALLBACK(gtk_blist_menu_autojoin_cb), node,
	                      autojoin);
	pidgin_new_check_item(menu, _("Persistent"),
	                      G_CALLBACK(gtk_blist_menu_persistent_cb), node,
	                      persistent);
	pidgin_new_menu_item(menu, _("View _Log"), NULL,
	                     G_CALLBACK(gtk_blist_menu_showlog_cb), node);

	pidgin_append_blist_node_proto_menu(menu, purple_account_get_connection(purple_chat_get_account(c)), node);
	pidgin_append_blist_node_extended_menu(menu, node);

	pidgin_separator(menu);

	pidgin_new_menu_item(menu, _("_Edit Settings..."), NULL,
	                     G_CALLBACK(chat_components_edit), node);
	pidgin_new_menu_item(menu, _("_Alias..."), NULL,
	                     G_CALLBACK(gtk_blist_menu_alias_cb), node);
	pidgin_new_menu_item(menu, _("_Remove"), NULL,
	                     G_CALLBACK(pidgin_blist_remove_cb), node);

	add_buddy_icon_menu_items(menu, node);

	return menu;
}

static GtkWidget *
create_contact_menu (PurpleBlistNode *node)
{
	GtkWidget *menu;

	menu = gtk_menu_new();

	pidgin_new_menu_item(menu, _("View _Log"), NULL,
				 G_CALLBACK(gtk_blist_menu_showlog_cb),
				 node);

	pidgin_separator(menu);

	pidgin_new_menu_item(menu, _("_Alias..."), NULL,
	                     G_CALLBACK(gtk_blist_menu_alias_cb), node);
	pidgin_new_menu_item(menu, _("_Remove"), NULL,
	                     G_CALLBACK(pidgin_blist_remove_cb), node);

	add_buddy_icon_menu_items(menu, node);

	pidgin_separator(menu);

	pidgin_new_menu_item(menu, _("_Collapse"), NULL,
	                     G_CALLBACK(pidgin_blist_collapse_contact_cb), node);

	pidgin_append_blist_node_extended_menu(menu, node);

	return menu;
}

static GtkWidget *
create_buddy_menu(PurpleBlistNode *node, PurpleBuddy *b)
{
	PidginBlistNode *gtknode = g_object_get_data(G_OBJECT(node), UI_DATA);
	GtkWidget *menu;
	GtkWidget *menuitem;
	gboolean show_offline = purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_offline_buddies");

	menu = gtk_menu_new();
	pidgin_blist_make_buddy_menu(menu, b, FALSE);

	if(PURPLE_IS_CONTACT(node)) {
		pidgin_separator(menu);

		add_buddy_icon_menu_items(menu, node);

		if(gtknode->contact_expanded) {
			pidgin_new_menu_item(menu, _("_Collapse"),
                                        NULL,
                                        G_CALLBACK(pidgin_blist_collapse_contact_cb),
                                        node);
		} else {
			pidgin_new_menu_item(menu, _("_Expand"),
                                        NULL,
                                        G_CALLBACK(pidgin_blist_expand_contact_cb),
                                        node);
		}
		if(node->child->next) {
			PurpleBlistNode *bnode;

			for(bnode = node->child; bnode; bnode = bnode->next) {
				PurpleBuddy *buddy = (PurpleBuddy*)bnode;
				GtkWidget *submenu;

				if(buddy == b)
					continue;
				if(!purple_account_get_connection(purple_buddy_get_account(buddy)))
					continue;
				if(!show_offline && !PURPLE_BUDDY_IS_ONLINE(buddy))
					continue;

				menuitem = gtk_menu_item_new_with_label(purple_buddy_get_name(buddy));
				gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
				gtk_widget_show(menuitem);

				submenu = gtk_menu_new();
				gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);
				gtk_widget_show(submenu);

				pidgin_blist_make_buddy_menu(submenu, buddy, TRUE);
			}
		}
	}
	return menu;
}

static gboolean
pidgin_blist_show_context_menu(GtkWidget *tv, PurpleBlistNode *node, GdkEvent *event)
{
	PidginBlistNode *gtknode = g_object_get_data(G_OBJECT(node), UI_DATA);
	GtkWidget *menu = NULL;
	gboolean handled = FALSE;

	/* Create a menu based on the thing we right-clicked on */
	if (PURPLE_IS_GROUP(node)) {
		PurpleGroup *g = (PurpleGroup *)node;

		menu = create_group_menu(node, g);
	} else if (PURPLE_IS_CHAT(node)) {
		PurpleChat *c = (PurpleChat *)node;

		menu = create_chat_menu(node, c);
	} else if ((PURPLE_IS_CONTACT(node)) && (gtknode->contact_expanded)) {
		menu = create_contact_menu(node);
	} else if (PURPLE_IS_CONTACT(node) || PURPLE_IS_BUDDY(node)) {
		PurpleBuddy *b;

		if (PURPLE_IS_CONTACT(node))
			b = purple_contact_get_priority_buddy((PurpleContact*)node);
		else
			b = (PurpleBuddy *)node;

		menu = create_buddy_menu(node, b);
	}

	/* Now display the menu */
	if (menu != NULL) {
		gtk_widget_show_all(menu);
		if (event != NULL) {
			/* Pointer event */
			gtk_menu_popup_at_pointer(GTK_MENU(menu), event);
		} else {
			/* Keyboard event */
			pidgin_menu_popup_at_treeview_selection(menu, tv);
		}
		handled = TRUE;
	}

	return handled;
}

static gboolean
gtk_blist_button_press_cb(GtkWidget *tv, GdkEventButton *event, gpointer user_data)
{
	GtkTreePath *path;
	PurpleBlistNode *node;
	GtkTreeIter iter;
	GtkTreeSelection *sel;
	PurpleProtocol *protocol = NULL;
	PidginBlistNode *gtknode;
	gboolean handled = FALSE;

	/* Here we figure out which node was clicked */
	if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(tv), event->x, event->y, &path, NULL, NULL, NULL))
		return FALSE;
	gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &iter, path);
	gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);
	gtknode = g_object_get_data(G_OBJECT(node), UI_DATA);

	/* Right click draws a context menu */
	if (gdk_event_triggers_context_menu((GdkEvent *)event)) {
		handled = pidgin_blist_show_context_menu(tv, node, (GdkEvent *)event);

	/* CTRL+middle click expands or collapse a contact */
	} else if ((event->button == GDK_BUTTON_MIDDLE) && (event->type == GDK_BUTTON_PRESS) &&
			   (event->state & GDK_CONTROL_MASK) && (PURPLE_IS_CONTACT(node))) {
		if (gtknode->contact_expanded)
			pidgin_blist_collapse_contact_cb(NULL, node);
		else
			pidgin_blist_expand_contact_cb(NULL, node);
		handled = TRUE;

	/* Double middle click gets info */
	} else if ((event->button == GDK_BUTTON_MIDDLE) && (event->type == GDK_2BUTTON_PRESS) &&
			   ((PURPLE_IS_CONTACT(node)) || (PURPLE_IS_BUDDY(node)))) {
		PurpleAccount *account;
		PurpleBuddy *b;
		if(PURPLE_IS_CONTACT(node))
			b = purple_contact_get_priority_buddy((PurpleContact*)node);
		else
			b = (PurpleBuddy *)node;

		account = purple_buddy_get_account(b);
		protocol = purple_account_get_protocol(account);

		if (protocol && PURPLE_PROTOCOL_IMPLEMENTS(protocol, SERVER, get_info))
			pidgin_retrieve_user_info(purple_account_get_connection(account), purple_buddy_get_name(b));
		handled = TRUE;
	}

#if (1)
	/*
	 * This code only exists because GTK doesn't work.  If we return
	 * FALSE here, as would be normal the event propoagates down and
	 * somehow gets interpreted as the start of a drag event.
	 *
	 * Um, isn't it _normal_ to return TRUE here?  Since the event
	 * was handled?  --Mark
	 */
	if(handled) {
		sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv));
		gtk_tree_selection_select_path(sel, path);
		gtk_tree_path_free(path);
		return TRUE;
	}
#endif
	gtk_tree_path_free(path);

	return FALSE;
}

static gboolean
pidgin_blist_popup_menu_cb(GtkWidget *tv, void *user_data)
{
	PurpleBlistNode *node;
	GtkTreeIter iter;
	GtkTreeSelection *sel;
	gboolean handled = FALSE;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv));
	if (!gtk_tree_selection_get_selected(sel, NULL, &iter))
		return FALSE;

	gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);

	/* Shift+F10 draws a context menu */
	handled = pidgin_blist_show_context_menu(tv, node, NULL);

	return handled;
}

static void
add_buddies_from_vcard(const char *protocol_id, PurpleGroup *group, GList *list,
					   const char *alias)
{
	GList *l;
	PurpleAccount *account = NULL;
	PurpleConnection *gc;

	if (list == NULL)
		return;

	for (l = purple_connections_get_all(); l != NULL; l = l->next)
	{
		gc = (PurpleConnection *)l->data;
		account = purple_connection_get_account(gc);

		if (purple_strequal(purple_account_get_protocol_id(account), protocol_id))
			break;

		account = NULL;
	}

	if (account != NULL)
	{
		for (l = list; l != NULL; l = l->next)
		{
			purple_blist_request_add_buddy(account, l->data,
										 (group ? purple_group_get_name(group) : NULL),
										 alias);
		}
	}

	g_list_free_full(list, g_free);
}

static gboolean
parse_vcard(const char *vcard, PurpleGroup *group)
{
	char *temp_vcard;
	char *s, *c;
	char *alias    = NULL;
	GList *jabbers = NULL;

	s = temp_vcard = g_strdup(vcard);

	while (*s != '\0' && strncmp(s, "END:vCard", strlen("END:vCard")))
	{
		char *field, *value;

		field = s;

		/* Grab the field */
		while (*s != '\r' && *s != '\n' && *s != '\0' && *s != ':')
			s++;

		if (*s == '\r') s++;
		if (*s == '\n')
		{
			s++;
			continue;
		}

		if (*s != '\0') *s++ = '\0';

		if ((c = strchr(field, ';')) != NULL)
			*c = '\0';

		/* Proceed to the end of the line */
		value = s;

		while (*s != '\r' && *s != '\n' && *s != '\0')
			s++;

		if (*s == '\r') *s++ = '\0';
		if (*s == '\n') *s++ = '\0';

		/* We only want to worry about a few fields here. */
		if (purple_strequal(field, "FN"))
			alias = g_strdup(value);
		else if (purple_strequal(field, "X-JABBER"))
		{
			char **values = g_strsplit(value, ":", 0);
			char **im;

			for (im = values; *im != NULL; im++)
			{
				jabbers = g_list_append(jabbers, g_strdup(*im));
			}

			g_strfreev(values);
		}
	}

	g_free(temp_vcard);

	if (jabbers == NULL)
	{
		g_free(alias);

		return FALSE;
	}

	add_buddies_from_vcard("prpl-jabber", group, jabbers, alias);

	g_free(alias);

	return TRUE;
}

static void pidgin_blist_drag_data_get_cb(GtkWidget *widget,
											GdkDragContext *dc,
											GtkSelectionData *data,
											guint info,
											guint time,
											gpointer null)
{
	GdkAtom target = gtk_selection_data_get_target(data);

	if (target == gdk_atom_intern("PURPLE_BLIST_NODE", FALSE)) {
		GtkTreeRowReference *ref = g_object_get_data(G_OBJECT(dc), "gtk-tree-view-source-row");
		GtkTreePath *sourcerow = gtk_tree_row_reference_get_path(ref);
		GtkTreeIter iter;
		PurpleBlistNode *node = NULL;
		if(!sourcerow)
			return;
		gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &iter, sourcerow);
		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);
		gtk_selection_data_set (data,
					gdk_atom_intern ("PURPLE_BLIST_NODE", FALSE),
					8, /* bits */
					(void*)&node,
					sizeof (node));

		gtk_tree_path_free(sourcerow);
	} else if (target == gdk_atom_intern("application/x-im-contact", FALSE)) {
		GtkTreeRowReference *ref;
		GtkTreePath *sourcerow;
		GtkTreeIter iter;
		PurpleBlistNode *node = NULL;
		PurpleBuddy *buddy;
		PurpleConnection *gc;
		GString *str;
		const char *protocol;

		ref = g_object_get_data(G_OBJECT(dc), "gtk-tree-view-source-row");
		sourcerow = gtk_tree_row_reference_get_path(ref);

		if (!sourcerow)
			return;

		gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &iter,
								sourcerow);
		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);

		if (PURPLE_IS_CONTACT(node))
		{
			buddy = purple_contact_get_priority_buddy((PurpleContact *)node);
		}
		else if (!PURPLE_IS_BUDDY(node))
		{
			gtk_tree_path_free(sourcerow);
			return;
		}
		else
		{
			buddy = (PurpleBuddy *)node;
		}

		gc = purple_account_get_connection(purple_buddy_get_account(buddy));

		if (gc == NULL)
		{
			gtk_tree_path_free(sourcerow);
			return;
		}

		protocol =
			purple_protocol_get_list_icon(purple_connection_get_protocol(gc),
					purple_buddy_get_account(buddy), buddy);

		str = g_string_new(NULL);
		g_string_printf(str,
			"MIME-Version: 1.0\r\n"
			"Content-Type: application/x-im-contact\r\n"
			"X-IM-Protocol: %s\r\n"
			"X-IM-Username: %s\r\n",
			protocol,
			purple_buddy_get_name(buddy));

		if (purple_buddy_get_local_alias(buddy) != NULL)
		{
			g_string_append_printf(str,
				"X-IM-Alias: %s\r\n",
				purple_buddy_get_local_alias(buddy));
		}

		g_string_append(str, "\r\n");

		gtk_selection_data_set(data,
					gdk_atom_intern("application/x-im-contact", FALSE),
					8, /* bits */
					(const guchar *)str->str,
					strlen(str->str) + 1);

		g_string_free(str, TRUE);
		gtk_tree_path_free(sourcerow);
	}
}

static void pidgin_blist_drag_data_rcv_cb(GtkWidget *widget, GdkDragContext *dc, guint x, guint y,
			  GtkSelectionData *sd, guint info, guint t)
{
	GdkAtom target = gtk_selection_data_get_target(sd);
	const guchar *data = gtk_selection_data_get_data(sd);

	if (gtkblist->drag_timeout) {
		g_source_remove(gtkblist->drag_timeout);
		gtkblist->drag_timeout = 0;
	}

	if (target == gdk_atom_intern("PURPLE_BLIST_NODE", FALSE) && data) {
		PurpleBlistNode *n = NULL;
		GtkTreePath *path = NULL;
		GtkTreeViewDropPosition position;
		memcpy(&n, data, sizeof(n));
		if(gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget), x, y, &path, &position)) {
			/* if we're here, I think it means the drop is ok */
			GtkTreeIter iter;
			PurpleBlistNode *node;
			PidginBlistNode *gtknode;

			gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel),
					&iter, path);
			gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel),
					&iter, NODE_COLUMN, &node, -1);
			gtknode = g_object_get_data(G_OBJECT(node), UI_DATA);

			if (PURPLE_IS_CONTACT(n)) {
				PurpleContact *c = (PurpleContact*)n;
				if (PURPLE_IS_CONTACT(node) && gtknode->contact_expanded) {
					purple_contact_merge(c, node);
				} else if (PURPLE_IS_CONTACT(node) ||
						PURPLE_IS_CHAT(node)) {
					switch(position) {
						case GTK_TREE_VIEW_DROP_AFTER:
						case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
							purple_blist_add_contact(c, (PurpleGroup*)node->parent,
									node);
							break;
						case GTK_TREE_VIEW_DROP_BEFORE:
						case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
							purple_blist_add_contact(c, (PurpleGroup*)node->parent,
									node->prev);
							break;
					}
				} else if(PURPLE_IS_GROUP(node)) {
					purple_blist_add_contact(c, (PurpleGroup*)node, NULL);
				} else if(PURPLE_IS_BUDDY(node)) {
					purple_contact_merge(c, node);
				}
			} else if (PURPLE_IS_BUDDY(n)) {
				PurpleBuddy *b = (PurpleBuddy*)n;
				if (PURPLE_IS_BUDDY(node)) {
					switch(position) {
						case GTK_TREE_VIEW_DROP_AFTER:
						case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
							purple_blist_add_buddy(b, (PurpleContact*)node->parent,
									(PurpleGroup*)node->parent->parent, node);
							break;
						case GTK_TREE_VIEW_DROP_BEFORE:
						case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
							purple_blist_add_buddy(b, (PurpleContact*)node->parent,
									(PurpleGroup*)node->parent->parent,
									node->prev);
							break;
					}
				} else if(PURPLE_IS_CHAT(node)) {
					purple_blist_add_buddy(b, NULL, (PurpleGroup*)node->parent,
							NULL);
				} else if (PURPLE_IS_GROUP(node)) {
					purple_blist_add_buddy(b, NULL, (PurpleGroup*)node, NULL);
				} else if (PURPLE_IS_CONTACT(node)) {
					if(gtknode->contact_expanded) {
						switch(position) {
							case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
							case GTK_TREE_VIEW_DROP_AFTER:
							case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
								purple_blist_add_buddy(b, (PurpleContact*)node,
										(PurpleGroup*)node->parent, NULL);
								break;
							case GTK_TREE_VIEW_DROP_BEFORE:
								purple_blist_add_buddy(b, NULL,
										(PurpleGroup*)node->parent, node->prev);
								break;
						}
					} else {
						switch(position) {
							case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
							case GTK_TREE_VIEW_DROP_AFTER:
								purple_blist_add_buddy(b, NULL,
										(PurpleGroup*)node->parent, NULL);
								break;
							case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
							case GTK_TREE_VIEW_DROP_BEFORE:
								purple_blist_add_buddy(b, NULL,
										(PurpleGroup*)node->parent, node->prev);
								break;
						}
					}
				}
			} else if (PURPLE_IS_CHAT(n)) {
				PurpleChat *chat = (PurpleChat *)n;
				if (PURPLE_IS_BUDDY(node)) {
					switch(position) {
						case GTK_TREE_VIEW_DROP_AFTER:
						case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
						case GTK_TREE_VIEW_DROP_BEFORE:
						case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
							purple_blist_add_chat(chat,
									(PurpleGroup*)node->parent->parent,
									node->parent);
							break;
					}
				} else if(PURPLE_IS_CONTACT(node) ||
						PURPLE_IS_CHAT(node)) {
					switch(position) {
						case GTK_TREE_VIEW_DROP_AFTER:
						case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
							purple_blist_add_chat(chat, (PurpleGroup*)node->parent, node);
							break;
						case GTK_TREE_VIEW_DROP_BEFORE:
						case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
							purple_blist_add_chat(chat, (PurpleGroup*)node->parent, node->prev);
							break;
					}
				} else if (PURPLE_IS_GROUP(node)) {
					purple_blist_add_chat(chat, (PurpleGroup*)node, NULL);
				}
			} else if (PURPLE_IS_GROUP(n)) {
				PurpleGroup *g = (PurpleGroup*)n;
				if (PURPLE_IS_GROUP(node)) {
					switch (position) {
					case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
					case GTK_TREE_VIEW_DROP_AFTER:
						purple_blist_add_group(g, node);
						break;
					case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
					case GTK_TREE_VIEW_DROP_BEFORE:
						purple_blist_add_group(g, node->prev);
						break;
					}
				} else if(PURPLE_IS_BUDDY(node)) {
					purple_blist_add_group(g, node->parent->parent);
				} else if(PURPLE_IS_CONTACT(node) ||
						PURPLE_IS_CHAT(node)) {
					purple_blist_add_group(g, node->parent);
				}
			}

			gtk_tree_path_free(path);
			gtk_drag_finish(dc, TRUE, (gdk_drag_context_get_actions(dc) == GDK_ACTION_MOVE), t);
		}
	} else if (target == gdk_atom_intern("application/x-im-contact",
										   FALSE) && data) {
		PurpleGroup *group = NULL;
		GtkTreePath *path = NULL;
		GtkTreeViewDropPosition position;
		PurpleAccount *account;
		char *protocol = NULL;
		char *username = NULL;
		char *alias = NULL;

		if (gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget),
											  x, y, &path, &position))
		{
			GtkTreeIter iter;
			PurpleBlistNode *node;

			gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel),
									&iter, path);
			gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel),
				&iter, NODE_COLUMN, &node, -1);

			if (PURPLE_IS_BUDDY(node))
			{
				group = (PurpleGroup *)node->parent->parent;
			}
			else if (PURPLE_IS_CHAT(node) ||
					 PURPLE_IS_CONTACT(node))
			{
				group = (PurpleGroup *)node->parent;
			}
			else if (PURPLE_IS_GROUP(node))
			{
				group = (PurpleGroup *)node;
			}
		}

		if (pidgin_parse_x_im_contact((const char *)data, FALSE, &account,
										&protocol, &username, &alias))
		{
			if (account == NULL)
			{
				purple_notify_error(NULL, NULL,
					_("You are not currently signed on with an account that "
					  "can add that buddy."), NULL, NULL);
			}
			else
			{
				purple_blist_request_add_buddy(account, username,
											 (group ? purple_group_get_name(group) : NULL),
											 alias);
			}
		}

		g_free(username);
		g_free(protocol);
		g_free(alias);

		if (path != NULL)
			gtk_tree_path_free(path);

		gtk_drag_finish(dc, TRUE,
		                gdk_drag_context_get_actions(dc) == GDK_ACTION_MOVE, t);
	}
	else if (target == gdk_atom_intern("text/x-vcard", FALSE) && data)
	{
		gboolean result;
		PurpleGroup *group = NULL;
		GtkTreePath *path = NULL;
		GtkTreeViewDropPosition position;

		if (gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget),
											  x, y, &path, &position))
		{
			GtkTreeIter iter;
			PurpleBlistNode *node;

			gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel),
									&iter, path);
			gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel),
				&iter, NODE_COLUMN, &node, -1);

			if (PURPLE_IS_BUDDY(node))
			{
				group = (PurpleGroup *)node->parent->parent;
			}
			else if (PURPLE_IS_CHAT(node) ||
					 PURPLE_IS_CONTACT(node))
			{
				group = (PurpleGroup *)node->parent;
			}
			else if (PURPLE_IS_GROUP(node))
			{
				group = (PurpleGroup *)node;
			}
		}

		result = parse_vcard((const gchar *)data, group);

		gtk_drag_finish(dc, result,
		                gdk_drag_context_get_actions(dc) == GDK_ACTION_MOVE, t);
	} else if (target == gdk_atom_intern("text/uri-list", FALSE) && data) {
		GtkTreePath *path = NULL;
		GtkTreeViewDropPosition position;

		if (gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(widget),
											  x, y, &path, &position))
			{
				GtkTreeIter iter;
				PurpleBlistNode *node;

				gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel),
							&iter, path);
				gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel),
					&iter, NODE_COLUMN, &node, -1);

				if (PURPLE_IS_BUDDY(node) || PURPLE_IS_CONTACT(node)) {
					PurpleBuddy *b = PURPLE_IS_BUDDY(node) ? PURPLE_BUDDY(node) : purple_contact_get_priority_buddy(PURPLE_CONTACT(node));
					pidgin_dnd_file_manage(sd, purple_buddy_get_account(b), purple_buddy_get_name(b));
					gtk_drag_finish(dc, TRUE,
					                gdk_drag_context_get_actions(dc) == GDK_ACTION_MOVE, t);
				} else {
					gtk_drag_finish(dc, FALSE, FALSE, t);
				}
			}
	}
}

/* Altered from do_colorshift in gnome-panel */
static void
do_alphashift(GdkPixbuf *pixbuf, int shift)
{
	gint i, j;
	gint width, height, padding;
	guchar *pixels;
	int val;

	if (!gdk_pixbuf_get_has_alpha(pixbuf))
	  return;

	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);
	padding = gdk_pixbuf_get_rowstride(pixbuf) - width * 4;
	pixels = gdk_pixbuf_get_pixels(pixbuf);

	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			pixels++;
			pixels++;
			pixels++;
			val = *pixels - shift;
			*(pixels++) = CLAMP(val, 0, 255);
		}
		pixels += padding;
	}
}


static GdkPixbuf *pidgin_blist_get_buddy_icon(PurpleBlistNode *node,
                                              gboolean scaled, gboolean greyed)
{
	gsize len;
	PurpleBuddy *buddy = NULL;
	PurpleGroup *group = NULL;
	const guchar *data = NULL;
	GdkPixbuf *buf, *ret = NULL;
	PurpleBuddyIcon *icon = NULL;
	PurpleAccount *account = NULL;
	PurpleContact *contact = NULL;
	PurpleImage *custom_img;
	PurpleProtocol *protocol = NULL;
	PurpleBuddyIconSpec *icon_spec = NULL;
	gint orig_width, orig_height, scale_width, scale_height;

	if (PURPLE_IS_CONTACT(node)) {
		buddy = purple_contact_get_priority_buddy((PurpleContact*)node);
		contact = (PurpleContact*)node;
	} else if (PURPLE_IS_BUDDY(node)) {
		buddy = (PurpleBuddy*)node;
		contact = purple_buddy_get_contact(buddy);
	} else if (PURPLE_IS_GROUP(node)) {
		group = (PurpleGroup*)node;
	} else if (PURPLE_IS_CHAT(node)) {
		/* We don't need to do anything here. We just need to not fall
		 * into the else block and return. */
	} else {
		return NULL;
	}

	if (buddy) {
		account = purple_buddy_get_account(buddy);
	}

	if(account && purple_account_get_connection(account)) {
		protocol = purple_connection_get_protocol(purple_account_get_connection(account));
	}

#if 0
	if (!purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_buddy_icons"))
		return NULL;
#endif

	/* If we have a contact then this is either a contact or a buddy and
	 * we want to fetch the custom icon for the contact. If we don't have
	 * a contact then this is a group or some other type of node and we
	 * want to use that directly. */
	if (contact) {
		custom_img = purple_buddy_icons_node_find_custom_icon(PURPLE_BLIST_NODE(contact));
	} else {
		custom_img = purple_buddy_icons_node_find_custom_icon(node);
	}

	if (custom_img) {
		data = purple_image_get_data(custom_img);
		len = purple_image_get_data_size(custom_img);
	}

	if (data == NULL) {
		if (buddy) {
			/* Not sure I like this...*/
			if (!(icon = purple_buddy_icons_find(purple_buddy_get_account(buddy), purple_buddy_get_name(buddy))))
				return NULL;
			data = purple_buddy_icon_get_data(icon, &len);
		}

		if(data == NULL)
			return NULL;
	}

	buf = pidgin_pixbuf_from_data(data, len);
	purple_buddy_icon_unref(icon);
	if (!buf) {
		purple_debug_warning("gtkblist", "Couldn't load buddy icon on "
			"account %s (%s); buddyname=%s; custom_img_size=%" G_GSIZE_FORMAT,
			account ? purple_account_get_username(account) : "(no account)",
			account ? purple_account_get_protocol_id(account) : "(no account)",
			buddy ? purple_buddy_get_name(buddy) : "(no buddy)",
			custom_img ? purple_image_get_data_size(custom_img) : 0);
		if (custom_img)
			g_object_unref(custom_img);
		return NULL;
	}
	if (custom_img)
		g_object_unref(custom_img);

	if (greyed) {
		gboolean offline = FALSE, idle = FALSE;

		if (buddy) {
			PurplePresence *presence = purple_buddy_get_presence(buddy);
			if (!PURPLE_BUDDY_IS_ONLINE(buddy))
				offline = TRUE;
			if (purple_presence_is_idle(presence))
				idle = TRUE;
		} else if (group) {
			if (purple_counting_node_get_online_count(PURPLE_COUNTING_NODE(group)) == 0)
				offline = TRUE;
		}

		if (offline)
			gdk_pixbuf_saturate_and_pixelate(buf, buf, 0.0, FALSE);

		if (idle)
			gdk_pixbuf_saturate_and_pixelate(buf, buf, 0.25, FALSE);
	}

	/* I'd use the pidgin_buddy_icon_get_scale_size() thing, but it won't
	 * tell me the original size, which I need for scaling purposes. */
	scale_width = orig_width = gdk_pixbuf_get_width(buf);
	scale_height = orig_height = gdk_pixbuf_get_height(buf);

	if (protocol)
		icon_spec = purple_protocol_get_icon_spec(protocol);

	if (icon_spec && (icon_spec->scale_rules & PURPLE_ICON_SCALE_DISPLAY)) {
		purple_buddy_icon_spec_get_scaled_size(icon_spec, &scale_width,
		                                       &scale_height);
	}

	purple_buddy_icon_spec_free(icon_spec);

	if (scaled || scale_height > 200 || scale_width > 200) {
		GdkPixbuf *tmpbuf;
		float scale_size = scaled ? 32.0 : 200.0;
		if(scale_height > scale_width) {
			scale_width = scale_size * (double)scale_width / (double)scale_height;
			scale_height = scale_size;
		} else {
			scale_height = scale_size * (double)scale_height / (double)scale_width;
			scale_width = scale_size;
		}
		/* Scale & round before making square, so rectangular (but
		 * non-square) images get rounded corners too. */
		tmpbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, scale_width, scale_height);
		gdk_pixbuf_fill(tmpbuf, 0x00000000);
		gdk_pixbuf_scale(buf, tmpbuf, 0, 0, scale_width, scale_height, 0, 0, (double)scale_width/(double)orig_width, (double)scale_height/(double)orig_height, GDK_INTERP_BILINEAR);
		if (pidgin_gdk_pixbuf_is_opaque(tmpbuf))
			pidgin_gdk_pixbuf_make_round(tmpbuf);
		ret = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, scale_size, scale_size);
		gdk_pixbuf_fill(ret, 0x00000000);
		gdk_pixbuf_copy_area(tmpbuf, 0, 0, scale_width, scale_height, ret, (scale_size-scale_width)/2, (scale_size-scale_height)/2);
		g_object_unref(G_OBJECT(tmpbuf));
	} else {
		ret = gdk_pixbuf_scale_simple(buf,scale_width,scale_height, GDK_INTERP_BILINEAR);
	}
	g_object_unref(G_OBJECT(buf));

	return ret;
}

static gboolean pidgin_blist_expand_timeout(GtkWidget *tv)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	PurpleBlistNode *node;
	PidginBlistNode *gtknode;

	if (!gtk_tree_view_get_path_at_pos(
	        GTK_TREE_VIEW(tv),
	        gtkblist->drag_rect.x,
	        gtkblist->drag_rect.y + (gtkblist->drag_rect.height/2),
	        &path, NULL, NULL, NULL))
	{
		return FALSE;
	}
	gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &iter, path);
	gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);

	if(!PURPLE_IS_CONTACT(node)) {
		gtk_tree_path_free(path);
		return FALSE;
	}

	gtknode = g_object_get_data(G_OBJECT(node), UI_DATA);

	if (!gtknode->contact_expanded) {
		GtkTreeIter i;

		pidgin_blist_expand_contact_cb(NULL, node);

		gtk_tree_view_get_cell_area(GTK_TREE_VIEW(tv), path, NULL, &gtkblist->contact_rect);
		gtkblist->contact_rect.width =
				gdk_window_get_width(gtk_widget_get_window(tv));
		gtkblist->mouseover_contact = node;
		gtk_tree_path_down (path);
		while (gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &i, path)) {
			GdkRectangle rect;
			gtk_tree_view_get_cell_area(GTK_TREE_VIEW(tv), path, NULL, &rect);
			gtkblist->contact_rect.height += rect.height;
			gtk_tree_path_next(path);
		}
	}
	gtk_tree_path_free(path);
	return FALSE;
}

static gboolean buddy_is_displayable(PurpleBuddy *buddy)
{
	PidginBlistNode *gtknode;

	if(!buddy)
		return FALSE;

	gtknode = g_object_get_data(G_OBJECT(buddy), UI_DATA);

	return (purple_account_is_connected(purple_buddy_get_account(buddy)) &&
			(purple_presence_is_online(purple_buddy_get_presence(buddy)) ||
			 (gtknode && gtknode->recent_signonoff) ||
			 purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_offline_buddies") ||
			 purple_blist_node_get_bool(PURPLE_BLIST_NODE(buddy), "show_offline")));
}

static gboolean pidgin_blist_drag_motion_cb(GtkWidget *tv, GdkDragContext *drag_context,
					      gint x, gint y, guint time, gpointer user_data)
{
	GtkTreePath *path;
	int delay;
	GdkRectangle rect;

	/*
	 * When dragging a buddy into a contact, this is the delay before
	 * the contact auto-expands.
	 */
	delay = 900;

	if (gtkblist->drag_timeout) {
		if (y > gtkblist->drag_rect.y &&
		        (y - gtkblist->drag_rect.height) < gtkblist->drag_rect.y)
		{
			return FALSE;
		}
		/* We've left the cell.  Remove the timeout and create a new one below */
		g_source_remove(gtkblist->drag_timeout);
	}

	gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(tv), x, y, &path, NULL, NULL, NULL);
	gtk_tree_view_get_cell_area(GTK_TREE_VIEW(tv), path, NULL, &rect);

	if (path)
		gtk_tree_path_free(path);

	/* Only autoexpand when in the middle of the cell to avoid annoying un-intended expands */
	if (y < rect.y + (rect.height / 3) ||
	    y > rect.y + (2 * (rect.height /3)))
		return FALSE;

	rect.height = rect.height / 3;
	rect.y += rect.height;

	gtkblist->drag_rect = rect;
	gtkblist->drag_timeout = g_timeout_add(delay, (GSourceFunc)pidgin_blist_expand_timeout, tv);

	if (gtkblist->mouseover_contact) {
		if ((y < gtkblist->contact_rect.y) || ((y - gtkblist->contact_rect.height) > gtkblist->contact_rect.y)) {
			pidgin_blist_collapse_contact_cb(NULL, gtkblist->mouseover_contact);
			gtkblist->mouseover_contact = NULL;
		}
	}

	return FALSE;
}

/* # - Status Icon
 * P - Protocol Icon
 * A - Buddy Icon
 * [ - SMALL_SPACE
 * = - LARGE_SPACE
 *                   +--- STATUS_SIZE                +--- td->avatar_width
 *                   |         +-- td->name_width    |
 *                +----+   +-------+            +---------+
 *                |    |   |       |            |         |
 *                +-------------------------------------------+
 *                |       [          =        [               |--- TOOLTIP_BORDER
 *name_height --+-| ######[BuddyName = PP     [   AAAAAAAAAAA |--+
 *              | | ######[          = PP     [   AAAAAAAAAAA |  |
 * STATUS SIZE -| | ######[[[[[[[[[[[[[[[[[[[[[   AAAAAAAAAAA |  |
 *           +--+-| ######[Account: So-and-so [   AAAAAAAAAAA |  |-- td->avatar_height
 *           |    |       [Idle: 4h 15m       [   AAAAAAAAAAA |  |
 *  height --+    |       [Foo: Bar, Baz      [   AAAAAAAAAAA |  |
 *           |    |       [Status: Awesome    [   AAAAAAAAAAA |--+
 *           +----|       [Stop: Hammer Time  [               |
 *                |       [                   [               |--- TOOLTIP_BORDER
 *                +-------------------------------------------+
 *                 |       |                |                |
 *                 |       +----------------+                |
 *                 |               |                         |
 *                 |               +-- td->width             |
 *                 |                                         |
 *                 +---- TOOLTIP_BORDER                      +---- TOOLTIP_BORDER
 *
 *
 */
#define STATUS_SIZE 16
#define TOOLTIP_BORDER 12
#define SMALL_SPACE 6
#define LARGE_SPACE 12

static void
add_tip_for_account(GtkWidget *grid, gint row, PurpleAccount *account)
{
	GdkPixbuf *protocol_icon = NULL;
	GtkWidget *image = NULL;
	GtkWidget *name = NULL;

	protocol_icon = pidgin_create_protocol_icon(account, PIDGIN_PROTOCOL_ICON_SMALL);
	if (purple_account_is_disconnected(account)) {
		gdk_pixbuf_saturate_and_pixelate(protocol_icon, protocol_icon, 0.0, FALSE);
	}
	image = gtk_image_new_from_pixbuf(protocol_icon);
	gtk_image_set_pixel_size(GTK_IMAGE(image), STATUS_SIZE);
	gtk_grid_attach(GTK_GRID(grid), image, 0, row, 1, 1);
	g_clear_object(&protocol_icon);

	name = gtk_label_new(purple_account_get_username(account));
	gtk_label_set_xalign(GTK_LABEL(name), 0);
	gtk_label_set_yalign(GTK_LABEL(name), 0);
	gtk_label_set_line_wrap(GTK_LABEL(name), TRUE);
	gtk_label_set_max_width_chars(GTK_LABEL(name), 36);
	gtk_grid_attach(GTK_GRID(grid), name, 1, row, 1, 1);
}

static void
add_tip_for_node(GtkWidget *grid, gint row, PurpleBlistNode *node, gboolean full)
{
	GdkPixbuf *status_icon = NULL;
	GtkWidget *image = NULL;
	GtkWidget *name = NULL;
	GdkPixbuf *avatar = NULL;
	GtkWidget *avatar_image = NULL;
	PurpleAccount *account = NULL;
	char *tmp = NULL, *node_name = NULL, *tooltip_text = NULL;

	if (PURPLE_IS_BUDDY(node)) {
		account = purple_buddy_get_account(PURPLE_BUDDY(node));
	} else if (PURPLE_IS_CHAT(node)) {
		account = purple_chat_get_account(PURPLE_CHAT(node));
	}

	status_icon = pidgin_blist_get_status_icon(node, PIDGIN_STATUS_ICON_LARGE);
	image = gtk_image_new_from_pixbuf(status_icon);
	gtk_image_set_pixel_size(GTK_IMAGE(image), STATUS_SIZE);
	gtk_grid_attach(GTK_GRID(grid), image, 0, row, 1, 1);
	g_clear_object(&status_icon);

	if (PURPLE_IS_BUDDY(node)) {
		tmp = g_markup_escape_text(purple_buddy_get_name(PURPLE_BUDDY(node)), -1);
	} else if (PURPLE_IS_CHAT(node)) {
		tmp = g_markup_escape_text(purple_chat_get_name(PURPLE_CHAT(node)), -1);
	} else if (PURPLE_IS_GROUP(node)) {
		tmp = g_markup_escape_text(purple_group_get_name(PURPLE_GROUP(node)), -1);
	} else {
		/* I don't believe this can happen currently, I think
		 * everything that calls this function checks for one of the
		 * above node types first. */
		tmp = g_strdup(_("Unknown node type"));
	}
	node_name = g_strdup_printf("<span size='x-large' weight='bold'>%s</span>",
								tmp ? tmp : "");
	g_free(tmp);

	name = gtk_label_new(NULL);
	gtk_label_set_xalign(GTK_LABEL(name), 0);
	gtk_label_set_yalign(GTK_LABEL(name), 0);
	gtk_label_set_markup(GTK_LABEL(name), node_name);
	gtk_label_set_max_width_chars(GTK_LABEL(name), 36);
	gtk_grid_attach(GTK_GRID(grid), name, 1, row, 1, 1);

	if (account != NULL) {
		GdkPixbuf *protocol_icon = pidgin_create_protocol_icon(
		        account, PIDGIN_PROTOCOL_ICON_SMALL);
		image = gtk_image_new_from_pixbuf(protocol_icon);
		gtk_image_set_pixel_size(GTK_IMAGE(image), STATUS_SIZE);
		gtk_widget_set_halign(image, GTK_ALIGN_END);
		gtk_grid_attach(GTK_GRID(grid), image, 2, row, 1, 1);
		g_clear_object(&protocol_icon);
	}

	tooltip_text = pidgin_get_tooltip_text(node, full);
	if (tooltip_text && *tooltip_text) {
		GtkWidget *contents = gtk_label_new(NULL);
		gtk_label_set_xalign(GTK_LABEL(contents), 0);
		gtk_label_set_yalign(GTK_LABEL(contents), 0);
		gtk_label_set_markup(GTK_LABEL(contents), tooltip_text);
		gtk_label_set_line_wrap(GTK_LABEL(contents), TRUE);
		gtk_label_set_max_width_chars(GTK_LABEL(contents), 36);
		gtk_grid_attach(GTK_GRID(grid), contents, 1, row+1, 2, 1);
	}

	avatar = pidgin_blist_get_buddy_icon(node, !full, FALSE);
#if 0  /* Protocol Icon as avatar */
	if(!avatar && full) {
		avatar = pidgin_create_protocol_icon(account, PIDGIN_PROTOCOL_ICON_LARGE);
	}
#endif

	if (avatar != NULL) {
		avatar_image = gtk_image_new_from_pixbuf(avatar);
		gtk_widget_set_halign(avatar_image, GTK_ALIGN_END);
		gtk_widget_set_valign(avatar_image, GTK_ALIGN_START);
		gtk_grid_attach(GTK_GRID(grid), avatar_image, 3, row, 1, 2);
		g_object_unref(avatar);
	}

	g_free(node_name);
	g_free(tooltip_text);
}

gboolean
pidgin_blist_query_tooltip_for_node(PurpleBlistNode *node, GtkTooltip *tooltip)
{
	GtkWidget *grid;

	grid = gtk_grid_new();

	if (PURPLE_IS_CHAT(node) || PURPLE_IS_BUDDY(node)) {
		add_tip_for_node(grid, 0, node, TRUE);

	} else if (PURPLE_IS_GROUP(node)) {
		PurpleGroup *group = PURPLE_GROUP(node);
		GSList *accounts;
		add_tip_for_node(grid, 0, node, TRUE);

		/* Accounts with buddies in group */
		accounts = purple_group_get_accounts(group);
		for (gint row = 2; accounts != NULL;
		     row++, accounts = g_slist_delete_link(accounts, accounts))
		{
			PurpleAccount *account = PURPLE_ACCOUNT(accounts->data);
			add_tip_for_account(grid, row, account);
		}

	} else if (PURPLE_IS_CONTACT(node)) {
		PurpleBlistNode *child;
		PurpleBuddy *b = purple_contact_get_priority_buddy(PURPLE_CONTACT(node));
		gint row = 0;

		for(child = node->child; child; child = child->next) {
			if(PURPLE_IS_BUDDY(child) && buddy_is_displayable(PURPLE_BUDDY(child))) {
				if (b == (PurpleBuddy *)child) {
					/* Priority buddy goes first (-2) and is more detailed. */
					add_tip_for_node(grid, -2, child, TRUE);
				} else {
					add_tip_for_node(grid, row, child, FALSE);
					row += 2;
				}
			}
		}

	} else {
		return FALSE;
	}

	gtk_widget_show_all(grid);
	gtk_tooltip_set_custom(tooltip, grid);

	return TRUE;
}

static gboolean
pidgin_blist_query_tooltip(GtkWidget *widget, int x, int y,
                           gboolean keyboard_mode, GtkTooltip *tooltip,
                           gpointer userdata)
{
	PidginBuddyList *blist = userdata;
	GtkTreeIter iter;
	PurpleBlistNode *node;
	gboolean editable = FALSE;
	GtkTreePath *path = NULL;

	/* If we're editing a cell (e.g. alias editing), don't show the tooltip */
	g_object_get(G_OBJECT(blist->text_rend), "editable", &editable, NULL);
	if (editable) {
		return FALSE;
	}

	if (keyboard_mode) {
		GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
		if (!gtk_tree_selection_get_selected(selection, NULL, &iter)) {
			return FALSE;
		}
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(blist->treemodel), &iter);
	} else {
		gint bx, by;

		gtk_tree_view_convert_widget_to_bin_window_coords(GTK_TREE_VIEW(widget),
		                                                  x, y, &bx, &by);
		gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), bx, by, &path,
		                              NULL, NULL, NULL);
		if (path == NULL) {
			return FALSE;
		}
		if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(blist->treemodel),
		                             &iter, path))
		{
			gtk_tree_path_free(path);
			return FALSE;
		}
	}

	gtk_tree_model_get(GTK_TREE_MODEL(blist->treemodel), &iter,
	                   NODE_COLUMN, &node, -1);
	if (pidgin_blist_query_tooltip_for_node(node, tooltip)) {
		gtk_tree_view_set_tooltip_row(GTK_TREE_VIEW(widget), tooltip, path);
		gtk_tree_path_free(path);
		return TRUE;
	} else {
		gtk_tree_path_free(path);
		return FALSE;
	}
}

static gboolean pidgin_blist_motion_cb (GtkWidget *tv, GdkEventMotion *event, gpointer null)
{
	if (gtkblist->mouseover_contact) {
		if ((event->y < gtkblist->contact_rect.y) || ((event->y - gtkblist->contact_rect.height) > gtkblist->contact_rect.y)) {
			pidgin_blist_collapse_contact_cb(NULL, gtkblist->mouseover_contact);
			gtkblist->mouseover_contact = NULL;
		}
	}

	return FALSE;
}

static gboolean pidgin_blist_leave_cb (GtkWidget *w, GdkEventCrossing *e, gpointer n)
{
	if (gtkblist->drag_timeout) {
		g_source_remove(gtkblist->drag_timeout);
		gtkblist->drag_timeout = 0;
	}

	if (gtkblist->mouseover_contact &&
		!((e->x > gtkblist->contact_rect.x) && (e->x < (gtkblist->contact_rect.x + gtkblist->contact_rect.width)) &&
		 (e->y > gtkblist->contact_rect.y) && (e->y < (gtkblist->contact_rect.y + gtkblist->contact_rect.height)))) {
			pidgin_blist_collapse_contact_cb(NULL, gtkblist->mouseover_contact);
		gtkblist->mouseover_contact = NULL;
	}
	return FALSE;
}

/*********************************************************
 * Private Utility functions                             *
 *********************************************************/

static char *pidgin_get_tooltip_text(PurpleBlistNode *node, gboolean full)
{
	GString *str = g_string_new("");
	PurpleProtocol *protocol = NULL;
	char *tmp;

	if (PURPLE_IS_CHAT(node))
	{
		PurpleAccount *account;
		PurpleChat *chat;
		GList *connections;
		GList *cur = NULL;
		PurpleProtocolChatEntry *pce;
		char *name, *value;
		PurpleChatConversation *conv = NULL;
		PidginBlistNode *bnode = g_object_get_data(G_OBJECT(node), UI_DATA);

		chat = (PurpleChat *)node;
		account = purple_chat_get_account(chat);
		protocol = purple_account_get_protocol(account);

		connections = purple_connections_get_all();
		if (connections && connections->next)
		{
			tmp = g_markup_escape_text(purple_account_get_username(account), -1);
			g_string_append_printf(str, _("<b>Account:</b> %s"), tmp);
			g_free(tmp);
		}

		if (bnode && bnode->conv.conv) {
			conv = PURPLE_CHAT_CONVERSATION(bnode->conv.conv);
		} else {
			PurpleConversation *chat_conv;
			PurpleConversationManager *manager;
			char *chat_name;

			if (protocol && PURPLE_PROTOCOL_IMPLEMENTS(protocol, CHAT, get_name)) {
				chat_name = purple_protocol_chat_get_name(PURPLE_PROTOCOL_CHAT(protocol),
				                                          purple_chat_get_components(chat));
			} else {
				chat_name = g_strdup(purple_chat_get_name(chat));
			}

			manager = purple_conversation_manager_get_default();
			chat_conv = purple_conversation_manager_find_chat(manager, account,
			                                                  chat_name);
			g_free(chat_name);

			if(PURPLE_IS_CHAT_CONVERSATION(chat_conv)) {
				conv = PURPLE_CHAT_CONVERSATION(chat_conv);
			}
		}

		if (conv && !purple_chat_conversation_has_left(conv)) {
			g_string_append_printf(str, _("\n<b>Occupants:</b> %d"),
				purple_chat_conversation_get_users_count(conv));

			if (protocol && (purple_protocol_get_options(protocol) & OPT_PROTO_CHAT_TOPIC)) {
				const char *chattopic = purple_chat_conversation_get_topic(conv);
				char *topic = chattopic ? g_markup_escape_text(chattopic, -1) : NULL;
				g_string_append_printf(str, _("\n<b>Topic:</b> %s"), topic ? topic : _("(no topic set)"));
				g_free(topic);
			}
		}

		if(protocol) {
			cur = purple_protocol_chat_info(PURPLE_PROTOCOL_CHAT(protocol),
			                                purple_account_get_connection(account));
		}

		while (cur != NULL)
		{
			pce = cur->data;

			if (!pce->secret)
			{
				tmp = purple_text_strip_mnemonic(pce->label);
				name = g_markup_escape_text(tmp, -1);
				g_free(tmp);
				value = g_markup_escape_text(g_hash_table_lookup(
										purple_chat_get_components(chat), pce->identifier), -1);
				g_string_append_printf(str, "\n<b>%s</b> %s",
							name ? name : "",
							value ? value : "");
				g_free(name);
				g_free(value);
			}

			g_free(pce);
			cur = g_list_delete_link(cur, cur);
		}
	}
	else if (PURPLE_IS_CONTACT(node) || PURPLE_IS_BUDDY(node))
	{
		/* NOTE: THIS FUNCTION IS NO LONGER CALLED FOR CONTACTS.
		 * It is only called by create_tip_for_node(), and create_tip_for_node() is never called for a contact.
		 */
		PurpleAccount *account;
		PurpleContact *c;
		PurpleBuddy *b;
		PurplePresence *presence;
		PurpleNotifyUserInfo *user_info;
		GList *connections;
		char *tmp;
		gchar *alias;
		time_t idle_secs, signon;

		if (PURPLE_IS_CONTACT(node))
		{
			c = (PurpleContact *)node;
			b = purple_contact_get_priority_buddy(c);
		}
		else
		{
			b = (PurpleBuddy *)node;
			c = purple_buddy_get_contact(b);
		}

		account = purple_buddy_get_account(b);
		protocol = purple_account_get_protocol(account);

		presence = purple_buddy_get_presence(b);
		user_info = purple_notify_user_info_new();

		/* Account */
		connections = purple_connections_get_all();
		if (full && connections && connections->next)
		{
			purple_notify_user_info_add_pair_plaintext(user_info, _("Account"),
					purple_account_get_username(account));
		}

		/* Alias */
		/* If there's not a contact alias, the node is being displayed with
		 * this alias, so there's no point in showing it in the tooltip. */
		g_object_get(c, "alias", &alias, NULL);
		if (full && c && purple_buddy_get_local_alias(b) != NULL && purple_buddy_get_local_alias(b)[0] != '\0' &&
		    (alias != NULL && alias[0] != '\0') &&
		    !purple_strequal(alias, purple_buddy_get_local_alias(b)))
		{
			purple_notify_user_info_add_pair_plaintext(user_info,
					_("Buddy Alias"), purple_buddy_get_local_alias(b));
		}

		/* Nickname/Server Alias */
		/* I'd like to only show this if there's a contact or buddy
		 * alias, but people often set long nicknames, which
		 * get ellipsized, so the only way to see the whole thing is
		 * to look at the tooltip. */
		if (full && purple_buddy_get_server_alias(b))
		{
			purple_notify_user_info_add_pair_plaintext(user_info,
					_("Nickname"), purple_buddy_get_server_alias(b));
		}

		/* Logged In */
		signon = purple_presence_get_login_time(presence);
		if (full && PURPLE_BUDDY_IS_ONLINE(b) && signon > 0)
		{
			if (signon > time(NULL)) {
				/*
				 * They signed on in the future?!  Our local clock
				 * must be wrong, show the actual date instead of
				 * "4 days", etc.
				 */
				GDateTime *dt = NULL, *local = NULL;

				dt = g_date_time_new_from_unix_utc(signon);
				local = g_date_time_to_local(dt);
				g_date_time_unref(dt);

				tmp = g_date_time_format(local, _("%x %X"));
				g_date_time_unref(local);
			} else {
				tmp = purple_str_seconds_to_string(time(NULL) - signon);
			}
			purple_notify_user_info_add_pair_plaintext(user_info, _("Logged In"), tmp);
			g_free(tmp);
		}

		/* Idle */
		if (purple_presence_is_idle(presence))
		{
			idle_secs = purple_presence_get_idle_time(presence);
			if (idle_secs > 0)
			{
				tmp = purple_str_seconds_to_string(time(NULL) - idle_secs);
				purple_notify_user_info_add_pair_plaintext(user_info, _("Idle"), tmp);
				g_free(tmp);
			}
		}

		/* Last Seen */
		if (full && c && !PURPLE_BUDDY_IS_ONLINE(b))
		{
			PidginBlistNode *gtknode = g_object_get_data(G_OBJECT(c), UI_DATA);
			PurpleBlistNode *bnode;
			int lastseen = 0;

			if (gtknode && (!gtknode->contact_expanded || PURPLE_IS_CONTACT(node)))
			{
				/* We're either looking at a buddy for a collapsed contact or
				 * an expanded contact itself so we show the most recent
				 * (largest) last_seen time for any of the buddies under
				 * the contact. */
				for (bnode = ((PurpleBlistNode *)c)->child ; bnode != NULL ; bnode = bnode->next)
				{
					int value = purple_blist_node_get_int(bnode, "last_seen");
					if (value > lastseen)
						lastseen = value;
				}
			}
			else
			{
				/* We're dealing with a buddy under an expanded contact,
				 * so we show the last_seen time for the buddy. */
				lastseen = purple_blist_node_get_int(&b->node, "last_seen");
			}

			if (lastseen > 0)
			{
				tmp = purple_str_seconds_to_string(time(NULL) - lastseen);
				purple_notify_user_info_add_pair_plaintext(user_info, _("Last Seen"), tmp);
				g_free(tmp);
			}
		}


		/* Offline? */
		/* FIXME: Why is this status special-cased by the core? --rlaager
		 * FIXME: Alternatively, why not have the core do all of them? --rlaager */
		if (!PURPLE_BUDDY_IS_ONLINE(b)) {
			purple_notify_user_info_add_pair_plaintext(user_info, _("Status"), _("Offline"));
		}

		if (purple_account_is_connected(account) && protocol) {
			/* Additional text from the protocol */
			purple_protocol_client_tooltip_text(PURPLE_PROTOCOL_CLIENT(protocol), b, user_info, full);
		}

		/* These are Easter Eggs.  Patches to remove them will be rejected. */
		if (!g_ascii_strcasecmp(purple_buddy_get_name(b), "robflynn"))
			purple_notify_user_info_add_pair_plaintext(user_info, _("Description"), _("Spooky"));
		if (!g_ascii_strcasecmp(purple_buddy_get_name(b), "seanegn"))
			purple_notify_user_info_add_pair_plaintext(user_info, _("Status"), _("Awesome"));
		if (!g_ascii_strcasecmp(purple_buddy_get_name(b), "chipx86"))
			purple_notify_user_info_add_pair_plaintext(user_info, _("Status"), _("Rockin'"));

		tmp = purple_notify_user_info_get_text_with_newline(user_info, "\n");
		g_string_append(str, tmp);
		g_free(tmp);
		g_free(alias);

		purple_notify_user_info_destroy(user_info);
	} else if (PURPLE_IS_GROUP(node)) {
		gint count;
		PurpleGroup *group = (PurpleGroup*)node;
		PurpleNotifyUserInfo *user_info;

		user_info = purple_notify_user_info_new();

		count = purple_counting_node_get_online_count(PURPLE_COUNTING_NODE(group));
		if (count != 0) {
			/* Online buddies in group */
			char tmp2[12];
			sprintf(tmp2, "%d", count);
			purple_notify_user_info_add_pair_plaintext(user_info,
					_("Online Buddies"), tmp2);
		}

		count = purple_counting_node_get_current_size(PURPLE_COUNTING_NODE(group));
		if (count != 0) {
			/* Total buddies (from online accounts) in group */
			char tmp2[12];
			sprintf(tmp2, "%d", count);
			purple_notify_user_info_add_pair_html(user_info,
					_("Total Buddies"), tmp2);
		}

		tmp = purple_notify_user_info_get_text_with_newline(user_info, "\n");
		g_string_append(str, tmp);
		g_free(tmp);

		purple_notify_user_info_destroy(user_info);
	}

	purple_signal_emit(pidgin_blist_get_handle(), "drawing-tooltip",
	                   node, str, full);

	return g_string_free(str, FALSE);
}

static GHashTable *cached_emblems;

static void _cleanup_cached_emblem(gpointer data, GObject *obj) {
	g_hash_table_remove(cached_emblems, data);
}

static GdkPixbuf * _pidgin_blist_get_cached_emblem(gchar *path) {
	GdkPixbuf *pb = g_hash_table_lookup(cached_emblems, path);

	if (pb != NULL) {
		/* The caller gets a reference */
		g_object_ref(pb);
		g_free(path);
	} else {
		pb = pidgin_pixbuf_new_from_file(path);
		if (pb != NULL) {
			/* We don't want to own a ref to the pixbuf, but we need to keep clean up. */
			/* I'm not sure if it would be better to just keep our ref and not let the emblem ever be destroyed */
			g_object_weak_ref(G_OBJECT(pb), _cleanup_cached_emblem, path);
			g_hash_table_insert(cached_emblems, path, pb);
		} else
			g_free(path);
	}

	return pb;
}

GdkPixbuf *
pidgin_blist_get_emblem(PurpleBlistNode *node)
{
	PurpleAccount *account;
	PurpleBuddy *buddy = NULL;
	PidginBlistNode *gtknode = g_object_get_data(G_OBJECT(node), UI_DATA);
	PurpleProtocol *protocol;
	const char *name = NULL;
	char *filename, *path;
	PurplePresence *p = NULL;
	PurpleStatus *tune;

	if(PURPLE_IS_CONTACT(node)) {
		if(!gtknode->contact_expanded) {
			buddy = purple_contact_get_priority_buddy((PurpleContact*)node);
		}
	} else if(PURPLE_IS_BUDDY(node)) {
		PidginBlistNode *pidgin_node = NULL;
		buddy = (PurpleBuddy*)node;

		p = purple_buddy_get_presence(buddy);
		if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_MOBILE)) {
			/* This emblem comes from the small emoticon set now,
			 * to reduce duplication. */
			path = g_build_filename(PURPLE_DATADIR, "pixmaps",
				"pidgin", "emotes", "small", "mobile.png", NULL);
			return _pidgin_blist_get_cached_emblem(path);
		}

		pidgin_node = g_object_get_data(G_OBJECT(node->parent), UI_DATA);
		if(pidgin_node->contact_expanded) {
			if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_protocol_icons"))
				return NULL;
			return pidgin_create_protocol_icon(purple_buddy_get_account((PurpleBuddy*)node), PIDGIN_PROTOCOL_ICON_SMALL);
		}
	} else {
		return NULL;
	}

	g_return_val_if_fail(buddy != NULL, NULL);

	account = purple_buddy_get_account(buddy);
	if (!purple_account_privacy_check(account, purple_buddy_get_name(buddy))) {
		path = g_build_filename(PURPLE_DATADIR, "pidgin", "icons",
			"hicolor", "16x16", "emblems", "emblem-blocked.png",
			NULL);
		return _pidgin_blist_get_cached_emblem(path);
	}

	/* If we came through the contact code flow above, we didn't need
	 * to get the presence until now. */
	if (p == NULL)
		p = purple_buddy_get_presence(buddy);

	if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_MOBILE)) {
		/* This emblem comes from the small emoticon set now, to reduce duplication. */
		path = g_build_filename(PURPLE_DATADIR, "pixmaps", "pidgin",
			"emotes", "small", "mobile.png", NULL);
		return _pidgin_blist_get_cached_emblem(path);
	}

	tune = purple_presence_get_status(p, "tune");
	if (tune && purple_status_is_active(tune)) {
		/* TODO: Replace "Tune" with generalized "Media" in 3.0. */
		if (purple_status_get_attr_string(tune, "game") != NULL) {
			path = g_build_filename(PURPLE_DATADIR, "pidgin",
				"icons", "hicolor", "16x16", "emblems",
				"emblem-game.png", NULL);
			return _pidgin_blist_get_cached_emblem(path);
		}
		/* TODO: Replace "Tune" with generalized "Media" in 3.0. */
		if (purple_status_get_attr_string(tune, "office") != NULL) {
			path = g_build_filename(PURPLE_DATADIR, "pidgin",
				"icons", "hicolor", "16x16", "emblems",
				"emblem-office.png", NULL);
			return _pidgin_blist_get_cached_emblem(path);
		}
		/* Regular old "tune" is the only one in all protocols. */
		/* This emblem comes from the small emoticon set now, to reduce duplication. */
		path = g_build_filename(PURPLE_DATADIR, "pixmaps", "pidgin",
			"emotes", "small", "music.png", NULL);
		return _pidgin_blist_get_cached_emblem(path);
	}

	protocol = purple_account_get_protocol(account);
	if (!protocol)
		return NULL;

	name = purple_protocol_client_list_emblem(PURPLE_PROTOCOL_CLIENT(protocol), buddy);

	if (name == NULL) {
		PurpleStatus *status;

		if (!purple_presence_is_status_primitive_active(p, PURPLE_STATUS_MOOD))
			return NULL;

		status = purple_presence_get_status(p, "mood");
		name = purple_status_get_attr_string(status, PURPLE_MOOD_NAME);

		if (!(name && *name))
			return NULL;

		path = pidgin_mood_get_icon_path(name);
	} else {
		filename = g_strdup_printf("emblem-%s.png", name);
		path = g_build_filename(PURPLE_DATADIR, "pidgin", "icons",
			"hicolor", "16x16", "emblems", filename, NULL);
		g_free(filename);
	}

	/* _pidgin_blist_get_cached_emblem() assumes ownership of path */
	return _pidgin_blist_get_cached_emblem(path);
}


GdkPixbuf *
pidgin_blist_get_status_icon(PurpleBlistNode *node, PidginStatusIconSize size)
{
	GdkPixbuf *ret;
	const char *icon = NULL;
	gboolean trans = FALSE;
	PidginBlistNode *gtknode = g_object_get_data(G_OBJECT(node), UI_DATA);
	PidginBlistNode *gtkbuddynode = NULL;
	PurpleBuddy *buddy = NULL;
	PurpleChat *chat = NULL;
	gint icon_size = (size == PIDGIN_STATUS_ICON_LARGE) ? 16 : 11;

	if(PURPLE_IS_CONTACT(node)) {
		if(!gtknode->contact_expanded) {
			buddy = purple_contact_get_priority_buddy((PurpleContact*)node);
			if(buddy != NULL) {
				gtkbuddynode = g_object_get_data(G_OBJECT(buddy), UI_DATA);
			}
		}
	} else if(PURPLE_IS_BUDDY(node)) {
		buddy = (PurpleBuddy*)node;
		gtkbuddynode = g_object_get_data(G_OBJECT(node), UI_DATA);
	} else if(PURPLE_IS_CHAT(node)) {
		chat = (PurpleChat*)node;
	} else {
		return NULL;
	}

	if(buddy || chat) {
		PurpleAccount *account;
		PurpleProtocol *protocol;

		if(buddy)
			account = purple_buddy_get_account(buddy);
		else
			account = purple_chat_get_account(chat);

		protocol = purple_account_get_protocol(account);
		if(!protocol)
			return NULL;
	}

	if(buddy) {
		PurpleConversation *conv = find_conversation_with_buddy(buddy);

		if(conv != NULL) {
			PidginConversation *gtkconv = PIDGIN_CONVERSATION(conv);
			if (gtkconv == NULL && size == PIDGIN_STATUS_ICON_SMALL) {
				PidginBlistNode *ui = g_object_get_data(G_OBJECT(buddy), UI_DATA);
				if (ui == NULL || (ui->conv.flags & PIDGIN_BLIST_NODE_HAS_PENDING_MESSAGE)) {
					icon = "message-new";
				}
			}
		}

		if (icon == NULL) { /* The conversation didn't have a new message. */
			PurplePresence *p = purple_buddy_get_presence(buddy);
			trans = purple_presence_is_idle(p);

			if (PURPLE_BUDDY_IS_ONLINE(buddy) && gtkbuddynode && gtkbuddynode->recent_signonoff) {
				icon = "log-in";
			} else if (gtkbuddynode && gtkbuddynode->recent_signonoff) {
				icon = "log-out";
			} else if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_UNAVAILABLE)) {
				icon = "pidgin-user-busy";
			} else if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_AWAY)) {
				icon = "pidgin-user-away";
			} else if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_EXTENDED_AWAY)) {
				icon = "pidgin-user-extended-away";
			} else if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_OFFLINE)) {
				icon = "pidgin-user-offline";
			} else if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_INVISIBLE)) {
				icon = "pidgin-user-invisible";
			} else {
				icon = "pidgin-user-available";
			}
		}
	} else if (chat) {
		icon = "chat";
	} else {
		icon = "person";
	}

	ret = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), icon, icon_size, 0, NULL);
	if (trans) {
		GdkPixbuf *copy = gdk_pixbuf_copy(ret);
		g_object_unref(ret);
		do_alphashift(copy, 77);
		ret = copy;
	}

	return ret;
}

gchar *
pidgin_blist_get_name_markup(PurpleBuddy *b, gboolean selected, gboolean aliased)
{
	const char *name, *name_color, *status_color, *dim_grey;
	char *text = NULL;
	PurpleProtocol *protocol = NULL;
	PurpleContact *contact;
	PurplePresence *presence;
	PidginBlistNode *gtkcontactnode = NULL;
	char *idletime = NULL, *statustext = NULL, *nametext = NULL;
	PurpleConversation *conv = find_conversation_with_buddy(b);
	gboolean hidden_conv = FALSE;
	gboolean biglist = purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_buddy_icons");
	gchar *contact_alias;

	if (conv != NULL) {
		PidginBlistNode *ui = g_object_get_data(G_OBJECT(b), UI_DATA);
		if (ui) {
			if (ui->conv.flags & PIDGIN_BLIST_NODE_HAS_PENDING_MESSAGE)
				hidden_conv = TRUE;
		} else {
			if (PIDGIN_CONVERSATION(conv) == NULL)
				hidden_conv = TRUE;
		}
	}

	/* XXX Good luck cleaning up this crap */
	contact = PURPLE_CONTACT(PURPLE_BLIST_NODE(b)->parent);
	if(contact) {
		gtkcontactnode = g_object_get_data(G_OBJECT(contact), UI_DATA);
	}

	g_object_get(contact, "alias", &contact_alias, NULL);

	/* Name */
	if (gtkcontactnode && !gtkcontactnode->contact_expanded && contact_alias)
		name = contact_alias;
	else
		name = purple_buddy_get_alias(b);

	/* Raise a contact pre-draw signal here.  THe callback will return an
	 * escaped version of the name. */
	nametext = purple_signal_emit_return_1(pidgin_blist_get_handle(), "drawing-buddy", b);

	if(!nametext)
		nametext = g_markup_escape_text(name, strlen(name));

	presence = purple_buddy_get_presence(b);

	/* Name is all that is needed */
	if (!aliased || biglist) {
		PurpleAccount *account = purple_buddy_get_account(b);

		/* Status Info */
		protocol = purple_account_get_protocol(account);

		if (protocol && PURPLE_PROTOCOL_IMPLEMENTS(protocol, CLIENT, status_text) &&
				purple_account_get_connection(account)) {
			char *tmp = purple_protocol_client_status_text(PURPLE_PROTOCOL_CLIENT(protocol), b);
			const char *end;

			if(tmp && !g_utf8_validate(tmp, -1, &end)) {
				char *new = g_strndup(tmp,
						g_utf8_pointer_to_offset(tmp, end));
				g_free(tmp);
				tmp = new;
			}
			if(tmp) {
				g_strdelimit(tmp, "\n", ' ');
				purple_str_strip_char(tmp, '\r');
			}
			statustext = tmp;
		}

		if(!purple_presence_is_online(presence) && !statustext)
				statustext = g_strdup(_("Offline"));

		/* Idle Text */
		if (purple_presence_is_idle(presence) && purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_idle_time")) {
			time_t idle_secs = purple_presence_get_idle_time(presence);

			if (idle_secs > 0) {
				int iday, ihrs, imin;
				time_t t;

				time(&t);
				iday = (t - idle_secs) / (24 * 60 * 60);
				ihrs = ((t - idle_secs) / 60 / 60) % 24;
				imin = ((t - idle_secs) / 60) % 60;

				if (iday)
					idletime = g_strdup_printf(_("Idle %dd %dh %02dm"), iday, ihrs, imin);
				else if (ihrs)
					idletime = g_strdup_printf(_("Idle %dh %02dm"), ihrs, imin);
				else
					idletime = g_strdup_printf(_("Idle %dm"), imin);

			} else
				idletime = g_strdup(_("Idle"));
		}
	}

	dim_grey = pidgin_style_context_is_dark() ? "light slate grey" : "dim grey";

	/* choose the colors of the text */
	name_color = NULL;
	status_color = dim_grey;

	if(!selected) {
		if(purple_presence_is_idle(presence) ||
		   !purple_presence_is_online(presence))
		{
			name_color = dim_grey;
		}
	}

	if(aliased && selected) {
		name_color = NULL;
		status_color = NULL;
	}

	if (hidden_conv) {
		char *tmp = nametext;
		nametext = g_strdup_printf("<b>%s</b>", tmp);
		g_free(tmp);
	}

	/* Put it all together */
	if ((!aliased || biglist) && (statustext || idletime)) {
		/* using <span size='smaller'> breaks the status, so it must be separated into <small><span>*/
		if (name_color) {
			text = g_strdup_printf("<span foreground='%s'>%s</span>\n"
			                       "<small><span foreground='%s'>%s%s%s</span></small>",
			                       name_color, nametext, status_color,
			                       idletime != NULL ? idletime : "",
			                       (idletime != NULL && statustext != NULL) ? " - " : "",
			                       statustext != NULL ? statustext : "");
		} else if (status_color) {
			text = g_strdup_printf("%s\n<small><span foreground='%s'>%s%s%s</span></small>",
			                       nametext, status_color,
			                       idletime != NULL ? idletime : "",
			                       (idletime != NULL && statustext != NULL) ? " - " : "",
			                       statustext != NULL ? statustext : "");
		} else {
			text = g_strdup_printf("%s\n<small>%s%s%s</small>",
			                       nametext,
			                       idletime != NULL ? idletime : "",
			                       (idletime != NULL && statustext != NULL) ? " - " : "",
			                       statustext != NULL ? statustext : "");
		}
	} else {
		if (name_color) {
			text = g_strdup_printf("<span color='%s'>%s</span>",
			                       name_color, nametext);
		} else {
			text = g_strdup_printf("%s", nametext);
		}
	}
	g_free(nametext);
	g_free(statustext);
	g_free(idletime);
	g_free(contact_alias);

	if (hidden_conv) {
		char *tmp = text;
		text = g_strdup_printf("<b>%s</b>", tmp);
		g_free(tmp);
	}

	return text;
}

static void pidgin_blist_restore_window_state(void)
{
	int blist_width, blist_height;

	blist_width = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/blist/width");

	/* if the window exists, is hidden, we're saving sizes, and the
	 * size is sane... */
	if (gtkblist && gtkblist->window &&
		!gtk_widget_get_visible(gtkblist->window) && blist_width != 0) {

		blist_height = purple_prefs_get_int(PIDGIN_PREFS_ROOT "/blist/height");

		gtk_window_set_default_size(GTK_WINDOW(gtkblist->window),
				blist_width, blist_height);
		if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/list_maximized"))
			gtk_window_maximize(GTK_WINDOW(gtkblist->window));
	}
}

static gboolean pidgin_blist_refresh_timer(PurpleBuddyList *list)
{
	PurpleBlistNode *gnode, *cnode;

	if (gtk_blist_visibility == GDK_VISIBILITY_FULLY_OBSCURED
			|| !gtk_widget_get_visible(gtkblist->window))
		return TRUE;

	for (gnode = purple_blist_get_root(list); gnode; gnode = gnode->next) {
		if(!PURPLE_IS_GROUP(gnode))
			continue;
		for(cnode = gnode->child; cnode; cnode = cnode->next) {
			if(PURPLE_IS_CONTACT(cnode)) {
				PurpleBuddy *buddy;

				buddy = purple_contact_get_priority_buddy((PurpleContact*)cnode);

				if (buddy &&
						purple_presence_is_idle(purple_buddy_get_presence(buddy)))
					pidgin_blist_update_contact(list, PURPLE_BLIST_NODE(buddy));
			}
		}
	}

	/* keep on going */
	return TRUE;
}

static void pidgin_blist_hide_node(PurpleBuddyList *list, PurpleBlistNode *node, gboolean update)
{
	PidginBlistNode *gtknode = g_object_get_data(G_OBJECT(node), UI_DATA);
	GtkTreeIter iter;

	if (!gtknode || !gtknode->row || !gtkblist)
		return;

	if(gtkblist->selected_node == node)
		gtkblist->selected_node = NULL;
	if (get_iter_from_node(node, &iter)) {
		gtk_tree_store_remove(gtkblist->treemodel, &iter);
		if(update && (PURPLE_IS_CONTACT(node) ||
			PURPLE_IS_BUDDY(node) || PURPLE_IS_CHAT(node))) {
			pidgin_blist_update(list, node->parent);
		}
	}
	gtk_tree_row_reference_free(gtknode->row);
	gtknode->row = NULL;
}

static void
conversation_updated_cb(PurpleConversation *conv, PurpleConversationUpdateType type,
                        PidginBuddyList *gtkblist)
{
	PurpleAccount *account = purple_conversation_get_account(conv);

	if (type != PURPLE_CONVERSATION_UPDATE_UNSEEN)
		return;

	if(account != NULL && purple_conversation_get_name(conv) != NULL) {
		PurpleBuddy *buddy = purple_blist_find_buddy(account, purple_conversation_get_name(conv));
		if(buddy != NULL)
			pidgin_blist_update_buddy(NULL, PURPLE_BLIST_NODE(buddy), TRUE);
	}
}

static void
conversation_deleting_cb(PurpleConversation *conv, PidginBuddyList *gtkblist)
{
	conversation_updated_cb(conv, PURPLE_CONVERSATION_UPDATE_UNSEEN, gtkblist);
}

static void
conversation_deleted_update_ui_cb(PurpleConversation *conv, PidginBlistNode *ui)
{
	if (ui->conv.conv != conv)
		return;
	ui->conv.conv = NULL;
	ui->conv.flags = 0;
}

static void
written_msg_update_ui_cb(PurpleConversation *conv, PurpleMessage *msg, PurpleBlistNode *node)
{
	PidginBlistNode *ui = g_object_get_data(G_OBJECT(node), UI_DATA);

	if (ui->conv.conv != conv)
		return;

	if (!pidgin_conv_is_hidden(PIDGIN_CONVERSATION(conv)))
		return;

	if (!(purple_message_get_flags(msg) & (PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_RECV)))
		return;

	ui->conv.flags |= PIDGIN_BLIST_NODE_HAS_PENDING_MESSAGE;
	if (PURPLE_IS_CHAT_CONVERSATION(conv) && (purple_message_get_flags(msg) & PURPLE_MESSAGE_NICK))
		ui->conv.flags |= PIDGIN_BLIST_CHAT_HAS_PENDING_MESSAGE_WITH_NICK;

	pidgin_blist_update(purple_blist_get_default(), node);
}

static void
displayed_msg_update_ui_cb(PidginConversation *gtkconv, PurpleBlistNode *node)
{
	PidginBlistNode *ui = g_object_get_data(G_OBJECT(node), UI_DATA);
	if (ui->conv.conv != gtkconv->active_conv)
		return;
	ui->conv.flags &= ~(PIDGIN_BLIST_NODE_HAS_PENDING_MESSAGE |
	                    PIDGIN_BLIST_CHAT_HAS_PENDING_MESSAGE_WITH_NICK);
	pidgin_blist_update(purple_blist_get_default(), node);
}

static void
conversation_created_cb(PurpleConversation *conv, PidginBuddyList *gtkblist)
{
	PurpleAccount *account = purple_conversation_get_account(conv);

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		GSList *buddies = purple_blist_find_buddies(account, purple_conversation_get_name(conv));
		while (buddies) {
			PurpleBlistNode *buddy = buddies->data;
			PidginBlistNode *ui = g_object_get_data(G_OBJECT(buddy), UI_DATA);
			buddies = g_slist_delete_link(buddies, buddies);
			if (!ui)
				continue;
			ui->conv.conv = conv;
			ui->conv.flags = 0;
			purple_signal_connect(purple_conversations_get_handle(), "deleting-conversation",
					ui, PURPLE_CALLBACK(conversation_deleted_update_ui_cb), ui);
			purple_signal_connect(purple_conversations_get_handle(), "wrote-im-msg",
					ui, PURPLE_CALLBACK(written_msg_update_ui_cb), buddy);
			purple_signal_connect(pidgin_conversations_get_handle(), "conversation-displayed",
					ui, PURPLE_CALLBACK(displayed_msg_update_ui_cb), buddy);
		}
	} else if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
		PurpleChat *chat = purple_blist_find_chat(account, purple_conversation_get_name(conv));
		PidginBlistNode *ui;
		if (!chat)
			return;
		ui = g_object_get_data(G_OBJECT(chat), UI_DATA);
		if (!ui)
			return;
		ui->conv.conv = conv;
		ui->conv.flags = 0;
		purple_signal_connect(purple_conversations_get_handle(), "deleting-conversation",
				ui, PURPLE_CALLBACK(conversation_deleted_update_ui_cb), ui);
		purple_signal_connect(purple_conversations_get_handle(), "wrote-chat-msg",
				ui, PURPLE_CALLBACK(written_msg_update_ui_cb), chat);
		purple_signal_connect(pidgin_conversations_get_handle(), "conversation-displayed",
				ui, PURPLE_CALLBACK(displayed_msg_update_ui_cb), chat);
	}
}

/**********************************************************************************
 * Public API Functions                                                           *
 **********************************************************************************/
static void
pidgin_blist_node_free(PidginBlistNode *node) {
	if(node->recent_signonoff_timer > 0) {
		g_source_remove(node->recent_signonoff_timer);
	}

	purple_signals_disconnect_by_handle(node);

	g_free(node);
}

static void
pidgin_blist_new_node(PurpleBuddyList *list, PurpleBlistNode *node) {
	PidginBlistNode *pidgin_node = NULL;

	pidgin_node = g_new0(PidginBlistNode, 1);

	g_object_set_data_full(G_OBJECT(node), UI_DATA, pidgin_node,
	                       (GDestroyNotify)pidgin_blist_node_free);
}

gboolean
pidgin_blist_node_is_contact_expanded(PurpleBlistNode *node) {
	PidginBlistNode *pidgin_node = NULL;

	if (PURPLE_IS_BUDDY(node)) {
		node = node->parent;
		if (node == NULL)
			return FALSE;
	}

	g_return_val_if_fail(PURPLE_IS_CONTACT(node), FALSE);

	pidgin_node = g_object_get_data(G_OBJECT(node), UI_DATA);

	return pidgin_node->contact_expanded;
}

enum {
	DRAG_BUDDY,
	DRAG_ROW,
	DRAG_VCARD,
	DRAG_TEXT,
	DRAG_URI,
	NUM_TARGETS
};

void pidgin_blist_setup_sort_methods()
{
	const char *id;

	pidgin_blist_sort_method_reg("none", _("Manually"), sort_method_none);
	pidgin_blist_sort_method_reg("alphabetical", _("Alphabetically"), sort_method_alphabetical);
	pidgin_blist_sort_method_reg("status", _("By status"), sort_method_status);
	pidgin_blist_sort_method_reg("log_size", _("By recent log activity"), sort_method_log_activity);

	id = purple_prefs_get_string(PIDGIN_PREFS_ROOT "/blist/sort_type");
	if (id == NULL) {
		purple_debug_warning("gtkblist", "Sort method was NULL, resetting to alphabetical\n");
		id = "alphabetical";
	}
	pidgin_blist_sort_method_set(id);
}

static void _prefs_change_redo_list(const char *name, PurplePrefType type,
                                    gconstpointer val, gpointer data)
{
	GtkTreeSelection *sel;
	GtkTreeIter iter;
	PurpleBlistNode *node = NULL;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkblist->treeview));
	if (gtk_tree_selection_get_selected(sel, NULL, &iter))
	{
		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);
	}

	redo_buddy_list(purple_blist_get_default(), FALSE, FALSE);
	gtk_tree_view_columns_autosize(GTK_TREE_VIEW(gtkblist->treeview));

	if (node)
	{
		PidginBlistNode *gtknode;
		GtkTreePath *path;

		gtknode = g_object_get_data(G_OBJECT(node), UI_DATA);
		if (gtknode && gtknode->row)
		{
			path = gtk_tree_row_reference_get_path(gtknode->row);
			gtk_tree_selection_select_path(sel, path);
			gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(gtkblist->treeview), path, NULL, FALSE, 0, 0);
			gtk_tree_path_free(path);
		}
	}
}

static void _prefs_change_sort_method(const char *pref_name, PurplePrefType type,
									  gconstpointer val, gpointer data)
{
	if(purple_strequal(pref_name, PIDGIN_PREFS_ROOT "/blist/sort_type"))
		pidgin_blist_sort_method_set(val);
}

static gboolean pidgin_blist_select_notebook_page_cb(gpointer user_data)
{
	PidginBuddyList *gtkblist = (PidginBuddyList *)user_data;
	int errors = 0;
	GList *list = NULL;
	PidginBuddyListPrivate *priv;

	priv = pidgin_buddy_list_get_instance_private(gtkblist);

	priv->select_notebook_page_timeout = 0;

	/* this is far too ugly thanks to me not wanting to fix #3989 properly right now */
	if (priv->error_scrollbook != NULL) {
		PidginScrollBook *scroll_book = PIDGIN_SCROLL_BOOK(priv->error_scrollbook);
		GtkWidget *notebook = pidgin_scroll_book_get_notebook(scroll_book);
		errors = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));
	}
	if ((list = purple_accounts_get_all_active()) != NULL || errors) {
		gtk_notebook_set_current_page(GTK_NOTEBOOK(gtkblist->notebook), 1);
		g_list_free(list);
	} else
		gtk_notebook_set_current_page(GTK_NOTEBOOK(gtkblist->notebook), 0);

	priv->select_notebook_page_timeout = 0;
	return FALSE;
}

static void pidgin_blist_select_notebook_page(PidginBuddyList *gtkblist)
{
	PidginBuddyListPrivate *priv =
	        pidgin_buddy_list_get_instance_private(gtkblist);
	priv->select_notebook_page_timeout = g_timeout_add(0,
		pidgin_blist_select_notebook_page_cb, gtkblist);
}

static void account_modified(PurpleAccount *account, PidginBuddyList *gtkblist)
{
	if (!gtkblist)
		return;

	pidgin_blist_select_notebook_page(gtkblist);
}

static void
account_status_changed(PurpleAccount *account, PurpleStatus *old,
					   PurpleStatus *new, PidginBuddyList *gtkblist)
{
	if (!gtkblist)
		return;

	account_modified(account, gtkblist);
}

static void
reset_headline(PidginBuddyList *gtkblist)
{
	gtkblist->headline_callback = NULL;
	gtkblist->headline_data = NULL;
	gtkblist->headline_destroy = NULL;

	gtk_window_set_urgency_hint(GTK_WINDOW(gtkblist->window), FALSE);
}

static gboolean
headline_click_callback(gpointer unused)
{
	if (gtkblist->headline_callback)
		((GSourceFunc) gtkblist->headline_callback)(gtkblist->headline_data);
	reset_headline(gtkblist);
	return FALSE;
}

static gboolean
headline_response_cb(GtkInfoBar *infobar, int resp, PidginBuddyList *gtkblist)
{
	gtk_widget_hide(gtkblist->headline);

	if (resp == GTK_RESPONSE_OK) {
		if (gtkblist->headline_callback)
			g_idle_add(headline_click_callback, NULL);
		else {
			if (gtkblist->headline_destroy)
				gtkblist->headline_destroy(gtkblist->headline_data);
			reset_headline(gtkblist);
		}
	} else {
		if (gtkblist->headline_destroy)
			gtkblist->headline_destroy(gtkblist->headline_data);
		reset_headline(gtkblist);
	}

	return FALSE;
}

static void
headline_realize_cb(GtkWidget *widget, gpointer data)
{
	GdkWindow *window = gtk_widget_get_window(widget);
	GdkDisplay *display = gdk_window_get_display(window);
	GdkCursor *hand_cursor = gdk_cursor_new_for_display(display, GDK_HAND2);
	gdk_window_set_cursor(window, hand_cursor);
	g_object_unref(hand_cursor);
}

static gboolean
headline_press_cb(GtkWidget *widget, GdkEventButton *event, GtkInfoBar *infobar)
{
	gtk_info_bar_response(infobar, GTK_RESPONSE_OK);
	return TRUE;
}

/***********************************/
/* Connection error handling stuff */
/***********************************/

#define OBJECT_DATA_KEY_ACCOUNT "account"
#define DO_NOT_CLEAR_ERROR "do-not-clear-error"

static gboolean
find_account_widget(GObject *widget,
                    PurpleAccount *account)
{
	if (g_object_get_data(widget, OBJECT_DATA_KEY_ACCOUNT) == account)
		return 0; /* found */
	else
		return 1;
}

static void
pack_protocol_icon_start(GtkWidget *box,
                     PurpleAccount *account)
{
	GdkPixbuf *pixbuf;
	GtkWidget *image;

	pixbuf = pidgin_create_protocol_icon(account, PIDGIN_PROTOCOL_ICON_SMALL);
	if (pixbuf != NULL) {
		image = gtk_image_new_from_pixbuf(pixbuf);
		g_object_unref(pixbuf);

		gtk_box_pack_start(GTK_BOX(box), image, FALSE, FALSE, 0);
	}
}

static void
add_error_dialog(PidginBuddyList *gtkblist,
                 GtkWidget *dialog)
{
	PidginBuddyListPrivate *priv =
	        pidgin_buddy_list_get_instance_private(gtkblist);
	gtk_container_add(GTK_CONTAINER(priv->error_scrollbook), dialog);
}

static GtkWidget *
find_child_widget_by_account(GtkContainer *container,
                             PurpleAccount *account)
{
	GList *l = NULL;
	GList *children = NULL;
	GtkWidget *ret = NULL;
	/* XXX: Workaround for the currently incomplete implementation of PidginScrollBook */
	if(PIDGIN_IS_SCROLL_BOOK(container)) {
		PidginScrollBook *scroll_book = PIDGIN_SCROLL_BOOK(container);
		GtkWidget *notebook = pidgin_scroll_book_get_notebook(scroll_book);
		container = GTK_CONTAINER(notebook);
	}
	children = gtk_container_get_children(container);
	l = g_list_find_custom(children, account, (GCompareFunc) find_account_widget);
	if (l)
		ret = GTK_WIDGET(l->data);
	g_list_free(children);
	return ret;
}

static void
remove_child_widget_by_account(GtkContainer *container,
                               PurpleAccount *account)
{
	GtkWidget *widget = find_child_widget_by_account(container, account);
	if(widget) {
		/* Since we are destroying the widget in response to a change in
		 * error, we should not clear the error.
		 */
		g_object_set_data(G_OBJECT(widget), DO_NOT_CLEAR_ERROR,
			GINT_TO_POINTER(TRUE));
		gtk_widget_destroy(widget);
	}
}

/* Generic error buttons */

static void
generic_account_connect_cb(G_GNUC_UNUSED PidginMiniDialog *mini_dialog,
                           G_GNUC_UNUSED GtkButton *button,
                           gpointer user_data)
{
	PurpleAccount *account = user_data;
	purple_account_connect(account);
}

static void
generic_error_modify_cb(G_GNUC_UNUSED PidginMiniDialog *mini_dialog,
                        G_GNUC_UNUSED GtkButton *button,
                        gpointer user_data)
{
	PurpleAccount *account = user_data;
	purple_account_clear_current_error(account);
	pidgin_account_dialog_show(PIDGIN_MODIFY_ACCOUNT_DIALOG, account);
}

static void
generic_error_enable_cb(G_GNUC_UNUSED PidginMiniDialog *mini_dialog,
                        G_GNUC_UNUSED GtkButton *button,
                        gpointer user_data)
{
	PurpleAccount *account = user_data;
	purple_account_clear_current_error(account);
	purple_account_set_enabled(account, purple_core_get_ui(), TRUE);
}

static void
generic_error_destroy_cb(GtkWidget *dialog,
                         PurpleAccount *account)
{
	/* If the error dialog is being destroyed in response to the
	 * account-error-changed signal, we don't want to clear the current
	 * error.
	 */
	if (g_object_get_data(G_OBJECT(dialog), DO_NOT_CLEAR_ERROR) == NULL)
		purple_account_clear_current_error(account);
}

#define SSL_FAQ_URI "https://developer.pidgin.im/wiki/FAQssl"

static void
ssl_faq_clicked_cb(PidginMiniDialog *mini_dialog,
                   GtkButton *button,
                   gpointer ignored)
{
	purple_notify_uri(NULL, SSL_FAQ_URI);
}

static void
add_generic_error_dialog(PurpleAccount *account,
                         const PurpleConnectionErrorInfo *err)
{
	GtkWidget *mini_dialog;
	const char *username = purple_account_get_username(account);
	gboolean enabled =
		purple_account_get_enabled(account, purple_core_get_ui());
	char *primary;

	if (enabled)
		primary = g_strdup_printf(_("%s disconnected"), username);
	else
		primary = g_strdup_printf(_("%s disabled"), username);

	mini_dialog = pidgin_mini_dialog_new_with_buttons(
		primary, err->description, "dialog-error", account,
		enabled ? _("Reconnect") : _("Re-enable"),
		enabled ? generic_account_connect_cb : generic_error_enable_cb,
		_("Modify Account"), generic_error_modify_cb,
		NULL);

	g_free(primary);

	g_object_set_data(G_OBJECT(mini_dialog), OBJECT_DATA_KEY_ACCOUNT,
		account);

	 if(err->type == PURPLE_CONNECTION_ERROR_NO_SSL_SUPPORT)
		pidgin_mini_dialog_add_non_closing_button(PIDGIN_MINI_DIALOG(mini_dialog),
				_("SSL FAQs"), ssl_faq_clicked_cb, NULL);

	g_signal_connect_after(mini_dialog, "destroy",
		(GCallback)generic_error_destroy_cb,
		account);

	add_error_dialog(gtkblist, mini_dialog);
}

static void
remove_generic_error_dialog(PurpleAccount *account)
{
	PidginBuddyListPrivate *priv =
	        pidgin_buddy_list_get_instance_private(gtkblist);
	remove_child_widget_by_account(
		GTK_CONTAINER(priv->error_scrollbook), account);
}


static void
update_generic_error_message(PurpleAccount *account,
                             const char *description)
{
	PidginBuddyListPrivate *priv =
	        pidgin_buddy_list_get_instance_private(gtkblist);
	GtkWidget *mini_dialog = find_child_widget_by_account(
		GTK_CONTAINER(priv->error_scrollbook), account);
	pidgin_mini_dialog_set_description(PIDGIN_MINI_DIALOG(mini_dialog),
		description);
}


/* Notifications about accounts which were disconnected with
 * PURPLE_CONNECTION_ERROR_NAME_IN_USE
 */

typedef void (*AccountFunction)(PurpleAccount *);

static void
elsewhere_foreach_account(PidginMiniDialog *mini_dialog,
                          AccountFunction f)
{
	PurpleAccount *account;
	GList *labels = gtk_container_get_children(
		GTK_CONTAINER(mini_dialog->contents));
	GList *l;

	for (l = labels; l; l = l->next) {
		account = g_object_get_data(G_OBJECT(l->data), OBJECT_DATA_KEY_ACCOUNT);
		if (account)
			f(account);
		else
			purple_debug_warning("gtkblist", "mini_dialog's child "
				"didn't have an account stored in it!");
	}
	g_list_free(labels);
}

static void
enable_account(PurpleAccount *account)
{
	purple_account_set_enabled(account, purple_core_get_ui(), TRUE);
}

static void
reconnect_elsewhere_accounts(PidginMiniDialog *mini_dialog,
                             GtkButton *button,
                             gpointer unused)
{
	elsewhere_foreach_account(mini_dialog, enable_account);
}

static void
clear_elsewhere_errors(PidginMiniDialog *mini_dialog,
                       gpointer unused)
{
	elsewhere_foreach_account(mini_dialog, purple_account_clear_current_error);
}

static void
ensure_signed_on_elsewhere_minidialog(PidginBuddyList *gtkblist)
{
	PidginBuddyListPrivate *priv =
	        pidgin_buddy_list_get_instance_private(gtkblist);
	PidginMiniDialog *mini_dialog;

	if(priv->signed_on_elsewhere)
		return;

	mini_dialog = priv->signed_on_elsewhere =
		pidgin_mini_dialog_new(_("Welcome back!"), NULL, "pidgin-disconnect");

	pidgin_mini_dialog_add_button(mini_dialog, _("Re-enable"),
		reconnect_elsewhere_accounts, NULL);

	/* Make dismissing the dialog clear the errors.  The "destroy" signal
	 * does not appear to fire at quit, which is fortunate!
	 */
	g_signal_connect(G_OBJECT(mini_dialog), "destroy",
		(GCallback) clear_elsewhere_errors, NULL);

	add_error_dialog(gtkblist, GTK_WIDGET(mini_dialog));

	/* Set priv->signed_on_elsewhere to NULL when the dialog is destroyed */
	g_signal_connect(G_OBJECT(mini_dialog), "destroy",
		(GCallback) gtk_widget_destroyed, &(priv->signed_on_elsewhere));
}

static void
update_signed_on_elsewhere_minidialog_title(void)
{
	PidginBuddyListPrivate *priv =
	        pidgin_buddy_list_get_instance_private(gtkblist);
	PidginMiniDialog *mini_dialog = priv->signed_on_elsewhere;
	guint accounts;
	char *title;

	if (mini_dialog == NULL)
		return;

	accounts = pidgin_mini_dialog_get_num_children(mini_dialog);
	if (accounts == 0) {
		gtk_widget_destroy(GTK_WIDGET(mini_dialog));
		return;
	}

	title = g_strdup_printf(
		ngettext("%d account was disabled because you signed on from another location:",
			 "%d accounts were disabled because you signed on from another location:",
			 accounts),
		accounts);
	pidgin_mini_dialog_set_description(mini_dialog, title);
	g_free(title);
}

static GtkWidget *
create_account_label(PurpleAccount *account)
{
	GtkWidget *hbox, *label;
	const char *username = purple_account_get_username(account);
	char *markup;
	char *description;

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	g_object_set_data(G_OBJECT(hbox), OBJECT_DATA_KEY_ACCOUNT, account);

	pack_protocol_icon_start(hbox, account);

	label = gtk_label_new(NULL);
	markup = g_strdup_printf("<span size=\"smaller\">%s</span>", username);
	gtk_label_set_markup(GTK_LABEL(label), markup);
	g_free(markup);
	gtk_label_set_xalign(GTK_LABEL(label), 0);
	gtk_label_set_yalign(GTK_LABEL(label), 0);
	g_object_set(G_OBJECT(label), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	description = purple_account_get_current_error(account)->description;
	if (description != NULL && *description != '\0')
		gtk_widget_set_tooltip_text(label, description);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

	return hbox;
}

static void
add_to_signed_on_elsewhere(PurpleAccount *account)
{
	PidginBuddyListPrivate *priv =
	        pidgin_buddy_list_get_instance_private(gtkblist);
	PidginMiniDialog *mini_dialog;
	GtkWidget *account_label;

	ensure_signed_on_elsewhere_minidialog(gtkblist);
	mini_dialog = priv->signed_on_elsewhere;

	if(find_child_widget_by_account(GTK_CONTAINER(mini_dialog->contents), account))
		return;

	account_label = create_account_label(account);
	gtk_box_pack_start(mini_dialog->contents, account_label, FALSE, FALSE, 0);
	gtk_widget_show_all(account_label);

	update_signed_on_elsewhere_minidialog_title();
}

static void
remove_from_signed_on_elsewhere(PurpleAccount *account)
{
	PidginBuddyListPrivate *priv =
	        pidgin_buddy_list_get_instance_private(gtkblist);
	PidginMiniDialog *mini_dialog = priv->signed_on_elsewhere;
	if(mini_dialog == NULL)
		return;

	remove_child_widget_by_account(GTK_CONTAINER(mini_dialog->contents), account);

	update_signed_on_elsewhere_minidialog_title();
}


static void
update_signed_on_elsewhere_tooltip(PurpleAccount *account,
                                   const char *description)
{
	PidginBuddyListPrivate *priv =
	        pidgin_buddy_list_get_instance_private(gtkblist);
	GtkContainer *c = GTK_CONTAINER(priv->signed_on_elsewhere->contents);
	GtkWidget *label = find_child_widget_by_account(c, account);
	gtk_widget_set_tooltip_text(label, description);
}


/* Call appropriate error notification code based on error types */
static void
update_account_error_state(PurpleAccount *account,
                           const PurpleConnectionErrorInfo *old,
                           const PurpleConnectionErrorInfo *new,
                           PidginBuddyList *gtkblist)
{
	gboolean descriptions_differ;
	const char *desc;

	if (old == NULL && new == NULL)
		return;

	if (new != NULL)
		pidgin_blist_select_notebook_page(gtkblist);

	if (old != NULL && new == NULL) {
		if(old->type == PURPLE_CONNECTION_ERROR_NAME_IN_USE)
			remove_from_signed_on_elsewhere(account);
		else
			remove_generic_error_dialog(account);
		return;
	}

	if (old == NULL && new != NULL) {
		if(new->type == PURPLE_CONNECTION_ERROR_NAME_IN_USE)
			add_to_signed_on_elsewhere(account);
		else
			add_generic_error_dialog(account, new);
		return;
	}

	/* else, new and old are both non-NULL */

	descriptions_differ = !purple_strequal(old->description, new->description);
	desc = new->description;

	switch (new->type) {
	case PURPLE_CONNECTION_ERROR_NAME_IN_USE:
		if (old->type == PURPLE_CONNECTION_ERROR_NAME_IN_USE
		    && descriptions_differ) {
			update_signed_on_elsewhere_tooltip(account, desc);
		} else {
			remove_generic_error_dialog(account);
			add_to_signed_on_elsewhere(account);
		}
		break;
	default:
		if (old->type == PURPLE_CONNECTION_ERROR_NAME_IN_USE) {
			remove_from_signed_on_elsewhere(account);
			add_generic_error_dialog(account, new);
		} else if (descriptions_differ) {
			update_generic_error_message(account, desc);
		}
		break;
	}
}

/* In case accounts are loaded before the blist (which they currently are),
 * let's call update_account_error_state ourselves on every account's current
 * state when the blist starts.
 */
static void
show_initial_account_errors(PidginBuddyList *gtkblist)
{
	GList *l = purple_accounts_get_all();
	PurpleAccount *account;
	const PurpleConnectionErrorInfo *err;

	for (; l; l = l->next)
	{
		account = l->data;
		err = purple_account_get_current_error(account);

		update_account_error_state(account, NULL, err, gtkblist);
	}
}

/* This assumes there are not things like groupless buddies or multi-leveled groups.
 * I'm sure other things in this code assumes that also.
 */
static void
treeview_style_set(GtkWidget *widget,
		    gpointer data)
{
	PurpleBuddyList *list = data;
	PurpleBlistNode *node = purple_blist_get_root(list);
	while (node) {
		pidgin_blist_update_group(list, node);
		node = node->next;
	}
}

/******************************************/
/* End of connection error handling stuff */
/******************************************/

static int
blist_focus_cb(GtkWidget *widget, GdkEventFocus *event, PidginBuddyList *gtkblist)
{
	if(event->in) {
		gtk_blist_focused = TRUE;
		gtk_window_set_urgency_hint(GTK_WINDOW(gtkblist->window), FALSE);
	} else {
		gtk_blist_focused = FALSE;
	}
	return 0;
}

#if 0
static GtkWidget *
kiosk_page()
{
	GtkWidget *ret = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *bbox;
	GtkWidget *button;

	label = gtk_label_new(NULL);
	gtk_box_pack_start(GTK_BOX(ret), label, TRUE, TRUE, 0);

	label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label), _("<b>Username:</b>"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_box_pack_start(GTK_BOX(ret), label, FALSE, FALSE, 0);
	entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(ret), entry, FALSE, FALSE, 0);

	label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label), _("<b>Password:</b>"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_box_pack_start(GTK_BOX(ret), label, FALSE, FALSE, 0);
	entry = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
	gtk_box_pack_start(GTK_BOX(ret), entry, FALSE, FALSE, 0);

	label = gtk_label_new(" ");
	gtk_box_pack_start(GTK_BOX(ret), label, FALSE, FALSE, 0);

	bbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
	button = gtk_button_new_with_mnemonic(_("_Login"));
	gtk_box_pack_start(GTK_BOX(ret), bbox, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(bbox), button);


	label = gtk_label_new(NULL);
	gtk_box_pack_start(GTK_BOX(ret), label, TRUE, TRUE, 0);

	gtk_container_set_border_width(GTK_CONTAINER(ret), 12);

	gtk_widget_show_all(ret);
	return ret;
}
#endif

/* builds the blist layout according to to the current theme */
static void
pidgin_blist_build_layout(PurpleBuddyList *list)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *rend;

	column = gtkblist->text_column;

	gtk_tree_view_column_clear(column);

	/* group */
	rend = pidgin_cell_renderer_expander_new();
	gtk_tree_view_column_pack_start(column, rend, FALSE);
	gtk_tree_view_column_set_attributes(column, rend,
					    "visible", GROUP_EXPANDER_VISIBLE_COLUMN,
					    "is-expanded", GROUP_EXPANDER_COLUMN,
					    "sensitive", GROUP_EXPANDER_COLUMN,
					    NULL);

	/* contact */
	rend = pidgin_cell_renderer_expander_new();
	gtk_tree_view_column_pack_start(column, rend, FALSE);
	gtk_tree_view_column_set_attributes(column, rend,
					    "visible", CONTACT_EXPANDER_VISIBLE_COLUMN,
					    "is-expanded", CONTACT_EXPANDER_COLUMN,
					    "sensitive", CONTACT_EXPANDER_COLUMN,
					    NULL);

	/* status icons */
	rend = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, rend, FALSE);
	gtk_tree_view_column_set_attributes(column, rend,
					    "pixbuf", STATUS_ICON_COLUMN,
					    "visible", STATUS_ICON_VISIBLE_COLUMN,
					    NULL);
	g_object_set(rend, "xalign", 0.0, "xpad", 6, "ypad", 0, NULL);

	/* name */
	gtkblist->text_rend = rend = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, rend, TRUE);
	gtk_tree_view_column_set_attributes(column, rend,
	                                    "markup", NAME_COLUMN,
	                                    NULL);
	g_signal_connect(G_OBJECT(rend), "editing-started",
	                 G_CALLBACK(gtk_blist_renderer_editing_started_cb), list);
	g_signal_connect(G_OBJECT(rend), "editing-canceled",
	                 G_CALLBACK(gtk_blist_renderer_editing_cancelled_cb), list);
	g_signal_connect(G_OBJECT(rend), "edited",
	                 G_CALLBACK(gtk_blist_renderer_edited_cb), list);
	g_object_set(rend, "ypad", 0, "yalign", 0.5, NULL);
	g_object_set(rend, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	/* idle */
	rend = gtk_cell_renderer_text_new();
	g_object_set(rend, "xalign", 1.0, "ypad", 0, NULL);
	gtk_tree_view_column_pack_start(column, rend, FALSE);
	gtk_tree_view_column_set_attributes(column, rend,
	                                    "markup", IDLE_COLUMN,
	                                    "visible", IDLE_VISIBLE_COLUMN,
	                                    NULL);

	/* emblem */
	rend = gtk_cell_renderer_pixbuf_new();
	g_object_set(rend, "xalign", 1.0, "yalign", 0.5, "ypad", 0, "xpad", 3,
	             NULL);
	gtk_tree_view_column_pack_start(column, rend, FALSE);
	gtk_tree_view_column_set_attributes(column, rend,
	                                    "pixbuf", EMBLEM_COLUMN,
	                                    "visible", EMBLEM_VISIBLE_COLUMN, NULL);

	/* protocol icon */
	rend = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, rend, FALSE);
	gtk_tree_view_column_set_attributes(column, rend,
	                                    "pixbuf", PROTOCOL_ICON_COLUMN,
	                                    "visible", PROTOCOL_ICON_VISIBLE_COLUMN,
	                                    NULL);
	g_object_set(rend, "xalign", 0.0, "xpad", 3, "ypad", 0, NULL);

	/* buddy icon */
	rend = gtk_cell_renderer_pixbuf_new();
	g_object_set(rend, "xalign", 1.0, "ypad", 0, NULL);
	gtk_tree_view_column_pack_start(column, rend, FALSE);
	gtk_tree_view_column_set_attributes(column, rend,
	                                    "pixbuf", BUDDY_ICON_COLUMN,
	                                    "visible", BUDDY_ICON_VISIBLE_COLUMN,
	                                    NULL);
}

static gboolean
pidgin_blist_search_equal_func(GtkTreeModel *model, gint column,
			const gchar *key, GtkTreeIter *iter, gpointer data)
{
	PurpleBlistNode *node = NULL;
	gboolean res = TRUE;
	const char *compare = NULL;

	if (!pidgin_tree_view_search_equal_func(model, column, key, iter, data))
		return FALSE;

	/* If the search string does not match the displayed label, then look
	 * at the alternate labels for the nodes and search in them. Currently,
	 * alternate labels that make sense are usernames/email addresses for
	 * buddies (but only for the ones who don't have a local alias).
	 */

	gtk_tree_model_get(model, iter, NODE_COLUMN, &node, -1);
	if (!node)
		return TRUE;

	compare = NULL;
	if (PURPLE_IS_CONTACT(node)) {
		PurpleBuddy *b = purple_contact_get_priority_buddy(PURPLE_CONTACT(node));
		if (!purple_buddy_get_local_alias(b))
			compare = purple_buddy_get_name(b);
	} else if (PURPLE_IS_BUDDY(node)) {
		if (!purple_buddy_get_local_alias(PURPLE_BUDDY(node)))
			compare = purple_buddy_get_name(PURPLE_BUDDY(node));
	}

	if (compare) {
		char *tmp, *enteredstring;
		tmp = g_utf8_normalize(key, -1, G_NORMALIZE_DEFAULT);
		enteredstring = g_utf8_casefold(tmp, -1);
		g_free(tmp);

		if (g_str_has_prefix(compare, enteredstring)) {
			res = FALSE;
		}
		g_free(enteredstring);
	}

	return res;
}

static void pidgin_blist_show(PurpleBuddyList *list)
{
	PidginBuddyListPrivate *priv;
	void *handle;
	GtkTreeViewColumn *column;
	GtkWidget *sep;
	GtkWidget *infobar;
	GtkWidget *content_area;
	GtkWidget *label;
	GtkWidget *close;
	gchar *text;
	GtkTreeSelection *selection;
	GtkTargetEntry dte[] = {{"PURPLE_BLIST_NODE", GTK_TARGET_SAME_APP, DRAG_ROW},
				{"application/x-im-contact", 0, DRAG_BUDDY},
				{"text/x-vcard", 0, DRAG_VCARD },
				{"text/uri-list", 0, DRAG_URI},
				{"text/plain", 0, DRAG_TEXT}};
	GtkTargetEntry ste[] = {{"PURPLE_BLIST_NODE", GTK_TARGET_SAME_APP, DRAG_ROW},
				{"application/x-im-contact", 0, DRAG_BUDDY},
				{"text/x-vcard", 0, DRAG_VCARD }};
	if (gtkblist && gtkblist->window) {
		purple_blist_set_visible(purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/list_visible"));
		return;
	}

	gtkblist = PIDGIN_BUDDY_LIST(list);
	priv = pidgin_buddy_list_get_instance_private(gtkblist);

	gtkblist->empty_avatar = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, 32, 32);
	gdk_pixbuf_fill(gtkblist->empty_avatar, 0x00000000);

	gtkblist->window = pidgin_contact_list_new();
	g_signal_connect(G_OBJECT(gtkblist->window), "focus-in-event",
			 G_CALLBACK(blist_focus_cb), gtkblist);
	g_signal_connect(G_OBJECT(gtkblist->window), "focus-out-event",
			 G_CALLBACK(blist_focus_cb), gtkblist);

	/* the main vbox is already packed and shown via glade, we just need a
	 * reference to it to pack the rest of our widgets here.
	 */
	gtkblist->main_vbox = pidgin_contact_list_get_vbox(PIDGIN_CONTACT_LIST(gtkblist->window));

	g_signal_connect(G_OBJECT(gtkblist->window), "delete_event", G_CALLBACK(gtk_blist_delete_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->window), "hide",
	                 G_CALLBACK(gtk_blist_hide_cb), gtkblist);
	g_signal_connect(G_OBJECT(gtkblist->window), "show",
	                 G_CALLBACK(gtk_blist_show_cb), gtkblist);
	g_signal_connect(G_OBJECT(gtkblist->window), "size-allocate",
			G_CALLBACK(gtk_blist_size_allocate_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->window), "visibility_notify_event", G_CALLBACK(gtk_blist_visibility_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->window), "window_state_event", G_CALLBACK(gtk_blist_window_state_cb), NULL);
	gtk_widget_add_events(gtkblist->window, GDK_VISIBILITY_NOTIFY_MASK);

	/****************************** Notebook *************************************/
	gtkblist->notebook = gtk_notebook_new();
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(gtkblist->notebook), FALSE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(gtkblist->notebook), FALSE);
	gtk_box_pack_start(GTK_BOX(gtkblist->main_vbox), gtkblist->notebook, TRUE, TRUE, 0);

#if 0
	gtk_notebook_append_page(GTK_NOTEBOOK(gtkblist->notebook), kiosk_page(), NULL);
#endif

	/* Translators: Please maintain the use of ⇦ or ⇨ to refer to menu hierarchy */
	text = g_strdup_printf(_("<span weight='bold' size='larger'>Welcome to %s!</span>\n\n"

					       "You have no accounts enabled. Enable your IM accounts from the "
					       "<b>Accounts</b> window at <b>Accounts⇨Manage Accounts</b>. Once you "
					       "enable accounts, you'll be able to sign on, set your status, "
					       "and talk to your friends."), PIDGIN_NAME);
	label = gtk_label_new(NULL);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_yalign(GTK_LABEL(label), 0.2);
	gtk_label_set_markup(GTK_LABEL(label), text);
	g_free(text);
	gtk_notebook_append_page(GTK_NOTEBOOK(gtkblist->notebook),label, NULL);
	gtkblist->vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_notebook_append_page(GTK_NOTEBOOK(gtkblist->notebook), gtkblist->vbox, NULL);
	gtk_widget_show_all(gtkblist->notebook);
	pidgin_blist_select_notebook_page(gtkblist);

	/****************************** Headline **********************************/

	gtkblist->headline = gtk_event_box_new();
	gtk_box_pack_start(GTK_BOX(gtkblist->vbox), gtkblist->headline,
	                   FALSE, FALSE, 0);
	infobar = gtk_info_bar_new();
	gtk_container_add(GTK_CONTAINER(gtkblist->headline), infobar);
	gtk_info_bar_set_default_response(GTK_INFO_BAR(infobar), GTK_RESPONSE_OK);
	gtk_info_bar_set_message_type(GTK_INFO_BAR(infobar), GTK_MESSAGE_INFO);

	content_area = gtk_info_bar_get_content_area(GTK_INFO_BAR(infobar));
	gtkblist->headline_image = gtk_image_new_from_pixbuf(NULL);
	gtk_widget_set_halign(gtkblist->headline_image, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(gtkblist->headline_image, GTK_ALIGN_CENTER);
	gtkblist->headline_label = gtk_label_new(NULL);
	gtk_label_set_line_wrap(GTK_LABEL(gtkblist->headline_label), TRUE);
	gtk_box_pack_start(GTK_BOX(content_area), gtkblist->headline_image,
	                   FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(content_area), gtkblist->headline_label,
	                   TRUE, TRUE, 0);

	close = pidgin_close_button_new();
	gtk_info_bar_add_action_widget(GTK_INFO_BAR(infobar), close,
	                               GTK_RESPONSE_CLOSE);

	g_signal_connect(infobar, "response", G_CALLBACK(headline_response_cb),
	                 gtkblist);
	g_signal_connect(infobar, "close", G_CALLBACK(gtk_info_bar_response),
	                 GINT_TO_POINTER(GTK_RESPONSE_CLOSE));
	g_signal_connect(gtkblist->headline, "realize",
	                 G_CALLBACK(headline_realize_cb), NULL);
	g_signal_connect(gtkblist->headline, "button-press-event",
	                 G_CALLBACK(headline_press_cb), infobar);

	/****************************** GtkTreeView **********************************/
	gtkblist->treemodel = gtk_tree_store_new(BLIST_COLUMNS,
						 GDK_TYPE_PIXBUF, /* Status icon */
						 G_TYPE_BOOLEAN,  /* Status icon visible */
						 G_TYPE_STRING,   /* Name */
						 G_TYPE_STRING,   /* Idle */
						 G_TYPE_BOOLEAN,  /* Idle visible */
						 GDK_TYPE_PIXBUF, /* Buddy icon */
						 G_TYPE_BOOLEAN,  /* Buddy icon visible */
						 G_TYPE_POINTER,  /* Node */
						 G_TYPE_BOOLEAN,  /* Group expander */
						 G_TYPE_BOOLEAN,  /* Group expander visible */
						 G_TYPE_BOOLEAN,  /* Contact expander */
						 G_TYPE_BOOLEAN,  /* Contact expander visible */
						 GDK_TYPE_PIXBUF, /* Emblem */
						 G_TYPE_BOOLEAN,  /* Emblem visible */
						 GDK_TYPE_PIXBUF, /* Protocol icon */
						 G_TYPE_BOOLEAN   /* Protocol visible */
						);

	gtkblist->treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(gtkblist->treemodel));

	gtk_widget_show(gtkblist->treeview);
	gtk_widget_set_name(gtkblist->treeview, "pidgin_blist_treeview");

	g_signal_connect(gtkblist->treeview,
			 "style-updated",
			 G_CALLBACK(treeview_style_set), list);
	/* Set up selection stuff */
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkblist->treeview));
	g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(pidgin_blist_selection_changed), NULL);

	/* Set up dnd */
	gtk_tree_view_enable_model_drag_source(GTK_TREE_VIEW(gtkblist->treeview),
										   GDK_BUTTON1_MASK, ste, 3,
										   GDK_ACTION_COPY);
	gtk_tree_view_enable_model_drag_dest(GTK_TREE_VIEW(gtkblist->treeview),
										 dte, 5,
										 GDK_ACTION_COPY | GDK_ACTION_MOVE);

	g_signal_connect(G_OBJECT(gtkblist->treeview), "drag-data-received", G_CALLBACK(pidgin_blist_drag_data_rcv_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "drag-data-get", G_CALLBACK(pidgin_blist_drag_data_get_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "drag-motion", G_CALLBACK(pidgin_blist_drag_motion_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "motion-notify-event", G_CALLBACK(pidgin_blist_motion_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "leave-notify-event", G_CALLBACK(pidgin_blist_leave_cb), NULL);

	/* Tooltips */
	gtk_widget_set_has_tooltip(gtkblist->treeview, TRUE);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "query-tooltip",
	                 G_CALLBACK(pidgin_blist_query_tooltip), gtkblist);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(gtkblist->treeview), FALSE);

	/* expander columns */
	column = gtk_tree_view_column_new();
	gtk_tree_view_append_column(GTK_TREE_VIEW(gtkblist->treeview), column);
	gtk_tree_view_column_set_visible(column, FALSE);
	gtk_tree_view_set_expander_column(GTK_TREE_VIEW(gtkblist->treeview), column);

	/* everything else column */
	gtkblist->text_column = gtk_tree_view_column_new ();
	gtk_tree_view_append_column(GTK_TREE_VIEW(gtkblist->treeview), gtkblist->text_column);
	pidgin_blist_build_layout(list);

	g_signal_connect(G_OBJECT(gtkblist->treeview), "row-activated",
	                 G_CALLBACK(gtk_blist_row_activated_cb), gtkblist);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "row-expanded",
	                 G_CALLBACK(gtk_blist_row_expanded_cb), gtkblist);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "row-collapsed",
	                 G_CALLBACK(gtk_blist_row_collapsed_cb), gtkblist);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "button-press-event", G_CALLBACK(gtk_blist_button_press_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "key-press-event", G_CALLBACK(gtk_blist_key_press_cb), NULL);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "popup-menu", G_CALLBACK(pidgin_blist_popup_menu_cb), NULL);

	/* Enable CTRL+F searching */
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(gtkblist->treeview), NAME_COLUMN);
	gtk_tree_view_set_search_equal_func(GTK_TREE_VIEW(gtkblist->treeview),
			pidgin_blist_search_equal_func, NULL, NULL);

	gtk_box_pack_start(GTK_BOX(gtkblist->vbox),
		pidgin_make_scrollable(gtkblist->treeview, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC, GTK_SHADOW_NONE, -1, -1),
		TRUE, TRUE, 0);

	sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(gtkblist->vbox), sep, FALSE, FALSE, 0);

	gtkblist->scrollbook = pidgin_scroll_book_new();
	gtk_box_pack_start(GTK_BOX(gtkblist->vbox), gtkblist->scrollbook, FALSE, FALSE, 0);

	priv->error_scrollbook = PIDGIN_SCROLL_BOOK(pidgin_scroll_book_new());
	gtk_box_pack_start(GTK_BOX(gtkblist->vbox),
		GTK_WIDGET(priv->error_scrollbook), FALSE, FALSE, 0);

	/* Add the statusbox */
	gtkblist->statusbox = pidgin_status_box_new();
	gtk_box_pack_start(GTK_BOX(gtkblist->vbox), gtkblist->statusbox, FALSE, TRUE, 0);
	gtk_widget_set_name(gtkblist->statusbox, "pidgin_blist_statusbox");
	gtk_widget_show(gtkblist->statusbox);

	/* Update some dynamic things */
	pidgin_blist_update_sort_methods();

	/* OK... let's show this bad boy. */
	pidgin_blist_refresh(list);
	pidgin_blist_restore_window_state();
	gtk_widget_show_all(GTK_WIDGET(gtkblist->vbox));
	gtk_widget_realize(GTK_WIDGET(gtkblist->window));
	purple_blist_set_visible(purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/list_visible"));

	/* start the refresh timer */
	gtkblist->refresh_timer = g_timeout_add_seconds(30, (GSourceFunc)pidgin_blist_refresh_timer, list);

	handle = pidgin_blist_get_handle();

	/* things that affect how buddies are displayed */
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/blist/show_buddy_icons",
			_prefs_change_redo_list, NULL);
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/blist/show_idle_time",
			_prefs_change_redo_list, NULL);
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/blist/show_empty_groups",
			_prefs_change_redo_list, NULL);
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/blist/show_offline_buddies",
			_prefs_change_redo_list, NULL);
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/blist/show_protocol_icons",
			_prefs_change_redo_list, NULL);

	/* sorting */
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/blist/sort_type",
			_prefs_change_sort_method, NULL);

	/* Setup some purple signal handlers. */

	handle = purple_accounts_get_handle();
	purple_signal_connect(handle, "account-enabled", gtkblist,
	                      PURPLE_CALLBACK(account_modified), gtkblist);
	purple_signal_connect(handle, "account-disabled", gtkblist,
	                      PURPLE_CALLBACK(account_modified), gtkblist);
	purple_signal_connect(handle, "account-removed", gtkblist,
	                      PURPLE_CALLBACK(account_modified), gtkblist);
	purple_signal_connect(handle, "account-status-changed", gtkblist,
	                      PURPLE_CALLBACK(account_status_changed),
	                      gtkblist);
	purple_signal_connect(handle, "account-error-changed", gtkblist,
	                      PURPLE_CALLBACK(update_account_error_state),
	                      gtkblist);

	handle = pidgin_accounts_get_handle();
	purple_signal_connect(handle, "account-modified", gtkblist,
	                      PURPLE_CALLBACK(account_modified), gtkblist);

	handle = purple_conversations_get_handle();
	purple_signal_connect(handle, "conversation-updated", gtkblist,
	                      PURPLE_CALLBACK(conversation_updated_cb),
	                      gtkblist);
	purple_signal_connect(handle, "deleting-conversation", gtkblist,
	                      PURPLE_CALLBACK(conversation_deleting_cb),
	                      gtkblist);
	purple_signal_connect(handle, "conversation-created", gtkblist,
	                      PURPLE_CALLBACK(conversation_created_cb),
	                      gtkblist);
	purple_signal_connect(handle, "chat-joined", gtkblist,
	                      PURPLE_CALLBACK(conversation_created_cb),
	                      gtkblist);

	gtk_widget_hide(gtkblist->headline);

	show_initial_account_errors(gtkblist);

	/* emit our created signal */
	handle = pidgin_blist_get_handle();
	purple_signal_emit(handle, "gtkblist-created", list);
}

static void redo_buddy_list(PurpleBuddyList *list, gboolean remove, gboolean rerender)
{
	PurpleBlistNode *node;

	gtkblist = PIDGIN_BUDDY_LIST(list);
	if(!gtkblist || !gtkblist->treeview)
		return;

	node = purple_blist_get_root(list);

	while (node)
	{
		/* This is only needed when we're reverting to a non-GTK sorted
		 * status.  We shouldn't need to remove otherwise.
		 */
		if (remove && !PURPLE_IS_GROUP(node))
			pidgin_blist_hide_node(list, node, FALSE);

		if (PURPLE_IS_BUDDY(node))
			pidgin_blist_update_buddy(list, node, rerender);
		else if (PURPLE_IS_CHAT(node))
			pidgin_blist_update(list, node);
		else if (PURPLE_IS_GROUP(node))
			pidgin_blist_update(list, node);
		node = purple_blist_node_next(node, FALSE);
	}

}

void pidgin_blist_refresh(PurpleBuddyList *list)
{
	redo_buddy_list(list, FALSE, TRUE);
}

void
pidgin_blist_update_refresh_timeout()
{
	PurpleBuddyList *blist;
	PidginBuddyList *gtkblist;

	blist = purple_blist_get_default();
	gtkblist = PIDGIN_BUDDY_LIST(blist);

	gtkblist->refresh_timer = g_timeout_add_seconds(30,(GSourceFunc)pidgin_blist_refresh_timer, blist);
}

static gboolean get_iter_from_node(PurpleBlistNode *node, GtkTreeIter *iter) {
	PidginBlistNode *gtknode = g_object_get_data(G_OBJECT(node), UI_DATA);
	GtkTreePath *path;

	if (!gtknode) {
		return FALSE;
	}

	if (!gtkblist) {
		purple_debug_error("gtkblist", "get_iter_from_node was called, but we don't seem to have a blist\n");
		return FALSE;
	}

	if (!gtknode->row)
		return FALSE;

	if (!GTK_IS_TREE_MODEL(gtkblist->treemodel)) {
		return FALSE;
	}

	if ((path = gtk_tree_row_reference_get_path(gtknode->row)) == NULL)
		return FALSE;

	if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), iter, path)) {
		gtk_tree_path_free(path);
		return FALSE;
	}
	gtk_tree_path_free(path);
	return TRUE;
}

static void
pidgin_blist_remove(PurpleBuddyList *list, PurpleBlistNode *node) {
	purple_request_close_with_handle(node);

	pidgin_blist_hide_node(list, node, TRUE);

	if(node->parent) {
		pidgin_blist_update(list, node->parent);
	}
}

static gboolean do_selection_changed(PurpleBlistNode *new_selection)
{
	PurpleBlistNode *old_selection = NULL;

	/* test for gtkblist because crazy timeout means we can be called after the blist is gone */
	if (gtkblist && new_selection != gtkblist->selected_node) {
		old_selection = gtkblist->selected_node;
		gtkblist->selected_node = new_selection;
		if(new_selection)
			pidgin_blist_update(NULL, new_selection);
		if(old_selection)
			pidgin_blist_update(NULL, old_selection);
	}

	return FALSE;
}

static void pidgin_blist_selection_changed(GtkTreeSelection *selection, gpointer data)
{
	PurpleBlistNode *new_selection = NULL;
	GtkTreeIter iter;

	if(gtk_tree_selection_get_selected(selection, NULL, &iter)){
		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter,
				NODE_COLUMN, &new_selection, -1);
	}

	/* we set this up as a timeout, otherwise the blist flickers ...
	 * but we don't do it for groups, because it causes total bizarness -
	 * the previously selected buddy node might rendered at half height.
	 */
	if ((new_selection != NULL) && PURPLE_IS_GROUP(new_selection)) {
		do_selection_changed(new_selection);
	} else {
		g_timeout_add(0, (GSourceFunc)do_selection_changed, new_selection);
	}
}

static gboolean insert_node(PurpleBuddyList *list, PurpleBlistNode *node, GtkTreeIter *iter)
{
	GtkTreeIter parent_iter = {0, NULL, NULL, NULL}, cur, *curptr = NULL;
	PidginBlistNode *gtknode = g_object_get_data(G_OBJECT(node), UI_DATA);
	GtkTreePath *newpath;

	if(!iter)
		return FALSE;

	/* XXX: it's not necessary, but let's silence a warning*/
	memset(&parent_iter, 0, sizeof(parent_iter));

	if(node->parent && !get_iter_from_node(node->parent, &parent_iter))
		return FALSE;

	if(get_iter_from_node(node, &cur))
		curptr = &cur;

	if(PURPLE_IS_CONTACT(node) || PURPLE_IS_CHAT(node)) {
		current_sort_method->func(node, list, parent_iter, curptr, iter);
	} else {
		sort_method_none(node, list, parent_iter, curptr, iter);
	}

	if(gtknode != NULL) {
		gtk_tree_row_reference_free(gtknode->row);
	} else {
		pidgin_blist_new_node(list, node);
		gtknode = g_object_get_data(G_OBJECT(node), UI_DATA);
	}

	newpath = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel),
			iter);
	gtknode->row =
		gtk_tree_row_reference_new(GTK_TREE_MODEL(gtkblist->treemodel),
				newpath);

	gtk_tree_path_free(newpath);

	if (!editing_blist)
		gtk_tree_store_set(gtkblist->treemodel, iter,
				NODE_COLUMN, node,
				-1);

	if(node->parent) {
		GtkTreePath *expand = NULL;
		PidginBlistNode *gtkparentnode = g_object_get_data(G_OBJECT(node->parent), UI_DATA);

		if(PURPLE_IS_GROUP(node->parent)) {
			if(!purple_blist_node_get_bool(node->parent, "collapsed"))
				expand = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &parent_iter);
		} else if(PURPLE_IS_CONTACT(node->parent) &&
				gtkparentnode->contact_expanded) {
			expand = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &parent_iter);
		}
		if(expand) {
			gtk_tree_view_expand_row(GTK_TREE_VIEW(gtkblist->treeview), expand, FALSE);
			gtk_tree_path_free(expand);
		}
	}

	return TRUE;
}

static gboolean pidgin_blist_group_has_show_offline_buddy(PurpleGroup *group)
{
	PurpleBlistNode *gnode, *cnode, *bnode;

	gnode = PURPLE_BLIST_NODE(group);
	for(cnode = gnode->child; cnode; cnode = cnode->next) {
		if(PURPLE_IS_CONTACT(cnode)) {
			for(bnode = cnode->child; bnode; bnode = bnode->next) {
				PurpleBuddy *buddy = (PurpleBuddy *)bnode;
				if (purple_account_is_connected(purple_buddy_get_account(buddy)) &&
					purple_blist_node_get_bool(bnode, "show_offline"))
					return TRUE;
			}
		}
	}
	return FALSE;
}

/* This version of pidgin_blist_update_group can take the original buddy or a
 * group, but has much better algorithmic performance with a pre-known buddy.
 */
static void pidgin_blist_update_group(PurpleBuddyList *list,
                                      PurpleBlistNode *node)
{
	gint count;
	PurpleGroup *group;
	PurpleBlistNode* gnode;
	gboolean show = FALSE, show_offline = FALSE;

	g_return_if_fail(node != NULL);

	if (editing_blist)
		return;

	if (PURPLE_IS_GROUP(node))
		gnode = node;
	else if (PURPLE_IS_BUDDY(node))
		gnode = node->parent->parent;
	else if (PURPLE_IS_CONTACT(node) || PURPLE_IS_CHAT(node))
		gnode = node->parent;
	else
		return;

	group = (PurpleGroup*)gnode;

	show_offline = purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_offline_buddies");

	if(show_offline)
		count = purple_counting_node_get_current_size(PURPLE_COUNTING_NODE(group));
	else
		count = purple_counting_node_get_online_count(PURPLE_COUNTING_NODE(group));

	if (count > 0 || purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_empty_groups"))
		show = TRUE;
	else if (PURPLE_IS_BUDDY(node) && buddy_is_displayable((PurpleBuddy*)node)) { /* Or chat? */
		show = TRUE;
	} else if (!show_offline) {
		show = pidgin_blist_group_has_show_offline_buddy(group);
	}

	if (show) {
		gchar *title;
		gboolean biglist;
		GtkTreeIter iter;
		GtkTreePath *path;
		gboolean expanded;
		GdkPixbuf *avatar = NULL;

		if(!insert_node(list, gnode, &iter))
			return;

		path = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &iter);
		expanded = gtk_tree_view_row_expanded(GTK_TREE_VIEW(gtkblist->treeview), path);
		gtk_tree_path_free(path);

		title = pidgin_get_group_title(gnode, expanded);
		biglist = purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_buddy_icons");

		if (biglist) {
			avatar = pidgin_blist_get_buddy_icon(gnode, TRUE, TRUE);
		}

		gtk_tree_store_set(gtkblist->treemodel, &iter,
				   STATUS_ICON_VISIBLE_COLUMN, FALSE,
				   STATUS_ICON_COLUMN, NULL,
				   NAME_COLUMN, title,
				   NODE_COLUMN, gnode,
				   GROUP_EXPANDER_COLUMN, TRUE,
				   GROUP_EXPANDER_VISIBLE_COLUMN, TRUE,
				   CONTACT_EXPANDER_VISIBLE_COLUMN, FALSE,
				   BUDDY_ICON_COLUMN, avatar,
				   BUDDY_ICON_VISIBLE_COLUMN, biglist,
				   IDLE_VISIBLE_COLUMN, FALSE,
				   EMBLEM_VISIBLE_COLUMN, FALSE,
				   -1);
		g_free(title);
	} else {
		pidgin_blist_hide_node(list, gnode, TRUE);
	}
}

static char *pidgin_get_group_title(PurpleBlistNode *gnode, gboolean expanded)
{
	PurpleGroup *group;
	char group_count[12] = "";
	char *mark, *esc;
	PurpleBlistNode *selected_node = NULL;
	GtkTreeIter iter;

	group = (PurpleGroup*)gnode;

	if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkblist->treeview)), NULL, &iter)) {
		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter,
				NODE_COLUMN, &selected_node, -1);
	}

	if (!expanded) {
		g_snprintf(group_count, sizeof(group_count), "%d/%d",
		           purple_counting_node_get_online_count(PURPLE_COUNTING_NODE(group)),
		           purple_counting_node_get_current_size(PURPLE_COUNTING_NODE(group)));
	}

	esc = g_markup_escape_text(purple_group_get_name(group), -1);

	mark = g_strdup_printf("<b>%s</b>%s%s%s",
	                       esc ? esc : "",
	                       !expanded ? " <span weight='light'>(</span>" : "",
	                       group_count,
	                       !expanded ? "<span weight='light'>)</span>" : "");

	g_free(esc);
	return mark;
}

static void buddy_node(PurpleBuddy *buddy, GtkTreeIter *iter, PurpleBlistNode *node)
{
	PurplePresence *presence = purple_buddy_get_presence(buddy);
	PidginBlistNode *pidgin_node = NULL;
	GdkPixbuf *status, *avatar, *emblem, *protocol_icon;
	char *mark;
	char *idle = NULL;
	gboolean selected = (gtkblist->selected_node == node);
	gboolean biglist = purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_buddy_icons");

	if(editing_blist) {
		return;
	}

	pidgin_node = g_object_get_data(G_OBJECT(node->parent), UI_DATA);

	status = pidgin_blist_get_status_icon(PURPLE_BLIST_NODE(buddy),
						biglist ? PIDGIN_STATUS_ICON_LARGE : PIDGIN_STATUS_ICON_SMALL);

	/* Speed it up if we don't want buddy icons. */
	if(biglist)
		avatar = pidgin_blist_get_buddy_icon(PURPLE_BLIST_NODE(buddy), TRUE, TRUE);
	else
		avatar = NULL;

	if (!avatar) {
		g_object_ref(G_OBJECT(gtkblist->empty_avatar));
		avatar = gtkblist->empty_avatar;
	} else if ((!PURPLE_BUDDY_IS_ONLINE(buddy) || purple_presence_is_idle(presence))) {
		do_alphashift(avatar, 77);
	}

	emblem = pidgin_blist_get_emblem(PURPLE_BLIST_NODE(buddy));
	mark = pidgin_blist_get_name_markup(buddy, selected, TRUE);

	if (purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_idle_time") &&
		purple_presence_is_idle(presence) && !biglist)
	{
		time_t idle_secs = purple_presence_get_idle_time(presence);

		if (idle_secs > 0) {
			time_t t;
			int ihrs, imin;
			time(&t);

			ihrs = (t - idle_secs) / 3600;
			imin = ((t - idle_secs) / 60) % 60;

			if(selected) {
				idle = g_strdup_printf("%d:%02d", ihrs, imin);
			} else {
				idle = g_strdup_printf("<span color='%s'>%d:%02d</span>",
				                       pidgin_style_context_is_dark()
				                               ? "light slate grey"
				                               : "dim grey",
				                       ihrs, imin);
			}
		}
	}

	protocol_icon = pidgin_create_protocol_icon(purple_buddy_get_account(buddy), PIDGIN_PROTOCOL_ICON_SMALL);

	gtk_tree_store_set(gtkblist->treemodel, iter,
			   STATUS_ICON_COLUMN, status,
			   STATUS_ICON_VISIBLE_COLUMN, TRUE,
			   NAME_COLUMN, mark,
			   IDLE_COLUMN, idle,
			   IDLE_VISIBLE_COLUMN, !biglist && idle,
			   BUDDY_ICON_COLUMN, avatar,
			   BUDDY_ICON_VISIBLE_COLUMN, biglist,
			   EMBLEM_COLUMN, emblem,
			   EMBLEM_VISIBLE_COLUMN, (emblem != NULL),
			   PROTOCOL_ICON_COLUMN, protocol_icon,
			   PROTOCOL_ICON_VISIBLE_COLUMN, purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_protocol_icons"),
			   CONTACT_EXPANDER_COLUMN, NULL,
			   CONTACT_EXPANDER_VISIBLE_COLUMN, pidgin_node->contact_expanded,
			   GROUP_EXPANDER_VISIBLE_COLUMN, FALSE,
			-1);

	g_free(mark);
	g_free(idle);
	if(emblem)
		g_object_unref(emblem);
	if(status)
		g_object_unref(status);
	if(avatar)
		g_object_unref(avatar);
	if(protocol_icon)
		g_object_unref(protocol_icon);
}

/* This is a variation on the original gtk_blist_update_contact. Here we
	can know in advance which buddy has changed so we can just update that */
static void pidgin_blist_update_contact(PurpleBuddyList *list, PurpleBlistNode *node)
{
	PurpleBlistNode *cnode;
	PurpleContact *contact;
	PurpleBuddy *buddy;
	gboolean biglist = purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_buddy_icons");
	PidginBlistNode *gtknode;

	if (editing_blist)
		return;

	if (PURPLE_IS_BUDDY(node))
		cnode = node->parent;
	else
		cnode = node;

	g_return_if_fail(PURPLE_IS_CONTACT(cnode));

	/* First things first, update the group */
	if (PURPLE_IS_BUDDY(node))
		pidgin_blist_update_group(list, node);
	else
		pidgin_blist_update_group(list, cnode->parent);

	contact = (PurpleContact*)cnode;
	buddy = purple_contact_get_priority_buddy(contact);

	if (buddy_is_displayable(buddy))
	{
		GtkTreeIter iter;

		if(!insert_node(list, cnode, &iter))
			return;

		gtknode = g_object_get_data(G_OBJECT(cnode), UI_DATA);

		if(gtknode->contact_expanded) {
			GdkPixbuf *status;
			gchar *mark;

			mark = g_markup_escape_text(purple_contact_get_alias(contact), -1);

			status = pidgin_blist_get_status_icon(cnode,
					 biglist? PIDGIN_STATUS_ICON_LARGE : PIDGIN_STATUS_ICON_SMALL);

			gtk_tree_store_set(gtkblist->treemodel, &iter,
					   STATUS_ICON_COLUMN, status,
					   STATUS_ICON_VISIBLE_COLUMN, TRUE,
					   NAME_COLUMN, mark,
					   IDLE_COLUMN, NULL,
					   IDLE_VISIBLE_COLUMN, FALSE,
					   BUDDY_ICON_COLUMN, NULL,
					   CONTACT_EXPANDER_COLUMN, TRUE,
					   CONTACT_EXPANDER_VISIBLE_COLUMN, TRUE,
				  	   GROUP_EXPANDER_VISIBLE_COLUMN, FALSE,
					-1);
			g_free(mark);
			if(status)
				g_object_unref(status);
		} else {
			buddy_node(buddy, &iter, cnode);
		}
	} else {
		pidgin_blist_hide_node(list, cnode, TRUE);
	}
}



static void pidgin_blist_update_buddy(PurpleBuddyList *list, PurpleBlistNode *node, gboolean status_change)
{
	PurpleBuddy *buddy;
	PidginBlistNode *gtkparentnode;

	g_return_if_fail(PURPLE_IS_BUDDY(node));

	if (node->parent == NULL)
		return;

	buddy = (PurpleBuddy*)node;

	/* First things first, update the contact */
	pidgin_blist_update_contact(list, node);

	gtkparentnode = g_object_get_data(G_OBJECT(node->parent), UI_DATA);

	if (gtkparentnode->contact_expanded && buddy_is_displayable(buddy))
	{
		GtkTreeIter iter;

		if (!insert_node(list, node, &iter))
			return;

		buddy_node(buddy, &iter, node);

	} else {
		pidgin_blist_hide_node(list, node, TRUE);
	}

}

static void pidgin_blist_update_chat(PurpleBuddyList *list, PurpleBlistNode *node)
{
	PurpleChat *chat;

	g_return_if_fail(PURPLE_IS_CHAT(node));

	if (editing_blist)
		return;

	/* First things first, update the group */
	pidgin_blist_update_group(list, node->parent);

	chat = (PurpleChat*)node;

	if(purple_account_is_connected(purple_chat_get_account(chat))) {
		GtkTreeIter iter;
		GdkPixbuf *status, *avatar, *emblem, *protocol_icon;
		const gchar *color = NULL;
		gchar *mark, *tmp;
		gboolean showicons = purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_buddy_icons");
		gboolean biglist = purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_buddy_icons");
		PidginBlistNode *ui;
		PurpleConversation *conv;
		gboolean hidden = FALSE;
		gboolean selected = (gtkblist->selected_node == node);
		gboolean nick_said = FALSE;

		if (!insert_node(list, node, &iter))
			return;

		ui = g_object_get_data(G_OBJECT(node), UI_DATA);
		conv = ui->conv.conv;
		if (conv && pidgin_conv_is_hidden(PIDGIN_CONVERSATION(conv))) {
			hidden = (ui->conv.flags & PIDGIN_BLIST_NODE_HAS_PENDING_MESSAGE);
			nick_said = (ui->conv.flags & PIDGIN_BLIST_CHAT_HAS_PENDING_MESSAGE_WITH_NICK);
		}

		status = pidgin_blist_get_status_icon(node,
				 biglist ? PIDGIN_STATUS_ICON_LARGE : PIDGIN_STATUS_ICON_SMALL);
		emblem = pidgin_blist_get_emblem(node);

		/* Speed it up if we don't want buddy icons. */
		if(showicons)
			avatar = pidgin_blist_get_buddy_icon(node, TRUE, FALSE);
		else
			avatar = NULL;

		mark = g_markup_escape_text(purple_chat_get_name(chat), -1);

		if(selected ) {
			/* nick_said color is the same as gtkconv:tab-label-attention */
			color = (nick_said ? "#006aff" : NULL);
		}

		if(color) {
			tmp = g_strdup_printf("<span color='%s' weight='%s'>%s</span>",
			                      color, hidden ? "bold" : "normal", mark);
		} else {
			tmp = g_strdup_printf("<span weight='%s'>%s</span>",
			                      hidden ? "bold" : "normal", mark);
		}
		g_free(mark);
		mark = tmp;

		protocol_icon = pidgin_create_protocol_icon(purple_chat_get_account(chat), PIDGIN_PROTOCOL_ICON_SMALL);

		gtk_tree_store_set(gtkblist->treemodel, &iter,
				STATUS_ICON_COLUMN, status,
				STATUS_ICON_VISIBLE_COLUMN, TRUE,
				BUDDY_ICON_COLUMN, avatar ? avatar : gtkblist->empty_avatar,
				BUDDY_ICON_VISIBLE_COLUMN, showicons,
				EMBLEM_COLUMN, emblem,
				EMBLEM_VISIBLE_COLUMN, emblem != NULL,
				PROTOCOL_ICON_COLUMN, protocol_icon,
				PROTOCOL_ICON_VISIBLE_COLUMN, purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/blist/show_protocol_icons"),
				NAME_COLUMN, mark,
				GROUP_EXPANDER_VISIBLE_COLUMN, FALSE,
				-1);

		g_free(mark);
		if(emblem)
			g_object_unref(emblem);
		if(status)
			g_object_unref(status);
		if(avatar)
			g_object_unref(avatar);
		if(protocol_icon)
			g_object_unref(protocol_icon);

	} else {
		pidgin_blist_hide_node(list, node, TRUE);
	}
}

static void pidgin_blist_update(PurpleBuddyList *list, PurpleBlistNode *node)
{
	if(list) {
		gtkblist = PIDGIN_BUDDY_LIST(list);
	}
	if(!gtkblist || !gtkblist->treeview || !node) {
		return;
	}

	if(g_object_get_data(G_OBJECT(node), UI_DATA) == NULL) {
		pidgin_blist_new_node(list, node);
	}

	if(PURPLE_IS_GROUP(node)) {
		pidgin_blist_update_group(list, node);
	} else if(PURPLE_IS_CONTACT(node)) {
		pidgin_blist_update_contact(list, node);
	} else if(PURPLE_IS_BUDDY(node)) {
		pidgin_blist_update_buddy(list, node, TRUE);
	} else if(PURPLE_IS_CHAT(node)) {
		pidgin_blist_update_chat(list, node);
	}
}

static void pidgin_blist_set_visible(PurpleBuddyList *list, gboolean show)
{
	if (!(gtkblist && gtkblist->window))
		return;

	if (show) {
		gtk_widget_show(gtkblist->window);
	} else {
		if(visibility_manager_count) {
			gtk_widget_hide(gtkblist->window);
		} else {
			if (!gtk_widget_get_visible(gtkblist->window))
				gtk_widget_show(gtkblist->window);
			gtk_window_iconify(GTK_WINDOW(gtkblist->window));
		}
	}
}

static GList *
groups_tree(void)
{
	static GList *list = NULL;
	PurpleGroup *g;
	PurpleBlistNode *gnode;

	g_list_free(list);
	list = NULL;

	gnode = purple_blist_get_default_root();
	if (gnode == NULL) {
		list  = g_list_append(list,
			(gpointer)PURPLE_BLIST_DEFAULT_GROUP_NAME);
	} else {
		for (; gnode != NULL; gnode = gnode->next) {
			if (PURPLE_IS_GROUP(gnode))
			{
				g    = (PurpleGroup *)gnode;
				list  = g_list_append(list, (char *) purple_group_get_name(g));
			}
		}
	}

	return list;
}

static void
add_buddy_select_account_cb(GObject *w, PidginAddBuddyData *data)
{
	PidginAccountChooser *chooser = PIDGIN_ACCOUNT_CHOOSER(w);
	PurpleAccount *account = pidgin_account_chooser_get_selected(chooser);
	PurpleConnection *pc = NULL;
	PurpleProtocol *protocol = NULL;
	gboolean invite_enabled = TRUE;

	/* Save our account */
	data->rq_data.account = account;

	if (account)
		pc = purple_account_get_connection(account);
	if (pc)
		protocol = purple_connection_get_protocol(pc);
	if (protocol && !(purple_protocol_get_options(protocol) & OPT_PROTO_INVITE_MESSAGE))
		invite_enabled = FALSE;

	gtk_widget_set_sensitive(data->entry_for_invite, invite_enabled);
	set_sensitive_if_input_buddy_cb(data->entry, data);
}

static void
destroy_add_buddy_dialog_cb(GtkWidget *win, PidginAddBuddyData *data)
{
	g_free(data);
}

static void
add_buddy_cb(GtkWidget *w, int resp, PidginAddBuddyData *data)
{
	const char *grp, *who, *whoalias, *invite;
	PurpleAccount *account;
	PurpleGroup *g;
	PurpleBuddy *b;
	PurpleConversation *im;
	PurpleBuddyIcon *icon;

	if (resp == GTK_RESPONSE_OK)
	{
		PurpleConversationManager *manager;

		who = gtk_entry_get_text(GTK_ENTRY(data->entry));
		grp = pidgin_text_combo_box_entry_get_text(data->combo);
		whoalias = gtk_entry_get_text(GTK_ENTRY(data->entry_for_alias));
		if (*whoalias == '\0')
			whoalias = NULL;
		invite = gtk_entry_get_text(GTK_ENTRY(data->entry_for_invite));
		if (*invite == '\0')
			invite = NULL;

		account = data->rq_data.account;

		g = NULL;
		if ((grp != NULL) && (*grp != '\0'))
		{
			if ((g = purple_blist_find_group(grp)) == NULL)
			{
				g = purple_group_new(grp);
				purple_blist_add_group(g, NULL);
			}

			b = purple_blist_find_buddy_in_group(account, who, g);
		}
		else if ((b = purple_blist_find_buddy(account, who)) != NULL)
		{
			g = purple_buddy_get_group(b);
		}

		if (b == NULL)
		{
			b = purple_buddy_new(account, who, whoalias);
			purple_blist_add_buddy(b, NULL, g, NULL);
		}

		purple_account_add_buddy(account, b, invite);

		/* Offer to merge people with the same alias. */
		if (whoalias != NULL && g != NULL)
			gtk_blist_auto_personize(PURPLE_BLIST_NODE(g), whoalias);

		/*
		 * XXX
		 * It really seems like it would be better if the call to
		 * purple_account_add_buddy() and purple_conversation_update() were done in
		 * blist.c, possibly in the purple_blist_add_buddy() function.  Maybe
		 * purple_account_add_buddy() should be renamed to
		 * purple_blist_add_new_buddy() or something, and have it call
		 * purple_blist_add_buddy() after it creates it.  --Mark
		 *
		 * No that's not good.  blist.c should only deal with adding nodes to the
		 * local list.  We need a new, non-gtk file that calls both
		 * purple_account_add_buddy and purple_blist_add_buddy().
		 * Or something.  --Mark
		 */

		manager = purple_conversation_manager_get_default();
		im = purple_conversation_manager_find_im(manager, data->rq_data.account,
		                                         who);
		if(PURPLE_IS_IM_CONVERSATION(im)) {
			icon = purple_im_conversation_get_icon(PURPLE_IM_CONVERSATION(im));
			if(icon != NULL) {
				purple_buddy_icon_update(icon);
			}
		}
	}

	gtk_widget_destroy(data->rq_data.window);
}

static void
pidgin_blist_request_add_buddy(PurpleBuddyList *list, PurpleAccount *account,
                               const char *username, const char *group,
                               const char *alias)
{
	PidginAccountChooser *chooser = NULL;
	PidginAddBuddyData *data = g_new0(PidginAddBuddyData, 1);
	PidginBlistRequestData *blist_req_data = &data->rq_data;

	if (account == NULL)
		account = purple_connection_get_account(purple_connections_get_all()->data);

	make_blist_request_dialog(blist_req_data,
		account,
		_("Add Buddy"), "add_buddy",
		_("Add a buddy.\n"),
		G_CALLBACK(add_buddy_select_account_cb), add_buddy_account_filter_func,
		G_CALLBACK(add_buddy_cb));
	gtk_dialog_add_buttons(GTK_DIALOG(data->rq_data.window),
			_("Cancel"), GTK_RESPONSE_CANCEL,
			_("Add"), GTK_RESPONSE_OK,
			NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(data->rq_data.window),
			GTK_RESPONSE_OK);

	g_signal_connect(G_OBJECT(data->rq_data.window), "destroy",
	                 G_CALLBACK(destroy_add_buddy_dialog_cb), data);

	data->entry = gtk_entry_new();

	pidgin_add_widget_to_vbox(data->rq_data.vbox, _("Buddy's _username:"),
		data->rq_data.sg, data->entry, TRUE, NULL);
	gtk_widget_grab_focus(data->entry);

	if (username != NULL)
		gtk_entry_set_text(GTK_ENTRY(data->entry), username);
	else
		gtk_dialog_set_response_sensitive(GTK_DIALOG(data->rq_data.window),
		                                  GTK_RESPONSE_OK, FALSE);

	gtk_entry_set_activates_default (GTK_ENTRY(data->entry), TRUE);

	g_signal_connect(G_OBJECT(data->entry), "changed",
	                 G_CALLBACK(set_sensitive_if_input_buddy_cb),
	                 data);

	data->entry_for_alias = gtk_entry_new();
	pidgin_add_widget_to_vbox(data->rq_data.vbox, _("(Optional) A_lias:"),
	                          data->rq_data.sg, data->entry_for_alias, TRUE,
	                          NULL);

	if (alias != NULL)
		gtk_entry_set_text(GTK_ENTRY(data->entry_for_alias), alias);

	if (username != NULL)
		gtk_widget_grab_focus(GTK_WIDGET(data->entry_for_alias));

	data->entry_for_invite = gtk_entry_new();
	pidgin_add_widget_to_vbox(data->rq_data.vbox, _("(Optional) _Invite message:"),
	                          data->rq_data.sg, data->entry_for_invite, TRUE,
	                          NULL);

	data->combo = pidgin_text_combo_box_entry_new(group, groups_tree());
	pidgin_add_widget_to_vbox(data->rq_data.vbox, _("Add buddy to _group:"),
	                          data->rq_data.sg, data->combo, TRUE, NULL);

	gtk_widget_show_all(data->rq_data.window);

	/* Force update of invite message entry sensitivity */
	chooser = PIDGIN_ACCOUNT_CHOOSER(blist_req_data->account_menu);
	pidgin_account_chooser_set_selected(chooser, account);
}

static void
add_chat_cb(GtkWidget *w, PidginAddChatData *data)
{
	GList *tmp;
	PurpleChat *chat;
	GHashTable *components;

	components = g_hash_table_new_full(g_str_hash, g_str_equal,
									   g_free, g_free);

	for (tmp = data->chat_data.entries; tmp; tmp = tmp->next)
	{
		if (g_object_get_data(tmp->data, "is_spin"))
		{
			g_hash_table_replace(components,
					g_strdup(g_object_get_data(tmp->data, "identifier")),
					g_strdup_printf("%d",
						gtk_spin_button_get_value_as_int(tmp->data)));
		}
		else
		{
			const char *value = gtk_entry_get_text(tmp->data);

			if (*value != '\0')
				g_hash_table_replace(components,
						g_strdup(g_object_get_data(tmp->data, "identifier")),
						g_strdup(value));
		}
	}

	chat = purple_chat_new(data->chat_data.rq_data.account,
	                       gtk_entry_get_text(GTK_ENTRY(data->alias_entry)),
	                       components);

	if (chat != NULL) {
		PurpleGroup *group;
		const char *group_name;

		group_name = pidgin_text_combo_box_entry_get_text(data->group_combo);

		group = NULL;
		if ((group_name != NULL) && (*group_name != '\0') &&
		    ((group = purple_blist_find_group(group_name)) == NULL))
		{
			group = purple_group_new(group_name);
			purple_blist_add_group(group, NULL);
		}

		purple_blist_add_chat(chat, group, NULL);

		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->autojoin)))
			purple_blist_node_set_bool(PURPLE_BLIST_NODE(chat), "gtk-autojoin", TRUE);

		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(data->persistent)))
			purple_blist_node_set_bool(PURPLE_BLIST_NODE(chat), "gtk-persistent", TRUE);
	}

	gtk_widget_destroy(data->chat_data.rq_data.window);
	g_free(data->chat_data.default_chat_name);
	g_list_free(data->chat_data.entries);
	g_free(data);
}

static void
add_chat_resp_cb(GtkWidget *w, int resp, PidginAddChatData *data)
{
	if (resp == GTK_RESPONSE_OK)
	{
		add_chat_cb(NULL, data);
	}
	else if (resp == 1)
	{
		pidgin_roomlist_dialog_show_with_account(data->chat_data.rq_data.account);
	}
	else
	{
		gtk_widget_destroy(data->chat_data.rq_data.window);
		g_free(data->chat_data.default_chat_name);
		g_list_free(data->chat_data.entries);
		g_free(data);
	}
}

static void
pidgin_blist_request_add_chat(PurpleBuddyList *list, PurpleAccount *account,
                              PurpleGroup *group, const char *alias,
                              const char *name)
{
	PidginAddChatData *data;
	GList *l;
	PurpleConnection *gc;
	GtkBox *vbox;
	PurpleProtocol *protocol = NULL;

	if (account != NULL) {
		gc = purple_account_get_connection(account);
		protocol = purple_connection_get_protocol(gc);

		if (!PURPLE_PROTOCOL_IMPLEMENTS(protocol, CHAT, join)) {
			purple_notify_error(gc, NULL, _("This protocol does not"
				" support chat rooms."), NULL,
				purple_request_cpar_from_account(account));
			return;
		}
	} else {
		/* Find an account with chat capabilities */
		for (l = purple_connections_get_all(); l != NULL; l = l->next) {
			gc = (PurpleConnection *)l->data;
			protocol = purple_connection_get_protocol(gc);

			if (PURPLE_PROTOCOL_IMPLEMENTS(protocol, CHAT, join)) {
				account = purple_connection_get_account(gc);
				break;
			}
		}

		if (account == NULL) {
			purple_notify_error(NULL, NULL,
							  _("You are not currently signed on with any "
								"protocols that have the ability to chat."), NULL, NULL);
			return;
		}
	}

	data = g_new0(PidginAddChatData, 1);
	vbox = GTK_BOX(make_blist_request_dialog((PidginBlistRequestData *)data, account,
			_("Add Chat"), "add_chat",
			_("Please enter an alias, and the appropriate information "
			  "about the chat you would like to add to your buddy list.\n"),
			G_CALLBACK(chat_select_account_cb), chat_account_filter_func,
			G_CALLBACK(add_chat_resp_cb)));
	gtk_dialog_add_buttons(GTK_DIALOG(data->chat_data.rq_data.window),
		_("Room List"), 1,
		_("Cancel"), GTK_RESPONSE_CANCEL,
		_("Add"), GTK_RESPONSE_OK,
		NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(data->chat_data.rq_data.window),
			GTK_RESPONSE_OK);

	data->chat_data.default_chat_name = g_strdup(name);

	rebuild_chat_entries((PidginChatData *)data, name);

	data->alias_entry = gtk_entry_new();
	if (alias != NULL)
		gtk_entry_set_text(GTK_ENTRY(data->alias_entry), alias);
	gtk_entry_set_activates_default(GTK_ENTRY(data->alias_entry), TRUE);

	pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("A_lias:"),
	                          data->chat_data.rq_data.sg, data->alias_entry,
	                          TRUE, NULL);
	if (name != NULL)
		gtk_widget_grab_focus(data->alias_entry);

	data->group_combo = pidgin_text_combo_box_entry_new(group ? purple_group_get_name(group) : NULL, groups_tree());
	pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("_Group:"),
	                          data->chat_data.rq_data.sg, data->group_combo,
	                          TRUE, NULL);

	data->autojoin = gtk_check_button_new_with_mnemonic(_("Automatically _join when account connects"));
	data->persistent = gtk_check_button_new_with_mnemonic(_("_Remain in chat after window is closed"));
	gtk_box_pack_start(GTK_BOX(vbox), data->autojoin, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), data->persistent, FALSE, FALSE, 0);

	gtk_widget_show_all(data->chat_data.rq_data.window);
}

static void
add_group_cb(PurpleConnection *gc, const char *group_name)
{
	PurpleGroup *group;

	if ((group_name == NULL) || (*group_name == '\0'))
		return;

	group = purple_group_new(group_name);
	purple_blist_add_group(group, NULL);
}

static void
pidgin_blist_request_add_group(PurpleBuddyList *list)
{
	purple_request_input(NULL, _("Add Group"), NULL,
					   _("Please enter the name of the group to be added."),
					   NULL, FALSE, FALSE, NULL,
					   _("Add"), G_CALLBACK(add_group_cb),
					   _("Cancel"), NULL,
					   NULL, NULL);
}

void
pidgin_blist_toggle_visibility()
{
	if (gtkblist && gtkblist->window) {
		if (gtk_widget_get_visible(gtkblist->window)) {
			/* make the buddy list visible if it is iconified or if it is
			 * obscured and not currently focused (the focus part ensures
			 * that we do something reasonable if the buddy list is obscured
			 * by a window set to always be on top), otherwise hide the
			 * buddy list
			 */
			purple_blist_set_visible(PIDGIN_WINDOW_ICONIFIED(gtkblist->window) ||
					((gtk_blist_visibility != GDK_VISIBILITY_UNOBSCURED) &&
					!gtk_blist_focused));
		} else {
			purple_blist_set_visible(TRUE);
		}
	}
}

void
pidgin_blist_visibility_manager_add()
{
	visibility_manager_count++;
	purple_debug_info("gtkblist", "added visibility manager: %d\n", visibility_manager_count);
}

void
pidgin_blist_visibility_manager_remove()
{
	if (visibility_manager_count)
		visibility_manager_count--;
	if (!visibility_manager_count)
		purple_blist_set_visible(TRUE);
	purple_debug_info("gtkblist", "removed visibility manager: %d\n", visibility_manager_count);
}

void pidgin_blist_add_alert(GtkWidget *widget)
{
	gtk_container_add(GTK_CONTAINER(gtkblist->scrollbook), widget);
	set_urgent();
}

void
pidgin_blist_set_headline(const char *text, const gchar *icon_name,
		GCallback callback, gpointer user_data, GDestroyNotify destroy)
{
	/* Destroy any existing headline first */
	if (gtkblist->headline_destroy)
		gtkblist->headline_destroy(gtkblist->headline_data);

	gtk_label_set_markup(GTK_LABEL(gtkblist->headline_label), text);
	gtk_image_set_from_icon_name(GTK_IMAGE(gtkblist->headline_image),
			icon_name, GTK_ICON_SIZE_SMALL_TOOLBAR);

	gtkblist->headline_callback = callback;
	gtkblist->headline_data = user_data;
	gtkblist->headline_destroy = destroy;
	if (text != NULL || icon_name != NULL) {
		set_urgent();
		gtk_widget_show_all(gtkblist->headline);
	} else {
		gtk_widget_hide(gtkblist->headline);
	}
}


static void
set_urgent(void) {
	if(gtkblist->window && !gtk_widget_has_focus(gtkblist->window)) {
		gtk_window_set_urgency_hint(GTK_WINDOW(gtkblist->window), TRUE);
	}
}

PidginBuddyList *
pidgin_blist_get_default_gtk_blist(void)
{
	return gtkblist;
}

static gboolean autojoin_cb(PurpleConnection *gc, gpointer data)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	PurpleBlistNode *gnode, *cnode;
	for (gnode = purple_blist_get_default_root(); gnode;
	     gnode = gnode->next) {
		if(!PURPLE_IS_GROUP(gnode))
			continue;
		for(cnode = gnode->child; cnode; cnode = cnode->next)
		{
			PurpleChat *chat;

			if(!PURPLE_IS_CHAT(cnode))
				continue;

			chat = (PurpleChat *)cnode;

			if(purple_chat_get_account(chat) != account)
				continue;

			if (purple_blist_node_get_bool(PURPLE_BLIST_NODE(chat), "gtk-autojoin"))
				purple_serv_join_chat(gc, purple_chat_get_components(chat));
		}
	}

	/* Stop processing; we handled the autojoins. */
	return TRUE;
}

void *
pidgin_blist_get_handle() {
	static int handle;

	return &handle;
}

static gboolean buddy_signonoff_timeout_cb(PurpleBuddy *buddy)
{
	PidginBlistNode *gtknode = g_object_get_data(G_OBJECT(buddy), UI_DATA);

	gtknode->recent_signonoff = FALSE;
	gtknode->recent_signonoff_timer = 0;

	pidgin_blist_update(NULL, PURPLE_BLIST_NODE(buddy));

	return FALSE;
}

static void
buddy_signonoff_cb(PurpleBuddy *buddy) {
	PidginBlistNode *gtknode = g_object_get_data(G_OBJECT(buddy), UI_DATA);

	if(gtknode == NULL) {
		pidgin_blist_new_node(purple_blist_get_default(),
		                      PURPLE_BLIST_NODE(buddy));

		gtknode = g_object_get_data(G_OBJECT(buddy), UI_DATA);
	}

	gtknode->recent_signonoff = TRUE;

	if(gtknode->recent_signonoff_timer > 0)
		g_source_remove(gtknode->recent_signonoff_timer);

	g_object_ref(buddy);
	gtknode->recent_signonoff_timer = g_timeout_add_seconds_full(
			G_PRIORITY_DEFAULT, 10,
			(GSourceFunc)buddy_signonoff_timeout_cb,
			buddy, g_object_unref);
}

void pidgin_blist_init(void)
{
	void *gtk_blist_handle = pidgin_blist_get_handle();

	cached_emblems = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	/* Initialize prefs */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/blist");
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/blist/show_buddy_icons", TRUE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/blist/show_empty_groups", FALSE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/blist/show_idle_time", TRUE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/blist/show_offline_buddies", FALSE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/blist/show_protocol_icons", FALSE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/blist/list_visible", FALSE);
	purple_prefs_add_bool(PIDGIN_PREFS_ROOT "/blist/list_maximized", FALSE);
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/blist/sort_type", "alphabetical");
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/blist/width", 250); /* Golden ratio, baby */
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/blist/height", 405); /* Golden ratio, baby */
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/blist/theme", "");

	/* Register our signals */
	purple_signal_register(gtk_blist_handle, "gtkblist-hiding",
	                     purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
	                     PURPLE_TYPE_BUDDY_LIST);

	purple_signal_register(gtk_blist_handle, "gtkblist-unhiding",
	                     purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
	                     PURPLE_TYPE_BUDDY_LIST);

	purple_signal_register(gtk_blist_handle, "gtkblist-created",
	                     purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
	                     PURPLE_TYPE_BUDDY_LIST);

	purple_signal_register(gtk_blist_handle, "drawing-tooltip",
	                     purple_marshal_VOID__POINTER_POINTER_UINT, G_TYPE_NONE,
	                     3, PURPLE_TYPE_BLIST_NODE,
	                     G_TYPE_POINTER, /* pointer to a (GString *) */
	                     G_TYPE_BOOLEAN);

	purple_signal_register(gtk_blist_handle, "drawing-buddy",
						purple_marshal_POINTER__POINTER,
						G_TYPE_STRING, 1, PURPLE_TYPE_BUDDY);

	purple_signal_connect(purple_blist_get_handle(), "buddy-signed-on",
			gtk_blist_handle, PURPLE_CALLBACK(buddy_signonoff_cb), NULL);
	purple_signal_connect(purple_blist_get_handle(), "buddy-signed-off",
			gtk_blist_handle, PURPLE_CALLBACK(buddy_signonoff_cb), NULL);
	purple_signal_connect(purple_blist_get_handle(), "buddy-privacy-changed",
			gtk_blist_handle, PURPLE_CALLBACK(pidgin_blist_update_privacy_cb), NULL);

	purple_signal_connect_priority(purple_connections_get_handle(), "autojoin",
	                               gtk_blist_handle, PURPLE_CALLBACK(autojoin_cb),
	                               NULL, PURPLE_SIGNAL_PRIORITY_HIGHEST);
}

void
pidgin_blist_uninit(void) {
	g_hash_table_destroy(cached_emblems);

	purple_signals_unregister_by_instance(pidgin_blist_get_handle());
	purple_signals_disconnect_by_handle(pidgin_blist_get_handle());

	gtkblist = NULL;
}

/**************************************************************************
 * GTK Buddy list GObject code
 **************************************************************************/
static void
pidgin_buddy_list_init(PidginBuddyList *self)
{
}

static void
pidgin_buddy_list_finalize(GObject *obj)
{
	PidginBuddyList *gtkblist = PIDGIN_BUDDY_LIST(obj);
	PidginBuddyListPrivate *priv =
	        pidgin_buddy_list_get_instance_private(gtkblist);

	purple_signals_disconnect_by_handle(gtkblist);

	gtk_widget_destroy(gtkblist->window);

	if (gtkblist->refresh_timer) {
		g_source_remove(gtkblist->refresh_timer);
		gtkblist->refresh_timer = 0;
	}
	if (gtkblist->drag_timeout) {
		g_source_remove(gtkblist->drag_timeout);
		gtkblist->drag_timeout = 0;
	}

	gtkblist->window = gtkblist->vbox = gtkblist->treeview = NULL;
	g_clear_object(&gtkblist->treemodel);
	g_object_unref(G_OBJECT(gtkblist->empty_avatar));

	if (priv->select_notebook_page_timeout) {
		g_source_remove(priv->select_notebook_page_timeout);
	}

	purple_prefs_disconnect_by_handle(pidgin_blist_get_handle());

	G_OBJECT_CLASS(pidgin_buddy_list_parent_class)->finalize(obj);
}

static void
pidgin_buddy_list_class_init(PidginBuddyListClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	PurpleBuddyListClass *purple_blist_class;

	obj_class->finalize = pidgin_buddy_list_finalize;

	purple_blist_class = PURPLE_BUDDY_LIST_CLASS(klass);
	purple_blist_class->new_node = pidgin_blist_new_node;
	purple_blist_class->show = pidgin_blist_show;
	purple_blist_class->update = pidgin_blist_update;
	purple_blist_class->remove = pidgin_blist_remove;
	purple_blist_class->set_visible = pidgin_blist_set_visible;
	purple_blist_class->request_add_buddy = pidgin_blist_request_add_buddy;
	purple_blist_class->request_add_chat = pidgin_blist_request_add_chat;
	purple_blist_class->request_add_group = pidgin_blist_request_add_group;
}

/*********************************************************************
 * Buddy List sorting functions                                      *
 *********************************************************************/

GList *pidgin_blist_get_sort_methods()
{
	return pidgin_blist_sort_methods;
}

void pidgin_blist_sort_method_reg(const char *id, const char *name, pidgin_blist_sort_function func)
{
	struct _PidginBlistSortMethod *method;

	g_return_if_fail(id != NULL);
	g_return_if_fail(name != NULL);
	g_return_if_fail(func != NULL);

	method = g_new0(struct _PidginBlistSortMethod, 1);
	method->id = g_strdup(id);
	method->name = g_strdup(name);
	method->func = func;
	pidgin_blist_sort_methods = g_list_append(pidgin_blist_sort_methods, method);
	pidgin_blist_update_sort_methods();
}

void pidgin_blist_sort_method_unreg(const char *id)
{
	GList *l = pidgin_blist_sort_methods;

	g_return_if_fail(id != NULL);

	while(l) {
		struct _PidginBlistSortMethod *method = l->data;
		if(purple_strequal(method->id, id)) {
			pidgin_blist_sort_methods = g_list_delete_link(pidgin_blist_sort_methods, l);
			g_free(method->id);
			g_free(method->name);
			g_free(method);
			break;
		}
		l = l->next;
	}
	pidgin_blist_update_sort_methods();
}

void pidgin_blist_sort_method_set(const char *id){
	GList *l = pidgin_blist_sort_methods;

	if(!id)
		id = "none";

	while (l && !purple_strequal(((struct _PidginBlistSortMethod*)l->data)->id, id))
		l = l->next;

	if (l) {
		current_sort_method = l->data;
	} else if (!current_sort_method) {
		pidgin_blist_sort_method_set("none");
		return;
	}
	if (purple_strequal(id, "none")) {
		redo_buddy_list(purple_blist_get_default(), TRUE, FALSE);
	} else {
		redo_buddy_list(purple_blist_get_default(), FALSE, FALSE);
	}
}

/******************************************
 ** Sort Methods
 ******************************************/

static void sort_method_none(PurpleBlistNode *node, PurpleBuddyList *blist, GtkTreeIter parent_iter, GtkTreeIter *cur, GtkTreeIter *iter)
{
	PurpleBlistNode *sibling = node->prev;
	GtkTreeIter sibling_iter;

	if (cur != NULL) {
		*iter = *cur;
		return;
	}

	while (sibling && !get_iter_from_node(sibling, &sibling_iter)) {
		sibling = sibling->prev;
	}

	gtk_tree_store_insert_after(gtkblist->treemodel, iter,
			node->parent ? &parent_iter : NULL,
			sibling ? &sibling_iter : NULL);
}

static void sort_method_alphabetical(PurpleBlistNode *node, PurpleBuddyList *blist, GtkTreeIter groupiter, GtkTreeIter *cur, GtkTreeIter *iter)
{
	GtkTreeIter more_z;

	const char *my_name;

	if(PURPLE_IS_CONTACT(node)) {
		my_name = purple_contact_get_alias((PurpleContact*)node);
	} else if(PURPLE_IS_CHAT(node)) {
		my_name = purple_chat_get_name((PurpleChat*)node);
	} else {
		sort_method_none(node, blist, groupiter, cur, iter);
		return;
	}

	if (!gtk_tree_model_iter_children(GTK_TREE_MODEL(gtkblist->treemodel), &more_z, &groupiter)) {
		gtk_tree_store_insert(gtkblist->treemodel, iter, &groupiter, 0);
		return;
	}

	do {
		PurpleBlistNode *n;
		const char *this_name;
		int cmp;

		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &more_z, NODE_COLUMN, &n, -1);

		if(PURPLE_IS_CONTACT(n)) {
			this_name = purple_contact_get_alias((PurpleContact*)n);
		} else if(PURPLE_IS_CHAT(n)) {
			this_name = purple_chat_get_name((PurpleChat*)n);
		} else {
			this_name = NULL;
		}

		cmp = purple_utf8_strcasecmp(my_name, this_name);

		if(this_name && (cmp < 0 || (cmp == 0 && node < n))) {
			if(cur) {
				gtk_tree_store_move_before(gtkblist->treemodel, cur, &more_z);
				*iter = *cur;
				return;
			} else {
				gtk_tree_store_insert_before(gtkblist->treemodel, iter,
						&groupiter, &more_z);
				return;
			}
		}
	} while (gtk_tree_model_iter_next (GTK_TREE_MODEL(gtkblist->treemodel), &more_z));

	if(cur) {
		gtk_tree_store_move_before(gtkblist->treemodel, cur, NULL);
		*iter = *cur;
		return;
	} else {
		gtk_tree_store_append(gtkblist->treemodel, iter, &groupiter);
		return;
	}
}

static void sort_method_status(PurpleBlistNode *node, PurpleBuddyList *blist, GtkTreeIter groupiter, GtkTreeIter *cur, GtkTreeIter *iter)
{
	GtkTreeIter more_z;

	PurpleBuddy *my_buddy, *this_buddy;

	if(PURPLE_IS_CONTACT(node)) {
		my_buddy = purple_contact_get_priority_buddy((PurpleContact*)node);
	} else if(PURPLE_IS_CHAT(node)) {
		if (cur != NULL) {
			*iter = *cur;
			return;
		}

		gtk_tree_store_append(gtkblist->treemodel, iter, &groupiter);
		return;
	} else {
		sort_method_alphabetical(node, blist, groupiter, cur, iter);
		return;
	}


	if (!gtk_tree_model_iter_children(GTK_TREE_MODEL(gtkblist->treemodel), &more_z, &groupiter)) {
		gtk_tree_store_insert(gtkblist->treemodel, iter, &groupiter, 0);
		return;
	}

	do {
		PurpleBlistNode *n;
		gint name_cmp;
		gint presence_cmp;

		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &more_z, NODE_COLUMN, &n, -1);

		if(PURPLE_IS_CONTACT(n)) {
			this_buddy = purple_contact_get_priority_buddy((PurpleContact*)n);
		} else {
			this_buddy = NULL;
		}

		name_cmp = purple_utf8_strcasecmp(
			purple_contact_get_alias(purple_buddy_get_contact(my_buddy)),
			(this_buddy
			 ? purple_contact_get_alias(purple_buddy_get_contact(this_buddy))
			 : NULL));

		presence_cmp = purple_buddy_presence_compare(
			PURPLE_BUDDY_PRESENCE(purple_buddy_get_presence(my_buddy)),
			this_buddy ? PURPLE_BUDDY_PRESENCE(purple_buddy_get_presence(this_buddy)) : NULL);

		if (this_buddy == NULL ||
			(presence_cmp < 0 ||
			 (presence_cmp == 0 &&
			  (name_cmp < 0 || (name_cmp == 0 && node < n)))))
		{
			if (cur != NULL)
			{
				gtk_tree_store_move_before(gtkblist->treemodel, cur, &more_z);
				*iter = *cur;
				return;
			}
			else
			{
				gtk_tree_store_insert_before(gtkblist->treemodel, iter,
											 &groupiter, &more_z);
				return;
			}
		}
	}
	while (gtk_tree_model_iter_next(GTK_TREE_MODEL(gtkblist->treemodel),
									&more_z));

	if (cur) {
		gtk_tree_store_move_before(gtkblist->treemodel, cur, NULL);
		*iter = *cur;
		return;
	} else {
		gtk_tree_store_append(gtkblist->treemodel, iter, &groupiter);
		return;
	}
}

static void sort_method_log_activity(PurpleBlistNode *node, PurpleBuddyList *blist, GtkTreeIter groupiter, GtkTreeIter *cur, GtkTreeIter *iter)
{
	GtkTreeIter more_z;

	int activity_score = 0, this_log_activity_score = 0;
	const char *buddy_name, *this_buddy_name;

	if(cur && (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(gtkblist->treemodel), &groupiter) == 1)) {
		*iter = *cur;
		return;
	}

	if(PURPLE_IS_CONTACT(node)) {
		PurpleBlistNode *n;
		PurpleBuddy *buddy;
		for (n = node->child; n; n = n->next) {
			buddy = (PurpleBuddy*)n;
			activity_score += purple_log_get_activity_score(PURPLE_LOG_IM, purple_buddy_get_name(buddy), purple_buddy_get_account(buddy));
		}
		buddy_name = purple_contact_get_alias((PurpleContact*)node);
	} else if(PURPLE_IS_CHAT(node)) {
		/* we don't have a reliable way of getting the log filename
		 * from the chat info in the blist, yet */
		if (cur != NULL) {
			*iter = *cur;
			return;
		}

		gtk_tree_store_append(gtkblist->treemodel, iter, &groupiter);
		return;
	} else {
		sort_method_none(node, blist, groupiter, cur, iter);
		return;
	}


	if (!gtk_tree_model_iter_children(GTK_TREE_MODEL(gtkblist->treemodel), &more_z, &groupiter)) {
		gtk_tree_store_insert(gtkblist->treemodel, iter, &groupiter, 0);
		return;
	}

	do {
		PurpleBlistNode *n;
		PurpleBlistNode *n2;
		PurpleBuddy *buddy;
		int cmp;

		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &more_z, NODE_COLUMN, &n, -1);
		this_log_activity_score = 0;

		if(PURPLE_IS_CONTACT(n)) {
			for (n2 = n->child; n2; n2 = n2->next) {
				buddy = (PurpleBuddy*)n2;
				this_log_activity_score += purple_log_get_activity_score(PURPLE_LOG_IM, purple_buddy_get_name(buddy), purple_buddy_get_account(buddy));
			}
			this_buddy_name = purple_contact_get_alias((PurpleContact*)n);
		} else {
			this_buddy_name = NULL;
		}

		cmp = purple_utf8_strcasecmp(buddy_name, this_buddy_name);

		if (!PURPLE_IS_CONTACT(n) || activity_score > this_log_activity_score ||
				((activity_score == this_log_activity_score) &&
				 (cmp < 0 || (cmp == 0 && node < n)))) {
			if (cur != NULL) {
				gtk_tree_store_move_before(gtkblist->treemodel, cur, &more_z);
				*iter = *cur;
				return;
			} else {
				gtk_tree_store_insert_before(gtkblist->treemodel, iter,
						&groupiter, &more_z);
				return;
			}
		}
	} while (gtk_tree_model_iter_next (GTK_TREE_MODEL(gtkblist->treemodel), &more_z));

	if (cur != NULL) {
		gtk_tree_store_move_before(gtkblist->treemodel, cur, NULL);
		*iter = *cur;
		return;
	} else {
		gtk_tree_store_append(gtkblist->treemodel, iter, &groupiter);
		return;
	}
}

void
pidgin_blist_update_sort_methods(void)
{
	GMenu *menu = NULL;
	GList *l;

	if(gtkblist == NULL)
		return;

	/* create the gmenu */
	menu = g_menu_new();

	/* walk through the sort methods and update all the things */
	for (l = pidgin_blist_sort_methods; l; l = l->next) {
		PidginBlistSortMethod *method = NULL;
		GMenuItem *item = NULL;
		gchar *action = NULL;

		method = (PidginBlistSortMethod *)l->data;

		action = g_action_print_detailed_name("blist.sort-method",
		                                      g_variant_new_string(method->id));
		item = g_menu_item_new(method->name, action);
		g_free(action);

		g_menu_append_item(menu, item);
	}

	/* replace the old submenu with a new one */
	if(PIDGIN_IS_CONTACT_LIST(gtkblist->window)) {
		GtkWidget *item = NULL;

		item = pidgin_contact_list_get_menu_sort_item(PIDGIN_CONTACT_LIST(gtkblist->window));
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(item),
		                          gtk_menu_new_from_model(G_MENU_MODEL(menu)));
		g_object_unref(G_OBJECT(menu));
	}
}
