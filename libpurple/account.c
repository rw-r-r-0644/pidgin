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

#include "accounts.h"
#include "core.h"
#include "debug.h"
#include "network.h"
#include "notify.h"
#include "prefs.h"
#include "purpleaccountmanager.h"
#include "purpleaccountpresence.h"
#include "purpleconversationmanager.h"
#include "purplecredentialmanager.h"
#include "purplenotification.h"
#include "purplenotificationmanager.h"
#include "purpleprivate.h"
#include "purpleprotocolclient.h"
#include "purpleprotocolmanager.h"
#include "purpleprotocolserver.h"
#include "request.h"
#include "server.h"
#include "signals.h"
#include "util.h"

/**
 * PurpleAccount:
 *
 * Structure representing an account.
 */
struct _PurpleAccount
{
	GObject gparent;
};

typedef struct
{
	gchar *id;

	char *username;             /* The username.                          */
	char *alias;                /* How you appear to yourself.            */
	char *user_info;            /* User information.                      */

	char *buddy_icon_path;      /* The buddy icon's non-cached path.      */

	gboolean enabled;
	gboolean remember_pass;     /* Remember the password.                 */

	/*
	 * TODO: After a GObject representing a protocol is ready, use it
	 * here instead of the protocol ID.
	 */
	char *protocol_id;          /* The ID of the protocol.                */

	PurpleConnection *gc;       /* The connection handle.               */
	gboolean disconnecting;     /* The account is currently disconnecting */

	GHashTable *settings;       /* Protocol-specific settings.            */

	PurpleProxyInfo *proxy_info;  /* Proxy information.  This will be set */
								/*   to NULL when the account inherits      */
								/*   proxy settings from global prefs.      */

	/*
	 * TODO: Instead of linked lists for permit and deny, use a data
	 * structure that allows fast lookups AND decent performance when
	 * iterating through all items. Fast lookups should help performance
	 * for protocols like MSN, where all your buddies exist in your permit
	 * list therefore the permit list is large. Possibly GTree or
	 * GHashTable.
	 */
	GSList *permit;             /* Permit list.                           */
	GSList *deny;               /* Deny list.                             */
	PurpleAccountPrivacyType privacy_type;  /* The permit/deny setting.   */

	GList *status_types;        /* Status types.                          */

	PurplePresence *presence;     /* Presence.                            */

	PurpleAccountRegistrationCb registration_cb;
	void *registration_cb_user_data;

	PurpleConnectionErrorInfo *current_error;	/* Errors */
	PurpleNotification *error_notification;
} PurpleAccountPrivate;

typedef struct
{
	char *ui;
	GValue value;

} PurpleAccountSetting;

typedef struct
{
	PurpleAccountRequestType type;
	PurpleAccount *account;
	void *ui_handle;
	char *user;
	gpointer userdata;
	PurpleAccountRequestAuthorizationCb auth_cb;
	PurpleAccountRequestAuthorizationCb deny_cb;
	guint ref;
} PurpleAccountRequestInfo;

typedef struct
{
	PurpleAccount *account;
	GCallback cb;
	gpointer data;
} PurpleCallbackBundle;

/* GObject Property enums */
enum
{
	PROP_0,
	PROP_ID,
	PROP_USERNAME,
	PROP_PRIVATE_ALIAS,
	PROP_ENABLED,
	PROP_CONNECTION,
	PROP_PROTOCOL_ID,
	PROP_USER_INFO,
	PROP_BUDDY_ICON_PATH,
	PROP_REMEMBER_PASSWORD,
	PROP_PROXY_INFO,
	PROP_LAST
};

static GParamSpec    *properties[PROP_LAST];
static GList         *handles = NULL;

G_DEFINE_TYPE_WITH_PRIVATE(PurpleAccount, purple_account, G_TYPE_OBJECT);

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
purple_account_register_got_password_cb(GObject *obj, GAsyncResult *res,
                                        gpointer data)
{
	PurpleCredentialManager *manager = PURPLE_CREDENTIAL_MANAGER(obj);
	PurpleAccount *account = PURPLE_ACCOUNT(data);
	GError *error = NULL;
	gchar *password = NULL;

	password = purple_credential_manager_read_password_finish(manager, res,
	                                                          &error);

	if(error != NULL) {
		purple_debug_warning("account", "failed to read password: %s",
		                     error->message);
		g_error_free(error);
	}

	_purple_connection_new(account, TRUE, password);

	g_free(password);
}

void
purple_account_register(PurpleAccount *account)
{
	PurpleCredentialManager *manager = NULL;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));

	purple_debug_info("account", "Registering account %s\n",
					purple_account_get_username(account));

	manager = purple_credential_manager_get_default();
	purple_credential_manager_read_password_async(manager, account, NULL,
	                                              purple_account_register_got_password_cb,
	                                              account);
}

static void
purple_account_unregister_got_password_cb(GObject *obj, GAsyncResult *res,
                                          gpointer data)
{
	PurpleCredentialManager *manager = PURPLE_CREDENTIAL_MANAGER(obj);
	PurpleCallbackBundle *cbb = data;
	PurpleAccountUnregistrationCb cb;
	GError *error = NULL;
	gchar *password = NULL;

	cb = (PurpleAccountUnregistrationCb)cbb->cb;

	password = purple_credential_manager_read_password_finish(manager, res,
	                                                          &error);

	if(error != NULL) {
		purple_debug_warning("account", "failed to read password: %s",
		                     error->message);

		g_error_free(error);
	}

	_purple_connection_new_unregister(cbb->account, password, cb, cbb->data);

	g_free(password);
	g_free(cbb);
}

struct register_completed_closure
{
	PurpleAccount *account;
	gboolean succeeded;
};

static gboolean
purple_account_register_completed_cb(gpointer data)
{
	struct register_completed_closure *closure = data;
	PurpleAccountPrivate *priv;

	priv = purple_account_get_instance_private(closure->account);

	if (priv->registration_cb)
		(priv->registration_cb)(closure->account, closure->succeeded,
				priv->registration_cb_user_data);

	g_object_unref(closure->account);
	g_free(closure);

	return FALSE;
}

static void
request_password_write_cb(GObject *obj, GAsyncResult *res, gpointer data) {
	PurpleCredentialManager *manager = PURPLE_CREDENTIAL_MANAGER(obj);
	PurpleAccount *account = PURPLE_ACCOUNT(data);
	GError *error = NULL;
	gchar *password = NULL;

	/* We stash the password on the account to get it to this call back... It's
	 * kind of gross but shouldn't be a big deal because any plugin has access
	 * to the credential store, so it's not really a security leak.
	 */
	password = (gchar *)g_object_get_data(G_OBJECT(account), "_tmp_password");
	g_object_set_data(G_OBJECT(account), "_tmp_password", NULL);

	if(!purple_credential_manager_write_password_finish(manager, res, &error)) {
		const gchar *name = purple_account_get_name_for_display(account);

		/* we can't error an account without a connection, so we just drop a
		 * debug message for now and continue to connect the account.
		 */
		purple_debug_info("account",
		                  "failed to save password for account \"%s\": %s",
		                  name,
		                  error != NULL ? error->message : "unknown error");
	}

	_purple_connection_new(account, FALSE, password);

	g_free(password);
}

static void
request_password_ok_cb(PurpleAccount *account, PurpleRequestFields *fields)
{
	const char *entry;
	gboolean remember;

	entry = purple_request_fields_get_string(fields, "password");
	remember = purple_request_fields_get_bool(fields, "remember");

	if (!entry || !*entry)
	{
		purple_notify_error(account, NULL,
			_("Password is required to sign on."), NULL,
			purple_request_cpar_from_account(account));
		return;
	}

	purple_account_set_remember_password(account, remember);

	if(remember) {
		PurpleCredentialManager *manager = NULL;

		manager = purple_credential_manager_get_default();

		/* The requests field can be invalidated by the time we write the
		 * password and we want to use it in the write callback, so we need to
		 * duplicate it for that callback.
		 */
		g_object_set_data(G_OBJECT(account), "_tmp_password", g_strdup(entry));
		purple_credential_manager_write_password_async(manager, account, entry,
		                                               NULL,
		                                               request_password_write_cb,
		                                               account);
	} else {
		_purple_connection_new(account, FALSE, entry);
	}
}

static void
request_password_cancel_cb(PurpleAccount *account, PurpleRequestFields *fields)
{
	/* Disable the account as the user has cancelled connecting */
	purple_account_set_enabled(account, FALSE);
}


static void
purple_account_connect_got_password_cb(GObject *obj, GAsyncResult *res,
                                       gpointer data)
{
	PurpleCredentialManager *manager = PURPLE_CREDENTIAL_MANAGER(obj);
	PurpleAccount *account = PURPLE_ACCOUNT(data);
	PurpleProtocol *protocol = NULL;
	GError *error = NULL;
	gchar *password = NULL;

	password = purple_credential_manager_read_password_finish(manager, res,
	                                                          &error);

	if(error != NULL) {
		purple_debug_warning("account", "failed to read password %s",
		                     error->message);

		g_error_free(error);
	}

	protocol = purple_account_get_protocol(account);

	if((password == NULL || *password == '\0') &&
		!(purple_protocol_get_options(protocol) & OPT_PROTO_NO_PASSWORD) &&
		!(purple_protocol_get_options(protocol) & OPT_PROTO_PASSWORD_OPTIONAL))
	{
		purple_account_request_password(account,
			G_CALLBACK(request_password_ok_cb),
			G_CALLBACK(request_password_cancel_cb), account);
	} else {
		_purple_connection_new(account, FALSE, password);
	}

	g_free(password);
}

