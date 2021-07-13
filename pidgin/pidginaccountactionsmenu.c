/*
 * Pidgin - Internet Messenger
 * Copyright (C) Pidgin Developers <devel@pidgin.im>
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
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#include "pidginaccountactionsmenu.h"

#include <purple.h>

#include "gtkaccount.h"
#include "pidgincore.h"
#include "pidginmooddialog.h"

struct _PidginAccountActionsMenu {
	GtkMenu parent;

	GtkWidget *separator;
	GtkWidget *set_mood;

	PurpleAccount *account;
};

enum {
	PROP_ZERO,
	PROP_ACCOUNT,
	N_PROPERTIES
};
static GParamSpec *properties[N_PROPERTIES] = { NULL, };

/******************************************************************************
 * GSignal Handlers
 *****************************************************************************/
static void
pidgin_account_actions_menu_edit_cb(GtkMenuItem *item, gpointer data) {
	PidginAccountActionsMenu *menu = PIDGIN_ACCOUNT_ACTIONS_MENU(data);

	pidgin_account_dialog_show(PIDGIN_MODIFY_ACCOUNT_DIALOG, menu->account);
}

static void
pidgin_account_actions_menu_disable_cb(GtkMenuItem *item, gpointer data) {
	PidginAccountActionsMenu *menu = PIDGIN_ACCOUNT_ACTIONS_MENU(data);

	purple_account_set_enabled(menu->account, PIDGIN_UI, FALSE);
}

static void
pidgin_account_actions_menu_set_mood_cb(GtkMenuItem *item, gpointer data) {
	PidginAccountActionsMenu *menu = PIDGIN_ACCOUNT_ACTIONS_MENU(data);

	pidgin_mood_dialog_show(menu->account);
}

