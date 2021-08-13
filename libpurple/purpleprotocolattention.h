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

#if !defined(PURPLE_GLOBAL_HEADER_INSIDE) && !defined(PURPLE_COMPILATION)
# error "only <purple.h> may be included directly"
#endif

#ifndef PURPLE_ATTENTION_H
#define PURPLE_ATTENTION_H

#include <glib.h>
#include <glib-object.h>

/**
 * SECTION:purpleprotocolattention
 * @section_id: libpurple-purpleprotocolattention
 * @title: Protocol Attention Interface
 * @short_description: Protocol interface for attention.
 *
 * @PurpleProtocolAttention is an interface that PurpleProtocols can implement
 * to support getting a contact's attention.
 */

#include <libpurple/account.h>
#include <libpurple/connection.h>
#include <libpurple/protocol.h>

G_BEGIN_DECLS

/**
 * PURPLE_TYPE_PROTOCOL_ATTENTION:
 *
 * The standard _get_type macro for #PurpleProtocolAttention.
 *
 * Since: 3.0.0
 */
#define PURPLE_TYPE_PROTOCOL_ATTENTION (purple_protocol_attention_get_type())

/**
 * purple_protocol_attention_get_type:
 *
 * Gets the #GType for #PurpleProtocolAttentionInterface.
 *
 * Returns: The #GType for the protocol attention interface.
 *
 * Since: 3.0.0
 */

/**
 * PurpleProtocolAttention:
 *
 * An opaque data structure used to represent an object that implements the
 * #PurpleProtocolAttention interface.
 *
 * Since: 3.0.0
 */
G_DECLARE_INTERFACE(PurpleProtocolAttention, purple_protocol_attention, PURPLE,
                    PROTOCOL_ATTENTION, PurpleProtocol)

/**
 * PurpleProtocolAttentionInterface:
 * @send_attention: Called to send an attention message. See
 *                  purple_protocol_attention_send_attention().
 * @get_types: Called to list the protocol's attention types. See
 *             purple_protocol_attention_get_types().
 *
 * The protocol attention interface.
 *
 * This interface provides attention API for sending and receiving
 * zaps/nudges/buzzes etc.
 *
 * Since: 3.0.0
 */
struct _PurpleProtocolAttentionInterface {
	/*< private >*/
	GTypeInterface parent;

	/*< public >*/
	gboolean (*send_attention)(PurpleProtocolAttention *attention,
	                           PurpleConnection *pc, const gchar *username,
	                           guint type);

	GList *(*get_types)(PurpleProtocolAttention *attention,
	                    PurpleAccount *account);

	/*< private >*/
	gpointer reserved[4];
};

/**
 * purple_protocol_attention_get_types:
 * @attention: The #PurpleProtocolAttention.
 * @account: The #PurpleAccount whose attention types to get.
 *
 * Returns a list of #PurpleAttentionType's for @attention.
 *
 * Returns: (transfer container) (element-type PurpleAttentionType): The list of
 *          #PurpleAttentionType's.
 *
 * Since: 3.0.0
 */
GList *purple_protocol_attention_get_types(PurpleProtocolAttention *attention, PurpleAccount *account);

/**
 * purple_protocol_attention_send_attention:
 * @attention: The #PurpleProtocolAttention instance.
 * @pc: The #PurpleConnection to send on
 * @username: The name of the user to send the attention to.
 * @type: The type of attention to send.
 *
 * Sends an attention message of @type to @username.
 *
 * Returns: %TRUE on success, %FALSE otherwise.
 *
 * Since: 3.0.0
 */
gboolean purple_protocol_attention_send_attention(PurpleProtocolAttention *attention, PurpleConnection *pc, const gchar *username, guint type);

G_END_DECLS

#endif /* PURPLE_PROTOCOL_ATTENTION_H */
