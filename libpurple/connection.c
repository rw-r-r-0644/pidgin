/* purple
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

#include <glib/gi18n-lib.h>

#include "internal.h"

#include "account.h"
#include "buddylist.h"
#include "connection.h"
#include "debug.h"
#include "notify.h"
#include "prefs.h"
#include "proxy.h"
#include "purpleenums.h"
#include "purpleprivate.h"
#include "purpleprotocolserver.h"
#include "purpleprotocolfactory.h"
#include "request.h"
#include "server.h"
#include "signals.h"
#include "util.h"

/**
 * PurpleConnection:
 *
 * Represents an active connection on an account.
 */
struct _PurpleConnection {
	GObject gparent;

	PurpleProtocol *protocol;     /* The protocol.                     */
	PurpleConnectionFlags flags;  /* Connection flags.                 */

	PurpleConnectionState state;  /* The connection state.             */

	PurpleAccount *account;       /* The account being connected to.   */
	char *password;               /* The password used.                */

	GSList *active_chats;         /* A list of active chats
	                                  (#PurpleChatConversation structs). */

	/* TODO Remove this and use protocol-specific subclasses. */
	void *proto_data;             /* Protocol-specific data.           */

	char *display_name;           /* How you appear to other people.   */
	GSource *keepalive;           /* Keep-alive.                       */

	/* Wants to Die state.  This is set when the user chooses to log out, or
	 * when the protocol is disconnected and should not be automatically
	 * reconnected (incorrect password, etc.).  Protocols should rely on
	 * purple_connection_error() to set this for them rather than
	 * setting it themselves.
	 * See purple_connection_error_is_fatal()
	 */
	gboolean wants_to_die;

	gboolean is_finalizing;    /* The object is being destroyed. */

	/* The connection error and its description if an error occurred. */
	PurpleConnectionErrorInfo *error_info;

	guint disconnect_timeout;  /* Timer used for nasty stack tricks. */
} PurpleConnectionPrivate;

enum {
	PROP_0,
	PROP_PROTOCOL,
	PROP_FLAGS,
	PROP_STATE,
	PROP_ACCOUNT,
	PROP_PASSWORD,
	PROP_DISPLAY_NAME,
	PROP_LAST
};

static GParamSpec *properties[PROP_LAST] = {NULL, };

static GList *connections = NULL;
static GList *connections_connecting = NULL;
static GList *connections_connected = NULL;
static PurpleConnectionUiOps *connection_ui_ops = NULL;

static int connections_handle;

static PurpleConnectionErrorInfo *
purple_connection_error_info_new(PurpleConnectionError type,
                                 const gchar *description);

G_DEFINE_TYPE(PurpleConnection, purple_connection, G_TYPE_OBJECT)

/**************************************************************************
 * Connection API
 **************************************************************************/
static gboolean
send_keepalive(gpointer data)
{
	PurpleConnection *gc = data;

	purple_protocol_server_keepalive(PURPLE_PROTOCOL_SERVER(gc->protocol), gc);

	return TRUE;
}

static void
update_keepalive(PurpleConnection *gc, gboolean on)
{
	if(!PURPLE_PROTOCOL_IMPLEMENTS(gc->protocol, SERVER, keepalive)) {
		return;
	}

	if(on && !gc->keepalive) {
		int interval = purple_protocol_server_get_keepalive_interval(PURPLE_PROTOCOL_SERVER(gc->protocol));
		purple_debug_info("connection", "Activating keepalive to %d seconds.", interval);
		gc->keepalive = g_main_context_find_source_by_id(NULL, g_timeout_add_seconds(interval, send_keepalive, gc));
	} else if (!on && gc->keepalive) {
		purple_debug_info("connection", "Deactivating keepalive.\n");
		g_source_destroy(gc->keepalive);
		gc->keepalive = NULL;
	}
}

/*
 * d:)->-<
 *
 * d:O-\-<
 *
 * d:D-/-<
 *
 * d8D->-< DANCE!
 */

