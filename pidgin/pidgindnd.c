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
#include <glib/gstdio.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <talkatu.h>

#include "pidgindnd.h"

#include "gtkconv.h"
#include "pidgincore.h"
#include "pidgingdkpixbuf.h"

enum {
	DND_FILE_TRANSFER,
	DND_IM_IMAGE,
	DND_BUDDY_ICON
};

typedef struct {
	char *filename;
	PurpleAccount *account;
	char *who;
} _DndData;

static void dnd_image_ok_callback(_DndData *data, int choice)
{
	const gchar *shortname;
	gchar *filedata;
	size_t size;
	GStatBuf st;
	GError *err = NULL;
	PurpleBuddy *buddy;
	PurpleContact *contact;
	PurpleImage *img;

	switch (choice) {
	case DND_BUDDY_ICON:
		if (g_stat(data->filename, &st)) {
			char *str;

			str = g_strdup_printf(_("The following error has occurred loading %s: %s"),
						data->filename, g_strerror(errno));
			purple_notify_error(NULL, NULL,
				_("Failed to load image"), str, NULL);
			g_free(str);

			break;
		}

		buddy = purple_blist_find_buddy(data->account, data->who);
		if (!buddy) {
			purple_debug_info("custom-icon", "You can only set custom icons for people on your buddylist.\n");
			break;
		}
		contact = purple_buddy_get_contact(buddy);
		purple_buddy_icons_node_set_custom_icon_from_file((PurpleBlistNode*)contact, data->filename);
		break;
	case DND_FILE_TRANSFER:
		purple_serv_send_file(purple_account_get_connection(data->account), data->who, data->filename);
		break;
	case DND_IM_IMAGE:
		if (!g_file_get_contents(data->filename, &filedata, &size,
					 &err)) {
			char *str;

			str = g_strdup_printf(_("The following error has occurred loading %s: %s"), data->filename, err->message);
			purple_notify_error(NULL, NULL,
				_("Failed to load image"), str, NULL);

			g_error_free(err);
			g_free(str);

			break;
		}
		shortname = strrchr(data->filename, G_DIR_SEPARATOR);
		shortname = shortname ? shortname + 1 : data->filename;
		img = purple_image_new_from_data((guint8 *)filedata, size);
		purple_image_set_friendly_filename(img, shortname);

# warning fix this when talkatu has a way to programmatically insert an image
		// pidgin_webview_insert_image(PIDGIN_WEBVIEW(gtkconv->entry), img);
		g_object_unref(img);

		break;
	}
	g_free(data->filename);
	g_free(data->who);
	g_free(data);
}

static void dnd_image_cancel_callback(_DndData *data, int choice)
{
	g_free(data->filename);
	g_free(data->who);
	g_free(data);
}

static void dnd_set_icon_ok_cb(_DndData *data)
{
	dnd_image_ok_callback(data, DND_BUDDY_ICON);
}

static void dnd_set_icon_cancel_cb(_DndData *data)
{
	g_free(data->filename);
	g_free(data->who);
	g_free(data);
}

static void
pidgin_dnd_file_send_image(PurpleAccount *account, const gchar *who,
		const gchar *filename)
{
	PurpleConnection *gc = purple_account_get_connection(account);
	PurpleProtocol *protocol = NULL;
	_DndData *data = g_new0(_DndData, 1);
	gboolean ft = FALSE, im = FALSE;

	data->who = g_strdup(who);
	data->filename = g_strdup(filename);
	data->account = account;

	if (gc)
		protocol = purple_connection_get_protocol(gc);

	if (!(purple_connection_get_flags(gc) & PURPLE_CONNECTION_FLAG_NO_IMAGES))
		im = TRUE;

	if (protocol && PURPLE_IS_PROTOCOL_XFER(protocol)) {
		PurpleProtocolXferInterface *iface =
				PURPLE_PROTOCOL_XFER_GET_IFACE(protocol);

		if(iface->can_receive) {
			ft = purple_protocol_xfer_can_receive(
					PURPLE_PROTOCOL_XFER(protocol),
					gc, who);
		} else {
			ft = (iface->send_file) ? TRUE : FALSE;
		}
	}

	if (im && ft) {
		purple_request_choice(NULL, NULL,
				_("You have dragged an image"),
				_("You can send this image as a file "
					"transfer, embed it into this message, "
					"or use it as the buddy icon for this user."),
				(gpointer)DND_FILE_TRANSFER, _("OK"),
				(GCallback)dnd_image_ok_callback, _("Cancel"),
				(GCallback)dnd_image_cancel_callback,
				purple_request_cpar_from_account(account), data,
				_("Set as buddy icon"), DND_BUDDY_ICON,
				_("Send image file"), DND_FILE_TRANSFER,
				_("Insert in message"), DND_IM_IMAGE,
				NULL);
	} else if (!(im || ft)) {
		purple_request_yes_no(NULL, NULL, _("You have dragged an image"),
				_("Would you like to set it as the buddy icon for this user?"),
				PURPLE_DEFAULT_ACTION_NONE,
				purple_request_cpar_from_account(account),
				data, (GCallback)dnd_set_icon_ok_cb, (GCallback)dnd_set_icon_cancel_cb);
	} else {
		purple_request_choice(NULL, NULL,
				_("You have dragged an image"),
				(ft ? _("You can send this image as a file transfer, or use it as the buddy icon for this user.") :
				 _("You can insert this image into this message, or use it as the buddy icon for this user")),
				GINT_TO_POINTER(ft ? DND_FILE_TRANSFER : DND_IM_IMAGE),
				_("OK"), (GCallback)dnd_image_ok_callback,
				_("Cancel"), (GCallback)dnd_image_cancel_callback,
				purple_request_cpar_from_account(account),
				data,
				_("Set as buddy icon"), DND_BUDDY_ICON,
				(ft ? _("Send image file") : _("Insert in message")), (ft ? DND_FILE_TRANSFER : DND_IM_IMAGE),
				NULL);
	}

}