static void
pidgin_account_actions_menu_action(GtkMenuItem *item, gpointer data) {
	PurpleProtocolAction *action = (PurpleProtocolAction *)data;

	if(action && action->callback) {
		action->callback(action);
	}
}

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
pidgin_account_actions_menu_set_account(PidginAccountActionsMenu *menu,
                                        PurpleAccount *account)
{
	PurpleConnection *connection = NULL;
	PurpleProtocol *protocol = NULL;
	GList *children = NULL;
	gboolean show_separator = FALSE;
	gint position = 0;

	if(g_set_object(&menu->account, account)) {
		if(!PURPLE_IS_ACCOUNT(menu->account)) {
			return;
		}
	}

	connection = purple_account_get_connection(account);
	if(connection == NULL) {
		return;
	}

	if(!PURPLE_CONNECTION_IS_CONNECTED(connection)) {
		return;
	}

	/* we're pretty sure we're going to insert some items into the menu, so we
	 * need to figure out where to put them.  GtkMenu stores its children in
	 * order, so we just need to get the index of the set_mood item to find the
	 * proper position.
	 */
	children = gtk_container_get_children(GTK_CONTAINER(menu));
	position = g_list_index(children, menu->set_mood) + 1;
	g_list_free(children);

	protocol = purple_connection_get_protocol(connection);

	if(PURPLE_PROTOCOL_IMPLEMENTS(protocol, CLIENT, get_moods)) {
		gtk_widget_show(menu->set_mood);

		show_separator = TRUE;
	}

	if(PURPLE_IS_PROTOCOL_CLIENT(protocol)) {
		PurpleProtocolClient *client = PURPLE_PROTOCOL_CLIENT(protocol);
		GtkWidget *item = NULL;
		GList *actions = NULL;

		actions = purple_protocol_client_get_actions(client, connection);

		while(actions != NULL) {
			PurpleProtocolAction *action = (PurpleProtocolAction *)actions->data;

			if(action == NULL) {
				item = gtk_separator_menu_item_new();
				gtk_menu_shell_insert(GTK_MENU_SHELL(menu), item, position++);
				gtk_widget_show(item);

				continue;
			}

			if(action->label == NULL) {
				purple_protocol_action_free(action);

				continue;
			}

			/* Make sure the action has its connection set. */
			action->connection = connection;

			/* now add the action */
			item = gtk_menu_item_new_with_label(action->label);
			g_signal_connect_data(G_OBJECT(item), "activate",
			                      G_CALLBACK(pidgin_account_actions_menu_action),
			                      action,
			                      (GClosureNotify)purple_protocol_action_free,
			                      0);
			gtk_menu_shell_insert(GTK_MENU_SHELL(menu), item, position++);
			gtk_widget_show(item);

			/* since we added an item, make sure items_added is true */
			show_separator = TRUE;

			/* Iterate to the next item while deleting the one we just
			 * processed.
			 */
			actions = g_list_delete_link(actions, actions);
		}
	}

	/* if we added any items, make our separator visible. */
	if(show_separator) {
		gtk_widget_show(menu->separator);
	}
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
G_DEFINE_TYPE(PidginAccountActionsMenu, pidgin_account_actions_menu,
              GTK_TYPE_MENU)

static void
pidgin_account_actions_menu_get_property(GObject *obj, guint param_id,
                                         GValue *value, GParamSpec *pspec)
{
	PidginAccountActionsMenu *menu = PIDGIN_ACCOUNT_ACTIONS_MENU(obj);

	switch(param_id) {
		case PROP_ACCOUNT:
			g_value_set_object(value,
			                   pidgin_account_actions_menu_get_account(menu));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
pidgin_account_actions_menu_set_property(GObject *obj, guint param_id,
                                         const GValue *value,
                                         GParamSpec *pspec)
{
	PidginAccountActionsMenu *menu = PIDGIN_ACCOUNT_ACTIONS_MENU(obj);

	switch(param_id) {
		case PROP_ACCOUNT:
			pidgin_account_actions_menu_set_account(menu,
			                                        g_value_get_object(value));
			break;

		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
pidgin_account_actions_menu_finalize(GObject *obj) {
	PidginAccountActionsMenu *menu = PIDGIN_ACCOUNT_ACTIONS_MENU(obj);

	g_clear_object(&menu->account);

	G_OBJECT_CLASS(pidgin_account_actions_menu_parent_class)->finalize(obj);
}

static void
pidgin_account_actions_menu_init(PidginAccountActionsMenu *menu) {
	/* initialize our template */
	gtk_widget_init_template(GTK_WIDGET(menu));
};

static void
pidgin_account_actions_menu_class_init(PidginAccountActionsMenuClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(
	    widget_class,
	    "/im/pidgin/Pidgin/Accounts/actionsmenu.ui"
	);

	obj_class->get_property = pidgin_account_actions_menu_get_property;
	obj_class->set_property = pidgin_account_actions_menu_set_property;
	obj_class->finalize = pidgin_account_actions_menu_finalize;

	/**
	 * PidginAccountActionsMenu::account:
	 *
	 * The #PurpleAccount that this menu was created for.
	 */
	properties[PROP_ACCOUNT] =
		g_param_spec_object("account", "account",
		                    "The account this menu is for",
		                    PURPLE_TYPE_ACCOUNT,
		                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
		                    G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, N_PROPERTIES, properties);

	gtk_widget_class_bind_template_child(widget_class, PidginAccountActionsMenu,
	                                     separator);
	gtk_widget_class_bind_template_child(widget_class, PidginAccountActionsMenu,
	                                     set_mood);

	gtk_widget_class_bind_template_callback(widget_class,
	                                        pidgin_account_actions_menu_edit_cb);
	gtk_widget_class_bind_template_callback(widget_class,
	                                        pidgin_account_actions_menu_disable_cb);
	gtk_widget_class_bind_template_callback(widget_class,
	                                        pidgin_account_actions_menu_set_mood_cb);
}

/******************************************************************************
 * Public API
 *****************************************************************************/
GtkWidget *
pidgin_account_actions_menu_new(PurpleAccount *account) {
	GObject *obj = NULL;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);

	obj = g_object_new(PIDGIN_TYPE_ACCOUNT_ACTIONS_MENU,
	                   "account", account,
	                   NULL);

	return GTK_WIDGET(obj);
}

PurpleAccount *
pidgin_account_actions_menu_get_account(PidginAccountActionsMenu *menu) {
	g_return_val_if_fail(PIDGIN_IS_ACCOUNT_ACTIONS_MENU(menu), NULL);

	return menu->account;
}
