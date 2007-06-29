/**
* @file gntsound.c GNT Sound API
* @ingroup finch
*
* finch
*
* Finch is the legal property of its developers, whose names are too numerous
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
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include "internal.h"
#include "finch.h"

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

#ifdef USE_GSTREAMER
#include <gst/gst.h>
#endif /* USE_GSTREAMER */

#include "debug.h"
#include "notify.h"
#include "prefs.h"
#include "sound.h"
#include "util.h"

#include "gntbox.h"
#include "gntwindow.h"
#include "gntcombobox.h"
#include "gntlabel.h"
#include "gntconv.h"
#include "gntsound.h"
#include "gntwidget.h"
#include "gntentry.h"
#include "gntcheckbox.h"
#include "gntline.h"
#include "gntslider.h"

struct finch_sound_event {
	char *label;
	char *pref;
	char *def;
};

typedef struct {
	GntWidget *method;
	GntWidget *command;
	GntWidget *conv_focus;
	GntWidget *while_status;
	GntWidget *volume;
	GntWidget *window;
} SoundPrefDialog;

typedef struct {
	gchar *method;
	gchar *command;
	gboolean conv_focus;
	gint while_status;
	gint volume;
	GntTree *events;
	SoundPrefDialog *dialog;
} SoundPrefData;

SoundPrefData *sound_pref_data;

#define PLAY_SOUND_TIMEOUT 15000

static guint mute_login_sounds_timeout = 0;
static gboolean mute_login_sounds = FALSE;

#ifdef USE_GSTREAMER
static gboolean gst_init_failed;
#endif /* USE_GSTREAMER */

static struct finch_sound_event sounds[PURPLE_NUM_SOUNDS] = {
	{N_("Buddy logs in"), "login", "login.wav"},
	{N_("Buddy logs out"), "logout", "logout.wav"},
	{N_("Message received"), "im_recv", "receive.wav"},
	{N_("Message received begins conversation"), "first_im_recv", "receive.wav"},
	{N_("Message sent"), "send_im", "send.wav"},
	{N_("Person enters chat"), "join_chat", "login.wav"},
	{N_("Person leaves chat"), "left_chat", "logout.wav"},
	{N_("You talk in chat"), "send_chat_msg", "send.wav"},
	{N_("Others talk in chat"), "chat_msg_recv", "receive.wav"},
	/* this isn't a terminator, it's the buddy pounce default sound event ;-) */
	{NULL, "pounce_default", "alert.wav"},
	{N_("Someone says your screen name in chat"), "nick_said", "alert.wav"}
};

static gboolean
unmute_login_sounds_cb(gpointer data)
{
	mute_login_sounds = FALSE;
	mute_login_sounds_timeout = 0;
	return FALSE;
}

static gboolean
chat_nick_matches_name(PurpleConversation *conv, const char *aname)
{
	PurpleConvChat *chat = NULL;
	char *nick = NULL;
	char *name = NULL;
	gboolean ret = FALSE;
	chat = purple_conversation_get_chat_data(conv);

	if (chat==NULL)
		return ret;

	nick = g_strdup(purple_normalize(conv->account, chat->nick));
	name = g_strdup(purple_normalize(conv->account, aname));

	if (g_utf8_collate(nick, name) == 0)
		ret = TRUE;

	g_free(nick);
	g_free(name);

	return ret;
}

/*
 * play a sound event for a conversation, honoring make_sound flag
 * of conversation and checking for focus if conv_focus pref is set
 */
static void
play_conv_event(PurpleConversation *conv, PurpleSoundEventID event)
{
	/* If we should not play the sound for some reason, then exit early */
	if (conv != NULL)
	{
		FinchConv *gntconv;
		gboolean has_focus;

		gntconv = FINCH_CONV(conv);

		has_focus = purple_conversation_has_focus(conv);

		if (has_focus && !purple_prefs_get_bool(FINCH_PREFS_ROOT "/sound/conv_focus"))
		{
			return;
		}
	}

	purple_sound_play_event(event, conv ? purple_conversation_get_account(conv) : NULL);
}

