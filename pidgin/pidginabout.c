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

#ifdef HAVE_MESON_CONFIG
#include "meson-config.h"
#endif

#include <glib/gi18n-lib.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <json-glib/json-glib.h>

#include <talkatu.h>

#include "pidginabout.h"

#include "package_revision.h"
#include "gtkutils.h"
#include "pidgincore.h"
#include "pidginresources.h"

struct _PidginAboutDialog {
	GtkDialog parent;

	GtkWidget *close_button;
	GtkWidget *application_name;
	GtkWidget *stack;

	GtkWidget *main_scrolled_window;
	GtkTextBuffer *main_buffer;

	GtkWidget *developers_page;
	GtkWidget *developers_treeview;
	GtkTreeStore *developers_store;

	GtkWidget *translators_page;
	GtkWidget *translators_treeview;
	GtkTreeStore *translators_store;

	GtkWidget *build_info_page;
	GtkWidget *build_info_treeview;
	GtkTreeStore *build_info_store;
};

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
pidgin_about_dialog_load_application_name(PidginAboutDialog *about) {
	gchar *label = g_strdup_printf("%s %s", PIDGIN_NAME, VERSION);

	gtk_label_set_text(GTK_LABEL(about->application_name), label);

	g_free(label);
}

static void
pidgin_about_dialog_load_main_page(PidginAboutDialog *about) {
	GtkTextIter start;
	GInputStream *istream = NULL;
	GString *str = NULL;
	TalkatuMarkdownBuffer *md_buffer = NULL;
	gchar buffer[8192];
	gssize read = 0, size = 0;

	/* now load the html */
	istream = g_resource_open_stream(pidgin_get_resource(),
	                                 "/im/pidgin/Pidgin/About/about.md",
	                                 G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);

	str = g_string_new("");

	while((read = g_input_stream_read(istream, buffer, sizeof(buffer), NULL, NULL)) > 0) {
		g_string_append_len(str, (gchar *)buffer, read);
		size += read;
	}

	gtk_text_buffer_get_start_iter(about->main_buffer, &start);

	md_buffer = TALKATU_MARKDOWN_BUFFER(about->main_buffer);
	talkatu_markdown_buffer_insert_markdown(md_buffer, &start, str->str, size);

	g_string_free(str, TRUE);

	g_input_stream_close(istream, NULL, NULL);
}

static void
pidgin_about_dialog_load_json(GtkTreeStore *store, const gchar *json_section) {
	GInputStream *istream = NULL;
	GList *l = NULL, *sections = NULL;
	GError *error = NULL;
	JsonParser *parser = NULL;
	JsonNode *root_node = NULL;
	JsonObject *root_object = NULL;
	JsonArray *sections_array = NULL;

	/* get a stream to the credits resource */
	istream = g_resource_open_stream(pidgin_get_resource(),
	                                 "/im/pidgin/Pidgin/About/credits.json",
	                                 G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);

	/* create our parser */
	parser = json_parser_new();

	if(!json_parser_load_from_stream(parser, istream, NULL, &error)) {
		g_critical("%s", error->message);
	}

	root_node = json_parser_get_root(parser);
	root_object = json_node_get_object(root_node);

	sections_array = json_object_get_array_member(root_object, json_section);
	sections = json_array_get_elements(sections_array);

	for(l = sections; l; l = l->next) {
		GtkTreeIter section_iter;
		JsonObject *section = json_node_get_object(l->data);
		JsonArray *people = NULL;
		gchar *markup = NULL;
		const gchar *title = NULL;
		guint idx = 0, n_people = 0;

		title = json_object_get_string_member(section, "title");
		markup = g_strdup_printf("<b><big>%s</big></b>", title);

		gtk_tree_store_append(store, &section_iter, NULL);
		gtk_tree_store_set(store, &section_iter,
		                   0, markup,
		                   1, 0.5f,
		                   -1);

		g_free(markup);

		people = json_object_get_array_member(section, "people");
		n_people = json_array_get_length(people);

		for(idx = 0; idx < n_people; idx++) {
			GtkTreeIter person_iter;

			gtk_tree_store_append(store, &person_iter, &section_iter);
			gtk_tree_store_set(store, &person_iter,
			                   0, json_array_get_string_element(people, idx),
			                   1, 0.5f,
			                   -1);
		}
	}

	g_list_free(sections);

	/* clean up */
	g_object_unref(G_OBJECT(parser));

	g_input_stream_close(istream, NULL, NULL);
}

static void
pidgin_about_dialog_load_developers(PidginAboutDialog *about) {
	pidgin_about_dialog_load_json(about->developers_store, "developers");
}

