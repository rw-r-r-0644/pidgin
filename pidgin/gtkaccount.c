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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <glib/gi18n-lib.h>

#include <purple.h>
#include "libpurple/glibcompat.h"

#include "gtkaccount.h"
#include "gtkblist.h"
#include "gtkdialogs.h"
#include "gtkutils.h"
#include "pidgincore.h"
#include "pidgindialog.h"
#include "minidialog.h"
#include "pidginprotocolchooser.h"
#include "pidginproxyoptions.h"

enum {
	RESPONSE_ADD = 0,
	RESPONSE_CLOSE,
};

typedef struct
{
	PurpleAccount *account;
	char *username;
	char *alias;

} PidginAccountAddUserData;

typedef struct
{
	GtkWidget *widget;
	gchar *setting;
	PurplePrefType type;
} ProtocolOptEntry;

typedef struct
{
	PidginAccountDialogType type;

	PurpleAccount *account;
	char *protocol_id;
	PurpleProtocol *protocol;

	GList *user_split_entries;
	GList *protocol_opt_entries;

	GtkSizeGroup *sg;
	GtkWidget *window;

	GtkWidget *notebook;
	GtkWidget *top_vbox;
	GtkWidget *ok_button;
	GtkWidget *register_button;

	/* Login Options */
	GtkWidget *login_frame;
	GtkWidget *protocol_menu;
	gchar *password;
	GtkWidget *username_entry;
	GdkRGBA username_entry_hint_color;
	GtkWidget *alias_entry;

	/* User Options */
	GtkWidget *user_frame;
	GtkWidget *icon_hbox;
	GtkWidget *icon_check;
	GtkWidget *icon_entry;
	GtkFileChooserNative *icon_filesel;
	GtkWidget *icon_preview;
	GtkWidget *icon_text;
	PurpleImage *icon_img;

	/* Protocol Options */
	GtkWidget *protocol_frame;

	GtkWidget *proxy_options;

	/* Voice & Video Options*/
	GtkWidget *voice_frame;
	GtkWidget *suppression_check;

} AccountPrefsDialog;

typedef struct {
	PurpleAccount *account;
	PidginAccountDialogType type;
} PidginAccountDialogShowData;

/**************************************************************************
 * Add/Modify Account dialog
 **************************************************************************/
static void add_login_options(AccountPrefsDialog *dialog, GtkWidget *parent);
static void add_user_options(AccountPrefsDialog *dialog, GtkWidget *parent);
static void add_account_options(AccountPrefsDialog *dialog);
static void add_voice_options(AccountPrefsDialog *dialog);

static GtkWidget *
add_pref_box(AccountPrefsDialog *dialog, GtkWidget *parent,
			 const char *text, GtkWidget *widget)
{
	return pidgin_add_widget_to_vbox(GTK_BOX(parent), text, dialog->sg, widget, TRUE, NULL);
}

static void
set_dialog_icon(AccountPrefsDialog *dialog, gpointer data, size_t len, gchar *new_icon_path)
{
	GdkPixbuf *pixbuf = NULL;
	PurpleBuddyIconSpec *icon_spec = NULL;

	if (dialog->icon_img) {
		g_object_unref(dialog->icon_img);
		dialog->icon_img = NULL;
	}

	if (new_icon_path != NULL) {
		dialog->icon_img = purple_image_new_from_file(new_icon_path, NULL);
		purple_debug_warning("gtkaccount", "data was not necessary");
		g_free(data);
	} else if (data != NULL) {
		if (len > 0)
			dialog->icon_img = purple_image_new_take_data(data, len);
		else
			g_free(data);
	}

	if (dialog->icon_img != NULL) {
		pixbuf = purple_gdk_pixbuf_from_image(dialog->icon_img);
	}

	if (dialog->protocol)
		icon_spec = purple_protocol_get_icon_spec(dialog->protocol);

	if (pixbuf && icon_spec && (icon_spec->scale_rules & PURPLE_ICON_SCALE_DISPLAY))
	{
		/* Scale the icon to something reasonable */
		int width, height;
		GdkPixbuf *scale;

		pidgin_buddy_icon_get_scale_size(pixbuf, icon_spec,
				PURPLE_ICON_SCALE_DISPLAY, &width, &height);
		scale = gdk_pixbuf_scale_simple(pixbuf, width, height, GDK_INTERP_BILINEAR);

		g_object_unref(G_OBJECT(pixbuf));
		pixbuf = scale;
	}

	purple_buddy_icon_spec_free(icon_spec);

	if (pixbuf == NULL)
	{
		/* Show a placeholder icon */
		gtk_image_set_from_icon_name(GTK_IMAGE(dialog->icon_entry),
				"select-avatar", GTK_ICON_SIZE_LARGE_TOOLBAR);
	} else {
		gtk_image_set_from_pixbuf(GTK_IMAGE(dialog->icon_entry), pixbuf);
		g_object_unref(G_OBJECT(pixbuf));
	}
}

static void
set_account_protocol_cb(GtkWidget *widget, AccountPrefsDialog *dialog) {
	PidginProtocolChooser *chooser = PIDGIN_PROTOCOL_CHOOSER(widget);
	PurpleProtocol *protocol = pidgin_protocol_chooser_get_selected(chooser);

	if(g_set_object(&dialog->protocol, protocol)) {
		g_clear_pointer(&dialog->protocol_id, g_free);
	}
	g_object_unref(G_OBJECT(protocol));

	if(PURPLE_IS_PROTOCOL(dialog->protocol)) {
		dialog->protocol_id = g_strdup(purple_protocol_get_id(dialog->protocol));
	}

	if (dialog->account != NULL) {
		purple_account_clear_settings(dialog->account);
	}

	add_login_options(dialog, dialog->top_vbox);
	add_user_options(dialog, dialog->top_vbox);
	add_account_options(dialog);
	add_voice_options(dialog);

	gtk_widget_grab_focus(dialog->protocol_menu);

	if (!dialog->protocol ||
	    !PURPLE_PROTOCOL_IMPLEMENTS(dialog->protocol, SERVER, register_user))
	{
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->register_button), FALSE);
		gtk_widget_hide(dialog->register_button);
	} else {
		if (purple_protocol_get_options(dialog->protocol) &
		    OPT_PROTO_REGISTER_NOSCREENNAME) {
			gtk_widget_set_sensitive(dialog->register_button, TRUE);
		} else {
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
				dialog->register_button), FALSE);
			gtk_widget_set_sensitive(dialog->register_button, FALSE);
		}
		gtk_widget_show(dialog->register_button);
	}
}

