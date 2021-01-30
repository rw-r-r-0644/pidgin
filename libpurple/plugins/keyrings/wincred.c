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
 * along with this program ; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#include <glib/gi18n-lib.h>

#include <purple.h>

#include <wincred.h>

#define WINCRED_NAME        N_("Windows credentials")
#define WINCRED_ID          "keyring-wincred"

#define WINCRED_MAX_TARGET_NAME 256

/******************************************************************************
 * Globals
 *****************************************************************************/
static PurpleCredentialProvider *instance = NULL;

#define PURPLE_TYPE_WINCRED (purple_wincred_get_type())
G_DECLARE_FINAL_TYPE(PurpleWinCred, purple_wincred, PURPLE, SECRET_SERVICE,
                     PurpleCredentialProvider)

struct _PurpleWinCred {
	PurpleCredentialProvider parent;
};

G_DEFINE_DYNAMIC_TYPE(PurpleWinCred, purple_wincred,
                      PURPLE_TYPE_CREDENTIAL_PROVIDER)

/******************************************************************************
 * PurpleCredentialProvider Implementation
 *****************************************************************************/

static gunichar2 *
wincred_get_target_name(PurpleAccount *account, GError **error)
{
	gchar target_name_utf8[WINCRED_MAX_TARGET_NAME];
	gunichar2 *target_name_utf16;

	g_return_val_if_fail(account != NULL, NULL);

	g_snprintf(target_name_utf8, WINCRED_MAX_TARGET_NAME, "libpurple_%s_%s",
		purple_account_get_protocol_id(account),
		purple_account_get_username(account));

	target_name_utf16 =
	        g_utf8_to_utf16(target_name_utf8, -1, NULL, NULL, error);

	if (target_name_utf16 == NULL) {
		purple_debug_fatal("keyring-wincred", "Couldn't convert target name");
		return NULL;
	}

	return target_name_utf16;
}

static void
purple_wincred_read_password_async(PurpleCredentialProvider *provider,
                                   PurpleAccount *account,
                                   GCancellable *cancellable,
                                   GAsyncReadyCallback callback, gpointer data)
{
	GTask *task = NULL;
	GError *error = NULL;
	gunichar2 *target_name = NULL;
	gchar *password = NULL;
	PCREDENTIALW credential = NULL;

	task = g_task_new(G_OBJECT(provider), cancellable, callback, data);
	target_name = wincred_get_target_name(account, &error);
	if (target_name == NULL) {
		g_task_return_error(task, error);
		return;
	}

	if (!CredReadW(target_name, CRED_TYPE_GENERIC, 0, &credential)) {
		DWORD error_code = GetLastError();

		if (error_code == ERROR_NOT_FOUND) {
			if (purple_debug_is_verbose()) {
				purple_debug_misc("keyring-wincred",
					"No password found for account %s\n",
					purple_account_get_username(account));
			}
			error = g_error_new(PURPLE_KEYRING_ERROR,
				PURPLE_KEYRING_ERROR_NOPASSWORD,
				_("Password not found."));
		} else if (error_code == ERROR_NO_SUCH_LOGON_SESSION) {
			purple_debug_error("keyring-wincred",
				"Cannot read password, no valid logon "
				"session\n");
			error = g_error_new(PURPLE_KEYRING_ERROR,
				PURPLE_KEYRING_ERROR_ACCESSDENIED,
				_("Cannot read password, no valid logon "
				"session."));
		} else {
			purple_debug_error("keyring-wincred",
				"Cannot read password, error %lx\n",
				error_code);
			error = g_error_new(PURPLE_KEYRING_ERROR,
				PURPLE_KEYRING_ERROR_BACKENDFAIL,
				_("Cannot read password (error %lx)."), error_code);
		}

		g_task_return_error(task, error);
		return;
	}

	password = g_utf16_to_utf8((gunichar2*)credential->CredentialBlob,
		credential->CredentialBlobSize / sizeof(gunichar2),
		NULL, NULL, NULL);

	memset(credential->CredentialBlob, 0, credential->CredentialBlobSize);
	CredFree(credential);

	if (password == NULL) {
		purple_debug_error("keyring-wincred",
			"Cannot convert password\n");
		error = g_error_new(PURPLE_KEYRING_ERROR,
			PURPLE_KEYRING_ERROR_BACKENDFAIL,
			_("Cannot read password (unicode error)."));
		g_task_return_error(task, error);
		return;
	} else {
		purple_debug_misc("keyring-wincred",
			_("Got password for account %s.\n"),
			purple_account_get_username(account));
	}

	g_task_return_pointer(task, password, g_free);
}