void
purple_connection_set_state(PurpleConnection *gc, PurpleConnectionState state)
{
	PurpleConnectionUiOps *ops;

	g_return_if_fail(PURPLE_IS_CONNECTION(gc));

	if(gc->state == state) {
		return;
	}

	gc->state = state;

	ops = purple_connections_get_ui_ops();

	if(gc->state == PURPLE_CONNECTION_CONNECTING) {
		connections_connecting = g_list_append(connections_connecting, gc);
	} else {
		connections_connecting = g_list_remove(connections_connecting, gc);
	}

	if(gc->state == PURPLE_CONNECTION_CONNECTED) {
		PurpleAccount *account;
		PurplePresence *presence;
		gboolean emit_online = FALSE;
		gpointer handle = NULL;

		account = purple_connection_get_account(gc);
		presence = purple_account_get_presence(account);
		handle = purple_connections_get_handle();

		/* Set the time the account came online */
		purple_presence_set_login_time(presence, time(NULL));

		if(ops != NULL && ops->connected != NULL) {
			ops->connected(gc);
		}

		purple_blist_add_account(account);

		purple_signal_emit(handle, "signed-on", gc);
		purple_signal_emit_return_1(handle, "autojoin", gc);

		purple_serv_set_permit_deny(gc);

		update_keepalive(gc, TRUE);

		/* check if connections_connected is NULL, if so we need to emit the
		 * online signal.
		 */
		if(connections_connected == NULL) {
			emit_online = TRUE;
		}

		connections_connected = g_list_append(connections_connected, gc);

		if(emit_online) {
			purple_signal_emit(handle, "online");
		}
	} else if(gc->state == PURPLE_CONNECTION_DISCONNECTED) {
		if(ops != NULL && ops->disconnected != NULL) {
			ops->disconnected(gc);
		}
	}

	if(!gc->is_finalizing) {
		g_object_notify_by_pspec(G_OBJECT(gc), properties[PROP_STATE]);
	}
}

void
purple_connection_set_flags(PurpleConnection *gc, PurpleConnectionFlags flags)
{
	g_return_if_fail(PURPLE_IS_CONNECTION(gc));

	gc->flags = flags;

	if(!gc->is_finalizing) {
		g_object_notify_by_pspec(G_OBJECT(gc), properties[PROP_FLAGS]);
	}
}

void
purple_connection_set_display_name(PurpleConnection *gc, const gchar *name) {
	g_return_if_fail(PURPLE_IS_CONNECTION(gc));

	g_free(gc->display_name);
	gc->display_name = g_strdup(name);

	g_object_notify_by_pspec(G_OBJECT(gc), properties[PROP_DISPLAY_NAME]);
}

void
purple_connection_set_protocol_data(PurpleConnection *gc, void *proto_data) {
	g_return_if_fail(PURPLE_IS_CONNECTION(gc));

	gc->proto_data = proto_data;
}

PurpleConnectionState
purple_connection_get_state(PurpleConnection *gc) {
	g_return_val_if_fail(PURPLE_IS_CONNECTION(gc), PURPLE_CONNECTION_DISCONNECTED);

	return gc->state;
}

PurpleConnectionFlags
purple_connection_get_flags(PurpleConnection *gc) {
	g_return_val_if_fail(PURPLE_IS_CONNECTION(gc), 0);

	return gc->flags;
}

gboolean
purple_connection_is_disconnecting(PurpleConnection *gc) {
	g_return_val_if_fail(PURPLE_IS_CONNECTION(gc), TRUE);

	return gc->is_finalizing;
}

PurpleAccount *
purple_connection_get_account(PurpleConnection *gc) {
	g_return_val_if_fail(PURPLE_IS_CONNECTION(gc), NULL);

	return gc->account;
}

PurpleProtocol *
purple_connection_get_protocol(PurpleConnection *gc) {
	g_return_val_if_fail(PURPLE_IS_CONNECTION(gc), NULL);

	return gc->protocol;
}

const char *
purple_connection_get_password(PurpleConnection *gc) {
	g_return_val_if_fail(PURPLE_IS_CONNECTION(gc), NULL);

	return gc->password;
}

GSList *
purple_connection_get_active_chats(PurpleConnection *gc) {
	g_return_val_if_fail(PURPLE_IS_CONNECTION(gc), NULL);

	return gc->active_chats;
}