static void
buddy_state_cb(PurpleBuddy *buddy, PurpleSoundEventID event)
{
	purple_sound_play_event(event, purple_buddy_get_account(buddy));
}

static void
im_msg_received_cb(PurpleAccount *account, char *sender,
				   char *message, PurpleConversation *conv,
				   PurpleMessageFlags flags, PurpleSoundEventID event)
{
	if (flags & PURPLE_MESSAGE_DELAYED)
		return;

	if (conv==NULL){
		purple_sound_play_event(PURPLE_SOUND_FIRST_RECEIVE, account);
	}
	else{
		play_conv_event(conv, event);
	}
}

static void
im_msg_sent_cb(PurpleAccount *account, const char *receiver,
			   const char *message, PurpleSoundEventID event)
{
	PurpleConversation *conv = purple_find_conversation_with_account(
		PURPLE_CONV_TYPE_ANY, receiver, account);
	play_conv_event(conv, event);
}

static void
chat_buddy_join_cb(PurpleConversation *conv, const char *name,
				   PurpleConvChatBuddyFlags flags, gboolean new_arrival,
				   PurpleSoundEventID event)
{
	if (new_arrival && !chat_nick_matches_name(conv, name))
		play_conv_event(conv, event);
}

static void
chat_buddy_left_cb(PurpleConversation *conv, const char *name,
				   const char *reason, PurpleSoundEventID event)
{
	if (!chat_nick_matches_name(conv, name))
		play_conv_event(conv, event);
}

static void
chat_msg_sent_cb(PurpleAccount *account, const char *message,
				 int id, PurpleSoundEventID event)
{
	PurpleConnection *conn = purple_account_get_connection(account);
	PurpleConversation *conv = NULL;

	if (conn!=NULL)
		conv = purple_find_chat(conn,id);

	play_conv_event(conv, event);
}

static void
chat_msg_received_cb(PurpleAccount *account, char *sender,
					 char *message, PurpleConversation *conv,
					 PurpleMessageFlags flags, PurpleSoundEventID event)
{
	PurpleConvChat *chat;

	if (flags & PURPLE_MESSAGE_DELAYED)
		return;

	chat = purple_conversation_get_chat_data(conv);
	g_return_if_fail(chat != NULL);

	if (purple_conv_chat_is_user_ignored(chat, sender))
		return;

	if (chat_nick_matches_name(conv, sender))
		return;

	if (flags & PURPLE_MESSAGE_NICK || purple_utf8_has_word(message, chat->nick))
		play_conv_event(conv, PURPLE_SOUND_CHAT_NICK);
	else
		play_conv_event(conv, event);
}

/*
 * We mute sounds for the 10 seconds after you log in so that
 * you don't get flooded with sounds when the blist shows all
 * your buddies logging in.
 */
static void
account_signon_cb(PurpleConnection *gc, gpointer data)
{
	if (mute_login_sounds_timeout != 0)
		g_source_remove(mute_login_sounds_timeout);
	mute_login_sounds = TRUE;
	mute_login_sounds_timeout = purple_timeout_add(10000, unmute_login_sounds_cb, NULL);
}

const char *
finch_sound_get_event_option(PurpleSoundEventID event)
{
	if(event >= PURPLE_NUM_SOUNDS)
		return 0;

	return sounds[event].pref;
}

const char *
finch_sound_get_event_label(PurpleSoundEventID event)
{
	if(event >= PURPLE_NUM_SOUNDS)
		return NULL;

	return sounds[event].label;
}

void *
finch_sound_get_handle()
{
	static int handle;

	return &handle;
}

