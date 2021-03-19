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

#include "pidgin/pidginavatar.h"

struct _PidginAvatar {
	GtkEventBox parent;

	GtkWidget *icon;

	GdkPixbufAnimation *animation;
	gboolean animate;

	PurpleBuddy *buddy;
	PurpleConversation *conversation;
};

enum {
	PROP_0,
	PROP_ANIMATE,
	PROP_BUDDY,
	PROP_CONVERSATION,
	N_PROPERTIES,
};
static GParamSpec *properties[N_PROPERTIES] = {NULL, };

G_DEFINE_TYPE(PidginAvatar, pidgin_avatar, GTK_TYPE_EVENT_BOX)

/******************************************************************************
 * Actions
 *****************************************************************************/
static void
pidgin_avatar_save_response_cb(GtkNativeDialog *native, gint response,
                               gpointer data)
{
	PidginAvatar *avatar = PIDGIN_AVATAR(data);
	PurpleBuddyIcon *icon = NULL;

	if(response != GTK_RESPONSE_ACCEPT || !PURPLE_IS_BUDDY(avatar->buddy)) {
		gtk_native_dialog_destroy(native);

		return;
	}

	icon = purple_buddy_get_icon(avatar->buddy);

	if(icon != NULL) {
		GtkFileChooser *chooser = GTK_FILE_CHOOSER(native);
		gchar *filename = NULL;

		filename = gtk_file_chooser_get_filename(chooser);

		purple_buddy_icon_save_to_filename(icon, filename, NULL);

		g_free(filename);
	}

	gtk_native_dialog_destroy(native);
}

static void
pidgin_avatar_save_cb(GSimpleAction *action, GVariant *parameter,
                      gpointer data)
{
	PidginAvatar *avatar = PIDGIN_AVATAR(data);
	PurpleAccount *account = NULL;
	GtkFileChooserNative *native = NULL;
	GtkFileChooser *chooser = NULL;
	GtkWindow *window = NULL;
	const gchar *ext = NULL, *name = NULL;
	gchar *filename = NULL;

	g_return_if_fail(PURPLE_IS_BUDDY(avatar->buddy));

	ext = purple_buddy_icon_get_extension(purple_buddy_get_icon(avatar->buddy));

	account = purple_buddy_get_account(avatar->buddy);
	name = purple_buddy_get_name(avatar->buddy);
	filename = g_strdup_printf("%s.%s", purple_normalize(account, name), ext);

	window = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(avatar)));
	native = gtk_file_chooser_native_new(_("Save Avatar"),
	                                     window,
	                                     GTK_FILE_CHOOSER_ACTION_SAVE,
	                                     _("_Save"),
	                                     _("_Cancel"));
	g_signal_connect(G_OBJECT(native), "response",
	                 G_CALLBACK(pidgin_avatar_save_response_cb), avatar);

	chooser = GTK_FILE_CHOOSER(native);

	gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);
	gtk_file_chooser_set_filename(chooser, filename);
	g_free(filename);

	gtk_native_dialog_show(GTK_NATIVE_DIALOG(native));
}

static void
pidgin_avatar_set_custom_response_cb(GtkNativeDialog *native, gint response,
                                     gpointer data)
{
	PidginAvatar *avatar = PIDGIN_AVATAR(data);
	GtkFileChooser *chooser = GTK_FILE_CHOOSER(native);
	gchar *filename = NULL;

	if(response != GTK_RESPONSE_ACCEPT || !PURPLE_IS_BUDDY(avatar->buddy)) {
		gtk_native_dialog_destroy(native);

		return;
	}

	filename = gtk_file_chooser_get_filename(chooser);
	if(filename != NULL) {
		PurpleContact *contact = purple_buddy_get_contact(avatar->buddy);
		PurpleBlistNode *node = PURPLE_BLIST_NODE(contact);

		purple_buddy_icons_node_set_custom_icon_from_file(node, filename);
	}

	gtk_native_dialog_destroy(native);
}