#ifndef _WIN32
static void
pidgin_dnd_file_send_desktop(PurpleAccount *account, const gchar *who,
		const gchar *filename)
{
	gchar *name;
	gchar *type;
	gchar *url;
	GKeyFile *desktop_file;
	PurpleConversation *conv;
	PidginConversation *gtkconv;
	GError *error = NULL;

	desktop_file = g_key_file_new();

	if (!g_key_file_load_from_file(desktop_file, filename, G_KEY_FILE_NONE, &error)) {
		if (error) {
			purple_debug_warning("D&D", "Failed to load %s: %s\n",
					filename, error->message);
			g_error_free(error);
		}
		return;
	}

	name = g_key_file_get_string(desktop_file, G_KEY_FILE_DESKTOP_GROUP,
			G_KEY_FILE_DESKTOP_KEY_NAME, &error);
	if (error) {
		purple_debug_warning("D&D", "Failed to read the Name from a desktop file: %s\n",
				error->message);
		g_error_free(error);

	}

	type = g_key_file_get_string(desktop_file, G_KEY_FILE_DESKTOP_GROUP,
			G_KEY_FILE_DESKTOP_KEY_TYPE, &error);
	if (error) {
		purple_debug_warning("D&D", "Failed to read the Type from a desktop file: %s\n",
				error->message);
		g_error_free(error);

	}

	url = g_key_file_get_string(desktop_file, G_KEY_FILE_DESKTOP_GROUP,
			G_KEY_FILE_DESKTOP_KEY_URL, &error);
	if (error) {
		purple_debug_warning("D&D", "Failed to read the Type from a desktop file: %s\n",
				error->message);
		g_error_free(error);

	}


	/* If any of this is null, do nothing. */
	if (!name || !type || !url) {
		g_free(type);
		g_free(name);
		g_free(url);

		return;
	}

	/* I don't know if we really want to do anything here.  Most of
	 * the desktop item types are crap like "MIME Type" (I have no
	 * clue how that would be a desktop item) and "Comment"...
	 * nothing we can really send.  The only logical one is
	 * "Application," but do we really want to send a binary and
	 * nothing else? Probably not.  I'll just give an error and
	 * return. */
	/* The original patch sent the icon used by the launcher.  That's probably wrong */
	if (purple_strequal(type, "Link")) {
		purple_notify_error(NULL, NULL, _("Cannot send launcher"),
				_("You dragged a desktop launcher. Most "
					"likely you wanted to send the target "
					"of this launcher instead of this "
					"launcher itself."), NULL);

	} else {
		GtkTextBuffer *buffer = NULL;
		GtkTextMark *mark = NULL;
		GtkTextIter iter;

		conv = PURPLE_CONVERSATION(purple_im_conversation_new(account, who));
		gtkconv =  PIDGIN_CONVERSATION(conv);

		buffer = talkatu_editor_get_buffer(TALKATU_EDITOR(gtkconv->editor));
		mark = gtk_text_buffer_get_insert(buffer);

		gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);

		talkatu_buffer_insert_link(TALKATU_BUFFER(buffer), &iter, name, url);
	}

	g_free(type);
	g_free(name);
	g_free(url);
}
#endif /* _WIN32 */

void
pidgin_dnd_file_manage(GtkSelectionData *sd, PurpleAccount *account, const char *who)
{
	GdkPixbuf *pb;
	GList *files = purple_uri_list_extract_filenames((const gchar *) gtk_selection_data_get_data(sd));
	PurpleConnection *gc = purple_account_get_connection(account);
	gchar *filename = NULL;
	gchar *basename = NULL;

	g_return_if_fail(account != NULL);
	g_return_if_fail(who != NULL);

	for ( ; files; files = g_list_delete_link(files, files)) {
		g_free(filename);
		g_free(basename);

		filename = files->data;
		basename = g_path_get_basename(filename);

		/* XXX - Make ft API support creating a transfer with more than one file */
		if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
			continue;
		}

		/* XXX - make ft api suupport sending a directory */
		/* Are we dealing with a directory? */
		if (g_file_test(filename, G_FILE_TEST_IS_DIR)) {
			char *str, *str2;

			str = g_strdup_printf(_("Cannot send folder %s."), basename);
			str2 = g_strdup_printf(_("%s cannot transfer a folder. You will need to send the files within individually."), PIDGIN_NAME);

			purple_notify_error(NULL, NULL, str, str2,
				purple_request_cpar_from_connection(gc));

			g_free(str);
			g_free(str2);
			continue;
		}

		/* Are we dealing with an image? */
		pb = pidgin_pixbuf_new_from_file(filename);
		if (pb) {
			pidgin_dnd_file_send_image(account, who, filename);

			g_object_unref(G_OBJECT(pb));

			continue;
		}

#ifndef _WIN32
		/* Are we trying to send a .desktop file? */
		else if (g_str_has_suffix(basename, ".desktop")) {
			pidgin_dnd_file_send_desktop(account, who, filename);

			continue;
		}
#endif /* _WIN32 */

		/* Everything is fine, let's send */
		purple_serv_send_file(gc, who, filename);
	}

	g_free(filename);
	g_free(basename);
}