static PurpleAccountRequestInfo *
purple_account_request_info_unref(PurpleAccountRequestInfo *info)
{
	if (--info->ref)
		return info;

	/* TODO: This will leak info->user_data, but there is no callback to just clean that up */
	g_free(info->user);
	g_free(info);
	return NULL;
}

static void
purple_account_request_close_info(PurpleAccountRequestInfo *info)
{
	PurpleAccountUiOps *ops;

	ops = purple_accounts_get_ui_ops();

	if (ops != NULL && ops->close_account_request != NULL)
		ops->close_account_request(info->ui_handle);

	purple_account_request_info_unref(info);
}

static void
request_auth_cb(const char *message, void *data)
{
	PurpleAccountRequestInfo *info = data;

	handles = g_list_remove(handles, info);

	if (info->auth_cb != NULL)
		info->auth_cb(message, info->userdata);

	purple_signal_emit(purple_accounts_get_handle(),
			"account-authorization-granted", info->account, info->user, message);

	purple_account_request_info_unref(info);
}

static void
request_deny_cb(const char *message, void *data)
{
	PurpleAccountRequestInfo *info = data;

	handles = g_list_remove(handles, info);

	if (info->deny_cb != NULL)
		info->deny_cb(message, info->userdata);

	purple_signal_emit(purple_accounts_get_handle(),
			"account-authorization-denied", info->account, info->user, message);

	purple_account_request_info_unref(info);
}

static void
change_password_cb(PurpleAccount *account, PurpleRequestFields *fields)
{
	const char *orig_pass, *new_pass_1, *new_pass_2;

	orig_pass  = purple_request_fields_get_string(fields, "password");
	new_pass_1 = purple_request_fields_get_string(fields, "new_password_1");
	new_pass_2 = purple_request_fields_get_string(fields, "new_password_2");

	if (g_utf8_collate(new_pass_1, new_pass_2))
	{
		purple_notify_error(account, NULL,
			_("New passwords do not match."), NULL,
			purple_request_cpar_from_account(account));

		return;
	}

	if ((purple_request_fields_is_field_required(fields, "password") &&
			(orig_pass == NULL || *orig_pass == '\0')) ||
		(purple_request_fields_is_field_required(fields, "new_password_1") &&
			(new_pass_1 == NULL || *new_pass_1 == '\0')) ||
		(purple_request_fields_is_field_required(fields, "new_password_2") &&
			(new_pass_2 == NULL || *new_pass_2 == '\0')))
	{
		purple_notify_error(account, NULL,
			_("Fill out all fields completely."), NULL,
			purple_request_cpar_from_account(account));
		return;
	}

	purple_account_change_password(account, orig_pass, new_pass_1);
}

static void
set_user_info_cb(PurpleAccount *account, const char *user_info)
{
	PurpleConnection *gc;

	purple_account_set_user_info(account, user_info);
	gc = purple_account_get_connection(account);
	purple_serv_set_info(gc, user_info);
}

static void
delete_setting(void *data)
{
	PurpleAccountSetting *setting = (PurpleAccountSetting *)data;

	g_free(setting->ui);
	g_value_unset(&setting->value);

	g_free(setting);
}

static PurpleConnectionState
purple_account_get_state(PurpleAccount *account)
{
	PurpleConnection *gc;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), PURPLE_CONNECTION_DISCONNECTED);

	gc = purple_account_get_connection(account);
	if (!gc)
		return PURPLE_CONNECTION_DISCONNECTED;

	return purple_connection_get_state(gc);
}

/*
 * This makes sure your permit list contains all buddies from your
 * buddy list and ONLY buddies from your buddy list.
 */
static void
add_all_buddies_to_permit_list(PurpleAccount *account, gboolean local)
{
	GSList *list;
	PurpleAccountPrivate *priv = purple_account_get_instance_private(account);

	/* Remove anyone in the permit list who is not in the buddylist */
	for (list = priv->permit; list != NULL; ) {
		char *person = list->data;
		list = list->next;
		if (!purple_blist_find_buddy(account, person))
			purple_account_privacy_permit_remove(account, person, local);
	}

	/* Now make sure everyone in the buddylist is in the permit list */
	list = purple_blist_find_buddies(account, NULL);
	while (list != NULL)
	{
		PurpleBuddy *buddy = list->data;
		const gchar *name = purple_buddy_get_name(buddy);

		if (!g_slist_find_custom(priv->permit, name, (GCompareFunc)g_utf8_collate))
			purple_account_privacy_permit_add(account, name, local);
		list = g_slist_delete_link(list, list);
	}
}

void
_purple_account_set_current_error(PurpleAccount *account,
		PurpleConnectionErrorInfo *new_err)
{
	PurpleConnectionErrorInfo *old_err;
	PurpleAccountPrivate *priv;
	PurpleNotificationManager *manager = NULL;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));
	priv = purple_account_get_instance_private(account);

	old_err = priv->current_error;

	if(new_err == old_err)
		return;

	manager = purple_notification_manager_get_default();

	if(PURPLE_IS_NOTIFICATION(priv->error_notification)) {
		purple_notification_manager_remove(manager, priv->error_notification);
		g_clear_object(&priv->error_notification);
	}

	if(new_err != NULL) {
		PurpleProtocol *protocol = NULL;

		priv->error_notification =
			purple_notification_new(PURPLE_NOTIFICATION_TYPE_CONNECTION_ERROR,
			                        account, new_err, NULL);

		protocol = purple_account_get_protocol(account);
		if(PURPLE_IS_PROTOCOL(protocol)) {
			const gchar *icon_name = purple_protocol_get_icon_name(protocol);

			if(icon_name != NULL) {
				purple_notification_set_icon_name(priv->error_notification,
				                                  icon_name);
			}
		}

		purple_notification_manager_add(manager, priv->error_notification);
	}

	purple_signal_emit(purple_accounts_get_handle(),
	                   "account-error-changed",
	                   account, old_err, new_err);
	purple_accounts_schedule_save();

	if(old_err)
		g_free(old_err->description);

	g_free(old_err);
}

/******************************************************************************
 * XmlNode Helpers
 *****************************************************************************/
static PurpleXmlNode *
proxy_settings_to_xmlnode(PurpleProxyInfo *proxy_info)
{
	PurpleXmlNode *node, *child;
	PurpleProxyType proxy_type;
	const char *value;
	int int_value;
	char buf[21];

	proxy_type = purple_proxy_info_get_proxy_type(proxy_info);

	node = purple_xmlnode_new("proxy");

	child = purple_xmlnode_new_child(node, "type");
	purple_xmlnode_insert_data(child,
			(proxy_type == PURPLE_PROXY_TYPE_USE_GLOBAL ? "global" :
			 proxy_type == PURPLE_PROXY_TYPE_NONE       ? "none"   :
			 proxy_type == PURPLE_PROXY_TYPE_HTTP       ? "http"   :
			 proxy_type == PURPLE_PROXY_TYPE_SOCKS4     ? "socks4" :
			 proxy_type == PURPLE_PROXY_TYPE_SOCKS5     ? "socks5" :
			 proxy_type == PURPLE_PROXY_TYPE_TOR        ? "tor" :
			 proxy_type == PURPLE_PROXY_TYPE_USE_ENVVAR ? "envvar" : "unknown"), -1);

	if ((value = purple_proxy_info_get_hostname(proxy_info)) != NULL)
	{
		child = purple_xmlnode_new_child(node, "host");
		purple_xmlnode_insert_data(child, value, -1);
	}

	if ((int_value = purple_proxy_info_get_port(proxy_info)) != 0)
	{
		g_snprintf(buf, sizeof(buf), "%d", int_value);
		child = purple_xmlnode_new_child(node, "port");
		purple_xmlnode_insert_data(child, buf, -1);
	}

	if ((value = purple_proxy_info_get_username(proxy_info)) != NULL)
	{
		child = purple_xmlnode_new_child(node, "username");
		purple_xmlnode_insert_data(child, value, -1);
	}

	if ((value = purple_proxy_info_get_password(proxy_info)) != NULL)
	{
		child = purple_xmlnode_new_child(node, "password");
		purple_xmlnode_insert_data(child, value, -1);
	}

	return node;
}

static PurpleXmlNode *
current_error_to_xmlnode(PurpleConnectionErrorInfo *err)
{
	PurpleXmlNode *node, *child;
	char type_str[3];

	node = purple_xmlnode_new("current_error");

	if(err == NULL)
		return node;

	/* It doesn't make sense to have transient errors persist across a
	 * restart.
	 */
	if(!purple_connection_error_is_fatal (err->type))
		return node;

	child = purple_xmlnode_new_child(node, "type");
	g_snprintf(type_str, sizeof(type_str), "%u", err->type);
	purple_xmlnode_insert_data(child, type_str, -1);

	child = purple_xmlnode_new_child(node, "description");
	if(err->description) {
		char *utf8ized = purple_utf8_try_convert(err->description);
		if(utf8ized == NULL) {
			utf8ized = g_utf8_make_valid(err->description, -1);
		}
		purple_xmlnode_insert_data(child, utf8ized, -1);
		g_free(utf8ized);
	}

	return node;
}

