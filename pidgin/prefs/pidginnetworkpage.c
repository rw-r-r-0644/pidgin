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

#include <glib/gi18n-lib.h>

#include <purple.h>

#include <handy.h>
#include <nice.h>

#include "pidginnetworkpage.h"
#include "pidginprefsinternal.h"

struct _PidginNetworkPage {
	HdyPreferencesPage parent;

	GtkWidget *stun_server;
	GtkWidget *auto_ip;
	GtkWidget *public_ip;
	GtkWidget *public_ip_hbox;
	GtkWidget *map_ports;
	GtkWidget *ports_range_use;
	GtkWidget *ports_range_hbox;
	GtkWidget *ports_range_start;
	GtkWidget *ports_range_end;
	GtkWidget *turn_server;
	GtkWidget *turn_port_udp;
	GtkWidget *turn_port_tcp;
	GtkWidget *turn_username;
	GtkWidget *turn_password;
};

G_DEFINE_TYPE(PidginNetworkPage, pidgin_network_page, HDY_TYPE_PREFERENCES_PAGE)

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
network_ip_changed(GtkEntry *entry, gpointer data)
{
	const gchar *text = gtk_entry_get_text(entry);
	GtkStyleContext *context = gtk_widget_get_style_context(GTK_WIDGET(entry));

	if (text && *text) {
		if (g_hostname_is_ip_address(text)) {
			purple_network_set_public_ip(text);
			gtk_style_context_add_class(context, "good-ip");
			gtk_style_context_remove_class(context, "bad-ip");
		} else {
			gtk_style_context_add_class(context, "bad-ip");
			gtk_style_context_remove_class(context, "good-ip");
		}

	} else {
		purple_network_set_public_ip("");
		gtk_style_context_remove_class(context, "bad-ip");
		gtk_style_context_remove_class(context, "good-ip");
	}
}

static gboolean
network_stun_server_changed_cb(GtkWidget *widget,
                               GdkEventFocus *event, gpointer data)
{
	GtkEntry *entry = GTK_ENTRY(widget);
	purple_prefs_set_string("/purple/network/stun_server",
		gtk_entry_get_text(entry));
	purple_network_set_stun_server(gtk_entry_get_text(entry));

	return FALSE;
}

static gboolean
network_turn_server_changed_cb(GtkWidget *widget,
                               GdkEventFocus *event, gpointer data)
{
	GtkEntry *entry = GTK_ENTRY(widget);
	purple_prefs_set_string("/purple/network/turn_server",
		gtk_entry_get_text(entry));
	purple_network_set_turn_server(gtk_entry_get_text(entry));

	return FALSE;
}

