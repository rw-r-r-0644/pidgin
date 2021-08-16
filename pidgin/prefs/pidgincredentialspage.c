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

#include <purple.h>

#include <handy.h>

#include "pidgincredentialspage.h"

#include "pidgincredentialproviderrow.h"

struct _PidginCredentialsPage {
	HdyPreferencesPage parent;

	GtkWidget *credential_list;
};

G_DEFINE_TYPE(PidginCredentialsPage, pidgin_credentials_page,
              HDY_TYPE_PREFERENCES_PAGE)

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
pidgin_credentials_page_create_row(PurpleCredentialProvider *provider,
                                   gpointer data)
{
	GtkListBox *box = GTK_LIST_BOX(data);
	GtkWidget *row = NULL;

	row = pidgin_credential_provider_row_new(provider);
	gtk_list_box_prepend(box, row);
}

static gint
pidgin_credentials_page_sort_rows(GtkListBoxRow *row1, GtkListBoxRow *row2,
                                  G_GNUC_UNUSED gpointer user_data)
{
	PidginCredentialProviderRow *pcprow = NULL;
	PurpleCredentialProvider *provider = NULL;
	const gchar *id1 = NULL;
	gboolean is_noop1 = FALSE;
	const gchar *id2 = NULL;
	gboolean is_noop2 = FALSE;

	pcprow = PIDGIN_CREDENTIAL_PROVIDER_ROW(row1);
	provider = pidgin_credential_provider_row_get_provider(pcprow);
	id1 = purple_credential_provider_get_id(provider);
	is_noop1 = purple_strequal(id1, "noop-provider");

	pcprow = PIDGIN_CREDENTIAL_PROVIDER_ROW(row2);
	provider = pidgin_credential_provider_row_get_provider(pcprow);
	id2 = purple_credential_provider_get_id(provider);
	is_noop2 = purple_strequal(id2, "noop-provider");

	/* Sort None provider after everything else. */
	if (is_noop1 && is_noop2) {
		return 0;
	} else if (is_noop1 && !is_noop2) {
		return 1;
	} else if (!is_noop1 && is_noop2) {
		return -1;
	}
	/* Sort normally by ID. */
	return g_strcmp0(id1, id2);
}

static void
pidgin_credential_page_list_row_activated_cb(GtkListBox *box,
                                             GtkListBoxRow *row,
                                             G_GNUC_UNUSED gpointer data)
{
	PurpleCredentialManager *manager = NULL;
	PurpleCredentialProvider *provider = NULL;
	const gchar *id = NULL;
	GError *error = NULL;

	provider = pidgin_credential_provider_row_get_provider(
	    PIDGIN_CREDENTIAL_PROVIDER_ROW(row));
	id = purple_credential_provider_get_id(provider);

	manager = purple_credential_manager_get_default();
	if(purple_credential_manager_set_active(manager, id, &error)) {
		purple_prefs_set_string("/purple/credentials/active-provider", id);

		return;
	}

	purple_debug_warning("credentials-page", "failed to set the active "
			     "credential provider to '%s': %s",
			     id, error ? error->message : "unknown error");

	g_clear_error(&error);
}

static void
pidgin_credentials_page_set_active_provider(PidginCredentialsPage *page,
                                            const gchar *new_id)
{
	GList *rows = NULL;

	rows = gtk_container_get_children(GTK_CONTAINER(page->credential_list));
	for (; rows; rows = g_list_delete_link(rows, rows)) {
		PidginCredentialProviderRow *row = NULL;
		PurpleCredentialProvider *provider = NULL;
		const gchar *id = NULL;

		row = PIDGIN_CREDENTIAL_PROVIDER_ROW(rows->data);
		provider = pidgin_credential_provider_row_get_provider(row);
		id = purple_credential_provider_get_id(provider);

		pidgin_credential_provider_row_set_active(row,
		                                          purple_strequal(new_id, id));
	}
}

static void
pidgin_credentials_page_active_provider_changed_cb(const gchar *name,
                                                   PurplePrefType type,
                                                   gconstpointer value,
                                                   gpointer data)
{
	PidginCredentialsPage *page = PIDGIN_CREDENTIALS_PAGE(data);

	pidgin_credentials_page_set_active_provider(page, (const gchar *)value);
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
static void
pidgin_credentials_page_finalize(GObject *obj) {
	purple_prefs_disconnect_by_handle(obj);

	G_OBJECT_CLASS(pidgin_credentials_page_parent_class)->finalize(obj);
}

static void
pidgin_credentials_page_init(PidginCredentialsPage *page) {
	PurpleCredentialManager *manager = NULL;
	const gchar *active = NULL;

	gtk_widget_init_template(GTK_WIDGET(page));

	purple_prefs_add_none("/purple/credentials");
	purple_prefs_add_string("/purple/credentials/active-provider", NULL);

	manager = purple_credential_manager_get_default();
	purple_credential_manager_foreach(
	    manager,
	    pidgin_credentials_page_create_row,
	    page->credential_list);
	gtk_list_box_set_sort_func(GTK_LIST_BOX(page->credential_list),
	                           pidgin_credentials_page_sort_rows, NULL, NULL);

	purple_prefs_connect_callback(page, "/purple/credentials/active-provider",
	                              pidgin_credentials_page_active_provider_changed_cb,
	                              page);

	active = purple_prefs_get_string("/purple/credentials/active-provider");
	if(active != NULL) {
		pidgin_credentials_page_set_active_provider(page, active);
	}
}

static void
pidgin_credentials_page_class_init(PidginCredentialsPageClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	obj_class->finalize = pidgin_credentials_page_finalize;

	gtk_widget_class_set_template_from_resource(
	    widget_class,
	    "/im/pidgin/Pidgin3/Prefs/credentials.ui"
	);

	gtk_widget_class_bind_template_child(widget_class, PidginCredentialsPage,
	                                     credential_list);
	gtk_widget_class_bind_template_callback(widget_class,
	                                        pidgin_credential_page_list_row_activated_cb);
}

/******************************************************************************
 * API
 *****************************************************************************/
GtkWidget *
pidgin_credentials_page_new(void) {
	return GTK_WIDGET(g_object_new(PIDGIN_TYPE_CREDENTIALS_PAGE, NULL));
}
