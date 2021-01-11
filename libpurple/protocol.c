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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */
#include "protocol.h"

static GObjectClass *parent_class;

/**************************************************************************
 * Protocol Object API
 **************************************************************************/
const char *
purple_protocol_get_id(const PurpleProtocol *protocol)
{
	g_return_val_if_fail(PURPLE_IS_PROTOCOL(protocol), NULL);

	return protocol->id;
}

const char *
purple_protocol_get_name(const PurpleProtocol *protocol)
{
	g_return_val_if_fail(PURPLE_IS_PROTOCOL(protocol), NULL);

	return protocol->name;
}

PurpleProtocolOptions
purple_protocol_get_options(const PurpleProtocol *protocol)
{
	g_return_val_if_fail(PURPLE_IS_PROTOCOL(protocol), 0);

	return protocol->options;
}

GList *
purple_protocol_get_user_splits(const PurpleProtocol *protocol)
{
	g_return_val_if_fail(PURPLE_IS_PROTOCOL(protocol), NULL);

	return protocol->user_splits;
}

GList *
purple_protocol_get_account_options(const PurpleProtocol *protocol)
{
	g_return_val_if_fail(PURPLE_IS_PROTOCOL(protocol), NULL);

	return protocol->account_options;
}

PurpleBuddyIconSpec *
purple_protocol_get_icon_spec(const PurpleProtocol *protocol)
{
	g_return_val_if_fail(PURPLE_IS_PROTOCOL(protocol), NULL);

	return protocol->icon_spec;
}

PurpleWhiteboardOps *
purple_protocol_get_whiteboard_ops(const PurpleProtocol *protocol)
{
	g_return_val_if_fail(PURPLE_IS_PROTOCOL(protocol), NULL);

	return protocol->whiteboard_ops;
}

static void
icon_spec_free(PurpleProtocol *protocol)
{
	g_return_if_fail(PURPLE_IS_PROTOCOL(protocol));

	g_free(protocol->icon_spec);
	protocol->icon_spec = NULL;
}

/**************************************************************************
 * GObject stuff
 **************************************************************************/
static void
purple_protocol_init(GTypeInstance *instance, gpointer klass)
{
}

static void
purple_protocol_finalize(GObject *object)
{
	PurpleProtocol *protocol = PURPLE_PROTOCOL(object);
	GList *accounts, *l;

	accounts = purple_accounts_get_all_active();
	for (l = accounts; l != NULL; l = l->next) {
		PurpleAccount *account = PURPLE_ACCOUNT(l->data);
		if (purple_account_is_disconnected(account))
			continue;

		if (purple_strequal(protocol->id,
				purple_account_get_protocol_id(account)))
			purple_account_disconnect(account);
	}

	g_list_free(accounts);

	purple_request_close_with_handle(protocol);
	purple_notify_close_with_handle(protocol);

	purple_signals_disconnect_by_handle(protocol);
	purple_signals_unregister_by_instance(protocol);

	purple_prefs_disconnect_by_handle(protocol);

	g_list_free_full(protocol->user_splits, (GDestroyNotify)purple_account_user_split_destroy);
	g_list_free_full(protocol->account_options, (GDestroyNotify)purple_account_option_destroy);
	icon_spec_free(protocol);

	parent_class->finalize(object);
}

static void
purple_protocol_class_init(PurpleProtocolClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	obj_class->finalize = purple_protocol_finalize;
}

GType
purple_protocol_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurpleProtocolClass),
			.class_init = (GClassInitFunc)purple_protocol_class_init,
			.instance_size = sizeof(PurpleProtocol),
			.instance_init = (GInstanceInitFunc)purple_protocol_init,
		};

		type = g_type_register_static(G_TYPE_OBJECT, "PurpleProtocol",
		                              &info, G_TYPE_FLAG_ABSTRACT);
	}

	return type;
}

/**************************************************************************
 * Protocol Class API
 **************************************************************************/
