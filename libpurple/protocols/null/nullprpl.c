/**
 * purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * Nullprpl is a mock protocol plugin for Pidgin and libpurple. You can create
 * accounts with it, sign on and off, add buddies, and send and receive IMs,
 * all without connecting to a server!
 *
 * Beyond that basic functionality, nullprpl supports presence and
 * away/available messages, offline messages, user info, typing notification,
 * privacy allow/block lists, chat rooms, whispering, room lists, and protocol
 * icons and emblems. Notable missing features are file transfer and account
 * registration and authentication.
 *
 * Nullprpl is intended as an example of how to write a libpurple protocol
 * plugin. It doesn't contain networking code or an event loop, but it does
 * demonstrate how to use the libpurple API to do pretty much everything a prpl
 * might need to do.
 *
 * Nullprpl is also a useful tool for hacking on Pidgin, Finch, and other
 * libpurple clients. It's a full-featured protocol plugin, but doesn't depend
 * on an external server, so it's a quick and easy way to exercise test new
 * code. It also allows you to work while you're disconnected.
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

#include <stdarg.h>
#include <string.h>
#include <time.h>

#include <glib.h>

/* If you're using this as the basis of a prpl that will be distributed
 * separately from libpurple, remove the internal.h include below and replace
 * it with code to include your own config.h or similar.  If you're going to
 * provide for translation, you'll also need to setup the gettext macros. */
#include "internal.h"

#include "account.h"
#include "accountopt.h"
#include "buddylist.h"
#include "cmds.h"
#include "conversation.h"
#include "connection.h"
#include "debug.h"
#include "notify.h"
#include "plugins.h"
#include "protocol.h"
#include "roomlist.h"
#include "status.h"
#include "util.h"
#include "version.h"

#define NULLPRPL_ID "prpl-null"
static PurpleProtocol *_null_protocol = NULL;

#define NULL_STATUS_ONLINE   "online"
#define NULL_STATUS_AWAY     "away"
#define NULL_STATUS_OFFLINE  "offline"

typedef void (*GcFunc)(PurpleConnection *from,
                       PurpleConnection *to,
                       gpointer userdata);

typedef struct {
  GcFunc fn;
  PurpleConnection *from;
  gpointer userdata;
} GcFuncData;

/*
 * stores offline messages that haven't been delivered yet. maps username
 * (char *) to GList * of GOfflineMessages. initialized in plugin_load.
 */
GHashTable* goffline_messages = NULL;

typedef struct {
  char *from;
  char *message;
  time_t mtime;
  PurpleMessageFlags flags;
} GOfflineMessage;

/*
 * helpers
 */
static PurpleConnection *get_nullprpl_gc(const char *username) {
  PurpleAccount *acct = purple_accounts_find(username, NULLPRPL_ID);
  if (acct && purple_account_is_connected(acct))
    return purple_account_get_connection(acct);
  else
    return NULL;
}

static void call_if_nullprpl(gpointer data, gpointer userdata) {
  PurpleConnection *gc = (PurpleConnection *)(data);
  GcFuncData *gcfdata = (GcFuncData *)userdata;

  if (!strcmp(purple_account_get_protocol_id(purple_connection_get_account(gc)), NULLPRPL_ID))
    gcfdata->fn(gcfdata->from, gc, gcfdata->userdata);
}

static void foreach_nullprpl_gc(GcFunc fn, PurpleConnection *from,
                                gpointer userdata) {
  GcFuncData gcfdata = { fn, from, userdata };
  g_list_foreach(purple_connections_get_all(), call_if_nullprpl,
                 &gcfdata);
}


typedef void(*ChatFunc)(PurpleChatConversation *from, PurpleChatConversation *to,
                        int id, const char *room, gpointer userdata);

typedef struct {
  ChatFunc fn;
  PurpleChatConversation *from_chat;
  gpointer userdata;
} ChatFuncData;

static void call_chat_func(gpointer data, gpointer userdata) {
  PurpleConnection *to = (PurpleConnection *)data;
  ChatFuncData *cfdata = (ChatFuncData *)userdata;

  int id = purple_chat_conversation_get_id(cfdata->from_chat);
  PurpleChatConversation *chat = purple_conversations_find_chat(to, id);
  if (chat)
    cfdata->fn(cfdata->from_chat, chat, id,
               purple_conversation_get_name(PURPLE_CONVERSATION(chat)), cfdata->userdata);
}

static void foreach_gc_in_chat(ChatFunc fn, PurpleConnection *from,
                               int id, gpointer userdata) {
  PurpleChatConversation *chat = purple_conversations_find_chat(from, id);
  ChatFuncData cfdata = { fn,
                          chat,
                          userdata };

  g_list_foreach(purple_connections_get_all(), call_chat_func,
                 &cfdata);
}


static void discover_status(PurpleConnection *from, PurpleConnection *to,
                            gpointer userdata) {
  const char *from_username = purple_account_get_username(purple_connection_get_account(from));
  const char *to_username = purple_account_get_username(purple_connection_get_account(to));

  if (purple_blist_find_buddy(purple_connection_get_account(from), to_username)) {
    PurpleStatus *status = purple_account_get_active_status(purple_connection_get_account(to));
    const char *status_id = purple_status_get_id(status);
    const char *message = purple_status_get_attr_string(status, "message");

    if (!strcmp(status_id, NULL_STATUS_ONLINE) ||
        !strcmp(status_id, NULL_STATUS_AWAY) ||
        !strcmp(status_id, NULL_STATUS_OFFLINE)) {
      purple_debug_info("nullprpl", "%s sees that %s is %s: %s\n",
                        from_username, to_username, status_id, message);
      purple_protocol_got_user_status(purple_connection_get_account(from), to_username, status_id,
                                  (message) ? "message" : NULL, message, NULL);
    } else {
      purple_debug_error("nullprpl",
                         "%s's buddy %s has an unknown status: %s, %s",
                         from_username, to_username, status_id, message);
    }
  }
}

