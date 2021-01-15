/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * purple
 *
 * Copyright (C) 1998-2001, Mark Spencer <markster@marko.net>
 * Some code borrowed from GtkZephyr, by
 * Jag/Sean Dilda <agrajag@linuxpower.org>/<smdilda@unity.ncsu.edu>
 * http://gtkzephyr.linuxpower.org/
 *
 * Some code borrowed from kzephyr, by
 * Chris Colohan <colohan+@cs.cmu.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301	 USA
 *

*/

#include <glib/gi18n-lib.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <purple.h>

#include "internal.h"
#include "zephyr.h"
#include "zephyr_html.h"

#define ZEPHYR_FALLBACK_CHARSET "ISO-8859-1"

#define BUF_LEN (2048)

/* these are deliberately high, since most people don't send multiple "PING"s */
#define ZEPHYR_TYPING_SEND_TIMEOUT 15
#define ZEPHYR_TYPING_RECV_TIMEOUT 10
#define ZEPHYR_FD_READ 0
#define ZEPHYR_FD_WRITE 1

#ifndef HOST_NAME_MAX
# define HOST_NAME_MAX 255
#endif

static PurpleProtocol *my_protocol = NULL;
static GSList *cmds = NULL;

#ifdef LIBZEPHYR_EXT
extern char __Zephyr_realm[];
#endif

typedef struct _zephyr_triple zephyr_triple;
typedef struct _zephyr_account zephyr_account;

typedef gssize (*PollableInputStreamReadFunc)(GPollableInputStream *stream, void *bufcur, GError **error);
typedef gboolean (*ZephyrLoginFunc)(zephyr_account *zephyr);
typedef Code_t (*ZephyrSubscribeToFunc)(zephyr_account *zephyr, char *class, char *instance, char *recipient);

typedef enum {
	PURPLE_ZEPHYR_NONE, /* Non-kerberized ZEPH0.2 */
	PURPLE_ZEPHYR_KRB4, /* ZEPH0.2 w/ KRB4 support */
	PURPLE_ZEPHYR_TZC,  /* tzc executable proxy */
	PURPLE_ZEPHYR_INTERGALACTIC_KRB4 /* Kerberized ZEPH0.3 */
} zephyr_connection_type;

struct _zephyr_account {
	PurpleAccount* account;
	char *username;
	char *realm;
	char *encoding;
	char* galaxy; /* not yet useful */
	char* krbtkfile; /* not yet useful */
	guint32 nottimer;
	guint32 loctimer;
	GList *pending_zloc_names;
	GSList *subscrips;
	int last_id;
	unsigned short port;
	char ourhost[HOST_NAME_MAX + 1];
	char ourhostcanon[HOST_NAME_MAX + 1];
	zephyr_connection_type connection_type;
	char *exposure;
	GSubprocess *tzc_proc;
	GOutputStream *tzc_stdin;
	GInputStream *tzc_stdout;
	gchar *away;
	ZephyrSubscribeToFunc subscribe_to;
};

#define MAXCHILDREN 20

struct _zephyr_triple {
	char *class;
	char *instance;
	char *recipient;
	char *name;
	gboolean open;
	int id;
};

#ifdef WIN32
extern const char *username;
#endif

static inline gboolean
use_tzc(const zephyr_account *zephyr)
{
	return zephyr->connection_type == PURPLE_ZEPHYR_TZC;
}

static inline gboolean
use_zeph02(const zephyr_account *zephyr)
{
	return zephyr->connection_type == PURPLE_ZEPHYR_NONE ||
	       zephyr->connection_type == PURPLE_ZEPHYR_KRB4;
}

static gboolean
zephyr_write_message(zephyr_account *zephyr, const gchar *message)
{
	GError *error = NULL;
	gboolean success;

	success = g_output_stream_write_all(zephyr->tzc_stdin, message, strlen(message),
	                                    NULL, NULL, &error);
	if (!success) {
		purple_debug_error("zephyr", "Unable to write a message: %s", error->message);
		g_error_free(error);
	}
	return success;
}

static Code_t
subscribe_to_tzc(zephyr_account *zephyr, char *class, char *instance, char *recipient)
{
	Code_t ret_val = -1;
	gchar *zsubstr;

	/* ((tzcfodder . subscribe) ("class" "instance" "recipient")) */
	zsubstr = g_strdup_printf("((tzcfodder . subscribe) (\"%s\" \"%s\" \"%s\"))\n",
	                          class, instance, recipient);
	if (zephyr_write_message(zephyr, zsubstr)) {
		ret_val = ZERR_NONE;
	}
	g_free(zsubstr);
	return ret_val;
}

static Code_t
subscribe_to_zeph02(G_GNUC_UNUSED zephyr_account *zephyr, char *class, char *instance, char *recipient)
{
	ZSubscription_t sub;
	sub.zsub_class = class;
	sub.zsub_classinst = instance;
	sub.zsub_recipient = recipient;
	return ZSubscribeTo(&sub, 1, 0);
}

char *local_zephyr_normalize(zephyr_account* zephyr,const char *);
static void zephyr_chat_set_topic(PurpleConnection * gc, int id, const char *topic);
char* zephyr_tzc_deescape_str(const char *message);

static char *
zephyr_strip_local_realm(const zephyr_account *zephyr, const char *user)
{
	/*
	  Takes in a username of the form username or username@realm
	  and returns:
	  username, if there is no realm, or the realm is the local realm
	  or:
	  username@realm if there is a realm and it is foreign
	*/
	char *at = strchr(user, '@');
	if (at && !g_ascii_strcasecmp(at+1,zephyr->realm)) {
		/* We're passed in a username of the form user@users-realm */
		return g_strndup(user, at - user);
	}
	else {
		/* We're passed in a username of the form user or user@foreign-realm */
		return g_strdup(user);
	}
}

/* this is so bad, and if Zephyr weren't so fucked up to begin with I
 * wouldn't do this. but it is so i will. */

/* just for debugging */
static void handle_unknown(ZNotice_t *notice)
{
	purple_debug_error("zephyr","z_packet: %s\n", notice->z_packet);
	purple_debug_error("zephyr","z_version: %s\n", notice->z_version);
	purple_debug_error("zephyr","z_kind: %d\n", (int)(notice->z_kind));
	purple_debug_error("zephyr","z_class: %s\n", notice->z_class);
	purple_debug_error("zephyr","z_class_inst: %s\n", notice->z_class_inst);
	purple_debug_error("zephyr","z_opcode: %s\n", notice->z_opcode);
	purple_debug_error("zephyr","z_sender: %s\n", notice->z_sender);
	purple_debug_error("zephyr","z_recipient: %s\n", notice->z_recipient);
	purple_debug_error("zephyr","z_message: %s\n", notice->z_message);
	purple_debug_error("zephyr","z_message_len: %d\n", notice->z_message_len);
}


static zephyr_triple *new_triple(zephyr_account *zephyr,const char *c, const char *i, const char *r)
{
	zephyr_triple *zt;

	zt = g_new0(zephyr_triple, 1);
	zt->class = g_strdup(c);
	zt->instance = g_strdup(i);
	zt->recipient = g_strdup(r);
	zt->name = g_strdup_printf("%s,%s,%s", c, i?i:"", r?r:"");
	zt->id = ++(zephyr->last_id);
	zt->open = FALSE;
	return zt;
}

static void free_triple(zephyr_triple * zt)
{
	g_free(zt->class);
	g_free(zt->instance);
	g_free(zt->recipient);
	g_free(zt->name);
	g_free(zt);
}

/* returns true if zt1 is a subset of zt2.  This function is used to
   determine whether a zephyr sent to zt1 should be placed in the chat
   with triple zt2

   zt1 is a subset of zt2
   iff. the classnames are identical ignoring case
   AND. the instance names are identical (ignoring case), or zt2->instance is *.
   AND. the recipient names are identical
*/

static gboolean triple_subset(zephyr_triple * zt1, zephyr_triple * zt2)
{

	if (!zt2) {
		purple_debug_error("zephyr","zt2 doesn't exist\n");
		return FALSE;
	}
	if (!zt1) {
		purple_debug_error("zephyr","zt1 doesn't exist\n");
		return FALSE;
	}
	if (!(zt1->class)) {
		purple_debug_error("zephyr","zt1c doesn't exist\n");
		return FALSE;
	}
	if (!(zt1->instance)) {
		purple_debug_error("zephyr","zt1i doesn't exist\n");
		return FALSE;
	}
	if (!(zt1->recipient)) {
		purple_debug_error("zephyr","zt1r doesn't exist\n");
		return FALSE;
	}
	if (!(zt2->class)) {
		purple_debug_error("zephyr","zt2c doesn't exist\n");
		return FALSE;
	}
	if (!(zt2->recipient)) {
		purple_debug_error("zephyr","zt2r doesn't exist\n");
		return FALSE;
	}
	if (!(zt2->instance)) {
		purple_debug_error("zephyr","zt2i doesn't exist\n");
		return FALSE;
	}

	if (g_ascii_strcasecmp(zt2->class, zt1->class)) {
		return FALSE;
	}
	if (g_ascii_strcasecmp(zt2->instance, zt1->instance) && g_ascii_strcasecmp(zt2->instance, "*")) {
		return FALSE;
	}
	if (g_ascii_strcasecmp(zt2->recipient, zt1->recipient)) {
		return FALSE;
	}
	purple_debug_info("zephyr","<%s,%s,%s> is in <%s,%s,%s>\n",zt1->class,zt1->instance,zt1->recipient,zt2->class,zt2->instance,zt2->recipient);
	return TRUE;
}

static zephyr_triple *find_sub_by_triple(zephyr_account *zephyr,zephyr_triple * zt)
{
	zephyr_triple *curr_t;
	GSList *curr = zephyr->subscrips;

	while (curr) {
		curr_t = curr->data;
		if (triple_subset(zt, curr_t))
			return curr_t;
		curr = curr->next;
	}
	return NULL;
}

static zephyr_triple *find_sub_by_id(zephyr_account *zephyr,int id)
{
	zephyr_triple *zt;
	GSList *curr = zephyr->subscrips;

	while (curr) {
		zt = curr->data;
		if (zt->id == id)
			return zt;
		curr = curr->next;
	}
	return NULL;
}

/*
   Converts strings to utf-8 if necessary using user specified encoding
*/

static gchar *zephyr_recv_convert(PurpleConnection *gc, gchar *string)
{
	gchar *utf8;
	GError *err = NULL;
	zephyr_account *zephyr = purple_connection_get_protocol_data(gc);
	if (g_utf8_validate(string, -1, NULL)) {
		return g_strdup(string);
	} else {
		utf8 = g_convert(string, -1, "UTF-8", zephyr->encoding, NULL, NULL, &err);
		if (err) {
			purple_debug_error("zephyr", "recv conversion error: %s\n", err->message);
			utf8 = g_strdup(_("(There was an error converting this message.	 Check the 'Encoding' option in the Account Editor)"));
			g_error_free(err);
		}

		return utf8;
	}
}

static gboolean pending_zloc(zephyr_account *zephyr, const char *who)
{
	GList *curr;
	char* normalized_who = local_zephyr_normalize(zephyr,who);

	curr = g_list_find_custom(zephyr->pending_zloc_names, normalized_who, (GCompareFunc)g_ascii_strcasecmp);
	g_free(normalized_who);
	if (curr == NULL)
		return FALSE;

	g_free((char *)curr->data);
	zephyr->pending_zloc_names = g_list_delete_link(zephyr->pending_zloc_names, curr);
	return TRUE;
}

/* Called when the server notifies us a message couldn't get sent */