static void
pidgin_avatar_set_custom_cb(GSimpleAction *action, GVariant *parameter,
                            gpointer data)
{
	PidginAvatar *avatar = PIDGIN_AVATAR(data);
	GtkFileChooserNative *native = NULL;
	GtkWindow *window = NULL;

	window = GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(avatar)));
	native = gtk_file_chooser_native_new(_("Set Custom Avatar"),
	                                     window,
	                                     GTK_FILE_CHOOSER_ACTION_OPEN,
	                                     _("_Set Custom"),
	                                     _("_Cancel"));

	g_signal_connect(G_OBJECT(native), "response",
	                 G_CALLBACK(pidgin_avatar_set_custom_response_cb), avatar);

	gtk_native_dialog_show(GTK_NATIVE_DIALOG(native));
}

static void
pidgin_avatar_clear_custom_cb(GSimpleAction *action, GVariant *parameter,
                              gpointer data)
{
	PidginAvatar *avatar = PIDGIN_AVATAR(data);

	if(PURPLE_IS_BUDDY(avatar->buddy)) {
		PurpleContact *contact = purple_buddy_get_contact(avatar->buddy);
		PurpleBlistNode *node = PURPLE_BLIST_NODE(contact);

		purple_buddy_icons_node_set_custom_icon_from_file(node, NULL);
	}
}

static GActionEntry actions[] = {
	{
		.name = "save-avatar",
		.activate = pidgin_avatar_save_cb,
	}, {
		.name = "set-custom-avatar",
		.activate = pidgin_avatar_set_custom_cb,
	}, {
		.name = "clear-custom-avatar",
		.activate = pidgin_avatar_clear_custom_cb,
	},
};

/******************************************************************************
 * Helpers
 *****************************************************************************/
static GdkPixbufAnimation *
pidgin_avatar_find_buddy_icon(PurpleBuddy *buddy,
                              PurpleIMConversation *conversation)
{
	GdkPixbufAnimation *ret = NULL;
	GInputStream *stream = NULL;
	PurpleContact *contact = NULL;

	g_return_val_if_fail(PURPLE_IS_BUDDY(buddy), NULL);

	/* First check if our user has set a custom icon for this buddy. */
	contact = purple_buddy_get_contact(buddy);
	if(PURPLE_IS_CONTACT(contact)) {
		PurpleBlistNode *node = PURPLE_BLIST_NODE(contact);
		PurpleImage *custom_image = NULL;

		custom_image = purple_buddy_icons_node_find_custom_icon(node);
		if(PURPLE_IS_IMAGE(custom_image)) {
			gconstpointer data = purple_image_get_data(custom_image);
			gsize length = purple_image_get_data_size(custom_image);

			stream = g_memory_input_stream_new_from_data(data, (gssize)length,
			                                             NULL);
		}
	}

	/* If there is no custom icon, fall back to checking if the buddy has an
	 * icon set.
	 */
	if(!G_IS_INPUT_STREAM(stream)) {
		PurpleBuddyIcon *icon = purple_buddy_get_icon(buddy);

		if(icon != NULL) {
			stream = purple_buddy_icon_get_stream(icon);
		}
	}

	/* Finally if we still don't have icon, we fallback to asking the
	 * conversation for one.
	 */
	if(!G_IS_INPUT_STREAM(stream) && PURPLE_IS_IM_CONVERSATION(conversation)) {
		PurpleBuddyIcon *icon = purple_im_conversation_get_icon(PURPLE_IM_CONVERSATION(conversation));

		if(icon != NULL) {
			stream = purple_buddy_icon_get_stream(icon);
		}
	}

	if(G_IS_INPUT_STREAM(stream)) {
		ret = gdk_pixbuf_animation_new_from_stream(stream, NULL, NULL);
		g_clear_object(&stream);
	}

	return ret;
}