static void report_status_change(PurpleConnection *from, PurpleConnection *to,
                                 gpointer userdata) {
  purple_debug_info("nullprpl", "notifying %s that %s changed status\n",
                    purple_account_get_username(purple_connection_get_account(to)), purple_account_get_username(purple_connection_get_account(from)));
  discover_status(to, from, NULL);
}


/*
 * UI callbacks
 */
static void nullprpl_input_user_info(PurpleProtocolAction *action)
{
  PurpleConnection *gc = action->connection;
  PurpleAccount *acct = purple_connection_get_account(gc);
  purple_debug_info("nullprpl", "showing 'Set User Info' dialog for %s\n",
                    purple_account_get_username(acct));

  purple_account_request_change_user_info(acct);
}

/*
 * prpl functions
 */
static GList *nullprpl_get_actions(PurpleConnection *gc)
{
  PurpleProtocolAction *action = purple_protocol_action_new(
    _("Set User Info..."), nullprpl_input_user_info);
  return g_list_append(NULL, action);
}

static const char *nullprpl_list_icon(PurpleAccount *acct, PurpleBuddy *buddy)
{
  return "null";
}

static char *nullprpl_status_text(PurpleBuddy *buddy) {
  purple_debug_info("nullprpl", "getting %s's status text for %s\n",
                    purple_buddy_get_name(buddy),
                    purple_account_get_username(purple_buddy_get_account(buddy)));

  if (purple_blist_find_buddy(purple_buddy_get_account(buddy), purple_buddy_get_name(buddy))) {
    PurplePresence *presence = purple_buddy_get_presence(buddy);
    PurpleStatus *status = purple_presence_get_active_status(presence);
    const char *name = purple_status_get_name(status);
    const char *message = purple_status_get_attr_string(status, "message");

    char *text;
    if (message && *message)
      text = g_strdup_printf("%s: %s", name, message);
    else
      text = g_strdup(name);

    purple_debug_info("nullprpl", "%s's status text is %s\n",
                      purple_buddy_get_name(buddy), text);
    return text;

  } else {
    purple_debug_info("nullprpl", "...but %s is not logged in\n", purple_buddy_get_name(buddy));
    return g_strdup("Not logged in");
  }
}

static void nullprpl_tooltip_text(PurpleBuddy *buddy,
                                  PurpleNotifyUserInfo *info,
                                  gboolean full) {
  PurpleConnection *gc = get_nullprpl_gc(purple_buddy_get_name(buddy));

  if (gc) {
    /* they're logged in */
    PurplePresence *presence = purple_buddy_get_presence(buddy);
    PurpleStatus *status = purple_presence_get_active_status(presence);
    char *msg = nullprpl_status_text(buddy);
	/* TODO: Check whether it's correct to call add_pair_html,
	         or if we should be using add_pair_plaintext */
    purple_notify_user_info_add_pair_html(info, purple_status_get_name(status),
                                     msg);
    g_free(msg);

    if (full) {
      const char *user_info = purple_account_get_user_info(purple_connection_get_account(gc));
      if (user_info)
		/* TODO: Check whether it's correct to call add_pair_html,
		         or if we should be using add_pair_plaintext */
        purple_notify_user_info_add_pair_html(info, _("User info"), user_info);
    }

  } else {
    /* they're not logged in */
    purple_notify_user_info_add_pair_plaintext(info, _("User info"), _("not logged in"));
  }

  purple_debug_info("nullprpl", "showing %s tooltip for %s\n",
                    (full) ? "full" : "short", purple_buddy_get_name(buddy));
}

static GList *nullprpl_status_types(PurpleAccount *acct)
{
  GList *types = NULL;
  PurpleStatusType *type;

  purple_debug_info("nullprpl", "returning status types for %s: %s, %s, %s\n",
                    purple_account_get_username(acct),
                    NULL_STATUS_ONLINE, NULL_STATUS_AWAY, NULL_STATUS_OFFLINE);

  type = purple_status_type_new_with_attrs(PURPLE_STATUS_AVAILABLE,
      NULL_STATUS_ONLINE, NULL, TRUE, TRUE, FALSE,
      "message", _("Message"), purple_g_value_new(G_TYPE_STRING),
      NULL);
  types = g_list_prepend(types, type);

  type = purple_status_type_new_with_attrs(PURPLE_STATUS_AWAY,
      NULL_STATUS_AWAY, NULL, TRUE, TRUE, FALSE,
      "message", _("Message"), purple_g_value_new(G_TYPE_STRING),
      NULL);
  types = g_list_prepend(types, type);

  type = purple_status_type_new_with_attrs(PURPLE_STATUS_OFFLINE,
      NULL_STATUS_OFFLINE, NULL, TRUE, TRUE, FALSE,
      "message", _("Message"), purple_g_value_new(G_TYPE_STRING),
      NULL);
  types = g_list_prepend(types, type);

  return g_list_reverse(types);
}

static void blist_example_menu_item(PurpleBlistNode *node, gpointer userdata) {
  purple_debug_info("nullprpl", "example menu item clicked on user %s\n",
                    purple_buddy_get_name(PURPLE_BUDDY(node)));

  purple_notify_info(NULL,  /* plugin handle or PurpleConnection */
                     _("Primary title"),
                     _("Secondary title"),
                     _("This is the callback for the nullprpl menu item."));
}

static GList *nullprpl_blist_node_menu(PurpleBlistNode *node) {
  purple_debug_info("nullprpl", "providing buddy list context menu item\n");

  if (PURPLE_IS_BUDDY(node)) {
    PurpleMenuAction *action = purple_menu_action_new(
      _("Nullprpl example menu item"),
      PURPLE_CALLBACK(blist_example_menu_item),
      NULL,   /* userdata passed to the callback */
      NULL);  /* child menu items */
    return g_list_append(NULL, action);
  } else {
    return NULL;
  }
}