static void
auto_ip_button_clicked_cb(GtkWidget *button, gpointer null)
{
	const char *ip;
	PurpleStunNatDiscovery *stun;
	char *auto_ip_text;
	GList *list = NULL;

	/* Make a lookup for the auto-detected IP ourselves. */
	if (purple_prefs_get_bool("/purple/network/auto_ip")) {
		/* Check if STUN discovery was already done */
		stun = purple_stun_discover(NULL);
		if ((stun != NULL) && (stun->status == PURPLE_STUN_STATUS_DISCOVERED)) {
			ip = stun->publicip;
		} else {
			/* Attempt to get the IP from a NAT device using UPnP */
			ip = purple_upnp_get_public_ip();
			if (ip == NULL) {
				/* Attempt to get the IP from a NAT device using NAT-PMP */
				ip = purple_pmp_get_public_ip();
				if (ip == NULL) {
					/* Just fetch the first IP of the local system */
					list = nice_interfaces_get_local_ips(FALSE);
					if (list) {
						ip = list->data;
					} else {
						ip = "0.0.0.0";
					}
				}
			}
		}
	} else {
		ip = _("Disabled");
	}

	auto_ip_text = g_strdup_printf(_("Use _automatically detected IP address: %s"), ip);
	gtk_button_set_label(GTK_BUTTON(button), auto_ip_text);
	g_free(auto_ip_text);
	g_list_free_full(list, g_free);
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
static void
pidgin_network_page_class_init(PidginNetworkPageClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(
		widget_class,
		"/im/pidgin/Pidgin3/Prefs/network.ui"
	);

	gtk_widget_class_bind_template_child(
			widget_class, PidginNetworkPage, stun_server);
	gtk_widget_class_bind_template_child(
			widget_class, PidginNetworkPage, auto_ip);
	gtk_widget_class_bind_template_child(
			widget_class, PidginNetworkPage, public_ip);
	gtk_widget_class_bind_template_child(
			widget_class, PidginNetworkPage, public_ip_hbox);
	gtk_widget_class_bind_template_child(
			widget_class, PidginNetworkPage, map_ports);
	gtk_widget_class_bind_template_child(
			widget_class, PidginNetworkPage, ports_range_use);
	gtk_widget_class_bind_template_child(
			widget_class, PidginNetworkPage, ports_range_hbox);
	gtk_widget_class_bind_template_child(
			widget_class, PidginNetworkPage, ports_range_start);
	gtk_widget_class_bind_template_child(
			widget_class, PidginNetworkPage, ports_range_end);
	gtk_widget_class_bind_template_child(
			widget_class, PidginNetworkPage, turn_server);
	gtk_widget_class_bind_template_child(
			widget_class, PidginNetworkPage, turn_port_udp);
	gtk_widget_class_bind_template_child(
			widget_class, PidginNetworkPage, turn_port_tcp);
	gtk_widget_class_bind_template_child(
			widget_class, PidginNetworkPage, turn_username);
	gtk_widget_class_bind_template_child(
			widget_class, PidginNetworkPage, turn_password);
	gtk_widget_class_bind_template_callback(widget_class,
			network_stun_server_changed_cb);
	gtk_widget_class_bind_template_callback(widget_class,
	                 auto_ip_button_clicked_cb);
	gtk_widget_class_bind_template_callback(widget_class,
			network_ip_changed);
	gtk_widget_class_bind_template_callback(widget_class,
			network_turn_server_changed_cb);
}

static void
pidgin_network_page_init(PidginNetworkPage *page)
{
	GtkStyleContext *context;
	GtkCssProvider *ip_css;

	gtk_widget_init_template(GTK_WIDGET(page));

	gtk_entry_set_text(GTK_ENTRY(page->stun_server),
			purple_prefs_get_string("/purple/network/stun_server"));

	pidgin_prefs_bind_checkbox("/purple/network/auto_ip", page->auto_ip);
	auto_ip_button_clicked_cb(page->auto_ip, NULL); /* Update label */

	gtk_entry_set_text(GTK_ENTRY(page->public_ip),
			purple_network_get_public_ip());

	ip_css = gtk_css_provider_new();
	gtk_css_provider_load_from_resource(ip_css,
	                                    "/im/pidgin/Pidgin3/Prefs/ip.css");

	context = gtk_widget_get_style_context(page->public_ip);
	gtk_style_context_add_provider(context,
	                               GTK_STYLE_PROVIDER(ip_css),
	                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	g_object_bind_property(page->auto_ip, "active",
			page->public_ip_hbox, "sensitive",
			G_BINDING_SYNC_CREATE|G_BINDING_INVERT_BOOLEAN);

	pidgin_prefs_bind_checkbox("/purple/network/map_ports",
			page->map_ports);

	pidgin_prefs_bind_checkbox("/purple/network/ports_range_use",
			page->ports_range_use);
	g_object_bind_property(page->ports_range_use, "active",
			page->ports_range_hbox, "sensitive",
			G_BINDING_SYNC_CREATE);

	pidgin_prefs_bind_spin_button("/purple/network/ports_range_start",
			page->ports_range_start);
	pidgin_prefs_bind_spin_button("/purple/network/ports_range_end",
			page->ports_range_end);

	/* TURN server */
	gtk_entry_set_text(GTK_ENTRY(page->turn_server),
			purple_prefs_get_string("/purple/network/turn_server"));

	pidgin_prefs_bind_spin_button("/purple/network/turn_port",
			page->turn_port_udp);

	pidgin_prefs_bind_spin_button("/purple/network/turn_port_tcp",
			page->turn_port_tcp);

	pidgin_prefs_bind_entry("/purple/network/turn_username",
			page->turn_username);
	pidgin_prefs_bind_entry("/purple/network/turn_password",
			page->turn_password);
}

/******************************************************************************
 * API
 *****************************************************************************/
GtkWidget *
pidgin_network_page_new(void) {
	return GTK_WIDGET(g_object_new(PIDGIN_TYPE_NETWORK_PAGE, NULL));
}