static void
username_changed_cb(GtkEntry *entry, AccountPrefsDialog *dialog)
{
	gboolean opt_noscreenname = (dialog->protocol != NULL &&
		(purple_protocol_get_options(dialog->protocol) & OPT_PROTO_REGISTER_NOSCREENNAME));
	gboolean username_valid = purple_validate(dialog->protocol,
		gtk_entry_get_text(entry));

	if (dialog->ok_button) {
		if (opt_noscreenname && dialog->register_button &&
			gtk_toggle_button_get_active(
				GTK_TOGGLE_BUTTON(dialog->register_button)))
			gtk_widget_set_sensitive(dialog->ok_button, TRUE);
		else
			gtk_widget_set_sensitive(dialog->ok_button,
				username_valid);
	}

	if (dialog->register_button) {
		if (opt_noscreenname)
			gtk_widget_set_sensitive(dialog->register_button, TRUE);
		else
			gtk_widget_set_sensitive(dialog->register_button,
				username_valid);
	}
}

static void
register_button_cb(GtkWidget *checkbox, AccountPrefsDialog *dialog)
{
	int register_checked = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(dialog->register_button));
	int opt_noscreenname = (dialog->protocol != NULL &&
		(purple_protocol_get_options(dialog->protocol) & OPT_PROTO_REGISTER_NOSCREENNAME));
	int register_noscreenname = (opt_noscreenname && register_checked);

	if (register_noscreenname) {
		gtk_entry_set_text(GTK_ENTRY(dialog->username_entry), "");
	}
	gtk_widget_set_sensitive(dialog->username_entry, !register_noscreenname);

	if (dialog->ok_button) {
		gtk_widget_set_sensitive(dialog->ok_button,
			(opt_noscreenname && register_checked) ||
			*gtk_entry_get_text(GTK_ENTRY(dialog->username_entry))
				!= '\0');
	}
}

static void
icon_filesel_choose_cb(const char *filename, gpointer data)
{
	AccountPrefsDialog *dialog = data;

	if (filename != NULL)
	{
		size_t len = 0;
		gpointer data = pidgin_convert_buddy_icon(dialog->protocol, filename, &len);
		set_dialog_icon(dialog, data, len, g_strdup(filename));
	}

	g_clear_object(&dialog->icon_filesel);
}

static void
icon_select_cb(GtkWidget *button, AccountPrefsDialog *dialog)
{
	dialog->icon_filesel = pidgin_buddy_icon_chooser_new(GTK_WINDOW(dialog->window), icon_filesel_choose_cb, dialog);
	gtk_native_dialog_show(GTK_NATIVE_DIALOG(dialog->icon_filesel));
}

static void
icon_reset_cb(GtkWidget *button, AccountPrefsDialog *dialog)
{
	set_dialog_icon(dialog, NULL, 0, NULL);
}

static void
update_editable(PurpleConnection *gc, AccountPrefsDialog *dialog)
{
	GtkStyleContext *style;
	gboolean set;
	GList *l;

	if (dialog->account == NULL)
		return;

	if (gc != NULL && dialog->account != purple_connection_get_account(gc))
		return;

	set = !(purple_account_is_connected(dialog->account) || purple_account_is_connecting(dialog->account));
	gtk_widget_set_sensitive(dialog->protocol_menu, set);
	gtk_editable_set_editable(GTK_EDITABLE(dialog->username_entry), set);
	style = gtk_widget_get_style_context(dialog->username_entry);

	if (set) {
		gtk_style_context_remove_class(style, "copyable-insensitive");
	} else {
		gtk_style_context_add_class(style, "copyable-insensitive");
	}

	for (l = dialog->user_split_entries ; l != NULL ; l = l->next) {
		if (l->data == NULL)
			continue;
		if (GTK_IS_EDITABLE(l->data)) {
			gtk_editable_set_editable(GTK_EDITABLE(l->data), set);
			style = gtk_widget_get_style_context(GTK_WIDGET(l->data));
			if (set) {
				gtk_style_context_remove_class(style,
						"copyable-insensitive");
			} else {
				gtk_style_context_add_class(style,
						"copyable-insensitive");
			}
		} else {
			gtk_widget_set_sensitive(GTK_WIDGET(l->data), set);
		}
	}
}

