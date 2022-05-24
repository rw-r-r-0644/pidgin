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

#include <adwaita.h>

#include "pidginawayprefs.h"
#include "gtksavedstatuses.h"
#include "gtkutils.h"
#include "pidginprefsinternal.h"

struct _PidginAwayPrefs {
	AdwPreferencesPage parent;

	PidginPrefCombo idle_reporting;
	GtkWidget *mins_before_away;
	GtkWidget *idle_hbox;
	GtkWidget *away_when_idle;
	PidginPrefCombo auto_reply;
	GtkWidget *startup_current_status;
	GtkWidget *startup_hbox;
	GtkWidget *startup_label;
};

G_DEFINE_TYPE(PidginAwayPrefs, pidgin_away_prefs, ADW_TYPE_PREFERENCES_PAGE)

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
pidgin_away_prefs_class_init(PidginAwayPrefsClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(
	    widget_class,
	    "/im/pidgin/Pidgin3/Prefs/away.ui"
	);

	gtk_widget_class_bind_template_child(widget_class, PidginAwayPrefs,
	                                     idle_reporting.combo);
	gtk_widget_class_bind_template_child(widget_class, PidginAwayPrefs,
	                                     mins_before_away);
	gtk_widget_class_bind_template_child(widget_class, PidginAwayPrefs,
	                                     away_when_idle);
	gtk_widget_class_bind_template_child(widget_class, PidginAwayPrefs,
	                                     idle_hbox);
	gtk_widget_class_bind_template_child(widget_class, PidginAwayPrefs,
	                                     auto_reply.combo);
	gtk_widget_class_bind_template_child(widget_class, PidginAwayPrefs,
	                                     startup_current_status);
	gtk_widget_class_bind_template_child(widget_class, PidginAwayPrefs,
	                                     startup_hbox);
	gtk_widget_class_bind_template_child(widget_class, PidginAwayPrefs,
	                                     startup_label);
}

static void
pidgin_away_prefs_init(PidginAwayPrefs *prefs)
{
	GtkWidget *menu;

	gtk_widget_init_template(GTK_WIDGET(prefs));

	prefs->idle_reporting.type = PURPLE_PREF_STRING;
	prefs->idle_reporting.key = "/purple/away/idle_reporting";
	pidgin_prefs_bind_dropdown(&prefs->idle_reporting);

	pidgin_prefs_bind_spin_button("/purple/away/mins_before_away",
			prefs->mins_before_away);

	pidgin_prefs_bind_checkbox("/purple/away/away_when_idle",
			prefs->away_when_idle);

	/* TODO: Show something useful if we don't have any saved statuses. */
	menu = pidgin_status_menu(purple_savedstatus_get_idleaway(),
	                          G_CALLBACK(set_idle_away));
	gtk_box_append(GTK_BOX(prefs->idle_hbox), menu);

	g_object_bind_property(prefs->away_when_idle, "active",
			menu, "sensitive",
			G_BINDING_SYNC_CREATE);

	/* Away stuff */
	prefs->auto_reply.type = PURPLE_PREF_STRING;
	prefs->auto_reply.key = "/purple/away/auto_reply";
	pidgin_prefs_bind_dropdown(&prefs->auto_reply);

	/* Signon status stuff */
	pidgin_prefs_bind_checkbox("/purple/savedstatus/startup_current_status",
			prefs->startup_current_status);

	/* TODO: Show something useful if we don't have any saved statuses. */
	menu = pidgin_status_menu(purple_savedstatus_get_startup(),
	                          G_CALLBACK(set_startupstatus));
	gtk_box_append(GTK_BOX(prefs->startup_hbox), menu);
	gtk_label_set_mnemonic_widget(GTK_LABEL(prefs->startup_label), menu);
	pidgin_set_accessible_label(menu, GTK_LABEL(prefs->startup_label));
	g_object_bind_property(prefs->startup_current_status, "active",
			prefs->startup_hbox, "sensitive",
			G_BINDING_SYNC_CREATE|G_BINDING_INVERT_BOOLEAN);
}

/******************************************************************************
 * API
 *****************************************************************************/
GtkWidget *
pidgin_away_prefs_new(void) {
	return GTK_WIDGET(g_object_new(PIDGIN_TYPE_AWAY_PREFS, NULL));
}
