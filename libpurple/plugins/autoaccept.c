/*
 * Autoaccept - Auto-accept file transfers from selected users
 * Copyright (C) 2006
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02111-1301, USA.
 */

#include <glib/gi18n-lib.h>

#define PLUGIN_ID			"core-plugin_pack-autoaccept"
#define PLUGIN_NAME			N_("Autoaccept")
#define PLUGIN_CATEGORY		N_("Utility")
#define PLUGIN_STATIC_NAME	Autoaccept
#define PLUGIN_SUMMARY		N_("Auto-accept file transfer requests from selected users.")
#define PLUGIN_DESCRIPTION	N_("Auto-accept file transfer requests from selected users.")
#define PLUGIN_AUTHORS		{"Sadrul H Chowdhury <sadrul@users.sourceforge.net>", NULL}

/* System headers */
#include <glib.h>
#include <glib/gstdio.h>

#include <gplugin.h>
#include <gplugin-native.h>

#include <purple.h>

#define PREF_PREFIX		"/plugins/core/" PLUGIN_ID
#define PREF_PATH		PREF_PREFIX "/path"
#define PREF_STRANGER	PREF_PREFIX "/stranger"
#define PREF_NOTIFY		PREF_PREFIX "/notify"
#define PREF_NEWDIR     PREF_PREFIX "/newdir"
#define PREF_ESCAPE     PREF_PREFIX "/escape"

#define PREF_STRANGER_OLD PREF_PREFIX "/reject_stranger"

typedef enum
{
	FT_ASK,
	FT_ACCEPT,
	FT_REJECT
} AutoAcceptSetting;

static gboolean
ensure_path_exists(const char *dir)
{
	if (!g_file_test(dir, G_FILE_TEST_IS_DIR))
	{
		if (g_mkdir_with_parents(dir, S_IRUSR | S_IWUSR | S_IXUSR)) {
			return FALSE;
		}
	}

	return TRUE;
}

static void
auto_accept_complete_cb(PurpleXfer *xfer, G_GNUC_UNUSED GParamSpec *pspec,
                        G_GNUC_UNUSED gpointer data)
{
	PurpleConversationManager *manager = NULL;

	if (purple_xfer_get_status(xfer) != PURPLE_XFER_STATUS_DONE) {
		return;
	}

	manager = purple_conversation_manager_get_default();

	if(purple_prefs_get_bool(PREF_NOTIFY) &&
	   !purple_conversation_manager_find_im(manager,
	                                        purple_xfer_get_account(xfer),
	                                        purple_xfer_get_remote_user(xfer)))
	{
		char *message = g_strdup_printf(_("Autoaccepted file transfer of \"%s\" from \"%s\" completed."),
					purple_xfer_get_filename(xfer), purple_xfer_get_remote_user(xfer));
		purple_notify_info(NULL, _("Autoaccept complete"), message,
			NULL, purple_request_cpar_from_account(
				purple_xfer_get_account(xfer)));
		g_free(message);
	}
}

