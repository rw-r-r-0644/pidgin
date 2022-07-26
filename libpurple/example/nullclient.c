/*
 * pidgin
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */

#include "purple.h"

#include <glib.h>
#include <glib/gprintf.h>

#include <signal.h>
#include <string.h>
#ifdef _WIN32
#  include <conio.h>
#else
#  include <unistd.h>
#endif

#include "defines.h"

static GLogWriterOutput
nullclient_g_log_handler(G_GNUC_UNUSED GLogLevelFlags log_level,
                         G_GNUC_UNUSED const GLogField *fields,
                         G_GNUC_UNUSED gsize n_fields,
                         G_GNUC_UNUSED gpointer user_data)
{
	/* We do not want any debugging for now to keep the noise to a minimum. */
	return G_LOG_WRITER_HANDLED;
}

/*** Conversation uiops ***/
static void
null_write_conv(PurpleConversation *conv, PurpleMessage *msg)
{
	gchar *timestamp = purple_message_format_timestamp(msg, "(%H:%M:%S)");

	printf("(%s) %s %s: %s\n",
		purple_conversation_get_name(conv),
		timestamp,
		purple_message_get_author_alias(msg),
		purple_message_get_contents(msg));

	g_free(timestamp);
}

static PurpleConversationUiOps null_conv_uiops =
{
	.write_conv = null_write_conv,
};

static void
null_ui_init(void)
{
	/**
	 * This should initialize the UI components for all the modules. Here we
	 * just initialize the UI for conversations.
	 */
	purple_conversations_set_ui_ops(&null_conv_uiops);
}

static PurpleCoreUiOps null_core_uiops =
{
	.ui_init = null_ui_init,
};

static void
init_libpurple(void)
{
	PurpleUiInfo *ui_info = NULL;

	/* Set a custom user directory (optional) */
	purple_util_set_user_dir(CUSTOM_USER_DIRECTORY);

	/* We do not want any debugging for now to keep the noise to a minimum. */
	g_log_set_writer_func(nullclient_g_log_handler, NULL, NULL);

	/* Set the core-uiops, which is used to
	 * 	- initialize the ui specific preferences.
	 * 	- initialize the debug ui.
	 * 	- initialize the ui components for all the modules.
	 * 	- uninitialize the ui components for all the modules when the core terminates.
	 */
	purple_core_set_ui_ops(&null_core_uiops);

	ui_info = purple_ui_info_new(UI_ID, "NullClient", VERSION, PURPLE_WEBSITE,
	                             PURPLE_WEBSITE, "example");

	/* Now that all the essential stuff has been set, let's try to init the core. It's
	 * necessary to provide a non-NULL name for the current ui to the core. This name
	 * is used by stuff that depends on this ui, for example the ui-specific plugins. */
	if (!purple_core_init(ui_info)) {
		/* Initializing the core failed. Terminate. */
		fprintf(stderr,
				"libpurple initialization failed. Dumping core.\n"
				"Please report this!\n");
		abort();
	}

	/* Set path to search for plugins. The core (libpurple) takes care of loading the
	 * core-plugins, which includes the in-tree protocols. So it is not essential to add
	 * any path here, but it might be desired, especially for ui-specific plugins. */
	purple_plugins_add_search_path(CUSTOM_PLUGIN_PATH);
	purple_plugins_refresh();

	/* Load the preferences. */
	purple_prefs_load();

	/* Load the desired plugins. The client should save the list of loaded plugins in
	 * the preferences using purple_plugins_save_loaded(PLUGIN_SAVE_PREF) */
	purple_plugins_load_saved(PLUGIN_SAVE_PREF);
}

static void
signed_on(PurpleConnection *gc, gpointer null)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	printf("Account connected: %s %s\n", purple_account_get_username(account), purple_account_get_protocol_id(account));
}

static void
connect_to_signals_for_demonstration_purposes_only(void)
{
	static int handle;
	purple_signal_connect(purple_connections_get_handle(), "signed-on", &handle,
				G_CALLBACK(signed_on), NULL);
}

#if defined(_WIN32) || defined(__BIONIC__)
#ifndef PASS_MAX
#  define PASS_MAX 1024
#endif
static gchar *
getpass(const gchar *prompt)
{
	static gchar buff[PASS_MAX + 1];
	guint i = 0;

	g_fprintf(stderr, "%s", prompt);
	fflush(stderr);

	while (i < sizeof(buff) - 1) {
#ifdef __BIONIC__
		buff[i] = getc(stdin);
#else
		buff[i] = _getch();
#endif
		if (buff[i] == '\r' || buff[i] == '\n')
			break;
		i++;
	}
	buff[i] = '\0';
	g_fprintf(stderr, "\n");

	return buff;
}
#endif /* _WIN32 || __BIONIC__ */

int main(int argc, char *argv[])
{
	PurpleCredentialManager *manager = NULL;
	GList *list, *iter;
	int i, num;
	GList *names = NULL;
	const char *protocol = NULL;
	char name[128];
	char *password;
	GMainLoop *loop = g_main_loop_new(NULL, FALSE);
	PurpleAccount *account;
	PurpleSavedStatus *status;
	PurpleProtocolManager *protocol_manager = NULL;
	char *res;

#ifndef _WIN32
	/* libpurple's built-in DNS resolution forks processes to perform
	 * blocking lookups without blocking the main process.  It does not
	 * handle SIGCHLD itself, so if the UI does not you quickly get an army
	 * of zombie subprocesses marching around.
	 */
	signal(SIGCHLD, SIG_IGN);
#endif

	init_libpurple();

	printf("libpurple initialized.\n");

	protocol_manager = purple_protocol_manager_get_default();
	list = purple_protocol_manager_get_all(protocol_manager);
	for (i = 0, iter = list; iter; iter = iter->next) {
		PurpleProtocol *protocol = iter->data;
		if (protocol && purple_protocol_get_name(protocol)) {
			printf("\t%d: %s\n", i++, purple_protocol_get_name(protocol));
			names = g_list_append(names, (gpointer)purple_protocol_get_id(protocol));
		}
	}
	g_list_free(list);

	num = -1;
	while (num < 0 || num >= i) {
		printf("Select the protocol [0-%d]: ", i - 1);
		res = fgets(name, sizeof(name), stdin);
		if (!res) {
			fprintf(stderr, "Failed to get protocol selection.");
			abort();
		}
		if (sscanf(name, "%d", &num) != 1) {
			num = -1;
		}
	}
	protocol = g_list_nth_data(names, num);
	if (!protocol) {
		fprintf(stderr, "Failed to get protocol.");
		abort();
	}

	printf("Username: ");
	res = fgets(name, sizeof(name), stdin);
	if (!res) {
		fprintf(stderr, "Failed to read user name.");
		abort();
	}
	name[strlen(name) - 1] = 0;  /* strip the \n at the end */

	/* Create the account */
	account = purple_account_new(name, protocol);

	/* Get the password for the account */
	password = getpass("Password: ");

	manager = purple_credential_manager_get_default();
	purple_credential_manager_write_password_async(manager, account, password,
	                                               NULL, NULL, NULL);

	/* It's necessary to enable the account first. */
	purple_account_set_enabled(account, TRUE);

	/* Now, to connect the account(s), create a status and activate it. */
	status = purple_savedstatus_new(NULL, PURPLE_STATUS_AVAILABLE);
	purple_savedstatus_activate(status);

	connect_to_signals_for_demonstration_purposes_only();

	g_main_loop_run(loop);

	return 0;
}
