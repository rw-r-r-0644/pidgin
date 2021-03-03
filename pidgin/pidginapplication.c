/*
 * Pidgin - Internet Messenger
 * Copyright (C) Pidgin Developers <devel@pidgin.im>
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
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include <gplugin.h>
#include <purple.h>

#include "pidginapplication.h"

#include "gtkaccount.h"
#include "gtkblist.h"
#include "gtkdialogs.h"
#include "gtkprefs.h"
#include "gtkprivacy.h"
#include "gtkroomlist.h"
#include "gtksmiley-manager.h"
#include "gtkxfer.h"
#include "pidginabout.h"
#include "pidgincore.h"
#include "pidgindebug.h"
#include "pidginlog.h"
#include "pidginmooddialog.h"
#include "pidgin/pidginpluginsdialog.h"

struct _PidginApplication {
	GtkApplication parent;
};

/******************************************************************************
 * Globals
 *****************************************************************************/
static gchar *opt_config_dir_arg = NULL;
static gboolean opt_nologin = FALSE;

static GOptionEntry option_entries[] = {
	{
		"config", 'c', 0, G_OPTION_ARG_FILENAME, &opt_config_dir_arg,
		N_("use DIR for config files"), N_("DIR")
	}, {
		"nologin", 'n', 0, G_OPTION_ARG_NONE, &opt_nologin,
		N_("don't automatically login"), NULL
	},
	{
		"version", 'v', 0, G_OPTION_ARG_NONE, NULL,
		N_("display the current version and exit"), NULL
	}, {
		NULL
	}
};

G_DEFINE_TYPE(PidginApplication, pidgin_application, GTK_TYPE_APPLICATION)

/******************************************************************************
 * Actions
 *****************************************************************************/
/**< private >
 * pidgin_application_online_actions:
 *
 * This list keeps track of which actions should only be enabled while online.
 */
static const gchar *pidgin_application_online_actions[] = {
	"add-buddy",
	"add-group",
	"get-user-info",
	"new-message",
	"privacy",
	"set-mood",
};

/**< private >
 * pidgin_application_chat_actions:
 *
 * This list keeps track of which actions should only be enabled if a protocol
 * supporting groups chats is connected.
 */
static const gchar *pidgin_application_chat_actions[] = {
	"add-chat",
	"join-chat",
};

/**< private >
 * pidgin_application_room_list_actions:
 *
 * This list keeps track of which actions should only be enabled if an online
 * account supports room lists.
 */
static const gchar *pidgin_application_room_list_actions[] = {
	"room-list",
};

/*< private >
 * pidgin_action_group_actions_set_enable:
 * @group: The #PidginActionGroup instance.
 * @actions: The action names.
 * @n_actions: The number of @actions.
 * @enabled: Whether or not to enable the actions.
 *
 * Sets the enabled property of the named actions to @enabled.
 */
static void
pidgin_application_actions_set_enabled(PidginApplication *application,
                                       const gchar *const *actions,
                                       gint n_actions,
                                       gboolean enabled)
{
	gint i = 0;

	for(i = 0; i < n_actions; i++) {
		GAction *action = NULL;
		const gchar *name = actions[i];

		action = g_action_map_lookup_action(G_ACTION_MAP(application), name);

		if(action != NULL) {
			g_simple_action_set_enabled(G_SIMPLE_ACTION(action), enabled);
		} else {
			g_warning("Failed to find action named %s", name);
		}
	}
}

static void
pidgin_application_about(GSimpleAction *simple, GVariant *parameter,
                         gpointer data)
{
	GtkWidget *about = pidgin_about_dialog_new();

	/* fix me? */
#if 0
	gtk_window_set_transient_for(GTK_WINDOW(about), GTK_WINDOW(window));
#endif

	gtk_widget_show_all(about);
}

static void
pidgin_application_accounts(GSimpleAction *simple, GVariant *parameter,
                            gpointer data)
{
	pidgin_accounts_window_show();
}

static void
pidgin_application_add_buddy(GSimpleAction *simple, GVariant *parameter,
                             gpointer data)
{
	purple_blist_request_add_buddy(NULL, NULL, NULL, NULL);
}

static void
pidgin_application_add_chat(GSimpleAction *simple, GVariant *parameter,
                            gpointer data)
{
	purple_blist_request_add_chat(NULL, NULL, NULL, NULL);
}