static void
file_recv_request_cb(PurpleXfer *xfer, gpointer handle)
{
	PurpleAccount *account;
	PurpleBlistNode *node;
	const char *pref;
	char *filename;
	char *dirname;

    int accept_setting;

	account = purple_xfer_get_account(xfer);
	node = PURPLE_BLIST_NODE(purple_blist_find_buddy(account, purple_xfer_get_remote_user(xfer)));

	/* If person is on buddy list, use the buddy setting; otherwise, use the
	   stranger setting. */
	if (node) {
		node = purple_blist_node_get_parent(node);
		g_return_if_fail(PURPLE_IS_CONTACT(node));
		accept_setting = purple_blist_node_get_int(node, "autoaccept");
	} else {
		accept_setting = purple_prefs_get_int(PREF_STRANGER);
	}

	switch (accept_setting)
	{
		case FT_ASK:
			break;
		case FT_ACCEPT:
            pref = purple_prefs_get_string(PREF_PATH);
			if (ensure_path_exists(pref))
			{
				int count = 1;
				const char *escape;
				gchar **name_and_ext;
				const gchar *name;
				gchar *ext;

				if (purple_prefs_get_bool(PREF_NEWDIR))
					dirname = g_build_filename(pref, purple_normalize(account, purple_xfer_get_remote_user(xfer)), NULL);
				else
					dirname = g_build_filename(pref, NULL);

				if (!ensure_path_exists(dirname))
				{
					g_free(dirname);
					break;
				}

				/* Escape filename (if escaping is turned on) */
				if (purple_prefs_get_bool(PREF_ESCAPE)) {
					escape = purple_escape_filename(purple_xfer_get_filename(xfer));
				} else {
					escape = purple_xfer_get_filename(xfer);
				}
				filename = g_build_filename(dirname, escape, NULL);

				/* Split at the first dot, to avoid uniquifying "foo.tar.gz" to "foo.tar-2.gz" */
				name_and_ext = g_strsplit(escape, ".", 2);
				name = name_and_ext[0];
				if (name == NULL) {
					g_strfreev(name_and_ext);
					g_return_if_reached();
				}
				if (name_and_ext[1] != NULL) {
					/* g_strsplit does not include the separator in each chunk. */
					ext = g_strdup_printf(".%s", name_and_ext[1]);
				} else {
					ext = g_strdup("");
				}

				/* Make sure the file doesn't exist. Do we want some better checking than this? */
				/* FIXME: There is a race here: if the newly uniquified file name gets created between
				 *        this g_file_test and the transfer starting, the file created in the meantime
				 *        will be clobbered. But it's not at all straightforward to fix.
				 */
				while (g_file_test(filename, G_FILE_TEST_EXISTS)) {
					char *file = g_strdup_printf("%s-%d%s", name, count++, ext);
					g_free(filename);
					filename = g_build_filename(dirname, file, NULL);
					g_free(file);
				}

				purple_xfer_request_accepted(xfer, filename);

				g_strfreev(name_and_ext);
				g_free(ext);
				g_free(dirname);
				g_free(filename);
			}

			g_signal_connect(xfer, "notify::status",
			                 G_CALLBACK(auto_accept_complete_cb), NULL);
			break;
		case FT_REJECT:
			purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_CANCEL_LOCAL);
			break;
	}
}

static void
save_cb(PurpleBlistNode *node, int choice)
{
	if (PURPLE_IS_BUDDY(node))
		node = purple_blist_node_get_parent(node);
	g_return_if_fail(PURPLE_IS_CONTACT(node));
	purple_blist_node_set_int(node, "autoaccept", choice);
}

static void
set_auto_accept_settings(PurpleBlistNode *node, gpointer plugin)
{
	char *message;

	if (PURPLE_IS_BUDDY(node))
		node = purple_blist_node_get_parent(node);
	g_return_if_fail(PURPLE_IS_CONTACT(node));

	message = g_strdup_printf(_("When a file-transfer request arrives from %s"),
					purple_contact_get_alias(PURPLE_CONTACT(node)));
	purple_request_choice(plugin, _("Set Autoaccept Setting"), message,
						NULL, GINT_TO_POINTER(purple_blist_node_get_int(node, "autoaccept")),
						_("_Save"), G_CALLBACK(save_cb),
						_("_Cancel"), NULL,
						NULL, node,
						_("Ask"), GINT_TO_POINTER(FT_ASK),
						_("Auto Accept"), GINT_TO_POINTER(FT_ACCEPT),
						_("Auto Reject"), GINT_TO_POINTER(FT_REJECT),
						NULL);
	g_free(message);
}

static void
context_menu(PurpleBlistNode *node, GList **menu, gpointer plugin)
{
	PurpleActionMenu *action;

	if (!PURPLE_IS_BUDDY(node) && !PURPLE_IS_CONTACT(node) &&
		!purple_blist_node_is_transient(node))
		return;

	action = purple_action_menu_new(_("Autoaccept File Transfers..."),
					G_CALLBACK(set_auto_accept_settings), plugin, NULL);
	(*menu) = g_list_prepend(*menu, action);
}