static GList *nullprpl_chat_info(PurpleConnection *gc) {
  struct proto_chat_entry *pce; /* defined in protocol.h */

  purple_debug_info("nullprpl", "returning chat setting 'room'\n");

  pce = g_new0(struct proto_chat_entry, 1);
  pce->label = _("Chat _room");
  pce->identifier = "room";
  pce->required = TRUE;

  return g_list_append(NULL, pce);
}

static GHashTable *nullprpl_chat_info_defaults(PurpleConnection *gc,
                                               const char *room) {
  GHashTable *defaults;

  purple_debug_info("nullprpl", "returning chat default setting "
                    "'room' = 'default'\n");

  defaults = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
  g_hash_table_insert(defaults, "room", g_strdup("default"));
  return defaults;
}

static void nullprpl_login(PurpleAccount *acct)
{
  PurpleConnection *gc = purple_account_get_connection(acct);
  GList *offline_messages;

  purple_debug_info("nullprpl", "logging in %s\n", purple_account_get_username(acct));

  purple_connection_update_progress(gc, _("Connecting"),
                                    0,   /* which connection step this is */
                                    2);  /* total number of steps */

  purple_connection_update_progress(gc, _("Connected"),
                                    1,   /* which connection step this is */
                                    2);  /* total number of steps */
  purple_connection_set_state(gc, PURPLE_CONNECTION_CONNECTED);

  /* tell purple about everyone on our buddy list who's connected */
  foreach_nullprpl_gc(discover_status, gc, NULL);

  /* notify other nullprpl accounts */
  foreach_nullprpl_gc(report_status_change, gc, NULL);

  /* fetch stored offline messages */
  purple_debug_info("nullprpl", "checking for offline messages for %s\n",
                    purple_account_get_username(acct));
  offline_messages = g_hash_table_lookup(goffline_messages, purple_account_get_username(acct));
  while (offline_messages) {
    GOfflineMessage *message = (GOfflineMessage *)offline_messages->data;
    purple_debug_info("nullprpl", "delivering offline message to %s: %s\n",
                      purple_account_get_username(acct), message->message);
    serv_got_im(gc, message->from, message->message, message->flags,
                message->mtime);
    offline_messages = g_list_next(offline_messages);

    g_free(message->from);
    g_free(message->message);
    g_free(message);
  }

  g_list_free(offline_messages);
  g_hash_table_remove(goffline_messages, purple_account_get_username(acct));
}

static void nullprpl_close(PurpleConnection *gc)
{
  /* notify other nullprpl accounts */
  foreach_nullprpl_gc(report_status_change, gc, NULL);
}

static int nullprpl_send_im(PurpleConnection *gc, const char *who,
                            const char *message, PurpleMessageFlags flags)
{
  const char *from_username = purple_account_get_username(purple_connection_get_account(gc));
  PurpleMessageFlags receive_flags = ((flags & ~PURPLE_MESSAGE_SEND)
                                      | PURPLE_MESSAGE_RECV);
  PurpleAccount *to_acct = purple_accounts_find(who, NULLPRPL_ID);
  PurpleConnection *to;

  purple_debug_info("nullprpl", "sending message from %s to %s: %s\n",
                    from_username, who, message);

  /* is the sender blocked by the recipient's privacy settings? */
  if (to_acct &&
      !purple_account_privacy_check(to_acct, purple_account_get_username(purple_connection_get_account(gc)))) {
    char *msg = g_strdup_printf(
      _("Your message was blocked by %s's privacy settings."), who);
    purple_debug_info("nullprpl",
                      "discarding; %s is blocked by %s's privacy settings\n",
                      from_username, who);
    purple_conversation_present_error(who, purple_connection_get_account(gc), msg);
    g_free(msg);
    return 0;
  }

  /* is the recipient online? */
  to = get_nullprpl_gc(who);
  if (to) {  /* yes, send */
    serv_got_im(to, from_username, message, receive_flags, time(NULL));

  } else {  /* nope, store as an offline message */
    GOfflineMessage *offline_message;
    GList *messages;

    purple_debug_info("nullprpl",
                      "%s is offline, sending as offline message\n", who);
    offline_message = g_new0(GOfflineMessage, 1);
    offline_message->from = g_strdup(from_username);
    offline_message->message = g_strdup(message);
    offline_message->mtime = time(NULL);
    offline_message->flags = receive_flags;

    messages = g_hash_table_lookup(goffline_messages, who);
    messages = g_list_append(messages, offline_message);
    g_hash_table_insert(goffline_messages, g_strdup(who), messages);
  }

   return 1;
}

static void nullprpl_set_info(PurpleConnection *gc, const char *info) {
  purple_debug_info("nullprpl", "setting %s's user info to %s\n",
                    purple_account_get_username(purple_connection_get_account(gc)), info);
}

static const char *typing_state_to_string(PurpleIMTypingState typing) {
  switch (typing) {
  case PURPLE_IM_NOT_TYPING:  return "is not typing";
  case PURPLE_IM_TYPING:      return "is typing";
  case PURPLE_IM_TYPED:       return "stopped typing momentarily";
  default:               return "unknown typing state";
  }
}

static void notify_typing(PurpleConnection *from, PurpleConnection *to,
                          gpointer typing) {
  const char *from_username = purple_account_get_username(purple_connection_get_account(from));
  const char *action = typing_state_to_string((PurpleIMTypingState)typing);
  purple_debug_info("nullprpl", "notifying %s that %s %s\n",
                    purple_account_get_username(purple_connection_get_account(to)), from_username, action);

  serv_got_typing(to,
                  from_username,
                  0, /* if non-zero, a timeout in seconds after which to
                      * reset the typing status to PURPLE_IM_NOT_TYPING */
                  (PurpleIMTypingState)typing);
}

static unsigned int nullprpl_send_typing(PurpleConnection *gc, const char *name,
                                         PurpleIMTypingState typing) {
  purple_debug_info("nullprpl", "%s %s\n", purple_account_get_username(purple_connection_get_account(gc)),
                    typing_state_to_string(typing));
  foreach_nullprpl_gc(notify_typing, gc, (gpointer)typing);
  return 0;
}