static void
message_failed(PurpleConnection *gc, ZNotice_t *notice)
{
	if (g_ascii_strcasecmp(notice->z_class, "message")) {
		gchar* chat_failed = g_strdup_printf(
			_("Unable to send to chat %s,%s,%s"),
			notice->z_class, notice->z_class_inst,
			notice->z_recipient);
		purple_notify_error(gc,"",chat_failed,NULL,
			purple_request_cpar_from_connection(gc));
		g_free(chat_failed);
	} else {
		purple_notify_error(gc, notice->z_recipient,
			_("User is offline"), NULL,
			purple_request_cpar_from_connection(gc));
	}
}

static PurpleBuddy *
find_buddy(const zephyr_account *zephyr, const char *user)
{
	PurpleBuddy *buddy = purple_blist_find_buddy(zephyr->account, user);

	if (buddy == NULL) {
		char *stripped_user = zephyr_strip_local_realm(zephyr, user);
		buddy = purple_blist_find_buddy(zephyr->account, stripped_user);
		g_free(stripped_user);
	}

	return buddy;
}

static void handle_message(PurpleConnection *gc, ZNotice_t *notice_p)
{
	ZNotice_t notice;
	zephyr_account* zephyr = purple_connection_get_protocol_data(gc);

	memcpy(&notice, notice_p, sizeof(notice)); /* TODO - use pointer? */

	if (!g_ascii_strcasecmp(notice.z_class, LOGIN_CLASS)) {
		/* well, we'll be updating in 20 seconds anyway, might as well ignore this. */
	} else if (!g_ascii_strcasecmp(notice.z_class, LOCATE_CLASS)) {
		if (!g_ascii_strcasecmp(notice.z_opcode, LOCATE_LOCATE)) {
			int nlocs;
			char *user;
			PurpleBuddy *b;
			const char *bname;

			/* XXX add real error reporting */
			if (ZParseLocations(&notice, NULL, &nlocs, &user) != ZERR_NONE)
				return;

			b = find_buddy(zephyr, user);
			bname = b ? purple_buddy_get_name(b) : NULL;
			if ((b && pending_zloc(zephyr,bname)) || pending_zloc(zephyr,user)) {
				ZLocations_t locs;
				int one = 1;
				PurpleNotifyUserInfo *user_info = purple_notify_user_info_new();
				char *tmp;
				const char *balias;

				/* TODO: Check whether it's correct to call add_pair_html,
				         or if we should be using add_pair_plaintext */
				purple_notify_user_info_add_pair_html(user_info, _("User"), (b ? bname : user));
				balias = purple_buddy_get_local_alias(b);
				if (b && balias)
					purple_notify_user_info_add_pair_plaintext(user_info, _("Alias"), balias);

				if (!nlocs) {
					purple_notify_user_info_add_pair_plaintext(user_info, NULL, _("Hidden or not logged-in"));
				}
				for (; nlocs > 0; nlocs--) {
					/* XXX add real error reporting */

					ZGetLocations(&locs, &one);
					/* TODO: Need to escape locs.host and locs.time? */
					tmp = g_strdup_printf(_("<br>At %s since %s"), locs.host, locs.time);
					purple_notify_user_info_add_pair_html(user_info, _("Location"), tmp);
					g_free(tmp);
				}
				purple_notify_userinfo(gc, (b ? bname : user),
						     user_info, NULL, NULL);
				purple_notify_user_info_destroy(user_info);
			} else {
				if (nlocs>0)
					purple_protocol_got_user_status(purple_connection_get_account(gc), b ? bname : user, "available", NULL);
				else
					purple_protocol_got_user_status(purple_connection_get_account(gc), b ? bname : user, "offline", NULL);
			}

			g_free(user);
		}
	} else {
		char *buf, *buf2, *buf3;
		char *send_inst;
		PurpleChatConversation *gcc;
		char *ptr = (char *) notice.z_message + (strlen(notice.z_message) + 1);
		int len;
		char *stripped_sender;
		int signature_length = strlen(notice.z_message);
		PurpleMessageFlags flags = 0;
		gchar *tmpescape;

		/* Need to deal with 0 length  messages to handle typing notification (OPCODE) ping messages */
		/* One field zephyrs would have caused purple to crash */
		if ( (notice.z_message_len == 0) || (signature_length >= notice.z_message_len - 1)) {
			len = 0;
			purple_debug_info("zephyr","message_size %d %d %d\n",len,notice.z_message_len,signature_length);
			buf3 = g_strdup("");

		} else {
			len =  notice.z_message_len - ( signature_length +1);
			purple_debug_info("zephyr","message_size %d %d %d\n",len,notice.z_message_len,signature_length);
			buf = g_malloc(len + 1);
			g_snprintf(buf, len + 1, "%s", ptr);
			g_strchomp(buf);
			tmpescape = g_markup_escape_text(buf, -1);
			g_free(buf);
			buf2 = zephyr_to_html(tmpescape);
			buf3 = zephyr_recv_convert(gc, buf2);
			g_free(buf2);
			g_free(tmpescape);
		}

		stripped_sender = zephyr_strip_local_realm(zephyr,notice.z_sender);

		if (!g_ascii_strcasecmp(notice.z_class, "MESSAGE") && !g_ascii_strcasecmp(notice.z_class_inst, "PERSONAL")
		    && !g_ascii_strcasecmp(notice.z_recipient,zephyr->username)) {
			if (!g_ascii_strcasecmp(notice.z_message, "Automated reply:"))
				flags |= PURPLE_MESSAGE_AUTO_RESP;

			if (!g_ascii_strcasecmp(notice.z_opcode,"PING"))
				purple_serv_got_typing(gc,stripped_sender,ZEPHYR_TYPING_RECV_TIMEOUT, PURPLE_IM_TYPING);
			else
				purple_serv_got_im(gc, stripped_sender, buf3, flags, time(NULL));

		} else {
			zephyr_triple *zt1, *zt2;
			gchar *send_inst_utf8;
			zephyr_account *zephyr = purple_connection_get_protocol_data(gc);
			zt1 = new_triple(zephyr,notice.z_class, notice.z_class_inst, notice.z_recipient);
			zt2 = find_sub_by_triple(zephyr,zt1);
			if (!zt2) {
				/* This is a server supplied subscription */
				zephyr->subscrips = g_slist_append(zephyr->subscrips, new_triple(zephyr,zt1->class,zt1->instance,zt1->recipient));
				zt2 = find_sub_by_triple(zephyr,zt1);
			}

			if (!zt2->open) {
				zt2->open = TRUE;
				purple_serv_got_joined_chat(gc, zt2->id, zt2->name);
				zephyr_chat_set_topic(gc,zt2->id,notice.z_class_inst);
			}

			if (!g_ascii_strcasecmp(notice.z_class_inst,"PERSONAL"))
				send_inst_utf8 = g_strdup(stripped_sender);
			else {
				send_inst = g_strdup_printf("[%s] %s",notice.z_class_inst,stripped_sender);
				send_inst_utf8 = zephyr_recv_convert(gc,send_inst);
				g_free(send_inst);
				if (!send_inst_utf8) {
					purple_debug_error("zephyr","Failed to convert instance for sender %s.\n", stripped_sender);
					send_inst_utf8 = g_strdup(stripped_sender);
				}
			}

			gcc = purple_conversations_find_chat_with_account(
														 zt2->name, purple_connection_get_account(gc));
			if (!purple_chat_conversation_has_user(gcc, stripped_sender)) {
				GInetAddress *inet_addr = NULL;
				gchar *ipaddr = NULL;

				inet_addr = g_inet_address_new_from_bytes(
				        (const guint8 *)&notice.z_sender_addr,
				        G_SOCKET_FAMILY_IPV4);
				ipaddr = g_inet_address_to_string(inet_addr);
				purple_chat_conversation_add_user(gcc, stripped_sender, ipaddr,
				                                  PURPLE_CHAT_USER_NONE, TRUE);
				g_free(ipaddr);
				g_object_unref(inet_addr);
			}
			purple_serv_got_chat_in(gc, zt2->id, send_inst_utf8,
				PURPLE_MESSAGE_RECV, buf3, time(NULL));
			g_free(send_inst_utf8);

			free_triple(zt1);
		}
		g_free(stripped_sender);
		g_free(buf3);
	}
}

static gchar *
tree_child_contents(GNode *tree, int index)
{
	GNode *child = g_node_nth_child(tree, index);
	return child ? child->data : "";
}

static GNode *
find_node(GNode *ptree, gchar *key)
{
	guint num_children;
	gchar* tc;

	if (!ptree || ! key)
		return NULL;

	num_children = g_node_n_children(ptree);
	tc = tree_child_contents(ptree, 0);

	/* g_strcasecmp() is deprecated.  What is the encoding here??? */
	if (num_children > 0 && tc && !g_ascii_strcasecmp(tc, key)) {
		return ptree;
	} else {
		GNode *result = NULL;
		guint i;
		for (i = 0; i < num_children; i++) {
			result = find_node(g_node_nth_child(ptree, i), key);
			if(result != NULL) {
				break;
			}
		}
		return result;
	}
}

static GNode *
parse_buffer(const gchar *source, gboolean do_parse)
{
	GNode *ptree = g_node_new(NULL);

	if (do_parse) {
		unsigned int p = 0;
		while(p < strlen(source)) {
			unsigned int end;
			gchar *newstr;

			/* Eat white space: */
			if(g_ascii_isspace(source[p]) || source[p] == '\001') {
				p++;
				continue;
			}

			/* Skip comments */
			if(source[p] == ';') {
				while(source[p] != '\n' && p < strlen(source)) {
					p++;
				}
				continue;
			}

			if(source[p] == '(') {
				int nesting = 0;
				gboolean in_quote = FALSE;
				gboolean escape_next = FALSE;
				p++;
				end = p;
				while(!(source[end] == ')' && nesting == 0 && !in_quote) && end < strlen(source)) {
					if(!escape_next) {
						if(source[end] == '\\') {
							escape_next = TRUE;
						}
						if(!in_quote) {
							if(source[end] == '(') {
								nesting++;
							}
							if(source[end] == ')') {
								nesting--;
							}
						}
						if(source[end] == '"') {
							in_quote = !in_quote;
						}
					} else {
						escape_next = FALSE;
					}
					end++;
				}
				do_parse = TRUE;

			} else {
				gchar end_char;
				if(source[p] == '"') {
					end_char = '"';
					p++;
				} else {
					end_char = ' ';
				}
				do_parse = FALSE;

				end = p;
				while(source[end] != end_char && end < strlen(source)) {
					if(source[end] == '\\')
						end++;
					end++;
				}
			}
			newstr = g_new0(gchar, end+1-p);
			strncpy(newstr,source+p,end-p);
			if (g_node_n_children(ptree) < MAXCHILDREN) {
				/* In case we surpass maxchildren, ignore this */
				g_node_append(ptree, parse_buffer(newstr, do_parse));
			} else {
				purple_debug_error("zephyr","too many children in tzc output. skipping\n");
			}
			g_free(newstr);
			p = end + 1;
		}
	} else {
		/* XXX does this have to be strdup'd */
		ptree->data = g_strdup(source);
	}

	return ptree;
}

static gchar *
read_from_tzc(zephyr_account *zephyr, PollableInputStreamReadFunc read_func)
{
	GPollableInputStream *stream = G_POLLABLE_INPUT_STREAM(zephyr->tzc_stdout);
	gsize bufsize = 2048;
	gchar *buf = g_new(gchar, bufsize);
	gchar *bufcur = buf;
	gboolean selected = FALSE;

	while (TRUE) {
		GError *error = NULL;
		if (read_func(stream, bufcur, &error) < 0) {
			if (error->code == G_IO_ERROR_WOULD_BLOCK ||
			    error->code == G_IO_ERROR_TIMED_OUT) {
				g_error_free(error);
				break;
			}
			purple_debug_error("zephyr", "couldn't read: %s", error->message);
			purple_connection_error(purple_account_get_connection(zephyr->account), PURPLE_CONNECTION_ERROR_NETWORK_ERROR, "couldn't read");
			g_error_free(error);
			g_free(buf);
			return NULL;
		}
		selected = TRUE;
		bufcur++;
		if ((bufcur - buf) > (bufsize - 1)) {
			if ((buf = g_realloc(buf, bufsize * 2)) == NULL) {
				purple_debug_error("zephyr","Ran out of memory\n");
				exit(-1);
			} else {
				bufcur = buf + bufsize;
				bufsize *= 2;
			}
		}
	}
	*bufcur = '\0';

	if (!selected) {
		g_free(buf);
		buf = NULL;
	}
	return buf;
}

