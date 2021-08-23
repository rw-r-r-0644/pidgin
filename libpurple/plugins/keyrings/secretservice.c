/*
 * Purple - Internet Messaging Library
 * Copyright (C) Pidgin Developers <devel@pidgin.im>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <https://www.gnu.org/licenses/>.
 */

/* TODO
 *
 * This keyring now works (at the time of this writing), but there are
 * some inconvenient edge cases. When looking up passwords, libsecret
 * doesn't error if the keyring is locked. Therefore, it appears to
 * this plugin that there's no stored password. libpurple seems to
 * handle this as gracefully as possible, but it's still inconvenient.
 * This plugin could possibly be ported to use libsecret's "Complete API"
 * to resolve this if desired.
 */

#include <glib/gi18n-lib.h>

#include <gplugin.h>
#include <gplugin-native.h>

#include <purple.h>

#include <libsecret/secret.h>

/* Translators: Secret Service is a service that runs on the user's computer.
   It is one option for where the user's passwords can be stored. It is a
   project name. It may not be appropriate to translate this string, but
   transliterating to your alphabet is reasonable. More info about the
   project can be found at https://wiki.gnome.org/Projects/Libsecret */
#define SECRETSERVICE_ID "secret-service"
#define SECRETSERVICE_NAME N_("Secret Service")
#define SECRETSERVICE_DESCRIPTION N_("D-Bus Secret Service. Common in GNOME " \
                                     "and other desktop environments.")

/******************************************************************************
 * Globals
 *****************************************************************************/
static PurpleCredentialProvider *instance = NULL;

static const SecretSchema purple_secret_service_schema = {
	"im.pidgin.Purple3", SECRET_SCHEMA_NONE,
	{
		{"user", SECRET_SCHEMA_ATTRIBUTE_STRING},
		{"protocol", SECRET_SCHEMA_ATTRIBUTE_STRING},
		{"NULL", 0}
	},
	/* Reserved fields */
	0, 0, 0, 0, 0, 0, 0, 0
};

#define PURPLE_TYPE_SECRET_SERVICE (purple_secret_service_get_type())
G_DECLARE_FINAL_TYPE(PurpleSecretService, purple_secret_service,
                     PURPLE, SECRET_SERVICE, PurpleCredentialProvider)

struct _PurpleSecretService {
	PurpleCredentialProvider parent;
};

G_DEFINE_DYNAMIC_TYPE(PurpleSecretService, purple_secret_service,
                      PURPLE_TYPE_CREDENTIAL_PROVIDER)

/******************************************************************************
 * Callbacks
 *****************************************************************************/
static void
purple_secret_service_read_password_callback(GObject *obj,
                                             GAsyncResult *result,
                                             gpointer data)
{
	GTask *task = G_TASK(data);
	GError *error = NULL;
	gchar *password = NULL;

	password = secret_password_lookup_finish(result, &error);

	if(error != NULL) {
		g_task_return_error(task, error);
	} else {
		g_task_return_pointer(task, password, g_free);
	}

	g_object_unref(G_OBJECT(task));
}

static void
purple_secret_service_write_password_callback(GObject *obj,
                                              GAsyncResult *result,
                                              gpointer data)
{
	GTask *task = G_TASK(data);
	GError *error = NULL;
	gboolean ret = FALSE;

	ret = secret_password_store_finish(result, &error);

	if(error != NULL) {
		g_task_return_error(task, error);
	} else {
		g_task_return_boolean(task, ret);
	}

	g_object_unref(G_OBJECT(task));
}

static void
purple_secret_service_clear_password_callback(GObject *obj,
                                              GAsyncResult *result,
                                              gpointer data)
{
	GTask *task = G_TASK(data);
	GError *error = NULL;

	/* This returns whether a password was removed or not. Which means that it
	 * can return FALSE with error unset. This would complicate all of the other
	 * credential API and we don't need to make this distinction, so we just
	 * return TRUE unless error is set.
	 */
	secret_password_clear_finish(result, &error);

	if(error != NULL) {
		g_task_return_error(task, error);
	} else {
		g_task_return_boolean(task, TRUE);
	}

	g_object_unref(G_OBJECT(task));
}

/******************************************************************************
 * PurpleCredentialProvider Implementation
 *****************************************************************************/
static void
purple_secret_service_read_password_async(PurpleCredentialProvider *provider,
                                          PurpleAccount *account,
                                          GCancellable *cancellable,
                                          GAsyncReadyCallback callback,
                                          gpointer data)
{
	GTask *task = g_task_new(G_OBJECT(provider), cancellable, callback, data);

	secret_password_lookup(&purple_secret_service_schema, cancellable,
                           purple_secret_service_read_password_callback, task,
                           "user", purple_account_get_username(account),
                           "protocol", purple_account_get_protocol_id(account),
                           NULL);
}

static gchar *
purple_secret_service_read_password_finish(PurpleCredentialProvider *provider,
                                           GAsyncResult *result,
                                           GError **error)
{
	g_return_val_if_fail(PURPLE_IS_CREDENTIAL_PROVIDER(provider), FALSE);
	g_return_val_if_fail(G_IS_ASYNC_RESULT(result), FALSE);

	return g_task_propagate_pointer(G_TASK(result), error);
}

