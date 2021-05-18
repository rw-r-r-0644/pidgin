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
#include "zephyr_account.h"
#include "zephyr_html.h"
#include "zephyr_tzc.h"
#include "zephyr_zeph02.h"

#define ZEPHYR_FALLBACK_CHARSET "ISO-8859-1"

#define BUF_LEN (2048)

/* these are deliberately high, since most people don't send multiple "PING"s */
#define ZEPHYR_TYPING_SEND_TIMEOUT 15
#define ZEPHYR_TYPING_RECV_TIMEOUT 10

static PurpleProtocol *my_protocol = NULL;
static GSList *cmds = NULL;

#ifdef LIBZEPHYR_EXT
extern char __Zephyr_realm[];
#endif

typedef struct _zephyr_triple zephyr_triple;

typedef gboolean (*ZephyrLoginFunc)(zephyr_account *zephyr);

struct _ZephyrProtocol {
	PurpleProtocol parent;
};

struct _zephyr_triple {
	ZSubscription_t sub;
	char *name;
	gboolean open;
	int id;
};

#ifdef WIN32
extern const char *username;
#endif

static void zephyr_chat_set_topic(PurpleConnection *gc, int id, const char *topic);

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

static zephyr_triple *
zephyr_triple_new(zephyr_account *zephyr, const ZSubscription_t *sub)
{
	zephyr_triple *zt;

	zt = g_new0(zephyr_triple, 1);
	zt->sub.zsub_class = g_strdup(sub->zsub_class);
	zt->sub.zsub_classinst = g_strdup(sub->zsub_classinst);
	zt->sub.zsub_recipient = g_strdup(sub->zsub_recipient);
	zt->name = g_strdup_printf("%s,%s,%s", sub->zsub_class,
	                           sub->zsub_classinst ? sub->zsub_classinst : "",
	                           sub->zsub_recipient ? sub->zsub_recipient : "");
	zt->id = ++(zephyr->last_id);
	zt->open = FALSE;
	return zt;
}

static void
zephyr_triple_free(zephyr_triple *zt)
{
	g_free(zt->sub.zsub_class);
	g_free(zt->sub.zsub_classinst);
	g_free(zt->sub.zsub_recipient);
	g_free(zt->name);
	g_free(zt);
}

/* returns 0 if sub is a subset of zt.sub.  This function is used to
   determine whether a zephyr sent to sub should be placed in the chat
   with triple zt.sub

   sub is a subset of zt.sub
   iff. the classnames are identical ignoring case
   AND. the instance names are identical (ignoring case), or zt.sub->instance is *.
   AND. the recipient names are identical
*/
static gint
zephyr_triple_subset(const zephyr_triple *zt, const ZSubscription_t *sub)
{
	if (!sub->zsub_class) {
		purple_debug_error("zephyr", "sub1c doesn't exist\n");
		return 1;
	}
	if (!sub->zsub_classinst) {
		purple_debug_error("zephyr", "sub1i doesn't exist\n");
		return 1;
	}
	if (!sub->zsub_recipient) {
		purple_debug_error("zephyr", "sub1r doesn't exist\n");
		return 1;
	}
	if (!zt->sub.zsub_class) {
		purple_debug_error("zephyr", "sub2c doesn't exist\n");
		return 1;
	}
	if (!zt->sub.zsub_classinst) {
		purple_debug_error("zephyr", "sub2i doesn't exist\n");
		return 1;
	}
	if (!zt->sub.zsub_recipient) {
		purple_debug_error("zephyr", "sub2r doesn't exist\n");
		return 1;
	}

	if (g_ascii_strcasecmp(zt->sub.zsub_class, sub->zsub_class)) {
		return 1;
	}
	if (g_ascii_strcasecmp(zt->sub.zsub_classinst, sub->zsub_classinst) && g_ascii_strcasecmp(zt->sub.zsub_classinst, "*")) {
		return 1;
	}
	if (g_ascii_strcasecmp(zt->sub.zsub_recipient, sub->zsub_recipient)) {
		return 1;
	}
	purple_debug_info("zephyr", "<%s,%s,%s> is in <%s,%s,%s>\n",
	                  sub->zsub_class, sub->zsub_classinst, sub->zsub_recipient,
	                  zt->sub.zsub_class, zt->sub.zsub_classinst, zt->sub.zsub_recipient);
	return 0;
}

