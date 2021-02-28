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

#ifndef PIDGIN_CREDENTIAL_PROVIDER_STORE_H
#define PIDGIN_CREDENTIAL_PROVIDER_STORE_H

/**
 * SECTION:pidgincredentialproviderstore
 * @section_id: pidgin-pidgincredentialproviderstore
 * @short_description: A GtkListStore that keeps track of credential providers.
 * @title: Credential Provider Store
 *
 * #PidginCredentialProviderStore is a #GtkListStore that automatically keeps
 * track of what credential providers are currently available in libpurple.
 * It's intended to be used any time that you need to present a credential
 * provider selection to the user.
 */

#include <gtk/gtk.h>

#include <purple.h>

/**
 * PidginCredentialProviderStoreColumn:
 * @PIDGIN_CREDENTIAL_PROVIDER_STORE_COLUMN_ID: A constant for the id column.
 * @PIDGIN_CREDENTIAL_PROVIDER_STORE_COLUMN_MARKUP: A constant for the markup
 *                                                  that should be displayed.
 * @PIDGIN_CREDENTIAL_PROVIDER_STORE_N_COLUMNS: The number of columns in the
 *                                              store.
 *
 * A collection of constants for referring to the columns in a
 * #PidginCredentialProviderStore.
 *
 * Since: 3.0.0
 */
typedef enum {
	PIDGIN_CREDENTIAL_PROVIDER_STORE_COLUMN_ID,
	PIDGIN_CREDENTIAL_PROVIDER_STORE_COLUMN_MARKUP,
	PIDGIN_CREDENTIAL_PROVIDER_STORE_N_COLUMNS,
} PidginCredentialProviderStoreColumn;

G_BEGIN_DECLS

#define PIDGIN_TYPE_CREDENTIAL_PROVIDER_STORE pidgin_credential_provider_store_get_type()
G_DECLARE_FINAL_TYPE(PidginCredentialProviderStore, pidgin_credential_provider_store, PIDGIN,
                     CREDENTIAL_PROVIDER_STORE, GtkListStore)

/**
 * pidgin_credential_provider_store_new:
 *
 * Creates a new #PidginCredentialProviderStore that can be used anywhere a
 * #GtkListStore can be used.
 *
 * Returns: (transfer full): The new #PidginCredentialProviderStore instance.
 *
 * Since: 3.0.0
 */
GtkListStore *pidgin_credential_provider_store_new(void);

G_END_DECLS

#endif /* PIDGIN_CREDENTIAL_PROVIDER_STORE_H */