const char *
purple_connection_get_display_name(PurpleConnection *gc) {
	g_return_val_if_fail(PURPLE_IS_CONNECTION(gc), NULL);

	return gc->display_name;
}

void *
purple_connection_get_protocol_data(PurpleConnection *gc) {
	g_return_val_if_fail(PURPLE_IS_CONNECTION(gc), NULL);

	return gc->proto_data;
}

void
_purple_connection_add_active_chat(PurpleConnection *gc,
                                   PurpleChatConversation *chat)
{
	g_return_if_fail(PURPLE_IS_CONNECTION(gc));

	gc->active_chats = g_slist_append(gc->active_chats, chat);
}

void
_purple_connection_remove_active_chat(PurpleConnection *gc,
                                      PurpleChatConversation *chat)
{
	g_return_if_fail(PURPLE_IS_CONNECTION(gc));

	gc->active_chats = g_slist_remove(gc->active_chats, chat);
}

gboolean
_purple_connection_wants_to_die(PurpleConnection *gc) {
	g_return_val_if_fail(PURPLE_IS_CONNECTION(gc), FALSE);

	return gc->wants_to_die;
}

void
purple_connection_update_progress(PurpleConnection *gc, const char *text,
                                  size_t step, size_t count)
{
	PurpleConnectionUiOps *ops;

	g_return_if_fail(PURPLE_IS_CONNECTION(gc));
	g_return_if_fail(text != NULL);
	g_return_if_fail(step < count);
	g_return_if_fail(count > 1);

	ops = purple_connections_get_ui_ops();

	if(ops != NULL && ops->connect_progress != NULL) {
		ops->connect_progress(gc, text, step, count);
	}
}

void
purple_connection_notice(PurpleConnection *gc, const char *text) {
	PurpleConnectionUiOps *ops;

	g_return_if_fail(PURPLE_IS_CONNECTION(gc));
	g_return_if_fail(text != NULL);

	ops = purple_connections_get_ui_ops();

	if(ops != NULL && ops->notice != NULL) {
		ops->notice(gc, text);
	}
}

static gboolean
purple_connection_disconnect_cb(gpointer data) {
	PurpleAccount *account = data;
	PurpleConnection *gc;

	gc = purple_account_get_connection(account);

	if(PURPLE_IS_CONNECTION(gc)) {
		gc->disconnect_timeout = 0;
	}

	purple_account_disconnect(account);

	return FALSE;
}

void
purple_connection_error(PurpleConnection *gc,
                        PurpleConnectionError reason,
                        const char *description)
{
	PurpleConnectionUiOps *ops;

	g_return_if_fail(PURPLE_IS_CONNECTION(gc));

	/* This sanity check relies on PURPLE_CONNECTION_ERROR_OTHER_ERROR
	 * being the last member of the PurpleConnectionError enum in
	 * connection.h; if other reasons are added after it, this check should
	 * be updated.
	 */
	if(reason > PURPLE_CONNECTION_ERROR_OTHER_ERROR) {
		purple_debug_error("connection",
			"purple_connection_error: reason %u isn't a "
			"valid reason\n", reason);
		reason = PURPLE_CONNECTION_ERROR_OTHER_ERROR;
	}

	if(description == NULL) {
		purple_debug_error("connection", "purple_connection_error called with NULL description\n");
		description = _("Unknown error");
	}

	/* If we've already got one error, we don't need any more */
	if(purple_connection_get_error_info(gc)) {
		return;
	}

	gc->wants_to_die = purple_connection_error_is_fatal(reason);

	purple_debug_info("connection",
	                  "Connection error on %p (reason: %u description: %s)\n",
	                  gc, reason, description);

	ops = purple_connections_get_ui_ops();

	if(ops && ops->report_disconnect) {
		ops->report_disconnect(gc, reason, description);
	}

	gc->error_info = purple_connection_error_info_new(reason, description);

	purple_signal_emit(purple_connections_get_handle(), "connection-error",
	                   gc, reason, description);

	gc->disconnect_timeout = g_timeout_add(0, purple_connection_disconnect_cb,
	                                       purple_connection_get_account(gc));
}