/*
   Converts strings to utf-8 if necessary using user specified encoding
*/

static gchar *
convert_to_utf8(const gchar *string, const gchar *from_encoding)
{
	gchar *utf8;
	GError *err = NULL;

	if (g_utf8_validate(string, -1, NULL)) {
		return g_strdup(string);
	}

	utf8 = g_convert(string, -1, "UTF-8", from_encoding, NULL, NULL, &err);
	if (err) {
		purple_debug_error("zephyr", "recv conversion error: %s\n", err->message);
		utf8 = g_strdup(_("(There was an error converting this message.	 Check the 'Encoding' option in the Account Editor)"));
		g_error_free(err);
	}

	return utf8;
}

static gboolean
pending_zloc(zephyr_account *zephyr, const char *who)
{
	GList *curr;
	char* normalized_who = zephyr_normalize_local_realm(zephyr, who);

	curr = g_list_find_custom(zephyr->pending_zloc_names, normalized_who, (GCompareFunc)g_ascii_strcasecmp);
	g_free(normalized_who);
	if (curr == NULL)
		return FALSE;

	g_free((char *)curr->data);
	zephyr->pending_zloc_names = g_list_delete_link(zephyr->pending_zloc_names, curr);
	return TRUE;
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

static void
zephyr_triple_open(zephyr_triple *zt, PurpleConnection *gc, const char *instance)
{
	zt->open = TRUE;
	purple_serv_got_joined_chat(gc, zt->id, zt->name);
	zephyr_chat_set_topic(gc, zt->id, instance);
}

void
handle_message(PurpleConnection *gc, ZNotice_t *notice)
{
	zephyr_account* zephyr = purple_connection_get_protocol_data(gc);

	if (!g_ascii_strcasecmp(notice->z_class, LOGIN_CLASS)) {
		/* well, we'll be updating in 20 seconds anyway, might as well ignore this. */
	} else if (!g_ascii_strcasecmp(notice->z_class, LOCATE_CLASS)) {
		if (!g_ascii_strcasecmp(notice->z_opcode, LOCATE_LOCATE)) {
			int nlocs;
			char *user;

			/* XXX add real error reporting */
			if (ZParseLocations(notice, NULL, &nlocs, &user) != ZERR_NONE)
				return;

			handle_locations(gc, user, nlocs, NULL);
			g_free(user);
		}
	} else {
		char *buf;
		int len;
		char *stripped_sender;
		int signature_length = strlen(notice->z_message);

		/* Need to deal with 0 length  messages to handle typing notification (OPCODE) ping messages */
		/* One field zephyrs would have caused purple to crash */
		if ((notice->z_message_len == 0) || (notice->z_message_len <= (signature_length + 1))) {
			len = 0;
			buf = g_strdup("");
		} else {
			char *tmpbuf;
			char *ptr = (char *) notice->z_message + (signature_length + 1);
			gchar *tmpescape;

			len = notice->z_message_len - (signature_length + 1);
			tmpbuf = g_malloc(len + 1);
			g_snprintf(tmpbuf, len + 1, "%s", ptr);
			g_strchomp(tmpbuf);
			tmpescape = g_markup_escape_text(tmpbuf, -1);
			g_free(tmpbuf);
			tmpbuf = zephyr_to_html(tmpescape);
			buf = convert_to_utf8(tmpbuf, zephyr->encoding);
			g_free(tmpbuf);
			g_free(tmpescape);
		}
		purple_debug_info("zephyr", "message_size %d %d %d", len, notice->z_message_len, signature_length);

		stripped_sender = zephyr_strip_local_realm(zephyr, notice->z_sender);

		if (!g_ascii_strcasecmp(notice->z_class, "MESSAGE") &&
		    !g_ascii_strcasecmp(notice->z_class_inst, "PERSONAL") &&
		    !g_ascii_strcasecmp(notice->z_recipient, zephyr->username)) {
			PurpleMessageFlags flags = 0;

			if (!g_ascii_strcasecmp(notice->z_message, "Automated reply:"))
				flags |= PURPLE_MESSAGE_AUTO_RESP;

			if (!g_ascii_strcasecmp(notice->z_opcode, "PING"))
				purple_serv_got_typing(gc,stripped_sender,ZEPHYR_TYPING_RECV_TIMEOUT, PURPLE_IM_TYPING);
			else
				purple_serv_got_im(gc, stripped_sender, buf, flags, time(NULL));

		} else {
			ZSubscription_t sub = {
			        .zsub_class = notice->z_class,
			        .zsub_classinst = (gchar *)notice->z_class_inst,
			        .zsub_recipient = (gchar *)notice->z_recipient
			};
			zephyr_triple *zt;
			gchar *send_inst_utf8;
			GSList *l = g_slist_find_custom(zephyr->subscrips, &sub, (GCompareFunc)zephyr_triple_subset);
			PurpleConversation *gcc;

			if (!l) {
				/* This is a server supplied subscription */
				zt = zephyr_triple_new(zephyr, &sub);
				zephyr->subscrips = g_slist_append(zephyr->subscrips, zt);
			} else {
				zt = l->data;
			}

			if (!zt->open) {
				zephyr_triple_open(zt, gc, notice->z_class_inst);
			}

			if (!g_ascii_strcasecmp(notice->z_class_inst, "PERSONAL"))
				send_inst_utf8 = g_strdup(stripped_sender);
			else {
				char *send_inst = g_strdup_printf("[%s] %s", notice->z_class_inst, stripped_sender);
				send_inst_utf8 = convert_to_utf8(send_inst, zephyr->encoding);
				g_free(send_inst);
				if (!send_inst_utf8) {
					purple_debug_error("zephyr","Failed to convert instance for sender %s.\n", stripped_sender);
					send_inst_utf8 = g_strdup(stripped_sender);
				}
			}

			gcc = purple_conversations_find_chat_with_account(zt->name, purple_connection_get_account(gc));
			if (!purple_chat_conversation_has_user(PURPLE_CHAT_CONVERSATION(gcc), stripped_sender)) {
				GInetAddress *inet_addr = NULL;
				gchar *ipaddr = NULL;

				inet_addr = g_inet_address_new_from_bytes(
				        (const guint8 *)&notice->z_sender_addr,
				        G_SOCKET_FAMILY_IPV4);
				ipaddr = g_inet_address_to_string(inet_addr);
				purple_chat_conversation_add_user(PURPLE_CHAT_CONVERSATION(gcc),
				                                  stripped_sender, ipaddr,
				                                  PURPLE_CHAT_USER_NONE, TRUE);
				g_free(ipaddr);
				g_object_unref(inet_addr);
			}
			purple_serv_got_chat_in(gc, zt->id, send_inst_utf8,
			        PURPLE_MESSAGE_RECV, buf, time(NULL));
			g_free(send_inst_utf8);
		}
		g_free(stripped_sender);
		g_free(buf);
	}
}

void
handle_locations(PurpleConnection *gc, const gchar *user, int nlocs, const ZLocations_t *zloc)
{
	zephyr_account *zephyr = purple_connection_get_protocol_data(gc);
	PurpleBuddy *b;
	const char *bname;
	const gchar *name;

	b = find_buddy(zephyr, user);
	bname = b ? purple_buddy_get_name(b) : NULL;
	name = b ? bname : user;
	if ((b && pending_zloc(zephyr, bname)) || pending_zloc(zephyr, user)) {
		PurpleNotifyUserInfo *user_info = purple_notify_user_info_new();
		const char *balias;

		/* TODO: Check whether it's correct to call add_pair_html,
			 or if we should be using add_pair_plaintext */
		purple_notify_user_info_add_pair_html(user_info, _("User"), name);

		balias = b ? purple_buddy_get_local_alias(b) : NULL;
		if (balias)
			purple_notify_user_info_add_pair_plaintext(user_info, _("Alias"), balias);

		if (!nlocs) {
			purple_notify_user_info_add_pair_plaintext(user_info, NULL, _("Hidden or not logged-in"));
		}
		for (; nlocs > 0; nlocs--) {
			ZLocations_t locs;
			char *tmp;

			if (!zloc) {
				/* XXX add real error reporting */
				int one = 1;

				ZGetLocations(&locs, &one);
			} else {
				locs = *zloc;
			}

			/* TODO: Need to escape locs.host and locs.time? */
			tmp = g_strdup_printf(_("<br>At %s since %s"), locs.host, locs.time);
			purple_notify_user_info_add_pair_html(user_info, _("Location"), tmp);
			g_free(tmp);
		}
		purple_notify_userinfo(gc, name, user_info, NULL, NULL);
		purple_notify_user_info_destroy(user_info);
	} else {
		purple_protocol_got_user_status(zephyr->account, name, (nlocs > 0) ? "available" : "offline", NULL);
	}
}

static void
check_loc_buddy(PurpleBuddy *buddy, zephyr_account *zephyr)
{
	const char *bname = purple_buddy_get_name(buddy);
	char *chk = zephyr_normalize_local_realm(zephyr, bname);
#ifdef WIN32
	int numlocs;

	ZLocateUser(chk, &numlocs, ZAUTH);
	for (int i = 0; i < numlocs; i++) {
		ZLocations_t locations;
		int one = 1;

		ZGetLocations(&locations, &one);
		serv_got_update(zgc, bname, 1, 0, 0, 0, 0);
	}
#else

	purple_debug_info("zephyr", "chk: %s, bname: %s", chk, bname);
	/* XXX add real error reporting */
	/* doesn't matter if this fails or not; we'll just move on to the next one */
	zephyr->request_locations(zephyr, chk);
#endif /* WIN32 */

	g_free(chk);
}

static gboolean
check_loc(gpointer data)
{
	zephyr_account *zephyr = (zephyr_account *)data;
	GSList *buddies = purple_blist_find_buddies(zephyr->account, NULL);

	g_slist_foreach(buddies, (GFunc)check_loc_buddy, zephyr);
	g_slist_free(buddies);

	return G_SOURCE_CONTINUE;
}

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

	zephyr->ourhost = g_strdup(g_get_host_name());
	if (!(hent = gethostbyname(zephyr->ourhost))) {
		purple_debug_error("zephyr",
		                   "unable to resolve hostname, %%canon%% will be "
		                   "wrong in subscriptions and has been set to the "
		                   "value of %%host%%, %s",
		                   zephyr->ourhost);
		zephyr->ourhostcanon = g_strdup(zephyr->ourhost);
		return;
	}

	zephyr->ourhostcanon = g_strdup(hent->h_name);
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
		ZSubscription_t sub;

		while (fgets(buff, BUFSIZ, f)) {
			strip_comments(buff);
			if (buff[0]) {
				triple = g_strsplit(buff, ",", 3);
				if (triple[0] && triple[1]) {
					char *tmp = g_strdup(zephyr->username);
					char *atptr;

					if (triple[2] == NULL) {
						sub.zsub_recipient = g_malloc0(1);
					} else if (!g_ascii_strcasecmp(triple[2], "%me%")) {
						sub.zsub_recipient = g_strdup(zephyr->username);
					} else if (!g_ascii_strcasecmp(triple[2], "*")) {
						/* wildcard
						 * form of class,instance,* */
						sub.zsub_recipient = g_malloc0(1);
					} else if (!g_ascii_strcasecmp(triple[2], tmp)) {
						/* form of class,instance,aatharuv@ATHENA.MIT.EDU */
						sub.zsub_recipient = g_strdup(triple[2]);
					} else if ((atptr = strchr(triple[2], '@')) != NULL) {
						/* form of class,instance,*@ANDREW.CMU.EDU
						 * class,instance,@ANDREW.CMU.EDU
						 * If realm is local realm, blank recipient, else
						 * @REALM-NAME
						 */
						char *realmat = g_strdup_printf("@%s",zephyr->realm);

						if (!g_ascii_strcasecmp(atptr, realmat)) {
							sub.zsub_recipient = g_malloc0(1);
						} else {
							sub.zsub_recipient = g_strdup(atptr);
						}
						g_free(realmat);
					} else {
						sub.zsub_recipient = g_strdup(triple[2]);
					}
					g_free(tmp);

					if (!g_ascii_strcasecmp(triple[0], "%host%")) {
						sub.zsub_class = g_strdup(zephyr->ourhost);
					} else if (!g_ascii_strcasecmp(triple[0], "%canon%")) {
						sub.zsub_class = g_strdup(zephyr->ourhostcanon);
					} else {
						sub.zsub_class = g_strdup(triple[0]);
					}

					if (!g_ascii_strcasecmp(triple[1], "%host%")) {
						sub.zsub_classinst = g_strdup(zephyr->ourhost);
					} else if (!g_ascii_strcasecmp(triple[1], "%canon%")) {
						sub.zsub_classinst = g_strdup(zephyr->ourhostcanon);
					} else {
						sub.zsub_classinst = g_strdup(triple[1]);
					}

					/* There should be some sort of error report listing classes that couldn't be subbed to.
					   Not important right now though */

					if (!zephyr->subscribe_to(zephyr, &sub)) {
						purple_debug_error("zephyr", "Couldn't subscribe to %s, %s, %s\n",
						                   sub.zsub_class, sub.zsub_classinst, sub.zsub_recipient);
					}

					zephyr->subscrips = g_slist_append(zephyr->subscrips, zephyr_triple_new(zephyr, &sub));
					g_free(sub.zsub_class);
					g_free(sub.zsub_classinst);
					g_free(sub.zsub_recipient);
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

gchar *
get_zephyr_realm(PurpleAccount *account, const gchar *local_realm)
{
	const char *realm = purple_account_get_string(account, "realm", "");
	if (!*realm) {
		realm = local_realm;
	}
	g_strlcpy(__Zephyr_realm, realm, REALM_SZ - 1);
	return g_strdup(realm);
}

static void zephyr_login(PurpleAccount * account)
{
	PurpleConnection *gc;
	zephyr_account *zephyr;
	ZephyrLoginFunc login;
	GSourceFunc check_notify;
	ZSubscription_t sub;

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
		login = tzc_login;
		check_notify = tzc_check_notify;
		zephyr->subscribe_to = tzc_subscribe_to;
		zephyr->request_locations = tzc_request_locations;
		zephyr->send_message = tzc_send_message;
		zephyr->set_location = tzc_set_location;
		zephyr->get_subs_from_server = tzc_get_subs_from_server;
		zephyr->close = tzc_close;
	} else {
		zephyr->connection_type = PURPLE_ZEPHYR_KRB4;
		login = zeph02_login;
		check_notify = zeph02_check_notify;
		zephyr->subscribe_to = zeph02_subscribe_to;
		zephyr->request_locations = zeph02_request_locations;
		zephyr->send_message = zeph02_send_message;
		zephyr->set_location = zeph02_set_location;
		zephyr->get_subs_from_server = zeph02_get_subs_from_server;
		zephyr->close = zeph02_close;
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

	sub.zsub_class = "MESSAGE";
	sub.zsub_classinst = "PERSONAL";
	sub.zsub_recipient = zephyr->username;

	if (!zephyr->subscribe_to(zephyr, &sub)) {
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
	zephyr->loctimer = g_timeout_add_seconds(20, check_loc, zephyr);
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

	g_slist_free_full(zephyr->subscrips, (GDestroyNotify)zephyr_triple_free);

	if (zephyr->nottimer)
		g_source_remove(zephyr->nottimer);
	zephyr->nottimer = 0;
	if (zephyr->loctimer)
		g_source_remove(zephyr->loctimer);
	zephyr->loctimer = 0;
	zephyr->close(zephyr);

	g_clear_pointer(&zephyr->ourhost, g_free);
	g_clear_pointer(&zephyr->ourhostcanon, g_free);
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
	gboolean result;

	tmp_buf = html_to_zephyr(im);
	html_buf = purple_unescape_html(tmp_buf);
	g_free(tmp_buf);

	result = zephyr->send_message(zephyr, zclass, instance, recipient, html_buf, sig, opcode);
	g_free(html_buf);
	return result;
}

static gint
zephyr_triple_cmp_id(gconstpointer data, gconstpointer user_data)
{
	const zephyr_triple *zt = data;
	int id = GPOINTER_TO_INT(user_data);
	return zt->id - id;
}

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
	GSList *l;
	zephyr_triple *zt;
	const char *sig;
	PurpleConversation *gcc;
	char *inst;
	char *recipient;
	zephyr_account *zephyr = purple_connection_get_protocol_data(gc);

	l = g_slist_find_custom(zephyr->subscrips, GINT_TO_POINTER(id), zephyr_triple_cmp_id);
	if (!l) {
		/* this should never happen. */
		return -EINVAL;
	}

	zt = l->data;
	sig = zephyr_get_signature();

	gcc = purple_conversations_find_chat_with_account(zt->name, purple_connection_get_account(gc));

	if (!(inst = (char *)purple_chat_conversation_get_topic(PURPLE_CHAT_CONVERSATION(gcc))))
		inst = g_strdup("PERSONAL");

	if (!g_ascii_strcasecmp(zt->sub.zsub_recipient, "*")) {
		recipient = zephyr_normalize_local_realm(zephyr, "");
	} else {
		recipient = zephyr_normalize_local_realm(zephyr, zt->sub.zsub_recipient);
	}
	zephyr_send_message(zephyr, zt->sub.zsub_class, inst, recipient,
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
	        zephyr_normalize_local_realm(zephyr, purple_message_get_recipient(msg)),
		purple_message_get_contents(msg), sig, "");

	return 1;
}


/* Basically the inverse of zephyr_strip_local_realm */
char *
zephyr_normalize_local_realm(const zephyr_account *zephyr, const char *orig)
{
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

	tmp = zephyr_normalize_local_realm(purple_connection_get_protocol_data(gc), who);

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
	gchar *normalized_who = zephyr_normalize_local_realm(zephyr, who);

	if (zephyr->request_locations(zephyr, normalized_who)) {
		zephyr->pending_zloc_names = g_list_append(zephyr->pending_zloc_names, normalized_who);
	} else {
		/* XXX deal with errors somehow */
		g_free(normalized_who);
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
		zephyr->set_location(zephyr, zephyr->exposure);
	}
	else if (primitive == PURPLE_STATUS_INVISIBLE) {
		/* XXX handle errors */
		zephyr->set_location(zephyr, EXPOSE_OPSTAFF);
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


static void
zephyr_triple_open_personal(zephyr_triple *zt, PurpleConnection *gc, char *instance)
{
	if (!g_ascii_strcasecmp(instance, "*")) {
		instance = "PERSONAL";
	}
	zephyr_triple_open(zt, gc, instance);
}

static void
zephyr_join_chat(PurpleConnection *gc, ZSubscription_t *sub)
{
	GSList *l;
	zephyr_triple *zt;
	zephyr_account *zephyr = purple_connection_get_protocol_data(gc);

	g_return_if_fail(zephyr != NULL);

	if (!sub->zsub_class) {
		return;
	}
	if (!g_ascii_strcasecmp(sub->zsub_class, "%host%")) {
		sub->zsub_class = zephyr->ourhost;
	} else if (!g_ascii_strcasecmp(sub->zsub_class, "%canon%")) {
		sub->zsub_class = zephyr->ourhostcanon;
	}

	if (!sub->zsub_classinst || *sub->zsub_classinst == '\0') {
		sub->zsub_classinst = "*";
	} else if (!g_ascii_strcasecmp(sub->zsub_classinst, "%host%")) {
		sub->zsub_classinst = zephyr->ourhost;
	} else if (!g_ascii_strcasecmp(sub->zsub_classinst, "%canon%")) {
		sub->zsub_classinst = zephyr->ourhostcanon;
	}

	if (!sub->zsub_recipient || *sub->zsub_recipient == '*') {
		sub->zsub_recipient = "";
	} else if (!g_ascii_strcasecmp(sub->zsub_recipient, "%me%")) {
		sub->zsub_recipient = zephyr->username;
	}

	l = g_slist_find_custom(zephyr->subscrips, &sub, (GCompareFunc)zephyr_triple_subset);
	if (l) {
		zt = l->data;
		if (!zt->open) {
			zephyr_triple_open_personal(zt, gc, sub->zsub_classinst);
		}
		return;
	}

	if (!zephyr->subscribe_to(zephyr, sub)) {
		/* Called when the server notifies us a message couldn't get sent */
		/* XXX output better subscription information */
		gchar *subscribe_failed = g_strdup_printf(_("Attempt to subscribe to %s,%s,%s failed"),
		                                          sub->zsub_class, sub->zsub_classinst, sub->zsub_recipient);
		purple_notify_error(gc,"", subscribe_failed, NULL, purple_request_cpar_from_connection(gc));
		g_free(subscribe_failed);
		return;
	}

	zt = zephyr_triple_new(zephyr, sub);
	zephyr->subscrips = g_slist_append(zephyr->subscrips, zt);
	zephyr_triple_open_personal(zt, gc, sub->zsub_classinst);
}

static void
zephyr_protocol_chat_join(PurpleProtocolChat *protocol_chat,
                          PurpleConnection *gc, GHashTable * data)
{
	ZSubscription_t sub = {
	        .zsub_class = g_hash_table_lookup(data, "class"),
	        .zsub_classinst = g_hash_table_lookup(data, "instance"),
	        .zsub_recipient = g_hash_table_lookup(data, "recipient")
	};
	zephyr_join_chat(gc, &sub);
}

static void
zephyr_chat_leave(PurpleProtocolChat *protocol_chat, PurpleConnection * gc,
                  int id)
{
	zephyr_account *zephyr = purple_connection_get_protocol_data(gc);
	GSList *l;

	l = g_slist_find_custom(zephyr->subscrips, GINT_TO_POINTER(id), zephyr_triple_cmp_id);
	if (l) {
		zephyr_triple *zt = l->data;
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

static GList *
zephyr_protocol_get_account_options(PurpleProtocol *protocol) {
	PurpleAccountOption *option;
	GList *opts = NULL;
	const gchar *tmp = get_exposure_level();

	option = purple_account_option_bool_new(_("Use tzc"), "use_tzc", FALSE);
	opts = g_list_append(opts, option);

	option = purple_account_option_string_new(_("tzc command"), "tzc_command",
	                                          "/usr/bin/tzc -e %s");
	opts = g_list_append(opts, option);

	option = purple_account_option_bool_new(_("Export to .anyone"),
	                                        "write_anyone", FALSE);
	opts = g_list_append(opts, option);

	option = purple_account_option_bool_new(_("Export to .zephyr.subs"),
	                                        "write_zsubs", FALSE);
	opts = g_list_append(opts, option);

	option = purple_account_option_bool_new(_("Import from .anyone"),
	                                        "read_anyone", TRUE);
	opts = g_list_append(opts, option);

	option = purple_account_option_bool_new(_("Import from .zephyr.subs"),
	                                        "read_zsubs", TRUE);
	opts = g_list_append(opts, option);

	option = purple_account_option_string_new(_("Realm"), "realm", "");
	opts = g_list_append(opts, option);

	option = purple_account_option_string_new(_("Exposure"), "exposure_level",
	                                          tmp);
	opts = g_list_append(opts, option);

	option = purple_account_option_string_new(_("Encoding"), "encoding",
	                                          ZEPHYR_FALLBACK_CHARSET);
	opts = g_list_append(opts, option);

	return opts;
}

static unsigned int
zephyr_send_typing(G_GNUC_UNUSED PurpleProtocolIM *im, PurpleConnection *gc, const char *who, PurpleIMTypingState state)
{
	zephyr_account *zephyr;
	gchar *recipient;

	if (state == PURPLE_IM_NOT_TYPING) {
		return 0;
	}

	zephyr = purple_connection_get_protocol_data(gc);
	/* XXX We probably should care if this fails. Or maybe we don't want to */
	if (!who) {
		purple_debug_info("zephyr", "who is null\n");
		recipient = zephyr_normalize_local_realm(zephyr, "");
	} else {
		char *comma = strrchr(who, ',');
		/* Don't ping broadcast (chat) recipients */
		/* The strrchr case finds a realm-stripped broadcast subscription
		   e.g. comma is the last character in the string */
		if (comma && ((*(comma+1) == '\0') || (*(comma+1) == '@'))) {
			return 0;
		}

		recipient = zephyr_normalize_local_realm(zephyr, who);
	}

	purple_debug_info("zephyr", "about to send typing notification to %s", recipient);
	zephyr_send_message(zephyr, "MESSAGE", "PERSONAL", recipient, "", "", "PING");
	purple_debug_info("zephyr", "sent typing notification\n");

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
	PurpleConversation *gcc;
	gchar *topic_utf8;
	zephyr_account *zephyr = purple_connection_get_protocol_data(gc);
	GSList *l;

	l = g_slist_find_custom(zephyr->subscrips, GINT_TO_POINTER(id), zephyr_triple_cmp_id);
	if (!l) {
		return;
	}
	zt = l->data;

	gcc = purple_conversations_find_chat_with_account(zt->name, purple_connection_get_account(gc));

	topic_utf8 = convert_to_utf8(topic, zephyr->encoding);
	purple_chat_conversation_set_topic(PURPLE_CHAT_CONVERSATION(gcc), zephyr->username, topic_utf8);
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
		recipient = zephyr_normalize_local_realm(zephyr, args[0]);

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
	ZSubscription_t sub = {
	        .zsub_class = args[0],
	        .zsub_classinst = args[1],
	        .zsub_recipient = args[2]
	};
	zephyr_join_chat(purple_conversation_get_connection(conv), &sub);
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
		zephyr->subscribe_to(zephyr, &zt->sub);
	}
	/* XXX handle unsubscriptions */
}


static void zephyr_action_get_subs_from_server(PurpleProtocolAction *action)
{
	PurpleConnection *gc = action->connection;
	zephyr_account *zephyr = purple_connection_get_protocol_data(gc);
	zephyr->get_subs_from_server(zephyr, gc);
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
}


static void
zephyr_protocol_class_init(ZephyrProtocolClass *klass)
{
	PurpleProtocolClass *protocol_class = PURPLE_PROTOCOL_CLASS(klass);

	protocol_class->login = zephyr_login;
	protocol_class->close = zephyr_close;
	protocol_class->status_types = zephyr_status_types;
	protocol_class->list_icon = zephyr_list_icon;

	protocol_class->get_account_options = zephyr_protocol_get_account_options;
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

static PurpleProtocol *
zephyr_protocol_new(void) {
	return PURPLE_PROTOCOL(g_object_new(
		ZEPHYR_TYPE_PROTOCOL,
		"id", "prpl-zephyr",
		"name", "Zephyr",
		"options", OPT_PROTO_CHAT_TOPIC | OPT_PROTO_NO_PASSWORD,
		NULL));
}

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
	PurpleProtocolManager *manager = purple_protocol_manager_get_default();

	zephyr_protocol_register_type(G_TYPE_MODULE(plugin));

	my_protocol = zephyr_protocol_new();
	if(!purple_protocol_manager_register(manager, my_protocol, error)) {
		g_clear_object(&my_protocol);

		return FALSE;
	}

	zephyr_register_slash_commands();

	return TRUE;
}


static gboolean
plugin_unload(PurplePlugin *plugin, GError **error)
{
	PurpleProtocolManager *manager = purple_protocol_manager_get_default();

	if(!purple_protocol_manager_unregister(manager, my_protocol, error)) {
		return FALSE;
	}

	zephyr_unregister_slash_commands();

	g_clear_object(&my_protocol);

	return TRUE;
}


PURPLE_PLUGIN_INIT(zephyr, plugin_query, plugin_load, plugin_unload);
