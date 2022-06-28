/*
 * Purple - Internet Messaging Library
 * Copyright (C) Pidgin Developers <devel@pidgin.im>
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
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#include "purpleprotocolactions.h"

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
G_DEFINE_INTERFACE(PurpleProtocolActions, purple_protocol_actions,
                   PURPLE_TYPE_PROTOCOL)

static void
purple_protocol_actions_default_init(PurpleProtocolActionsInterface *iface) {
}

/******************************************************************************
 * Public API
 *****************************************************************************/
GActionGroup *
purple_protocol_actions_get_action_group(PurpleProtocolActions *actions,
                                         PurpleConnection *connection)
{
	PurpleProtocolActionsInterface *iface = NULL;

	g_return_val_if_fail(PURPLE_IS_PROTOCOL_ACTIONS(actions), NULL);
	g_return_val_if_fail(PURPLE_IS_CONNECTION(connection), NULL);

	iface = PURPLE_PROTOCOL_ACTIONS_GET_IFACE(actions);
	if(iface != NULL && iface->get_action_group != NULL) {
		return iface->get_action_group(actions, connection);
	}

	return NULL;
}

GMenu *
purple_protocol_actions_get_menu(PurpleProtocolActions *actions)
{
	PurpleProtocolActionsInterface *iface = NULL;

	g_return_val_if_fail(PURPLE_IS_PROTOCOL_ACTIONS(actions), NULL);

	iface = PURPLE_PROTOCOL_ACTIONS_GET_IFACE(actions);
	if(iface != NULL && iface->get_menu != NULL) {
		return iface->get_menu(actions);
	}

	return NULL;
}