static void
add_login_options(AccountPrefsDialog *dialog, GtkWidget *parent)
{
	GtkWidget *frame;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *entry;
	GList *user_splits;
	GList *l, *l2;
	char *username = NULL;
	GtkCssProvider *entry_css;
	const gchar *res = "/im/pidgin/Pidgin3/Accounts/entry.css";

	entry_css = gtk_css_provider_new();
	gtk_css_provider_load_from_resource(entry_css, res);

	if (dialog->protocol_menu != NULL)
	{
		g_object_ref(G_OBJECT(dialog->protocol_menu));
		hbox = g_object_get_data(G_OBJECT(dialog->protocol_menu), "container");
		gtk_container_remove(GTK_CONTAINER(hbox), dialog->protocol_menu);
	}

	if (dialog->login_frame != NULL)
		gtk_widget_destroy(dialog->login_frame);

	/* Build the login options frame. */
	frame = pidgin_make_frame(parent, _("Login Options"));

	/* cringe */
	dialog->login_frame = gtk_widget_get_parent(gtk_widget_get_parent(frame));

	gtk_box_reorder_child(GTK_BOX(parent), dialog->login_frame, 0);
	gtk_widget_show(dialog->login_frame);

	/* Main vbox */
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	/* Protocol */
	if(dialog->protocol_menu == NULL) {
		dialog->protocol_menu = pidgin_protocol_chooser_new();
		pidgin_protocol_chooser_set_selected_id(PIDGIN_PROTOCOL_CHOOSER(dialog->protocol_menu),
		                                        dialog->protocol_id);
		g_signal_connect(G_OBJECT(dialog->protocol_menu), "changed",
		                 G_CALLBACK(set_account_protocol_cb), dialog);
		gtk_widget_show(dialog->protocol_menu);
		g_object_ref(G_OBJECT(dialog->protocol_menu));
	}

	hbox = add_pref_box(dialog, vbox, _("Pro_tocol:"), dialog->protocol_menu);
	g_object_set_data(G_OBJECT(dialog->protocol_menu), "container", hbox);

	g_object_unref(G_OBJECT(dialog->protocol_menu));

	/* Username */
	dialog->username_entry = gtk_entry_new();
	gtk_style_context_add_provider(
			gtk_widget_get_style_context(dialog->username_entry),
			GTK_STYLE_PROVIDER(entry_css),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_set(G_OBJECT(dialog->username_entry), "truncate-multiline", TRUE, NULL);

	add_pref_box(dialog, vbox, _("_Username:"), dialog->username_entry);

	if (dialog->account != NULL)
		username = g_strdup(purple_account_get_username(dialog->account));

	if (!username && dialog->protocol
			&& PURPLE_PROTOCOL_IMPLEMENTS(dialog->protocol, CLIENT, get_account_text_table)) {
		GHashTable *table;
		const char *label;
		table = purple_protocol_client_get_account_text_table(PURPLE_PROTOCOL_CLIENT(dialog->protocol), NULL);
		label = g_hash_table_lookup(table, "login_label");

		gtk_entry_set_placeholder_text(GTK_ENTRY(dialog->username_entry), label);

		g_hash_table_destroy(table);
	}

	g_signal_connect(G_OBJECT(dialog->username_entry), "changed",
					 G_CALLBACK(username_changed_cb), dialog);

	/* Do the user split thang */
	if (dialog->protocol == NULL)
		user_splits = NULL;
	else
		user_splits = purple_protocol_get_user_splits(dialog->protocol);

	if (dialog->user_split_entries != NULL) {
		g_list_free(dialog->user_split_entries);
		dialog->user_split_entries = NULL;
	}

	for (l = user_splits; l != NULL; l = l->next) {
		PurpleAccountUserSplit *split = l->data;
		char *buf;

		if (purple_account_user_split_is_constant(split))
			entry = NULL;
		else {
			buf = g_strdup_printf("_%s:", purple_account_user_split_get_text(split));
			entry = gtk_entry_new();
			gtk_style_context_add_provider(
					gtk_widget_get_style_context(entry),
					GTK_STYLE_PROVIDER(entry_css),
					GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
			add_pref_box(dialog, vbox, buf, entry);
			g_free(buf);
		}

		dialog->user_split_entries =
			g_list_append(dialog->user_split_entries, entry);
	}

	for (l = g_list_last(dialog->user_split_entries),
		 l2 = g_list_last(user_splits);
		 l != NULL && l2 != NULL;
		 l = l->prev, l2 = l2->prev) {

		GtkWidget *entry = l->data;
		PurpleAccountUserSplit *split = l2->data;
		const char *value = NULL;
		char *c;

		if (dialog->account != NULL && username != NULL) {
			if(purple_account_user_split_get_reverse(split))
				c = strrchr(username,
						purple_account_user_split_get_separator(split));
			else
				c = strchr(username,
						purple_account_user_split_get_separator(split));

			if (c != NULL) {
				*c = '\0';
				c++;

				value = c;
			}
		}
		if (value == NULL)
			value = purple_account_user_split_get_default_value(split);

		if (value != NULL && entry != NULL)
			gtk_entry_set_text(GTK_ENTRY(entry), value);
	}

	g_list_free_full(user_splits,
	                 (GDestroyNotify)purple_account_user_split_destroy);

	if (username != NULL)
		gtk_entry_set_text(GTK_ENTRY(dialog->username_entry), username);

	g_free(username);

	/* Do not let the user change the protocol/username while connected. */
	update_editable(NULL, dialog);
	purple_signal_connect(purple_connections_get_handle(), "signing-on", dialog,
					G_CALLBACK(update_editable), dialog);
	purple_signal_connect(purple_connections_get_handle(), "signed-off", dialog,
					G_CALLBACK(update_editable), dialog);

	g_object_unref(entry_css);
}

static void
icon_check_cb(GtkWidget *checkbox, AccountPrefsDialog *dialog)
{
	gtk_widget_set_sensitive(dialog->icon_hbox, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->icon_check)));
}

static void
add_user_options(AccountPrefsDialog *dialog, GtkWidget *parent)
{
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *vbox2;
	GtkWidget *hbox;
	GtkWidget *hbox2;
	GtkWidget *button;
	GtkWidget *label;

	if (dialog->user_frame != NULL)
		gtk_widget_destroy(dialog->user_frame);

	/* Build the user options frame. */
	frame = pidgin_make_frame(parent, _("User Options"));
	dialog->user_frame = gtk_widget_get_parent(gtk_widget_get_parent(frame));

	gtk_box_reorder_child(GTK_BOX(parent), dialog->user_frame, 1);
	gtk_widget_show(dialog->user_frame);

	/* Main vbox */
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	/* Alias */
	dialog->alias_entry = gtk_entry_new();
	add_pref_box(dialog, vbox, _("_Local alias:"), dialog->alias_entry);

	/* Buddy icon */
	dialog->icon_check = gtk_check_button_new_with_mnemonic(_("Use this buddy _icon for this account:"));
	g_signal_connect(G_OBJECT(dialog->icon_check), "toggled", G_CALLBACK(icon_check_cb), dialog);
	gtk_widget_show(dialog->icon_check);
	gtk_box_pack_start(GTK_BOX(vbox), dialog->icon_check, FALSE, FALSE, 0);

	dialog->icon_hbox = hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_sensitive(hbox, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->icon_check)));
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(hbox);

	label = gtk_label_new("    ");
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	button = gtk_button_new();
	gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);
	g_signal_connect(G_OBJECT(button), "clicked",
	                 G_CALLBACK(icon_select_cb), dialog);

	dialog->icon_entry = gtk_image_new();
	gtk_container_add(GTK_CONTAINER(button), dialog->icon_entry);
	gtk_widget_show(dialog->icon_entry);
	/* TODO: Uh, isn't this next line pretty useless? */
	pidgin_set_accessible_label(dialog->icon_entry, GTK_LABEL(label));
	if (dialog->icon_img) {
		g_object_unref(dialog->icon_img);
		dialog->icon_img = NULL;
	}

	vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE, TRUE, 0);
	gtk_widget_show(vbox2);

	hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox2, FALSE, FALSE, 12);
	gtk_widget_show(hbox2);

	button = gtk_button_new_with_mnemonic(_("_Remove"));
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(icon_reset_cb), dialog);
	gtk_box_pack_start(GTK_BOX(hbox2), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

	if (dialog->protocol != NULL) {
		PurpleBuddyIconSpec *icon_spec = purple_protocol_get_icon_spec(dialog->protocol);

		if (!icon_spec || icon_spec->format == NULL) {
			gtk_widget_hide(dialog->icon_check);
			gtk_widget_hide(dialog->icon_hbox);
		}

		purple_buddy_icon_spec_free(icon_spec);
	}

	if (dialog->account != NULL) {
		PurpleImage *img;
		gpointer data = NULL;
		size_t len = 0;

		if (purple_account_get_private_alias(dialog->account))
			gtk_entry_set_text(GTK_ENTRY(dialog->alias_entry),
							   purple_account_get_private_alias(dialog->account));

		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->icon_check),
					     !purple_account_get_bool(dialog->account, "use-global-buddyicon",
								       TRUE));

		img = purple_buddy_icons_find_account_icon(dialog->account);
		if (img)
		{
			len = purple_image_get_data_size(img);
			data = g_memdup2(purple_image_get_data(img), len);
		}
		set_dialog_icon(dialog, data, len,
		                g_strdup(purple_account_get_buddy_icon_path(dialog->account)));
	} else {
		set_dialog_icon(dialog, NULL, 0, NULL);
	}

