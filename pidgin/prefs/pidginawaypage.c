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

#include "pidginawaypage.h"
#include "gtksavedstatuses.h"
#include "gtkutils.h"
#include "pidginprefsinternal.h"

struct _PidginAwayPage {
	HdyPreferencesPage parent;

	PidginPrefCombo idle_reporting;
	GtkWidget *mins_before_away;
	GtkWidget *idle_hbox;
	GtkWidget *away_when_idle;
	PidginPrefCombo auto_reply;
	GtkWidget *startup_current_status;
	GtkWidget *startup_hbox;
	GtkWidget *startup_label;
};

G_DEFINE_TYPE(PidginAwayPage, pidgin_away_page, HDY_TYPE_PREFERENCES_PAGE)

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
set_idle_away(PurpleSavedStatus *status)
{
	purple_prefs_set_int("/purple/savedstatus/idleaway",
	                     purple_savedstatus_get_creation_time(status));
}

static void
set_startupstatus(PurpleSavedStatus *status)
{
	purple_prefs_set_int("/purple/savedstatus/startup",
	                     purple_savedstatus_get_creation_time(status));
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
static void
pidgin_away_page_class_init(PidginAwayPageClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(
	    widget_class,
	    "/im/pidgin/Pidgin3/Prefs/away.ui"
	);

	gtk_widget_class_bind_template_child(
			widget_class, PidginAwayPage, idle_reporting.combo);
	gtk_widget_class_bind_template_child(
			widget_class, PidginAwayPage, mins_before_away);
	gtk_widget_class_bind_template_child(
			widget_class, PidginAwayPage, away_when_idle);
	gtk_widget_class_bind_template_child(
			widget_class, PidginAwayPage, idle_hbox);
	gtk_widget_class_bind_template_child(
			widget_class, PidginAwayPage, auto_reply.combo);
	gtk_widget_class_bind_template_child(
			widget_class, PidginAwayPage, startup_current_status);
	gtk_widget_class_bind_template_child(
			widget_class, PidginAwayPage, startup_hbox);
	gtk_widget_class_bind_template_child(
			widget_class, PidginAwayPage, startup_label);
}

static void
pidgin_away_page_init(PidginAwayPage *page)
{
	GtkWidget *menu;

	gtk_widget_init_template(GTK_WIDGET(page));

	page->idle_reporting.type = PURPLE_PREF_STRING;
	page->idle_reporting.key = "/purple/away/idle_reporting";
	pidgin_prefs_bind_dropdown(&page->idle_reporting);

	pidgin_prefs_bind_spin_button("/purple/away/mins_before_away",
			page->mins_before_away);

	pidgin_prefs_bind_checkbox("/purple/away/away_when_idle",
			page->away_when_idle);

	/* TODO: Show something useful if we don't have any saved statuses. */
	menu = pidgin_status_menu(purple_savedstatus_get_idleaway(),
	                          G_CALLBACK(set_idle_away));
	gtk_widget_show_all(menu);
	gtk_box_pack_start(GTK_BOX(page->idle_hbox), menu, FALSE, FALSE, 0);

	g_object_bind_property(page->away_when_idle, "active",
			menu, "sensitive",
			G_BINDING_SYNC_CREATE);

	/* Away stuff */
	page->auto_reply.type = PURPLE_PREF_STRING;
	page->auto_reply.key = "/purple/away/auto_reply";
	pidgin_prefs_bind_dropdown(&page->auto_reply);

	/* Signon status stuff */
	pidgin_prefs_bind_checkbox("/purple/savedstatus/startup_current_status",
			page->startup_current_status);

	/* TODO: Show something useful if we don't have any saved statuses. */
	menu = pidgin_status_menu(purple_savedstatus_get_startup(),
	                          G_CALLBACK(set_startupstatus));
	gtk_widget_show_all(menu);
	gtk_box_pack_start(GTK_BOX(page->startup_hbox), menu, FALSE, FALSE, 0);
	gtk_label_set_mnemonic_widget(GTK_LABEL(page->startup_label), menu);
	pidgin_set_accessible_label(menu, GTK_LABEL(page->startup_label));
	g_object_bind_property(page->startup_current_status, "active",
			page->startup_hbox, "sensitive",
			G_BINDING_SYNC_CREATE|G_BINDING_INVERT_BOOLEAN);
}

/******************************************************************************
 * API
 *****************************************************************************/
GtkWidget *
pidgin_away_page_new(void) {
	return GTK_WIDGET(g_object_new(PIDGIN_TYPE_AWAY_PAGE, NULL));
}