#define DEFINE_PROTOCOL_FUNC(protocol,funcname,...) \
	PurpleProtocolClass *klass = PURPLE_PROTOCOL_GET_CLASS(protocol); \
	g_return_if_fail(klass != NULL); \
	if (klass->funcname) \
		klass->funcname(__VA_ARGS__);

#define DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol,defaultreturn,funcname,...) \
	PurpleProtocolClass *klass = PURPLE_PROTOCOL_GET_CLASS(protocol); \
	g_return_val_if_fail(klass != NULL, defaultreturn); \
	if (klass->funcname) \
		return klass->funcname(__VA_ARGS__); \
	else \
		return defaultreturn;

void
purple_protocol_class_login(PurpleProtocol *protocol, PurpleAccount *account)
{
	DEFINE_PROTOCOL_FUNC(protocol, login, account);
}

void
purple_protocol_class_close(PurpleProtocol *protocol, PurpleConnection *gc)
{
	DEFINE_PROTOCOL_FUNC(protocol, close, gc);
}

GList *
purple_protocol_class_status_types(PurpleProtocol *protocol,
		PurpleAccount *account)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, status_types, account);
}

const char *
purple_protocol_class_list_icon(PurpleProtocol *protocol,
		PurpleAccount *account, PurpleBuddy *buddy)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, NULL, list_icon, account, buddy);
}

#undef DEFINE_PROTOCOL_FUNC_WITH_RETURN
#undef DEFINE_PROTOCOL_FUNC

/**************************************************************************
 * Protocol Server Interface API
 **************************************************************************/
#define DEFINE_PROTOCOL_FUNC(protocol,funcname,...) \
	PurpleProtocolServerInterface *server_iface = \
		PURPLE_PROTOCOL_SERVER_GET_IFACE(protocol); \
	if (server_iface && server_iface->funcname) \
		server_iface->funcname(__VA_ARGS__);

#define DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol,defaultreturn,funcname,...) \
	PurpleProtocolServerInterface *server_iface = \
		PURPLE_PROTOCOL_SERVER_GET_IFACE(protocol); \
	if (server_iface && server_iface->funcname) \
		return server_iface->funcname(__VA_ARGS__); \
	else \
		return defaultreturn;

GType
purple_protocol_server_iface_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurpleProtocolServerInterface),
		};

		type = g_type_register_static(G_TYPE_INTERFACE,
				"PurpleProtocolServerInterface", &info, 0);
	}
	return type;
}

void
purple_protocol_server_iface_register_user(PurpleProtocol *protocol,
		PurpleAccount *account)
{
	DEFINE_PROTOCOL_FUNC(protocol, register_user, account);
}

void
purple_protocol_server_iface_unregister_user(PurpleProtocol *protocol,
		PurpleAccount *account, PurpleAccountUnregistrationCb cb,
		void *user_data)
{
	DEFINE_PROTOCOL_FUNC(protocol, unregister_user, account, cb, user_data);
}

void
purple_protocol_server_iface_set_info(PurpleProtocol *protocol,
		PurpleConnection *gc, const char *info)
{
	DEFINE_PROTOCOL_FUNC(protocol, set_info, gc, info);
}

void
purple_protocol_server_iface_get_info(PurpleProtocol *protocol,
		PurpleConnection *gc, const char *who)
{
	DEFINE_PROTOCOL_FUNC(protocol, get_info, gc, who);
}

void
purple_protocol_server_iface_set_status(PurpleProtocol *protocol,
		PurpleAccount *account, PurpleStatus *status)
{
	DEFINE_PROTOCOL_FUNC(protocol, set_status, account, status);
}

void
purple_protocol_server_iface_set_idle(PurpleProtocol *protocol,
		PurpleConnection *gc, int idletime)
{
	DEFINE_PROTOCOL_FUNC(protocol, set_idle, gc, idletime);
}

void
purple_protocol_server_iface_change_passwd(PurpleProtocol *protocol,
		PurpleConnection *gc, const char *old_pass, const char *new_pass)
{
	DEFINE_PROTOCOL_FUNC(protocol, change_passwd, gc, old_pass, new_pass);
}