static gchar *
purple_wincred_read_password_finish(PurpleCredentialProvider *provider,
                                    GAsyncResult *result, GError **error)
{
	g_return_val_if_fail(PURPLE_IS_CREDENTIAL_PROVIDER(provider), FALSE);
	g_return_val_if_fail(G_IS_ASYNC_RESULT(result), FALSE);

	return g_task_propagate_pointer(G_TASK(result), error);
}

static void
purple_wincred_write_password_async(PurpleCredentialProvider *provider,
                                    PurpleAccount *account,
                                    const gchar *password,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback, gpointer data)
{
	GTask *task = NULL;
	GError *error = NULL;
	gunichar2 *target_name = NULL;
	gunichar2 *username_utf16 = NULL;
	gunichar2 *password_utf16 = NULL;
	glong password_len = 0;
	CREDENTIALW credential;

	task = g_task_new(G_OBJECT(provider), cancellable, callback, data);

	target_name = wincred_get_target_name(account, &error);
	if (target_name == NULL) {
		g_task_return_error(task, error);
		return;
	}

	username_utf16 = g_utf8_to_utf16(purple_account_get_username(account), -1,
	                                 NULL, NULL, &error);
	if (username_utf16 == NULL) {
		g_free(target_name);
		purple_debug_fatal("keyring-wincred", "Couldn't convert username");
		g_task_return_error(task, error);
		return;
	}

	password_utf16 = g_utf8_to_utf16(password, -1, NULL, &password_len, &error);
	if (password_utf16 == NULL) {
		g_free(username_utf16);
		g_free(target_name);
		purple_debug_fatal("keyring-wincred", "Couldn't convert password");
		g_task_return_error(task, error);
		return;
	}

	memset(&credential, 0, sizeof(CREDENTIALW));
	credential.Type = CRED_TYPE_GENERIC;
	credential.TargetName = target_name;
	credential.CredentialBlobSize = purple_utf16_size(password_utf16) - 2;
	credential.CredentialBlob = (LPBYTE)password_utf16;
	credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
	credential.UserName = username_utf16;

	if (!CredWriteW(&credential, 0)) {
		DWORD error_code = GetLastError();

		if (error_code == ERROR_NO_SUCH_LOGON_SESSION) {
			purple_debug_error("keyring-wincred",
			                   "Cannot store password, no valid logon session");
			error = g_error_new(
			        PURPLE_KEYRING_ERROR, PURPLE_KEYRING_ERROR_ACCESSDENIED,
			        _("Cannot remove password, no valid logon session."));
		} else {
			purple_debug_error("keyring-wincred",
				"Cannot store password, error %lx\n",
				error_code);
			error = g_error_new(PURPLE_KEYRING_ERROR,
				PURPLE_KEYRING_ERROR_BACKENDFAIL,
				_("Cannot store password (error %lx)."), error_code);
		}
	} else {
		purple_debug_misc("keyring-wincred", "Password updated for account %s.",
		                  purple_account_get_username(account));
	}

	g_free(target_name);
	g_free(username_utf16);
	memset(password_utf16, 0, password_len * sizeof(gunichar));
	g_free(password_utf16);

	if (error != NULL) {
		g_task_return_error(task, error);
	} else {
		g_task_return_boolean(task, TRUE);
	}
}

static gboolean
purple_wincred_write_password_finish(PurpleCredentialProvider *provider,
                                     GAsyncResult *result, GError **error)
{
	g_return_val_if_fail(PURPLE_IS_CREDENTIAL_PROVIDER(provider), FALSE);
	g_return_val_if_fail(G_IS_ASYNC_RESULT(result), FALSE);

	return g_task_propagate_boolean(G_TASK(result), error);
}

static void
purple_wincred_clear_password_async(PurpleCredentialProvider *provider,
                                    PurpleAccount *account,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback, gpointer data)
{
	GTask *task = NULL;
	GError *error = NULL;
	gunichar2 *target_name = NULL;

	task = g_task_new(G_OBJECT(provider), cancellable, callback, data);

	target_name = wincred_get_target_name(account, &error);
	if (target_name == NULL) {
		g_task_return_error(task, error);
		return;
	}

	if (CredDeleteW(target_name, CRED_TYPE_GENERIC, 0)) {
		purple_debug_misc("keyring-wincred", "Password for account %s removed",
		                  purple_account_get_username(account));
		g_task_return_boolean(task, TRUE);

	} else {
		DWORD error_code = GetLastError();

		if (error_code == ERROR_NOT_FOUND) {
			if (purple_debug_is_verbose()) {
				purple_debug_misc(
				        "keyring-wincred",
				        "Password for account %s was already removed.",
				        purple_account_get_username(account));
			}
		} else if (error_code == ERROR_NO_SUCH_LOGON_SESSION) {
			purple_debug_error(
			        "keyring-wincred",
			        "Cannot remove password, no valid logon session");
			error = g_error_new(
			        PURPLE_KEYRING_ERROR, PURPLE_KEYRING_ERROR_ACCESSDENIED,
			        _("Cannot remove password, no valid logon session."));
		} else {
			purple_debug_error("keyring-wincred",
			                   "Cannot remove password, error %lx", error_code);
			error = g_error_new(
			        PURPLE_KEYRING_ERROR, PURPLE_KEYRING_ERROR_BACKENDFAIL,
			        _("Cannot remove password (error %lx)."), error_code);
		}

		g_task_return_error(task, error);
	}
}