static void
finch_sound_init(void)
{
	void *gnt_sound_handle = finch_sound_get_handle();
	void *blist_handle = purple_blist_get_handle();
	void *conv_handle = purple_conversations_get_handle();
#ifdef USE_GSTREAMER
	GError *error = NULL;
#endif

	purple_signal_connect(purple_connections_get_handle(), "signed-on",
						gnt_sound_handle, PURPLE_CALLBACK(account_signon_cb),
						NULL);

	purple_prefs_add_none(FINCH_PREFS_ROOT "/sound");
	purple_prefs_add_none(FINCH_PREFS_ROOT "/sound/enabled");
	purple_prefs_add_none(FINCH_PREFS_ROOT "/sound/file");
	purple_prefs_add_bool(FINCH_PREFS_ROOT "/sound/enabled/login", TRUE);
	purple_prefs_add_path(FINCH_PREFS_ROOT "/sound/file/login", "");
	purple_prefs_add_bool(FINCH_PREFS_ROOT "/sound/enabled/logout", TRUE);
	purple_prefs_add_path(FINCH_PREFS_ROOT "/sound/file/logout", "");
	purple_prefs_add_bool(FINCH_PREFS_ROOT "/sound/enabled/im_recv", TRUE);
	purple_prefs_add_path(FINCH_PREFS_ROOT "/sound/file/im_recv", "");
	purple_prefs_add_bool(FINCH_PREFS_ROOT "/sound/enabled/first_im_recv", FALSE);
	purple_prefs_add_path(FINCH_PREFS_ROOT "/sound/file/first_im_recv", "");
	purple_prefs_add_bool(FINCH_PREFS_ROOT "/sound/enabled/send_im", TRUE);
	purple_prefs_add_path(FINCH_PREFS_ROOT "/sound/file/send_im", "");
	purple_prefs_add_bool(FINCH_PREFS_ROOT "/sound/enabled/join_chat", FALSE);
	purple_prefs_add_path(FINCH_PREFS_ROOT "/sound/file/join_chat", "");
	purple_prefs_add_bool(FINCH_PREFS_ROOT "/sound/enabled/left_chat", FALSE);
	purple_prefs_add_path(FINCH_PREFS_ROOT "/sound/file/left_chat", "");
	purple_prefs_add_bool(FINCH_PREFS_ROOT "/sound/enabled/send_chat_msg", FALSE);
	purple_prefs_add_path(FINCH_PREFS_ROOT "/sound/file/send_chat_msg", "");
	purple_prefs_add_bool(FINCH_PREFS_ROOT "/sound/enabled/chat_msg_recv", FALSE);
	purple_prefs_add_path(FINCH_PREFS_ROOT "/sound/file/chat_msg_recv", "");
	purple_prefs_add_bool(FINCH_PREFS_ROOT "/sound/enabled/nick_said", FALSE);
	purple_prefs_add_path(FINCH_PREFS_ROOT "/sound/file/nick_said", "");
	purple_prefs_add_bool(FINCH_PREFS_ROOT "/sound/enabled/pounce_default", TRUE);
	purple_prefs_add_path(FINCH_PREFS_ROOT "/sound/file/pounce_default", "");
	purple_prefs_add_bool(FINCH_PREFS_ROOT "/sound/conv_focus", TRUE);
	purple_prefs_add_bool(FINCH_PREFS_ROOT "/sound/mute", FALSE);
	purple_prefs_add_path(FINCH_PREFS_ROOT "/sound/command", "");
	purple_prefs_add_string(FINCH_PREFS_ROOT "/sound/method", "automatic");
	purple_prefs_add_int(FINCH_PREFS_ROOT "/sound/volume", 50);

#ifdef USE_GSTREAMER
	purple_debug_info("sound", "Initializing sound output drivers.\n");
	if ((gst_init_failed = !gst_init_check(NULL, NULL, &error))) {
		purple_notify_error(NULL, _("GStreamer Failure"),
					_("GStreamer failed to initialize."),
					error ? error->message : "");
		if (error) {
			g_error_free(error);
			error = NULL;
		}
	}
#endif /* USE_GSTREAMER */

	purple_signal_connect(blist_handle, "buddy-signed-on",
						gnt_sound_handle, PURPLE_CALLBACK(buddy_state_cb),
						GINT_TO_POINTER(PURPLE_SOUND_BUDDY_ARRIVE));
	purple_signal_connect(blist_handle, "buddy-signed-off",
						gnt_sound_handle, PURPLE_CALLBACK(buddy_state_cb),
						GINT_TO_POINTER(PURPLE_SOUND_BUDDY_LEAVE));
	purple_signal_connect(conv_handle, "received-im-msg",
						gnt_sound_handle, PURPLE_CALLBACK(im_msg_received_cb),
						GINT_TO_POINTER(PURPLE_SOUND_RECEIVE));
	purple_signal_connect(conv_handle, "sent-im-msg",
						gnt_sound_handle, PURPLE_CALLBACK(im_msg_sent_cb),
						GINT_TO_POINTER(PURPLE_SOUND_SEND));
	purple_signal_connect(conv_handle, "chat-buddy-joined",
						gnt_sound_handle, PURPLE_CALLBACK(chat_buddy_join_cb),
						GINT_TO_POINTER(PURPLE_SOUND_CHAT_JOIN));
	purple_signal_connect(conv_handle, "chat-buddy-left",
						gnt_sound_handle, PURPLE_CALLBACK(chat_buddy_left_cb),
						GINT_TO_POINTER(PURPLE_SOUND_CHAT_LEAVE));
	purple_signal_connect(conv_handle, "sent-chat-msg",
						gnt_sound_handle, PURPLE_CALLBACK(chat_msg_sent_cb),
						GINT_TO_POINTER(PURPLE_SOUND_CHAT_YOU_SAY));
	purple_signal_connect(conv_handle, "received-chat-msg",
						gnt_sound_handle, PURPLE_CALLBACK(chat_msg_received_cb),
						GINT_TO_POINTER(PURPLE_SOUND_CHAT_SAY));
}