#if 0
	if (!dialog->protocol ||
			(!(purple_protocol_get_options(dialog->protocol) & OPT_PROTO_MAIL_CHECK) &&
			 (purple_protocol_get_icon_spec(dialog->protocol).format ==  NULL))) {

		/* Nothing to see :( aww. */
		gtk_widget_hide(dialog->user_frame);
	}
#endif
}

static void
protocol_opt_entry_free(ProtocolOptEntry *opt_entry)
{
	g_return_if_fail(opt_entry != NULL);

	g_free(opt_entry->setting);
	g_free(opt_entry);
}

static void
add_account_options(AccountPrefsDialog *dialog)
{
	PurpleAccountOption *option;
	PurpleAccount *account;
	GtkWidget *vbox, *check, *entry, *combo;
	GList *list, *node, *opts;
	gint i, idx, int_value;
	GtkListStore *model;
	GtkTreeIter iter;
	GtkCellRenderer *renderer;
	PurpleKeyValuePair *kvp;
	GList *l;
	char buf[1024];
	char *title, *tmp;
	const char *str_value;
	gboolean bool_value;
	ProtocolOptEntry *opt_entry;
	const GSList *str_hints;

	if (dialog->protocol_frame != NULL) {
		gtk_notebook_remove_page (GTK_NOTEBOOK(dialog->notebook), 1);
		dialog->protocol_frame = NULL;
	}

	g_list_free_full(dialog->protocol_opt_entries, (GDestroyNotify)protocol_opt_entry_free);
	dialog->protocol_opt_entries = NULL;

	if (dialog->protocol == NULL) {
		return;
	}

	opts = purple_protocol_get_account_options(dialog->protocol);
	if(opts == NULL) {
		return;
	}

	account = dialog->account;

	/* Main vbox */
	dialog->protocol_frame = vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
	gtk_notebook_insert_page(GTK_NOTEBOOK(dialog->notebook), vbox,
			gtk_label_new_with_mnemonic(_("Ad_vanced")), 1);
	gtk_widget_show(vbox);

	for (l = opts; l != NULL; l = l->next)
	{
		option = (PurpleAccountOption *)l->data;

		opt_entry = g_new0(ProtocolOptEntry, 1);
		opt_entry->type = purple_account_option_get_pref_type(option);
		opt_entry->setting = g_strdup(purple_account_option_get_setting(option));

		switch (opt_entry->type)
		{
			case PURPLE_PREF_BOOLEAN:
				if (account == NULL ||
					!purple_strequal(purple_account_get_protocol_id(account),
						   dialog->protocol_id))
				{
					bool_value = purple_account_option_get_default_bool(option);
				}
				else
				{
					bool_value = purple_account_get_bool(account,
						purple_account_option_get_setting(option),
						purple_account_option_get_default_bool(option));
				}

				tmp = g_strconcat("_", purple_account_option_get_text(option), NULL);
				opt_entry->widget = check = gtk_check_button_new_with_mnemonic(tmp);
				g_free(tmp);

				gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check),
											 bool_value);

				gtk_box_pack_start(GTK_BOX(vbox), check, FALSE, FALSE, 0);
				gtk_widget_show(check);
				break;

			case PURPLE_PREF_INT:
				if (account == NULL ||
					!purple_strequal(purple_account_get_protocol_id(account),
						   dialog->protocol_id))
				{
					int_value = purple_account_option_get_default_int(option);
				}
				else
				{
					int_value = purple_account_get_int(account,
						purple_account_option_get_setting(option),
						purple_account_option_get_default_int(option));
				}

				g_snprintf(buf, sizeof(buf), "%d", int_value);

				opt_entry->widget = entry = gtk_entry_new();
				gtk_entry_set_text(GTK_ENTRY(entry), buf);

				title = g_strdup_printf("_%s:",
						purple_account_option_get_text(option));
				add_pref_box(dialog, vbox, title, entry);
				g_free(title);
				break;

			case PURPLE_PREF_STRING:
				if (account == NULL ||
					!purple_strequal(purple_account_get_protocol_id(account),
						   dialog->protocol_id))
				{
					str_value = purple_account_option_get_default_string(option);
				}
				else
				{
					str_value = purple_account_get_string(account,
						purple_account_option_get_setting(option),
						purple_account_option_get_default_string(option));
				}

				str_hints = purple_account_option_string_get_hints(option);
				if (str_hints)
				{
					const GSList *hint_it = str_hints;
					entry = gtk_combo_box_text_new_with_entry();
					while (hint_it)
					{
						const gchar *hint = hint_it->data;
						hint_it = g_slist_next(hint_it);
						gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(entry),
						                               hint);
					}
				}
				else
					entry = gtk_entry_new();
				
				opt_entry->widget = entry;
				if (purple_account_option_string_get_masked(option) && str_hints)
					g_warn_if_reached();
				else if (purple_account_option_string_get_masked(option))
				{
					gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
				}

				if (str_value != NULL && str_hints)
					gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(entry))),
					                   str_value);
				else
					gtk_entry_set_text(GTK_ENTRY(entry), str_value ? str_value : "");

				title = g_strdup_printf("_%s:",
						purple_account_option_get_text(option));
				add_pref_box(dialog, vbox, title, entry);
				g_free(title);
				break;

			case PURPLE_PREF_STRING_LIST:
				i = 0;
				idx = 0;

				if (account == NULL ||
					!purple_strequal(purple_account_get_protocol_id(account),
						   dialog->protocol_id))
				{
					str_value = purple_account_option_get_default_list_value(option);
				}
				else
				{
					str_value = purple_account_get_string(account,
						purple_account_option_get_setting(option),
						purple_account_option_get_default_list_value(option));
				}

				list = purple_account_option_get_list(option);
				model = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_POINTER);
				opt_entry->widget = combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(model));

				/* Loop through list of PurpleKeyValuePair items */
				for (node = list; node != NULL; node = node->next) {
					if (node->data != NULL) {
						kvp = (PurpleKeyValuePair *) node->data;
						if ((kvp->value != NULL) && (str_value != NULL) &&
						    !g_utf8_collate(kvp->value, str_value))
							idx = i;

						gtk_list_store_append(model, &iter);
						gtk_list_store_set(model, &iter,
								0, kvp->key,
								1, kvp->value,
								-1);
					}

					i++;
				}

				/* Set default */
				gtk_combo_box_set_active(GTK_COMBO_BOX(combo), idx);

				/* Define renderer */
				renderer = gtk_cell_renderer_text_new();
				gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer,
						TRUE);
				gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo),
						renderer, "text", 0, NULL);

				title = g_strdup_printf("_%s:",
						purple_account_option_get_text(option));
				add_pref_box(dialog, vbox, title, combo);
				g_free(title);
				break;

			default:
				purple_debug_error("gtkaccount", "Invalid Account Option pref type (%d)\n",
						   opt_entry->type);
				g_free(opt_entry->setting);
				g_free(opt_entry);
				continue;
		}

		dialog->protocol_opt_entries =
			g_list_append(dialog->protocol_opt_entries, opt_entry);

	}
	g_list_free_full(opts, (GDestroyNotify)purple_account_option_destroy);
}