static void
setting_to_xmlnode(gpointer key, gpointer value, gpointer user_data)
{
	const char *name;
	PurpleAccountSetting *setting;
	PurpleXmlNode *node, *child;
	char buf[21];

	name    = (const char *)key;
	setting = (PurpleAccountSetting *)value;
	node    = (PurpleXmlNode *)user_data;

	child = purple_xmlnode_new_child(node, "setting");
	purple_xmlnode_set_attrib(child, "name", name);

	if (G_VALUE_HOLDS_INT(&setting->value)) {
		purple_xmlnode_set_attrib(child, "type", "int");
		g_snprintf(buf, sizeof(buf), "%d", g_value_get_int(&setting->value));
		purple_xmlnode_insert_data(child, buf, -1);
	}
	else if (G_VALUE_HOLDS_STRING(&setting->value) && g_value_get_string(&setting->value) != NULL) {
		purple_xmlnode_set_attrib(child, "type", "string");
		purple_xmlnode_insert_data(child, g_value_get_string(&setting->value), -1);
	}
	else if (G_VALUE_HOLDS_BOOLEAN(&setting->value)) {
		purple_xmlnode_set_attrib(child, "type", "bool");
		g_snprintf(buf, sizeof(buf), "%d", g_value_get_boolean(&setting->value));
		purple_xmlnode_insert_data(child, buf, -1);
	}
}

PurpleXmlNode *
_purple_account_to_xmlnode(PurpleAccount *account)
{
	PurpleXmlNode *node, *child;
	gchar *data = NULL;
	const char *tmp;
	PurpleProxyInfo *proxy_info;
	PurpleAccountPrivate *priv = purple_account_get_instance_private(account);

	node = purple_xmlnode_new("account");

	if(priv->id != NULL) {
		child = purple_xmlnode_new_child(node, "id");
		purple_xmlnode_insert_data(child, priv->id, -1);
	}

	child = purple_xmlnode_new_child(node, "protocol");
	purple_xmlnode_insert_data(child, purple_account_get_protocol_id(account), -1);

	child = purple_xmlnode_new_child(node, "name");
	purple_xmlnode_insert_data(child, purple_account_get_username(account), -1);

	child = purple_xmlnode_new_child(node, "enabled");
	data = g_strdup_printf("%d", priv->enabled);
	purple_xmlnode_insert_data(child, data, -1);
	g_clear_pointer(&data, g_free);

	if ((tmp = purple_account_get_private_alias(account)) != NULL)
	{
		child = purple_xmlnode_new_child(node, "alias");
		purple_xmlnode_insert_data(child, tmp, -1);
	}

	if ((tmp = purple_account_get_user_info(account)) != NULL)
	{
		/* TODO: Do we need to call purple_str_strip_char(tmp, '\r') here? */
		child = purple_xmlnode_new_child(node, "user-info");
		purple_xmlnode_insert_data(child, tmp, -1);
	}

	if (g_hash_table_size(priv->settings) > 0)
	{
		child = purple_xmlnode_new_child(node, "settings");
		g_hash_table_foreach(priv->settings, setting_to_xmlnode, child);
	}

	if ((proxy_info = purple_account_get_proxy_info(account)) != NULL)
	{
		child = proxy_settings_to_xmlnode(proxy_info);
		purple_xmlnode_insert_child(node, child);
	}

	child = current_error_to_xmlnode(priv->current_error);
	purple_xmlnode_insert_child(node, child);

	return node;
}

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
purple_account_set_id(PurpleAccount *account, const gchar *id) {
	PurpleAccountPrivate *priv = purple_account_get_instance_private(account);

	g_free(priv->id);
	priv->id = g_strdup(id);

	g_object_notify_by_pspec(G_OBJECT(account), properties[PROP_ID]);
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
static void
purple_account_set_property(GObject *obj, guint param_id, const GValue *value,
                            GParamSpec *pspec)
{
	PurpleAccount *account = PURPLE_ACCOUNT(obj);

	switch (param_id) {
		case PROP_ID:
			purple_account_set_id(account, g_value_get_string(value));
			break;
		case PROP_USERNAME:
			purple_account_set_username(account, g_value_get_string(value));
			break;
		case PROP_PRIVATE_ALIAS:
			purple_account_set_private_alias(account, g_value_get_string(value));
			break;
		case PROP_ENABLED:
			purple_account_set_enabled(account, g_value_get_boolean(value));
			break;
		case PROP_CONNECTION:
			purple_account_set_connection(account, g_value_get_object(value));
			break;
		case PROP_PROTOCOL_ID:
			purple_account_set_protocol_id(account, g_value_get_string(value));
			break;
		case PROP_USER_INFO:
			purple_account_set_user_info(account, g_value_get_string(value));
			break;
		case PROP_BUDDY_ICON_PATH:
			purple_account_set_buddy_icon_path(account,
					g_value_get_string(value));
			break;
		case PROP_REMEMBER_PASSWORD:
			purple_account_set_remember_password(account,
					g_value_get_boolean(value));
			break;
		case PROP_PROXY_INFO:
			purple_account_set_proxy_info(account, g_value_get_object(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_account_get_property(GObject *obj, guint param_id, GValue *value,
                            GParamSpec *pspec)
{
	PurpleAccount *account = PURPLE_ACCOUNT(obj);

	switch (param_id) {
		case PROP_ID:
			g_value_set_string(value, purple_account_get_id(account));
			break;
		case PROP_USERNAME:
			g_value_set_string(value, purple_account_get_username(account));
			break;
		case PROP_PRIVATE_ALIAS:
			g_value_set_string(value, purple_account_get_private_alias(account));
			break;
		case PROP_ENABLED:
			g_value_set_boolean(value, purple_account_get_enabled(account));
			break;
		case PROP_CONNECTION:
			g_value_set_object(value, purple_account_get_connection(account));
			break;
		case PROP_PROTOCOL_ID:
			g_value_set_string(value, purple_account_get_protocol_id(account));
			break;
		case PROP_USER_INFO:
			g_value_set_string(value, purple_account_get_user_info(account));
			break;
		case PROP_BUDDY_ICON_PATH:
			g_value_set_string(value,
					purple_account_get_buddy_icon_path(account));
			break;
		case PROP_REMEMBER_PASSWORD:
			g_value_set_boolean(value,
					purple_account_get_remember_password(account));
			break;
		case PROP_PROXY_INFO:
			g_value_set_object(value, purple_account_get_proxy_info(account));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_account_init(PurpleAccount *account)
{
	PurpleAccountPrivate *priv = purple_account_get_instance_private(account);

	priv->settings = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, delete_setting);

	priv->privacy_type = PURPLE_ACCOUNT_PRIVACY_ALLOW_ALL;
}

static void
purple_account_constructed(GObject *object)
{
	PurpleAccount *account = PURPLE_ACCOUNT(object);
	PurpleAccountPrivate *priv = purple_account_get_instance_private(account);
	gchar *username, *protocol_id;
	PurpleProtocol *protocol = NULL;
	PurpleProtocolManager *manager = NULL;
	PurpleStatusType *status_type;

	G_OBJECT_CLASS(purple_account_parent_class)->constructed(object);

	/* If we didn't get an id, just generate a random one. */
	if(priv->id == NULL) {
		gchar *uuid = g_uuid_string_random();

		purple_account_set_id(account, uuid);

		g_free(uuid);
	}

	g_object_get(object,
			"username",    &username,
			"protocol-id", &protocol_id,
			NULL);

	purple_signal_emit(purple_accounts_get_handle(), "account-created",
			account);

	manager = purple_protocol_manager_get_default();
	protocol = purple_protocol_manager_find(manager, protocol_id);
	if (protocol == NULL) {
		g_free(username);
		g_free(protocol_id);
		return;
	}

	purple_account_set_status_types(account,
			purple_protocol_get_status_types(protocol, account));

	priv->presence = PURPLE_PRESENCE(purple_account_presence_new(account));

	status_type = purple_account_get_status_type_with_primitive(account,
			PURPLE_STATUS_AVAILABLE);
	if (status_type != NULL)
		purple_presence_set_status_active(priv->presence,
										purple_status_type_get_id(status_type),
										TRUE);
	else
		purple_presence_set_status_active(priv->presence,
										"offline",
										TRUE);

	g_free(username);
	g_free(protocol_id);
}

static void
purple_account_dispose(GObject *object)
{
	PurpleAccount *account = PURPLE_ACCOUNT(object);
	PurpleAccountPrivate *priv = purple_account_get_instance_private(account);

	if (!purple_account_is_disconnected(account))
		purple_account_disconnect(account);

	if (priv->presence) {
		g_object_unref(priv->presence);
		priv->presence = NULL;
	}

	G_OBJECT_CLASS(purple_account_parent_class)->dispose(object);
}

static void
purple_account_finalize(GObject *object)
{
	GList *l;
	PurpleAccount *account = PURPLE_ACCOUNT(object);
	PurpleAccountPrivate *priv = purple_account_get_instance_private(account);
	PurpleConversationManager *manager = NULL;

	purple_debug_info("account", "Destroying account %p\n", account);
	purple_signal_emit(purple_accounts_get_handle(), "account-destroying",
						account);

	manager = purple_conversation_manager_get_default();
	l = purple_conversation_manager_get_all(manager);
	while(l != NULL) {
		PurpleConversation *conv = PURPLE_CONVERSATION(l->data);

		if (purple_conversation_get_account(conv) == account) {
			purple_conversation_set_account(conv, NULL);
		}

		l = g_list_delete_link(l, l);
	}

	purple_account_set_status_types(account, NULL);

	g_clear_object(&priv->proxy_info);

	if (priv->current_error) {
		g_free(priv->current_error->description);
		g_free(priv->current_error);
	}

	g_clear_object(&priv->error_notification);

	g_free(priv->id);
	g_free(priv->username);
	g_free(priv->alias);
	g_free(priv->user_info);
	g_free(priv->buddy_icon_path);
	g_free(priv->protocol_id);

	g_hash_table_destroy(priv->settings);

	g_slist_free_full(priv->deny, g_free);
	g_slist_free_full(priv->permit, g_free);

	G_OBJECT_CLASS(purple_account_parent_class)->finalize(object);
}

static void
purple_account_class_init(PurpleAccountClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->dispose = purple_account_dispose;
	obj_class->finalize = purple_account_finalize;
	obj_class->constructed = purple_account_constructed;

	/* Setup properties */
	obj_class->get_property = purple_account_get_property;
	obj_class->set_property = purple_account_set_property;

	/**
	 * PurpleAccount::id
	 *
	 * An identifier for the account.
	 *
	 * Since: 3.0.0
	 */
	properties[PROP_ID] = g_param_spec_string(
		"id", "id",
		"The identifier of the account",
		NULL,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

	properties[PROP_USERNAME] = g_param_spec_string("username", "Username",
				"The username for the account.", NULL,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	properties[PROP_PRIVATE_ALIAS] = g_param_spec_string("private-alias",
				"Private Alias",
				"The private alias for the account.", NULL,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_USER_INFO] = g_param_spec_string("user-info",
				"User information",
				"Detailed user information for the account.", NULL,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_BUDDY_ICON_PATH] = g_param_spec_string("buddy-icon-path",
				"Buddy icon path",
				"Path to the buddyicon for the account.", NULL,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_ENABLED] = g_param_spec_boolean("enabled", "Enabled",
				"Whether the account is enabled or not.", FALSE,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_REMEMBER_PASSWORD] = g_param_spec_boolean(
				"remember-password", "Remember password",
				"Whether to remember and store the password for this account.",
				FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_CONNECTION] = g_param_spec_object("connection",
				"Connection",
				"The connection for the account.", PURPLE_TYPE_CONNECTION,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	properties[PROP_PROTOCOL_ID] = g_param_spec_string("protocol-id",
				"Protocol ID",
				"ID of the protocol that is responsible for the account.", NULL,
				G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
				G_PARAM_STATIC_STRINGS);

	properties[PROP_PROXY_INFO] = g_param_spec_object(
		"proxy-info", "proxy-info",
		"The PurpleProxyInfo for this account.",
		PURPLE_TYPE_PROXY_INFO,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, PROP_LAST, properties);
}

/******************************************************************************
 * Public API
 *****************************************************************************/
PurpleAccount *
purple_account_new(const gchar *username, const gchar *protocol_id) {
	PurpleAccount *account;
	PurpleAccountManager *manager = NULL;

	g_return_val_if_fail(username != NULL, NULL);
	g_return_val_if_fail(protocol_id != NULL, NULL);

	manager = purple_account_manager_get_default();
	account = purple_account_manager_find(manager, username, protocol_id);
	if(account != NULL) {
		return account;
	}

	account = g_object_new(
		PURPLE_TYPE_ACCOUNT,
		"username", username,
		"protocol-id", protocol_id,
		"enabled", FALSE,
		NULL);

	return account;
}

const gchar *
purple_account_get_id(PurpleAccount *account) {
	PurpleAccountPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);

	priv = purple_account_get_instance_private(account);

	return priv->id;
}

void
purple_account_connect(PurpleAccount *account)
{
	PurpleCredentialManager *manager = NULL;
	PurpleProtocol *protocol = NULL;
	const char *username = NULL;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));

	username = purple_account_get_username(account);

	if (!purple_account_get_enabled(account)) {
		purple_debug_info("account",
				  "Account %s not enabled, not connecting.\n",
				  username);
		return;
	}

	protocol = purple_account_get_protocol(account);
	if (protocol == NULL) {
		gchar *message;

		message = g_strdup_printf(_("Missing protocol for %s"), username);
		purple_notify_error(account, _("Connection Error"), message,
			NULL, purple_request_cpar_from_account(account));
		g_free(message);
		return;
	}

	purple_debug_info("account", "Connecting to account %s.\n", username);

	manager = purple_credential_manager_get_default();
	purple_credential_manager_read_password_async(manager, account, NULL,
	                                              purple_account_connect_got_password_cb,
	                                              account);
}

void
purple_account_set_register_callback(PurpleAccount *account, PurpleAccountRegistrationCb cb, void *user_data)
{
	PurpleAccountPrivate *priv;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));

	priv = purple_account_get_instance_private(account);

	priv->registration_cb = cb;
	priv->registration_cb_user_data = user_data;
}

void
purple_account_register_completed(PurpleAccount *account, gboolean succeeded)
{
	struct register_completed_closure *closure;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));

	closure = g_new0(struct register_completed_closure, 1);
	closure->account = g_object_ref(account);
	closure->succeeded = succeeded;

	g_timeout_add(0, purple_account_register_completed_cb, closure);
}

void
purple_account_unregister(PurpleAccount *account,
                          PurpleAccountUnregistrationCb cb, gpointer user_data)
{
	PurpleCallbackBundle *cbb;
	PurpleCredentialManager *manager = NULL;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));

	purple_debug_info("account", "Unregistering account %s\n",
					  purple_account_get_username(account));

	cbb = g_new0(PurpleCallbackBundle, 1);
	cbb->account = account;
	cbb->cb = G_CALLBACK(cb);
	cbb->data = user_data;

	manager = purple_credential_manager_get_default();
	purple_credential_manager_read_password_async(manager, account, NULL,
	                                              purple_account_unregister_got_password_cb,
	                                              cbb);
}

