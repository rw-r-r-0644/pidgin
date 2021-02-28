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

#include "pidgincredentialspage.h"

#include "pidgincredentialproviderstore.h"

struct _PidginCredentialsPage {
	GtkBox parent;

	GtkWidget *combo;
	GtkCellRenderer *renderer;
};

G_DEFINE_TYPE(PidginCredentialsPage, pidgin_credentials_page,
              GTK_TYPE_BOX)

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
pidgin_credentials_page_combo_changed_cb(GtkComboBox *widget, gpointer data) {
	PidginCredentialsPage *page = PIDGIN_CREDENTIALS_PAGE(data);
	GtkTreeIter iter;

	if(gtk_combo_box_get_active_iter(GTK_COMBO_BOX(page->combo), &iter)) {
		PurpleCredentialManager *manager = NULL;
		GError *error = NULL;
		GtkTreeModel *model = NULL;
		gchar *id = NULL;

		model = gtk_combo_box_get_model(GTK_COMBO_BOX(page->combo));

		gtk_tree_model_get(model, &iter,
		                   PIDGIN_CREDENTIAL_PROVIDER_STORE_COLUMN_ID, &id,
		                   -1);

		manager = purple_credential_manager_get_default();
		if(purple_credential_manager_set_active_provider(manager, id, &error)) {
			purple_prefs_set_string("/purple/credentials/active-provider", id);

			g_free(id);

			return;
		}

		purple_debug_warning("credentials-page", "failed to set the active "
		                     "credential provider to '%s': %s",
		                     id,
		                     error ? error->message : "unknown error");

		g_free(id);
		g_clear_error(&error);
	}
}

static void
pidgin_credentials_page_set_active_provider(PidginCredentialsPage *page,
                                            const gchar *new_id)
{
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;

	model = gtk_combo_box_get_model(GTK_COMBO_BOX(page->combo));

	if(gtk_tree_model_get_iter_first(model, &iter)) {
		do {
			gchar *id = NULL;

			gtk_tree_model_get(model, &iter,
			                   PIDGIN_CREDENTIAL_PROVIDER_STORE_COLUMN_ID, &id,
			                   -1);

			if(purple_strequal(new_id, id)) {
				g_signal_handlers_block_by_func(page->combo,
				                                pidgin_credentials_page_combo_changed_cb,
				                                page);

				gtk_combo_box_set_active_iter(GTK_COMBO_BOX(page->combo),
				                              &iter);

				g_signal_handlers_unblock_by_func(page->combo,
				                                  pidgin_credentials_page_combo_changed_cb,
				                                  page);

				g_free(id);

				return;
			}

			g_free(id);
		} while(gtk_tree_model_iter_next(model, &iter));
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
 * GObjectImplementation
 *****************************************************************************/
static void
pidgin_credentials_page_finalize(GObject *obj) {
	purple_prefs_disconnect_by_handle(obj);

	G_OBJECT_CLASS(pidgin_credentials_page_parent_class)->finalize(obj);
}

static void
pidgin_credentials_page_init(PidginCredentialsPage *page) {
	const gchar *active = NULL;

	gtk_widget_init_template(GTK_WIDGET(page));

	/* Set some constant properties on the renderer. This stuff is kind of
	 * dodgy, but it does stop the dialog from growing to fit a long
	 * description.
	 */
	g_object_set(G_OBJECT(page->renderer),
	             "width-chars", 40,
	             "wrap-mode", PANGO_WRAP_WORD_CHAR,
	             NULL);

	purple_prefs_add_none("/purple/credentials");
	purple_prefs_add_string("/purple/credentials/active-provider", NULL);

	purple_prefs_connect_callback(page, "/purple/credentials/active-provider",
	                              pidgin_credentials_page_active_provider_changed_cb,
	                              page);

	g_signal_connect(G_OBJECT(page->combo), "changed",
	                 G_CALLBACK(pidgin_credentials_page_combo_changed_cb),
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
	    "/im/pidgin/Pidgin/Prefs/credentials.ui"
	);

	gtk_widget_class_bind_template_child(widget_class, PidginCredentialsPage,
	                                     combo);
	gtk_widget_class_bind_template_child(widget_class, PidginCredentialsPage,
	                                     renderer);
}

/******************************************************************************
 * API
 *****************************************************************************/
GtkWidget *
pidgin_credentials_page_new(void) {
	return GTK_WIDGET(g_object_new(PIDGIN_TYPE_CREDENTIALS_PAGE, NULL));
}