static void
add_voice_options(AccountPrefsDialog *dialog)
{
#ifdef USE_VV
	if (!dialog->protocol || !PURPLE_PROTOCOL_IMPLEMENTS(dialog->protocol, MEDIA, initiate_session)) {
		if (dialog->voice_frame) {
			gtk_widget_destroy(dialog->voice_frame);
			dialog->voice_frame = NULL;
			dialog->suppression_check = NULL;
		}
		return;
	}

	if (!dialog->voice_frame) {
		dialog->voice_frame = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
		gtk_container_set_border_width(GTK_CONTAINER(dialog->voice_frame), 12);

		dialog->suppression_check =
				gtk_check_button_new_with_mnemonic(_("Use _silence suppression"));
		gtk_box_pack_start(GTK_BOX(dialog->voice_frame), dialog->suppression_check,
				FALSE, FALSE, 0);

		gtk_notebook_append_page(GTK_NOTEBOOK(dialog->notebook),
				dialog->voice_frame, gtk_label_new_with_mnemonic(_("_Voice and Video")));
		gtk_widget_show_all(dialog->voice_frame);
	}

	if (dialog->account) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->suppression_check),
		                             purple_account_get_silence_suppression(dialog->account));
	} else {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->suppression_check), FALSE);
	}
#endif
}

static gboolean
account_win_destroy_cb(GtkWidget *w, GdkEvent *event,
					   AccountPrefsDialog *dialog)
{
	gtk_widget_destroy(dialog->window);

	g_list_free(dialog->user_split_entries);
	g_list_free_full(dialog->protocol_opt_entries, (GDestroyNotify)protocol_opt_entry_free);
	g_free(dialog->protocol_id);
	g_object_unref(dialog->sg);

	if (dialog->icon_img)
		g_object_unref(dialog->icon_img);

	g_clear_object(&dialog->icon_filesel);

	purple_signals_disconnect_by_handle(dialog);

	purple_str_wipe(dialog->password);

	g_free(dialog);
	return FALSE;
}

static void
account_register_cb(PurpleAccount *account, gboolean succeeded, void *user_data)
{
	if (succeeded)
	{
		const PurpleSavedStatus *saved_status = purple_savedstatus_get_current();
		purple_signal_emit(pidgin_accounts_get_handle(), "account-modified", account);

		if (saved_status != NULL && purple_account_get_remember_password(account)) {
			purple_savedstatus_activate_for_account(saved_status, account);
			purple_account_set_enabled(account, PIDGIN_UI, TRUE);
		}
	}
	else
		purple_accounts_delete(account);
}

static void
account_prefs_save(AccountPrefsDialog *dialog) {
	PurpleAccountManager *manager = NULL;
	PurpleProxyInfo *proxy_info = NULL;
	GList *l, *l2;
	const char *value;
	char *username;
	char *tmp;
	gboolean new_acct = FALSE, icon_change = FALSE;
	PurpleAccount *account;
	PurpleBuddyIconSpec *icon_spec = NULL;

	manager = purple_account_manager_get_default();

	/* Build the username string. */
	username = g_strdup(gtk_entry_get_text(GTK_ENTRY(dialog->username_entry)));

	if (dialog->protocol != NULL)
	{
		for (l = purple_protocol_get_user_splits(dialog->protocol),
			 l2 = dialog->user_split_entries;
			 l != NULL && l2 != NULL;
			 l = l->next, l2 = l2->next)
		{
			PurpleAccountUserSplit *split = l->data;
			GtkEntry *entry = l2->data;
			char sep[2] = " ";

			value = entry ? gtk_entry_get_text(entry) : "";
			if (!value)
				value = "";

			*sep = purple_account_user_split_get_separator(split);

			tmp = g_strconcat(username, sep,
					(*value ? value :
					 purple_account_user_split_get_default_value(split)),
					NULL);

			g_free(username);
			username = tmp;
		}
	}

	if (dialog->account == NULL)
	{
		account = purple_account_manager_find(manager, username,
		                                      dialog->protocol_id);
		if(PURPLE_IS_ACCOUNT(account)) {
			purple_debug_warning("gtkaccount",
			                     "Trying to add a duplicate %s account (%s).\n",
			                     dialog->protocol_id, username);

			purple_notify_error(NULL, NULL, _("Unable to save new account"),
			                    _("An account already exists with the "
			                      "specified criteria."),
			                    NULL);

			g_free(username);

			return;
		}

		account = purple_account_new(username, dialog->protocol_id);
		new_acct = TRUE;
	}
	else
	{
		account = dialog->account;

		/* Protocol */
		purple_account_set_protocol_id(account, dialog->protocol_id);
	}

	/* Alias */
	value = gtk_entry_get_text(GTK_ENTRY(dialog->alias_entry));

	if (*value != '\0')
		purple_account_set_private_alias(account, value);
	else
		purple_account_set_private_alias(account, NULL);

	/* Buddy Icon */
	if (dialog->protocol != NULL)
		icon_spec = purple_protocol_get_icon_spec(dialog->protocol);

	if (icon_spec && icon_spec->format != NULL)
	{
		const char *filename;

		if (new_acct || purple_account_get_bool(account, "use-global-buddyicon", TRUE) ==
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->icon_check)))
		{
			icon_change = TRUE;
		}
		purple_account_set_bool(account, "use-global-buddyicon", !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->icon_check)));

		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->icon_check)))
		{
			if (dialog->icon_img)
			{
				size_t len = purple_image_get_data_size(dialog->icon_img);
				purple_buddy_icons_set_account_icon(account,
					g_memdup2(purple_image_get_data(dialog->icon_img), len), len);
				purple_account_set_buddy_icon_path(account,
					purple_image_get_path(dialog->icon_img));
			}
			else
			{
				purple_buddy_icons_set_account_icon(account, NULL, 0);
				purple_account_set_buddy_icon_path(account, NULL);
			}
		}
		else if ((filename = purple_prefs_get_path(PIDGIN_PREFS_ROOT "/accounts/buddyicon")) && icon_change)
		{
			size_t len = 0;
			gpointer data = pidgin_convert_buddy_icon(dialog->protocol, filename, &len);
			purple_account_set_buddy_icon_path(account, filename);
			purple_buddy_icons_set_account_icon(account, data, len);
		}
	}

	purple_buddy_icon_spec_free(icon_spec);

	purple_account_set_username(account, username);
	g_free(username);

	/* Add the protocol settings */
	if (dialog->protocol) {
		ProtocolOptEntry *opt_entry;
		GtkTreeIter iter;
		char *value2;
		int int_value;
		gboolean bool_value;

		for (l2 = dialog->protocol_opt_entries; l2; l2 = l2->next) {

			opt_entry = l2->data;

			switch (opt_entry->type) {
				case PURPLE_PREF_STRING:
					if (GTK_IS_COMBO_BOX(opt_entry->widget))
						value = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(opt_entry->widget));
					else
						value = gtk_entry_get_text(GTK_ENTRY(opt_entry->widget));
					purple_account_set_string(account, opt_entry->setting, value);
					break;

				case PURPLE_PREF_INT:
					int_value = atoi(gtk_entry_get_text(GTK_ENTRY(opt_entry->widget)));
					purple_account_set_int(account, opt_entry->setting, int_value);
					break;

				case PURPLE_PREF_BOOLEAN:
					bool_value =
						gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(opt_entry->widget));
					purple_account_set_bool(account, opt_entry->setting, bool_value);
					break;

				case PURPLE_PREF_STRING_LIST:
					value2 = NULL;
					if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(opt_entry->widget), &iter))
						gtk_tree_model_get(gtk_combo_box_get_model(GTK_COMBO_BOX(opt_entry->widget)), &iter, 1, &value2, -1);
					purple_account_set_string(account, opt_entry->setting, value2);
					break;

				default:
					break;
			}
		}
	}

	proxy_info = pidgin_proxy_options_get_info(PIDGIN_PROXY_OPTIONS(dialog->proxy_options));
	purple_account_set_proxy_info(account, proxy_info);

	/* Voice and Video settings */
	if (dialog->voice_frame) {
		purple_account_set_silence_suppression(account,
				gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->suppression_check)));
	}

	/* If this is a new account, add it to our list */
	if(new_acct) {
		purple_account_manager_add(manager, account);
	} else {
		purple_signal_emit(pidgin_accounts_get_handle(), "account-modified", account);
	}

	/* If this is a new account, then sign on! */
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->register_button))) {
		purple_account_set_register_callback(account, account_register_cb, NULL);
		purple_account_register(account);
	} else if (new_acct) {
		const PurpleSavedStatus *saved_status;

		saved_status = purple_savedstatus_get_current();
		if (saved_status != NULL) {
			purple_savedstatus_activate_for_account(saved_status, account);
			purple_account_set_enabled(account, PIDGIN_UI, TRUE);
		}
	}

	/* We no longer need the data from the dialog window */
	account_win_destroy_cb(NULL, NULL, dialog);
}