void
purple_protocol_server_iface_add_buddy(PurpleProtocol *protocol,
		PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group,
		const char *message)
{
	DEFINE_PROTOCOL_FUNC(protocol, add_buddy, gc, buddy, group, message);
}

void
purple_protocol_server_iface_add_buddies(PurpleProtocol *protocol,
		PurpleConnection *gc, GList *buddies, GList *groups,
		const char *message)
{
	DEFINE_PROTOCOL_FUNC(protocol, add_buddies, gc, buddies, groups, message);
}

void
purple_protocol_server_iface_remove_buddy(PurpleProtocol *protocol,
		PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
	DEFINE_PROTOCOL_FUNC(protocol, remove_buddy, gc, buddy, group);
}

void
purple_protocol_server_iface_remove_buddies(PurpleProtocol *protocol,
		PurpleConnection *gc, GList *buddies, GList *groups)
{
	DEFINE_PROTOCOL_FUNC(protocol, remove_buddies, gc, buddies, groups);
}

void
purple_protocol_server_iface_keepalive(PurpleProtocol *protocol,
		PurpleConnection *gc)
{
	DEFINE_PROTOCOL_FUNC(protocol, keepalive, gc);
}

int
purple_protocol_server_iface_get_keepalive_interval(PurpleProtocol *protocol)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, 30, get_keepalive_interval);
}

void
purple_protocol_server_iface_alias_buddy(PurpleProtocol *protocol,
		PurpleConnection *gc, const char *who, const char *alias)
{
	DEFINE_PROTOCOL_FUNC(protocol, alias_buddy, gc, who, alias);
}

void
purple_protocol_server_iface_group_buddy(PurpleProtocol *protocol,
		PurpleConnection *gc, const char *who, const char *old_group,
		const char *new_group)
{
	DEFINE_PROTOCOL_FUNC(protocol, group_buddy, gc, who, old_group, new_group);
}

void
purple_protocol_server_iface_rename_group(PurpleProtocol *protocol,
		PurpleConnection *gc, const char *old_name, PurpleGroup *group,
		GList *moved_buddies)
{
	DEFINE_PROTOCOL_FUNC(protocol, rename_group, gc, old_name, group,
			moved_buddies);
}

void
purple_protocol_server_iface_set_buddy_icon(PurpleProtocol *protocol,
		PurpleConnection *gc, PurpleImage *img)
{
	DEFINE_PROTOCOL_FUNC(protocol, set_buddy_icon, gc, img);
}

void
purple_protocol_server_iface_remove_group(PurpleProtocol *protocol,
		PurpleConnection *gc, PurpleGroup *group)
{
	DEFINE_PROTOCOL_FUNC(protocol, remove_group, gc, group);
}

int
purple_protocol_server_iface_send_raw(PurpleProtocol *protocol,
		PurpleConnection *gc, const char *buf, int len)
{
	DEFINE_PROTOCOL_FUNC_WITH_RETURN(protocol, 0, send_raw, gc, buf, len);
}

void
purple_protocol_server_iface_set_public_alias(PurpleProtocol *protocol,
		PurpleConnection *gc, const char *alias,
		PurpleSetPublicAliasSuccessCallback success_cb,
		PurpleSetPublicAliasFailureCallback failure_cb)
{
	DEFINE_PROTOCOL_FUNC(protocol, set_public_alias, gc, alias, success_cb,
			failure_cb);
}

void
purple_protocol_server_iface_get_public_alias(PurpleProtocol *protocol,
		PurpleConnection *gc, PurpleGetPublicAliasSuccessCallback success_cb,
		PurpleGetPublicAliasFailureCallback failure_cb)
{
	DEFINE_PROTOCOL_FUNC(protocol, get_public_alias, gc, success_cb,
			failure_cb);
}

#undef DEFINE_PROTOCOL_FUNC_WITH_RETURN
#undef DEFINE_PROTOCOL_FUNC