static gssize
pollable_input_stream_read(GPollableInputStream *stream, void *bufcur, GError **error)
{
	return g_pollable_input_stream_read_nonblocking(stream, bufcur, 1, NULL, error);
}

static gint check_notify_tzc(gpointer data)
{
	PurpleConnection *gc = (PurpleConnection *)data;
	zephyr_account* zephyr = purple_connection_get_protocol_data(gc);
	GNode *newparsetree = NULL;
	gchar *buf = read_from_tzc(zephyr, pollable_input_stream_read);

	if (buf != NULL) {
		newparsetree = parse_buffer(buf, TRUE);
		g_free(buf);
	}

	if (newparsetree != NULL) {
		gchar *spewtype;
		if ( (spewtype =  tree_child_contents(find_node(newparsetree, "tzcspew"), 2)) ) {
			if (!g_ascii_strncasecmp(spewtype,"message",7)) {
				ZNotice_t notice;
				GNode *msgnode = g_node_nth_child(find_node(newparsetree, "message"), 2);
				/*char *zsig = g_strdup(" ");*/ /* purple doesn't care about zsigs */
				char *msg  = zephyr_tzc_deescape_str(tree_child_contents(msgnode, 1));
				size_t bufsize = strlen(msg) + 3;
				char *buf = g_new0(char,bufsize);
				g_snprintf(buf,1+strlen(msg)+2," %c%s",'\0',msg);
				memset((char *)&notice, 0, sizeof(notice));
				notice.z_kind = ACKED;
				notice.z_port = 0;
				notice.z_opcode = tree_child_contents(find_node(newparsetree, "opcode"), 2);
				notice.z_class = zephyr_tzc_deescape_str(tree_child_contents(find_node(newparsetree, "class"), 2));
				notice.z_class_inst = tree_child_contents(find_node(newparsetree, "instance"), 2);
				notice.z_recipient = local_zephyr_normalize(zephyr, tree_child_contents(find_node(newparsetree, "recipient"), 2));
				notice.z_sender = local_zephyr_normalize(zephyr, tree_child_contents(find_node(newparsetree, "sender"), 2));
				notice.z_default_format = "Class $class, Instance $instance:\n" "To: @bold($recipient) at $time $date\n" "From: @bold($1) <$sender>\n\n$2";
				notice.z_message_len = strlen(msg) + 3;
				notice.z_message = buf;
				handle_message(gc, &notice);
				g_free(msg);
				/*g_free(zsig);*/
				g_free(buf);
			}
			else if (!g_ascii_strncasecmp(spewtype,"zlocation",9)) {
				/* check_loc or zephyr_zloc respectively */
				/* XXX fix */
				char *user;
				PurpleBuddy *b;
				const char *bname;
				const gchar *name;
				gboolean has_locations;
				GNode *locations;
				gchar *locval;

				user = tree_child_contents(find_node(newparsetree, "user"), 2);
				b = find_buddy(zephyr, user);
				bname = b ? purple_buddy_get_name(b) : NULL;
				name = b ? bname : user;

				locations = g_node_nth_child(g_node_nth_child(find_node(newparsetree, "locations"), 2), 0);
				locval = tree_child_contents(g_node_nth_child(locations, 0), 2);
				has_locations = (locval && *locval && !purple_strequal(locval, " "));
				if ((b && pending_zloc(zephyr, bname)) || pending_zloc(zephyr, user)) {
					PurpleNotifyUserInfo *user_info = purple_notify_user_info_new();
					char *tmp;
					const char *balias;

					/* TODO: Check whether it's correct to call add_pair_html,
					         or if we should be using add_pair_plaintext */
					purple_notify_user_info_add_pair_html(user_info, _("User"), name);

					balias = b ? purple_buddy_get_local_alias(b) : NULL;
					if (balias)
						purple_notify_user_info_add_pair_plaintext(user_info, _("Alias"), balias);

					if (!has_locations) {
						purple_notify_user_info_add_pair_plaintext(user_info, NULL, _("Hidden or not logged-in"));
					} else {
						/* TODO: Need to escape the two strings that make up tmp? */
						tmp = g_strdup_printf(_("<br>At %s since %s"), locval,
						                      tree_child_contents(g_node_nth_child(locations, 2), 2));
						purple_notify_user_info_add_pair_html(user_info, _("Location"), tmp);
						g_free(tmp);
					}

					purple_notify_userinfo(gc, name, user_info, NULL, NULL);
					purple_notify_user_info_destroy(user_info);
				} else {
					purple_protocol_got_user_status(zephyr->account, name, has_locations ? "available" : "offline", NULL);
				}
			}
			else if (!g_ascii_strncasecmp(spewtype,"subscribed",10)) {
			}
			else if (!g_ascii_strncasecmp(spewtype,"start",5)) {
			}
			else if (!g_ascii_strncasecmp(spewtype,"error",5)) {
				/* XXX handle */
			}
		} else {
		}
	} else {
	}

	g_node_destroy(newparsetree);
	return TRUE;
}

static gint check_notify_zeph02(gpointer data)
{
	/* XXX add real error reporting */
	PurpleConnection *gc = (PurpleConnection*) data;
	while (ZPending()) {
		ZNotice_t notice;
		/* XXX add real error reporting */

		if (ZReceiveNotice(&notice, NULL) != ZERR_NONE) {
			return TRUE;
		}

		switch (notice.z_kind) {
		case UNSAFE:
		case UNACKED:
		case ACKED:
			handle_message(gc, &notice);
			break;
		case SERVACK:
			if (!(g_ascii_strcasecmp(notice.z_message, ZSRVACK_NOTSENT))) {
				message_failed(gc, &notice);
			}
			break;
		case CLIENTACK:
			purple_debug_error("zephyr", "Client ack received\n");
			handle_unknown(&notice); /* XXX: is it really unknown? */
			break;
		default:
			/* we'll just ignore things for now */
			handle_unknown(&notice);
			purple_debug_error("zephyr", "Unhandled notice.\n");
			break;
		}
		/* XXX add real error reporting */
		ZFreeNotice(&notice);
	}

	return TRUE;
}

static gboolean
zephyr_request_locations(zephyr_account *zephyr, gchar *who)
{
	if (use_zeph02(zephyr)) {
		ZAsyncLocateData_t ald;
		Code_t zerr;

		zerr = ZRequestLocations(who, &ald, UNACKED, ZAUTH);
		g_free(ald.user);
		g_free(ald.version);
		return zerr == ZERR_NONE;
	}

	if (use_tzc(zephyr)) {
		gchar *zlocstr;

		zlocstr = g_strdup_printf("((tzcfodder . zlocate) \"%s\")\n", who);
		zephyr_write_message(zephyr, zlocstr);
		g_free(zlocstr);
		return TRUE;
	}

	return FALSE;
}

#ifdef WIN32

static gint check_loc(gpointer data)
{
	GSList *buddies;
	ZLocations_t locations;
	PurpleConnection *gc = data;
	zephyr_account *zephyr = purple_connection_get_protocol_data(gc);
	PurpleAccount *account = purple_connection_get_account(gc);
	int numlocs;
	int one = 1;

	for (buddies = purple_blist_find_buddies(account, NULL); buddies;
			buddies = g_slist_delete_link(buddies, buddies)) {
		PurpleBuddy *b = buddies->data;
		char *chk;
		const char *bname = purple_buddy_get_name(b);
		chk = local_zephyr_normalize(zephyr, bname);
		ZLocateUser(chk,&numlocs, ZAUTH);
		if (numlocs) {
			int i;
			for(i=0;i<numlocs;i++) {
				ZGetLocations(&locations,&one);
				serv_got_update(zgc,bname,1,0,0,0,0);
			}
		}
	}

	return TRUE;
}

#else

static gint check_loc(gpointer data)
{
	GSList *buddies;
	PurpleConnection *gc = (PurpleConnection *)data;
	zephyr_account *zephyr = purple_connection_get_protocol_data(gc);
	PurpleAccount *account = purple_connection_get_account(gc);

	for (buddies = purple_blist_find_buddies(account, NULL); buddies;
			buddies = g_slist_delete_link(buddies, buddies)) {
		PurpleBuddy *b = buddies->data;

		char *chk;
		const char *name = purple_buddy_get_name(b);

		chk = local_zephyr_normalize(zephyr,name);
		purple_debug_info("zephyr","chk: %s b->name %s\n",chk,name);
		/* XXX add real error reporting */
		/* doesn't matter if this fails or not; we'll just move on to the next one */
		zephyr_request_locations(zephyr, chk);
		g_free(chk);
	}

	return TRUE;
}

#endif /* WIN32 */

static const gchar *
get_exposure_level(void)
{
	/* XXX add real error reporting */
	const gchar *exposure = ZGetVariable("exposure");

	if (exposure) {
		if (!g_ascii_strcasecmp(exposure, EXPOSE_NONE)) {
			return EXPOSE_NONE;
		}
		if (!g_ascii_strcasecmp(exposure, EXPOSE_OPSTAFF)) {
			return EXPOSE_OPSTAFF;
		}
		if (!g_ascii_strcasecmp(exposure, EXPOSE_REALMANN)) {
			return EXPOSE_REALMANN;
		}
		if (!g_ascii_strcasecmp(exposure, EXPOSE_NETVIS)) {
			return EXPOSE_NETVIS;
		}
		if (!g_ascii_strcasecmp(exposure, EXPOSE_NETANN)) {
			return EXPOSE_NETANN;
		}
	}
	return EXPOSE_REALMVIS;
}

static void strip_comments(char *str)
{
	char *tmp = strchr(str, '#');

	if (tmp)
		*tmp = '\0';
	g_strchug(str);
	g_strchomp(str);
}

static void zephyr_inithosts(zephyr_account *zephyr)
{
	/* XXX This code may not be Win32 clean */
	struct hostent *hent;

	if (gethostname(zephyr->ourhost, sizeof(zephyr->ourhost)) == -1) {
		purple_debug_error("zephyr", "unable to retrieve hostname, %%host%% and %%canon%% will be wrong in subscriptions and have been set to unknown\n");
		g_strlcpy(zephyr->ourhost, "unknown", sizeof(zephyr->ourhost));
		g_strlcpy(zephyr->ourhostcanon, "unknown", sizeof(zephyr->ourhostcanon));
		return;
	}

	if (!(hent = gethostbyname(zephyr->ourhost))) {
		purple_debug_error("zephyr", "unable to resolve hostname, %%canon%% will be wrong in subscriptions.and has been set to the value of %%host%%, %s\n",zephyr->ourhost);
		g_strlcpy(zephyr->ourhostcanon, zephyr->ourhost, sizeof(zephyr->ourhostcanon));
		return;
	}

	g_strlcpy(zephyr->ourhostcanon, hent->h_name, sizeof(zephyr->ourhostcanon));
}