static void
account_prefs_response_cb(GtkDialog *dialog, gint response_id, gpointer data) {
	AccountPrefsDialog *window = (AccountPrefsDialog *)data;

	switch(response_id) {
		case RESPONSE_ADD:
			account_prefs_save(window);
			break;
		case RESPONSE_CLOSE:
			account_win_destroy_cb(NULL, NULL, window);
			break;
		default:
			break;
	}
}

static void
pidgin_account_dialog_show_continue(PidginAccountDialogType type,
                                    PurpleAccount *account,
                                    const gchar *password)
{
	AccountPrefsDialog *dialog;
	GtkWidget *win;
	GtkWidget *main_vbox;
	GtkWidget *vbox;
	GtkWidget *notebook;
	GtkWidget *button;

	dialog = g_new0(AccountPrefsDialog, 1);

	if(PURPLE_IS_ACCOUNT(account)) {
		dialog->protocol_id = g_strdup(purple_account_get_protocol_id(account));
	}

	dialog->account = account;
	dialog->password = g_strdup(password);
	dialog->type = type;
	dialog->sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	if(dialog->protocol_id != NULL) {
		PurpleProtocolManager *manager = purple_protocol_manager_get_default();

		dialog->protocol = purple_protocol_manager_find(manager,
		                                                dialog->protocol_id);
	}

	dialog->window = win = pidgin_dialog_new((type == PIDGIN_ADD_ACCOUNT_DIALOG) ? _("Add Account") : _("Modify Account"),
		6, "account", FALSE);

	g_signal_connect(win, "delete_event", G_CALLBACK(account_win_destroy_cb),
	                 dialog);
	g_signal_connect(win, "response", G_CALLBACK(account_prefs_response_cb),
	                 dialog);

	/* Setup the vbox */
	main_vbox = gtk_dialog_get_content_area(GTK_DIALOG(win));
	gtk_box_set_spacing(GTK_BOX(main_vbox), 6);

	dialog->notebook = notebook = gtk_notebook_new();
	gtk_box_pack_start(GTK_BOX(main_vbox), notebook, FALSE, FALSE, 0);
	gtk_widget_show(GTK_WIDGET(notebook));

	/* Setup the inner vbox */
	dialog->top_vbox = vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox,
			gtk_label_new_with_mnemonic(_("_Basic")));
	gtk_widget_show(vbox);

	/* Setup the top frames. */
	add_login_options(dialog, vbox);
	add_user_options(dialog, vbox);

	button = gtk_check_button_new_with_mnemonic(
		_("Create _this new account on the server"));
	gtk_box_pack_start(GTK_BOX(main_vbox), button, FALSE, FALSE, 0);
	gtk_widget_show(button);
	dialog->register_button = button;
	g_signal_connect(G_OBJECT(dialog->register_button), "toggled", G_CALLBACK(register_button_cb), dialog);
	if (dialog->account == NULL)
		gtk_widget_set_sensitive(button, FALSE);

	if (!dialog->protocol || !PURPLE_PROTOCOL_IMPLEMENTS(dialog->protocol, SERVER, register_user))
		gtk_widget_hide(button);

	/* Setup the page with 'Advanced' (protocol options). */
	add_account_options(dialog);

	/* Setup the proxy options page. */
	dialog->proxy_options = pidgin_proxy_options_new();
	if(PURPLE_IS_ACCOUNT(dialog->account)) {
		pidgin_proxy_options_set_info(PIDGIN_PROXY_OPTIONS(dialog->proxy_options),
		                              purple_account_get_proxy_info(dialog->account));
	}
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), dialog->proxy_options,
	                         gtk_label_new_with_mnemonic(_("Proxy")));
	gtk_widget_show(dialog->proxy_options);

	add_voice_options(dialog);

	/* Buttons */
	gtk_dialog_add_button(GTK_DIALOG(win), _("_Cancel"), RESPONSE_CLOSE);
	dialog->ok_button = gtk_dialog_add_button(GTK_DIALOG(win),
	                                          (type == PIDGIN_ADD_ACCOUNT_DIALOG) ? _("_Add") : _("_Save"),
	                                          RESPONSE_ADD);

	if (dialog->account == NULL) {
		gtk_widget_set_sensitive(dialog->ok_button, FALSE);
	}

	/* Show the window. */
	gtk_widget_show(win);
	if (!account)
		gtk_widget_grab_focus(dialog->protocol_menu);
}