static void nullprpl_get_info(PurpleConnection *gc, const char *username) {
  const char *body;
  PurpleNotifyUserInfo *info = purple_notify_user_info_new();
  PurpleAccount *acct;

  purple_debug_info("nullprpl", "Fetching %s's user info for %s\n", username,
                    purple_account_get_username(purple_connection_get_account(gc)));

  if (!get_nullprpl_gc(username)) {
    char *msg = g_strdup_printf(_("%s is not logged in."), username);
    purple_notify_error(gc, _("User Info"), _("User info not available. "), msg);
    g_free(msg);
  }

  acct = purple_accounts_find(username, NULLPRPL_ID);
  if (acct)
    body = purple_account_get_user_info(acct);
  else
    body = _("No user info.");
  /* TODO: Check whether it's correct to call add_pair_html,
           or if we should be using add_pair_plaintext */
  purple_notify_user_info_add_pair_html(info, "Info", body);

  /* show a buddy's user info in a nice dialog box */
  purple_notify_userinfo(gc,        /* connection the buddy info came through */
                         username,  /* buddy's username */
                         info,      /* body */
                         NULL,      /* callback called when dialog closed */
                         NULL);     /* userdata for callback */
}

static void nullprpl_set_status(PurpleAccount *acct, PurpleStatus *status) {
  const char *msg = purple_status_get_attr_string(status, "message");
  purple_debug_info("nullprpl", "setting %s's status to %s: %s\n",
                    purple_account_get_username(acct), purple_status_get_name(status), msg);

  foreach_nullprpl_gc(report_status_change, get_nullprpl_gc(purple_account_get_username(acct)),
                      NULL);
}

static void nullprpl_set_idle(PurpleConnection *gc, int idletime) {
  purple_debug_info("nullprpl",
                    "purple reports that %s has been idle for %d seconds\n",
                    purple_account_get_username(purple_connection_get_account(gc)), idletime);
}

static void nullprpl_change_passwd(PurpleConnection *gc, const char *old_pass,
                                   const char *new_pass) {
  purple_debug_info("nullprpl", "%s wants to change their password\n",
                    purple_account_get_username(purple_connection_get_account(gc)));
}

static void nullprpl_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy,
                               PurpleGroup *group, const char *message)
{
  const char *username = purple_account_get_username(purple_connection_get_account(gc));
  PurpleConnection *buddy_gc = get_nullprpl_gc(purple_buddy_get_name(buddy));

  purple_debug_info("nullprpl", "adding %s to %s's buddy list\n", purple_buddy_get_name(buddy),
                    username);

  if (buddy_gc) {
    PurpleAccount *buddy_acct = purple_connection_get_account(buddy_gc);

    discover_status(gc, buddy_gc, NULL);

    if (purple_blist_find_buddy(buddy_acct, username)) {
      purple_debug_info("nullprpl", "%s is already on %s's buddy list\n",
                        username, purple_buddy_get_name(buddy));
    } else {
      purple_debug_info("nullprpl", "asking %s if they want to add %s\n",
                        purple_buddy_get_name(buddy), username);
      purple_account_request_add(buddy_acct,
                                 username,
                                 NULL,   /* local account id (rarely used) */
                                 NULL,   /* alias */
                                 message);  /* message */
    }
  }
}

static void nullprpl_add_buddies(PurpleConnection *gc, GList *buddies,
                                 GList *groups, const char *message) {
  GList *buddy = buddies;
  GList *group = groups;

  purple_debug_info("nullprpl", "adding multiple buddies\n");

  while (buddy && group) {
    nullprpl_add_buddy(gc, (PurpleBuddy *)buddy->data, (PurpleGroup *)group->data, message);
    buddy = g_list_next(buddy);
    group = g_list_next(group);
  }
}

static void nullprpl_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy,
                                  PurpleGroup *group)
{
  purple_debug_info("nullprpl", "removing %s from %s's buddy list\n",
                    purple_buddy_get_name(buddy),
                    purple_account_get_username(purple_connection_get_account(gc)));
}

static void nullprpl_remove_buddies(PurpleConnection *gc, GList *buddies,
                                    GList *groups) {
  GList *buddy = buddies;
  GList *group = groups;

  purple_debug_info("nullprpl", "removing multiple buddies\n");

  while (buddy && group) {
    nullprpl_remove_buddy(gc, (PurpleBuddy *)buddy->data,
                          (PurpleGroup *)group->data);
    buddy = g_list_next(buddy);
    group = g_list_next(group);
  }
}

/*
 * nullprpl uses purple's local whitelist and blacklist, stored in blist.xml, as
 * its authoritative privacy settings, and uses purple's logic (specifically
 * purple_privacy_check(), from privacy.h), to determine whether messages are
 * allowed or blocked.
 */
static void nullprpl_add_permit(PurpleConnection *gc, const char *name) {
  purple_debug_info("nullprpl", "%s adds %s to their allowed list\n",
                    purple_account_get_username(purple_connection_get_account(gc)), name);
}

static void nullprpl_add_deny(PurpleConnection *gc, const char *name) {
  purple_debug_info("nullprpl", "%s adds %s to their blocked list\n",
                    purple_account_get_username(purple_connection_get_account(gc)), name);
}

static void nullprpl_rem_permit(PurpleConnection *gc, const char *name) {
  purple_debug_info("nullprpl", "%s removes %s from their allowed list\n",
                    purple_account_get_username(purple_connection_get_account(gc)), name);
}

static void nullprpl_rem_deny(PurpleConnection *gc, const char *name) {
  purple_debug_info("nullprpl", "%s removes %s from their blocked list\n",
                    purple_account_get_username(purple_connection_get_account(gc)), name);
}

static void nullprpl_set_permit_deny(PurpleConnection *gc) {
  /* this is for synchronizing the local black/whitelist with the server.
   * for nullprpl, it's a noop.
   */
}