static void
finch_sound_uninit(void)
{
#ifdef USE_GSTREAMER
	if (!gst_init_failed)
		gst_deinit();
#endif

	purple_signals_disconnect_by_handle(finch_sound_get_handle());
}

#ifdef USE_GSTREAMER
static gboolean
bus_call (GstBus *bus, GstMessage *msg, gpointer data)
{
	GstElement *play = data;
	GError *err = NULL;

	switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_EOS:
		gst_element_set_state(play, GST_STATE_NULL);
		gst_object_unref(GST_OBJECT(play));
		break;
	case GST_MESSAGE_ERROR:
		gst_message_parse_error(msg, &err, NULL);
		purple_debug_error("gstreamer", err->message);
		g_error_free(err);
		break;
	case GST_MESSAGE_WARNING:
		gst_message_parse_warning(msg, &err, NULL);
		purple_debug_warning("gstreamer", err->message);
		g_error_free(err);
		break;
	default:
		break;
	}
	return TRUE;
}
#endif

static void
finch_sound_play_file(const char *filename)
{
	const char *method;
#ifdef USE_GSTREAMER
	float volume;
	char *uri;
	GstElement *sink = NULL;
	GstElement *play = NULL;
	GstBus *bus = NULL;
#endif
	if (purple_prefs_get_bool(FINCH_PREFS_ROOT "/sound/mute"))
		return;

	method = purple_prefs_get_string(FINCH_PREFS_ROOT "/sound/method");

	if (!strcmp(method, "none")) {
		return;
	} else if (!strcmp(method, "beep")) {
		beep();
		return;
	}

	if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
		purple_debug_error("gntsound", "sound file (%s) does not exist.\n", filename);
		return;
	}

#ifndef _WIN32
	if (!strcmp(method, "custom")) {
		const char *sound_cmd;
		char *command;
		char *esc_filename;
		GError *error = NULL;

		sound_cmd = purple_prefs_get_path(FINCH_PREFS_ROOT "/sound/command");

		if (!sound_cmd || *sound_cmd == '\0') {
			purple_debug_error("gntsound",
					 "'Command' sound method has been chosen, "
					 "but no command has been set.");
			return;
		}

		esc_filename = g_shell_quote(filename);

		if(strstr(sound_cmd, "%s"))
			command = purple_strreplace(sound_cmd, "%s", esc_filename);
		else
			command = g_strdup_printf("%s %s", sound_cmd, esc_filename);

		if(!g_spawn_command_line_async(command, &error)) {
			purple_debug_error("gntsound", "sound command could not be launched: %s\n", error->message);
			g_error_free(error);
		}

		g_free(esc_filename);
		g_free(command);
		return;
	}