static void
pidgin_account_dialog_read_password_cb(GObject *obj, GAsyncResult *res,
                                       gpointer data)
{
	PurpleCredentialManager *manager = PURPLE_CREDENTIAL_MANAGER(obj);
	PidginAccountDialogShowData *d = (PidginAccountDialogShowData *)data;
	GError *error = NULL;
	gchar *password;

	password = purple_credential_manager_read_password_finish(manager, res,
	                                                          &error);

	if(error != NULL) {
		purple_debug_warning("gtkaccount", "failed to read password: %s",
		                     error->message);

		g_error_free(error);
	}

	pidgin_account_dialog_show_continue(d->type, d->account, password);

	g_free(password);
	g_free(d);
}

void
pidgin_account_dialog_show(PidginAccountDialogType type, PurpleAccount *account)
{
	PurpleCredentialManager *manager = NULL;

	manager = purple_credential_manager_get_default();

	if(PURPLE_IS_ACCOUNT(account)) {
		/* this is kind of dangerous, but it's no worse than the old version.
		 * Regardless this dialog needs a lot of TLC.
		 */
		PidginAccountDialogShowData *data = NULL;
		data = g_new0(PidginAccountDialogShowData, 1);
		data->account = account;
		data->type = type;

		purple_credential_manager_read_password_async(manager, account, NULL,
		                                              pidgin_account_dialog_read_password_cb,
		                                              data);
	} else {
		pidgin_account_dialog_show_continue(type, account, NULL);
	}
}

/**************************************************************************
 * Accounts Dialog
 **************************************************************************/

static void
free_add_user_data(G_GNUC_UNUSED PidginMiniDialog *mini_dialog,
                   G_GNUC_UNUSED GtkButton *button,
                   gpointer user_data)
{
	PidginAccountAddUserData *data = user_data;
	g_free(data->username);
	g_free(data->alias);
	g_free(data);
}

static void
add_user_cb(G_GNUC_UNUSED PidginMiniDialog *mini_dialog,
            G_GNUC_UNUSED GtkButton *button, gpointer user_data)
{
	PidginAccountAddUserData *data = user_data;
	PurpleConnection *gc = purple_account_get_connection(data->account);

	if (g_list_find(purple_connections_get_all(), gc))
	{
		purple_blist_request_add_buddy(data->account, data->username,
									 NULL, data->alias);
	}

	free_add_user_data(NULL, NULL, user_data);
}

static char *
make_info(PurpleAccount *account, PurpleConnection *gc, const char *remote_user,
          const char *id, const char *alias, const char *msg)
{
	if (msg != NULL && *msg == '\0')
		msg = NULL;

	return g_strdup_printf(_("%s%s%s%s has made %s his or her buddy%s%s"),
	                       remote_user,
	                       (alias != NULL ? " ("  : ""),
	                       (alias != NULL ? alias : ""),
	                       (alias != NULL ? ")"   : ""),
	                       (id != NULL
	                        ? id
	                        : (purple_connection_get_display_name(gc) != NULL
	                           ? purple_connection_get_display_name(gc)
	                           : purple_account_get_username(account))),
	                       (msg != NULL ? ": " : "."),
	                       (msg != NULL ? msg  : ""));
}

static void
pidgin_accounts_notify_added(PurpleAccount *account, const char *remote_user,
                               const char *id, const char *alias,
                               const char *msg)
{
	char *buffer;
	PurpleConnection *gc;
	GtkWidget *alert;

	gc = purple_account_get_connection(account);

	buffer = make_info(account, gc, remote_user, id, alias, msg);
	alert = pidgin_mini_dialog_new_with_buttons(
		buffer, NULL, "dialog-information", NULL,
		_("Close"), NULL, NULL);
	pidgin_blist_add_alert(alert);

	g_free(buffer);
}

static void
pidgin_accounts_request_add(PurpleAccount *account, const char *remote_user,
                              const char *id, const char *alias,
                              const char *msg)
{
	char *buffer;
	PurpleConnection *gc;
	PidginAccountAddUserData *data;
	GtkWidget *alert;

	gc = purple_account_get_connection(account);

	data = g_new0(PidginAccountAddUserData, 1);
	data->account  = account;
	data->username = g_strdup(remote_user);
	data->alias    = g_strdup(alias);

	buffer = make_info(account, gc, remote_user, id, alias, msg);
	alert = pidgin_mini_dialog_new_with_buttons(
		_("Add buddy to your list?"), buffer, "dialog-question", data,
		_("Add"), add_user_cb, _("Cancel"), free_add_user_data, NULL);
	pidgin_blist_add_alert(alert);

	g_free(buffer);
}

struct auth_request
{
	PurpleAccountRequestAuthorizationCb auth_cb;
	PurpleAccountRequestAuthorizationCb deny_cb;
	void *data;
	char *username;
	char *alias;
	PurpleAccount *account;
	gboolean add_buddy_after_auth;
};

static void
free_auth_request(struct auth_request *ar)
{
	g_free(ar->username);
	g_free(ar->alias);
	g_free(ar);
}

static void
authorize_and_add_cb(struct auth_request *ar, const char *message)
{
	ar->auth_cb(message, ar->data);
	if (ar->add_buddy_after_auth) {
		purple_blist_request_add_buddy(ar->account, ar->username, NULL, ar->alias);
	}
}

static void
authorize_noreason_cb(struct auth_request *ar)
{
	authorize_and_add_cb(ar, NULL);
}

static void
authorize_reason_cb(G_GNUC_UNUSED PidginMiniDialog *mini_dialog,
                    G_GNUC_UNUSED GtkButton *button, gpointer user_data)
{
	struct auth_request *ar = user_data;
	PurpleProtocol *protocol = purple_account_get_protocol(ar->account);

	if (protocol && (purple_protocol_get_options(protocol) & OPT_PROTO_AUTHORIZATION_GRANTED_MESSAGE)) {
		/* Duplicate information because ar is freed by closing minidialog */
		struct auth_request *aa = g_new0(struct auth_request, 1);
		aa->auth_cb = ar->auth_cb;
		aa->deny_cb = ar->deny_cb;
		aa->data = ar->data;
		aa->account = ar->account;
		aa->username = g_strdup(ar->username);
		aa->alias = g_strdup(ar->alias);
		aa->add_buddy_after_auth = ar->add_buddy_after_auth;
		purple_request_input(ar->account, NULL, _("Authorization acceptance message:"),
		                     NULL, _("No reason given."), TRUE, FALSE, NULL,
		                     _("OK"), G_CALLBACK(authorize_and_add_cb),
		                     _("Cancel"), G_CALLBACK(authorize_noreason_cb),
		                     purple_request_cpar_from_account(ar->account),
		                     aa);
		/* FIXME: aa is going to leak now. */
	} else {
		authorize_noreason_cb(ar);
	}
}

static void
deny_no_add_cb(struct auth_request *ar, const char *message)
{
	ar->deny_cb(message, ar->data);
}