static void joined_chat(PurpleChatConversation *from, PurpleChatConversation *to,
                        int id, const char *room, gpointer userdata) {
  /*  tell their chat window that we joined */
  purple_debug_info("nullprpl", "%s sees that %s joined chat room %s\n",
                    purple_chat_conversation_get_nick(to), purple_chat_conversation_get_nick(from), room);
  purple_chat_conversation_add_user(to,
                            purple_chat_conversation_get_nick(from),
                            NULL,   /* user-provided join message, IRC style */
                            PURPLE_CHAT_USER_NONE,
                            TRUE);  /* show a join message */

  if (from != to) {
    /* add them to our chat window */
    purple_debug_info("nullprpl", "%s sees that %s is in chat room %s\n",
                      purple_chat_conversation_get_nick(from), purple_chat_conversation_get_nick(to), room);
    purple_chat_conversation_add_user(from,
                              purple_chat_conversation_get_nick(to),
                              NULL,   /* user-provided join message, IRC style */
                              PURPLE_CHAT_USER_NONE,
                              FALSE);  /* show a join message */
  }
}

static void nullprpl_join_chat(PurpleConnection *gc, GHashTable *components) {
  const char *username = purple_account_get_username(purple_connection_get_account(gc));
  const char *room = g_hash_table_lookup(components, "room");
  int chat_id = g_str_hash(room);
  purple_debug_info("nullprpl", "%s is joining chat room %s\n", username, room);

  if (!purple_conversations_find_chat(gc, chat_id)) {
    serv_got_joined_chat(gc, chat_id, room);

    /* tell everyone that we joined, and add them if they're already there */
    foreach_gc_in_chat(joined_chat, gc, chat_id, NULL);
  } else {
    char *tmp = g_strdup_printf(_("%s is already in chat room %s."),
                                username,
                                room);
    purple_debug_info("nullprpl", "%s is already in chat room %s\n", username,
                      room);
    purple_notify_info(gc, _("Join chat"), _("Join chat"), tmp);
    g_free(tmp);
  }
}

static void nullprpl_reject_chat(PurpleConnection *gc, GHashTable *components) {
  const char *invited_by = g_hash_table_lookup(components, "invited_by");
  const char *room = g_hash_table_lookup(components, "room");
  const char *username = purple_account_get_username(purple_connection_get_account(gc));
  PurpleConnection *invited_by_gc = get_nullprpl_gc(invited_by);
  char *message = g_strdup_printf(
    "%s %s %s.",
    username,
    _("has rejected your invitation to join the chat room"),
    room);

  purple_debug_info("nullprpl",
                    "%s has rejected %s's invitation to join chat room %s\n",
                    username, invited_by, room);

  purple_notify_info(invited_by_gc,
                     _("Chat invitation rejected"),
                     _("Chat invitation rejected"),
                     message);
  g_free(message);
}

static char *nullprpl_get_chat_name(GHashTable *components) {
  const char *room = g_hash_table_lookup(components, "room");
  purple_debug_info("nullprpl", "reporting chat room name '%s'\n", room);
  return g_strdup(room);
}

static void nullprpl_chat_invite(PurpleConnection *gc, int id,
                                 const char *message, const char *who) {
  const char *username = purple_account_get_username(purple_connection_get_account(gc));
  PurpleChatConversation *chat = purple_conversations_find_chat(gc, id);
  const char *room = purple_conversation_get_name(PURPLE_CONVERSATION(chat));
  PurpleAccount *to_acct = purple_accounts_find(who, NULLPRPL_ID);

  purple_debug_info("nullprpl", "%s is inviting %s to join chat room %s\n",
                    username, who, room);

  if (to_acct) {
    PurpleChatConversation *to_conv = purple_conversations_find_chat(purple_account_get_connection(to_acct), id);
    if (to_conv) {
      char *tmp = g_strdup_printf("%s is already in chat room %s.", who, room);
      purple_debug_info("nullprpl",
                        "%s is already in chat room %s; "
                        "ignoring invitation from %s\n",
                        who, room, username);
      purple_notify_info(gc, _("Chat invitation"), _("Chat invitation"), tmp);
      g_free(tmp);
    } else {
      GHashTable *components;
      components = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
      g_hash_table_replace(components, "room", g_strdup(room));
      g_hash_table_replace(components, "invited_by", g_strdup(username));
      serv_got_chat_invite(purple_account_get_connection(to_acct), room, username, message, components);
    }
  }
}

static void left_chat_room(PurpleChatConversation *from, PurpleChatConversation *to,
                           int id, const char *room, gpointer userdata) {
  if (from != to) {
    /*  tell their chat window that we left */
    purple_debug_info("nullprpl", "%s sees that %s left chat room %s\n",
                      purple_chat_conversation_get_nick(to), purple_chat_conversation_get_nick(from), room);
    purple_chat_conversation_remove_user(to,
                                 purple_chat_conversation_get_nick(from),
                                 NULL);  /* user-provided message, IRC style */
  }
}

static void nullprpl_chat_leave(PurpleConnection *gc, int id) {
  PurpleChatConversation *chat = purple_conversations_find_chat(gc, id);
  purple_debug_info("nullprpl", "%s is leaving chat room %s\n",
                    purple_account_get_username(purple_connection_get_account(gc)),
                    purple_conversation_get_name(PURPLE_CONVERSATION(chat)));

  /* tell everyone that we left */
  foreach_gc_in_chat(left_chat_room, gc, id, NULL);
}