static void
pidgin_application_add_group(GSimpleAction *simple, GVariant *parameter,
                             gpointer data)
{
	purple_blist_request_add_group();
}

static void
pidgin_application_custom_smiley(GSimpleAction *simple, GVariant *parameter,
                                 gpointer data)
{
	pidgin_smiley_manager_show();
}

static void
pidgin_application_debug(GSimpleAction *simple, GVariant *parameter,
                         gpointer data)
{
	gboolean old = purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/debug/enabled");
	purple_prefs_set_bool(PIDGIN_PREFS_ROOT "/debug/enabled", !old);
}

static void
pidgin_application_file_transfers(GSimpleAction *simple, GVariant *parameter,
                                  gpointer data)
{
	pidgin_xfer_dialog_show(NULL);
}

static void
pidgin_application_get_user_info(GSimpleAction *simple, GVariant *parameter,
                                 gpointer data)
{
	pidgin_dialogs_info();
}

static void
pidgin_application_join_chat(GSimpleAction *simple, GVariant *parameter,
                             gpointer data)
{
	pidgin_blist_joinchat_show();
}

static void
pidgin_application_new_message(GSimpleAction *simple, GVariant *parameter,
                               gpointer data)
{
	pidgin_dialogs_im();
}

static void
pidgin_application_online_help(GSimpleAction *simple, GVariant *parameter,
                               gpointer data)
{
	purple_notify_uri(NULL, PURPLE_WEBSITE "help");
}

static void
pidgin_application_plugins(GSimpleAction *simple, GVariant *parameter,
                           gpointer data)
{
	GtkWidget *dialog = pidgin_plugins_dialog_new();

	/* fixme? */
#if 0
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(window));
#endif

	gtk_widget_show_all(dialog);
}

static void
pidgin_application_preferences(GSimpleAction *simple, GVariant *parameter,
                               gpointer data)
{
	pidgin_prefs_show();
}

static void
pidgin_application_privacy(GSimpleAction *simple, GVariant *parameter,
                           gpointer data)
{
	pidgin_privacy_dialog_show();
}

static void
pidgin_application_quit(GSimpleAction *simple, GVariant *parameter,
                        gpointer data)
{
	purple_core_quit();
}

static void
pidgin_application_room_list(GSimpleAction *simple, GVariant *parameter,
                             gpointer data)
{
	pidgin_roomlist_dialog_show();
}

static void
pidgin_application_set_mood(GSimpleAction *simple, GVariant *parameter,
                            gpointer data)
{
	pidgin_mood_dialog_show(NULL);
}

static void
pidgin_application_system_log(GSimpleAction *simple, GVariant *parameter,
                              gpointer data)
{
	pidgin_syslog_show();
}

static void
pidgin_application_view_user_log(GSimpleAction *simple, GVariant *parameter,
                                 gpointer data)
{
	pidgin_dialogs_log();
}

static GActionEntry app_entries[] = {
	{
		.name = "about",
		.activate = pidgin_application_about,
	}, {
		.name = "add-buddy",
		.activate = pidgin_application_add_buddy,
	}, {
		.name = "add-chat",
		.activate = pidgin_application_add_chat,
	}, {
		.name = "add-group",
		.activate = pidgin_application_add_group,
	}, {
		.name = "custom-smiley",
		.activate = pidgin_application_custom_smiley,
	}, {
		.name = "debug",
		.activate = pidgin_application_debug,
	}, {
		.name = "file-transfers",
		.activate = pidgin_application_file_transfers,
	}, {
		.name = "get-user-info",
		.activate = pidgin_application_get_user_info,
	}, {
		.name = "join-chat",
		.activate = pidgin_application_join_chat,
	}, {
		.name = "manage-accounts",
		.activate = pidgin_application_accounts,
	}, {
		.name = "manage-plugins",
		.activate = pidgin_application_plugins,
	}, {
		.name = "new-message",
		.activate = pidgin_application_new_message,
	}, {
		.name = "online-help",
		.activate = pidgin_application_online_help,
	}, {
		.name = "preferences",
		.activate = pidgin_application_preferences,
	}, {
		.name = "privacy",
		.activate = pidgin_application_privacy,
	}, {
		.name = "quit",
		.activate = pidgin_application_quit,
	}, {
		.name = "room-list",
		.activate = pidgin_application_room_list,
	}, {
		.name = "set-mood",
		.activate = pidgin_application_set_mood,
	}, {
		.name = "system-log",
		.activate = pidgin_application_system_log,
	}, {
		.name = "view-user-log",
		.activate = pidgin_application_view_user_log,
	}
};

