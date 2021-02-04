/*
 * Purple - Internet Messaging Library
 * Copyright (C) Pidgin Developers <devel@pidgin.im>
 *
 * Purple is the legal property of its developers, whose names are too numerous
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

#include "zephyr_tzc.h"

#define MAXCHILDREN 20

typedef gssize (*PollableInputStreamReadFunc)(GPollableInputStream *stream, void *bufcur, GError **error);

static gboolean tzc_write(zephyr_account *zephyr, const gchar *format, ...) G_GNUC_PRINTF(2, 3);

static gchar *
tzc_read(zephyr_account *zephyr, PollableInputStreamReadFunc read_func)
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

static gboolean
tzc_write(zephyr_account *zephyr, const gchar *format, ...)
{
	va_list args;
	gchar *message;
	GError *error = NULL;
	gboolean success;

	va_start(args, format);
	message = g_strdup_vprintf(format, args);
	va_end(args);

	success = g_output_stream_write_all(zephyr->tzc_stdin, message, strlen(message),
	                                    NULL, NULL, &error);
	if (!success) {
		purple_debug_error("zephyr", "Unable to write a message: %s", error->message);
		g_error_free(error);
	}
	g_free(message);
	return success;
}

/* Munge the outgoing zephyr so that any quotes or backslashes are
   escaped and do not confuse tzc: */
static char *
tzc_escape_msg(const char *message)
{
	gsize msglen;
	char *newmsg;

	if (!message || !*message) {
		return g_strdup("");
	}

	msglen = strlen(message);
	newmsg = g_new0(char, msglen*2 + 1);
	for (gsize pos = 0, pos2 = 0; pos < msglen; pos++, pos2++) {
		if (message[pos] == '\\' || message[pos] == '"') {
			newmsg[pos2] = '\\';
			pos2++;
		}
		newmsg[pos2] = message[pos];
	}

	return newmsg;
}

static char *
tzc_deescape_str(const char *message)
{
	gsize msglen;
	char *newmsg;

	if (!message || !*message) {
		return g_strdup("");
	}

	msglen = strlen(message);
	newmsg = g_new0(char, msglen + 1);
	for (gsize pos = 0, pos2 = 0; pos < msglen; pos++, pos2++) {
		if (message[pos] == '\\') {
			pos++;
		}
		newmsg[pos2] = message[pos];
	}

	return newmsg;
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

static gssize
pollable_input_stream_read(GPollableInputStream *stream, void *bufcur, GError **error)
{
	return g_pollable_input_stream_read_nonblocking(stream, bufcur, 1, NULL, error);
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
		}
		if (local_error->code != G_IO_ERROR_WOULD_BLOCK) {
			g_propagate_error(error, local_error);
			return ret;
		}
		/* Keep on waiting if this is a blocking error. */
		g_clear_error(&local_error);
	}

	g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT,
	                    "tzc did not respond in time");
	return -1;
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

gboolean
tzc_login(zephyr_account *zephyr)
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
	buf = tzc_read(zephyr, pollable_input_stream_read_with_timeout);
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

gint
tzc_check_notify(gpointer data)
{
	PurpleConnection *gc = (PurpleConnection *)data;
	zephyr_account* zephyr = purple_connection_get_protocol_data(gc);
	GNode *newparsetree = NULL;
	gchar *buf = tzc_read(zephyr, pollable_input_stream_read);

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
				char *msg  = tzc_deescape_str(tree_child_contents(msgnode, 1));
				char *buf = g_strdup_printf(" %c%s", '\0', msg);
				memset((char *)&notice, 0, sizeof(notice));
				notice.z_kind = ACKED;
				notice.z_port = 0;
				notice.z_opcode = tree_child_contents(find_node(newparsetree, "opcode"), 2);
				notice.z_class = tzc_deescape_str(tree_child_contents(find_node(newparsetree, "class"), 2));
				notice.z_class_inst = tree_child_contents(find_node(newparsetree, "instance"), 2);
				notice.z_recipient = zephyr_normalize_local_realm(zephyr, tree_child_contents(find_node(newparsetree, "recipient"), 2));
				notice.z_sender = zephyr_normalize_local_realm(zephyr, tree_child_contents(find_node(newparsetree, "sender"), 2));
				notice.z_default_format = "Class $class, Instance $instance:\n" "To: @bold($recipient) at $time $date\n" "From: @bold($1) <$sender>\n\n$2";
				notice.z_message_len = 1 + 1 + strlen(msg) + 1;
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
						char *tmp = g_strdup_printf(_("<br>At %s since %s"), locval,
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

gboolean
tzc_subscribe_to(zephyr_account *zephyr, ZSubscription_t *sub)
{
	/* ((tzcfodder . subscribe) ("class" "instance" "recipient")) */
	return tzc_write(zephyr, "((tzcfodder . subscribe) (\"%s\" \"%s\" \"%s\"))\n",
	                 sub->zsub_class, sub->zsub_classinst, sub->zsub_recipient);
}

gboolean
tzc_request_locations(zephyr_account *zephyr, gchar *who)
{
	return tzc_write(zephyr, "((tzcfodder . zlocate) \"%s\")\n", who);
}

gboolean
tzc_send_message(zephyr_account *zephyr, gchar *zclass, gchar *instance, gchar *recipient,
                 const gchar *html_buf, const gchar *sig, G_GNUC_UNUSED const gchar *opcode)
{
	/* CMU cclub tzc doesn't grok opcodes for now  */
	char *tzc_sig = tzc_escape_msg(sig);
	char *tzc_body = tzc_escape_msg(html_buf);
	gboolean result;

	result = tzc_write(zephyr, "((tzcfodder . send) (class . \"%s\") (auth . t) (recipients (\"%s\" . \"%s\")) (message . (\"%s\" \"%s\"))	) \n",
	                   zclass, instance, recipient, tzc_sig, tzc_body);
	g_free(tzc_sig);
	g_free(tzc_body);
	return result;
}

void
tzc_set_location(zephyr_account *zephyr, char *exposure)
{
	tzc_write(zephyr, "((tzcfodder . set-location) (hostname . \"%s\") (exposure . \"%s\"))\n",
	          zephyr->ourhost, exposure);
}

void
tzc_get_subs_from_server(G_GNUC_UNUSED zephyr_account *zephyr, PurpleConnection *gc)
{
	/* XXX fix */
	purple_notify_error(gc, "", "tzc doesn't support this action",
	        NULL, purple_request_cpar_from_connection(gc));
}

void
tzc_close(zephyr_account *zephyr)
{
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