static PurpleCmdRet send_whisper(PurpleConversation *conv, const gchar *cmd,
                                 gchar **args, gchar **error, void *userdata) {
  const char *to_username;
  const char *message;
  const char *from_username;
  PurpleChatUser *chat_user;
  PurpleConnection *to;

  /* parse args */
  to_username = args[0];
  message = args[1];

  if (!to_username || !*to_username) {
    *error = g_strdup(_("Whisper is missing recipient."));
    return PURPLE_CMD_RET_FAILED;
  } else if (!message || !*message) {
    *error = g_strdup(_("Whisper is missing message."));
    return PURPLE_CMD_RET_FAILED;
  }

  from_username = purple_account_get_username(purple_conversation_get_account(conv));
  purple_debug_info("nullprpl", "%s whispers to %s in chat room %s: %s\n",
                    from_username, to_username,
                    purple_conversation_get_name(conv), message);

  chat_user = purple_chat_conversation_find_user(PURPLE_CHAT_CONVERSATION(conv), to_username);
  to = get_nullprpl_gc(to_username);

  if (!chat_user) {
    /* this will be freed by the caller */
    *error = g_strdup_printf(_("%s is not logged in."), to_username);
    return PURPLE_CMD_RET_FAILED;
  } else if (!to) {
    *error = g_strdup_printf(_("%s is not in this chat room."), to_username);
    return PURPLE_CMD_RET_FAILED;
  } else {
    /* write the whisper in the sender's chat window  */
    char *message_to = g_strdup_printf("%s (to %s)", message, to_username);
    purple_conversation_write_message(conv, from_username, message_to,
                                      PURPLE_MESSAGE_SEND | PURPLE_MESSAGE_WHISPER,
                                      time(NULL));
    g_free(message_to);

    /* send the whisper */
    serv_chat_whisper(to, purple_chat_conversation_get_id(PURPLE_CHAT_CONVERSATION(conv)),
                      from_username, message);

    return PURPLE_CMD_RET_OK;
  }
}

static void nullprpl_chat_whisper(PurpleConnection *gc, int id, const char *who,
                                  const char *message) {
  const char *username = purple_account_get_username(purple_connection_get_account(gc));
  PurpleChatConversation *chat = purple_conversations_find_chat(gc, id);
  purple_debug_info("nullprpl",
                    "%s receives whisper from %s in chat room %s: %s\n",
                    username, who, purple_conversation_get_name(PURPLE_CONVERSATION(chat)),
                    message);

  /* receive whisper on recipient's account */
  serv_got_chat_in(gc, id, who, PURPLE_MESSAGE_RECV | PURPLE_MESSAGE_WHISPER,
                   message, time(NULL));
}

static void receive_chat_message(PurpleChatConversation *from, PurpleChatConversation *to,
                                 int id, const char *room, gpointer userdata) {
  const char *message = (const char *)userdata;
  PurpleConnection *to_gc = get_nullprpl_gc(purple_chat_conversation_get_nick(to));

  purple_debug_info("nullprpl",
                    "%s receives message from %s in chat room %s: %s\n",
                    purple_chat_conversation_get_nick(to), purple_chat_conversation_get_nick(from), room, message);
  serv_got_chat_in(to_gc, id, purple_chat_conversation_get_nick(from), PURPLE_MESSAGE_RECV, message,
                   time(NULL));
}

static int nullprpl_chat_send(PurpleConnection *gc, int id, const char *message,
                              PurpleMessageFlags flags) {
  const char *username = purple_account_get_username(purple_connection_get_account(gc));
  PurpleChatConversation *chat = purple_conversations_find_chat(gc, id);

  if (chat) {
    purple_debug_info("nullprpl",
                      "%s is sending message to chat room %s: %s\n", username,
                      purple_conversation_get_name(PURPLE_CONVERSATION(chat)), message);

    /* send message to everyone in the chat room */
    foreach_gc_in_chat(receive_chat_message, gc, id, (gpointer)message);
    return 0;
  } else {
    purple_debug_info("nullprpl",
                      "tried to send message from %s to chat room #%d: %s\n"
                      "but couldn't find chat room",
                      username, id, message);
    return -1;
  }
}

static void nullprpl_register_user(PurpleAccount *acct) {
 purple_debug_info("nullprpl", "registering account for %s\n",
                   purple_account_get_username(acct));
}

static void nullprpl_get_cb_info(PurpleConnection *gc, int id, const char *who) {
  PurpleChatConversation *chat = purple_conversations_find_chat(gc, id);
  purple_debug_info("nullprpl",
                    "retrieving %s's info for %s in chat room %s\n", who,
                    purple_account_get_username(purple_connection_get_account(gc)),
                    purple_conversation_get_name(PURPLE_CONVERSATION(chat)));

  nullprpl_get_info(gc, who);
}

static void nullprpl_alias_buddy(PurpleConnection *gc, const char *who,
                                 const char *alias) {
 purple_debug_info("nullprpl", "%s sets %s's alias to %s\n",
                   purple_account_get_username(purple_connection_get_account(gc)), who, alias);
}

static void nullprpl_group_buddy(PurpleConnection *gc, const char *who,
                                 const char *old_group,
                                 const char *new_group) {
  purple_debug_info("nullprpl", "%s has moved %s from group %s to group %s\n",
                    purple_account_get_username(purple_connection_get_account(gc)), who, old_group, new_group);
}

static void nullprpl_rename_group(PurpleConnection *gc, const char *old_name,
                                  PurpleGroup *group, GList *moved_buddies) {
  purple_debug_info("nullprpl", "%s has renamed group %s to %s\n",
                    purple_account_get_username(purple_connection_get_account(gc)), old_name,
                    purple_group_get_name(group));
}

static void nullprpl_convo_closed(PurpleConnection *gc, const char *who) {
  purple_debug_info("nullprpl", "%s's conversation with %s was closed\n",
                    purple_account_get_username(purple_connection_get_account(gc)), who);
}

/* normalize a username (e.g. remove whitespace, add default domain, etc.)
 * for nullprpl, this is a noop.
 */
static const char *nullprpl_normalize(const PurpleAccount *acct,
                                      const char *input) {
  return NULL;
}

static void nullprpl_set_buddy_icon(PurpleConnection *gc,
                                    PurpleStoredImage *img) {
 purple_debug_info("nullprpl", "setting %s's buddy icon to %s\n",
                   purple_account_get_username(purple_connection_get_account(gc)),
                   img ? purple_imgstore_get_filename(img) : "(null)");
}

static void nullprpl_remove_group(PurpleConnection *gc, PurpleGroup *group) {
  purple_debug_info("nullprpl", "%s has removed group %s\n",
                    purple_account_get_username(purple_connection_get_account(gc)),
                    purple_group_get_name(group));
}


