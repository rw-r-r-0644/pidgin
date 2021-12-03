/*
 * Purple - Internet Messaging Library
 * Copyright (C) Pidgin Developers <devel@pidgin.im>
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

#include "purpleaccountmanager.h"
#include "purpleprivate.h"

#include "account.h"
#include "accounts.h"
#include "core.h"

enum {
	SIG_ADDED,
	SIG_REMOVED,
	N_SIGNALS,
};
static guint signals[N_SIGNALS] = {0, };

struct _PurpleAccountManager {
	GObject parent;

	GList *accounts;
};

static PurpleAccountManager *default_manager = NULL;

G_DEFINE_TYPE(PurpleAccountManager, purple_account_manager, G_TYPE_OBJECT)

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
static void
purple_account_manager_finalize(GObject *obj) {
	PurpleAccountManager *manager = PURPLE_ACCOUNT_MANAGER(obj);
	GList *l = NULL;

	for(l = manager->accounts; l != NULL; l = l->next) {
		g_object_unref(l->data);
	}

	G_OBJECT_CLASS(purple_account_manager_parent_class)->finalize(obj);
}

static void
purple_account_manager_init(PurpleAccountManager *manager) {
}

static void
purple_account_manager_class_init(PurpleAccountManagerClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->finalize = purple_account_manager_finalize;

	/**
	 * PurpleAccountManager::added:
	 * @manager: The account manager instance.
	 * @account: The account that was added.
	 *
	 * Emitted after @account was added to @manager.
	 *
	 * Since: 3.0.0
	 */
	signals[SIG_ADDED] = g_signal_new_class_handler(
		"added",
		G_OBJECT_CLASS_TYPE(klass),
		G_SIGNAL_RUN_LAST,
		NULL,
		NULL,
		NULL,
		NULL,
		G_TYPE_NONE,
		1,
		PURPLE_TYPE_ACCOUNT);

	/**
	 * PurpleAccountManager::removed:
	 * @manager: The account manager instance.
	 * @account: The account that was removed.
	 *
	 * Emitted after @account was removed from @manager.
	 *
	 * Since: 3.0.0
	 */
	signals[SIG_REMOVED] = g_signal_new_class_handler(
		"removed",
		G_OBJECT_CLASS_TYPE(klass),
		G_SIGNAL_RUN_LAST,
		NULL,
		NULL,
		NULL,
		NULL,
		G_TYPE_NONE,
		1,
		PURPLE_TYPE_ACCOUNT);
}

/******************************************************************************
 * Private API
 *****************************************************************************/
void
purple_account_manager_startup(void) {
	if(!PURPLE_IS_ACCOUNT_MANAGER(default_manager)) {
		default_manager = g_object_new(PURPLE_TYPE_ACCOUNT_MANAGER, NULL);
	}
}

void
purple_account_manager_shutdown(void) {
	g_clear_object(&default_manager);
}

/******************************************************************************
 * Public API
 *****************************************************************************/
PurpleAccountManager *
purple_account_manager_get_default(void) {
	return default_manager;
}

void
purple_account_manager_add(PurpleAccountManager *manager,
                           PurpleAccount *account)
{
	g_return_if_fail(PURPLE_IS_ACCOUNT_MANAGER(manager));
	g_return_if_fail(PURPLE_IS_ACCOUNT(account));

	/* If the manager already knows about the account, we do nothing. */
	if(g_list_find(manager->accounts, account) != NULL) {
		return;
	}

	/* Since the manager doesn't know about the account, put the new account
	 * at the start of the list as that's likely to be the first one in user
	 * interfaces and the most likely to have configuration issues as it's a
	 * new account.
	 */
	manager->accounts = g_list_prepend(manager->accounts, account);

	purple_accounts_schedule_save();

	g_signal_emit(manager, signals[SIG_ADDED], 0, account);

	/* Finally emit the old purple signal that will eventually be removed. */
	purple_signal_emit(purple_accounts_get_handle(), "account-added", account);
}