PurpleConnectionErrorInfo *
purple_connection_get_error_info(PurpleConnection *gc) {
	g_return_val_if_fail(PURPLE_IS_CONNECTION(gc), NULL);

	return gc->error_info;
}

void
purple_connection_ssl_error(PurpleConnection *gc,
                            PurpleSslErrorType ssl_error)
{
	PurpleConnectionError reason;
	const gchar *message;

	switch (ssl_error) {
		case PURPLE_SSL_HANDSHAKE_FAILED:
			message = _("SSL Handshake Failed");
			reason = PURPLE_CONNECTION_ERROR_ENCRYPTION_ERROR;
			break;
		case PURPLE_SSL_CONNECT_FAILED:
			message = _("SSL Connection Failed");
			reason = PURPLE_CONNECTION_ERROR_NETWORK_ERROR;
			break;
		case PURPLE_SSL_CERTIFICATE_INVALID:
			/* TODO: maybe PURPLE_SSL_* should be more specific? */
			message = _("SSL peer presented an invalid certificate");
			reason = PURPLE_CONNECTION_ERROR_CERT_OTHER_ERROR;
			break;
		default:
			g_assert_not_reached ();
			message = _("Unknown SSL error");
			reason = PURPLE_CONNECTION_ERROR_CERT_OTHER_ERROR;
	}

	purple_connection_error(gc, reason, message);
}

void
purple_connection_g_error(PurpleConnection *pc, const GError *error) {
	PurpleConnectionError reason;

	if(g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		/* Not a connection error. Ignore. */
		return;
	}

	if(error->domain == G_TLS_ERROR) {
		switch (error->code) {
			case G_TLS_ERROR_UNAVAILABLE:
				reason = PURPLE_CONNECTION_ERROR_NO_SSL_SUPPORT;
				break;
			case G_TLS_ERROR_NOT_TLS:
			case G_TLS_ERROR_HANDSHAKE:
				reason = PURPLE_CONNECTION_ERROR_ENCRYPTION_ERROR;
				break;
			case G_TLS_ERROR_BAD_CERTIFICATE:
			case G_TLS_ERROR_CERTIFICATE_REQUIRED:
				reason = PURPLE_CONNECTION_ERROR_CERT_OTHER_ERROR;
				break;
			case G_TLS_ERROR_EOF:
			case G_TLS_ERROR_MISC:
			default:
				reason = PURPLE_CONNECTION_ERROR_NETWORK_ERROR;
		}
	} else if (error->domain == G_IO_ERROR) {
		reason = PURPLE_CONNECTION_ERROR_NETWORK_ERROR;
	} else if (error->domain == PURPLE_CONNECTION_ERROR) {
		reason = error->code;
	} else {
		reason = PURPLE_CONNECTION_ERROR_OTHER_ERROR;
	}

	purple_connection_error(pc, reason, error->message);
}

void
purple_connection_take_error(PurpleConnection *pc, GError *error) {
	purple_connection_g_error(pc, error);
	g_error_free(error);
}

gboolean
purple_connection_error_is_fatal(PurpleConnectionError reason) {
	switch (reason) {
		case PURPLE_CONNECTION_ERROR_NETWORK_ERROR:
		case PURPLE_CONNECTION_ERROR_ENCRYPTION_ERROR:
			return FALSE;
		case PURPLE_CONNECTION_ERROR_INVALID_USERNAME:
		case PURPLE_CONNECTION_ERROR_AUTHENTICATION_FAILED:
		case PURPLE_CONNECTION_ERROR_AUTHENTICATION_IMPOSSIBLE:
		case PURPLE_CONNECTION_ERROR_NO_SSL_SUPPORT:
		case PURPLE_CONNECTION_ERROR_NAME_IN_USE:
		case PURPLE_CONNECTION_ERROR_INVALID_SETTINGS:
		case PURPLE_CONNECTION_ERROR_OTHER_ERROR:
		case PURPLE_CONNECTION_ERROR_CERT_NOT_PROVIDED:
		case PURPLE_CONNECTION_ERROR_CERT_UNTRUSTED:
		case PURPLE_CONNECTION_ERROR_CERT_EXPIRED:
		case PURPLE_CONNECTION_ERROR_CERT_NOT_ACTIVATED:
		case PURPLE_CONNECTION_ERROR_CERT_HOSTNAME_MISMATCH:
		case PURPLE_CONNECTION_ERROR_CERT_FINGERPRINT_MISMATCH:
		case PURPLE_CONNECTION_ERROR_CERT_SELF_SIGNED:
		case PURPLE_CONNECTION_ERROR_CERT_OTHER_ERROR:
			return TRUE;
		default:
			g_return_val_if_reached(TRUE);
	}
}

