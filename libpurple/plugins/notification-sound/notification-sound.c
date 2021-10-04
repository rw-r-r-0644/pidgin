/*
 * pidgin
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

#include <config.h>

#include <glib/gi18n-lib.h>

#include <gplugin.h>
#include <gplugin-native.h>

#include <purple.h>
#include "libpurple/glibcompat.h"

#include <canberra.h>

#define PURPLE_NOTIFICATION_SOUND_DOMAIN \
	g_quark_from_static_string("purple-notification-sound")

#define PREF_ROOT "/plugins/core/notification-sound"
#define PREF_MUTED PREF_ROOT "/muted"
#define PREF_MUTE_UNTIL PREF_ROOT "/mute_until"

/******************************************************************************
 * Globals
 *****************************************************************************/
static gboolean muted = FALSE;
static GDateTime *mute_until = NULL;
static ca_context *context = NULL;

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
purple_notification_sound_load_prefs(void) {
	GTimeZone *tz = g_time_zone_new_utc();
	const gchar *str_mute_until = NULL;

	muted = purple_prefs_get_bool(PREF_MUTED);

	str_mute_until = purple_prefs_get_string(PREF_MUTE_UNTIL);
	mute_until = g_date_time_new_from_iso8601(str_mute_until, tz);
	g_time_zone_unref(tz);
}

static void
purple_notification_sound_save_prefs(void) {
	gchar *str_mute_until = NULL;

	if(mute_until != NULL) {
		str_mute_until = g_date_time_format_iso8601(mute_until);
	}

	purple_prefs_set_bool(PREF_MUTED, muted);
	purple_prefs_set_string(PREF_MUTE_UNTIL, str_mute_until);
	g_free(str_mute_until);
}

static void
purple_notification_sound_play(const gchar *event_id) {
	/* If we have a mute_util time check to see if it has passed. */
	if(mute_until != NULL) {
		GDateTime *now = g_date_time_new_now_utc();

		if(g_date_time_compare(mute_until, now) <= 0) {
			muted = FALSE;
			g_clear_pointer(&mute_until, g_date_time_unref);

			purple_notification_sound_save_prefs();
		}

		g_date_time_unref(now);
	}

	/* If we're not muted at this point play the sound. */
	if(!muted) {
		gint r = 0;

		r = ca_context_play(context, 0,
		                    CA_PROP_EVENT_ID, event_id,
		                    CA_PROP_EVENT_DESCRIPTION, "New libpurple message",
		                    NULL);

		if(r != 0) {
			purple_debug_warning("notification-sound",
			                     "failed to play sound %s\n",
			                     ca_strerror(r));
		}
	}
}

/******************************************************************************
 * Callbacks
 *****************************************************************************/
static void
purple_notification_sound_im_message_received(PurpleAccount *account,
                                              const gchar *sender,
                                              const gchar *message,
                                              PurpleConversation *conv,
                                              PurpleMessageFlags flags,
                                              gpointer data)
{
	purple_notification_sound_play("message-new-instant");
}

static void
purple_notification_sound_chat_message_received(PurpleAccount *account,
                                                gchar *sender,
                                                gchar *message,
                                                PurpleConversation *conv,
                                                PurpleMessageFlags flags,
                                                gpointer data)
{
	purple_notification_sound_play("message-new-instant");
}

/******************************************************************************
 * Actions
 *****************************************************************************/
static void
purple_notification_sound_mute(PurplePluginAction *action) {
	if(!muted) {
		muted = TRUE;
		g_clear_pointer(&mute_until, g_date_time_unref);

		purple_notification_sound_save_prefs();
	}
}

static void
purple_notification_sound_unmute(PurplePluginAction *action) {
	if(muted) {
		muted = FALSE;
		g_clear_pointer(&mute_until, g_date_time_unref);

		purple_notification_sound_save_prefs();
	}
}

static void
purple_notification_sound_mute_minutes(PurplePluginAction *action) {
	GDateTime *now = NULL;

	g_clear_pointer(&mute_until, g_date_time_unref);

	now = g_date_time_new_now_utc();
	mute_until = g_date_time_add_minutes(now,
	                                     GPOINTER_TO_INT(action->user_data));
	g_date_time_unref(now);

	muted = TRUE;

	purple_notification_sound_save_prefs();
}