#ifdef USE_GSTREAMER
	if (gst_init_failed)  /* Perhaps do beep instead? */
		return;
	volume = (float)(CLAMP(purple_prefs_get_int(FINCH_PREFS_ROOT "/sound/volume"),0,100)) / 50;
	if (!strcmp(method, "automatic")) {
		if (purple_running_gnome()) {
			sink = gst_element_factory_make("gconfaudiosink", "sink");
		}
		if (!sink)
			sink = gst_element_factory_make("autoaudiosink", "sink");
		if (!sink) {
			purple_debug_error("sound", "Unable to create GStreamer audiosink.\n");
			return;
		}
	} else if (!strcmp(method, "esd")) {
		sink = gst_element_factory_make("esdsink", "sink");
		if (!sink) {
			purple_debug_error("sound", "Unable to create GStreamer audiosink.\n");
			return;
		}
	} else if (!strcmp(method, "alsa")) {
		sink = gst_element_factory_make("alsasink", "sink");
		if (!sink) {
			purple_debug_error("sound", "Unable to create GStreamer audiosink.\n");
			return;
		}
	} else {
		purple_debug_error("sound", "Unknown sound method '%s'\n", method);
		return;
	}

	play = gst_element_factory_make("playbin", "play");
	
	if (play == NULL) {
		return;
	}
	
	uri = g_strdup_printf("file://%s", filename);

	g_object_set(G_OBJECT(play), "uri", uri,
		                     "volume", volume,
		                     "audio-sink", sink, NULL);

	bus = gst_pipeline_get_bus(GST_PIPELINE(play));
	gst_bus_add_watch(bus, bus_call, play);

	gst_element_set_state(play, GST_STATE_PLAYING);

	gst_object_unref(bus);
	g_free(uri);

#else /* USE_GSTREAMER */
	beep();
	return;
#endif /* USE_GSTREAMER */
#else /* _WIN32 */
	purple_debug_info("sound", "Playing %s\n", filename);

	if (G_WIN32_HAVE_WIDECHAR_API ()) {
		wchar_t *wc_filename = g_utf8_to_utf16(filename,
				-1, NULL, NULL, NULL);
		if (!PlaySoundW(wc_filename, NULL, SND_ASYNC | SND_FILENAME))
			purple_debug(PURPLE_DEBUG_ERROR, "sound", "Error playing sound.\n");
		g_free(wc_filename);
	} else {
		char *l_filename = g_locale_from_utf8(filename,
				-1, NULL, NULL, NULL);
		if (!PlaySoundA(l_filename, NULL, SND_ASYNC | SND_FILENAME))
			purple_debug(PURPLE_DEBUG_ERROR, "sound", "Error playing sound.\n");
		g_free(l_filename);
	}
#endif /* _WIN32 */
}

static void
finch_sound_play_event(PurpleSoundEventID event)
{
	char *enable_pref;
	char *file_pref;
	if ((event == PURPLE_SOUND_BUDDY_ARRIVE) && mute_login_sounds)
		return;

	if (event >= PURPLE_NUM_SOUNDS) {
		purple_debug_error("sound", "got request for unknown sound: %d\n", event);
		return;
	}

	enable_pref = g_strdup_printf(FINCH_PREFS_ROOT "/sound/enabled/%s",
			sounds[event].pref);
	file_pref = g_strdup_printf(FINCH_PREFS_ROOT "/sound/file/%s", sounds[event].pref);

	/* check NULL for sounds that don't have an option, ie buddy pounce */
	if (purple_prefs_get_bool(enable_pref)) {
		char *filename = g_strdup(purple_prefs_get_path(file_pref));
		if(!filename || !strlen(filename)) {
			g_free(filename);
			/* XXX Consider creating a constant for "sounds/purple" to be shared with Pidgin */
			filename = g_build_filename(DATADIR, "sounds", "purple", sounds[event].def, NULL);
		}

		purple_sound_play_file(filename, NULL);
		g_free(filename);
	}

	g_free(enable_pref);
	g_free(file_pref);
}