static void process_zsubs(zephyr_account *zephyr)
{
	/* Loads zephyr chats "(subscriptions) from ~/.zephyr.subs, and
	   registers (subscribes to) them on the server */

	/* XXX deal with unsubscriptions */
	/* XXX deal with punts */

	FILE *f;
	gchar *fname;
	gchar buff[BUFSIZ];

	fname = g_strdup_printf("%s/.zephyr.subs", purple_home_dir());
	f = g_fopen(fname, "r");
	if (f) {
		char **triple;
		char *recip;
		char *z_class;
		char *z_instance;

		while (fgets(buff, BUFSIZ, f)) {
			strip_comments(buff);
			if (buff[0]) {
				triple = g_strsplit(buff, ",", 3);
				if (triple[0] && triple[1]) {
					char *tmp = g_strdup(zephyr->username);
					char *atptr;

					if (triple[2] == NULL) {
						recip = g_malloc0(1);
					} else if (!g_ascii_strcasecmp(triple[2], "%me%")) {
						recip = g_strdup(zephyr->username);
					} else if (!g_ascii_strcasecmp(triple[2], "*")) {
						/* wildcard
						 * form of class,instance,* */
						recip = g_malloc0(1);
					} else if (!g_ascii_strcasecmp(triple[2], tmp)) {
						/* form of class,instance,aatharuv@ATHENA.MIT.EDU */
						recip = g_strdup(triple[2]);
					} else if ((atptr = strchr(triple[2], '@')) != NULL) {
						/* form of class,instance,*@ANDREW.CMU.EDU
						 * class,instance,@ANDREW.CMU.EDU
						 * If realm is local realm, blank recipient, else
						 * @REALM-NAME
						 */
						char *realmat = g_strdup_printf("@%s",zephyr->realm);

						if (!g_ascii_strcasecmp(atptr, realmat))
							recip = g_malloc0(1);
						else
							recip = g_strdup(atptr);
						g_free(realmat);
					} else {
						recip = g_strdup(triple[2]);
					}
					g_free(tmp);

					if (!g_ascii_strcasecmp(triple[0],"%host%")) {
						z_class = g_strdup(zephyr->ourhost);
					} else if (!g_ascii_strcasecmp(triple[0],"%canon%")) {
						z_class = g_strdup(zephyr->ourhostcanon);
					} else {
						z_class = g_strdup(triple[0]);
					}

					if (!g_ascii_strcasecmp(triple[1],"%host%")) {
						z_instance = g_strdup(zephyr->ourhost);
					} else if (!g_ascii_strcasecmp(triple[1],"%canon%")) {
						z_instance = g_strdup(zephyr->ourhostcanon);
					} else {
						z_instance = g_strdup(triple[1]);
					}

					/* There should be some sort of error report listing classes that couldn't be subbed to.
					   Not important right now though */

					if (zephyr->subscribe_to(zephyr, z_class, z_instance, recip) != ZERR_NONE) {
						purple_debug_error("zephyr", "Couldn't subscribe to %s, %s, %s\n", z_class,z_instance,recip);
					}

					zephyr->subscrips = g_slist_append(zephyr->subscrips, new_triple(zephyr,z_class,z_instance,recip));
					/*					  g_hash_table_destroy(sub_hash_table); */
					g_free(z_instance);
					g_free(z_class);
					g_free(recip);
				}
				g_strfreev(triple);
			}
		}
		fclose(f);
	}
	g_free(fname);
}

static void
process_anyone(const zephyr_account *zephyr)
{
	FILE *fd;
	gchar buff[BUFSIZ], *filename;
	PurpleGroup *g;
	PurpleBuddy *b;

	if (!(g = purple_blist_find_group(_("Anyone")))) {
		g = purple_group_new(_("Anyone"));
		purple_blist_add_group(g, NULL);
	}

	filename = g_strconcat(purple_home_dir(), "/.anyone", NULL);
	if ((fd = g_fopen(filename, "r")) != NULL) {
		while (fgets(buff, BUFSIZ, fd)) {
			strip_comments(buff);
			if (*buff && !find_buddy(zephyr, buff)) {
				char *stripped_user = zephyr_strip_local_realm(zephyr, buff);
				purple_debug_info("zephyr", "stripped_user %s\n", stripped_user);
				b = purple_buddy_new(zephyr->account, stripped_user, NULL);
				purple_blist_add_buddy(b, NULL, g, NULL);
				g_free(stripped_user);
			}
		}
		fclose(fd);
	}
	g_free(filename);
}

static gchar *
get_zephyr_exposure(PurpleAccount *account)
{
	const gchar *exposure;
	gchar *exp2;

	exposure = purple_account_get_string(account, "exposure_level", EXPOSE_REALMVIS);

	/* Make sure that the exposure (visibility) is set to a sane value */
	exp2 = g_strstrip(g_ascii_strup(exposure, -1));
	if (exp2) {
		if (purple_strequal(exp2, EXPOSE_NONE) ||
		    purple_strequal(exp2, EXPOSE_OPSTAFF) ||
		    purple_strequal(exp2, EXPOSE_REALMANN) ||
		    purple_strequal(exp2, EXPOSE_NETVIS) ||
		    purple_strequal(exp2, EXPOSE_NETANN)) {
			return exp2;
		}
		g_free(exp2);
	}
	return g_strdup(EXPOSE_REALMVIS);
}

static gssize
pollable_input_stream_read_with_timeout(GPollableInputStream *stream,
                                        void *bufcur, GError **error)
{
	const gint64 timeout = 10 * G_USEC_PER_SEC;
	gint64 now = g_get_monotonic_time();

	while (g_get_monotonic_time() < now + timeout) {
		GError *local_error = NULL;
		gssize ret = g_pollable_input_stream_read_nonblocking(
		        stream, bufcur, 1, NULL, &local_error);
		if (ret == 1) {
			return ret;
		} else {
			if (local_error->code == G_IO_ERROR_WOULD_BLOCK) {
				/* Keep on waiting if this is a blocking error. */
				g_clear_error(&local_error);
			} else {
				g_propagate_error(error, local_error);
				return ret;
			}
		}
	}

	g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT,
	                    "tzc did not respond in time");
	return -1;
}

static GSubprocess *
get_tzc_process(const zephyr_account *zephyr)
{
	GSubprocess *tzc_process = NULL;
	const gchar *tzc_cmd;
	gchar **tzc_cmd_array = NULL;
	GError *error = NULL;
	gboolean found_ps = FALSE;
	gint i;

	/* tzc_command should really be of the form
	   path/to/tzc -e %s
	   or
	   ssh username@hostname pathtotzc -e %s
	   -- this should not require a password, and ideally should be
	   kerberized ssh --
	   or
	   fsh username@hostname pathtotzc -e %s
	*/
	tzc_cmd = purple_account_get_string(zephyr->account, "tzc_command", "/usr/bin/tzc -e %s");
	if (!g_shell_parse_argv(tzc_cmd, NULL, &tzc_cmd_array, &error)) {
		purple_debug_error("zephyr", "Unable to parse tzc_command: %s", error->message);
		purple_connection_error(
		        purple_account_get_connection(zephyr->account),
		        PURPLE_CONNECTION_ERROR_INVALID_SETTINGS,
		        "invalid tzc_command setting");
		g_error_free(error);
		return NULL;
	}
	for (i = 0; tzc_cmd_array[i] != NULL; i++) {
		if (purple_strequal(tzc_cmd_array[i], "%s")) {
			g_free(tzc_cmd_array[i]);
			tzc_cmd_array[i] = g_strdup(zephyr->exposure);
			found_ps = TRUE;
		}
	}

	if (!found_ps) {
		purple_debug_error("zephyr", "tzc exited early");
		purple_connection_error(
		        purple_account_get_connection(zephyr->account),
		        PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
		        "invalid output by tzc (or bad parsing code)");
		g_strfreev(tzc_cmd_array);
		return NULL;
	}

	tzc_process = g_subprocess_newv(
	        (const gchar *const *)tzc_cmd_array,
	        G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE,
	        &error);
	if (tzc_process == NULL) {
		purple_debug_error("zephyr", "tzc exited early: %s", error->message);
		purple_connection_error(
		        purple_account_get_connection(zephyr->account),
		        PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
		        "invalid output by tzc (or bad parsing code)");
		g_error_free(error);
	}

	g_strfreev(tzc_cmd_array);
	return tzc_process;
}

static gint
get_paren_level(gint paren_level, gchar ch)
{
	switch (ch) {
		case '(': return paren_level + 1;
		case ')': return paren_level - 1;
		default: return paren_level;
	}
}

static gchar *
get_zephyr_realm(PurpleAccount *account, const gchar *local_realm)
{
	const char *realm = purple_account_get_string(account, "realm", "");
	if (!*realm) {
		realm = local_realm;
	}
	g_strlcpy(__Zephyr_realm, realm, REALM_SZ - 1);
	return g_strdup(realm);
}

static void
parse_tzc_login_data(zephyr_account *zephyr, const gchar *buf, gint buflen)
{
	gchar *str = g_strndup(buf, buflen);

	purple_debug_info("zephyr", "tempstr parsed");

	/* str should now be a string containing all characters from
	 * buf after the first ( to the one before the last paren ).
	 * We should have the following possible lisp strings but we don't care
	 * (tzcspew . start) (version . "something") (pid . number)
	 * We care about 'zephyrid . "username@REALM.NAME"' and
	 * 'exposure . "SOMETHING"' */
	if (!g_ascii_strncasecmp(str, "zephyrid", 8)) {
		gchar **strv;
		gchar *username;
		const char *at;

		purple_debug_info("zephyr", "zephyrid found");

		strv = g_strsplit(str + 8, "\"", -1);
		username = strv[1] ? strv[1] : "";
		zephyr->username = g_strdup(username);

		at = strchr(username, '@');
		if (at != NULL) {
			zephyr->realm = g_strdup(at + 1);
		} else {
			zephyr->realm = get_zephyr_realm(zephyr->account, "local-realm");
		}

		g_strfreev(strv);
	} else {
		purple_debug_info("zephyr", "something that's not zephyr id found %s", str);
	}

	/* We don't care about anything else yet */
	g_free(str);
}

static gboolean
login_tzc(zephyr_account *zephyr)
{
	gchar *buf = NULL;
	const gchar *bufend = NULL;
	const gchar *ptr;
	const gchar *tmp;
	gint parenlevel = 0;

	zephyr->tzc_proc = get_tzc_process(zephyr);
	if (zephyr->tzc_proc == NULL) {
		return FALSE;
	}
	zephyr->tzc_stdin = g_subprocess_get_stdin_pipe(zephyr->tzc_proc);
	zephyr->tzc_stdout = g_subprocess_get_stdout_pipe(zephyr->tzc_proc);

	purple_debug_info("zephyr", "about to read from tzc");
	buf = read_from_tzc(zephyr, pollable_input_stream_read_with_timeout);
	if (buf == NULL) {
		return FALSE;
	}
	bufend = buf + strlen(buf);
	ptr = buf;

	/* ignore all tzcoutput till we've received the first ( */
	while (ptr < bufend && (*ptr != '(')) {
		ptr++;
	}
	if (ptr >= bufend) {
		purple_connection_error(
		        purple_account_get_connection(zephyr->account),
		        PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
		        "invalid output by tzc (or bad parsing code)");
		g_free(buf);
		return FALSE;
	}

	do {
		parenlevel = get_paren_level(parenlevel, *ptr);
		purple_debug_info("zephyr", "tzc parenlevel is %d", parenlevel);
		switch (parenlevel) {
			case 1:
				/* Search for next beginning (, or for the ending */
				do {
					ptr++;
				} while ((ptr < bufend) && (*ptr != '(') && (*ptr != ')'));
				if (ptr >= bufend) {
					purple_debug_error("zephyr", "tzc parsing error");
				}
				break;
			case 2:
				/* You are probably at
				   (foo . bar ) or (foo . "bar") or (foo . chars) or (foo . numbers) or (foo . () )
				   Parse all the data between the first and last f, and move past )
				*/
				tmp = ptr;
				do {
					ptr++;
					parenlevel = get_paren_level(parenlevel, *ptr);
				} while (parenlevel > 1);
				parse_tzc_login_data(zephyr, tmp + 1, ptr - tmp);
				ptr++;
				break;
			default:
				purple_debug_info("zephyr", "parenlevel is not 1 or 2");
				/* This shouldn't be happening */
				break;
		}
	} while (ptr < bufend && parenlevel != 0);
	purple_debug_info("zephyr", "tzc startup done");
	g_free(buf);

	return TRUE;
}

