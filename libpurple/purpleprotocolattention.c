/*
 * purple
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

#include "purpleprotocolattention.h"

G_DEFINE_INTERFACE(PurpleProtocolAttention, purple_protocol_attention,
                   G_TYPE_INVALID)

/******************************************************************************
 * GInterface Implementation
 *****************************************************************************/
static void
purple_protocol_attention_default_init(PurpleProtocolAttentionInterface *iface) {
}

/******************************************************************************
 * Public API
 *****************************************************************************/
gboolean
purple_protocol_attention_send_attention(PurpleProtocolAttention *attention,
                                         PurpleConnection *gc,
                                         const gchar *username, guint type)
{
	PurpleProtocolAttentionInterface *iface = NULL;

	g_return_val_if_fail(PURPLE_IS_PROTOCOL_ATTENTION(attention), FALSE);

	iface = PURPLE_PROTOCOL_ATTENTION_GET_IFACE(attention);
	if(iface && iface->send_attention) {
		return iface->send_attention(attention, gc, username, type);
	}

	return FALSE;
}

GList *
purple_protocol_attention_get_types(PurpleProtocolAttention *attention,
                                    PurpleAccount *account)
{
	PurpleProtocolAttentionInterface *iface = NULL;

	g_return_val_if_fail(PURPLE_IS_PROTOCOL_ATTENTION(attention), NULL);

	iface = PURPLE_PROTOCOL_ATTENTION_GET_IFACE(attention);
	if(iface && iface->get_types) {
		return iface->get_types(attention, account);
	}

	return NULL;
}