static void
pidgin_about_dialog_load_translators(PidginAboutDialog *about) {
	pidgin_about_dialog_load_json(about->translators_store, "languages");
}

static void
pidgin_about_dialog_add_build_args(PidginAboutDialog *about, const gchar *title,
                                   const gchar *build_args)
{
	GtkTreeIter section, value;
	gchar **splits = NULL;
	gchar *markup = NULL;
	gint idx = 0;

	markup = g_strdup_printf("<b>%s</b>", title);
	gtk_tree_store_append(about->build_info_store, &section, NULL);
	gtk_tree_store_set(about->build_info_store, &section,
	                   0, markup,
	                   -1);
	g_free(markup);

	/* now walk through the arguments and add them */
	splits = g_strsplit(build_args, " ", -1);
	for(idx = 0; splits[idx]; idx++) {
		gchar **value_split = g_strsplit(splits[idx], "=", 2);

		if(value_split[0] == NULL || value_split[0][0] == '\0') {
			continue;
		}

		gtk_tree_store_append(about->build_info_store, &value, &section);
		gtk_tree_store_set(about->build_info_store, &value, 0, value_split[0],
		                   1, value_split[1] ? value_split[1] : "", -1);

		g_strfreev(value_split);
	}

	g_strfreev(splits);
}

static void
pidgin_about_dialog_build_info_add_version(GtkTreeStore *store,
                                           GtkTreeIter *section,
                                           const gchar *title,
                                           guint major,
                                           guint minor,
                                           guint micro)
{
	GtkTreeIter item;
	gchar *version = g_strdup_printf("%u.%u.%u", major, minor, micro);

	gtk_tree_store_append(store, &item, section);
	gtk_tree_store_set(store, &item,
	                   0, title,
	                   1, version,
	                   -1);
	g_free(version);
}

static void
pidgin_about_dialog_load_build_info(PidginAboutDialog *about) {
	GtkTreeIter section, item;
	gchar *markup = NULL;

	/* create the section */
	markup = g_strdup_printf("<b>%s</b>", _("Build Information"));
	gtk_tree_store_append(about->build_info_store, &section, NULL);
	gtk_tree_store_set(about->build_info_store, &section,
	                   0, markup,
	                   -1);
	g_free(markup);

	/* add the commit hash */
	gtk_tree_store_append(about->build_info_store, &item, &section);
	gtk_tree_store_set(about->build_info_store, &item,
	                   0, "Commit Hash",
	                   1, REVISION,
	                   -1);

	/* add the purple version */
	pidgin_about_dialog_build_info_add_version(about->build_info_store,
	                                           &section, _("Purple Version"),
	                                           PURPLE_MAJOR_VERSION,
	                                           PURPLE_MINOR_VERSION,
	                                           PURPLE_MICRO_VERSION);

	/* add the glib version */
	pidgin_about_dialog_build_info_add_version(about->build_info_store,
	                                           &section, _("GLib Version"),
	                                           GLIB_MAJOR_VERSION,
	                                           GLIB_MINOR_VERSION,
	                                           GLIB_MICRO_VERSION);

	/* add the gtk version */
	pidgin_about_dialog_build_info_add_version(about->build_info_store,
	                                           &section, _("GTK Version"),
	                                           GTK_MAJOR_VERSION,
	                                           GTK_MINOR_VERSION,
	                                           GTK_MICRO_VERSION);
}

static void
pidgin_about_dialog_load_runtime_info(PidginAboutDialog *about) {
	GtkTreeIter section;
	gchar *markup = NULL;

	/* create the section */
	markup = g_strdup_printf("<b>%s</b>", _("Runtime Information"));
	gtk_tree_store_append(about->build_info_store, &section, NULL);
	gtk_tree_store_set(about->build_info_store, &section,
	                   0, markup,
	                   -1);
	g_free(markup);

	/* add the purple version */
	pidgin_about_dialog_build_info_add_version(about->build_info_store,
	                                           &section, _("Purple Version"),
	                                           purple_major_version,
	                                           purple_minor_version,
	                                           purple_micro_version);

	/* add the glib version */
	pidgin_about_dialog_build_info_add_version(about->build_info_store,
	                                           &section, _("GLib Version"),
	                                           glib_major_version,
	                                           glib_minor_version,
	                                           glib_micro_version);

	/* add the gtk version */
	pidgin_about_dialog_build_info_add_version(about->build_info_store,
	                                           &section, _("GTK Version"),
	                                           gtk_major_version,
	                                           gtk_minor_version,
	                                           gtk_micro_version);
}