static gboolean
login_zeph02(zephyr_account *zephyr)
{
	PurpleConnection *pc = purple_account_get_connection(zephyr->account);

	/* XXX Should actually try to report the com_err determined error */
	if (ZInitialize() != ZERR_NONE) {
		purple_connection_error(pc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
		                        "Couldn't initialize zephyr");
		return FALSE;
	}

	if (ZOpenPort(&(zephyr->port)) != ZERR_NONE) {
		purple_connection_error(pc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
		                        "Couldn't open port");
		return FALSE;
	}

	if (ZSetLocation(zephyr->exposure) != ZERR_NONE) {
		purple_connection_error(pc, PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
		                        "Couldn't set location");
		return FALSE;
	}

	zephyr->username = g_strdup(ZGetSender());
	zephyr->realm = get_zephyr_realm(zephyr->account, ZGetRealm());

	return TRUE;
}

static void zephyr_login(PurpleAccount * account)
{
	PurpleConnection *gc;
	zephyr_account *zephyr;
	ZephyrLoginFunc login;
	GSourceFunc check_notify;

	gc = purple_account_get_connection(account);

#ifdef WIN32
	username = purple_account_get_username(account);
#endif
	purple_connection_set_flags(gc, PURPLE_CONNECTION_FLAG_AUTO_RESP |
		PURPLE_CONNECTION_FLAG_HTML | PURPLE_CONNECTION_FLAG_NO_BGCOLOR |
		PURPLE_CONNECTION_FLAG_NO_URLDESC | PURPLE_CONNECTION_FLAG_NO_IMAGES);
	zephyr = g_new0(zephyr_account, 1);
	purple_connection_set_protocol_data(gc, zephyr);

	zephyr->account = account;
	zephyr->exposure = get_zephyr_exposure(account);

	if (purple_account_get_bool(account, "use_tzc", FALSE)) {
		zephyr->connection_type = PURPLE_ZEPHYR_TZC;
		login = login_tzc;
		check_notify = check_notify_tzc;
		zephyr->subscribe_to = subscribe_to_tzc;
	} else {
		zephyr->connection_type = PURPLE_ZEPHYR_KRB4;
		login = login_zeph02;
		check_notify = check_notify_zeph02;
		zephyr->subscribe_to = subscribe_to_zeph02;
	}

	zephyr->encoding = (char *)purple_account_get_string(account, "encoding", ZEPHYR_FALLBACK_CHARSET);
	purple_connection_update_progress(gc, _("Connecting"), 0, 8);

	if (!login(zephyr)) {
		return;
	}
	purple_debug_info("zephyr","does it get here\n");
	purple_debug_info("zephyr"," realm: %s username:%s\n", zephyr->realm, zephyr->username);

	/* For now */
	zephyr->galaxy = NULL;
	zephyr->krbtkfile = NULL;
	zephyr_inithosts(zephyr);

	if (zephyr->subscribe_to(zephyr, "MESSAGE", "PERSONAL", zephyr->username) != ZERR_NONE) {
		/* XXX don't translate this yet. It could be written better */
		/* XXX error messages could be handled with more detail */
		purple_notify_error(gc, NULL,
			"Unable to subscribe to messages", "Unable to subscribe to initial messages",
			purple_request_cpar_from_connection(gc));
		return;
	}

	purple_connection_set_state(gc, PURPLE_CONNECTION_CONNECTED);

	if (purple_account_get_bool(account, "read_anyone", TRUE)) {
		process_anyone(zephyr);
	}
	if (purple_account_get_bool(account, "read_zsubs", TRUE)) {
		process_zsubs(zephyr);
	}

	zephyr->nottimer = g_timeout_add(100, check_notify, gc);
	zephyr->loctimer = g_timeout_add_seconds(20, check_loc, gc);
}

static void write_zsubs(zephyr_account *zephyr)
{
	/* Exports subscription (chat) list back to
	 * .zephyr.subs
	 * XXX deal with %host%, %canon%, unsubscriptions, and negative subscriptions (punts?)
	 */

	GSList *s = zephyr->subscrips;
	zephyr_triple *zt;
	FILE *fd;
	char *fname;

	char **triple;

	fname = g_strdup_printf("%s/.zephyr.subs", purple_home_dir());
	fd = g_fopen(fname, "w");

	if (!fd) {
		g_free(fname);
		return;
	}

	while (s) {
		char *zclass, *zinst, *zrecip;
		zt = s->data;
		triple = g_strsplit(zt->name, ",", 3);

		/* deal with classes */
		if (!g_ascii_strcasecmp(triple[0],zephyr->ourhost)) {
			zclass = g_strdup("%host%");
		} else if (!g_ascii_strcasecmp(triple[0],zephyr->ourhostcanon)) {
			zclass = g_strdup("%canon%");
		} else {
			zclass = g_strdup(triple[0]);
		}

		/* deal with instances */

		if (!g_ascii_strcasecmp(triple[1],zephyr->ourhost)) {
			zinst = g_strdup("%host%");
		} else if (!g_ascii_strcasecmp(triple[1],zephyr->ourhostcanon)) {
			zinst = g_strdup("%canon%");;
		} else {
			zinst = g_strdup(triple[1]);
		}

		/* deal with recipients */
		if (triple[2] == NULL) {
			zrecip = g_strdup("*");
		} else if (!g_ascii_strcasecmp(triple[2],"")){
			zrecip = g_strdup("*");
		} else if (!g_ascii_strcasecmp(triple[2], zephyr->username)) {
			zrecip = g_strdup("%me%");
		} else {
			zrecip = g_strdup(triple[2]);
		}

		fprintf(fd, "%s,%s,%s\n",zclass,zinst,zrecip);

		g_free(zclass);
		g_free(zinst);
		g_free(zrecip);
		g_strfreev(triple);
		s = s->next;
	}
	g_free(fname);
	fclose(fd);
}

static void write_anyone(zephyr_account *zephyr)
{
	GSList *buddies;
	char *fname;
	FILE *fd;
	PurpleAccount *account;
	fname = g_strdup_printf("%s/.anyone", purple_home_dir());
	fd = g_fopen(fname, "w");
	if (!fd) {
		g_free(fname);
		return;
	}

	account = zephyr->account;
	for (buddies = purple_blist_find_buddies(account, NULL); buddies;
			buddies = g_slist_delete_link(buddies, buddies)) {
		PurpleBuddy *b = buddies->data;
		gchar *stripped_user = zephyr_strip_local_realm(zephyr, purple_buddy_get_name(b));
		fprintf(fd, "%s\n", stripped_user);
		g_free(stripped_user);
	}

	fclose(fd);
	g_free(fname);
}

static void zephyr_close(PurpleConnection * gc)
{
	zephyr_account *zephyr = purple_connection_get_protocol_data(gc);

	g_list_free_full(zephyr->pending_zloc_names, g_free);

	if (purple_account_get_bool(purple_connection_get_account(gc), "write_anyone", FALSE))
		write_anyone(zephyr);

	if (purple_account_get_bool(purple_connection_get_account(gc), "write_zsubs", FALSE))
		write_zsubs(zephyr);

	g_slist_free_full(zephyr->subscrips, (GDestroyNotify)free_triple);

	if (zephyr->nottimer)
		g_source_remove(zephyr->nottimer);
	zephyr->nottimer = 0;
	if (zephyr->loctimer)
		g_source_remove(zephyr->loctimer);
	zephyr->loctimer = 0;
	if (use_zeph02(zephyr)) {
		if (ZCancelSubscriptions(0) != ZERR_NONE) {
			return;
		}
		if (ZUnsetLocation() != ZERR_NONE) {
			return;
		}
		if (ZClosePort() != ZERR_NONE) {
			return;
		}
	} else {
		/* assume tzc */
#ifdef G_OS_UNIX
		GError *error = NULL;
		g_subprocess_send_signal(zephyr->tzc_proc, SIGTERM);
		if (!g_subprocess_wait(zephyr->tzc_proc, NULL, &error)) {
			purple_debug_error("zephyr",
			                   "error while attempting to close tzc: %s",
			                   error->message);
			g_error_free(error);
		}
#else
		g_subprocess_force_exit(zephyr->tzc_proc);
#endif
		zephyr->tzc_stdin = NULL;
		zephyr->tzc_stdout = NULL;
		g_clear_object(&zephyr->tzc_proc);
	}
}

static gboolean zephyr_send_message(zephyr_account *zephyr, gchar *zclass,
                                    gchar *instance, gchar *recipient,
                                    const gchar *im, const gchar *sig,
                                    gchar *opcode);

static const char * zephyr_get_signature(void)
{
	/* XXX add zephyr error reporting */
	const char * sig =ZGetVariable("zwrite-signature");
	if (!sig) {
		sig = g_get_real_name();
	}
	return sig;
}

static int
zephyr_chat_send(PurpleProtocolChat *protocol_chat, PurpleConnection *gc,
                 int id, PurpleMessage *msg)
{
	zephyr_triple *zt;
	const char *sig;
	PurpleChatConversation *gcc;
	char *inst;
	char *recipient;
	zephyr_account *zephyr = purple_connection_get_protocol_data(gc);

	zt = find_sub_by_id(zephyr,id);
	if (!zt)
		/* this should never happen. */
		return -EINVAL;

	sig = zephyr_get_signature();

	gcc = purple_conversations_find_chat_with_account(zt->name,
												 purple_connection_get_account(gc));

	if (!(inst = (char *)purple_chat_conversation_get_topic(gcc)))
		inst = g_strdup("PERSONAL");

	if (!g_ascii_strcasecmp(zt->recipient, "*"))
		recipient = local_zephyr_normalize(zephyr,"");
	else
		recipient = local_zephyr_normalize(zephyr,zt->recipient);

	zephyr_send_message(zephyr, zt->class, inst, recipient,
		purple_message_get_contents(msg), sig, "");
	return 0;
}


static int zephyr_send_im(PurpleProtocolIM *im, PurpleConnection *gc, PurpleMessage *msg)
{
	const char *sig;
	zephyr_account *zephyr = purple_connection_get_protocol_data(gc);

	if (purple_message_get_flags(msg) & PURPLE_MESSAGE_AUTO_RESP) {
		sig = "Automated reply:";
	} else {
		sig = zephyr_get_signature();
	}
	zephyr_send_message(zephyr, "MESSAGE", "PERSONAL",
		local_zephyr_normalize(zephyr, purple_message_get_recipient(msg)),
		purple_message_get_contents(msg), sig, "");

	return 1;
}

/* Munge the outgoing zephyr so that any quotes or backslashes are
   escaped and do not confuse tzc: */

static char* zephyr_tzc_escape_msg(const char *message)
{
	gsize pos = 0, pos2 = 0;
	char *newmsg;

	if (message && *message) {
		newmsg = g_new0(char,1+strlen(message)*2);
		while(pos < strlen(message)) {
			if (message[pos]=='\\') {
				newmsg[pos2]='\\';
				newmsg[pos2+1]='\\';
				pos2+=2;
			}
			else if (message[pos]=='"') {
				newmsg[pos2]='\\';
				newmsg[pos2+1]='"';
				pos2+=2;
			}
			else {
				newmsg[pos2] = message[pos];
				pos2++;
			}
			pos++;
		}
	} else {
		newmsg = g_strdup("");
	}
	/*	fprintf(stderr,"newmsg %s message %s\n",newmsg,message); */
	return newmsg;
}

char* zephyr_tzc_deescape_str(const char *message)
{
	gsize pos = 0, pos2 = 0;
	char *newmsg;

	if (message && *message) {
		newmsg = g_new0(char,strlen(message)+1);
		while(pos < strlen(message)) {
			if (message[pos]=='\\') {
				pos++;
			}
			newmsg[pos2] = message[pos];
			pos++;pos2++;
		}
		newmsg[pos2]='\0';
	} else {
		newmsg = g_strdup("");
	}

	return newmsg;
}