void
purple_account_disconnect(PurpleAccount *account)
{
	PurpleAccountPrivate *priv;
	const char *username;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));
	g_return_if_fail(!purple_account_is_disconnecting(account));
	g_return_if_fail(!purple_account_is_disconnected(account));

	priv = purple_account_get_instance_private(account);

	username = purple_account_get_username(account);
	purple_debug_info("account", "Disconnecting account %s (%p)\n",
	                  username ? username : "(null)", account);

	priv->disconnecting = TRUE;

	purple_account_set_connection(account, NULL);

	priv->disconnecting = FALSE;
}

gboolean
purple_account_is_disconnecting(PurpleAccount *account)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), TRUE);

	priv = purple_account_get_instance_private(account);
	return priv->disconnecting;
}

void
purple_account_notify_added(PurpleAccount *account, const char *remote_user,
                          const char *id, const char *alias,
                          const char *message)
{
	PurpleAccountUiOps *ui_ops;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));
	g_return_if_fail(remote_user != NULL);

	ui_ops = purple_accounts_get_ui_ops();

	if (ui_ops != NULL && ui_ops->notify_added != NULL)
		ui_ops->notify_added(account, remote_user, id, alias, message);
}

void
purple_account_request_add(PurpleAccount *account, const char *remote_user,
                         const char *id, const char *alias,
                         const char *message)
{
	PurpleAccountUiOps *ui_ops;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));
	g_return_if_fail(remote_user != NULL);

	ui_ops = purple_accounts_get_ui_ops();

	if (ui_ops != NULL && ui_ops->request_add != NULL)
		ui_ops->request_add(account, remote_user, id, alias, message);
}

void *
purple_account_request_authorization(PurpleAccount *account, const char *remote_user,
				     const char *id, const char *alias, const char *message, gboolean on_list,
				     PurpleAccountRequestAuthorizationCb auth_cb, PurpleAccountRequestAuthorizationCb deny_cb, void *user_data)
{
	PurpleAccountUiOps *ui_ops;
	PurpleAccountRequestInfo *info;
	int plugin_return;
	char *response = NULL;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);
	g_return_val_if_fail(remote_user != NULL, NULL);

	ui_ops = purple_accounts_get_ui_ops();

	plugin_return = GPOINTER_TO_INT(
			purple_signal_emit_return_1(
				purple_accounts_get_handle(),
				"account-authorization-requested",
				account, remote_user, message, &response
			));

	switch (plugin_return)
	{
		case PURPLE_ACCOUNT_RESPONSE_IGNORE:
			g_free(response);
			return NULL;
		case PURPLE_ACCOUNT_RESPONSE_ACCEPT:
			if (auth_cb != NULL)
				auth_cb(response, user_data);
			g_free(response);
			return NULL;
		case PURPLE_ACCOUNT_RESPONSE_DENY:
			if (deny_cb != NULL)
				deny_cb(response, user_data);
			g_free(response);
			return NULL;
	}

	g_free(response);

	if (ui_ops != NULL && ui_ops->request_authorize != NULL) {
		info            = g_new0(PurpleAccountRequestInfo, 1);
		info->type      = PURPLE_ACCOUNT_REQUEST_AUTHORIZATION;
		info->account   = account;
		info->auth_cb   = auth_cb;
		info->deny_cb   = deny_cb;
		info->userdata  = user_data;
		info->user      = g_strdup(remote_user);
		info->ref       = 2;  /* We hold an extra ref to make sure info remains valid
		                         if any of the callbacks are called synchronously. We
		                         unref it after the function call */

		info->ui_handle = ui_ops->request_authorize(account, remote_user, id, alias, message,
							    on_list, request_auth_cb, request_deny_cb, info);

		info = purple_account_request_info_unref(info);
		if (info) {
			handles = g_list_append(handles, info);
			return info->ui_handle;
		}
	}

	return NULL;
}