/******************************************************************************
 * Purple Signal Callbacks
 *****************************************************************************/
static void
pidgin_application_online_cb(gpointer data) {
	gint n_actions = G_N_ELEMENTS(pidgin_application_online_actions);

	pidgin_application_actions_set_enabled(PIDGIN_APPLICATION(data),
	                                       pidgin_application_online_actions,
	                                       n_actions,
	                                       TRUE);
}

static void
pidgin_application_offline_cb(gpointer data) {
	gint n_actions = G_N_ELEMENTS(pidgin_application_online_actions);

	pidgin_application_actions_set_enabled(PIDGIN_APPLICATION(data),
	                                       pidgin_application_online_actions,
	                                       n_actions,
	                                       FALSE);
}

static void
pidgin_application_signed_on_cb(PurpleAccount *account, gpointer data) {
	PidginApplication *application = PIDGIN_APPLICATION(data);
	PurpleProtocol *protocol = NULL;
	gboolean should_enable_chat = FALSE, should_enable_room_list = FALSE;
	gint n_actions = 0;

	protocol = purple_account_get_protocol(account);

	/* We assume that the current state is correct, so we don't bother changing
	 * state unless the newly connected account implements the chat interface,
	 * which would cause a state change.
	 */
	should_enable_chat = PURPLE_PROTOCOL_IMPLEMENTS(protocol, CHAT, info);
	if(should_enable_chat) {
		n_actions = G_N_ELEMENTS(pidgin_application_chat_actions);
		pidgin_application_actions_set_enabled(application,
		                                       pidgin_application_chat_actions,
		                                       n_actions,
		                                       TRUE);
	}

	/* likewise, for the room list, we only care about enabling in this
	 * handler.
	 */
	should_enable_room_list = PURPLE_PROTOCOL_IMPLEMENTS(protocol, ROOMLIST,
	                                                     get_list);
	if(should_enable_room_list) {
		n_actions = G_N_ELEMENTS(pidgin_application_room_list_actions);
		pidgin_application_actions_set_enabled(application,
		                                       pidgin_application_room_list_actions,
		                                       n_actions,
		                                       TRUE);
	}
}

static void
pidgin_application_signed_off_cb(PurpleAccount *account, gpointer data) {
	PidginApplication *application = PIDGIN_APPLICATION(data);
	gboolean should_disable_chat = TRUE, should_disable_room_list = TRUE;
	GList *connections = NULL, *l = NULL;
	gint n_actions = 0;

	/* walk through all the connections, looking for online ones that implement
	 * the chat interface.  We don't bother checking the account that this
	 * signal was emitted for, because it's already offline and will be
	 * filtered out by the online check.
	 */
	connections = purple_connections_get_all();
	for(l = connections; l != NULL; l = l->next) {
		PurpleConnection *connection = PURPLE_CONNECTION(l->data);
		PurpleProtocol *protocol = NULL;

		/* if the connection isn't online, we don't care about it */
		if(!PURPLE_CONNECTION_IS_CONNECTED(connection)) {
			continue;
		}

		protocol = purple_connection_get_protocol(connection);

		/* check if the protocol implements the chat interface */
		if(PURPLE_PROTOCOL_IMPLEMENTS(protocol, CHAT, info)) {
			should_disable_chat = FALSE;
		}

		/* check if the protocol implements the room list interface */
		if(PURPLE_PROTOCOL_IMPLEMENTS(protocol, ROOMLIST, get_list)) {
			should_disable_room_list = FALSE;
		}

		/* if we can't disable both, we can bail out of the loop */
		if(!should_disable_chat && !should_disable_room_list) {
			break;
		}
	}

	if(should_disable_chat) {
		n_actions = G_N_ELEMENTS(pidgin_application_chat_actions);
		pidgin_application_actions_set_enabled(application,
		                                       pidgin_application_chat_actions,
		                                       n_actions,
		                                       FALSE);
	}

	if(should_disable_room_list) {
		n_actions = G_N_ELEMENTS(pidgin_application_room_list_actions);
		pidgin_application_actions_set_enabled(application,
		                                       pidgin_application_room_list_actions,
		                                       n_actions,
		                                       FALSE);
	}
}

