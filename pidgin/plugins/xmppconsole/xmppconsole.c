/*
 * Purple - XMPP debugging tool
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

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <purple.h>
#include <pidgin.h>

#include "xmppconsole.h"

#define PLUGIN_ID      "gtk-xmpp"
#define PLUGIN_DOMAIN  (g_quark_from_static_string(PLUGIN_ID))

struct _PidginXmppConsole {
	GtkWindow parent;

	PurpleConnection *gc;
	GtkTextBuffer *buffer;
	struct {
		GtkTextTag *info;
		GtkTextTag *incoming;
		GtkTextTag *outgoing;
		GtkTextTag *bracket;
		GtkTextTag *tag;
		GtkTextTag *attr;
		GtkTextTag *value;
		GtkTextTag *xmlns;
	} tags;
	GtkWidget *entry;
	GtkTextBuffer *entry_buffer;
	GtkWidget *sw;

	struct {
		GtkPopover *popover;
		GtkEntry *to;
		GtkComboBoxText *type;
	} iq;

	struct {
		GtkPopover *popover;
		GtkEntry *to;
		GtkComboBoxText *type;
		GtkComboBoxText *show;
		GtkEntry *status;
		GtkEntry *priority;
	} presence;

	struct {
		GtkPopover *popover;
		GtkEntry *to;
		GtkComboBoxText *type;
		GtkEntry *body;
		GtkEntry *subject;
		GtkEntry *thread;
	} message;
};

G_DEFINE_DYNAMIC_TYPE(PidginXmppConsole, pidgin_xmpp_console, GTK_TYPE_WINDOW)

static PidginXmppConsole *console = NULL;

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
xmppconsole_append_xmlnode(PidginXmppConsole *console, PurpleXmlNode *node,
                           gint indent_level, GtkTextIter *iter,
                           GtkTextTag *tag)
{
	PurpleXmlNode *c;
	gboolean need_end = FALSE, pretty = TRUE;
	gint i;

	g_return_if_fail(node != NULL);

	for (i = 0; i < indent_level; i++) {
		gtk_text_buffer_insert_with_tags(console->buffer, iter, "\t", 1, tag, NULL);
	}

	gtk_text_buffer_insert_with_tags(console->buffer, iter, "<", 1,
	                                 tag, console->tags.bracket, NULL);
	gtk_text_buffer_insert_with_tags(console->buffer, iter, node->name, -1,
	                                 tag, console->tags.tag, NULL);

	if (node->xmlns) {
		if ((!node->parent ||
		     !node->parent->xmlns ||
		     !purple_strequal(node->xmlns, node->parent->xmlns)) &&
		    !purple_strequal(node->xmlns, "jabber:client"))
		{
			gtk_text_buffer_insert_with_tags(console->buffer, iter, " ", 1,
			                                 tag, NULL);
			gtk_text_buffer_insert_with_tags(console->buffer, iter, "xmlns", 5,
			                                 tag, console->tags.attr, NULL);
			gtk_text_buffer_insert_with_tags(console->buffer, iter, "='", 2,
			                                 tag, NULL);
			gtk_text_buffer_insert_with_tags(console->buffer, iter, node->xmlns, -1,
			                                 tag, console->tags.xmlns, NULL);
			gtk_text_buffer_insert_with_tags(console->buffer, iter, "'", 1,
			                                 tag, NULL);
		}
	}
	for (c = node->child; c; c = c->next)
	{
		if (c->type == PURPLE_XMLNODE_TYPE_ATTRIB) {
			gtk_text_buffer_insert_with_tags(console->buffer, iter, " ", 1,
			                                 tag, NULL);
			gtk_text_buffer_insert_with_tags(console->buffer, iter, c->name, -1,
			                                 tag, console->tags.attr, NULL);
			gtk_text_buffer_insert_with_tags(console->buffer, iter, "='", 2,
			                                 tag, NULL);
			gtk_text_buffer_insert_with_tags(console->buffer, iter, c->data, -1,
			                                 tag, console->tags.value, NULL);
			gtk_text_buffer_insert_with_tags(console->buffer, iter, "'", 1,
			                                 tag, NULL);
		} else if (c->type == PURPLE_XMLNODE_TYPE_TAG || c->type == PURPLE_XMLNODE_TYPE_DATA) {
			if (c->type == PURPLE_XMLNODE_TYPE_DATA)
				pretty = FALSE;
			need_end = TRUE;
		}
	}

	if (need_end) {
		gtk_text_buffer_insert_with_tags(console->buffer, iter, ">", 1,
		                                 tag, console->tags.bracket, NULL);
		if (pretty) {
			gtk_text_buffer_insert_with_tags(console->buffer, iter, "\n", 1,
			                                 tag, NULL);
		}

		for (c = node->child; c; c = c->next)
		{
			if (c->type == PURPLE_XMLNODE_TYPE_TAG) {
				xmppconsole_append_xmlnode(console, c, indent_level + 1, iter, tag);
			} else if (c->type == PURPLE_XMLNODE_TYPE_DATA && c->data_sz > 0) {
				gtk_text_buffer_insert_with_tags(console->buffer, iter, c->data, c->data_sz,
				                                 tag, NULL);
			}
		}

		if (pretty) {
			for (i = 0; i < indent_level; i++) {
				gtk_text_buffer_insert_with_tags(console->buffer, iter, "\t", 1, tag, NULL);
			}
		}
		gtk_text_buffer_insert_with_tags(console->buffer, iter, "<", 1,
		                                 tag, console->tags.bracket, NULL);
		gtk_text_buffer_insert_with_tags(console->buffer, iter, "/", 1,
		                                 tag, NULL);
		gtk_text_buffer_insert_with_tags(console->buffer, iter, node->name, -1,
		                                 tag, console->tags.tag, NULL);
		gtk_text_buffer_insert_with_tags(console->buffer, iter, ">", 1,
		                                 tag, console->tags.bracket, NULL);
		gtk_text_buffer_insert_with_tags(console->buffer, iter, "\n", 1,
		                                 tag, NULL);
	} else {
		gtk_text_buffer_insert_with_tags(console->buffer, iter, "/", 1,
		                                 tag, NULL);
		gtk_text_buffer_insert_with_tags(console->buffer, iter, ">", 1,
		                                 tag, console->tags.bracket, NULL);
		gtk_text_buffer_insert_with_tags(console->buffer, iter, "\n", 1,
		                                 tag, NULL);
	}
}

static void
purple_xmlnode_received_cb(PurpleConnection *gc, PurpleXmlNode **packet, gpointer null)
{
	GtkTextIter iter;

	if (console == NULL || console->gc != gc) {
		return;
	}

	gtk_text_buffer_get_end_iter(console->buffer, &iter);
	xmppconsole_append_xmlnode(console, *packet, 0, &iter,
	                           console->tags.incoming);
}

static void
purple_xmlnode_sent_cb(PurpleConnection *gc, char **packet, gpointer null)
{
	GtkTextIter iter;
	PurpleXmlNode *node;

	if (console == NULL || console->gc != gc) {
		return;
	}
	node = purple_xmlnode_from_str(*packet, -1);

	if (!node)
		return;

	gtk_text_buffer_get_end_iter(console->buffer, &iter);
	xmppconsole_append_xmlnode(console, node, 0, &iter,
	                           console->tags.outgoing);
	purple_xmlnode_free(node);
}

static gboolean
message_send_cb(GtkWidget *widget, GdkEventKey *event, gpointer data) {
	PidginXmppConsole *console = data;
	PurpleProtocol *protocol = NULL;
	PurpleConnection *gc;
	gchar *text;
	GtkTextIter start, end;

	if (event->keyval != GDK_KEY_KP_Enter && event->keyval != GDK_KEY_Return)
		return FALSE;

	gc = console->gc;

	if (gc)
		protocol = purple_connection_get_protocol(gc);

	gtk_text_buffer_get_bounds(console->entry_buffer, &start, &end);
	text = gtk_text_buffer_get_text(console->entry_buffer, &start, &end, FALSE);

	if(PURPLE_IS_PROTOCOL_SERVER(protocol)) {
		purple_protocol_server_send_raw(PURPLE_PROTOCOL_SERVER(protocol), gc,
		                                text, strlen(text));
	}

	g_free(text);
	gtk_text_buffer_set_text(console->entry_buffer, "", 0);

	return TRUE;
}

static void
entry_changed_cb(GtkTextBuffer *buffer, gpointer data) {
	PidginXmppConsole *console = data;
	GtkTextIter start, end;
	char *xmlstr, *str;
	GtkTextIter iter;
	int wrapped_lines;
	int lines;
	GdkRectangle oneline;
	int height;
	int pad_top, pad_inside, pad_bottom;
	PurpleXmlNode *node;
	GtkStyleContext *style;

	wrapped_lines = 1;
	gtk_text_buffer_get_start_iter(buffer, &iter);
	gtk_text_view_get_iter_location(GTK_TEXT_VIEW(console->entry), &iter, &oneline);
	while (gtk_text_view_forward_display_line(GTK_TEXT_VIEW(console->entry),
	                                          &iter)) {
		wrapped_lines++;
	}

	lines = gtk_text_buffer_get_line_count(buffer);

	/* Show a maximum of 64 lines */
	lines = MIN(lines, 6);
	wrapped_lines = MIN(wrapped_lines, 6);

	pad_top = gtk_text_view_get_pixels_above_lines(GTK_TEXT_VIEW(console->entry));
	pad_bottom = gtk_text_view_get_pixels_below_lines(GTK_TEXT_VIEW(console->entry));
	pad_inside = gtk_text_view_get_pixels_inside_wrap(GTK_TEXT_VIEW(console->entry));

	height = (oneline.height + pad_top + pad_bottom) * lines;
	height += (oneline.height + pad_inside) * (wrapped_lines - lines);

	gtk_widget_set_size_request(console->sw, -1, height + 6);

	gtk_text_buffer_get_bounds(buffer, &start, &end);
	str = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
	if (!str) {
		return;
	}

	xmlstr = g_strdup_printf("<xml>%s</xml>", str);
	node = purple_xmlnode_from_str(xmlstr, -1);
	style = gtk_widget_get_style_context(console->entry);
	if (node) {
		gtk_style_context_remove_class(style, GTK_STYLE_CLASS_ERROR);
	} else {
		gtk_style_context_add_class(style, GTK_STYLE_CLASS_ERROR);
	}
	g_free(str);
	g_free(xmlstr);
	if (node)
		purple_xmlnode_free(node);
}