void
purple_account_request_close_with_account(PurpleAccount *account)
{
	GList *l, *l_next;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));

	for (l = handles; l != NULL; l = l_next) {
		PurpleAccountRequestInfo *info = l->data;

		l_next = l->next;

		if (info->account == account) {
			handles = g_list_delete_link(handles, l);
			purple_account_request_close_info(info);
		}
	}
}

void
purple_account_request_close(void *ui_handle)
{
	GList *l, *l_next;

	g_return_if_fail(ui_handle != NULL);

	for (l = handles; l != NULL; l = l_next) {
		PurpleAccountRequestInfo *info = l->data;

		l_next = l->next;

		if (info->ui_handle == ui_handle) {
			handles = g_list_delete_link(handles, l);
			purple_account_request_close_info(info);
		}
	}
}

void
purple_account_request_password(PurpleAccount *account, GCallback ok_cb,
				GCallback cancel_cb, void *user_data)
{
	gchar *primary;
	const gchar *username;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;
	PurpleRequestFields *fields;

	/* Close any previous password request windows */
	purple_request_close_with_handle(account);

	username = purple_account_get_username(account);
	primary = g_strdup_printf(_("Enter password for %s (%s)"), username,
								  purple_account_get_protocol_name(account));

	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_string_new("password", _("Enter Password"), NULL, FALSE);
	purple_request_field_string_set_masked(field, TRUE);
	purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_bool_new("remember", _("Save password"), FALSE);
	purple_request_field_group_add_field(group, field);

	purple_request_fields(account, NULL, primary, NULL, fields, _("OK"),
		ok_cb, _("Cancel"), cancel_cb,
		purple_request_cpar_from_account(account), user_data);
	g_free(primary);
}

void
purple_account_request_change_password(PurpleAccount *account)
{
	PurpleRequestFields *fields;
	PurpleRequestFieldGroup *group;
	PurpleRequestField *field;
	PurpleConnection *gc;
	PurpleProtocol *protocol = NULL;
	char primary[256];

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));
	g_return_if_fail(purple_account_is_connected(account));

	gc = purple_account_get_connection(account);
	if (gc != NULL)
		protocol = purple_connection_get_protocol(gc);

	fields = purple_request_fields_new();

	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_string_new("password", _("Original password"),
										  NULL, FALSE);
	purple_request_field_string_set_masked(field, TRUE);
	if (!protocol || !(purple_protocol_get_options(protocol) & OPT_PROTO_PASSWORD_OPTIONAL))
		purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_string_new("new_password_1",
										  _("New password"),
										  NULL, FALSE);
	purple_request_field_string_set_masked(field, TRUE);
	if (!protocol || !(purple_protocol_get_options(protocol) & OPT_PROTO_PASSWORD_OPTIONAL))
		purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	field = purple_request_field_string_new("new_password_2",
										  _("New password (again)"),
										  NULL, FALSE);
	purple_request_field_string_set_masked(field, TRUE);
	if (!protocol || !(purple_protocol_get_options(protocol) & OPT_PROTO_PASSWORD_OPTIONAL))
		purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	g_snprintf(primary, sizeof(primary), _("Change password for %s"),
			   purple_account_get_username(account));

	/* I'm sticking this somewhere in the code: bologna */

	purple_request_fields(purple_account_get_connection(account), NULL,
		primary, _("Please enter your current password and your new "
		"password."), fields, _("OK"), G_CALLBACK(change_password_cb),
		_("Cancel"), NULL, purple_request_cpar_from_account(account),
		account);
}

void
purple_account_request_change_user_info(PurpleAccount *account)
{
	PurpleConnection *gc;
	char primary[256];

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));
	g_return_if_fail(purple_account_is_connected(account));

	gc = purple_account_get_connection(account);

	g_snprintf(primary, sizeof(primary),
			   _("Change user information for %s"),
			   purple_account_get_username(account));

	purple_request_input(gc, _("Set User Info"), primary, NULL,
					   purple_account_get_user_info(account),
					   TRUE, FALSE, ((gc != NULL) &&
					   (purple_connection_get_flags(gc) & PURPLE_CONNECTION_FLAG_HTML) ? "html" : NULL),
					   _("Save"), G_CALLBACK(set_user_info_cb),
					   _("Cancel"), NULL,
					   purple_request_cpar_from_account(account),
					   account);
}

void
purple_account_set_username(PurpleAccount *account, const char *username)
{
	PurpleAccountPrivate *priv;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));

	priv = purple_account_get_instance_private(account);

	g_free(priv->username);
	priv->username = g_strdup(username);

	g_object_notify_by_pspec(G_OBJECT(account), properties[PROP_USERNAME]);

	purple_accounts_schedule_save();

	/* if the name changes, we should re-write the buddy list
	 * to disk with the new name */
	purple_blist_save_account(purple_blist_get_default(), account);
}

void
purple_account_set_private_alias(PurpleAccount *account, const char *alias)
{
	PurpleAccountPrivate *priv;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));

	priv = purple_account_get_instance_private(account);

	/*
	 * Do nothing if alias and priv->alias are both NULL.  Or if
	 * they're the exact same string.
	 */
	if (alias == priv->alias)
		return;

	if ((!alias && priv->alias) || (alias && !priv->alias) ||
			g_utf8_collate(priv->alias, alias))
	{
		char *old = priv->alias;

		priv->alias = g_strdup(alias);
		g_object_notify_by_pspec(G_OBJECT(account),
						 properties[PROP_PRIVATE_ALIAS]);
		purple_signal_emit(purple_accounts_get_handle(), "account-alias-changed",
						 account, old);
		g_free(old);

		purple_accounts_schedule_save();
	}
}

void
purple_account_set_user_info(PurpleAccount *account, const char *user_info)
{
	PurpleAccountPrivate *priv;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));

	priv = purple_account_get_instance_private(account);

	g_free(priv->user_info);
	priv->user_info = g_strdup(user_info);

	g_object_notify_by_pspec(G_OBJECT(account), properties[PROP_USER_INFO]);

	purple_accounts_schedule_save();
}

void purple_account_set_buddy_icon_path(PurpleAccount *account, const char *path)
{
	PurpleAccountPrivate *priv;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));

	priv = purple_account_get_instance_private(account);

	g_free(priv->buddy_icon_path);
	priv->buddy_icon_path = g_strdup(path);

	g_object_notify_by_pspec(G_OBJECT(account),
			properties[PROP_BUDDY_ICON_PATH]);

	purple_accounts_schedule_save();
}

void
purple_account_set_protocol_id(PurpleAccount *account, const char *protocol_id)
{
	PurpleAccountPrivate *priv;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));
	g_return_if_fail(protocol_id != NULL);

	priv = purple_account_get_instance_private(account);

	g_free(priv->protocol_id);
	priv->protocol_id = g_strdup(protocol_id);

	g_object_notify_by_pspec(G_OBJECT(account), properties[PROP_PROTOCOL_ID]);

	purple_accounts_schedule_save();
}

void
purple_account_set_connection(PurpleAccount *account, PurpleConnection *gc)
{
	PurpleAccountPrivate *priv;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));

	priv = purple_account_get_instance_private(account);

	g_clear_object(&priv->gc);
	priv->gc = gc;

	g_object_notify_by_pspec(G_OBJECT(account), properties[PROP_CONNECTION]);
}

void
purple_account_set_remember_password(PurpleAccount *account, gboolean value)
{
	PurpleAccountPrivate *priv;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));

	priv = purple_account_get_instance_private(account);
	priv->remember_pass = value;

	g_object_notify_by_pspec(G_OBJECT(account),
			properties[PROP_REMEMBER_PASSWORD]);

	purple_accounts_schedule_save();
}

void
purple_account_set_enabled(PurpleAccount *account, gboolean value) {
	PurpleConnection *gc;
	PurpleAccountPrivate *priv;
	gboolean was_enabled = FALSE;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));

	priv = purple_account_get_instance_private(account);

	was_enabled = priv->enabled;
	priv->enabled = value;

	gc = purple_account_get_connection(account);
	if(was_enabled && !value)
		purple_signal_emit(purple_accounts_get_handle(), "account-disabled", account);
	else if(!was_enabled && value)
		purple_signal_emit(purple_accounts_get_handle(), "account-enabled", account);

	g_object_notify_by_pspec(G_OBJECT(account), properties[PROP_ENABLED]);

	if ((gc != NULL) && (_purple_connection_wants_to_die(gc)))
		return;

	if (value && purple_presence_is_online(priv->presence))
		purple_account_connect(account);
	else if (!value && !purple_account_is_disconnected(account))
		purple_account_disconnect(account);

	purple_accounts_schedule_save();
}