static void
purple_notification_sound_mute_hours(PurplePluginAction *action) {
	GDateTime *now = NULL;

	g_clear_pointer(&mute_until, g_date_time_unref);

	now = g_date_time_new_now_utc();
	mute_until = g_date_time_add_hours(now,
	                                   GPOINTER_TO_INT(action->user_data));
	g_date_time_unref(now);

	muted = TRUE;

	purple_notification_sound_save_prefs();
}

static GList *
purple_notification_sound_actions(PurplePlugin *plugin) {
	GList *l = NULL;
	PurplePluginAction *action = NULL;

	action = purple_plugin_action_new(_("Mute"),
	                                  purple_notification_sound_mute);
	l = g_list_append(l, action);

	action = purple_plugin_action_new(_("Unmute"),
	                                  purple_notification_sound_unmute);
	l = g_list_append(l, action);

	/* Add a separator */
	l = g_list_append(l, NULL);

	action = purple_plugin_action_new(_("Mute for 30 minutes"),
	                                  purple_notification_sound_mute_minutes);
	action->user_data = GINT_TO_POINTER(30);
	l = g_list_append(l, action);

	action = purple_plugin_action_new(_("Mute for 1 hour"),
	                                  purple_notification_sound_mute_hours);
	action->user_data = GINT_TO_POINTER(1);
	l = g_list_append(l, action);

	action = purple_plugin_action_new(_("Mute for 2 hours"),
	                                  purple_notification_sound_mute_hours);
	action->user_data = GINT_TO_POINTER(2);
	l = g_list_append(l, action);

	action = purple_plugin_action_new(_("Mute for 4 hours"),
	                                  purple_notification_sound_mute_hours);
	action->user_data = GINT_TO_POINTER(4);
	l = g_list_append(l, action);

	return l;
}

/******************************************************************************
 * Plugin Exports
 *****************************************************************************/
static GPluginPluginInfo *
notification_sound_query(G_GNUC_UNUSED GError **error) {
	const gchar * const authors[] = {
		"Pidgin Developers <devel@pidgin.im>",
		NULL
	};

	return purple_plugin_info_new(
		"id", "purple/notification-sound",
		"abi-version", PURPLE_ABI_VERSION,
		"name", N_("Notification Sounds"),
		"version", VERSION,
		"summary", N_("Play sounds for notifications"),
		"authors", authors,
		"actions-cb", purple_notification_sound_actions,
		NULL
	);
}

static gboolean
notification_sound_load(GPluginPlugin *plugin, GError **error) {
	gpointer conv_handle = NULL;
	gint error_code = 0;

	error_code = ca_context_create(&context);
	if(error_code != 0) {
		g_set_error(error, PURPLE_NOTIFICATION_SOUND_DOMAIN, error_code,
		            "failed to create canberra context: %s",
		            ca_strerror(error_code));

		return FALSE;
	}

	purple_prefs_add_none(PREF_ROOT);
	purple_prefs_add_string(PREF_MUTE_UNTIL, NULL);
	purple_prefs_add_bool(PREF_MUTED, FALSE);

	purple_notification_sound_load_prefs();

	conv_handle = purple_conversations_get_handle();

	purple_signal_connect(conv_handle,
	                      "received-im-msg",
	                      plugin,
	                      PURPLE_CALLBACK(purple_notification_sound_im_message_received),
	                      NULL
	);

	purple_signal_connect(conv_handle,
	                      "received-chat-msg",
	                      plugin,
	                      PURPLE_CALLBACK(purple_notification_sound_chat_message_received),
	                      NULL
	);

	return TRUE;
}

static gboolean
notification_sound_unload(GPluginPlugin *plugin,
                          G_GNUC_UNUSED gboolean shutdown,
                          G_GNUC_UNUSED GError **error)
{
	purple_signals_disconnect_by_handle(plugin);

	purple_notification_sound_save_prefs();

	g_clear_pointer(&context, ca_context_destroy);
	g_clear_pointer(&mute_until, g_date_time_unref);

	return TRUE;
}

GPLUGIN_NATIVE_PLUGIN_DECLARE(notification_sound)