static void
load_text_and_set_caret(PidginXmppConsole *console, const gchar *pre_text,
                        const gchar *post_text)
{
	GtkTextIter where;
	GtkTextMark *mark;

	gtk_text_buffer_begin_user_action(console->entry_buffer);

	gtk_text_buffer_set_text(console->entry_buffer, pre_text, -1);

	gtk_text_buffer_get_end_iter(console->entry_buffer, &where);
	mark = gtk_text_buffer_create_mark(console->entry_buffer, NULL, &where, TRUE);

	gtk_text_buffer_insert(console->entry_buffer, &where, post_text, -1);

	gtk_text_buffer_get_iter_at_mark(console->entry_buffer, &where, mark);
	gtk_text_buffer_place_cursor(console->entry_buffer, &where);
	gtk_text_buffer_delete_mark(console->entry_buffer, mark);

	gtk_text_buffer_end_user_action(console->entry_buffer);
}

static void
iq_clicked_cb(GtkWidget *w, gpointer data)
{
	PidginXmppConsole *console = data;
	const gchar *to;
	char *stanza;

	to = gtk_entry_get_text(console->iq.to);
	stanza = g_strdup_printf(
	        "<iq %s%s%s id='console%x' type='%s'>", to && *to ? "to='" : "",
	        to && *to ? to : "", to && *to ? "'" : "", g_random_int(),
	        gtk_combo_box_text_get_active_text(console->iq.type));
	load_text_and_set_caret(console, stanza, "</iq>");
	gtk_widget_grab_focus(console->entry);
	g_free(stanza);

	/* Reset everything. */
	gtk_entry_set_text(console->iq.to, "");
	gtk_combo_box_set_active(GTK_COMBO_BOX(console->iq.type), 0);
	gtk_popover_popdown(console->iq.popover);
}