static void
purple_secret_service_write_password_async(PurpleCredentialProvider *provider,
                                           PurpleAccount *account,
                                           const gchar *password,
                                           GCancellable *cancellable,
                                           GAsyncReadyCallback callback,
                                           gpointer data)
{
	GTask *task = NULL;
	gchar *label = NULL;
	const gchar *username = NULL;

	task = g_task_new(G_OBJECT(provider), cancellable, callback, data);
	username = purple_account_get_username(account);

	label = g_strdup_printf(_("libpurple password for account %s"), username);
	secret_password_store(&purple_secret_service_schema,
	                      SECRET_COLLECTION_DEFAULT, label, password,
	                      cancellable,
	                      purple_secret_service_write_password_callback, task,
	                      "user", username,
	                      "protocol", purple_account_get_protocol_id(account),
	                      NULL);
	g_free(label);
}

static gboolean
purple_secret_service_write_password_finish(PurpleCredentialProvider *provider,
                                            GAsyncResult *result,
                                            GError **error)
{
	g_return_val_if_fail(PURPLE_IS_CREDENTIAL_PROVIDER(provider), FALSE);
	g_return_val_if_fail(G_IS_ASYNC_RESULT(result), FALSE);

	return g_task_propagate_boolean(G_TASK(result), error);
}

static void
purple_secret_service_clear_password_async(PurpleCredentialProvider *provider,
                                           PurpleAccount *account,
                                           GCancellable *cancellable,
                                           GAsyncReadyCallback callback,
                                           gpointer data)
{
	GTask *task = g_task_new(G_OBJECT(provider), cancellable, callback, data);

	secret_password_clear(&purple_secret_service_schema, cancellable,
	                      purple_secret_service_clear_password_callback, task,
	                      "user", purple_account_get_username(account),
	                      "protocol", purple_account_get_protocol_id(account),
	                      NULL);
}

static gboolean
purple_secret_service_clear_password_finish(PurpleCredentialProvider *provider,
                                            GAsyncResult *result,
                                            GError **error)
{
	g_return_val_if_fail(PURPLE_IS_CREDENTIAL_PROVIDER(provider), FALSE);
	g_return_val_if_fail(G_IS_ASYNC_RESULT(result), FALSE);

	return g_task_propagate_boolean(G_TASK(result), error);
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
static void
purple_secret_service_init(PurpleSecretService *ss) {
}

static void
purple_secret_service_class_init(PurpleSecretServiceClass *klass) {
	PurpleCredentialProviderClass *provider_class = NULL;

	provider_class = PURPLE_CREDENTIAL_PROVIDER_CLASS(klass);
	provider_class->read_password_async =
		purple_secret_service_read_password_async;
	provider_class->read_password_finish =
		purple_secret_service_read_password_finish;
	provider_class->write_password_async =
		purple_secret_service_write_password_async;
	provider_class->write_password_finish =
		purple_secret_service_write_password_finish;
	provider_class->clear_password_async =
		purple_secret_service_clear_password_async;
	provider_class->clear_password_finish =
		purple_secret_service_clear_password_finish;
}

static void
purple_secret_service_class_finalize(PurpleSecretServiceClass *klass) {
}

/******************************************************************************
 * API
 *****************************************************************************/
static PurpleCredentialProvider *
purple_secret_service_new(void) {
	return PURPLE_CREDENTIAL_PROVIDER(g_object_new(
		PURPLE_TYPE_SECRET_SERVICE,
		"id", SECRETSERVICE_ID,
		"name", _(SECRETSERVICE_NAME),
		"description", _(SECRETSERVICE_DESCRIPTION),
		NULL
	));
}

/******************************************************************************
 * Plugin Exports
 *****************************************************************************/
static GPluginPluginInfo *
secret_service_query(G_GNUC_UNUSED GError **error) {
	const gchar * const authors[] = {
		"Pidgin Developers <devel@pidgin.im>",
		NULL
	};

	return GPLUGIN_PLUGIN_INFO(purple_plugin_info_new(
		"id",           SECRETSERVICE_ID,
		"name",         SECRETSERVICE_NAME,
		"version",      DISPLAY_VERSION,
		"category",     N_("Keyring"),
		"summary",      "Secret Service Plugin",
		"description",  N_("This plugin will store passwords in Secret Service."),
		"authors",      authors,
		"website",      PURPLE_WEBSITE,
		"abi-version",  PURPLE_ABI_VERSION,
		"flags",        PURPLE_PLUGIN_INFO_FLAGS_INTERNAL |
		                PURPLE_PLUGIN_INFO_FLAGS_AUTO_LOAD,
		NULL
	));
}

static gboolean
secret_service_load(GPluginPlugin *plugin, GError **error) {
	PurpleCredentialManager *manager = NULL;

	purple_secret_service_register_type(G_TYPE_MODULE(plugin));

	manager = purple_credential_manager_get_default();

	instance = purple_secret_service_new();

	return purple_credential_manager_register(manager, instance, error);
}

static gboolean
secret_service_unload(G_GNUC_UNUSED GPluginPlugin *plugin, GError **error) {
	PurpleCredentialManager *manager = NULL;
	gboolean ret = FALSE;

	manager = purple_credential_manager_get_default();
	ret = purple_credential_manager_unregister(manager, instance, error);
	if(!ret) {
		return ret;
	}

	g_clear_object(&instance);

	return TRUE;
}

GPLUGIN_NATIVE_PLUGIN_DECLARE(secret_service)