void
purple_account_manager_remove(PurpleAccountManager *manager,
                              PurpleAccount *account)
{
	g_return_if_fail(PURPLE_IS_ACCOUNT_MANAGER(manager));
	g_return_if_fail(PURPLE_IS_ACCOUNT(account));

	manager->accounts = g_list_remove(manager->accounts, account);

	purple_accounts_schedule_save();

	/* Clearing the error ensures that account-error-changed is emitted,
	 * which is the end of the guarantee that the error's pointer is valid.
	 */
	purple_account_clear_current_error(account);

	g_signal_emit(manager, signals[SIG_REMOVED], 0, account);

	/* Finally emit the old purple signal that will eventually be removed. */
	purple_signal_emit(purple_accounts_get_handle(), "account-removed",
	                   account);
}

GList *
purple_account_manager_get_all(PurpleAccountManager *manager) {
	g_return_val_if_fail(PURPLE_IS_ACCOUNT_MANAGER(manager), NULL);

	return manager->accounts;
}

GList *
purple_account_manager_get_active(PurpleAccountManager *manager) {
	GList *active = NULL, *l = NULL;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT_MANAGER(manager), NULL);

	for(l = manager->accounts; l != NULL; l = l->next) {
		PurpleAccount *account = PURPLE_ACCOUNT(l->data);

		if(purple_account_get_enabled(account, purple_core_get_ui())) {
			active = g_list_append(active, account);
		}
	}

	return active;
}

void
purple_account_manager_reorder(PurpleAccountManager *manager,
                               PurpleAccount *account,
                               guint new_index)
{
	GList *l = NULL;
	gboolean found = FALSE;
	guint index = 0;

	g_return_if_fail(PURPLE_IS_ACCOUNT_MANAGER(manager));
	g_return_if_fail(PURPLE_IS_ACCOUNT(account));

	/* Iterate over the known accounts until we have found a matching account
	 * or exhausted the list. For each iteration increment idx.
	 */
	for(l = manager->accounts; l != NULL; l = l->next, index++) {
		if(PURPLE_ACCOUNT(l->data) == account) {
			manager->accounts = g_list_delete_link(manager->accounts, l);

			found = TRUE;

			/* If new_index is greater than the current index, we need to
			 * decrement new_index by 1 to account for the move as we'll be
			 * inserting into a list with one less item.
			 */
			if(new_index > index) {
				new_index--;
			}

			break;
		}
	}

	if(!found) {
		g_critical("Unregistered account (%s) found during reorder!",
		           purple_account_get_username(account));
		return;
	}

	/* Insert the account into its new position. */
	manager->accounts = g_list_insert(manager->accounts, account, new_index);

	purple_accounts_schedule_save();
}

PurpleAccount *
purple_account_manager_find(PurpleAccountManager *manager,
                            const gchar *username, const gchar *protocol_id)
{
	GList *l;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT_MANAGER(manager), NULL);
	g_return_val_if_fail(username != NULL, NULL);
	g_return_val_if_fail(protocol_id != NULL, NULL);

	for(l = manager->accounts; l != NULL; l = l->next) {
		PurpleAccount *account = PURPLE_ACCOUNT(l->data);
		gchar *normalized = NULL;
		const gchar *existing_protocol_id = NULL;
		const gchar *existing_username = NULL;
		const gchar *existing_normalized = NULL;

		/* Check if the protocol id matches what the user asked for. */
		existing_protocol_id = purple_account_get_protocol_id(account);
		if(!purple_strequal(existing_protocol_id, protocol_id)) {
			continue;
		}

		/* Finally verify the username. */
		existing_username = purple_account_get_username(account);
		normalized = g_strdup(purple_normalize(account, username));
		existing_normalized = purple_normalize(account, existing_username);

		if(purple_strequal(existing_normalized, normalized)) {
			g_free(normalized);

			return account;
		}
		g_free(normalized);
	}

	return NULL;
}

void
purple_account_manager_foreach(PurpleAccountManager *manager,
                               PurpleAccountManagerForeachFunc callback,
                               gpointer data)
{
	GList *l = NULL;

	g_return_if_fail(PURPLE_IS_ACCOUNT_MANAGER(manager));
	g_return_if_fail(callback != NULL);

	for(l = manager->accounts; l != NULL; l = l->next) {
		callback(PURPLE_ACCOUNT(l->data), data);
	}
}