void
purple_connection_update_last_received(PurpleConnection *gc) {
	g_return_if_fail(PURPLE_IS_CONNECTION(gc));

	/*
	 * For safety, actually this function shouldn't be called when the
	 * keepalive mechanism is inactive.
	 */
	if(gc->keepalive) {
		/* The #GTimeoutSource API doesn't expose a function to reset when a
		 * #GTimeoutSource will dispatch the next time, but because it works to
		 * directly call g_source_set_ready_time() on a #GTimeoutSource, and since
		 * it seems unlikely that the implementation will change, we just do that
		 * for now as a workaround for this API shortcoming.
		 */
		gint64 seconds_from_now = purple_protocol_server_get_keepalive_interval(PURPLE_PROTOCOL_SERVER(gc->protocol));

		g_source_set_ready_time(
			gc->keepalive,
			g_get_monotonic_time() + (seconds_from_now * G_USEC_PER_SEC)
		);
	}
}

static PurpleConnectionErrorInfo *
purple_connection_error_info_new(PurpleConnectionError type,
                                 const gchar *description)
{
	PurpleConnectionErrorInfo *err;

	g_return_val_if_fail(description != NULL, NULL);

	err = g_new(PurpleConnectionErrorInfo, 1);

	err->type = type;
	err->description = g_strdup(description);

	return err;
}

/**************************************************************************
 * GBoxed code
 **************************************************************************/
static PurpleConnectionUiOps *
purple_connection_ui_ops_copy(PurpleConnectionUiOps *ops)
{
	PurpleConnectionUiOps *ops_new;

	g_return_val_if_fail(ops != NULL, NULL);

	ops_new = g_new(PurpleConnectionUiOps, 1);
	*ops_new = *ops;

	return ops_new;
}

GType
purple_connection_ui_ops_get_type(void) {
	static GType type = 0;

	if(type == 0) {
		type = g_boxed_type_register_static("PurpleConnectionUiOps",
				(GBoxedCopyFunc)purple_connection_ui_ops_copy,
				(GBoxedFreeFunc)g_free);
	}

	return type;
}

static PurpleConnectionErrorInfo *
purple_connection_error_info_copy(PurpleConnectionErrorInfo *err)
{
	g_return_val_if_fail(err != NULL, NULL);

	return purple_connection_error_info_new(err->type, err->description);
}

static void
purple_connection_error_info_free(PurpleConnectionErrorInfo *err) {
	g_return_if_fail(err != NULL);

	g_free(err->description);
	g_free(err);
}

GType
purple_connection_error_info_get_type(void) {
	static GType type = 0;

	if (type == 0) {
		type = g_boxed_type_register_static("PurpleConnectionErrorInfo",
				(GBoxedCopyFunc)purple_connection_error_info_copy,
				(GBoxedFreeFunc)purple_connection_error_info_free);
	}

	return type;
}

/**************************************************************************
 * GObject code
 **************************************************************************/