/******************************************************************************
 * GApplication Implementation
 *****************************************************************************/
static void
pidgin_application_startup(GApplication *application) {
	GtkCssProvider *provider = NULL;
	GError *error = NULL;
	GList *active_accounts = NULL;
	gchar *search_path = NULL;
	gpointer handle = NULL;

	G_APPLICATION_CLASS(pidgin_application_parent_class)->startup(application);

	/* set a user-specified config directory */
	if (opt_config_dir_arg != NULL) {
		if (g_path_is_absolute(opt_config_dir_arg)) {
			purple_util_set_user_dir(opt_config_dir_arg);
		} else {
			/* Make an absolute (if not canonical) path */
			gchar *cwd = g_get_current_dir();
			gchar *path = g_build_filename(cwd, opt_config_dir_arg, NULL);

			purple_util_set_user_dir(path);

			g_free(cwd);
			g_free(path);
		}
	}

	provider = gtk_css_provider_new();

	search_path = g_build_filename(purple_config_dir(), "gtk-3.0.css", NULL);
	gtk_css_provider_load_from_path(provider, search_path, &error);
	if(error != NULL) {
		purple_debug_error("gtk", "Unable to load custom gtk-3.0.css: %s\n",
		                   error->message);
		g_clear_error(&error);
	} else {
		GdkScreen *screen = gdk_screen_get_default();
		gtk_style_context_add_provider_for_screen(screen,
		                                          GTK_STYLE_PROVIDER(provider),
		                                          GTK_STYLE_PROVIDER_PRIORITY_USER);
	}

	g_free(search_path);

#ifdef _WIN32
	winpidgin_init();
#endif

	purple_core_set_ui_ops(pidgin_core_get_ui_ops());

	if(!purple_core_init(PIDGIN_UI)) {
		fprintf(stderr,
				_("Initialization of the libpurple core failed. Aborting!\n"
				  "Please report this!\n"));
		g_abort();
	}

	if(g_getenv("PURPLE_PLUGINS_SKIP")) {
		purple_debug_info("gtk",
				"PURPLE_PLUGINS_SKIP environment variable "
				"set, skipping normal Pidgin plugin paths");
	} else {
		search_path = g_build_filename(purple_data_dir(), "plugins", NULL);

		if(!g_file_test(search_path, G_FILE_TEST_IS_DIR)) {
			g_mkdir(search_path, S_IRUSR | S_IWUSR | S_IXUSR);
		}

		purple_plugins_add_search_path(search_path);
		g_free(search_path);

		purple_plugins_add_search_path(PIDGIN_LIBDIR);
	}

	purple_plugins_refresh();

	/* load plugins we had when we quit */
	purple_plugins_load_saved(PIDGIN_PREFS_ROOT "/plugins/loaded");

	/* gk 20201008: this needs to be moved to the buddy list initialization. */
	pidgin_blist_setup_sort_methods();

	gtk_window_set_default_icon_name("pidgin");

	g_free(opt_config_dir_arg);
	opt_config_dir_arg = NULL;

	/*
	 * We want to show the blist early in the init process so the
	 * user feels warm and fuzzy.
	 */
	purple_blist_show();

	if(purple_prefs_get_bool(PIDGIN_PREFS_ROOT "/debug/enabled")) {
		pidgin_debug_window_show();
	}

	if(opt_nologin) {
		/* Set all accounts to "offline" */
		PurpleSavedStatus *saved_status;

		/* If we've used this type+message before, lookup the transient status */
		saved_status = purple_savedstatus_find_transient_by_type_and_message(
							PURPLE_STATUS_OFFLINE, NULL);

		/* If this type+message is unique then create a new transient saved status */
		if(saved_status == NULL) {
			saved_status = purple_savedstatus_new(NULL, PURPLE_STATUS_OFFLINE);
		}

		/* Set the status for each account */
		purple_savedstatus_activate(saved_status);
	} else {
		/* Everything is good to go--sign on already */
		if (!purple_prefs_get_bool("/purple/savedstatus/startup_current_status")) {
			purple_savedstatus_activate(purple_savedstatus_get_startup());
		}

		purple_accounts_restore_current_statuses();
	}

	if((active_accounts = purple_accounts_get_all_active()) == NULL) {
		pidgin_accounts_window_show();
	} else {
		g_list_free(active_accounts);
	}

	/* GTK clears the notification for us when opening the first window, but we
	 * may have launched with only a status icon, so clear it just in case.
	 */
	gdk_notify_startup_complete();

	/* TODO: Use GtkApplicationWindow or add a window instead */
	g_application_hold(application);

	/* connect to the online and offline signals in purple connections.  This
	 * is used to toggle states of actions that require being online.
	 */
	handle = purple_connections_get_handle();
	purple_signal_connect(handle, "online", application,
	                      PURPLE_CALLBACK(pidgin_application_online_cb),
	                      application);
	purple_signal_connect(handle, "offline", application,
	                      PURPLE_CALLBACK(pidgin_application_offline_cb),
	                      application);

	/* connect to account-signed-on and account-signed-off to toggle actions
	 * that depend on specific interfaces in accounts.
	 */
	handle = purple_accounts_get_handle();
	purple_signal_connect(handle, "account-signed-on", application,
	                      PURPLE_CALLBACK(pidgin_application_signed_on_cb),
	                      application);
	purple_signal_connect(handle, "account-signed-off", application,
	                      PURPLE_CALLBACK(pidgin_application_signed_off_cb),
	                      application);

}