static gboolean
zephyr_send_message(zephyr_account *zephyr, gchar *zclass, gchar *instance,
                    gchar *recipient, const gchar *im, const gchar *sig,
                    gchar *opcode)
{

	/* (From the tzc source)
	 * emacs sends something of the form:
	 * ((class . "MESSAGE")
	 *  (auth . t)
	 *  (recipients ("PERSONAL" . "bovik") ("test" . ""))
	 *  (sender . "bovik")
	 *  (message . ("Harry Bovik" "my zgram"))
	 * )
	 */
	char *tmp_buf;
	char *html_buf;
	tmp_buf = html_to_zephyr(im);
	html_buf = purple_unescape_html(tmp_buf);
	g_free(tmp_buf);

	if(use_tzc(zephyr)) {
		char* zsendstr;
		/* CMU cclub tzc doesn't grok opcodes for now  */
		char* tzc_sig = zephyr_tzc_escape_msg(sig);
		char *tzc_body = zephyr_tzc_escape_msg(html_buf);
		zsendstr = g_strdup_printf("((tzcfodder . send) (class . \"%s\") (auth . t) (recipients (\"%s\" . \"%s\")) (message . (\"%s\" \"%s\"))	) \n",
					   zclass, instance, recipient, tzc_sig, tzc_body);

		if (!zephyr_write_message(zephyr, zsendstr)) {
			g_free(tzc_sig);
			g_free(tzc_body);
			g_free(zsendstr);
			g_free(html_buf);
			return FALSE;
		}
		g_free(tzc_sig);
		g_free(tzc_body);
		g_free(zsendstr);
	} else if (use_zeph02(zephyr)) {
		ZNotice_t notice;
		char *buf = g_strdup_printf("%s%c%s", sig, '\0', html_buf);
		memset((char *)&notice, 0, sizeof(notice));

		notice.z_kind = ACKED;
		notice.z_port = 0;
		notice.z_class = zclass;
		notice.z_class_inst = instance;
		notice.z_recipient = recipient;
		notice.z_sender = 0;
		notice.z_default_format = "Class $class, Instance $instance:\n" "To: @bold($recipient) at $time $date\n" "From: @bold($1) <$sender>\n\n$2";
		notice.z_message_len = strlen(html_buf) + strlen(sig) + 2;
		notice.z_message = buf;
		notice.z_opcode = g_strdup(opcode);
		purple_debug_info("zephyr","About to send notice\n");
		if (ZSendNotice(&notice, ZAUTH) != ZERR_NONE) {
			/* XXX handle errors here */
			g_free(buf);
			g_free(html_buf);
			return FALSE;
		}
		purple_debug_info("zephyr","notice sent\n");
		g_free(buf);
	}

	g_free(html_buf);

	return TRUE;
}

char *local_zephyr_normalize(zephyr_account *zephyr,const char *orig)
{
	/* Basically the inverse of zephyr_strip_local_realm */

	if (*orig == '\0' || strchr(orig, '@')) {
		return g_strdup(orig);
	}

	return g_strdup_printf("%s@%s", orig, zephyr->realm);
}

static const char *
zephyr_normalize(PurpleProtocolClient *client, PurpleAccount *account,
                 const char *who)
{
	static char buf[BUF_LEN];
	PurpleConnection *gc;
	char *tmp;

	if (account == NULL) {
		if (strlen(who) >= sizeof(buf))
			return NULL;
		return who;
	}

	gc = purple_account_get_connection((PurpleAccount *)account);
	if (gc == NULL)
		return NULL;

	tmp = local_zephyr_normalize(purple_connection_get_protocol_data(gc), who);

	if (strlen(tmp) >= sizeof(buf)) {
		g_free(tmp);
		return NULL;
	}

	g_strlcpy(buf, tmp, sizeof(buf));
	g_free(tmp);

	return buf;
}

static void
zephyr_zloc(PurpleProtocolServer *protocol_server, PurpleConnection *gc,
            const gchar *who)
{
	zephyr_account *zephyr = purple_connection_get_protocol_data(gc);
	gchar* normalized_who = local_zephyr_normalize(zephyr,who);

	if (zephyr_request_locations(zephyr, normalized_who)) {
		zephyr->pending_zloc_names = g_list_append(zephyr->pending_zloc_names, normalized_who);
	} else {
		/* XXX deal with errors somehow */
		g_free(normalized_who);
	}
}

static void
zephyr_set_location(zephyr_account *zephyr, char *exposure)
{
	if (use_zeph02(zephyr)) {
		ZSetLocation(exposure);
	}
	else {
		gchar *zexpstr = g_strdup_printf("((tzcfodder . set-location) (hostname . \"%s\") (exposure . \"%s\"))\n",
		                                 zephyr->ourhost, exposure);
		zephyr_write_message(zephyr, zexpstr);
		g_free(zexpstr);
	}
}

static void
zephyr_set_status(PurpleProtocolServer *protocol_server,
                  PurpleAccount *account, PurpleStatus *status)
{
	PurpleConnection *gc = purple_account_get_connection(account);
	zephyr_account *zephyr = purple_connection_get_protocol_data(gc);
	PurpleStatusPrimitive primitive = purple_status_type_get_primitive(purple_status_get_status_type(status));

	g_free(zephyr->away);
	zephyr->away = NULL;

	if (primitive == PURPLE_STATUS_AWAY) {
		zephyr->away = g_strdup(purple_status_get_attr_string(status,"message"));
	}
	else if (primitive == PURPLE_STATUS_AVAILABLE) {
		zephyr_set_location(zephyr, zephyr->exposure);
	}
	else if (primitive == PURPLE_STATUS_INVISIBLE) {
		/* XXX handle errors */
		zephyr_set_location(zephyr, EXPOSE_OPSTAFF);
	}
}

static GList *zephyr_status_types(PurpleAccount *account)
{
	PurpleStatusType *type;
	GList *types = NULL;

	/* zephyr has several exposures
	   NONE (where you are hidden, and zephyrs to you are in practice silently dropped -- yes this is wrong)
	   OPSTAFF "hidden"
	   REALM-VISIBLE visible to people in local realm
	   REALM-ANNOUNCED REALM-VISIBLE+ plus your logins/logouts are announced to <login,username,*>
	   NET-VISIBLE REALM-ANNOUNCED, plus visible to people in foreign realm
	   NET-ANNOUNCED NET-VISIBLE, plus logins/logouts are announced	 to <login,username,*>

	   Online will set the user to the exposure they have in their options (defaulting to REALM-VISIBLE),
	   Hidden, will set the user's exposure to OPSTAFF

	   Away won't change their exposure but will set an auto away message (for IMs only)
	*/

	type = purple_status_type_new(PURPLE_STATUS_AVAILABLE, NULL, NULL, TRUE);
	types = g_list_append(types,type);

	type = purple_status_type_new(PURPLE_STATUS_INVISIBLE, NULL, NULL, TRUE);
	types = g_list_append(types,type);

	type = purple_status_type_new_with_attrs(
					       PURPLE_STATUS_AWAY, NULL, NULL, TRUE, TRUE, FALSE,
					       "message", _("Message"), purple_value_new(G_TYPE_STRING),
					       NULL);
	types = g_list_append(types, type);

	type = purple_status_type_new(PURPLE_STATUS_OFFLINE, NULL, NULL, TRUE);
	types = g_list_append(types,type);

	return types;
}

static GList *
zephyr_chat_info(PurpleProtocolChat *protocol_chat, PurpleConnection * gc) {
	GList *m = NULL;
	PurpleProtocolChatEntry *pce;

	pce = g_new0(PurpleProtocolChatEntry, 1);

	pce->label = _("_Class:");
	pce->identifier = "class";
	m = g_list_append(m, pce);

	pce = g_new0(PurpleProtocolChatEntry, 1);

	pce->label = _("_Instance:");
	pce->identifier = "instance";
	m = g_list_append(m, pce);

	pce = g_new0(PurpleProtocolChatEntry, 1);

	pce->label = _("_Recipient:");
	pce->identifier = "recipient";
	m = g_list_append(m, pce);

	return m;
}

/* Called when the server notifies us a message couldn't get sent */

static void zephyr_subscribe_failed(PurpleConnection *gc,char * z_class, char *z_instance, char * z_recipient, char* z_galaxy)
{
	gchar* subscribe_failed = g_strdup_printf(_("Attempt to subscribe to %s,%s,%s failed"), z_class, z_instance,z_recipient);
	purple_notify_error(gc,"", subscribe_failed, NULL, purple_request_cpar_from_connection(gc));
	g_free(subscribe_failed);
}

static char *
zephyr_get_chat_name(PurpleProtocolChat *protocol_chat, GHashTable *data) {
	gchar* zclass = g_hash_table_lookup(data,"class");
	gchar* inst = g_hash_table_lookup(data,"instance");
	gchar* recipient = g_hash_table_lookup(data, "recipient");
	if (!zclass) /* This should never happen */
		zclass = "";
	if (!inst)
		inst = "*";
	if (!recipient)
		recipient = "";
	return g_strdup_printf("%s,%s,%s",zclass,inst,recipient);
}


static void zephyr_join_chat(PurpleConnection * gc, GHashTable * data)
{
	/*	ZSubscription_t sub; */
	zephyr_triple *zt1, *zt2;
	const char *classname;
	const char *instname;
	const char *recip;
	zephyr_account *zephyr = purple_connection_get_protocol_data(gc);

	g_return_if_fail(zephyr != NULL);

	classname = g_hash_table_lookup(data, "class");
	instname = g_hash_table_lookup(data, "instance");
	recip = g_hash_table_lookup(data, "recipient");
	if (!classname)
		return;

	if (!g_ascii_strcasecmp(classname,"%host%"))
		classname = zephyr->ourhost;
	if (!g_ascii_strcasecmp(classname,"%canon%"))
		classname = zephyr->ourhostcanon;

	if (!instname || *instname == '\0')
		instname = "*";

	if (!g_ascii_strcasecmp(instname,"%host%"))
		instname = zephyr->ourhost;
	if (!g_ascii_strcasecmp(instname,"%canon%"))
		instname = zephyr->ourhostcanon;

	if (!recip || (*recip == '*'))
		recip = "";
	if (!g_ascii_strcasecmp(recip, "%me%"))
		recip = zephyr->username;

	zt1 = new_triple(zephyr,classname, instname, recip);
	zt2 = find_sub_by_triple(zephyr,zt1);
	if (zt2) {
		free_triple(zt1);
		if (!zt2->open) {
			if (!g_ascii_strcasecmp(instname,"*"))
				instname = "PERSONAL";
			purple_serv_got_joined_chat(gc, zt2->id, zt2->name);
			zephyr_chat_set_topic(gc,zt2->id,instname);
			zt2->open = TRUE;
		}
		return;
	}

	/*	sub.zsub_class = zt1->class;
		sub.zsub_classinst = zt1->instance;
		sub.zsub_recipient = zt1->recipient; */

	if (zephyr->subscribe_to(zephyr, zt1->class, zt1->instance, zt1->recipient) != ZERR_NONE) {
		/* XXX output better subscription information */
		zephyr_subscribe_failed(gc,zt1->class,zt1->instance,zt1->recipient,NULL);
		free_triple(zt1);
		return;
	}

	zephyr->subscrips = g_slist_append(zephyr->subscrips, zt1);
	zt1->open = TRUE;
	purple_serv_got_joined_chat(gc, zt1->id, zt1->name);
	if (!g_ascii_strcasecmp(instname,"*"))
		instname = "PERSONAL";
	zephyr_chat_set_topic(gc,zt1->id,instname);
}