static void
presence_clicked_cb(GtkWidget *w, gpointer data)
{
	PidginXmppConsole *console = data;
	const gchar *to, *status, *priority;
	gchar *type, *show;
	char *stanza;

	to = gtk_entry_get_text(console->presence.to);
	type = gtk_combo_box_text_get_active_text(console->presence.type);
	if (purple_strequal(type, "default")) {
		g_free(type);
		type = g_strdup("");
	}
	show = gtk_combo_box_text_get_active_text(console->presence.show);
	if (purple_strequal(show, "default")) {
		g_free(show);
		show = g_strdup("");
	}
	status = gtk_entry_get_text(console->presence.status);
	priority = gtk_entry_get_text(console->presence.priority);
	if (purple_strequal(priority, "0"))
		priority = "";

	stanza = g_strdup_printf("<presence %s%s%s id='console%x' %s%s%s>"
	                         "%s%s%s%s%s%s%s%s%s",
	                         *to ? "to='" : "",
	                         *to ? to : "",
	                         *to ? "'" : "",
	                         g_random_int(),

	                         *type ? "type='" : "",
	                         *type ? type : "",
	                         *type ? "'" : "",

	                         *show ? "<show>" : "",
	                         *show ? show : "",
	                         *show ? "</show>" : "",

	                         *status ? "<status>" : "",
	                         *status ? status : "",
	                         *status ? "</status>" : "",

	                         *priority ? "<priority>" : "",
	                         *priority ? priority : "",
	                         *priority ? "</priority>" : "");

	load_text_and_set_caret(console, stanza, "</presence>");
	gtk_widget_grab_focus(console->entry);
	g_free(stanza);
	g_free(type);
	g_free(show);

	/* Reset everything. */
	gtk_entry_set_text(console->presence.to, "");
	gtk_combo_box_set_active(GTK_COMBO_BOX(console->presence.type), 0);
	gtk_combo_box_set_active(GTK_COMBO_BOX(console->presence.show), 0);
	gtk_entry_set_text(console->presence.status, "");
	gtk_entry_set_text(console->presence.priority, "0");
	gtk_popover_popdown(console->presence.popover);
}