static void
pidgin_avatar_update(PidginAvatar *avatar) {
	PurpleAccount *account = NULL;
	GdkPixbufAnimation *animation = NULL;

	if(PURPLE_IS_BUDDY(avatar->buddy)) {
		animation = pidgin_avatar_find_buddy_icon(avatar->buddy, NULL);
	} else if(PURPLE_IS_IM_CONVERSATION(avatar->conversation)) {
		PurpleBuddy *buddy = NULL;
		const gchar *name = NULL;

		account = purple_conversation_get_account(avatar->conversation);

		name = purple_conversation_get_name(avatar->conversation);
		buddy = purple_blist_find_buddy(account, name);

		if(PURPLE_IS_BUDDY(buddy)) {
			animation = pidgin_avatar_find_buddy_icon(buddy,
		                                              PURPLE_IM_CONVERSATION(avatar->conversation));
		}
	}

	g_set_object(&avatar->animation, animation);

	if(GDK_IS_PIXBUF_ANIMATION(avatar->animation)) {
		if(avatar->animate) {
			gtk_image_set_from_animation(GTK_IMAGE(avatar->icon),
			                             avatar->animation);
		} else {
			GdkPixbuf *frame = NULL;

			frame = gdk_pixbuf_animation_get_static_image(avatar->animation);

			gtk_image_set_from_pixbuf(GTK_IMAGE(avatar->icon), frame);
		}
	} else {
		gtk_image_clear(GTK_IMAGE(avatar->icon));
	}

	g_clear_object(&animation);
}

/******************************************************************************
 * Callbacks
 *****************************************************************************/
static gboolean
pidgin_avatar_button_press_handler(GtkWidget *widget, GdkEventButton *event,
                                   gpointer data)
{
	PidginAvatar *avatar = PIDGIN_AVATAR(data);
	GtkBuilder *builder = NULL;
	GtkWidget *menu = NULL;
	GMenuModel *model = NULL;

	if(!gdk_event_triggers_context_menu((GdkEvent *)event)) {
		return FALSE;
	}

	builder = gtk_builder_new_from_resource("/im/pidgin/Pidgin/Avatar/menu.ui");
	model = (GMenuModel *)gtk_builder_get_object(builder, "menu");

	menu = gtk_menu_new_from_model(model);
	gtk_menu_attach_to_widget(GTK_MENU(menu), GTK_WIDGET(avatar), NULL);

	g_clear_object(&builder);

	gtk_widget_show_all(menu);

	gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);

	return TRUE;
}

/*
 * This function is a callback for when properties change on the buddy we're
 * tracking.  It should not be reused for the conversation we're tracking
 * because we have to disconnect old handlers and reuse of this function will
 * cause issues if a buddy is changed but a conversation is not and vice versa.
 */
static void
pidgin_avatar_buddy_icon_updated(GObject *obj, GParamSpec *pspec, gpointer d) {
	PidginAvatar *avatar = PIDGIN_AVATAR(d);

	pidgin_avatar_update(avatar);
}

/*
 * This function is a callback for when properties change on the conversation
 * we're tracking.  It should not be reused for the buddy we're tracking
 * because we have to disconnect old handlers and reuse of this function will
 * cause issues if a buddy is changed but a conversation is not and vice versa.
 */
static void
pidgin_avatar_conversation_updated(GObject *obj, GParamSpec *pspec, gpointer d)
{
	PidginAvatar *avatar = PIDGIN_AVATAR(d);

	pidgin_avatar_update(avatar);
}

static gboolean
pidgin_avatar_enter_notify_handler(GtkWidget *widget, GdkEvent *event,
                                   gpointer data)
{
	PidginAvatar *avatar = PIDGIN_AVATAR(widget);

	pidgin_avatar_set_animate(avatar, TRUE);

	return FALSE;
}