static void
zephyr_protocol_chat_join(PurpleProtocolChat *protocol_chat,
                          PurpleConnection *gc, GHashTable * data)
{
	zephyr_join_chat(gc, data);
}

static void
zephyr_chat_leave(PurpleProtocolChat *protocol_chat, PurpleConnection * gc,
                  int id)
{
	zephyr_triple *zt;
	zephyr_account *zephyr = purple_connection_get_protocol_data(gc);
	zt = find_sub_by_id(zephyr,id);

	if (zt) {
		zt->open = FALSE;
		zt->id = ++(zephyr->last_id);
	}
}

static PurpleChat *
zephyr_find_blist_chat(PurpleProtocolClient *client, PurpleAccount *account,
                       const char *name)
{
	PurpleBlistNode *gnode, *cnode;

	/* XXX needs to be %host%,%canon%, and %me% clean */
	for (gnode = purple_blist_get_default_root(); gnode;
	     gnode = purple_blist_node_get_sibling_next(gnode)) {
		for(cnode = purple_blist_node_get_first_child(gnode);
				cnode;
				cnode = purple_blist_node_get_sibling_next(cnode)) {
			PurpleChat *chat = (PurpleChat*)cnode;
			const gchar *zclass, *inst, *recip;
			char** triple;
			GHashTable *components;
			if(!PURPLE_IS_CHAT(cnode))
				continue;
			if(purple_chat_get_account(chat) != account)
				continue;
			components = purple_chat_get_components(chat);
			if(!(zclass = g_hash_table_lookup(components, "class")))
				continue;
			if(!(inst = g_hash_table_lookup(components, "instance")))
				inst = "";
			if(!(recip = g_hash_table_lookup(components, "recipient")))
				recip = "";
			/*			purple_debug_info("zephyr","in zephyr_find_blist_chat name: %s\n",name?name:""); */
			triple = g_strsplit(name,",",3);
			if (!g_ascii_strcasecmp(triple[0], zclass) &&
			    !g_ascii_strcasecmp(triple[1], inst) &&
			    !g_ascii_strcasecmp(triple[2], recip)) {
				g_strfreev(triple);
				return chat;
			}
			g_strfreev(triple);
		}
	}
	return NULL;
}
static const char *zephyr_list_icon(PurpleAccount * a, PurpleBuddy * b)
{
	return "zephyr";
}

static unsigned int zephyr_send_typing(PurpleProtocolIM *im, PurpleConnection *gc, const char *who, PurpleIMTypingState state) {
	gchar *recipient;
	zephyr_account *zephyr = purple_connection_get_protocol_data(gc);
	if (use_tzc(zephyr))
		return 0;

	if (state == PURPLE_IM_NOT_TYPING)
		return 0;

	/* XXX We probably should care if this fails. Or maybe we don't want to */
	if (!who) {
		purple_debug_info("zephyr", "who is null\n");
		recipient = local_zephyr_normalize(zephyr,"");
	} else {
		char *comma = strrchr(who, ',');
		/* Don't ping broadcast (chat) recipients */
		/* The strrchr case finds a realm-stripped broadcast subscription
		   e.g. comma is the last character in the string */
		if (comma && ( (*(comma+1) == '\0') || (*(comma+1) == '@')))
			return 0;

		recipient = local_zephyr_normalize(zephyr,who);
	}

	purple_debug_info("zephyr","about to send typing notification to %s\n",recipient);
	zephyr_send_message(zephyr,"MESSAGE","PERSONAL",recipient,"","","PING");
	purple_debug_info("zephyr","sent typing notification\n");

	/*
	 * TODO: Is this correct?  It means we will call
	 *       purple_serv_send_typing(gc, who, PURPLE_IM_TYPING) once every 15 seconds
	 *       until the Purple user stops typing.
	 */
	return ZEPHYR_TYPING_SEND_TIMEOUT;
}



static void zephyr_chat_set_topic(PurpleConnection * gc, int id, const char *topic)
{
	zephyr_triple *zt;
	PurpleChatConversation *gcc;
	gchar *topic_utf8;
	zephyr_account* zephyr = purple_connection_get_protocol_data(gc);
	char *sender = (char *)zephyr->username;

	zt = find_sub_by_id(zephyr,id);
	/* find_sub_by_id can return NULL */
	if (!zt)
		return;
	gcc = purple_conversations_find_chat_with_account(zt->name,
												purple_connection_get_account(gc));

	topic_utf8 = zephyr_recv_convert(gc,(gchar *)topic);
	purple_chat_conversation_set_topic(gcc,sender,topic_utf8);
	g_free(topic_utf8);
}

static void
zephyr_protocol_chat_set_topic(PurpleProtocolChat *protocol_chat,
                               PurpleConnection *gc, int id, const char *topic)
{
	zephyr_chat_set_topic(gc, id, topic);
}

/*  commands */

static PurpleCmdRet zephyr_purple_cmd_msg(PurpleConversation *conv,
				      const char *cmd, char **args, char **error, void *data)
{
	char *recipient;
	PurpleCmdRet ret;
	PurpleConnection *gc = purple_conversation_get_connection(conv);
	zephyr_account *zephyr = purple_connection_get_protocol_data(gc);;
	if (!g_ascii_strcasecmp(args[0],"*"))
		return PURPLE_CMD_RET_FAILED;  /* "*" is not a valid argument */
	else
		recipient = local_zephyr_normalize(zephyr,args[0]);

	if (strlen(recipient) < 1) {
		g_free(recipient);
		return PURPLE_CMD_RET_FAILED; /* a null recipient is a chat message, not an IM */
	}

	if (zephyr_send_message(zephyr,"MESSAGE","PERSONAL",recipient,args[1],zephyr_get_signature(),""))
		ret = PURPLE_CMD_RET_OK;
	else
		ret = PURPLE_CMD_RET_FAILED;
	g_free(recipient);
	return ret;
}