void
purple_account_set_proxy_info(PurpleAccount *account, PurpleProxyInfo *info)
{
	PurpleAccountPrivate *priv;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));

	priv = purple_account_get_instance_private(account);

	if(g_set_object(&priv->proxy_info, info)) {
		g_object_notify_by_pspec(G_OBJECT(account),
		                         properties[PROP_PROXY_INFO]);

		purple_accounts_schedule_save();
	}
}

void
purple_account_set_privacy_type(PurpleAccount *account, PurpleAccountPrivacyType privacy_type)
{
	PurpleAccountPrivate *priv;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));

	priv = purple_account_get_instance_private(account);
	priv->privacy_type = privacy_type;
}

void
purple_account_set_status_types(PurpleAccount *account, GList *status_types)
{
	PurpleAccountPrivate *priv;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));

	priv = purple_account_get_instance_private(account);

	/* Out with the old... */
	g_list_free_full(priv->status_types,
	                 (GDestroyNotify)purple_status_type_destroy);

	/* In with the new... */
	priv->status_types = status_types;
}

void
purple_account_set_status(PurpleAccount *account, const char *status_id,
						gboolean active, ...)
{
	GHashTable *attrs;
	va_list args;

	va_start(args, active);
	attrs = purple_attrs_from_vargs(args);
	purple_account_set_status_attrs(account, status_id, active, attrs);
	g_hash_table_destroy(attrs);
	va_end(args);
}

void
purple_account_set_status_attrs(PurpleAccount *account, const char *status_id,
							 gboolean active, GHashTable *attrs)
{
	PurpleStatus *status;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));
	g_return_if_fail(status_id != NULL);

	status = purple_account_get_status(account, status_id);
	if (status == NULL)
	{
		purple_debug_error("account",
				   "Invalid status ID '%s' for account %s (%s)\n",
				   status_id, purple_account_get_username(account),
				   purple_account_get_protocol_id(account));
		return;
	}

	if (active || purple_status_is_independent(status))
		purple_status_set_active_with_attrs_dict(status, active, attrs);

	/*
	 * Our current statuses are saved to accounts.xml (so that when we
	 * reconnect, we go back to the previous status).
	 */
	purple_accounts_schedule_save();
}

gboolean
purple_account_get_silence_suppression(PurpleAccount *account)
{
	return purple_account_get_bool(account, "silence-suppression", FALSE);
}

void
purple_account_set_silence_suppression(PurpleAccount *account, gboolean value)
{
	g_return_if_fail(PURPLE_IS_ACCOUNT(account));

	purple_account_set_bool(account, "silence-suppression", value);
}

void
purple_account_clear_settings(PurpleAccount *account)
{
	PurpleAccountPrivate *priv;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));

	priv = purple_account_get_instance_private(account);
	g_hash_table_destroy(priv->settings);

	priv->settings = g_hash_table_new_full(g_str_hash, g_str_equal,
											  g_free, delete_setting);
}

void
purple_account_remove_setting(PurpleAccount *account, const char *setting)
{
	PurpleAccountPrivate *priv;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));
	g_return_if_fail(setting != NULL);

	priv = purple_account_get_instance_private(account);

	g_hash_table_remove(priv->settings, setting);
}

void
purple_account_set_int(PurpleAccount *account, const char *name, int value)
{
	PurpleAccountSetting *setting;
	PurpleAccountPrivate *priv;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));
	g_return_if_fail(name    != NULL);

	priv = purple_account_get_instance_private(account);

	setting = g_new0(PurpleAccountSetting, 1);

	g_value_init(&setting->value, G_TYPE_INT);
	g_value_set_int(&setting->value, value);

	g_hash_table_insert(priv->settings, g_strdup(name), setting);

	purple_accounts_schedule_save();
}

void
purple_account_set_string(PurpleAccount *account, const char *name,
						const char *value)
{
	PurpleAccountSetting *setting;
	PurpleAccountPrivate *priv;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));
	g_return_if_fail(name    != NULL);

	priv = purple_account_get_instance_private(account);

	setting = g_new0(PurpleAccountSetting, 1);

	g_value_init(&setting->value, G_TYPE_STRING);
	g_value_set_string(&setting->value, value);

	g_hash_table_insert(priv->settings, g_strdup(name), setting);

	purple_accounts_schedule_save();
}

void
purple_account_set_bool(PurpleAccount *account, const char *name, gboolean value)
{
	PurpleAccountSetting *setting;
	PurpleAccountPrivate *priv;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));
	g_return_if_fail(name    != NULL);

	priv = purple_account_get_instance_private(account);

	setting = g_new0(PurpleAccountSetting, 1);

	g_value_init(&setting->value, G_TYPE_BOOLEAN);
	g_value_set_boolean(&setting->value, value);

	g_hash_table_insert(priv->settings, g_strdup(name), setting);

	purple_accounts_schedule_save();
}

gboolean
purple_account_is_connected(PurpleAccount *account)
{
	return (purple_account_get_state(account) == PURPLE_CONNECTION_CONNECTED);
}

gboolean
purple_account_is_connecting(PurpleAccount *account)
{
	return (purple_account_get_state(account) == PURPLE_CONNECTION_CONNECTING);
}

gboolean
purple_account_is_disconnected(PurpleAccount *account)
{
	return (purple_account_get_state(account) == PURPLE_CONNECTION_DISCONNECTED);
}

const char *
purple_account_get_username(PurpleAccount *account)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);

	priv = purple_account_get_instance_private(account);
	return priv->username;
}

const char *
purple_account_get_private_alias(PurpleAccount *account)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);

	priv = purple_account_get_instance_private(account);
	return priv->alias;
}

const char *
purple_account_get_user_info(PurpleAccount *account)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);

	priv = purple_account_get_instance_private(account);
	return priv->user_info;
}

const char *
purple_account_get_buddy_icon_path(PurpleAccount *account)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);

	priv = purple_account_get_instance_private(account);
	return priv->buddy_icon_path;
}

const char *
purple_account_get_protocol_id(PurpleAccount *account)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);

	priv = purple_account_get_instance_private(account);
	return priv->protocol_id;
}

PurpleProtocol *
purple_account_get_protocol(PurpleAccount *account) {
	PurpleAccountPrivate *priv = NULL;
	PurpleProtocolManager *manager = NULL;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);

	priv = purple_account_get_instance_private(account);
	manager = purple_protocol_manager_get_default();

	return purple_protocol_manager_find(manager, priv->protocol_id);
}

const char *
purple_account_get_protocol_name(PurpleAccount *account)
{
	PurpleProtocol *p;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);

	p = purple_account_get_protocol(account);

	return (p && purple_protocol_get_name(p) ?
	        _(purple_protocol_get_name(p)) : _("Unknown"));
}

PurpleConnection *
purple_account_get_connection(PurpleAccount *account)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);

	priv = purple_account_get_instance_private(account);
	return priv->gc;
}

const gchar *
purple_account_get_name_for_display(PurpleAccount *account)
{
	PurpleBuddy *self = NULL;
	PurpleConnection *gc = NULL;
	const gchar *name = NULL, *username = NULL, *displayname = NULL;

	name = purple_account_get_private_alias(account);

	if (name) {
		return name;
	}

	username = purple_account_get_username(account);
	self = purple_blist_find_buddy((PurpleAccount *)account, username);

	if (self) {
		const gchar *calias= purple_buddy_get_contact_alias(self);

		/* We don't want to return the buddy name if the buddy/contact
		 * doesn't have an alias set. */
		if (!purple_strequal(username, calias)) {
			return calias;
		}
	}

	gc = purple_account_get_connection(account);
	displayname = purple_connection_get_display_name(gc);

	if (displayname) {
		return displayname;
	}

	return username;
}

gboolean
purple_account_get_remember_password(PurpleAccount *account)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), FALSE);

	priv = purple_account_get_instance_private(account);
	return priv->remember_pass;
}

gboolean
purple_account_get_enabled(PurpleAccount *account) {
	PurpleAccountPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), FALSE);

	priv = purple_account_get_instance_private(account);

	return priv->enabled;
}

PurpleProxyInfo *
purple_account_get_proxy_info(PurpleAccount *account)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);

	priv = purple_account_get_instance_private(account);
	return priv->proxy_info;
}

PurpleAccountPrivacyType
purple_account_get_privacy_type(PurpleAccount *account)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), PURPLE_ACCOUNT_PRIVACY_ALLOW_ALL);

	priv = purple_account_get_instance_private(account);
	return priv->privacy_type;
}