static gboolean
purple_wincred_clear_password_finish(PurpleCredentialProvider *provider,
                                     GAsyncResult *result, GError **error)
{
	g_return_val_if_fail(PURPLE_IS_CREDENTIAL_PROVIDER(provider), FALSE);
	g_return_val_if_fail(G_IS_ASYNC_RESULT(result), FALSE);

	return g_task_propagate_boolean(G_TASK(result), error);
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
static void
purple_wincred_init(PurpleWinCred *wincred)
{
}

static void
purple_wincred_class_init(PurpleWinCredClass *klass)
{
	PurpleCredentialProviderClass *provider_class = NULL;

	provider_class = PURPLE_CREDENTIAL_PROVIDER_CLASS(klass);
	provider_class->read_password_async = purple_wincred_read_password_async;
	provider_class->read_password_finish = purple_wincred_read_password_finish;
	provider_class->write_password_async = purple_wincred_write_password_async;
	provider_class->write_password_finish =
	        purple_wincred_write_password_finish;
	provider_class->clear_password_async = purple_wincred_clear_password_async;
	provider_class->clear_password_finish =
	        purple_wincred_clear_password_finish;
}

static void
purple_wincred_class_finalize(PurpleWinCredClass *klass)
{
}

/******************************************************************************
 * API
 *****************************************************************************/
static PurpleCredentialProvider *
purple_wincred_new(void)
{
	return PURPLE_CREDENTIAL_PROVIDER(g_object_new(
		PURPLE_TYPE_WINCRED,
		"id", WINCRED_ID,
		"name", _(WINCRED_NAME),
		NULL
	));
}

/******************************************************************************
 * Plugin Exports
 *****************************************************************************/

G_MODULE_EXPORT GPluginPluginInfo *gplugin_query(GError **error);
G_MODULE_EXPORT gboolean gplugin_load(GPluginNativePlugin *plugin,
                                      GError **error);
G_MODULE_EXPORT gboolean gplugin_unload(GPluginNativePlugin *plugin,
                                        GError **error);

G_MODULE_EXPORT GPluginPluginInfo *
gplugin_query(GError **error)
{
	const gchar * const authors[] = {
		"Tomek Wasilczyk <twasilczyk@pidgin.im>",
		NULL
	};

	return GPLUGIN_PLUGIN_INFO(purple_plugin_info_new(
		"id",           WINCRED_ID,
		"name",         WINCRED_NAME,
		"version",      DISPLAY_VERSION,
		"category",     _("Keyring"),
		"summary",      _("Store passwords using Windows credentials"),
		"description",  _("This plugin stores passwords using Windows credentials."),
		"authors",      authors,
		"website",      PURPLE_WEBSITE,
		"abi-version",  PURPLE_ABI_VERSION,
		"flags",        PURPLE_PLUGIN_INFO_FLAGS_INTERNAL |
		                PURPLE_PLUGIN_INFO_FLAGS_AUTO_LOAD,
		NULL
	));
}

G_MODULE_EXPORT gboolean
gplugin_load(GPluginNativePlugin *plugin, GError **error)
{
	PurpleCredentialManager *manager = NULL;

	purple_wincred_register_type(G_TYPE_MODULE(plugin));

	manager = purple_credential_manager_get_default();

	instance = purple_wincred_new();

	return purple_credential_manager_register_provider(manager, instance,
	                                                   error);
}

G_MODULE_EXPORT gboolean
gplugin_unload(GPluginNativePlugin *plugin, GError **error)
{
	PurpleCredentialManager *manager = NULL;
	gboolean ret = FALSE;

	manager = purple_credential_manager_get_default();
	ret = purple_credential_manager_unregister_provider(manager, instance,
	                                                    error);
	if (!ret) {
		return ret;
	}

	g_clear_object(&instance);

	return TRUE;
}