static void set_chat_topic_fn(PurpleChatConversation *from, PurpleChatConversation *to,
                              int id, const char *room, gpointer userdata) {
  const char *topic = (const char *)userdata;
  const char *username = purple_account_get_username(purple_conversation_get_account(PURPLE_CONVERSATION(from)));
  char *msg;

  purple_chat_conversation_set_topic(to, username, topic);

  if (topic && *topic)
    msg = g_strdup_printf(_("%s sets topic to: %s"), username, topic);
  else
    msg = g_strdup_printf(_("%s clears topic"), username);

  purple_conversation_write_message(PURPLE_CONVERSATION(to), username, msg,
                                    PURPLE_MESSAGE_SYSTEM | PURPLE_MESSAGE_NO_LOG,
                                    time(NULL));
  g_free(msg);
}

static void nullprpl_set_chat_topic(PurpleConnection *gc, int id,
                                    const char *topic) {
  PurpleChatConversation *chat = purple_conversations_find_chat(gc, id);
  const char *last_topic;

  if (!chat)
    return;

  purple_debug_info("nullprpl", "%s sets topic of chat room '%s' to '%s'\n",
                    purple_account_get_username(purple_connection_get_account(gc)),
                    purple_conversation_get_name(PURPLE_CONVERSATION(chat)), topic);

  last_topic = purple_chat_conversation_get_topic(chat);
  if ((!topic && !last_topic) ||
      (topic && last_topic && !strcmp(topic, last_topic)))
    return;  /* topic is unchanged, this is a noop */

  foreach_gc_in_chat(set_chat_topic_fn, gc, id, (gpointer)topic);
}

static gboolean nullprpl_finish_get_roomlist(gpointer roomlist) {
  purple_roomlist_set_in_progress((PurpleRoomlist *)roomlist, FALSE);
  return FALSE;
}

static PurpleRoomlist *nullprpl_roomlist_get_list(PurpleConnection *gc) {
  const char *username = purple_account_get_username(purple_connection_get_account(gc));
  PurpleRoomlist *roomlist = purple_roomlist_new(purple_connection_get_account(gc));
  GList *fields = NULL;
  PurpleRoomlistField *field;
  GList *chats;
  GList *seen_ids = NULL;

  purple_debug_info("nullprpl", "%s asks for room list; returning:\n", username);

  /* set up the room list */
  field = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_STRING, "room",
                                    "room", TRUE /* hidden */);
  fields = g_list_append(fields, field);

  field = purple_roomlist_field_new(PURPLE_ROOMLIST_FIELD_INT, "Id", "Id", FALSE);
  fields = g_list_append(fields, field);

  purple_roomlist_set_fields(roomlist, fields);

  /* add each chat room. the chat ids are cached in seen_ids so that each room
   * is only returned once, even if multiple users are in it. */
  for (chats  = purple_conversations_get_chats(); chats; chats = g_list_next(chats)) {
    PurpleChatConversation *chat = PURPLE_CHAT_CONVERSATION(chats->data);
    PurpleRoomlistRoom *room;
    const char *name = purple_conversation_get_name(PURPLE_CONVERSATION(chat));
    int id = purple_chat_conversation_get_id(chat);

    /* have we already added this room? */
    if (g_list_find_custom(seen_ids, name, (GCompareFunc)strcmp))
      continue;                                /* yes! try the next one. */

    /* This cast is OK because this list is only staying around for the life
     * of this function and none of the conversations are being deleted
     * in that timespan. */
    seen_ids = g_list_prepend(seen_ids, (char *)name); /* no, it's new. */
    purple_debug_info("nullprpl", "%s (%d), ", name, id);

    room = purple_roomlist_room_new(PURPLE_ROOMLIST_ROOMTYPE_ROOM, name, NULL);
    purple_roomlist_room_add_field(roomlist, room, name);
    purple_roomlist_room_add_field(roomlist, room, &id);
    purple_roomlist_room_add(roomlist, room);
  }

  g_list_free(seen_ids);
  purple_timeout_add(1 /* ms */, nullprpl_finish_get_roomlist, roomlist);
  return roomlist;
}

static void nullprpl_roomlist_cancel(PurpleRoomlist *list) {
 PurpleAccount *account = purple_roomlist_get_account(list);
 purple_debug_info("nullprpl", "%s asked to cancel room list request\n",
                   purple_account_get_username(account));
}

static void nullprpl_roomlist_expand_category(PurpleRoomlist *list,
                                              PurpleRoomlistRoom *category) {
 PurpleAccount *account = purple_roomlist_get_account(list);
 purple_debug_info("nullprpl", "%s asked to expand room list category %s\n",
                   purple_account_get_username(account),
                   purple_roomlist_room_get_name(category));
}

/* nullprpl doesn't support file transfer...yet... */
static gboolean nullprpl_can_receive_file(PurpleConnection *gc,
                                          const char *who) {
  return FALSE;
}

static gboolean nullprpl_offline_message(const PurpleBuddy *buddy) {
  purple_debug_info("nullprpl",
                    "reporting that offline messages are supported for %s\n",
                    purple_buddy_get_name(buddy));
  return TRUE;
}


/*
 * Protocol stuff. see protocol.h for more information.
 */