gboolean
purple_account_privacy_permit_add(PurpleAccount *account, const char *who,
						gboolean local_only)
{
	char *name;
	PurpleBuddy *buddy;
	PurpleAccountPrivate *priv;
	PurpleAccountUiOps *ui_ops = purple_accounts_get_ui_ops();

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), FALSE);
	g_return_val_if_fail(who     != NULL, FALSE);

	priv = purple_account_get_instance_private(account);
	name = g_strdup(purple_normalize(account, who));

	if (g_slist_find_custom(priv->permit, name, (GCompareFunc)g_strcmp0) != NULL) {
		/* This buddy already exists, so bail out */
		g_free(name);
		return FALSE;
	}

	priv->permit = g_slist_append(priv->permit, name);

	if (!local_only && purple_account_is_connected(account))
		purple_serv_add_permit(purple_account_get_connection(account), who);

	if (ui_ops != NULL && ui_ops->permit_added != NULL)
		ui_ops->permit_added(account, who);

	purple_blist_save_account(purple_blist_get_default(), account);

	/* This lets the UI know a buddy has had its privacy setting changed */
	buddy = purple_blist_find_buddy(account, name);
	if (buddy != NULL) {
		purple_signal_emit(purple_blist_get_handle(),
                "buddy-privacy-changed", buddy);
	}
	return TRUE;
}

gboolean
purple_account_privacy_permit_remove(PurpleAccount *account, const char *who,
						   gboolean local_only)
{
	GSList *l;
	const char *name;
	PurpleBuddy *buddy;
	char *del;
	PurpleAccountPrivate *priv;
	PurpleAccountUiOps *ui_ops = purple_accounts_get_ui_ops();

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), FALSE);
	g_return_val_if_fail(who     != NULL, FALSE);

	priv = purple_account_get_instance_private(account);
	name = purple_normalize(account, who);

	l = g_slist_find_custom(priv->permit, name, (GCompareFunc)g_strcmp0);
	if (l == NULL) {
		/* We didn't find the buddy we were looking for, so bail out */
		return FALSE;
	}

	/* We should not free l->data just yet. There can be occasions where
	 * l->data == who. In such cases, freeing l->data here can cause crashes
	 * later when who is used. */
	del = l->data;
	priv->permit = g_slist_delete_link(priv->permit, l);

	if (!local_only && purple_account_is_connected(account)) {
		purple_serv_remove_permit(purple_account_get_connection(account), who);
	}

	if (ui_ops != NULL && ui_ops->permit_removed != NULL) {
		ui_ops->permit_removed(account, who);
	}

	purple_blist_save_account(purple_blist_get_default(), account);

	buddy = purple_blist_find_buddy(account, name);
	if (buddy != NULL) {
		purple_signal_emit(purple_blist_get_handle(),
                "buddy-privacy-changed", buddy);
	}
	g_free(del);
	return TRUE;
}

gboolean
purple_account_privacy_deny_add(PurpleAccount *account, const char *who,
					  gboolean local_only)
{
	char *name;
	PurpleBuddy *buddy;
	PurpleAccountPrivate *priv;
	PurpleAccountUiOps *ui_ops = purple_accounts_get_ui_ops();

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), FALSE);
	g_return_val_if_fail(who     != NULL, FALSE);

	priv = purple_account_get_instance_private(account);
	name = g_strdup(purple_normalize(account, who));

	if (g_slist_find_custom(priv->deny, name, (GCompareFunc)g_strcmp0) != NULL) {
		/* This buddy already exists, so bail out */
		g_free(name);
		return FALSE;
	}

	priv->deny = g_slist_append(priv->deny, name);

	if (!local_only && purple_account_is_connected(account))
		purple_serv_add_deny(purple_account_get_connection(account), who);

	if (ui_ops != NULL && ui_ops->deny_added != NULL)
		ui_ops->deny_added(account, who);

	purple_blist_save_account(purple_blist_get_default(), account);

	buddy = purple_blist_find_buddy(account, name);
	if (buddy != NULL) {
		purple_signal_emit(purple_blist_get_handle(),
                "buddy-privacy-changed", buddy);
	}
	return TRUE;
}

gboolean
purple_account_privacy_deny_remove(PurpleAccount *account, const char *who,
						 gboolean local_only)
{
	GSList *l;
	const char *normalized;
	char *name;
	PurpleBuddy *buddy;
	PurpleAccountPrivate *priv;
	PurpleAccountUiOps *ui_ops = purple_accounts_get_ui_ops();

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), FALSE);
	g_return_val_if_fail(who     != NULL, FALSE);

	priv = purple_account_get_instance_private(account);
	normalized = purple_normalize(account, who);

	l = g_slist_find_custom(priv->deny, normalized, (GCompareFunc)g_strcmp0);
	if (l == NULL) {
		/* We didn't find the buddy we were looking for, so bail out */
		return FALSE;
	}

	buddy = purple_blist_find_buddy(account, normalized);

	name = l->data;
	priv->deny = g_slist_delete_link(priv->deny, l);

	if (!local_only && purple_account_is_connected(account)) {
		purple_serv_remove_deny(purple_account_get_connection(account), name);
	}

	if (ui_ops != NULL && ui_ops->deny_removed != NULL) {
		ui_ops->deny_removed(account, who);
	}

	if (buddy != NULL) {
		purple_signal_emit(purple_blist_get_handle(),
                "buddy-privacy-changed", buddy);
	}

	g_free(name);

	purple_blist_save_account(purple_blist_get_default(), account);

	return TRUE;
}

void
purple_account_privacy_allow(PurpleAccount *account, const char *who)
{
	GSList *list;
	PurpleAccountPrivacyType type = purple_account_get_privacy_type(account);
	PurpleAccountPrivate *priv = purple_account_get_instance_private(account);

	switch (type) {
		case PURPLE_ACCOUNT_PRIVACY_ALLOW_ALL:
			return;
		case PURPLE_ACCOUNT_PRIVACY_ALLOW_USERS:
			purple_account_privacy_permit_add(account, who, FALSE);
			break;
		case PURPLE_ACCOUNT_PRIVACY_DENY_USERS:
			purple_account_privacy_deny_remove(account, who, FALSE);
			break;
		case PURPLE_ACCOUNT_PRIVACY_DENY_ALL:
			{
				/* Empty the allow-list. */
				const char *norm = purple_normalize(account, who);
				for (list = priv->permit; list != NULL;) {
					char *person = list->data;
					list = list->next;
					if (!purple_strequal(norm, person))
						purple_account_privacy_permit_remove(account, person, FALSE);
				}
				purple_account_privacy_permit_add(account, who, FALSE);
				purple_account_set_privacy_type(account, PURPLE_ACCOUNT_PRIVACY_ALLOW_USERS);
			}
			break;
		case PURPLE_ACCOUNT_PRIVACY_ALLOW_BUDDYLIST:
			if (!purple_blist_find_buddy(account, who)) {
				add_all_buddies_to_permit_list(account, FALSE);
				purple_account_privacy_permit_add(account, who, FALSE);
				purple_account_set_privacy_type(account, PURPLE_ACCOUNT_PRIVACY_ALLOW_USERS);
			}
			break;
		default:
			g_return_if_reached();
	}

	/* Notify the server if the privacy setting was changed */
	if (type != purple_account_get_privacy_type(account) && purple_account_is_connected(account))
		purple_serv_set_permit_deny(purple_account_get_connection(account));
}

void
purple_account_privacy_deny(PurpleAccount *account, const char *who)
{
	GSList *list;
	PurpleAccountPrivacyType type = purple_account_get_privacy_type(account);
	PurpleAccountPrivate *priv = purple_account_get_instance_private(account);

	switch (type) {
		case PURPLE_ACCOUNT_PRIVACY_ALLOW_ALL:
			{
				/* Empty the deny-list. */
				const char *norm = purple_normalize(account, who);
				for (list = priv->deny; list != NULL; ) {
					char *person = list->data;
					list = list->next;
					if (!purple_strequal(norm, person))
						purple_account_privacy_deny_remove(account, person, FALSE);
				}
				purple_account_privacy_deny_add(account, who, FALSE);
				purple_account_set_privacy_type(account, PURPLE_ACCOUNT_PRIVACY_DENY_USERS);
			}
			break;
		case PURPLE_ACCOUNT_PRIVACY_ALLOW_USERS:
			purple_account_privacy_permit_remove(account, who, FALSE);
			break;
		case PURPLE_ACCOUNT_PRIVACY_DENY_USERS:
			purple_account_privacy_deny_add(account, who, FALSE);
			break;
		case PURPLE_ACCOUNT_PRIVACY_DENY_ALL:
			break;
		case PURPLE_ACCOUNT_PRIVACY_ALLOW_BUDDYLIST:
			if (purple_blist_find_buddy(account, who)) {
				add_all_buddies_to_permit_list(account, FALSE);
				purple_account_privacy_permit_remove(account, who, FALSE);
				purple_account_set_privacy_type(account, PURPLE_ACCOUNT_PRIVACY_ALLOW_USERS);
			}
			break;
		default:
			g_return_if_reached();
	}

	/* Notify the server if the privacy setting was changed */
	if (type != purple_account_get_privacy_type(account) && purple_account_is_connected(account))
		purple_serv_set_permit_deny(purple_account_get_connection(account));
}

GSList *
purple_account_privacy_get_permitted(PurpleAccount *account)
{
	PurpleAccountPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);

	priv = purple_account_get_instance_private(account);
	return priv->permit;
}

GSList *
purple_account_privacy_get_denied(PurpleAccount *account)
{
	PurpleAccountPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);

	priv = purple_account_get_instance_private(account);
	return priv->deny;
}