static void
pidgin_about_dialog_load_plugin_search_paths(PidginAboutDialog *about) {
	GtkTreeIter section;
	GList *path = NULL;
	gchar *markup = NULL;

	/* create the section */
	markup = g_strdup_printf("<b>%s</b>", _("Plugin Search Paths"));
	gtk_tree_store_append(about->build_info_store, &section, NULL);
	gtk_tree_store_set(about->build_info_store, &section,
	                   0, markup,
	                   -1);
	g_free(markup);

	/* add the search paths */
	for(path = gplugin_manager_get_paths(); path != NULL; path = path->next) {
		GtkTreeIter iter;

		gtk_tree_store_append(about->build_info_store, &iter, &section);
		gtk_tree_store_set(about->build_info_store, &iter,
		                   0, (gchar*)path->data,
		                   -1);
	}
}

static void
pidgin_about_dialog_load_build_configuration(PidginAboutDialog *about) {
#ifdef MESON_ARGS
	pidgin_about_dialog_add_build_args(about, _("Meson Arguments"), MESON_ARGS);
#endif /* MESON_ARGS */

	pidgin_about_dialog_load_build_info(about);
	pidgin_about_dialog_load_runtime_info(about);
	pidgin_about_dialog_load_plugin_search_paths(about);
}

/******************************************************************************
 * Callbacks
 *****************************************************************************/
static void
pidgin_about_dialog_close(GtkWidget *b, gpointer data) {
	gtk_widget_destroy(GTK_WIDGET(data));
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
G_DEFINE_TYPE(PidginAboutDialog, pidgin_about_dialog, GTK_TYPE_DIALOG);

static void
pidgin_about_dialog_class_init(PidginAboutDialogClass *klass) {
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(
		widget_class,
		"/im/pidgin/Pidgin/About/about.ui"
	);

	gtk_widget_class_bind_template_child(widget_class, PidginAboutDialog, close_button);
	gtk_widget_class_bind_template_child(widget_class, PidginAboutDialog, application_name);
	gtk_widget_class_bind_template_child(widget_class, PidginAboutDialog, stack);

	gtk_widget_class_bind_template_child(widget_class, PidginAboutDialog, main_scrolled_window);
	gtk_widget_class_bind_template_child(widget_class, PidginAboutDialog, main_buffer);

	gtk_widget_class_bind_template_child(widget_class, PidginAboutDialog, developers_page);
	gtk_widget_class_bind_template_child(widget_class, PidginAboutDialog, developers_store);
	gtk_widget_class_bind_template_child(widget_class, PidginAboutDialog, developers_treeview);

	gtk_widget_class_bind_template_child(widget_class, PidginAboutDialog, translators_page);
	gtk_widget_class_bind_template_child(widget_class, PidginAboutDialog, translators_store);
	gtk_widget_class_bind_template_child(widget_class, PidginAboutDialog, translators_treeview);

	gtk_widget_class_bind_template_child(widget_class, PidginAboutDialog, build_info_page);
	gtk_widget_class_bind_template_child(widget_class, PidginAboutDialog, build_info_store);
	gtk_widget_class_bind_template_child(widget_class, PidginAboutDialog, build_info_treeview);
}

static void
pidgin_about_dialog_init(PidginAboutDialog *about) {
	gtk_widget_init_template(GTK_WIDGET(about));

	/* wire up the close button */
	g_signal_connect(about->close_button, "clicked",
	                 G_CALLBACK(pidgin_about_dialog_close), about);

	/* setup the application name label */
	pidgin_about_dialog_load_application_name(about);

	/* setup the main page */
	pidgin_about_dialog_load_main_page(about);

	/* setup the developers stuff */
	pidgin_about_dialog_load_developers(about);
	gtk_tree_view_expand_all(GTK_TREE_VIEW(about->developers_treeview));

	/* setup the translators stuff */
	pidgin_about_dialog_load_translators(about);
	gtk_tree_view_expand_all(GTK_TREE_VIEW(about->translators_treeview));

	/* setup the build info page */
	pidgin_about_dialog_load_build_configuration(about);
	gtk_tree_view_expand_all(GTK_TREE_VIEW(about->build_info_treeview));
}

/******************************************************************************
 * Public API
 *****************************************************************************/
GtkWidget *
pidgin_about_dialog_new(void) {
	return GTK_WIDGET(g_object_new(
		PIDGIN_TYPE_ABOUT_DIALOG,
		"title", "About Pidgin",
		NULL
	));
}