static PurpleProtocol protocol =
{
  NULLPRPL_ID,                         /* id */
  "Null - Testing protocol",           /* name */
  sizeof(PurpleProtocol),    /* struct_size */
  OPT_PROTO_NO_PASSWORD | OPT_PROTO_CHAT_TOPIC,  /* options */
  NULL,               /* user_splits, initialized in plugin_load() */
  NULL,               /* protocol_options, initialized in plugin_load() */
  {   /* icon_spec, a PurpleBuddyIconSpec */
      "png,jpg,gif",                   /* format */
      0,                               /* min_width */
      0,                               /* min_height */
      128,                             /* max_width */
      128,                             /* max_height */
      10000,                           /* max_filesize */
      PURPLE_ICON_SCALE_DISPLAY,       /* scale_rules */
  },
  nullprpl_get_actions,                /* get_actions */
  nullprpl_list_icon,                  /* list_icon */
  NULL,                                /* list_emblem */
  nullprpl_status_text,                /* status_text */
  nullprpl_tooltip_text,               /* tooltip_text */
  nullprpl_status_types,               /* status_types */
  nullprpl_blist_node_menu,            /* blist_node_menu */
  nullprpl_chat_info,                  /* chat_info */
  nullprpl_chat_info_defaults,         /* chat_info_defaults */
  nullprpl_login,                      /* login */
  nullprpl_close,                      /* close */
  nullprpl_send_im,                    /* send_im */
  nullprpl_set_info,                   /* set_info */
  nullprpl_send_typing,                /* send_typing */
  nullprpl_get_info,                   /* get_info */
  nullprpl_set_status,                 /* set_status */
  nullprpl_set_idle,                   /* set_idle */
  nullprpl_change_passwd,              /* change_passwd */
  nullprpl_add_buddy,                  /* add_buddy */
  nullprpl_add_buddies,                /* add_buddies */
  nullprpl_remove_buddy,               /* remove_buddy */
  nullprpl_remove_buddies,             /* remove_buddies */
  nullprpl_add_permit,                 /* add_permit */
  nullprpl_add_deny,                   /* add_deny */
  nullprpl_rem_permit,                 /* rem_permit */
  nullprpl_rem_deny,                   /* rem_deny */
  nullprpl_set_permit_deny,            /* set_permit_deny */
  nullprpl_join_chat,                  /* join_chat */
  nullprpl_reject_chat,                /* reject_chat */
  nullprpl_get_chat_name,              /* get_chat_name */
  nullprpl_chat_invite,                /* chat_invite */
  nullprpl_chat_leave,                 /* chat_leave */
  nullprpl_chat_whisper,               /* chat_whisper */
  nullprpl_chat_send,                  /* chat_send */
  NULL,                                /* keepalive */
  nullprpl_register_user,              /* register_user */
  nullprpl_get_cb_info,                /* get_cb_info */
  nullprpl_alias_buddy,                /* alias_buddy */
  nullprpl_group_buddy,                /* group_buddy */
  nullprpl_rename_group,               /* rename_group */
  NULL,                                /* buddy_free */
  nullprpl_convo_closed,               /* convo_closed */
  nullprpl_normalize,                  /* normalize */
  nullprpl_set_buddy_icon,             /* set_buddy_icon */
  nullprpl_remove_group,               /* remove_group */
  NULL,                                /* get_cb_real_name */
  nullprpl_set_chat_topic,             /* set_chat_topic */
  NULL,                                /* find_blist_chat */
  nullprpl_roomlist_get_list,          /* roomlist_get_list */
  nullprpl_roomlist_cancel,            /* roomlist_cancel */
  nullprpl_roomlist_expand_category,   /* roomlist_expand_category */
  nullprpl_can_receive_file,           /* can_receive_file */
  NULL,                                /* send_file */
  NULL,                                /* new_xfer */
  nullprpl_offline_message,            /* offline_message */
  NULL,                                /* whiteboard_protocol_ops */
  NULL,                                /* send_raw */
  NULL,                                /* roomlist_room_serialize */
  NULL,                                /* unregister_user */
  NULL,                                /* send_attention */
  NULL,                                /* get_attention_types */
  NULL,                                /* get_account_text_table */
  NULL,                                /* initiate_media */
  NULL,                                /* get_media_caps */
  NULL,                                /* get_moods */
  NULL,                                /* set_public_alias */
  NULL                                 /* get_public_alias */
};

static PurplePluginInfo *
plugin_query(GError **error)
{
  return purple_plugin_info_new(
    "id",           NULLPRPL_ID,
    "name",         "Null - Testing Plugin",
    "version",      DISPLAY_VERSION,
    "category",     N_("Protocol"),
    "summary",      N_("Null Protocol Plugin"),
    "description",  N_("Null Protocol Plugin"),
    "website",      PURPLE_WEBSITE,
    "abi-version",  PURPLE_ABI_VERSION,

    /* If you're using this as the basis of a protocol plugin that will be
     * distributed separately from libpurple, do not include these flags.*/
    "flags",        GPLUGIN_PLUGIN_INFO_FLAGS_INTERNAL |
                    GPLUGIN_PLUGIN_INFO_FLAGS_LOAD_ON_QUERY,
    NULL
  );
}

static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
  /* see accountopt.h for information about user splits and protocol options */
  PurpleAccountUserSplit *split = purple_account_user_split_new(
    _("Example user split"),  /* text shown to user */
    "default",                /* default value */
    '@');                     /* field separator */
  PurpleAccountOption *option = purple_account_option_string_new(
    _("Example option"),      /* text shown to user */
    "example",                /* pref name */
    "default");               /* default value */

  purple_debug_info("nullprpl", "starting up\n");

  protocol.user_splits = g_list_append(NULL, split);
  protocol.protocol_options = g_list_append(NULL, option);

  /* register whisper chat command, /msg */
  purple_cmd_register("msg",
                    "ws",                  /* args: recipient and message */
                    PURPLE_CMD_P_DEFAULT,  /* priority */
                    PURPLE_CMD_FLAG_CHAT,
                    "prpl-null",
                    send_whisper,
                    "msg &lt;username&gt; &lt;message&gt;: send a private message, aka a whisper",
                    NULL);                 /* userdata */

  /* get ready to store offline messages */
  goffline_messages = g_hash_table_new_full(g_str_hash,  /* hash fn */
                                            g_str_equal, /* key comparison fn */
                                            g_free,      /* key free fn */
                                            NULL);       /* value free fn */

  _null_protocol = &protocol;
  purple_protocols_add(_null_protocol);

  return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
  purple_debug_info("nullprpl", "shutting down\n");

  purple_protocols_remove(_null_protocol);

  return TRUE;
}

PURPLE_PLUGIN_INIT(null, plugin_query, plugin_load, plugin_unload);