static void
deny_noreason_cb(struct auth_request *ar)
{
	ar->deny_cb(NULL, ar->data);
}

static void
deny_reason_cb(G_GNUC_UNUSED PidginMiniDialog *mini_dialog,
               G_GNUC_UNUSED GtkButton *button, gpointer user_data)
{
	struct auth_request *ar = user_data;
	PurpleProtocol *protocol = purple_account_get_protocol(ar->account);

	if (protocol && (purple_protocol_get_options(protocol) & OPT_PROTO_AUTHORIZATION_DENIED_MESSAGE)) {
		/* Duplicate information because ar is freed by closing minidialog */
		struct auth_request *aa = g_new0(struct auth_request, 1);
		aa->auth_cb = ar->auth_cb;
		aa->deny_cb = ar->deny_cb;
		aa->data = ar->data;
		aa->add_buddy_after_auth = ar->add_buddy_after_auth;
		purple_request_input(ar->account, NULL, _("Authorization denied message:"),
		                     NULL, _("No reason given."), TRUE, FALSE, NULL,
		                     _("OK"), G_CALLBACK(deny_no_add_cb),
		                     _("Cancel"), G_CALLBACK(deny_noreason_cb),
		                     purple_request_cpar_from_account(ar->account),
		                     aa);
		/* FIXME: aa is going to leak now. */
	} else {
		deny_noreason_cb(ar);
	}
}

static gboolean
get_user_info_cb(GtkWidget   *label,
                 const gchar *uri,
                 gpointer     data)
{
	struct auth_request *ar = data;
	if (purple_strequal(uri, "viewinfo")) {
		pidgin_retrieve_user_info(purple_account_get_connection(ar->account), ar->username);
		return TRUE;
	}
	return FALSE;
}

static void
send_im_cb(G_GNUC_UNUSED PidginMiniDialog *mini_dialog,
           G_GNUC_UNUSED GtkButton *button,
           gpointer data)
{
	struct auth_request *ar = data;
	pidgin_dialogs_im_with_user(ar->account, ar->username);
}

static void *
pidgin_accounts_request_authorization(PurpleAccount *account,
                                      const char *remote_user,
                                      const char *id,
                                      const char *alias,
                                      const char *message,
                                      gboolean on_list,
                                      PurpleAccountRequestAuthorizationCb auth_cb,
                                      PurpleAccountRequestAuthorizationCb deny_cb,
                                      void *user_data)
{
	char *buffer;
	PurpleConnection *gc;
	GtkWidget *alert;
	PidginMiniDialog *dialog;
	GdkPixbuf *protocol_icon;
	struct auth_request *aa;
	const char *our_name;
	gboolean have_valid_alias;
	char *escaped_remote_user;
	char *escaped_alias;
	char *escaped_our_name;
	char *escaped_message;

	gc = purple_account_get_connection(account);
	if (message != NULL && *message != '\0')
		escaped_message = g_markup_escape_text(message, -1);
	else
		escaped_message = g_strdup("");

	our_name = (id != NULL) ? id :
			(purple_connection_get_display_name(gc) != NULL) ? purple_connection_get_display_name(gc) :
			purple_account_get_username(account);
	escaped_our_name = g_markup_escape_text(our_name, -1);

	escaped_remote_user = g_markup_escape_text(remote_user, -1);

	have_valid_alias = alias && *alias;
	escaped_alias = have_valid_alias ? g_markup_escape_text(alias, -1) : g_strdup("");

	buffer = g_strdup_printf(_("<a href=\"viewinfo\">%s</a>%s%s%s wants to add you (%s) to his or her buddy list%s%s"),
				escaped_remote_user,
				(have_valid_alias ? " ("  : ""),
				escaped_alias,
				(have_valid_alias ? ")"   : ""),
				escaped_our_name,
				(*escaped_message ? ": " : "."),
				escaped_message);

	g_free(escaped_remote_user);
	g_free(escaped_alias);
	g_free(escaped_our_name);
	g_free(escaped_message);

	protocol_icon = pidgin_create_protocol_icon(account, PIDGIN_PROTOCOL_ICON_SMALL);

	aa = g_new0(struct auth_request, 1);
	aa->auth_cb = auth_cb;
	aa->deny_cb = deny_cb;
	aa->data = user_data;
	aa->username = g_strdup(remote_user);
	aa->alias = g_strdup(alias);
	aa->account = account;
	aa->add_buddy_after_auth = !on_list;

	dialog = pidgin_mini_dialog_new_with_custom_icon(
		_("Authorize buddy?"), NULL, protocol_icon);
	alert = GTK_WIDGET(dialog);

	pidgin_mini_dialog_enable_description_markup(dialog);
	pidgin_mini_dialog_set_link_callback(dialog, G_CALLBACK(get_user_info_cb), aa);
	pidgin_mini_dialog_set_description(dialog, buffer);
	pidgin_mini_dialog_add_button(dialog, _("Authorize"), authorize_reason_cb, aa);
	pidgin_mini_dialog_add_button(dialog, _("Deny"), deny_reason_cb, aa);
	pidgin_mini_dialog_add_non_closing_button(dialog, _("Send Instant Message"), send_im_cb, aa);

	g_signal_connect_swapped(G_OBJECT(alert), "destroy", G_CALLBACK(free_auth_request), aa);
	g_signal_connect(G_OBJECT(alert), "destroy", G_CALLBACK(purple_account_request_close), NULL);
	pidgin_blist_add_alert(alert);

	g_free(buffer);

	return alert;
}

static void
pidgin_accounts_request_close(void *ui_handle)
{
	gtk_widget_destroy(GTK_WIDGET(ui_handle));
}

static PurpleAccountUiOps ui_ops =
{
	.notify_added = pidgin_accounts_notify_added,
	.request_add = pidgin_accounts_request_add,
	.request_authorize = pidgin_accounts_request_authorization,
	.close_account_request = pidgin_accounts_request_close,
};

PurpleAccountUiOps *
pidgin_accounts_get_ui_ops(void)
{
	return &ui_ops;
}

void *
pidgin_accounts_get_handle(void) {
	static int handle;

	return &handle;
}

void
pidgin_accounts_init(void)
{
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/accounts");
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/accounts/dialog");
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/accounts/dialog/width",  520);
	purple_prefs_add_int(PIDGIN_PREFS_ROOT "/accounts/dialog/height", 321);

	purple_prefs_add_path(PIDGIN_PREFS_ROOT "/accounts/buddyicon", NULL);

	purple_signal_register(pidgin_accounts_get_handle(), "account-modified",
						 purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
						 PURPLE_TYPE_ACCOUNT);
}

void
pidgin_accounts_uninit(void)
{
	purple_signals_disconnect_by_handle(pidgin_accounts_get_handle());
	purple_signals_unregister_by_instance(pidgin_accounts_get_handle());
}