static void
message_clicked_cb(GtkWidget *w, gpointer data)
{
	PidginXmppConsole *console = data;
	const gchar *to, *body, *thread, *subject;
	char *stanza;

	to = gtk_entry_get_text(console->message.to);
	body = gtk_entry_get_text(console->message.body);
	thread = gtk_entry_get_text(console->message.thread);
	subject = gtk_entry_get_text(console->message.subject);

	stanza = g_strdup_printf(
	        "<message %s%s%s id='console%x' type='%s'>"
	        "%s%s%s%s%s%s%s%s%s",

	        *to ? "to='" : "", *to ? to : "", *to ? "'" : "",
	        g_random_int(),
	        gtk_combo_box_text_get_active_text(console->message.type),

	        *body ? "<body>" : "", *body ? body : "",
	        *body ? "</body>" : "",

	        *subject ? "<subject>" : "", *subject ? subject : "",
	        *subject ? "</subject>" : "",

	        *thread ? "<thread>" : "", *thread ? thread : "",
	        *thread ? "</thread>" : "");

	load_text_and_set_caret(console, stanza, "</message>");
	gtk_widget_grab_focus(console->entry);
	g_free(stanza);

	/* Reset everything. */
	gtk_entry_set_text(console->message.to, "");
	gtk_combo_box_set_active(GTK_COMBO_BOX(console->message.type), 0);
	gtk_entry_set_text(console->message.body, "");
	gtk_entry_set_text(console->message.subject, "0");
	gtk_entry_set_text(console->message.thread, "0");
	gtk_popover_popdown(console->message.popover);
}