static void init_pref_data()
{
	GntTree *events = NULL;

	if(sound_pref_data)
		return;

	/* TODO Setup the events tree */

	sound_pref_data = g_new0(SoundPrefData,1);
	sound_pref_data->method = g_strdup(purple_prefs_get_string(FINCH_PREFS_ROOT "/sound/method"));
	sound_pref_data->command = g_strdup(purple_prefs_get_string(FINCH_PREFS_ROOT "/sound/command"));
	sound_pref_data->conv_focus = purple_prefs_get_bool(FINCH_PREFS_ROOT "/sound/conv_focus");
	sound_pref_data->while_status = purple_prefs_get_int(FINCH_PREFS_ROOT "/purple/sound/while_status");
	sound_pref_data->volume = CLAMP(purple_prefs_get_int(FINCH_PREFS_ROOT "/sound/volume"),0,100);
	sound_pref_data->events = events;

	return;
}	

static gboolean
save_cb(GntBindable *data, gpointer *win)
{
	
	gnt_widget_destroy(GNT_WIDGET(win));
	return TRUE;
}

static gboolean
cancel_cb(GntBindable *data, gpointer win)
{
	gnt_widget_destroy(GNT_WIDGET(win));
	return TRUE;
}

static void 
release_pref_window(GntBindable *data, gpointer null) 
{
	g_free(sound_pref_data->dialog);
	sound_pref_data->dialog = NULL;
}