gboolean
purple_account_privacy_check(PurpleAccount *account, const char *who)
{
	PurpleAccountPrivate *priv = purple_account_get_instance_private(account);

	switch (purple_account_get_privacy_type(account)) {
		case PURPLE_ACCOUNT_PRIVACY_ALLOW_ALL:
			return TRUE;

		case PURPLE_ACCOUNT_PRIVACY_DENY_ALL:
			return FALSE;

		case PURPLE_ACCOUNT_PRIVACY_ALLOW_USERS:
			who = purple_normalize(account, who);
			return (g_slist_find_custom(priv->permit, who, (GCompareFunc)g_strcmp0) != NULL);

		case PURPLE_ACCOUNT_PRIVACY_DENY_USERS:
			who = purple_normalize(account, who);
			return (g_slist_find_custom(priv->deny, who, (GCompareFunc)g_strcmp0) == NULL);

		case PURPLE_ACCOUNT_PRIVACY_ALLOW_BUDDYLIST:
			return (purple_blist_find_buddy(account, who) != NULL);

		default:
			g_return_val_if_reached(TRUE);
	}
}

PurpleStatus *
purple_account_get_active_status(PurpleAccount *account)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);

	priv = purple_account_get_instance_private(account);
	return purple_presence_get_active_status(priv->presence);
}

PurpleStatus *
purple_account_get_status(PurpleAccount *account, const char *status_id)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);
	g_return_val_if_fail(status_id != NULL, NULL);

	priv = purple_account_get_instance_private(account);

	return purple_presence_get_status(priv->presence, status_id);
}

PurpleStatusType *
purple_account_get_status_type(PurpleAccount *account, const char *id)
{
	GList *l;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);
	g_return_val_if_fail(id != NULL, NULL);

	for (l = purple_account_get_status_types(account); l != NULL; l = l->next)
	{
		PurpleStatusType *status_type = (PurpleStatusType *)l->data;

		if (purple_strequal(purple_status_type_get_id(status_type), id))
			return status_type;
	}

	return NULL;
}

PurpleStatusType *
purple_account_get_status_type_with_primitive(PurpleAccount *account, PurpleStatusPrimitive primitive)
{
	GList *l;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);

	for (l = purple_account_get_status_types(account); l != NULL; l = l->next)
	{
		PurpleStatusType *status_type = (PurpleStatusType *)l->data;

		if (purple_status_type_get_primitive(status_type) == primitive)
			return status_type;
	}

	return NULL;
}

PurplePresence *
purple_account_get_presence(PurpleAccount *account)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);

	priv = purple_account_get_instance_private(account);
	return priv->presence;
}

gboolean
purple_account_is_status_active(PurpleAccount *account,
							  const char *status_id)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), FALSE);
	g_return_val_if_fail(status_id != NULL, FALSE);

	priv = purple_account_get_instance_private(account);

	return purple_presence_is_status_active(priv->presence, status_id);
}

GList *
purple_account_get_status_types(PurpleAccount *account)
{
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);

	priv = purple_account_get_instance_private(account);
	return priv->status_types;
}

int
purple_account_get_int(PurpleAccount *account, const char *name,
					 int default_value)
{
	PurpleAccountSetting *setting;
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), default_value);
	g_return_val_if_fail(name    != NULL, default_value);

	priv = purple_account_get_instance_private(account);

	setting = g_hash_table_lookup(priv->settings, name);

	if (setting == NULL)
		return default_value;

	g_return_val_if_fail(G_VALUE_HOLDS_INT(&setting->value), default_value);

	return g_value_get_int(&setting->value);
}

const char *
purple_account_get_string(PurpleAccount *account, const char *name,
						const char *default_value)
{
	PurpleAccountSetting *setting;
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), default_value);
	g_return_val_if_fail(name    != NULL, default_value);

	priv = purple_account_get_instance_private(account);

	setting = g_hash_table_lookup(priv->settings, name);

	if (setting == NULL)
		return default_value;

	g_return_val_if_fail(G_VALUE_HOLDS_STRING(&setting->value), default_value);

	return g_value_get_string(&setting->value);
}

gboolean
purple_account_get_bool(PurpleAccount *account, const char *name,
					  gboolean default_value)
{
	PurpleAccountSetting *setting;
	PurpleAccountPrivate *priv;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), default_value);
	g_return_val_if_fail(name    != NULL, default_value);

	priv = purple_account_get_instance_private(account);

	setting = g_hash_table_lookup(priv->settings, name);

	if (setting == NULL)
		return default_value;

	g_return_val_if_fail(G_VALUE_HOLDS_BOOLEAN(&setting->value), default_value);

	return g_value_get_boolean(&setting->value);
}

void
purple_account_add_buddy(PurpleAccount *account, PurpleBuddy *buddy, const char *message)
{
	PurpleProtocol *protocol = NULL;
	PurpleConnection *gc;

	g_return_if_fail(PURPLE_IS_ACCOUNT(account));
	g_return_if_fail(PURPLE_IS_BUDDY(buddy));

	gc = purple_account_get_connection(account);
	if (gc != NULL)
		protocol = purple_connection_get_protocol(gc);

	if(PURPLE_IS_PROTOCOL_SERVER(protocol)) {
		PurpleGroup *group = purple_buddy_get_group(buddy);

		purple_protocol_server_add_buddy(PURPLE_PROTOCOL_SERVER(protocol), gc,
		                                 buddy, group, message);
	}
}

void
purple_account_add_buddies(PurpleAccount *account, GList *buddies, const char *message)
{
	PurpleProtocol *protocol = NULL;
	PurpleConnection *gc = purple_account_get_connection(account);

	if (gc != NULL)
		protocol = purple_connection_get_protocol(gc);

	if (protocol) {
		GList *groups;

		/* Make a list of what group each buddy is in */
		groups = g_list_copy_deep(buddies, (GCopyFunc)purple_buddy_get_group, NULL);

		if(PURPLE_IS_PROTOCOL_SERVER(protocol)) {
			purple_protocol_server_add_buddies(PURPLE_PROTOCOL_SERVER(protocol),
			                                   gc, buddies, groups, message);
		}

		g_list_free(groups);
	}
}

void
purple_account_remove_buddy(PurpleAccount *account, PurpleBuddy *buddy,
		PurpleGroup *group)
{
	PurpleProtocol *protocol = NULL;
	PurpleConnection *gc = purple_account_get_connection(account);

	if (gc != NULL)
		protocol = purple_connection_get_protocol(gc);

	if(PURPLE_IS_PROTOCOL_SERVER(protocol)) {
		purple_protocol_server_remove_buddy(PURPLE_PROTOCOL_SERVER(protocol),
		                                    gc, buddy, group);
	}
}

void
purple_account_remove_buddies(PurpleAccount *account, GList *buddies, GList *groups)
{
	PurpleProtocol *protocol = NULL;
	PurpleConnection *gc = purple_account_get_connection(account);

	if (gc != NULL)
		protocol = purple_connection_get_protocol(gc);

	if(PURPLE_IS_PROTOCOL_SERVER(protocol)) {
		purple_protocol_server_remove_buddies(PURPLE_PROTOCOL_SERVER(protocol),
		                                      gc, buddies, groups);
	}
}

void
purple_account_remove_group(PurpleAccount *account, PurpleGroup *group)
{
	PurpleProtocol *protocol = NULL;
	PurpleConnection *gc = purple_account_get_connection(account);

	if (gc != NULL)
		protocol = purple_connection_get_protocol(gc);

	if(PURPLE_IS_PROTOCOL_SERVER(protocol)) {
		purple_protocol_server_remove_group(PURPLE_PROTOCOL_SERVER(protocol),
		                                    gc, group);
	}
}

void
purple_account_change_password(PurpleAccount *account, const char *orig_pw,
		const char *new_pw)
{
	PurpleCredentialManager *manager = NULL;
	PurpleProtocol *protocol = NULL;
	PurpleConnection *gc = purple_account_get_connection(account);

	/* just going to fire and forget this for now as not many protocols even
	 * implement the change password stuff.
	 */
	manager = purple_credential_manager_get_default();
	purple_credential_manager_write_password_async(manager, account, new_pw,
	                                               NULL, NULL, NULL);

	if (gc != NULL)
		protocol = purple_connection_get_protocol(gc);

	if(PURPLE_IS_PROTOCOL_SERVER(protocol)) {
		purple_protocol_server_change_passwd(PURPLE_PROTOCOL_SERVER(protocol),
		                                     gc, orig_pw, new_pw);
	}
}

gboolean purple_account_supports_offline_message(PurpleAccount *account, PurpleBuddy *buddy)
{
	PurpleConnection *gc;
	PurpleProtocol *protocol = NULL;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), FALSE);
	g_return_val_if_fail(PURPLE_IS_BUDDY(buddy), FALSE);

	gc = purple_account_get_connection(account);
	if(gc == NULL) {
		return FALSE;
	}

	protocol = purple_connection_get_protocol(gc);
	if(!protocol) {
		return FALSE;
	}

	return purple_protocol_client_offline_message(PURPLE_PROTOCOL_CLIENT(protocol), buddy);
}

const PurpleConnectionErrorInfo *
purple_account_get_current_error(PurpleAccount *account)
{
	PurpleAccountPrivate *priv = purple_account_get_instance_private(account);

	return priv->current_error;
}

void
purple_account_clear_current_error(PurpleAccount *account)
{
	_purple_account_set_current_error(account, NULL);
}