static void
dropdown_changed_cb(GtkComboBox *widget, gpointer data) {
	PidginXmppConsole *console = data;
	PidginAccountChooser *chooser = PIDGIN_ACCOUNT_CHOOSER(widget);
	PurpleAccount *account = NULL;

	account = pidgin_account_chooser_get_selected(chooser);
	if(PURPLE_IS_ACCOUNT(account)) {
		console->gc = purple_account_get_connection(account);
		gtk_text_buffer_set_text(console->buffer, "", 0);
	} else {
		GtkTextIter start, end;
		console->gc = NULL;
		gtk_text_buffer_set_text(console->buffer, _("Not connected to XMPP"), -1);
		gtk_text_buffer_get_bounds(console->buffer, &start, &end);
		gtk_text_buffer_apply_tag(console->buffer, console->tags.info, &start, &end);
	}
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
static void
pidgin_xmpp_console_class_finalize(PidginXmppConsoleClass *klass) {
}

static void
pidgin_xmpp_console_class_init(PidginXmppConsoleClass *klass) {
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(
	        widget_class,
	        "/im/pidgin/Pidgin3/Plugin/XMPPConsole/console.ui"
	);

	gtk_widget_class_bind_template_callback(widget_class, dropdown_changed_cb);

	gtk_widget_class_bind_template_child(widget_class, PidginXmppConsole,
	                                     buffer);
	gtk_widget_class_bind_template_child(widget_class, PidginXmppConsole,
	                                     tags.info);
	gtk_widget_class_bind_template_child(widget_class, PidginXmppConsole,
	                                     tags.incoming);
	gtk_widget_class_bind_template_child(widget_class, PidginXmppConsole,
	                                     tags.outgoing);
	gtk_widget_class_bind_template_child(widget_class, PidginXmppConsole,
	                                     tags.bracket);
	gtk_widget_class_bind_template_child(widget_class, PidginXmppConsole,
	                                     tags.tag);
	gtk_widget_class_bind_template_child(widget_class, PidginXmppConsole,
	                                     tags.attr);
	gtk_widget_class_bind_template_child(widget_class, PidginXmppConsole,
	                                     tags.value);
	gtk_widget_class_bind_template_child(widget_class, PidginXmppConsole,
	                                     tags.xmlns);

	/* Popover for <iq/> button. */
	gtk_widget_class_bind_template_child(widget_class, PidginXmppConsole,
	                                     iq.popover);
	gtk_widget_class_bind_template_child(widget_class, PidginXmppConsole,
	                                     iq.to);
	gtk_widget_class_bind_template_child(widget_class, PidginXmppConsole,
	                                     iq.type);
	gtk_widget_class_bind_template_callback(widget_class, iq_clicked_cb);

	/* Popover for <presence/> button. */
	gtk_widget_class_bind_template_child(widget_class, PidginXmppConsole,
	                                     presence.popover);
	gtk_widget_class_bind_template_child(widget_class, PidginXmppConsole,
	                                     presence.to);
	gtk_widget_class_bind_template_child(widget_class, PidginXmppConsole,
	                                     presence.type);
	gtk_widget_class_bind_template_child(widget_class, PidginXmppConsole,
	                                     presence.show);
	gtk_widget_class_bind_template_child(widget_class, PidginXmppConsole,
	                                     presence.status);
	gtk_widget_class_bind_template_child(widget_class, PidginXmppConsole,
	                                     presence.priority);
	gtk_widget_class_bind_template_callback(widget_class, presence_clicked_cb);

	/* Popover for <message/> button. */
	gtk_widget_class_bind_template_child(widget_class, PidginXmppConsole,
	                                     message.popover);
	gtk_widget_class_bind_template_child(widget_class, PidginXmppConsole,
	                                     message.to);
	gtk_widget_class_bind_template_child(widget_class, PidginXmppConsole,
	                                     message.type);
	gtk_widget_class_bind_template_child(widget_class, PidginXmppConsole,
	                                     message.body);
	gtk_widget_class_bind_template_child(widget_class, PidginXmppConsole,
	                                     message.subject);
	gtk_widget_class_bind_template_child(widget_class, PidginXmppConsole,
	                                     message.thread);
	gtk_widget_class_bind_template_callback(widget_class, message_clicked_cb);

	gtk_widget_class_bind_template_child(widget_class, PidginXmppConsole,
	                                     entry);
	gtk_widget_class_bind_template_child(widget_class, PidginXmppConsole,
	                                     entry_buffer);
	gtk_widget_class_bind_template_callback(widget_class, message_send_cb);

	gtk_widget_class_bind_template_child(widget_class, PidginXmppConsole, sw);
	gtk_widget_class_bind_template_callback(widget_class, entry_changed_cb);
}

static void
pidgin_xmpp_console_init(PidginXmppConsole *console) {
	GtkCssProvider *entry_css;
	GtkStyleContext *context;

	gtk_widget_init_template(GTK_WIDGET(console));

	entry_css = gtk_css_provider_new();
	gtk_css_provider_load_from_data(entry_css,
	                                "textview." GTK_STYLE_CLASS_ERROR " text {background-color:#ffcece;}",
	                                -1, NULL);
	context = gtk_widget_get_style_context(console->entry);
	gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(entry_css),
	                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	entry_changed_cb(console->entry_buffer, console);

	gtk_widget_show(GTK_WIDGET(console));
}

/******************************************************************************
 * Plugin implementation
 *****************************************************************************/
static void
create_console(PurplePluginAction *action)
{
	if (console == NULL) {
		console = g_object_new(PIDGIN_TYPE_XMPP_CONSOLE, NULL);
		g_object_add_weak_pointer(G_OBJECT(console), (gpointer)&console);
	}

	gtk_window_present(GTK_WINDOW(console));
}

static GList *
actions(PurplePlugin *plugin)
{
	GList *l = NULL;
	PurplePluginAction *act = NULL;

	act = purple_plugin_action_new(_("XMPP Console"), create_console);
	l = g_list_append(l, act);

	return l;
}

static GPluginPluginInfo *
xmpp_console_query(GError **error)
{
	const gchar * const authors[] = {
		"Sean Egan <seanegan@gmail.com>",
		NULL
	};

	return pidgin_plugin_info_new(
		"id",           PLUGIN_ID,
		"name",         N_("XMPP Console"),
		"version",      DISPLAY_VERSION,
		"category",     N_("Protocol utility"),
		"summary",      N_("Send and receive raw XMPP stanzas."),
		"description",  N_("This plugin is useful for debugging XMPP servers "
		                   "or clients."),
		"authors",      authors,
		"website",      PURPLE_WEBSITE,
		"abi-version",  PURPLE_ABI_VERSION,
		"actions-cb",   actions,
		NULL
	);
}

static gboolean
xmpp_console_load(GPluginPlugin *plugin, GError **error)
{
	PurpleProtocolManager *manager = NULL;
	PurpleProtocol *xmpp = NULL;

	pidgin_xmpp_console_register_type(G_TYPE_MODULE(plugin));

	manager = purple_protocol_manager_get_default();
	xmpp = purple_protocol_manager_find(manager, "prpl-jabber");
	if (!PURPLE_IS_PROTOCOL(xmpp)) {
		g_set_error_literal(error, PLUGIN_DOMAIN, 0,
		                    _("No XMPP protocol is loaded."));
		return FALSE;
	}

	purple_signal_connect(xmpp, "jabber-receiving-xmlnode", plugin,
	                      G_CALLBACK(purple_xmlnode_received_cb), NULL);
	purple_signal_connect(xmpp, "jabber-sending-text", plugin,
	                      G_CALLBACK(purple_xmlnode_sent_cb), NULL);

	return TRUE;
}

static gboolean
xmpp_console_unload(GPluginPlugin *plugin, gboolean shutdown, GError **error)
{
	if (console) {
		gtk_widget_destroy(GTK_WIDGET(console));
	}
	return TRUE;
}

GPLUGIN_NATIVE_PLUGIN_DECLARE(xmpp_console)