void
finch_sounds_show_all(void)
{
	GntWidget *box;
	GntWidget *cmbox;
	GntWidget *slider;
	GntWidget *entry;
	GntWidget *chkbox;
	GntWidget *button;
	GntWidget *label;
	GntWidget *win;

	init_pref_data();

	if(sound_pref_data->dialog){
		gnt_window_present(sound_pref_data->dialog->window);
		return;
	}


	sound_pref_data->dialog = g_new0(SoundPrefDialog,1);

	sound_pref_data->dialog->window = win = gnt_window_box_new(FALSE,TRUE);
	gnt_box_set_pad(GNT_BOX(win),0);
	gnt_box_set_fill(GNT_BOX(win),FALSE);
	gnt_box_set_toplevel(GNT_BOX(win), TRUE);
	gnt_box_set_title(GNT_BOX(win),_("Sound Preferences"));
	gnt_box_set_fill(GNT_BOX(win),TRUE);
	gnt_box_set_alignment(GNT_BOX(win),GNT_ALIGN_MID);

	sound_pref_data->dialog->method = cmbox = gnt_combo_box_new();
	gnt_combo_box_add_data(GNT_COMBO_BOX(cmbox),"automatic",_("Automatic"));
	gnt_combo_box_add_data(GNT_COMBO_BOX(cmbox),"alsa","ALSA");
	gnt_combo_box_add_data(GNT_COMBO_BOX(cmbox),"esd","ESD");
	gnt_combo_box_add_data(GNT_COMBO_BOX(cmbox),"beep",_("Console Beep"));
	gnt_combo_box_add_data(GNT_COMBO_BOX(cmbox),"custom",_("Command"));
	gnt_combo_box_add_data(GNT_COMBO_BOX(cmbox),"nosound",_("No Sound"));
	gnt_combo_box_set_selected(GNT_COMBO_BOX(cmbox),sound_pref_data->method);

	label = gnt_label_new_with_format(_("Sound Method"),GNT_TEXT_FLAG_BOLD);
	gnt_box_add_widget(GNT_BOX(win),label); 
	box = gnt_hbox_new(TRUE);
	gnt_box_set_fill(GNT_BOX(box),FALSE);
	gnt_box_set_pad(GNT_BOX(box),0);
	gnt_box_add_widget(GNT_BOX(box),gnt_label_new(_("Method: ")));
	gnt_box_add_widget(GNT_BOX(box),cmbox);
	gnt_box_add_widget(GNT_BOX(win),box); 

	box = gnt_hbox_new(TRUE);
	gnt_box_set_pad(GNT_BOX(box),0);
	gnt_box_set_fill(GNT_BOX(box),FALSE);
	gnt_box_add_widget(GNT_BOX(box),gnt_label_new(_("Sound Command\n(%s for filename)")));
	sound_pref_data->dialog->command = entry = gnt_entry_new(sound_pref_data->command);
	gnt_box_add_widget(GNT_BOX(box),entry);
	gnt_box_add_widget(GNT_BOX(win),box);

	gnt_box_add_widget(GNT_BOX(win), gnt_line_new(FALSE));

	gnt_box_add_widget(GNT_BOX(win),gnt_label_new_with_format(_("Sound Options"),GNT_TEXT_FLAG_BOLD)); 
	sound_pref_data->dialog->conv_focus = chkbox = gnt_check_box_new("Sounds when conversation has focus");
	gnt_check_box_set_checked(GNT_CHECK_BOX(chkbox),sound_pref_data->conv_focus);
	gnt_box_add_widget(GNT_BOX(win),chkbox);

	box = gnt_hbox_new(TRUE);
	gnt_box_set_pad(GNT_BOX(box),0);
	gnt_box_set_fill(GNT_BOX(box),FALSE);
	gnt_box_add_widget(GNT_BOX(box),gnt_label_new("Enable Sounds:"));
	sound_pref_data->dialog->while_status = cmbox = gnt_combo_box_new();
	gnt_combo_box_add_data(GNT_COMBO_BOX(cmbox),"always","Always");
	gnt_combo_box_add_data(GNT_COMBO_BOX(cmbox),"available","Only when available");
	gnt_combo_box_add_data(GNT_COMBO_BOX(cmbox),"away","Only when not available");
	switch(sound_pref_data->while_status){
		case 1:gnt_combo_box_set_selected(GNT_COMBO_BOX(cmbox),"available");break;
		case 2:gnt_combo_box_set_selected(GNT_COMBO_BOX(cmbox),"away");break;
		default:gnt_combo_box_set_selected(GNT_COMBO_BOX(cmbox),"always");break;
	}
	gnt_box_add_widget(GNT_BOX(box),cmbox);
	gnt_box_add_widget(GNT_BOX(win),box);

	box = gnt_hbox_new(TRUE);
	gnt_box_set_pad(GNT_BOX(box),0);
	gnt_box_set_fill(GNT_BOX(box),FALSE);
	gnt_box_add_widget(GNT_BOX(box),gnt_label_new("Volume(0-100):"));

	sound_pref_data->dialog->volume = slider = gnt_slider_new(FALSE,100,0);
	gnt_slider_set_step(GNT_SLIDER(slider),5);
	label = gnt_label_new("");
	gnt_slider_reflect_label(GNT_SLIDER(slider),GNT_LABEL(label));
	gnt_slider_set_value(GNT_SLIDER(slider),sound_pref_data->volume);
	gnt_box_set_pad(GNT_BOX(box),1);
	gnt_box_add_widget(GNT_BOX(box),slider);
	gnt_box_add_widget(GNT_BOX(box),label);
	gnt_box_add_widget(GNT_BOX(win),box);

	gnt_box_add_widget(GNT_BOX(win), gnt_line_new(FALSE));

	gnt_box_add_widget(GNT_BOX(win),gnt_label_new_with_format(_("Sound Events"),GNT_TEXT_FLAG_BOLD)); 
	/* Put events tree here */

	box = gnt_hbox_new(TRUE);
	gnt_box_set_pad(GNT_BOX(box),0);
	gnt_box_set_fill(GNT_BOX(box),TRUE);
	button = gnt_button_new("Save");
	g_signal_connect(G_OBJECT(button),"activate",G_CALLBACK(save_cb),NULL);
	gnt_box_add_widget(GNT_BOX(box),button);
	button = gnt_button_new("Cancel");
	g_signal_connect(G_OBJECT(button),"activate",G_CALLBACK(cancel_cb),NULL);
	gnt_box_add_widget(GNT_BOX(box),button);
	gnt_box_add_widget(GNT_BOX(win),box);


	g_signal_connect(G_OBJECT(win),"destroy",G_CALLBACK(release_pref_window),NULL);

	gnt_widget_show(win);

}	

static PurpleSoundUiOps sound_ui_ops =
{
	finch_sound_init,
	finch_sound_uninit,
	finch_sound_play_file,
	finch_sound_play_event,
	NULL,
	NULL,
	NULL,
	NULL
};

PurpleSoundUiOps *
finch_sound_get_ui_ops(void)
{
	return &sound_ui_ops;
}