static PurplePluginPrefFrame *
get_plugin_pref_frame(PurplePlugin *plugin)
{
	PurplePluginPrefFrame *frame;
	PurplePluginPref *pref;

	frame = purple_plugin_pref_frame_new();

	/* XXX: Is there a better way than this? There really should be. */
	pref = purple_plugin_pref_new_with_name_and_label(PREF_PATH, _("Path to save the files in\n"
								"(Please provide the full path)"));
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(PREF_STRANGER,
					_("When a file-transfer request arrives from a user who is\n"
                      "*not* on your buddy list:"));
	purple_plugin_pref_set_pref_type(pref, PURPLE_PLUGIN_PREF_CHOICE);
	purple_plugin_pref_add_choice(pref, _("Ask"), GINT_TO_POINTER(FT_ASK));
	purple_plugin_pref_add_choice(pref, _("Auto Accept"), GINT_TO_POINTER(FT_ACCEPT));
	purple_plugin_pref_add_choice(pref, _("Auto Reject"), GINT_TO_POINTER(FT_REJECT));
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(PREF_NOTIFY,
					_("Notify with a popup when an autoaccepted file transfer is complete\n"
					  "(only when there's no conversation with the sender)"));
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(PREF_NEWDIR,
			_("Create a new directory for each user"));
	purple_plugin_pref_frame_add(frame, pref);

	pref = purple_plugin_pref_new_with_name_and_label(PREF_ESCAPE,
			_("Escape the filenames"));
	purple_plugin_pref_frame_add(frame, pref);

	return frame;
}

static GPluginPluginInfo *
auto_accept_query(GError **error)
{
	const gchar * const authors[] = PLUGIN_AUTHORS;

	return purple_plugin_info_new(
		"id",             PLUGIN_ID,
		"name",           PLUGIN_NAME,
		"version",        DISPLAY_VERSION,
		"category",       PLUGIN_CATEGORY,
		"summary",        PLUGIN_SUMMARY,
		"description",    PLUGIN_DESCRIPTION,
		"authors",        authors,
		"website",        PURPLE_WEBSITE,
		"abi-version",    PURPLE_ABI_VERSION,
		"pref-frame-cb",  get_plugin_pref_frame,
		NULL
	);
}

static gboolean
auto_accept_load(GPluginPlugin *plugin, GError **error)
{
	char *dirname;

	dirname = g_build_filename(g_get_user_special_dir(G_USER_DIRECTORY_DOWNLOAD), "autoaccept", NULL);
	purple_prefs_add_none(PREF_PREFIX);
	purple_prefs_add_string(PREF_PATH, dirname);
	purple_prefs_add_bool(PREF_NOTIFY, TRUE);
	purple_prefs_add_bool(PREF_NEWDIR, TRUE);
	purple_prefs_add_bool(PREF_ESCAPE, TRUE);
	g_free(dirname);

	/* migrate the old pref (we should only care if the plugin is actually *used*) */
	/*
	 * TODO: We should eventually call purple_prefs_remove(PREFS_STRANGER_OLD)
	 *       to clean up after ourselves, but we don't want to do it yet
	 *       so that we don't break users who share a .purple directory
	 *       between old libpurple clients and new libpurple clients.
	 *                                             --Mark Doliner, 2011-01-03
	 */
	if (!purple_prefs_exists(PREF_STRANGER)) {
		if (purple_prefs_exists(PREF_STRANGER_OLD) && purple_prefs_get_bool(PREF_STRANGER_OLD))
			purple_prefs_add_int(PREF_STRANGER, FT_REJECT);
		else
			purple_prefs_set_int(PREF_STRANGER, FT_ASK);
	}

	purple_signal_connect(purple_xfers_get_handle(), "file-recv-request", plugin,
						G_CALLBACK(file_recv_request_cb), plugin);
	purple_signal_connect(purple_blist_get_handle(), "blist-node-extended-menu", plugin,
						G_CALLBACK(context_menu), plugin);
	return TRUE;
}

static gboolean
auto_accept_unload(GPluginPlugin *plugin, gboolean shutdown, GError **error)
{
	return TRUE;
}

GPLUGIN_NATIVE_PLUGIN_DECLARE(auto_accept)