static void
pidgin_application_activate(GApplication *application) {
	PidginBuddyList *blist = pidgin_blist_get_default_gtk_blist();

	if(blist != NULL && blist->window != NULL) {
		gtk_window_present(GTK_WINDOW(blist->window));
	}
}

static gint
pidgin_application_command_line(GApplication *application,
                                GApplicationCommandLine *cmdline)
{
	gchar **argv = NULL;
	gint argc = 0, i = 0;

	argv = g_application_command_line_get_arguments(cmdline, &argc);

	if(argc == 1) {
		/* No arguments, just activate */
		g_application_activate(application);
	}

	/* Start at 1 to skip the executable name */
	for (i = 1; i < argc; i++) {
		purple_got_protocol_handler_uri(argv[i]);
	}

	g_strfreev(argv);

	return 0;
}

static gint
pidgin_application_handle_local_options(GApplication *application,
                                        GVariantDict *options)
{
	if (g_variant_dict_contains(options, "version")) {
		printf("%s %s (libpurple %s)\n", PIDGIN_NAME, DISPLAY_VERSION,
		       purple_core_get_version());

		return 0;
	}

	return -1;
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
static void
pidgin_application_init(PidginApplication *application) {
	GApplication *gapp = G_APPLICATION(application);
	gboolean online = FALSE;
	gint n_actions = 0;

	g_application_add_main_option_entries(gapp, option_entries);
	g_application_add_option_group(gapp, purple_get_option_group());
	g_application_add_option_group(gapp, gplugin_get_option_group());

	g_action_map_add_action_entries(G_ACTION_MAP(application), app_entries,
	                                G_N_ELEMENTS(app_entries), application);

	/* Set the default state for our actions to match our online state. */
	online = purple_connections_is_online();

	n_actions = G_N_ELEMENTS(pidgin_application_online_actions);
	pidgin_application_actions_set_enabled(application,
	                                       pidgin_application_online_actions,
	                                       n_actions,
	                                       online);

	n_actions = G_N_ELEMENTS(pidgin_application_chat_actions);
	pidgin_application_actions_set_enabled(application,
	                                       pidgin_application_chat_actions,
	                                       n_actions,
	                                       online);

	n_actions = G_N_ELEMENTS(pidgin_application_room_list_actions);
	pidgin_application_actions_set_enabled(application,
	                                       pidgin_application_room_list_actions,
	                                       n_actions,
	                                       online);
}

static void
pidgin_application_class_init(PidginApplicationClass *klass) {
	GApplicationClass *app_class = G_APPLICATION_CLASS(klass);

	app_class->startup = pidgin_application_startup;
	app_class->activate = pidgin_application_activate;
	app_class->command_line = pidgin_application_command_line;
	app_class->handle_local_options = pidgin_application_handle_local_options;
}

/******************************************************************************
 * Public API
 *****************************************************************************/
GApplication *
pidgin_application_new(void) {
	return g_object_new(
		PIDGIN_TYPE_APPLICATION,
		"application-id", "im.pidgin.Pidgin3",
		"flags", G_APPLICATION_CAN_OVERRIDE_APP_ID |
		         G_APPLICATION_HANDLES_COMMAND_LINE,
		"register-session", TRUE,
		NULL);
}