static void
purple_connection_set_property(GObject *obj, guint param_id,
                               const GValue *value, GParamSpec *pspec)
{
	PurpleConnection *gc = PURPLE_CONNECTION(obj);

	switch (param_id) {
		case PROP_PROTOCOL:
			gc->protocol = g_value_get_object(value);
			break;
		case PROP_FLAGS:
			purple_connection_set_flags(gc, g_value_get_flags(value));
			break;
		case PROP_STATE:
			purple_connection_set_state(gc, g_value_get_enum(value));
			break;
		case PROP_ACCOUNT:
			gc->account = g_value_get_object(value);
			break;
		case PROP_PASSWORD:
			g_free(gc->password);
			gc->password = g_value_dup_string(value);
			break;
		case PROP_DISPLAY_NAME:
			purple_connection_set_display_name(gc, g_value_get_string(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_connection_get_property(GObject *obj, guint param_id, GValue *value,
                               GParamSpec *pspec)
{
	PurpleConnection *gc = PURPLE_CONNECTION(obj);

	switch (param_id) {
		case PROP_PROTOCOL:
			g_value_set_object(value, purple_connection_get_protocol(gc));
			break;
		case PROP_FLAGS:
			g_value_set_flags(value, purple_connection_get_flags(gc));
			break;
		case PROP_STATE:
			g_value_set_enum(value, purple_connection_get_state(gc));
			break;
		case PROP_ACCOUNT:
			g_value_set_object(value, purple_connection_get_account(gc));
			break;
		case PROP_PASSWORD:
			g_value_set_string(value, purple_connection_get_password(gc));
			break;
		case PROP_DISPLAY_NAME:
			g_value_set_string(value, purple_connection_get_display_name(gc));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_connection_init(PurpleConnection *gc) {
	purple_connection_set_state(gc, PURPLE_CONNECTION_CONNECTING);
	connections = g_list_append(connections, gc);
}

static void
purple_connection_constructed(GObject *object) {
	PurpleConnection *gc = PURPLE_CONNECTION(object);
	PurpleAccount *account;

	G_OBJECT_CLASS(purple_connection_parent_class)->constructed(object);

	g_object_get(gc, "account", &account, NULL);
	purple_account_set_connection(account, gc);
	g_object_unref(account);

	purple_signal_emit(purple_connections_get_handle(), "signing-on", gc);
}

static void
purple_connection_finalize(GObject *object) {
	PurpleConnection *gc = PURPLE_CONNECTION(object);
	PurpleAccount *account;
	GSList *buddies;
	gboolean remove = FALSE;
	gpointer handle;

	gc->is_finalizing = TRUE;

	account = purple_connection_get_account(gc);
	handle = purple_connections_get_handle();

	purple_debug_info("connection", "Disconnecting connection %p\n", gc);

	if (purple_connection_get_state(gc) != PURPLE_CONNECTION_CONNECTING)
		remove = TRUE;

	purple_signal_emit(handle, "signing-off", gc);

	g_slist_free_full(gc->active_chats,
	                  (GDestroyNotify)purple_chat_conversation_leave);

	update_keepalive(gc, FALSE);

	purple_protocol_close(gc->protocol, gc);

	/* Clear out the proto data that was freed in the protocol's close method */
	buddies = purple_blist_find_buddies(account, NULL);
	while (buddies != NULL) {
		PurpleBuddy *buddy = buddies->data;
		purple_buddy_set_protocol_data(buddy, NULL);
		buddies = g_slist_delete_link(buddies, buddies);
	}

	connections = g_list_remove(connections, gc);

	purple_connection_set_state(gc, PURPLE_CONNECTION_DISCONNECTED);

	if(remove) {
		purple_blist_remove_account(account);
	}

	purple_signal_emit(handle, "signed-off", gc);

	purple_account_request_close_with_account(account);
	purple_request_close_with_handle(gc);
	purple_notify_close_with_handle(gc);

	connections_connected = g_list_remove(connections_connected, gc);
	if(connections_connected == NULL) {
		purple_signal_emit(handle, "offline");
	}

	purple_debug_info("connection", "Destroying connection %p\n", gc);

	purple_account_set_connection(account, NULL);

	if(gc->error_info) {
		purple_connection_error_info_free(gc->error_info);
	}

	if(gc->disconnect_timeout > 0) {
		g_source_remove(gc->disconnect_timeout);
	}

	purple_str_wipe(gc->password);
	g_free(gc->display_name);

	G_OBJECT_CLASS(purple_connection_parent_class)->finalize(object);
}

static void
purple_connection_class_init(PurpleConnectionClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->get_property = purple_connection_get_property;
	obj_class->set_property = purple_connection_set_property;
	obj_class->finalize = purple_connection_finalize;
	obj_class->constructed = purple_connection_constructed;

	properties[PROP_PROTOCOL] = g_param_spec_object(
		"protocol", "Protocol",
		"The protocol that the connection is using.",
		PURPLE_TYPE_PROTOCOL,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

	properties[PROP_FLAGS] = g_param_spec_flags(
		"flags", "Connection flags",
		"The flags of the connection.",
		PURPLE_TYPE_CONNECTION_FLAGS, 0,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	properties[PROP_STATE] = g_param_spec_enum(
		"state", "Connection state",
		"The current state of the connection.",
		PURPLE_TYPE_CONNECTION_STATE, PURPLE_CONNECTION_DISCONNECTED,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	properties[PROP_ACCOUNT] = g_param_spec_object(
		"account", "Account",
		"The account using the connection.", PURPLE_TYPE_ACCOUNT,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

	properties[PROP_PASSWORD] = g_param_spec_string(
		"password", "Password",
		"The password used for connection.", NULL,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

	properties[PROP_DISPLAY_NAME] = g_param_spec_string(
		"display-name", "Display name",
		"Your name that appears to other people.", NULL,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, PROP_LAST, properties);
}

void
_purple_connection_new(PurpleAccount *account, gboolean is_registration,
                       const gchar *password)
{
	PurpleConnection *gc;
	PurpleProtocol *protocol;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));

	if(!purple_account_is_disconnected(account)) {
		return;
	}

	protocol = purple_account_get_protocol(account);

	if(protocol == NULL) {
		gchar *message;

		message = g_strdup_printf(_("Missing protocol for %s"),
			purple_account_get_username(account));
		purple_notify_error(NULL, is_registration ? _("Registration Error") :
			_("Connection Error"), message, NULL,
			purple_request_cpar_from_account(account));
		g_free(message);

		return;
	}

	if(is_registration) {
		if(!PURPLE_PROTOCOL_IMPLEMENTS(protocol, SERVER, register_user)) {
			return;
		}
	} else {
		if(((password == NULL) || (*password == '\0')) &&
		   !(purple_protocol_get_options(protocol) & OPT_PROTO_NO_PASSWORD) &&
		   !(purple_protocol_get_options(protocol) & OPT_PROTO_PASSWORD_OPTIONAL))
		{
			purple_debug_error("connection", "Cannot connect to account %s without "
							 "a password.\n", purple_account_get_username(account));

			return;
		}
	}

	if(PURPLE_IS_PROTOCOL_FACTORY(protocol)) {
		PurpleProtocolFactory *factory = PURPLE_PROTOCOL_FACTORY(protocol);

		gc = purple_protocol_factory_connection_new(factory, account,
		                                            password);
	} else {
		gc = g_object_new(
			PURPLE_TYPE_CONNECTION,
			"protocol", protocol,
			"account", account,
			"password", password,
			NULL);
	}

	g_return_if_fail(gc != NULL);

	if(is_registration) {
		purple_debug_info("connection", "Registering.  gc = %p\n", gc);

		/* set this so we don't auto-reconnect after registering */
		gc->wants_to_die = TRUE;

		purple_protocol_server_register_user(PURPLE_PROTOCOL_SERVER(protocol),
		                                     account);
	} else {
		purple_debug_info("connection", "Connecting. gc = %p\n", gc);

		purple_signal_emit(purple_accounts_get_handle(), "account-connecting",
		                   account);
		purple_protocol_login(protocol, account);
	}
}

void
_purple_connection_new_unregister(PurpleAccount *account, const char *password,
                                  PurpleAccountUnregistrationCb cb,
                                  gpointer user_data)
{
	/* Lots of copy/pasted code to avoid API changes. You might want to integrate that into the previous function when possible. */
	PurpleConnection *gc;
	PurpleProtocol *protocol;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));

	protocol = purple_account_get_protocol(account);

	if(protocol == NULL) {
		gchar *message;

		message = g_strdup_printf(_("Missing protocol for %s"),
								  purple_account_get_username(account));
		purple_notify_error(NULL, _("Unregistration Error"), message,
			NULL, purple_request_cpar_from_account(account));
		g_free(message);
		return;
	}

	if(!purple_account_is_disconnected(account)) {
		purple_protocol_server_unregister_user(PURPLE_PROTOCOL_SERVER(protocol),
		                                       account, cb, user_data);
		return;
	}

	if(((password == NULL) || (*password == '\0')) &&
	   !(purple_protocol_get_options(protocol) & OPT_PROTO_NO_PASSWORD) &&
	   !(purple_protocol_get_options(protocol) & OPT_PROTO_PASSWORD_OPTIONAL))
	{
		purple_debug_error("connection", "Cannot connect to account %s without "
						   "a password.\n", purple_account_get_username(account));

		return;
	}

	if(PURPLE_IS_PROTOCOL_FACTORY(protocol)) {
		PurpleProtocolFactory *factory = PURPLE_PROTOCOL_FACTORY(protocol);
		gc = purple_protocol_factory_connection_new(factory, account,
		                                            password);
	} else {
		gc = g_object_new(
			PURPLE_TYPE_CONNECTION,
			"protocol", protocol,
			"account", account,
			"password", password,
			NULL);
	}

	g_return_if_fail(gc != NULL);

	purple_debug_info("connection", "Unregistering.  gc = %p\n", gc);

	purple_protocol_server_unregister_user(PURPLE_PROTOCOL_SERVER(protocol),
	                                       account, cb, user_data);
}

/**************************************************************************
 * Connections API
 **************************************************************************/

void
_purple_assert_connection_is_valid(PurpleConnection *gc, const gchar *file,
                                   int line)
{
	if(gc && g_list_find(purple_connections_get_all(), gc)) {
		return;
	}

	purple_debug_fatal("connection", "PURPLE_ASSERT_CONNECTION_IS_VALID(%p)"
		" failed at %s:%d", gc, file, line);

	/* ugh - gk 2021-10-28 */
	exit(-1);
}

void
purple_connections_disconnect_all(void) {
	GList *l;
	PurpleConnection *gc;

	while((l = purple_connections_get_all()) != NULL) {
		gc = l->data;
		gc->wants_to_die = TRUE;
		purple_account_disconnect(gc->account);
	}
}

GList *
purple_connections_get_all(void) {
	return connections;
}

GList *
purple_connections_get_connecting(void) {
	return connections_connecting;
}

gboolean
purple_connections_is_online(void) {
	return (connections_connected != NULL);
}

void
purple_connections_set_ui_ops(PurpleConnectionUiOps *ops) {
	connection_ui_ops = ops;
}

PurpleConnectionUiOps *
purple_connections_get_ui_ops(void) {
	return connection_ui_ops;
}

void
purple_connections_init(void) {
	void *handle = purple_connections_get_handle();

	purple_signal_register(handle, "online", purple_marshal_VOID, G_TYPE_NONE,
	                       0);

	purple_signal_register(handle, "offline", purple_marshal_VOID, G_TYPE_NONE,
	                       0);

	purple_signal_register(handle, "signing-on",
						 purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
						 PURPLE_TYPE_CONNECTION);

	purple_signal_register(handle, "signed-on",
						 purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
						 PURPLE_TYPE_CONNECTION);

	purple_signal_register(handle, "signing-off",
						 purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
						 PURPLE_TYPE_CONNECTION);

	purple_signal_register(handle, "signed-off",
						 purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
						 PURPLE_TYPE_CONNECTION);

	purple_signal_register(handle, "connection-error",
	                       purple_marshal_VOID__POINTER_INT_POINTER,
	                       G_TYPE_NONE, 3, PURPLE_TYPE_CONNECTION,
	                       PURPLE_TYPE_CONNECTION_ERROR, G_TYPE_STRING);

	purple_signal_register(handle, "autojoin",
	                       purple_marshal_BOOLEAN__POINTER, G_TYPE_NONE, 1,
	                       PURPLE_TYPE_CONNECTION);
}

void
purple_connections_uninit(void) {
	purple_signals_unregister_by_instance(purple_connections_get_handle());
}

void *
purple_connections_get_handle(void) {
	return &connections_handle;
}