static PurpleCmdRet zephyr_purple_cmd_zlocate(PurpleConversation *conv,
					  const char *cmd, char **args, char **error, void *data)
{
	zephyr_zloc(NULL, purple_conversation_get_connection(conv),args[0]);
	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet zephyr_purple_cmd_instance(PurpleConversation *conv,
					   const char *cmd, char **args, char **error, void *data)
{
	/* Currently it sets the instance with leading spaces and
	 * all. This might not be the best thing to do, though having
	 * one word isn't ideal either.	 */

	const char* instance = args[0];
	zephyr_chat_set_topic(purple_conversation_get_connection(conv),
			purple_chat_conversation_get_id(PURPLE_CHAT_CONVERSATION(conv)),instance);
	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet zephyr_purple_cmd_joinchat_cir(PurpleConversation *conv,
					       const char *cmd, char **args, char **error, void *data)
{
	/* Join a new zephyr chat */
	GHashTable *triple = g_hash_table_new(NULL,NULL);
	g_hash_table_insert(triple,"class",args[0]);
	g_hash_table_insert(triple,"instance",args[1]);
	g_hash_table_insert(triple,"recipient",args[2]);
	zephyr_join_chat(purple_conversation_get_connection(conv),triple);
	return PURPLE_CMD_RET_OK;
}

static PurpleCmdRet zephyr_purple_cmd_zi(PurpleConversation *conv,
				     const char *cmd, char **args, char **error, void *data)
{
	/* args = instance, message */
	PurpleConnection *gc = purple_conversation_get_connection(conv);
	zephyr_account *zephyr = purple_connection_get_protocol_data(gc);
	if ( zephyr_send_message(zephyr,"message",args[0],"",args[1],zephyr_get_signature(),""))
		return PURPLE_CMD_RET_OK;
	else
		return PURPLE_CMD_RET_FAILED;
}

static PurpleCmdRet zephyr_purple_cmd_zci(PurpleConversation *conv,
				      const char *cmd, char **args, char **error, void *data)
{
	/* args = class, instance, message */
	PurpleConnection *gc = purple_conversation_get_connection(conv);
	zephyr_account *zephyr = purple_connection_get_protocol_data(gc);
	if ( zephyr_send_message(zephyr,args[0],args[1],"",args[2],zephyr_get_signature(),""))
		return PURPLE_CMD_RET_OK;
	else
		return PURPLE_CMD_RET_FAILED;
}

static PurpleCmdRet zephyr_purple_cmd_zcir(PurpleConversation *conv,
				       const char *cmd, char **args, char **error, void *data)
{
	/* args = class, instance, recipient, message */
	PurpleConnection *gc = purple_conversation_get_connection(conv);
	zephyr_account *zephyr = purple_connection_get_protocol_data(gc);
	if ( zephyr_send_message(zephyr,args[0],args[1],args[2],args[3],zephyr_get_signature(),""))
		return PURPLE_CMD_RET_OK;
	else
		return PURPLE_CMD_RET_FAILED;
}

static PurpleCmdRet zephyr_purple_cmd_zir(PurpleConversation *conv,
				      const char *cmd, char **args, char **error, void *data)
{
	/* args = instance, recipient, message */
	PurpleConnection *gc = purple_conversation_get_connection(conv);
	zephyr_account *zephyr = purple_connection_get_protocol_data(gc);
	if ( zephyr_send_message(zephyr,"message",args[0],args[1],args[2],zephyr_get_signature(),""))
		return PURPLE_CMD_RET_OK;
	else
		return PURPLE_CMD_RET_FAILED;
}

static PurpleCmdRet zephyr_purple_cmd_zc(PurpleConversation *conv,
				     const char *cmd, char **args, char **error, void *data)
{
	/* args = class, message */
	PurpleConnection *gc = purple_conversation_get_connection(conv);
	zephyr_account *zephyr = purple_connection_get_protocol_data(gc);
	if ( zephyr_send_message(zephyr,args[0],"PERSONAL","",args[1],zephyr_get_signature(),""))
		return PURPLE_CMD_RET_OK;
	else
		return PURPLE_CMD_RET_FAILED;
}

static void zephyr_register_slash_commands(void)
{
	PurpleCmdId id;

	id = purple_cmd_register("msg","ws", PURPLE_CMD_P_PROTOCOL,
			  PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PROTOCOL_ONLY,
			  "prpl-zephyr",
			  zephyr_purple_cmd_msg, _("msg &lt;nick&gt; &lt;message&gt;:  Send a private message to a user"), NULL);
	cmds = g_slist_prepend(cmds, GUINT_TO_POINTER(id));

	id = purple_cmd_register("zlocate","w", PURPLE_CMD_P_PROTOCOL,
			  PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PROTOCOL_ONLY,
			  "prpl-zephyr",
			  zephyr_purple_cmd_zlocate, _("zlocate &lt;nick&gt;: Locate user"), NULL);
	cmds = g_slist_prepend(cmds, GUINT_TO_POINTER(id));

	id = purple_cmd_register("zl","w", PURPLE_CMD_P_PROTOCOL,
			  PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PROTOCOL_ONLY,
			  "prpl-zephyr",
			  zephyr_purple_cmd_zlocate, _("zl &lt;nick&gt;: Locate user"), NULL);
	cmds = g_slist_prepend(cmds, GUINT_TO_POINTER(id));

	id = purple_cmd_register("instance","s", PURPLE_CMD_P_PROTOCOL,
			  PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PROTOCOL_ONLY,
			  "prpl-zephyr",
			  zephyr_purple_cmd_instance, _("instance &lt;instance&gt;: Set the instance to be used on this class"), NULL);
	cmds = g_slist_prepend(cmds, GUINT_TO_POINTER(id));

	id = purple_cmd_register("inst","s", PURPLE_CMD_P_PROTOCOL,
			  PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PROTOCOL_ONLY,
			  "prpl-zephyr",
			  zephyr_purple_cmd_instance, _("inst &lt;instance&gt;: Set the instance to be used on this class"), NULL);
	cmds = g_slist_prepend(cmds, GUINT_TO_POINTER(id));

	id = purple_cmd_register("topic","s", PURPLE_CMD_P_PROTOCOL,
			  PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PROTOCOL_ONLY,
			  "prpl-zephyr",
			  zephyr_purple_cmd_instance, _("topic &lt;instance&gt;: Set the instance to be used on this class"), NULL);
	cmds = g_slist_prepend(cmds, GUINT_TO_POINTER(id));

	id = purple_cmd_register("sub", "www", PURPLE_CMD_P_PROTOCOL,
			  PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PROTOCOL_ONLY,
			  "prpl-zephyr",
			  zephyr_purple_cmd_joinchat_cir,
			  _("sub &lt;class&gt; &lt;instance&gt; &lt;recipient&gt;: Join a new chat"), NULL);
	cmds = g_slist_prepend(cmds, GUINT_TO_POINTER(id));

	id = purple_cmd_register("zi","ws", PURPLE_CMD_P_PROTOCOL,
			  PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PROTOCOL_ONLY,
			  "prpl-zephyr",
			  zephyr_purple_cmd_zi, _("zi &lt;instance&gt;: Send a message to &lt;message,<i>instance</i>,*&gt;"), NULL);
	cmds = g_slist_prepend(cmds, GUINT_TO_POINTER(id));

	id = purple_cmd_register("zci","wws",PURPLE_CMD_P_PROTOCOL,
			  PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PROTOCOL_ONLY,
			  "prpl-zephyr",
			  zephyr_purple_cmd_zci,
			  _("zci &lt;class&gt; &lt;instance&gt;: Send a message to &lt;<i>class</i>,<i>instance</i>,*&gt;"), NULL);
	cmds = g_slist_prepend(cmds, GUINT_TO_POINTER(id));

	id = purple_cmd_register("zcir","wwws",PURPLE_CMD_P_PROTOCOL,
			  PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PROTOCOL_ONLY,
			  "prpl-zephyr",
			  zephyr_purple_cmd_zcir,
			  _("zcir &lt;class&gt; &lt;instance&gt; &lt;recipient&gt;: Send a message to &lt;<i>class</i>,<i>instance</i>,<i>recipient</i>&gt;"), NULL);
	cmds = g_slist_prepend(cmds, GUINT_TO_POINTER(id));

	id = purple_cmd_register("zir","wws",PURPLE_CMD_P_PROTOCOL,
			  PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PROTOCOL_ONLY,
			  "prpl-zephyr",
			  zephyr_purple_cmd_zir,
			  _("zir &lt;instance&gt; &lt;recipient&gt;: Send a message to &lt;MESSAGE,<i>instance</i>,<i>recipient</i>&gt;"), NULL);
	cmds = g_slist_prepend(cmds, GUINT_TO_POINTER(id));

	id = purple_cmd_register("zc","ws", PURPLE_CMD_P_PROTOCOL,
			  PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_CHAT | PURPLE_CMD_FLAG_PROTOCOL_ONLY,
			  "prpl-zephyr",
			  zephyr_purple_cmd_zc, _("zc &lt;class&gt;: Send a message to &lt;<i>class</i>,PERSONAL,*&gt;"), NULL);
	cmds = g_slist_prepend(cmds, GUINT_TO_POINTER(id));
}


static void zephyr_unregister_slash_commands(void)
{
	g_slist_free_full(cmds, (GDestroyNotify)purple_cmd_unregister);
}


/* Resubscribe to the in-memory list of subscriptions and also unsubscriptions */
static void
zephyr_action_resubscribe(PurpleProtocolAction *action)
{
	zephyr_account *zephyr = purple_connection_get_protocol_data(action->connection);

	for (GSList *s = zephyr->subscrips; s; s = s->next) {
		zephyr_triple *zt = s->data;
		/* XXX We really should care if this fails */
		zephyr->subscribe_to(zephyr, zt->class, zt->instance, zt->recipient);
	}
	/* XXX handle unsubscriptions */
}


static void zephyr_action_get_subs_from_server(PurpleProtocolAction *action)
{
	PurpleConnection *gc = action->connection;
	zephyr_account *zephyr = purple_connection_get_protocol_data(gc);
	gchar *title;
	int retval, nsubs, one,i;
	ZSubscription_t subs;
	if (use_zeph02(zephyr)) {
		GString *subout;

		if (zephyr->port == 0) {
			purple_debug_error("zephyr", "error while retrieving port\n");
			return;
		}
		if ((retval = ZRetrieveSubscriptions(zephyr->port,&nsubs)) != ZERR_NONE) {
			/* XXX better error handling */
			purple_debug_error("zephyr", "error while retrieving subscriptions from server\n");
			return;
		}

		title = g_strdup_printf("Server subscriptions for %s",
		                        zephyr->username);
		subout = g_string_new("Subscription list<br>");
		for(i=0;i<nsubs;i++) {
			one = 1;
			if ((retval = ZGetSubscriptions(&subs,&one)) != ZERR_NONE) {
				/* XXX better error handling */
				g_free(title);
				g_string_free(subout, TRUE);
				purple_debug_error("zephyr", "error while retrieving individual subscription\n");
				return;
			}
			g_string_append_printf(subout, "Class %s Instance %s Recipient %s<br>",
					       subs.zsub_class, subs.zsub_classinst,
					       subs.zsub_recipient);
		}
		purple_notify_formatted(gc, title, title, NULL,  subout->str, NULL, NULL);
		g_free(title);
		g_string_free(subout, TRUE);
	} else {
		/* XXX fix */
		purple_notify_error(gc, "", "tzc doesn't support this action",
			NULL, purple_request_cpar_from_connection(gc));
	}
}


static GList *
zephyr_get_actions(PurpleProtocolClient *client, PurpleConnection *gc) {
	GList *list = NULL;
	PurpleProtocolAction *act = NULL;

	act = purple_protocol_action_new(_("Resubscribe"), zephyr_action_resubscribe);
	list = g_list_append(list, act);

	act = purple_protocol_action_new(_("Retrieve subscriptions from server"), zephyr_action_get_subs_from_server);
	list = g_list_append(list,act);

	return list;
}


static void
zephyr_protocol_init(ZephyrProtocol *self)
{
	PurpleProtocol *protocol = PURPLE_PROTOCOL(self);
	PurpleAccountOption *option;
	const gchar *tmp = get_exposure_level();

	protocol->id      = "prpl-zephyr";
	protocol->name    = "Zephyr";
	protocol->options = OPT_PROTO_CHAT_TOPIC | OPT_PROTO_NO_PASSWORD;

	option = purple_account_option_bool_new(_("Use tzc"), "use_tzc", FALSE);
	protocol->account_options = g_list_append(protocol->account_options, option);

	option = purple_account_option_string_new(_("tzc command"), "tzc_command", "/usr/bin/tzc -e %s");
	protocol->account_options = g_list_append(protocol->account_options, option);

	option = purple_account_option_bool_new(_("Export to .anyone"), "write_anyone", FALSE);
	protocol->account_options = g_list_append(protocol->account_options, option);

	option = purple_account_option_bool_new(_("Export to .zephyr.subs"), "write_zsubs", FALSE);
	protocol->account_options = g_list_append(protocol->account_options, option);

	option = purple_account_option_bool_new(_("Import from .anyone"), "read_anyone", TRUE);
	protocol->account_options = g_list_append(protocol->account_options, option);

	option = purple_account_option_bool_new(_("Import from .zephyr.subs"), "read_zsubs", TRUE);
	protocol->account_options = g_list_append(protocol->account_options, option);

	option = purple_account_option_string_new(_("Realm"), "realm", "");
	protocol->account_options = g_list_append(protocol->account_options, option);

	option = purple_account_option_string_new(_("Exposure"), "exposure_level",
	                                          tmp);
	protocol->account_options = g_list_append(protocol->account_options, option);

	option = purple_account_option_string_new(_("Encoding"), "encoding", ZEPHYR_FALLBACK_CHARSET);
	protocol->account_options = g_list_append(protocol->account_options, option);
}


static void
zephyr_protocol_class_init(ZephyrProtocolClass *klass)
{
	PurpleProtocolClass *protocol_class = PURPLE_PROTOCOL_CLASS(klass);

	protocol_class->login = zephyr_login;
	protocol_class->close = zephyr_close;
	protocol_class->status_types = zephyr_status_types;
	protocol_class->list_icon = zephyr_list_icon;
}


static void
zephyr_protocol_class_finalize(G_GNUC_UNUSED ZephyrProtocolClass *klass)
{
}


static void
zephyr_protocol_client_iface_init(PurpleProtocolClientInterface *client_iface)
{
	client_iface->get_actions     = zephyr_get_actions;
	client_iface->normalize       = zephyr_normalize;
	client_iface->find_blist_chat = zephyr_find_blist_chat;
}


static void
zephyr_protocol_server_iface_init(PurpleProtocolServerInterface *server_iface)
{
	server_iface->get_info   = zephyr_zloc;
	server_iface->set_status = zephyr_set_status;

	server_iface->set_info       = NULL; /* XXX Location? */
	server_iface->set_buddy_icon = NULL; /* XXX */
}


static void
zephyr_protocol_im_iface_init(PurpleProtocolIMInterface *im_iface)
{
	im_iface->send        = zephyr_send_im;
	im_iface->send_typing = zephyr_send_typing;
}


static void
zephyr_protocol_chat_iface_init(PurpleProtocolChatInterface *chat_iface)
{
	chat_iface->info      = zephyr_chat_info;
	chat_iface->join      = zephyr_protocol_chat_join;
	chat_iface->get_name  = zephyr_get_chat_name;
	chat_iface->leave     = zephyr_chat_leave;
	chat_iface->send      = zephyr_chat_send;
	chat_iface->set_topic = zephyr_protocol_chat_set_topic;

	chat_iface->get_user_real_name = NULL; /* XXX */
}


G_DEFINE_DYNAMIC_TYPE_EXTENDED(
        ZephyrProtocol, zephyr_protocol, PURPLE_TYPE_PROTOCOL, 0,

        G_IMPLEMENT_INTERFACE_DYNAMIC(PURPLE_TYPE_PROTOCOL_CLIENT,
                                      zephyr_protocol_client_iface_init)

        G_IMPLEMENT_INTERFACE_DYNAMIC(PURPLE_TYPE_PROTOCOL_SERVER,
                                      zephyr_protocol_server_iface_init)

        G_IMPLEMENT_INTERFACE_DYNAMIC(PURPLE_TYPE_PROTOCOL_IM,
                                      zephyr_protocol_im_iface_init)

        G_IMPLEMENT_INTERFACE_DYNAMIC(PURPLE_TYPE_PROTOCOL_CHAT,
                                      zephyr_protocol_chat_iface_init));

static PurplePluginInfo *plugin_query(GError **error)
{
	return purple_plugin_info_new(
		"id",           "prpl-zephyr",
		"name",         "Zephyr Protocol",
		"version",      DISPLAY_VERSION,
		"category",     N_("Protocol"),
		"summary",      N_("Zephyr Protocol Plugin"),
		"description",  N_("Zephyr Protocol Plugin"),
		"website",      PURPLE_WEBSITE,
		"abi-version",  PURPLE_ABI_VERSION,
		"flags",        PURPLE_PLUGIN_INFO_FLAGS_INTERNAL |
		                PURPLE_PLUGIN_INFO_FLAGS_AUTO_LOAD,
		NULL
	);
}


static gboolean
plugin_load(PurplePlugin *plugin, GError **error)
{
	zephyr_protocol_register_type(G_TYPE_MODULE(plugin));

	my_protocol = purple_protocols_add(ZEPHYR_TYPE_PROTOCOL, error);
	if (!my_protocol)
		return FALSE;

	zephyr_register_slash_commands();

	return TRUE;
}


static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	zephyr_unregister_slash_commands();

	if (!purple_protocols_remove(my_protocol, error))
		return FALSE;

	return TRUE;
}


PURPLE_PLUGIN_INIT(zephyr, plugin_query, plugin_load, plugin_unload);
