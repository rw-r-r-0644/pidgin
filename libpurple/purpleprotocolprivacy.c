/*
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
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#include "purpleprotocolprivacy.h"

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
G_DEFINE_INTERFACE(PurpleProtocolPrivacy, purple_protocol_privacy,
                   PURPLE_TYPE_PROTOCOL)

static void
purple_protocol_privacy_default_init(PurpleProtocolPrivacyInterface *iface) {
}

/******************************************************************************
 * Public API
 *****************************************************************************/
void
purple_protocol_privacy_add_permit(PurpleProtocolPrivacy *privacy,
                                   PurpleConnection *connection,
                                   const gchar *name)
{
	PurpleProtocolPrivacyInterface *iface = NULL;

	g_return_if_fail(PURPLE_IS_PROTOCOL_PRIVACY(privacy));
	g_return_if_fail(PURPLE_IS_CONNECTION(connection));

	iface = PURPLE_PROTOCOL_PRIVACY_GET_IFACE(privacy);
	if(iface && iface->add_permit) {
		iface->add_permit(privacy, connection, name);
	}
}

void
purple_protocol_privacy_add_deny(PurpleProtocolPrivacy *privacy,
                                 PurpleConnection *connection,
                                 const gchar *name)
{
	PurpleProtocolPrivacyInterface *iface = NULL;

	g_return_if_fail(PURPLE_IS_PROTOCOL_PRIVACY(privacy));
	g_return_if_fail(PURPLE_IS_CONNECTION(connection));

	iface = PURPLE_PROTOCOL_PRIVACY_GET_IFACE(privacy);
	if(iface && iface->add_deny) {
		iface->add_deny(privacy, connection, name);
	}
}

void
purple_protocol_privacy_remove_permit(PurpleProtocolPrivacy *privacy,
                                      PurpleConnection *connection,
                                      const gchar *name)
{
	PurpleProtocolPrivacyInterface *iface = NULL;

	g_return_if_fail(PURPLE_IS_PROTOCOL_PRIVACY(privacy));
	g_return_if_fail(PURPLE_IS_CONNECTION(connection));

	iface = PURPLE_PROTOCOL_PRIVACY_GET_IFACE(privacy);
	if(iface && iface->remove_permit) {
		iface->remove_permit(privacy, connection, name);
	}
}

void
purple_protocol_privacy_remove_deny(PurpleProtocolPrivacy *privacy,
                                    PurpleConnection *connection,
                                    const gchar *name)
{
	PurpleProtocolPrivacyInterface *iface = NULL;

	g_return_if_fail(PURPLE_IS_PROTOCOL_PRIVACY(privacy));
	g_return_if_fail(PURPLE_IS_CONNECTION(connection));

	iface = PURPLE_PROTOCOL_PRIVACY_GET_IFACE(privacy);
	if(iface && iface->remove_deny) {
		iface->remove_deny(privacy, connection, name);
	}
}

void
purple_protocol_privacy_set_permit_deny(PurpleProtocolPrivacy *privacy,
                                        PurpleConnection *connection)
{
	PurpleProtocolPrivacyInterface *iface = NULL;

	g_return_if_fail(PURPLE_IS_PROTOCOL_PRIVACY(privacy));
	g_return_if_fail(PURPLE_IS_CONNECTION(connection));

	iface = PURPLE_PROTOCOL_PRIVACY_GET_IFACE(privacy);
	if(iface && iface->set_permit_deny) {
		iface->set_permit_deny(privacy, connection);
	}
}