static gboolean
pidgin_avatar_leave_notify_handler(GtkWidget *widget, GdkEvent *event,
                                   gpointer user_data)
{
	PidginAvatar *avatar = PIDGIN_AVATAR(widget);

	pidgin_avatar_set_animate(avatar, FALSE);

	return FALSE;
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
static void
pidgin_avatar_get_property(GObject *obj, guint param_id, GValue *value,
                           GParamSpec *pspec)
{
	PidginAvatar *avatar = PIDGIN_AVATAR(obj);

	switch(param_id) {
		case PROP_ANIMATE:
			g_value_set_boolean(value, pidgin_avatar_get_animate(avatar));
			break;
		case PROP_BUDDY:
			g_value_set_object(value, pidgin_avatar_get_buddy(avatar));
			break;
		case PROP_CONVERSATION:
			g_value_set_object(value, pidgin_avatar_get_conversation(avatar));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
pidgin_avatar_set_property(GObject *obj, guint param_id, const GValue *value,
                           GParamSpec *pspec)
{
	PidginAvatar *avatar = PIDGIN_AVATAR(obj);

	switch(param_id) {
		case PROP_ANIMATE:
			pidgin_avatar_set_animate(avatar, g_value_get_boolean(value));
			break;
		case PROP_BUDDY:
			pidgin_avatar_set_buddy(avatar, g_value_get_object(value));
			break;
		case PROP_CONVERSATION:
			pidgin_avatar_set_conversation(avatar, g_value_get_object(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
pidgin_avatar_dispose(GObject *obj) {
	PidginAvatar *avatar = PIDGIN_AVATAR(obj);

	pidgin_avatar_set_buddy(avatar, NULL);
	pidgin_avatar_set_conversation(avatar, NULL);

	g_clear_object(&avatar->animation);

	G_OBJECT_CLASS(pidgin_avatar_parent_class)->dispose(obj);
}

static void
pidgin_avatar_init(PidginAvatar *avatar) {
	GSimpleActionGroup *group = NULL;

	gtk_widget_init_template(GTK_WIDGET(avatar));

	/* For development/design purposes, the avatar defaults to the
	 * "image-missing" icon.  However, we don't want to display that to users,
	 * so we clear it during run time.
	 */
	gtk_image_clear(GTK_IMAGE(avatar->icon));

	/* Now setup our actions. */
	group = g_simple_action_group_new();
	g_action_map_add_action_entries(G_ACTION_MAP(group), actions,
	                                G_N_ELEMENTS(actions), avatar);

	gtk_widget_insert_action_group(GTK_WIDGET(avatar), "avatar",
	                               G_ACTION_GROUP(group));
}

static void
pidgin_avatar_class_init(PidginAvatarClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	obj_class->get_property = pidgin_avatar_get_property;
	obj_class->set_property = pidgin_avatar_set_property;
	obj_class->dispose = pidgin_avatar_dispose;

	/**
	 * PidginAvatar::animate:
	 *
	 * Whether or not an animated avatar should be animated.
	 */
	properties[PROP_ANIMATE] = g_param_spec_boolean(
		"animate", "animate",
		"Whether or not to animate an animated avatar",
		FALSE,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	/**
	 * PidginAvatar::buddy:
	 *
	 * The #PurpleBuddy whose avatar will be displayed.
	 */
	properties[PROP_BUDDY] = g_param_spec_object(
		"buddy", "buddy",
		"The buddy whose avatar to display",
		PURPLE_TYPE_BUDDY,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	/**
	 * PidginAvatar::conversation:
	 *
	 * The #PurpleConversation which will be used to find the correct
	 * #PurpleBuddy.
	 */
	properties[PROP_CONVERSATION] = g_param_spec_object(
		"conversation", "conversation",
		"The conversation used to find the correct buddy.",
		PURPLE_TYPE_CONVERSATION,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, N_PROPERTIES, properties);

	gtk_widget_class_set_template_from_resource(
	    widget_class,
	    "/im/pidgin/Pidgin/Avatar/avatar.ui"
	);

	gtk_widget_class_bind_template_child(widget_class, PidginAvatar, icon);

	gtk_widget_class_bind_template_callback(widget_class,
	                                        pidgin_avatar_button_press_handler);
	gtk_widget_class_bind_template_callback(widget_class,
	                                        pidgin_avatar_enter_notify_handler);
	gtk_widget_class_bind_template_callback(widget_class,
	                                        pidgin_avatar_leave_notify_handler);
}

/******************************************************************************
 * API
 *****************************************************************************/
GtkWidget *
pidgin_avatar_new(void) {
	return GTK_WIDGET(g_object_new(PIDGIN_TYPE_AVATAR, NULL));
}

void
pidgin_avatar_set_animate(PidginAvatar *avatar, gboolean animate) {
	g_return_if_fail(PIDGIN_IS_AVATAR(avatar));

	avatar->animate = animate;

	if(GDK_IS_PIXBUF_ANIMATION(avatar->animation)) {
		if(avatar->animate) {
			gtk_image_set_from_animation(GTK_IMAGE(avatar->icon),
			                             avatar->animation);
		} else {
			GdkPixbuf *frame = NULL;

			frame = gdk_pixbuf_animation_get_static_image(avatar->animation);

			gtk_image_set_from_pixbuf(GTK_IMAGE(avatar->icon), frame);
		}
	}
}

gboolean
pidgin_avatar_get_animate(PidginAvatar *avatar) {
	g_return_val_if_fail(PIDGIN_IS_AVATAR(avatar), FALSE);

	return avatar->animate;
}

void
pidgin_avatar_set_buddy(PidginAvatar *avatar, PurpleBuddy *buddy) {
	g_return_if_fail(PIDGIN_IS_AVATAR(avatar));

	/* Remove our old signal handler. */
	if(PURPLE_IS_BUDDY(avatar->buddy)) {
		g_signal_handlers_disconnect_by_func(avatar->buddy,
		                                     pidgin_avatar_buddy_icon_updated,
		                                     avatar);
	}

	if(g_set_object(&avatar->buddy, buddy)) {
		pidgin_avatar_update(avatar);

		g_object_notify_by_pspec(G_OBJECT(avatar), properties[PROP_BUDDY]);
	}

	/* Add the notify signal so we can update when the icon changes. */
	if(PURPLE_IS_BUDDY(avatar->buddy)) {
		g_signal_connect(G_OBJECT(avatar->buddy), "notify::icon",
		                 G_CALLBACK(pidgin_avatar_buddy_icon_updated), avatar);
	}
}

PurpleBuddy *
pidgin_avatar_get_buddy(PidginAvatar *avatar) {
	g_return_val_if_fail(PIDGIN_IS_AVATAR(avatar), NULL);

	return avatar->buddy;
}

void
pidgin_avatar_set_conversation(PidginAvatar *avatar,
                               PurpleConversation *conversation)
{
	g_return_if_fail(PIDGIN_IS_AVATAR(avatar));

	/* Remove our old signal handler. */
	if(PURPLE_IS_CONVERSATION(avatar->conversation)) {
		g_signal_handlers_disconnect_by_func(avatar->conversation,
		                                     pidgin_avatar_conversation_updated,
		                                     avatar);
	}

	if(g_set_object(&avatar->conversation, conversation)) {
		g_object_notify_by_pspec(G_OBJECT(avatar),
		                         properties[PROP_CONVERSATION]);
	}

	/* Add the notify signal so we can update when the icon changes. */
	if(PURPLE_IS_CONVERSATION(avatar->conversation)) {
		g_signal_connect(G_OBJECT(avatar->conversation), "notify",
		                 G_CALLBACK(pidgin_avatar_conversation_updated), avatar);
	}
}

PurpleConversation *
pidgin_avatar_get_conversation(PidginAvatar *avatar) {
	g_return_val_if_fail(PIDGIN_IS_AVATAR(avatar), NULL);

	return avatar->conversation;
}
