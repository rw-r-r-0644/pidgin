/* pidgin
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
 */

#include <purple.h>

#include "gtkblist.h"
#include "gtkconn.h"
#include "gtkdialogs.h"
#include "gtkstatusbox.h"
#include "pidgincore.h"
#include "gtkutils.h"

#define INITIAL_RECON_DELAY_MIN 8
#define INITIAL_RECON_DELAY_MAX 60

#define MAX_RECON_DELAY 600
#define MAX_RACCOON_DELAY "shorter in urban areas"

typedef struct {
	int delay;
	guint timeout;
} PidginAutoRecon;

/*
 * Contains accounts that are auto-reconnecting.
 * The key is a pointer to the PurpleAccount and the
 * value is a pointer to a PidginAutoRecon.
 */
static GHashTable *auto_reconns = NULL;

static void
pidgin_connection_connected(PurpleConnection *gc)
{
	PurpleAccount *account;

	account  = purple_connection_get_account(gc);

	g_hash_table_remove(auto_reconns, account);
}

static void
pidgin_connection_disconnected(PurpleConnection *gc)
{
	if (purple_connections_get_all() != NULL)
		return;

	pidgin_dialogs_destroy_all();
}

static void
free_auto_recon(gpointer data)
{
	PidginAutoRecon *info = data;

	if (info->timeout != 0)
		g_source_remove(info->timeout);

	g_free(info);
}

static gboolean
do_signon(gpointer data)
{
	PurpleAccount *account = data;
	PidginAutoRecon *info;
	PurpleStatus *status;

	purple_debug_info("autorecon", "do_signon called\n");
	g_return_val_if_fail(account != NULL, FALSE);
	info = g_hash_table_lookup(auto_reconns, account);

	if (info)
		info->timeout = 0;

	status = purple_account_get_active_status(account);
	if (purple_status_is_online(status))
	{
		purple_debug_info("autorecon", "calling purple_account_connect\n");
		purple_account_connect(account);
		purple_debug_info("autorecon", "done calling purple_account_connect\n");
	}

	return FALSE;
}

static void
pidgin_connection_report_disconnect(PurpleConnection *gc,
                                    PurpleConnectionError reason,
                                    const char *text)
{
	PurpleAccount *account = NULL;
	PidginAutoRecon *info;

	account = purple_connection_get_account(gc);
	info = g_hash_table_lookup(auto_reconns, account);

	if (!purple_connection_error_is_fatal (reason)) {
		if (info == NULL) {
			info = g_new0(PidginAutoRecon, 1);
			g_hash_table_insert(auto_reconns, account, info);
			info->delay = g_random_int_range(INITIAL_RECON_DELAY_MIN, INITIAL_RECON_DELAY_MAX);
		} else {
			info->delay = MIN(2 * info->delay, MAX_RECON_DELAY);
			if (info->timeout != 0)
				g_source_remove(info->timeout);
		}
		info->timeout = g_timeout_add_seconds(info->delay, do_signon, account);
	} else {
		if (info != NULL)
			g_hash_table_remove(auto_reconns, account);

		purple_account_set_enabled(account, PIDGIN_UI, FALSE);
	}
}

static void
pidgin_connection_network_connected(void) {
	PurpleAccountManager *manager = NULL;
	GList *l = NULL;

	manager = purple_account_manager_get_default();
	l = purple_account_manager_get_active(manager);

	while(l != NULL) {
		PurpleAccount *account = (PurpleAccount*)l->data;
		g_hash_table_remove(auto_reconns, account);
		if (purple_account_is_disconnected(account))
			do_signon(account);

		l = g_list_delete_link(l, l);
	}
}

static void
pidgin_connection_network_disconnected(void) {
	PurpleAccountManager *manager = NULL;
	GList *l = NULL;

	manager = purple_account_manager_get_default();
	l = purple_account_manager_get_active(manager);

	while(l != NULL) {
		PurpleAccount *a = (PurpleAccount*)l->data;
		if (!purple_account_is_disconnected(a)) {
			purple_account_disconnect(a);
		}

		l = g_list_delete_link(l, l);
	}
}

static void pidgin_connection_notice(PurpleConnection *gc, const char *text)
{ }

static PurpleConnectionUiOps conn_ui_ops =
{
	NULL,
	pidgin_connection_connected,
	pidgin_connection_disconnected,
	pidgin_connection_notice,
	pidgin_connection_network_connected,
	pidgin_connection_network_disconnected,
	pidgin_connection_report_disconnect,
	NULL,
	NULL,
	NULL,
	NULL
};

PurpleConnectionUiOps *
pidgin_connections_get_ui_ops(void)
{
	return &conn_ui_ops;
}

static void
account_removed_cb(PurpleAccount *account, gpointer user_data)
{
	g_hash_table_remove(auto_reconns, account);
}


/**************************************************************************
* GTK connection glue
**************************************************************************/

void *
pidgin_connection_get_handle(void)
{
	static int handle;

	return &handle;
}

void
pidgin_connection_init(void)
{
	auto_reconns = g_hash_table_new_full(
							g_direct_hash, g_direct_equal,
							NULL, free_auto_recon);

	purple_signal_connect(purple_accounts_get_handle(), "account-removed",
						pidgin_connection_get_handle(),
						G_CALLBACK(account_removed_cb), NULL);
}

void
pidgin_connection_uninit(void)
{
	purple_signals_disconnect_by_handle(pidgin_connection_get_handle());

	g_hash_table_destroy(auto_reconns);
}
