/* MySpaceIM Protocol Plugin
 *
 * \author Jeff Connelly
 *
 * Copyright (C) 2007, Jeff Connelly <jeff2@soc.pidgin.im>
 *
 * Based on Purple's "C Plugin HOWTO" hello world example.
 *
 * Code also drawn from mockprpl:
 *  http://snarfed.org/space/purple+mock+protocol+plugin
 *  Copyright (C) 2004-2007, Ryan Barrett <mockprpl@ryanb.org>
 *
 * and some constructs also based on existing Purple plugins, which are:
 *   Copyright (C) 2003, Robbert Haarman <purple@inglorion.net>
 *   Copyright (C) 2003, Ethan Blanton <eblanton@cs.purdue.edu>
 *   Copyright (C) 2000-2003, Rob Flynn <rob@tgflinux.com>
 *   Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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

#define PURPLE_PLUGIN

#include "message.h"
#include "persist.h"
#include "myspace.h"

/** 
 * Load the plugin.
 */
gboolean 
msim_load(PurplePlugin *plugin)
{
	/* If compiled to use RC4 from libpurple, check if it is really there. */
	if (!purple_ciphers_find_cipher("rc4"))
	{
		purple_debug_error("msim", "rc4 not in libpurple, but it is required - not loading MySpaceIM plugin!\n");
		purple_notify_error(plugin, _("Missing Cipher"), 
				_("The RC4 cipher could not be found"),
				_("Upgrade "
					"to a libpurple with RC4 support (>= 2.0.1). MySpaceIM "
					"plugin will not be loaded."));
		return FALSE;
	}
	return TRUE;
}

/**
 * Get possible user status types. Based on mockprpl.
 *
 * @return GList of status types.
 */
GList *
msim_status_types(PurpleAccount *acct)
{
    GList *types;
    PurpleStatusType *status;

    purple_debug_info("myspace", "returning status types\n");

    types = NULL;

    /* Statuses are almost all the same. Define a macro to reduce code repetition. */
#define _MSIM_ADD_NEW_STATUS(prim) status =                         \
        purple_status_type_new_with_attrs(                          \
        prim,   /* PurpleStatusPrimitive */                         \
        NULL,   /* id - use default */                              \
        NULL,   /* name - use default */                            \
        TRUE,   /* savable */                                       \
        TRUE,   /* user_settable */                                 \
        FALSE,  /* not independent */                               \
                                                                    \
        /* Attributes - each status can have a message. */          \
        "message",                                                  \
        _("Message"),                                               \
        purple_value_new(PURPLE_TYPE_STRING),                       \
        NULL);                                                      \
                                                                    \
                                                                    \
        types = g_list_append(types, status)
        

    _MSIM_ADD_NEW_STATUS(PURPLE_STATUS_AVAILABLE);
    _MSIM_ADD_NEW_STATUS(PURPLE_STATUS_AWAY);
    _MSIM_ADD_NEW_STATUS(PURPLE_STATUS_OFFLINE);
    _MSIM_ADD_NEW_STATUS(PURPLE_STATUS_INVISIBLE);


    return types;
}

/**
 * Return the icon name for a buddy and account.
 *
 * @param acct The account to find the icon for, or NULL for protocol icon.
 * @param buddy The buddy to find the icon for, or NULL for the account icon.
 *
 * @return The base icon name string.
 */
const gchar *
msim_list_icon(PurpleAccount *acct, PurpleBuddy *buddy)
{
    /* Use a MySpace icon submitted by hbons at
     * http://developer.pidgin.im/wiki/MySpaceIM. */
    return "myspace";
}

/* Replacement codes to be replaced with associated replacement text,
 * used for protocol message escaping / unescaping. */
static gchar* msim_replacement_code[] = { "/1", "/2", "/3", NULL };
static gchar* msim_replacement_text[] = { "/", "\\", "|", NULL };

/**
 * Unescape or escape a protocol message.
 *
 * @param msg The message to be unescaped or escaped. WILL BE FREED.
 * @param escape TRUE to escape, FALSE to unescape.
 *
 * @return The unescaped or escaped message. Caller must g_free().
 */
gchar *
msim_unescape_or_escape(gchar *msg, gboolean escape)
{
	gchar *tmp, *code, *text;
	guint i;

	/* Replace each code in msim_replacement_code with
	 * corresponding entry in msim_replacement_text. */
	for (i = 0; (code = msim_replacement_code[i])
		   	&& (text = msim_replacement_text[i]); ++i)
	{
		if (escape)
		{
			tmp = str_replace(msg, text, code);
		}
		else
		{
			tmp = str_replace(msg, code, text);
		}
		g_free(msg);
		msg = tmp;
	}
	
	return msg;
}

/**
 * Escape a protocol message.
 *
 * @return The escaped message. Caller must g_free().
 */
gchar *
msim_escape(const gchar *msg)
{
	return msim_unescape_or_escape(g_strdup(msg), TRUE);
}

gchar *
msim_unescape(const gchar *msg)
{
	return msim_unescape_or_escape(g_strdup(msg), FALSE);
}

/**
 * Replace 'old' with 'new' in 'str'.
 *
 * @param str The original string.
 * @param old The substring of 'str' to replace.
 * @param new The replacement for 'old' within 'str'.
 *
 * @return A _new_ string, based on 'str', with 'old' replaced
 * 		by 'new'. Must be g_free()'d by caller.
 *
 * This string replace method is based on
 * http://mail.gnome.org/archives/gtk-app-devel-list/2000-July/msg00201.html
 *
 */
gchar *
str_replace(const gchar *str, const gchar *old, const gchar *new)
{
	gchar **items;
	gchar *ret;

	items = g_strsplit(str, old, -1);
	ret = g_strjoinv(new, items);
	g_free(items);
	return ret;
}

#ifdef MSIM_DEBUG_MSG
void 
print_hash_item(gpointer key, gpointer value, gpointer user_data)
{
    purple_debug_info("msim", "%s=%s\n", (gchar *)key, (gchar *)value);
}
#endif

/** 
 * Send raw data (given as a NUL-terminated string) to the server.
 *
 * @param session 
 * @param msg The raw data to send, in a NUL-terminated string.
 *
 * @return TRUE if succeeded, FALSE if not.
 *
 */
gboolean 
msim_send_raw(MsimSession *session, const gchar *msg)
{
    g_return_val_if_fail(MSIM_SESSION_VALID(session), FALSE);
    g_return_val_if_fail(msg != NULL, FALSE);
	
    purple_debug_info("msim", "msim_send_raw: writing <%s>\n", msg);

	return msim_send_really_raw(session->gc, msg, strlen(msg)) ==
		strlen(msg);
}

/** Send raw data to the server, possibly with embedded NULs. 
 *
 * Used in prpl_info struct, so that plugins can have the most possible
 * control of what is sent over the connection. Inside this prpl, 
 * msim_send_raw() is used, since it sends NUL-terminated strings (easier).
 *
 * @param gc PurpleConnection
 * @param buf Buffer to send
 * @param total_bytes Size of buffer to send
 *
 * @return Bytes successfully sent, or -1 on error.
 */
int 
msim_send_really_raw(PurpleConnection *gc, const char *buf, int total_bytes)
{
	int total_bytes_sent;
    MsimSession *session;

    g_return_val_if_fail(gc != NULL, -1);
    g_return_val_if_fail(buf != NULL, -1);
    g_return_val_if_fail(total_bytes >= 0, -1);

    session = (MsimSession *)(gc->proto_data);

    g_return_val_if_fail(MSIM_SESSION_VALID(session), -1);
	
	/* Loop until all data is sent, or a failure occurs. */
	total_bytes_sent = 0;
	do
	{
		int bytes_sent;

		bytes_sent = send(session->fd, buf + total_bytes_sent, 
                total_bytes - total_bytes_sent, 0);

		if (bytes_sent < 0)
		{
			purple_debug_info("msim", "msim_send_raw(%s): send() failed: %s\n",
					buf, g_strerror(errno));
			return total_bytes_sent;
		}
		total_bytes_sent += bytes_sent;

	} while(total_bytes_sent < total_bytes);

	return total_bytes_sent;
}


/** 
 * Start logging in to the MSIM servers.
 * 
 * @param acct Account information to use to login.
 */
void 
msim_login(PurpleAccount *acct)
{
    PurpleConnection *gc;
    const gchar *host;
    int port;

    g_return_if_fail(acct != NULL);
    g_return_if_fail(acct->username != NULL);

    purple_debug_info("myspace", "logging in %s\n", acct->username);

    gc = purple_account_get_connection(acct);
    gc->proto_data = msim_session_new(acct);
    gc->flags |= PURPLE_CONNECTION_HTML | PURPLE_CONNECTION_NO_URLDESC;

    /* Passwords are limited in length. */
	if (strlen(acct->password) > MSIM_MAX_PASSWORD_LENGTH)
	{
		gchar *str;

		str = g_strdup_printf(
				_("Sorry, passwords over %d characters in length (yours is "
				"%d) are not supported by the MySpaceIM plugin."), 
				MSIM_MAX_PASSWORD_LENGTH,
				(int)strlen(acct->password));

		/* Notify an error message also, because this is important! */
		purple_notify_error(acct, g_strdup(_("MySpaceIM Error")), str, NULL);

        purple_connection_error(gc, str);
		
		g_free(str);
	}

    /* 1. connect to server */
    purple_connection_update_progress(gc, _("Connecting"),
                                  0,   /* which connection step this is */
                                  4);  /* total number of steps */

    host = purple_account_get_string(acct, "server", MSIM_SERVER);
    port = purple_account_get_int(acct, "port", MSIM_PORT);

    /* From purple.sf.net/api:
     * """Note that this function name can be misleading--although it is called 
     * "proxy connect," it is used for establishing any outgoing TCP connection, 
     * whether through a proxy or not.""" */

    /* Calls msim_connect_cb when connected. */
    if (purple_proxy_connect(gc, acct, host, port, msim_connect_cb, gc) == NULL)
    {
        /* TODO: try other ports if in auto mode, then save
         * working port and try that first next time. */
        purple_connection_error(gc, _("Couldn't create socket"));
        return;
    }
}

/**
 * Process a login challenge, sending a response. 
 *
 * @param session 
 * @param msg Login challenge message.
 *
 * @return TRUE if successful, FALSE if not
 */
gboolean 
msim_login_challenge(MsimSession *session, MsimMessage *msg) 
{
    PurpleAccount *account;
    const gchar *response;
	guint response_len;
	gchar *nc;
	gsize nc_len;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), FALSE);
    g_return_val_if_fail(msg != NULL, FALSE);

    g_return_val_if_fail(msim_msg_get_binary(msg, "nc", &nc, &nc_len), FALSE);

    account = session->account;

	g_return_val_if_fail(account != NULL, FALSE);

    purple_connection_update_progress(session->gc, _("Reading challenge"), 1, 4);

    purple_debug_info("msim", "nc is %d bytes, decoded\n", nc_len);

    if (nc_len != MSIM_AUTH_CHALLENGE_LENGTH)
    {
        purple_debug_info("msim", "bad nc length: %x != 0x%x\n", nc_len, MSIM_AUTH_CHALLENGE_LENGTH);
        purple_connection_error(session->gc, _("Unexpected challenge length from server"));
        return FALSE;
    }

    purple_connection_update_progress(session->gc, _("Logging in"), 2, 4);

    response = msim_compute_login_response(nc, account->username, account->password, &response_len);

    g_free(nc);

	return msim_send(session, 
			"login2", MSIM_TYPE_INTEGER, MSIM_AUTH_ALGORITHM,
			/* This is actually user's email address. */
			"username", MSIM_TYPE_STRING, g_strdup(account->username),
			/* GString and gchar * response will be freed in msim_msg_free() in msim_send(). */
			"response", MSIM_TYPE_BINARY, g_string_new_len(response, response_len),
			"clientver", MSIM_TYPE_INTEGER, MSIM_CLIENT_VERSION,
			"reconn", MSIM_TYPE_INTEGER, 0,
			"status", MSIM_TYPE_INTEGER, 100,
			"id", MSIM_TYPE_INTEGER, 1,
			NULL);
}

/**
 * Compute the base64'd login challenge response based on username, password, nonce, and IPs.
 *
 * @param nonce The base64 encoded nonce ('nc') field from the server.
 * @param email User's email address (used as login name).
 * @param password User's cleartext password.
 * @param response_len Will be written with response length.
 *
 * @return Binary login challenge response, ready to send to the server. 
 * Must be g_free()'d when finished. NULL if error.
 */
const gchar *
msim_compute_login_response(const gchar nonce[2 * NONCE_SIZE], 
		const gchar *email, const gchar *password, guint *response_len)
{
    PurpleCipherContext *key_context;
    PurpleCipher *sha1;
	PurpleCipherContext *rc4;

    guchar hash_pw[HASH_SIZE];
    guchar key[HASH_SIZE];
    gchar *password_utf16le, *password_ascii_lc;
    guchar *data;
	guchar *data_out;
	size_t data_len, data_out_len;
	gsize conv_bytes_read, conv_bytes_written;
	GError *conv_error;
#ifdef MSIM_DEBUG_LOGIN_CHALLENGE
	int i;
#endif

    g_return_val_if_fail(nonce != NULL, NULL);
    g_return_val_if_fail(email != NULL, NULL);
    g_return_val_if_fail(password != NULL, NULL);
    g_return_val_if_fail(response_len != NULL, NULL);

    /* Convert password to lowercase (required for passwords containing
     * uppercase characters). MySpace passwords are lowercase,
     * see ticket #2066. */
    password_ascii_lc = g_strdup(password);
    g_strdown(password_ascii_lc);

    /* Convert ASCII password to UTF16 little endian */
    purple_debug_info("msim", "converting password to UTF-16LE\n");
	conv_error = NULL;
	password_utf16le = g_convert(password_ascii_lc, -1, "UTF-16LE", "UTF-8", 
			&conv_bytes_read, &conv_bytes_written, &conv_error);
    g_free(password_ascii_lc);

	g_return_val_if_fail(conv_bytes_read == strlen(password), NULL);

	if (conv_error != NULL)
	{
		purple_debug_error("msim", 
				"g_convert password UTF8->UTF16LE failed: %s",
				conv_error->message);
		g_error_free(conv_error);
        return NULL;
	}

    /* Compute password hash */ 
    purple_cipher_digest_region("sha1", (guchar *)password_utf16le, 
			conv_bytes_written, sizeof(hash_pw), hash_pw, NULL);
	g_free(password_utf16le);

#ifdef MSIM_DEBUG_LOGIN_CHALLENGE
    purple_debug_info("msim", "pwhash = ");
    for (i = 0; i < sizeof(hash_pw); i++)
        purple_debug_info("msim", "%.2x ", hash_pw[i]);
    purple_debug_info("msim", "\n");
#endif

    /* key = sha1(sha1(pw) + nonce2) */
    sha1 = purple_ciphers_find_cipher("sha1");
    key_context = purple_cipher_context_new(sha1, NULL);
    purple_cipher_context_append(key_context, hash_pw, HASH_SIZE);
    purple_cipher_context_append(key_context, (guchar *)(nonce + NONCE_SIZE), NONCE_SIZE);
    purple_cipher_context_digest(key_context, sizeof(key), key, NULL);

#ifdef MSIM_DEBUG_LOGIN_CHALLENGE
    purple_debug_info("msim", "key = ");
    for (i = 0; i < sizeof(key); i++)
    {
        purple_debug_info("msim", "%.2x ", key[i]);
    }
    purple_debug_info("msim", "\n");
#endif

	rc4 = purple_cipher_context_new_by_name("rc4", NULL);

    /* Note: 'key' variable is 0x14 bytes (from SHA-1 hash), 
     * but only first 0x10 used for the RC4 key. */
	purple_cipher_context_set_option(rc4, "key_len", (gpointer)0x10);
	purple_cipher_context_set_key(rc4, key);

    /* TODO: obtain IPs of network interfaces */

    /* rc4 encrypt:
     * nonce1+email+IP list */

    data_len = NONCE_SIZE + strlen(email) + MSIM_LOGIN_IP_LIST_LEN;
    data = g_new0(guchar, data_len);
    memcpy(data, nonce, NONCE_SIZE);
    memcpy(data + NONCE_SIZE, email, strlen(email));
    memcpy(data + NONCE_SIZE + strlen(email), MSIM_LOGIN_IP_LIST, MSIM_LOGIN_IP_LIST_LEN);

	data_out = g_new0(guchar, data_len);

    purple_cipher_context_encrypt(rc4, (const guchar *)data, 
			data_len, data_out, &data_out_len);
	purple_cipher_context_destroy(rc4);

	g_assert(data_out_len == data_len);

#ifdef MSIM_DEBUG_LOGIN_CHALLENGE
    purple_debug_info("msim", "response=<%s>\n", data_out);
#endif

	*response_len = data_out_len;

    return (const gchar *)data_out;
}

/**
 * Schedule an IM to be sent once the user ID is looked up. 
 *
 * @param gc Connection.
 * @param who A user id, email, or username to send the message to.
 * @param message Instant message text to send.
 * @param flags Flags.
 *
 * @return 1 if successful or postponed, -1 if failed
 *
 * Allows sending to a user by username, email address, or userid. If
 * a username or email address is given, the userid must be looked up.
 * This function does that by calling msim_postprocess_outgoing().
 */
int 
msim_send_im(PurpleConnection *gc, const gchar *who, const gchar *message, 
		PurpleMessageFlags flags)
{
    MsimSession *session;
    gchar *message_msim;
    int rc;
    
    g_return_val_if_fail(gc != NULL, -1);
    g_return_val_if_fail(who != NULL, -1);
    g_return_val_if_fail(message != NULL, -1);

	/* 'flags' has many options, not used here. */

	session = (MsimSession *)gc->proto_data;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), -1);

    message_msim = html_to_msim_markup(message);

	if (msim_send_bm(session, who, message_msim, MSIM_BM_INSTANT))
	{
		/* Return 1 to have Purple show this IM as being sent, 0 to not. I always
		 * return 1 even if the message could not be sent, since I don't know if
		 * it has failed yet--because the IM is only sent after the userid is
		 * retrieved from the server (which happens after this function returns).
		 */
        /* TODO: maybe if message is delayed, don't echo to conv window,
         * but do echo it to conv window manually once it is actually
         * sent? Would be complicated. */
		rc = 1;
	} else {
		rc = -1;
	}

    g_free(message_msim);

    /*
     * In MySpace, you login with your email address, but don't talk to other
     * users using their email address. So there is currently an asymmetry in the 
     * IM windows when using this plugin:
     *
     * you@example.com: hello
     * some_other_user: what's going on?
     * you@example.com: just coding a prpl
     *
     * TODO: Make the sent IM's appear as from the user's username, instead of
     * their email address. Purple uses the login (in MSIM, the email)--change this.
     */

    return rc;
}

/** Send a buddy message of a given type.
 *
 * @param session
 * @param who Username to send message to.
 * @param text Message text to send. Not freed; will be copied.
 * @param type A MSIM_BM_* constant.
 *
 * @return TRUE if success, FALSE if fail.
 *
 * Buddy messages ('bm') include instant messages, action messages, status messages, etc.
 *
 */
gboolean 
msim_send_bm(MsimSession *session, const gchar *who, const gchar *text, 
		int type)
{
	gboolean rc;
	MsimMessage *msg;
    const gchar *from_username;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), FALSE);
    g_return_val_if_fail(who != NULL, FALSE);
    g_return_val_if_fail(text != NULL, FALSE);
   
	from_username = session->account->username;

    purple_debug_info("msim", "sending %d message from %s to %s: %s\n",
                  type, from_username, who, text);

	msg = msim_msg_new(TRUE,
			"bm", MSIM_TYPE_INTEGER, GUINT_TO_POINTER(type),
			"sesskey", MSIM_TYPE_INTEGER, GUINT_TO_POINTER(session->sesskey),
			/* 't' will be inserted here */
			"cv", MSIM_TYPE_INTEGER, GUINT_TO_POINTER(MSIM_CLIENT_VERSION),
			"msg", MSIM_TYPE_STRING, g_strdup(text),
			NULL);

	rc = msim_postprocess_outgoing(session, msg, who, "t", "cv");

	msim_msg_free(msg);

	return rc;
}

/* Indexes of this array + 1 map HTML font size to scale of normal font size. *
 * Based on _point_sizes from libpurple/gtkimhtml.c 
 *                                 1    2  3    4     5      6       7 */
static gdouble _font_scale[] = { .85, .95, 1, 1.2, 1.44, 1.728, 2.0736 };

#define MAX_FONT_SIZE                   7       /* Purple maximum font size */
#define POINTS_PER_INCH                 72      /* How many pt's in an inch */

/* Baseline size of purple's fonts, in points. What is size 3 in points. 
 * _font_scale specifies scaling factor relative to this point size.
 * TODO: configurable */
#define BASE_FONT_POINT_SIZE            8

/* Display's DPI. 96 is common but it can differ. TODO: configurable */
#define DOTS_PER_INCH                   96

/** Convert typographical font point size to HTML font size. 
 * Based on libpurple/gtkimhtml.c */
guint
msim_point_to_purple_size(guint point)
{
    guint size, this_point;
    gdouble scale;
   
    for (size = 0; size < sizeof(_font_scale) / sizeof(_font_scale[0]); ++size)
    {
        scale = _font_scale[CLAMP(size, 1, MAX_FONT_SIZE) - 1];
        this_point = (guint)round(scale * BASE_FONT_POINT_SIZE);

        if (this_point >= point)
        {
            purple_debug_info("msim", "msim_point_to_purple_size: %d pt -> size=%d\n",
                    point, size);
            return size;
        }
    }

    /* No HTML font size was this big; return largest possible. */
    return this_point;
}

/** Convert HTML font size to point size. */
guint
msim_purple_size_to_point(guint size)
{
    gdouble scale;
    guint point;

    scale = _font_scale[CLAMP(size, 1, MAX_FONT_SIZE) - 1];

    point = (guint)round(scale * BASE_FONT_POINT_SIZE);

    purple_debug_info("msim", "msim_purple_size_to_point: size=%d -> %d pt\n",
                    size, point);

    return point;
}

/** Convert a msim markup font pixel height to the more usual point size, for incoming messages. */
guint 
msim_height_to_point(guint height)
{
    return (guint)round((POINTS_PER_INCH * 1. / DOTS_PER_INCH) * height);

	/* See also: libpurple/protocols/bonjour/jabber.c
	 * _font_size_ichat_to_purple */
}

/** Convert point size to msim pixel height font size specification, for outgoing messages. */
guint
msim_point_to_height(guint point)
{
    return (guint)round((DOTS_PER_INCH * 1. / POINTS_PER_INCH) * point);
}

/** Convert the msim markup <f> (font) tag into HTML. */
static void 
msim_markup_f_to_html(xmlnode *root, gchar **begin, gchar **end)
{
	const gchar *face, *height_str, *decor_str;
	GString *gs_end, *gs_begin;
	guint decor, height;

	face = xmlnode_get_attrib(root, "f");	
	height_str = xmlnode_get_attrib(root, "h");
	decor_str = xmlnode_get_attrib(root, "s");

	if (height_str)
		height = atol(height_str);
	else
		height = 12;

	if (decor_str)
		decor = atol(decor_str);
	else
		decor = 0;

	gs_begin = g_string_new("");
	/* TODO: get font size working */
	if (height && !face)
		g_string_printf(gs_begin, "<font size='%d'>", 
                msim_point_to_purple_size(msim_height_to_point(height)));
    else if (height && face)
		g_string_printf(gs_begin, "<font face='%s' size='%d'>", face,  
                msim_point_to_purple_size(msim_height_to_point(height)));
    else
        g_string_printf(gs_begin, "<font>");

	/* No support for font-size CSS? */
	/* g_string_printf(gs_begin, "<span style='font-family: %s; font-size: %dpt'>", face, 
			msim_height_to_point(height)); */

	gs_end = g_string_new("</font>");

	if (decor & MSIM_TEXT_BOLD)
	{
		g_string_append(gs_begin, "<b>");
		g_string_prepend(gs_end, "</b>");
	}

	if (decor & MSIM_TEXT_ITALIC)
	{
		g_string_append(gs_begin, "<i>");
		g_string_append(gs_end, "</i>");	
	}

	if (decor & MSIM_TEXT_UNDERLINE)
	{
		g_string_append(gs_begin, "<u>");
		g_string_append(gs_end, "</u>");	
	}


	*begin = gs_begin->str;
	*end = gs_end->str;
}

/** Convert a msim markup color to a color suitable for libpurple.
  *
  * @param msim Either a color name, or an rgb(x,y,z) code.
  *
  * @return A new string, either a color name or #rrggbb code. Must g_free(). 
  */
static char *
msim_color_to_purple(const char *msim)
{
	guint red, green, blue;

	if (!msim)
		return g_strdup("black");

	if (sscanf(msim, "rgb(%d,%d,%d)", &red, &green, &blue) != 3)
	{
		/* Color name. */
		return g_strdup(msim);
	}
	/* TODO: rgba (alpha). */

	return g_strdup_printf("#%.2x%.2x%.2x", red, green, blue);
}	

/** Convert the msim markup <p> (paragraph) tag into HTML. */
static void 
msim_markup_p_to_html(xmlnode *root, gchar **begin, gchar **end)
{
    /* Just pass through unchanged. 
	 *
	 * Note: attributes currently aren't passed, if there are any. */
	*begin = g_strdup("<p>");
	*end = g_strdup("</p>");
}

/** Convert the msim markup <c> tag (text color) into HTML. TODO: Test */
static void 
msim_markup_c_to_html(xmlnode *root, gchar **begin, gchar **end)
{
	const gchar *color;
	gchar *purple_color;

	color = xmlnode_get_attrib(root, "v");
	if (!color)
	{
		purple_debug_info("msim", "msim_markup_c_to_html: <c> tag w/o v attr");
		*begin = g_strdup("");
		*end = g_strdup("");
		/* TODO: log as unrecognized */
		return;
	}

	purple_color = msim_color_to_purple(color);

	*begin = g_strdup_printf("<font color='%s'>", purple_color); 

	g_free(purple_color);

	/* *begin = g_strdup_printf("<span style='color: %s'>", color); */
	*end = g_strdup("</font>");
}

/** Convert the msim markup <b> tag (background color) into HTML. TODO: Test */
static void 
msim_markup_b_to_html(xmlnode *root, gchar **begin, gchar **end)
{
	const gchar *color;
	gchar *purple_color;

	color = xmlnode_get_attrib(root, "v");
	if (!color)
	{
		*begin = g_strdup("");
		*end = g_strdup("");
		purple_debug_info("msim", "msim_markup_b_to_html: <b> w/o v attr");
		/* TODO: log as unrecognized. */
		return;
	}

	purple_color = msim_color_to_purple(color);

	/* TODO: find out how to set background color. */
	*begin = g_strdup_printf("<span style='background-color: %s'>", 
			purple_color);
	g_free(purple_color);

	*end = g_strdup("</p>");
}

/** Convert the msim markup <i> tag (emoticon image) into HTML. TODO: Test */
static void 
msim_markup_i_to_html(xmlnode *root, gchar **begin, gchar **end)
{
	const gchar *name;

	name = xmlnode_get_attrib(root, "n");
	if (!name)
	{
		purple_debug_info("msim", "msim_markup_i_to_html: <i> w/o n");
		*begin = g_strdup("");
		*end = g_strdup("");
		/* TODO: log as unrecognized */
		return;
	}

	/* TODO: Support these emoticons:
	 *
	 * bigsmile growl mad scared tongue devil happy messed sidefrown upset 
	 * frazzled heart nerd sinister wink geek laugh oops smirk worried 
	 * googles mohawk pirate straight kiss 
	 */

	*begin = g_strdup_printf("<img id='%s'>", name);
	*end = g_strdup("</p>");
}

/** Convert an individual msim markup tag to HTML. */
void msim_markup_tag_to_html(xmlnode *root, gchar **begin, gchar **end)
{
	if (!strcmp(root->name, "f"))
	{
		msim_markup_f_to_html(root, begin, end);
	} else if (!strcmp(root->name, "p")) {
		msim_markup_p_to_html(root, begin, end);
	} else if (!strcmp(root->name, "c")) {
		msim_markup_c_to_html(root, begin, end);
	} else if (!strcmp(root->name, "b")) {
		msim_markup_b_to_html(root, begin, end);
	} else if (!strcmp(root->name, "i")) {
		msim_markup_i_to_html(root, begin, end);
	} else {
		purple_debug_info("msim", "msim_markup_tag_to_html: "
				"unknown tag name=%s, ignoring", root->name);
		*begin = g_strdup("");
		*end = g_strdup("");
	}
}

/** Convert an individual HTML tag to msim markup. */
void html_tag_to_msim_markup(xmlnode *root, gchar **begin, gchar **end)
{
    /* TODO: Coalesce nested tags into one <f> tag!
     * Currently, the 's' value will be overwritten when b/i/u is nested
     * within another one, and only the inner-most formatting will be 
     * applied to the text. */
    if (!strcmp(root->name, "root"))
    {
        *begin = g_strdup("");
        *end = g_strdup("");
    } else if (!strcmp(root->name, "b")) {
        *begin = g_strdup_printf("<f s='%d'>", MSIM_TEXT_BOLD);
        *end = g_strdup("</f>");
    } else if (!strcmp(root->name, "i")) {
        *begin = g_strdup_printf("<f s='%d'>", MSIM_TEXT_ITALIC);
        *end = g_strdup("</f>");
    } else if (!strcmp(root->name, "u")) {
        *begin = g_strdup_printf("<f s='%d'>", MSIM_TEXT_UNDERLINE);
        *end = g_strdup("</f>");
    } else if (!strcmp(root->name, "font")) {
        const gchar *size;
        const gchar *face;

        size = xmlnode_get_attrib(root, "size");
        face = xmlnode_get_attrib(root, "face");

        if (face && size)
        {
            *begin = g_strdup_printf("<f f='%s' h='%d'>", face, 
                    msim_point_to_height(msim_purple_size_to_point(
                            atoi(size))));
        } else if (face) {
            *begin = g_strdup_printf("<f f='%s'>", face);
        } else if (size) {
            *begin = g_strdup_printf("<f h='%d'>", 
                     msim_point_to_height(msim_purple_size_to_point(
                            atoi(size))));
        } else {
            *begin = g_strdup("<f>");
        }

        *end = g_strdup("</f>");

        /* TODO: color (bg uses <body>), emoticons */
    } else {
        *begin = g_strdup_printf("[%s]", root->name);
        *end = g_strdup_printf("[/%s]", root->name);
    }
}

/** Convert an xmlnode of msim markup or HTML to an HTML string or msim markup.
 *
 * @param f Function to convert tags.
 *
 * @return An HTML string. Caller frees.
 */
static gchar *
msim_convert_xmlnode(xmlnode *root, MSIM_XMLNODE_CONVERT f)
{
	xmlnode *node;
	gchar *begin, *inner, *end;
    GString *final;

	if (!root || !root->name)
		return g_strdup("");

	purple_debug_info("msim", "msim_convert_xmlnode: got root=%s\n",
			root->name);

	begin = inner = end = NULL;

    final = g_string_new("");

    f(root, &begin, &end);
    
    g_string_append(final, begin);

	/* Loop over all child nodes. */
 	for (node = root->child; node != NULL; node = node->next)
	{
		switch (node->type)
		{
		case XMLNODE_TYPE_ATTRIB:
			/* Attributes handled above. */
			break;

		case XMLNODE_TYPE_TAG:
			/* A tag or tag with attributes. Recursively descend. */
			inner = msim_convert_xmlnode(node, f);
            g_return_val_if_fail(inner != NULL, NULL);

			purple_debug_info("msim", " ** node name=%s\n", node->name);
			break;
	
		case XMLNODE_TYPE_DATA:	
			/* Literal text. */
			inner = g_new0(char, node->data_sz + 1);
			strncpy(inner, node->data, node->data_sz);
			inner[node->data_sz] = 0;

			purple_debug_info("msim", " ** node data=%s\n", inner);
			break;
			
		default:
			purple_debug_info("msim",
					"msim_convert_xmlnode: strange node\n");
			inner = g_strdup("");
		}

        if (inner)
            g_string_append(final, inner);
    }

    /* TODO: Note that msim counts each piece of text enclosed by <f> as
     * a paragraph and will display each on its own line. You actually have
     * to _nest_ <f> tags to intersperse different text in one paragraph!
     * Comment out this line below to see. */
    g_string_append(final, end);

	purple_debug_info("msim", "msim_markup_xmlnode_to_gtkhtml: RETURNING %s\n",
			final->str);

	return final->str;
}

/** Convert XML to something based on MSIM_XMLNODE_CONVERT. */
gchar *
msim_convert_xml(const gchar *raw, MSIM_XMLNODE_CONVERT f)
{
	xmlnode *root;
	gchar *str;

	root = xmlnode_from_str(raw, -1);
	if (!root)
	{
		purple_debug_info("msim", "msim_markup_to_html: couldn't parse "
				"%s as XML, returning raw\n", raw);
        /* TODO: msim_unrecognized */
		return g_strdup(raw);
	}

	str = msim_convert_xmlnode(root, f);
	purple_debug_info("msim", "msim_markup_to_html: returning %s\n", str);

	xmlnode_free(root);

	return str;
}

/** High-level function to convert MySpaceIM markup to Purple (HTML) markup. 
 *
 * @return Purple markup string, must be g_free()'d. */
gchar *
msim_markup_to_html(const gchar *raw)
{
    return msim_convert_xml(raw, 
            (MSIM_XMLNODE_CONVERT)(msim_markup_tag_to_html));
}

/** High-level function to convert Purple (HTML) to MySpaceIM markup.
 *
 * @return HTML markup string, must be g_free()'d. */
gchar *
html_to_msim_markup(const gchar *raw)
{
    gchar *markup;
    gchar *enclosed_raw;

    /* Enclose text in one root tag, to try to make it valid XML for parsing. */
    enclosed_raw = g_strconcat("<root>", raw, "</root>", NULL);

    markup = msim_convert_xml(enclosed_raw,
            (MSIM_XMLNODE_CONVERT)(html_tag_to_msim_markup));

    g_free(enclosed_raw);

    return markup;
}

/**
 * Handle an incoming instant message.
 *
 * @param session The session
 * @param msg Message from the server, containing 'f' (userid from) and 'msg'. 
 * 			  Should also contain username in _username from preprocessing.
 *
 * @return TRUE if successful.
 */
gboolean 
msim_incoming_im(MsimSession *session, MsimMessage *msg)
{
    gchar *username, *msg_msim_markup, *msg_purple_markup;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), FALSE);
    g_return_val_if_fail(msg != NULL, FALSE);

    username = msim_msg_get_string(msg, "_username");
    g_return_val_if_fail(username != NULL, FALSE);

	msg_msim_markup = msim_msg_get_string(msg, "msg");
    g_return_val_if_fail(msg_msim_markup != NULL, FALSE);

	msg_purple_markup = msim_markup_to_html(msg_msim_markup);
	g_free(msg_msim_markup);

    serv_got_im(session->gc, username, msg_purple_markup, 
			PURPLE_MESSAGE_RECV, time(NULL));

	g_free(username);
	g_free(msg_purple_markup);

	return TRUE;
}

/**
 * Process unrecognized information.
 *
 * @param session
 * @param msg An MsimMessage that was unrecognized, or NULL.
 * @param note Information on what was unrecognized, or NULL.
 */
void 
msim_unrecognized(MsimSession *session, MsimMessage *msg, gchar *note)
{
	/* TODO: Some more context, outwardly equivalent to a backtrace, 
     * for helping figure out what this msg is for. What was going on?
     * But not too much information so that a user
	 * posting this dump reveals confidential information.
	 */

	/* TODO: dump unknown msgs to file, so user can send them to me
	 * if they wish, to help add support for new messages (inspired
	 * by Alexandr Shutko, who maintains OSCAR protocol documentation). */

	purple_debug_info("msim", "Unrecognized data on account for %s\n", 
            session->account->username);
	if (note)
	{
		purple_debug_info("msim", "(Note: %s)\n", note);
	}

    if (msg)
    {
        msim_msg_dump("Unrecognized message dump: %s\n", msg);
    }
}

/**
 * Handle an incoming action message.
 *
 * @param session
 * @param msg
 *
 * @return TRUE if successful.
 *
 * UNTESTED
 */
gboolean 
msim_incoming_action(MsimSession *session, MsimMessage *msg)
{
	gchar *msg_text, *username;
	gboolean rc;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), FALSE);
    g_return_val_if_fail(msg != NULL, FALSE);

	msg_text = msim_msg_get_string(msg, "msg");
    g_return_val_if_fail(msg_text != NULL, FALSE);

	username = msim_msg_get_string(msg, "_username");
    g_return_val_if_fail(username != NULL, FALSE);

	purple_debug_info("msim", "msim_incoming_action: action <%s> from <%d>\n", 
            msg_text, username);

	if (strcmp(msg_text, "%typing%") == 0)
	{
		/* TODO: find out if msim repeatedly sends typing messages, so we can 
         * give it a timeout. Right now, there does seem to be an inordinately 
         * amount of time between typing stopped-typing notifications. */
		serv_got_typing(session->gc, username, 0, PURPLE_TYPING);
		rc = TRUE;
	} else if (strcmp(msg_text, "%stoptyping%") == 0) {
		serv_got_typing_stopped(session->gc, username);
		rc = TRUE;
	} else {
		msim_unrecognized(session, msg, 
                "got to msim_incoming_action but unrecognized value for 'msg'");
		rc = FALSE;
	}

	g_free(msg_text);
	g_free(username);

	return rc;
}

/** 
 * Handle when our user starts or stops typing to another user.
 *
 * @param gc
 * @param name The buddy name to which our user is typing to
 * @param state PURPLE_TYPING, PURPLE_TYPED, PURPLE_NOT_TYPING
 *
 * @return 0
 */
unsigned int 
msim_send_typing(PurpleConnection *gc, const gchar *name, 
        PurpleTypingState state)
{
	const gchar *typing_str;
	MsimSession *session;

    g_return_val_if_fail(gc != NULL, 0);
    g_return_val_if_fail(name != NULL, 0);

	session = (MsimSession *)gc->proto_data;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), 0);

	switch (state)
	{	
		case PURPLE_TYPING: 
			typing_str = "%typing%"; 
			break;

		case PURPLE_TYPED:
		case PURPLE_NOT_TYPING:
		default:
			typing_str = "%stoptyping%";
			break;
	}

	purple_debug_info("msim", "msim_send_typing(%s): %d (%s)\n", name, state, typing_str);
	msim_send_bm(session, name, typing_str, MSIM_BM_ACTION);
	return 0;
}

/** Callback for msim_get_info(), for when user info is received. */
void 
msim_get_info_cb(MsimSession *session, MsimMessage *user_info_msg, gpointer data)
{
	GHashTable *body;
	gchar *body_str;
	MsimMessage *msg;
	gchar *user;
	PurpleNotifyUserInfo *user_info;
	PurpleBuddy *buddy;
	const gchar *str, *str2;

    g_return_if_fail(MSIM_SESSION_VALID(session));

	/* Get user{name,id} from msim_get_info, passed as an MsimMessage for 
	   orthogonality. */
	msg = (MsimMessage *)data;
    g_return_if_fail(msg != NULL);

	user = msim_msg_get_string(msg, "user");
	if (!user)
	{
		purple_debug_info("msim", "msim_get_info_cb: no 'user' in msg");
		return;
	}

	msim_msg_free(msg);
	purple_debug_info("msim", "msim_get_info_cb: got for user: %s\n", user);

	body_str = msim_msg_get_string(user_info_msg, "body");
    g_return_if_fail(body_str != NULL);
	body = msim_parse_body(body_str);
	g_free(body_str);

	buddy = purple_find_buddy(session->account, user);
	/* Note: don't assume buddy is non-NULL; will be if lookup random user 
     * not on blist. */

	user_info = purple_notify_user_info_new();

	/* Identification */
	purple_notify_user_info_add_pair(user_info, _("User"), user);

	/* note: g_hash_table_lookup does not create a new string! */
	str = g_hash_table_lookup(body, "UserID");
	if (str)
		purple_notify_user_info_add_pair(user_info, _("User ID"), 
				g_strdup(str));

	/* a/s/l...the vitals */	
	str = g_hash_table_lookup(body, "Age");
	if (str)
		purple_notify_user_info_add_pair(user_info, _("Age"), g_strdup(str));

	str = g_hash_table_lookup(body, "Gender");
	if (str)
		purple_notify_user_info_add_pair(user_info, _("Gender"), g_strdup(str));

	str = g_hash_table_lookup(body, "Location");
	if (str)
		purple_notify_user_info_add_pair(user_info, _("Location"), 
				g_strdup(str));

	/* Other information */

	/* Headline comes from buddy status messages */
	if (buddy)
	{
		str = purple_blist_node_get_string(&buddy->node, "Headline");
		if (str)
			purple_notify_user_info_add_pair(user_info, "Headline", str);
	}


	str = g_hash_table_lookup(body, "BandName");
	str2 = g_hash_table_lookup(body, "SongName");
	if (str || str2)
	{
		purple_notify_user_info_add_pair(user_info, _("Song"), 
			g_strdup_printf("%s - %s",
				str ? str : "Unknown Artist",
				str2 ? str2 : "Unknown Song"));
	}


	/* Total friends only available if looked up by uid, not username. */
	str = g_hash_table_lookup(body, "TotalFriends");
	if (str)
		purple_notify_user_info_add_pair(user_info, _("Total Friends"), 
			g_strdup(str));

	purple_notify_userinfo(session->gc, user, user_info, NULL, NULL);
	purple_debug_info("msim", "msim_get_info_cb: username=%s\n", user);
	//purple_notify_user_info_destroy(user_info);
	/* Do not free username, since it will be used by user_info. */

	g_hash_table_destroy(body);
}

/** Retrieve a user's profile. */
void 
msim_get_info(PurpleConnection *gc, const gchar *user)
{
	PurpleBuddy *buddy;
	MsimSession *session;
	guint uid;
	gchar *user_to_lookup;
	MsimMessage *user_msg;

    g_return_if_fail(gc != NULL);
    g_return_if_fail(user != NULL);

	session = (MsimSession *)gc->proto_data;

    g_return_if_fail(MSIM_SESSION_VALID(session));

	/* Obtain uid of buddy. */
	buddy = purple_find_buddy(session->account, user);
	if (buddy)
	{
		uid = purple_blist_node_get_int(&buddy->node, "UserID");
		if (!uid)
		{
			PurpleNotifyUserInfo *user_info;

			user_info = purple_notify_user_info_new();
			purple_notify_user_info_add_pair(user_info, NULL,
					_("This buddy appears to not have a userid stored in the buddy list, can't look up. Is the user really on the buddy list?"));

			purple_notify_userinfo(session->gc, user, user_info, NULL, NULL);
			purple_notify_user_info_destroy(user_info);
			return;
		}

		user_to_lookup = g_strdup_printf("%d", uid);
	} else {

		/* Looking up buddy not on blist. Lookup by whatever user entered. */
		user_to_lookup = g_strdup(user);
	}

	/* Pass the username to msim_get_info_cb(), because since we lookup
	 * by userid, the userinfo message will only contain the uid (not 
	 * the username).
	 */
	user_msg = msim_msg_new(TRUE, 
			"user", MSIM_TYPE_STRING, g_strdup(user),
			NULL);
	purple_debug_info("msim", "msim_get_info, setting up lookup, user=%s\n", user);

	msim_lookup_user(session, user_to_lookup, msim_get_info_cb, user_msg);

	g_free(user_to_lookup); 
}

/** Set your status - callback for when user manually sets it.  */
void
msim_set_status(PurpleAccount *account, PurpleStatus *status)
{
	PurpleStatusType *type;
	MsimSession *session;
    guint status_code;
    const gchar *statstring;

	session = (MsimSession *)account->gc->proto_data;

    g_return_if_fail(MSIM_SESSION_VALID(session));

	type = purple_status_get_type(status);

	switch (purple_status_type_get_primitive(type))
	{
		case PURPLE_STATUS_AVAILABLE:
            purple_debug_info("msim", "msim_set_status: available (%d->%d)\n", PURPLE_STATUS_AVAILABLE,
                    MSIM_STATUS_CODE_ONLINE);
			status_code = MSIM_STATUS_CODE_ONLINE;
			break;

		case PURPLE_STATUS_INVISIBLE:
            purple_debug_info("msim", "msim_set_status: invisible (%d->%d)\n", PURPLE_STATUS_INVISIBLE,
                    MSIM_STATUS_CODE_OFFLINE_OR_HIDDEN);
			status_code = MSIM_STATUS_CODE_OFFLINE_OR_HIDDEN;
			break;

		case PURPLE_STATUS_AWAY:
            purple_debug_info("msim", "msim_set_status: away (%d->%d)\n", PURPLE_STATUS_AWAY,
                    MSIM_STATUS_CODE_AWAY);
			status_code = MSIM_STATUS_CODE_AWAY;
			break;

		default:
			purple_debug_info("msim", "msim_set_status: unknown "
					"status interpreting as online");
			status_code = MSIM_STATUS_CODE_ONLINE;
			break;
	}

    statstring = purple_status_get_attr_string(status, "message");

    if (!statstring)
        statstring = g_strdup("");

    msim_set_status_code(session, status_code, g_strdup(statstring));
}

/** Go idle. */
void
msim_set_idle(PurpleConnection *gc, int time)
{
    MsimSession *session;

    g_return_if_fail(gc != NULL);

    session = (MsimSession *)gc->proto_data;

    g_return_if_fail(MSIM_SESSION_VALID(session));

    if (time == 0)
    {
        /* Going back from idle. In msim, idle is mutually exclusive 
         * from the other states (you can only be away or idle, but not
         * both, for example), so by going non-idle I go online.
         */
        /* TODO: find out how to keep old status string? */
        msim_set_status_code(session, MSIM_STATUS_CODE_ONLINE, g_strdup(""));
    } else {
        /* msim doesn't support idle time, so just go idle */
        msim_set_status_code(session, MSIM_STATUS_CODE_IDLE, g_strdup(""));
    }
}

/** Set status using an MSIM_STATUS_CODE_* value.
 * @param status_code An MSIM_STATUS_CODE_* value.
 * @param statstring Status string, must be a dynamic string (will be freed by msim_send).
 */
void 
msim_set_status_code(MsimSession *session, guint status_code, gchar *statstring)
{
    g_return_if_fail(MSIM_SESSION_VALID(session));

    purple_debug_info("msim", "msim_set_status_code: going to set status to code=%d,str=%s\n",
            status_code, statstring);

	if (!msim_send(session,
			"status", MSIM_TYPE_INTEGER, status_code,
			"sesskey", MSIM_TYPE_INTEGER, session->sesskey,
			"statstring", MSIM_TYPE_STRING, statstring, 
			"locstring", MSIM_TYPE_STRING, g_strdup(""),
            NULL))
	{
		purple_debug_info("msim", "msim_set_status: failed to set status");
	}

}

/** After a uid is resolved to username, tag it with the username and submit for processing. 
 * 
 * @param session
 * @param userinfo Response messsage to resolving request.
 * @param data MsimMessage *, the message to attach information to. 
 */
static void 
msim_incoming_resolved(MsimSession *session, MsimMessage *userinfo, 
		gpointer data)
{
	gchar *body_str;
	GHashTable *body;
	gchar *username;
	MsimMessage *msg;

    g_return_if_fail(MSIM_SESSION_VALID(session));
    g_return_if_fail(userinfo != NULL);

	body_str = msim_msg_get_string(userinfo, "body");
	g_return_if_fail(body_str != NULL);
	body = msim_parse_body(body_str);
	g_return_if_fail(body != NULL);
	g_free(body_str);

	username = g_hash_table_lookup(body, "UserName");
	g_return_if_fail(username != NULL);

	msg = (MsimMessage *)data;
    g_return_if_fail(msg != NULL);

	/* Special elements name beginning with '_', we'll use internally within the
	 * program (did not come from the wire). */
	msg = msim_msg_append(msg, "_username", MSIM_TYPE_STRING, g_strdup(username));

	msim_process(session, msg);

	/* TODO: Free copy cloned from  msim_preprocess_incoming(). */
	//XXX msim_msg_free(msg);
	g_hash_table_destroy(body);
}

#if 0
/* Lookup a username by userid, from buddy list. 
 *
 * @param wanted_uid
 *
 * @return Username of wanted_uid, if on blist, or NULL. Static string. 
 *
 * XXX WARNING: UNKNOWN MEMORY CORRUPTION HERE! 
 */
static const gchar *
msim_uid2username_from_blist(MsimSession *session, guint wanted_uid)
{
	GSList *buddies, *cur;

	buddies = purple_find_buddies(session->account, NULL); 

	if (!buddies)
	{
		purple_debug_info("msim", "msim_uid2username_from_blist: no buddies?");
		return NULL;
	}

	for (cur = buddies; cur != NULL; cur = g_slist_next(cur))
	{
		PurpleBuddy *buddy;
		//PurpleBlistNode *node;
		guint uid;
		const gchar *name;


		/* See finch/gnthistory.c */
		buddy = cur->data;
		//node  = cur->data;

		uid = purple_blist_node_get_int(&buddy->node, "UserID");
		//uid = purple_blist_node_get_int(node, "UserID");

		/* name = buddy->name; */								/* crash */
		/* name = PURPLE_BLIST_NODE_NAME(&buddy->node);  */		/* crash */

		/* XXX Is this right? Memory corruption here somehow. Happens only
		 * when return one of these values. */
		name = purple_buddy_get_name(buddy); 					/* crash */
		//name = purple_buddy_get_name((PurpleBuddy *)node); 	/* crash */
		/* return name; */										/* crash (with above) */

		/* name = NULL; */										/* no crash */
		/* return NULL; */										/* no crash (with anything) */

		/* crash =
*** glibc detected *** pidgin: realloc(): invalid pointer: 0x0000000000d2aec0 ***
======= Backtrace: =========
/lib/libc.so.6(__libc_realloc+0x323)[0x2b7bfc012e03]
/usr/lib/libglib-2.0.so.0(g_realloc+0x31)[0x2b7bfba79a41]
/usr/lib/libgtk-x11-2.0.so.0(gtk_tree_path_append_index+0x3a)[0x2b7bfa110d5a]
/usr/lib/libgtk-x11-2.0.so.0[0x2b7bfa1287dc]
/usr/lib/libgtk-x11-2.0.so.0[0x2b7bfa128e56]
/usr/lib/libgtk-x11-2.0.so.0[0x2b7bfa128efd]
/usr/lib/libglib-2.0.so.0(g_main_context_dispatch+0x1b4)[0x2b7bfba72c84]
/usr/lib/libglib-2.0.so.0[0x2b7bfba75acd]
/usr/lib/libglib-2.0.so.0(g_main_loop_run+0x1ca)[0x2b7bfba75dda]
/usr/lib/libgtk-x11-2.0.so.0(gtk_main+0xa3)[0x2b7bfa0475f3]
pidgin(main+0x8be)[0x46b45e]
/lib/libc.so.6(__libc_start_main+0xf4)[0x2b7bfbfbf0c4]
pidgin(gtk_widget_grab_focus+0x39)[0x429ab9]

or:
 *** glibc detected *** /usr/local/bin/pidgin: malloc(): memory corruption (fast): 0x0000000000c10076 ***
 (gdb) bt
#0  0x00002b4074ecd47b in raise () from /lib/libc.so.6
#1  0x00002b4074eceda0 in abort () from /lib/libc.so.6
#2  0x00002b4074f0453b in __fsetlocking () from /lib/libc.so.6
#3  0x00002b4074f0c810 in free () from /lib/libc.so.6
#4  0x00002b4074f0d6dd in malloc () from /lib/libc.so.6
#5  0x00002b4074974b5b in g_malloc () from /usr/lib/libglib-2.0.so.0
#6  0x00002b40749868bf in g_strdup () from /usr/lib/libglib-2.0.so.0
#7  0x00002b407810969f in msim_parse (
    raw=0xd2a910 "\\bm\\100\\f\\3656574\\msg\\|s|0|ss|Offline")
	    at message.c:648
#8  0x00002b407810889c in msim_input_cb (gc_uncasted=0xcf92c0, 
    source=<value optimized out>, cond=<value optimized out>) at myspace.c:1478


	Why is it crashing in msim_parse()'s g_strdup()?
*/
		purple_debug_info("msim", "msim_uid2username_from_blist: %s's uid=%d (want %d)\n",
				name, uid, wanted_uid);

		if (uid == wanted_uid)
		{
			gchar *ret;

			ret = g_strdup(name);

			g_slist_free(buddies);

			return ret;
		}
	}

	g_slist_free(buddies);
	return NULL;
}
#endif

/** Preprocess incoming messages, resolving as needed, calling msim_process() when ready to process.
 *
 * @param session
 * @param msg MsimMessage *, freed by caller.
 */
gboolean 
msim_preprocess_incoming(MsimSession *session, MsimMessage *msg)
{
    g_return_val_if_fail(MSIM_SESSION_VALID(session), FALSE);
    g_return_val_if_fail(msg != NULL, FALSE);

	if (msim_msg_get(msg, "bm") && msim_msg_get(msg, "f"))
	{
		guint uid;
		const gchar *username;

		/* 'f' = userid message is from, in buddy messages */
		uid = msim_msg_get_integer(msg, "f");

		/* TODO: Make caching work. Currently it is commented out because
		 * it crashes for unknown reasons, memory realloc error. */
#if 0
		username = msim_uid2username_from_blist(session, uid); 
#else
		username = NULL; 
#endif

		if (username)
		{
			/* Know username already, use it. */
			purple_debug_info("msim", "msim_preprocess_incoming: tagging with _username=%s\n",
					username);
			msg = msim_msg_append(msg, "_username", MSIM_TYPE_STRING, g_strdup(username));
			return msim_process(session, msg);

		} else {
			gchar *from;

			/* Send lookup request. */
			/* XXX: where is msim_msg_get_string() freed? make _strdup and _nonstrdup. */
			purple_debug_info("msim", "msim_incoming: sending lookup, setting up callback\n");
			from = msim_msg_get_string(msg, "f");
			msim_lookup_user(session, from, msim_incoming_resolved, msim_msg_clone(msg)); 
			g_free(from);

			/* indeterminate */
			return TRUE;
		}
	} else {
		/* Nothing to resolve - send directly to processing. */
		return msim_process(session, msg);
	}
}

/** Check if the connection is still alive, based on last communication. */
gboolean
msim_check_alive(gpointer data)
{
    MsimSession *session;
    time_t delta;
    gchar *errmsg;

    session = (MsimSession *)data;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), FALSE);

    delta = time(NULL) - session->last_comm;
    //purple_debug_info("msim", "msim_check_alive: delta=%d\n", delta);
    if (delta >= MSIM_KEEPALIVE_INTERVAL)
    {
        errmsg = g_strdup_printf(_("Connection to server lost (no data received within %d seconds)"), (int)delta);

        purple_debug_info("msim", "msim_check_alive: %s > interval of %d, presumed dead\n",
                errmsg, MSIM_KEEPALIVE_INTERVAL);
        purple_connection_error(session->gc, errmsg);

        purple_notify_error(session->gc, NULL, errmsg, NULL);

        g_free(errmsg);

        return FALSE;
    }

    return TRUE;
}

/** Handle mail reply checks. */
void
msim_check_inbox_cb(MsimSession *session, MsimMessage *reply, gpointer data)
{
    GHashTable *body;
    gchar *body_str;
    GString *notification;
    guint old_inbox_status;
    guint i;

    /* Three parallel arrays for each new inbox message type. */
    static const gchar *inbox_keys[] = 
    { 
        "Mail", 
        "BlogComment", 
        "ProfileComment", 
        "FriendRequest", 
        "PictureComment" 
    };

    static const guint inbox_bits[] = 
    { 
        MSIM_INBOX_MAIL, 
        MSIM_INBOX_BLOG_COMMENT,
        MSIM_INBOX_PROFILE_COMMENT,
        MSIM_INBOX_FRIEND_REQUEST,
        MSIM_INBOX_PICTURE_COMMENT
    };

    static const gchar *inbox_urls[] =
    {
        "http://messaging.myspace.com/index.cfm?fuseaction=mail.inbox",
        "http://blog.myspace.com/index.cfm?fuseaction=blog",
        "http://home.myspace.com/index.cfm?fuseaction=user",
        "http://messaging.myspace.com/index.cfm?fuseaction=mail.friendRequests",
        "http://home.myspace.com/index.cfm?fuseaction=user"
    };

    static const gchar *inbox_text[5];

    /* Can't write _()'d strings in array initializers. Workaround. */
    inbox_text[0] = _("New mail messages");
    inbox_text[1] = _("New blog comments");
    inbox_text[2] = _("New profile comments");
    inbox_text[3] = _("New friend requests!");
    inbox_text[4] = _("New picture comments");

    g_return_if_fail(reply != NULL);

    msim_msg_dump("msim_check_inbox_cb: reply=%s\n", reply);

    body_str = msim_msg_get_string(reply, "body");
    g_return_if_fail(body_str != NULL);

    body = msim_parse_body(body_str);
    g_free(body_str);

    notification = g_string_new("");

    old_inbox_status = session->inbox_status;

    for (i = 0; i < sizeof(inbox_keys) / sizeof(inbox_keys[0]); ++i)
    {
        const gchar *key;
        guint bit;
        
        key = inbox_keys[i];
        bit = inbox_bits[i];

        if (g_hash_table_lookup(body, key))
        {
            /* Notify only on when _changes_ from no mail -> has mail
             * (edge triggered) */
            if (!(session->inbox_status & bit))
            {
                gchar *str;

                str = g_strdup_printf(
                        "<p><a href=\"%s\">%s</a><br>\n",
                        inbox_urls[i], inbox_text[i]);

                g_string_append(notification, str);
                
                g_free(str);
            } else {
                purple_debug_info("msim",
                        "msim_check_inbox_cb: already notified of %s\n",
                        key);
                /* TODO: some kind of non-intrusitive notification? */
            }

            session->inbox_status |= bit;
        }
    }

    if (notification->len)
    {
        purple_debug_info("msim",
                "msim_check_inbox_cb: notifying %s\n", notification->str);

        purple_notify_formatted(session->account, 
                _("New Inbox Messages"), _("New Inbox Messages"), NULL,
                notification->str,
                NULL, NULL);

    }

    g_hash_table_destroy(body);
}

/* Send request to check if there is new mail. */
gboolean
msim_check_inbox(gpointer data)
{
    MsimSession *session;

    session = (MsimSession *)data;

    purple_debug_info("msim", "msim_check_inbox: checking mail\n");
    g_return_val_if_fail(msim_send(session, 
			"persist", MSIM_TYPE_INTEGER, 1,
			"sesskey", MSIM_TYPE_INTEGER, session->sesskey,
			"cmd", MSIM_TYPE_INTEGER, MSIM_CMD_GET,
			"dsn", MSIM_TYPE_INTEGER, MG_CHECK_MAIL_DSN,
			"lid", MSIM_TYPE_INTEGER, MG_CHECK_MAIL_LID,
			"uid", MSIM_TYPE_INTEGER, session->userid,
			"rid", MSIM_TYPE_INTEGER, 
                msim_new_reply_callback(session, msim_check_inbox_cb, NULL),
			"body", MSIM_TYPE_STRING, g_strdup(""),
			NULL), TRUE);

    /* Always return true, so that we keep checking for mail. */
    return TRUE;
}

/** Called when the session key arrives. */
gboolean
msim_we_are_logged_on(MsimSession *session, MsimMessage *msg)
{
    g_return_val_if_fail(MSIM_SESSION_VALID(session), FALSE);
    g_return_val_if_fail(msg != NULL, FALSE);

    purple_connection_update_progress(session->gc, _("Connected"), 3, 4);

    session->sesskey = msim_msg_get_integer(msg, "sesskey");
    purple_debug_info("msim", "SESSKEY=<%d>\n", session->sesskey);

    /* Comes with: proof,profileid,userid,uniquenick -- all same values
     * some of the time, but can vary. This is our own user ID. */
    session->userid = msim_msg_get_integer(msg, "userid");

    purple_connection_set_state(session->gc, PURPLE_CONNECTED);

    /* We now know are our own username, only after we're logged in..
     * which is weird, but happens because you login with your email
     * address and not username. Will be freed in msim_session_destroy(). */
    session->username = msim_msg_get_string(msg, "uniquenick");


    purple_debug_info("msim", "msim_we_are_logged_on: notifying servers of status\n");
    /* Notify servers of our current status. */
    msim_set_status(session->account,
            purple_account_get_active_status(session->account));

    /* Disable due to problems with timeouts. TODO: fix. */
#ifdef MSIM_USE_KEEPALIVE
    purple_timeout_add(MSIM_KEEPALIVE_INTERVAL_CHECK, 
            (GSourceFunc)msim_check_alive, session);
#endif

    purple_timeout_add(MSIM_MAIL_INTERVAL_CHECK, 
            (GSourceFunc)msim_check_inbox, session);

    msim_check_inbox(session);

    return TRUE;
}

/**
 * Process a message. 
 *
 * @param session
 * @param msg A message from the server, ready for processing (possibly with resolved username information attached). Caller frees.
 *
 * @return TRUE if successful. FALSE if processing failed.
 */
gboolean 
msim_process(MsimSession *session, MsimMessage *msg)
{
    g_return_val_if_fail(session != NULL, FALSE);
    g_return_val_if_fail(msg != NULL, FALSE);

#ifdef MSIM_DEBUG_MSG
	{
		msim_msg_dump("ready to process: %s\n", msg);
	}
#endif

    if (msim_msg_get(msg, "nc"))
    {
        return msim_login_challenge(session, msg);
    } else if (msim_msg_get(msg, "sesskey")) {
        return msim_we_are_logged_on(session, msg);
    } else if (msim_msg_get(msg, "bm"))  {
        guint bm;
       
        bm = msim_msg_get_integer(msg, "bm");
        switch (bm)
        {
            case MSIM_BM_STATUS:
                return msim_status(session, msg);
            case MSIM_BM_INSTANT:
                return msim_incoming_im(session, msg);
			case MSIM_BM_ACTION:
				return msim_incoming_action(session, msg);
            default:
                /* Not really an IM, but show it for informational 
                 * purposes during development. */
                return msim_incoming_im(session, msg);
        }
    } else if (msim_msg_get(msg, "rid")) {
        return msim_process_reply(session, msg);
    } else if (msim_msg_get(msg, "error")) {
        return msim_error(session, msg);
    } else if (msim_msg_get(msg, "ka")) {
        return TRUE;
    } else {
		msim_unrecognized(session, msg, "in msim_process");
        return FALSE;
    }
}

/** Store an field of information about a buddy. */
void 
msim_store_buddy_info_each(gpointer key, gpointer value, gpointer user_data)
{
	PurpleBuddy *buddy;
	gchar *key_str, *value_str;

	buddy = (PurpleBuddy *)user_data;
	key_str = (gchar *)key;
	value_str = (gchar *)value;

	if (strcmp(key_str, "UserID") == 0 ||
			strcmp(key_str, "Age") == 0 ||
			strcmp(key_str, "TotalFriends") == 0)
	{
		/* Certain fields get set as integers, instead of strings, for
		 * convenience. May not be the best way to do it, but having at least
		 * UserID as an integer is convenient...until it overflows! */
		purple_blist_node_set_int(&buddy->node, key_str, atol(value_str));
	} else {
		purple_blist_node_set_string(&buddy->node, key_str, value_str);
	}
}

/** Save buddy information to the buddy list from a user info reply message.
 *
 * @param session
 * @param msg The user information reply, with any amount of information.
 *
 * The information is saved to the buddy's blist node, which ends up in blist.xml.
 */
gboolean 
msim_store_buddy_info(MsimSession *session, MsimMessage *msg)
{
	GHashTable *body;
	gchar *username, *body_str, *uid;
	PurpleBuddy *buddy;
	guint rid;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), FALSE);
    g_return_val_if_fail(msg != NULL, FALSE);
 
	rid = msim_msg_get_integer(msg, "rid");
	
	g_return_val_if_fail(rid != 0, FALSE);

	body_str = msim_msg_get_string(msg, "body");
	g_return_val_if_fail(body_str != NULL, FALSE);
	body = msim_parse_body(body_str);
	g_free(body_str);

	/* TODO: implement a better hash-like interface, and use it. */
	username = g_hash_table_lookup(body, "UserName");

	if (!username)
	{
		purple_debug_info("msim", 
			"msim_process_reply: not caching body, no UserName\n");
        g_hash_table_destroy(body);
		return FALSE;
	}

	uid = g_hash_table_lookup(body, "UserID");
    if (!uid)
    {
        g_hash_table_destroy(body);
        g_return_val_if_fail(uid, FALSE);
    }

	purple_debug_info("msim", "associating uid %d with username %s\n", uid, username);

	buddy = purple_find_buddy(session->account, username);
	if (buddy)
	{
		g_hash_table_foreach(body, msim_store_buddy_info_each, buddy);
	}

    g_hash_table_destroy(body);

	return TRUE;
}

/**
 * Process a persistance message reply from the server.
 *
 * @param session 
 * @param msg Message reply from server.
 *
 * @return TRUE if successful.
 */
gboolean 
msim_process_reply(MsimSession *session, MsimMessage *msg)
{
    g_return_val_if_fail(MSIM_SESSION_VALID(session), FALSE);
    g_return_val_if_fail(msg != NULL, FALSE);
    
    if (msim_msg_get(msg, "rid"))  /* msim_lookup_user sets callback for here */
    {
        MSIM_USER_LOOKUP_CB cb;
        gpointer data;
        guint rid;

		msim_store_buddy_info(session, msg);		

		rid = msim_msg_get_integer(msg, "rid");

        /* If a callback is registered for this userid lookup, call it. */
        cb = g_hash_table_lookup(session->user_lookup_cb, GUINT_TO_POINTER(rid));
        data = g_hash_table_lookup(session->user_lookup_cb_data, GUINT_TO_POINTER(rid));

        if (cb)
        {
            purple_debug_info("msim", 
					"msim_process_body: calling callback now\n");
			/* Clone message, so that the callback 'cb' can use it (needs to free it also). */
            cb(session, msim_msg_clone(msg), data);
            g_hash_table_remove(session->user_lookup_cb, GUINT_TO_POINTER(rid));
            g_hash_table_remove(session->user_lookup_cb_data, GUINT_TO_POINTER(rid));
        } else {
            purple_debug_info("msim", 
					"msim_process_body: no callback for rid %d\n", rid);
        }
    }

	return TRUE;
}

/**
 * Handle an error from the server.
 *
 * @param session 
 * @param msg The message.
 *
 * @return TRUE if successfully reported error.
 */
gboolean 
msim_error(MsimSession *session, MsimMessage *msg)
{
    gchar *errmsg, *full_errmsg;
	guint err;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), FALSE);
    g_return_val_if_fail(msg != NULL, FALSE);

    err = msim_msg_get_integer(msg, "err");
    errmsg = msim_msg_get_string(msg, "errmsg");

    full_errmsg = g_strdup_printf(_("Protocol error, code %d: %s"), err, 
			errmsg ? errmsg : "no 'errmsg' given");

	g_free(errmsg);

    purple_debug_info("msim", "msim_error: %s\n", full_errmsg);

    purple_notify_error(session->account, g_strdup(_("MySpaceIM Error")), 
            full_errmsg, NULL);

	/* Destroy session if fatal. */
    if (msim_msg_get(msg, "fatal"))
    {
        purple_debug_info("msim", "fatal error, closing\n");
        purple_connection_error(session->gc, full_errmsg);
    }

    return TRUE;
}

/**
 * Process incoming status messages.
 *
 * @param session
 * @param msg Status update message. Caller frees.
 *
 * @return TRUE if successful.
 */
gboolean 
msim_status(MsimSession *session, MsimMessage *msg)
{
    PurpleBuddyList *blist;
    PurpleBuddy *buddy;
    //PurpleStatus *status;
    gchar **status_array;
    GList *list;
    gchar *status_headline;
    gchar *status_str;
    gint i, status_code, purple_status_code;
    gchar *username;

    g_return_val_if_fail(MSIM_SESSION_VALID(session), FALSE);
    g_return_val_if_fail(msg != NULL, FALSE);

    status_str = msim_msg_get_string(msg, "msg");
	g_return_val_if_fail(status_str != NULL, FALSE);

	msim_msg_dump("msim_status msg=%s\n", msg);

	/* Helpfully looked up by msim_incoming_resolve() for us. */
    username = msim_msg_get_string(msg, "_username");
    /* Note: DisplayName doesn't seem to be resolvable. It could be displayed on
     * the buddy list, if the UserID was stored along with it. */

	if (username == NULL)
	{
		g_free(status_str);
		g_return_val_if_fail(NULL, FALSE);
	}

    purple_debug_info("msim", 
			"msim_status: updating status for <%s> to <%s>\n", 
			username, status_str);

    /* TODO: generic functions to split into a GList, part of MsimMessage */
    status_array = g_strsplit(status_str, "|", 0);
    for (list = NULL, i = 0;
            status_array[i];
            i++)
    {
		/* Note: this adds the 0th ordinal too, which might not be a value
		 * at all (the 0 in the 0|1|2|3... status fields, but 0 always appears blank).
		 */
        list = g_list_append(list, status_array[i]);
    }

    /* Example fields: 
	 *  |s|0|ss|Offline 
	 *  |s|1|ss|:-)|ls||ip|0|p|0 
	 *
	 * TODO: write list support in MsimMessage, and use it here.
	 */

    status_code = atoi(g_list_nth_data(list, MSIM_STATUS_ORDINAL_ONLINE));
	purple_debug_info("msim", "msim_status: %s's status code = %d\n", username, status_code);
    status_headline = g_list_nth_data(list, MSIM_STATUS_ORDINAL_HEADLINE);

    blist = purple_get_blist();

    /* Add buddy if not found */
    buddy = purple_find_buddy(session->account, username);
    if (!buddy)
    {
        purple_debug_info("msim", 
				"msim_status: making new buddy for %s\n", username);
        buddy = purple_buddy_new(session->account, username, NULL);

        purple_blist_add_buddy(buddy, NULL, NULL, NULL);

		/* All buddies on list should have 'uid' integer associated with them. */
		purple_blist_node_set_int(&buddy->node, "UserID", msim_msg_get_integer(msg, "f"));
		
		msim_store_buddy_info(session, msg);
    } else {
        purple_debug_info("msim", "msim_status: found buddy %s\n", username);
    }

	purple_blist_node_set_string(&buddy->node, "Headline", status_headline);
  
    /* Set user status */	
    switch (status_code)
	{
		case MSIM_STATUS_CODE_OFFLINE_OR_HIDDEN: 
			purple_status_code = PURPLE_STATUS_OFFLINE;	
			break;

		case MSIM_STATUS_CODE_ONLINE: 
			purple_status_code = PURPLE_STATUS_AVAILABLE;
			break;

		case MSIM_STATUS_CODE_AWAY:
			purple_status_code = PURPLE_STATUS_AWAY;
			break;

        case MSIM_STATUS_CODE_IDLE:
            /* will be handled below */
            purple_status_code = -1;
            break;

		default:
				purple_debug_info("msim", "msim_status for %s, unknown status code %d, treating as available\n",
						username, status_code);
				purple_status_code = PURPLE_STATUS_AVAILABLE;
	}

    purple_prpl_got_user_status(session->account, username, purple_primitive_get_id_from_type(purple_status_code), NULL);

    if (status_code == MSIM_STATUS_CODE_IDLE)
    {
        purple_debug_info("msim", "msim_status: got idle: %s\n", username);
        purple_prpl_got_user_idle(session->account, username, TRUE, time(NULL));
    } else {
        /* All other statuses indicate going back to non-idle. */
        purple_prpl_got_user_idle(session->account, username, FALSE, time(NULL));
    }

    g_strfreev(status_array);
	g_free(status_str);
	g_free(username);
    g_list_free(list);

    return TRUE;
}

/** Add a buddy to user's buddy list. */
void 
msim_add_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
	MsimSession *session;
	MsimMessage *msg;
    /* MsimMessage	*msg_blocklist; */

	session = (MsimSession *)gc->proto_data;
	purple_debug_info("msim", "msim_add_buddy: want to add %s to %s\n", buddy->name,
			group ? group->name : "(no group)");

	msg = msim_msg_new(TRUE,
			"addbuddy", MSIM_TYPE_BOOLEAN, TRUE,
			"sesskey", MSIM_TYPE_INTEGER, session->sesskey,
			/* "newprofileid" will be inserted here with uid. */
			"reason", MSIM_TYPE_STRING, g_strdup(""),
			NULL);

	if (!msim_postprocess_outgoing(session, msg, buddy->name, "newprofileid", "reason"))
	{
		purple_notify_error(NULL, NULL, _("Failed to add buddy"), _("'addbuddy' command failed."));
		msim_msg_free(msg);
		return;
	}
	msim_msg_free(msg);
	
	/* TODO: if addbuddy fails ('error' message is returned), delete added buddy from
	 * buddy list since Purple adds it locally. */

	/* TODO: Update blocklist. */
#if 0
	msg_blocklist = msim_msg_new(TRUE,
		"persist", MSIM_TYPE_INTEGER, 1,
		"sesskey", MSIM_TYPE_INTEGER, session->sesskey,
		"cmd", MSIM_TYPE_INTEGER, MSIM_CMD_BIT_ACTION | MSIM_CMD_PUT,
		"dsn", MSIM_TYPE_INTEGER, MC_CONTACT_INFO_DSN,
		"lid", MSIM_TYPE_INTEGER, MC_CONTACT_INFO_LID,
		/* TODO: Use msim_new_reply_callback to get rid. */
		"rid", MSIM_TYPE_INTEGER, session->next_rid++,
		"body", MSIM_TYPE_STRING,
			g_strdup_printf("ContactID=<uid>\034"
			"GroupName=%s\034"
			"Position=1000\034"
			"Visibility=1\034"
			"NickName=\034"
			"NameSelect=0",
			"Friends" /*group->name*/ ));

	if (!msim_postprocess_outgoing(session, msg, buddy->name, "body", NULL))
	{
		purple_notify_error(NULL, NULL, _("Failed to add buddy"), _("persist command failed"));
		msim_msg_free(msg_blocklist);
		return;
	}
	msim_msg_free(msg_blocklist);
#endif
}

/** Perform actual postprocessing on a message, adding userid as specified.
 *
 * @param msg The message to postprocess.
 * @param uid_before Name of field where to insert new field before, or NULL for end.
 * @param uid_field_name Name of field to add uid to.
 * @param uid The userid to insert.
 *
 * If the field named by uid_field_name already exists, then its string contents will
 * be used for the field, except "<uid>" will be replaced by the userid.
 *
 * If the field named by uid_field_name does not exist, it will be added before the
 * field named by uid_before, as an integer, with the userid.
 *
 * Does not handle sending, or scheduling userid lookup. For that, see msim_postprocess_outgoing().
 */ 
MsimMessage *
msim_do_postprocessing(MsimMessage *msg, const gchar *uid_before, 
		const gchar *uid_field_name, guint uid)
{	
	purple_debug_info("msim", "msim_do_postprocessing called with ufn=%s, ub=%s, uid=%d\n",
			uid_field_name, uid_before, uid);
	msim_msg_dump("msim_do_postprocessing msg: %s\n", msg);

	/* First, check - if the field already exists, treat it as a format string. */
	if (msim_msg_get(msg, uid_field_name))
	{
		MsimMessageElement *elem;
		gchar *fmt_string;
		gchar *uid_str;

		/* Warning: this probably violates the encapsulation of MsimMessage */

		elem = msim_msg_get(msg, uid_field_name);
		g_return_val_if_fail(elem->type == MSIM_TYPE_STRING, NULL);

		/* Get the raw string, not with msim_msg_get_string() since that copies it. 
		 * Want the original string so can free it. */
		fmt_string = (gchar *)(elem->data);

		uid_str = g_strdup_printf("%d", uid);
		elem->data = str_replace(fmt_string, "<uid>", uid_str);
		g_free(uid_str);
		g_free(fmt_string);

		purple_debug_info("msim", "msim_postprocess_outgoing_cb: formatted new string, %s\n",
				elem->data);

	} else {
		/* Otherwise, insert new field into outgoing message. */
		msg = msim_msg_insert_before(msg, uid_before, uid_field_name, MSIM_TYPE_INTEGER, GUINT_TO_POINTER(uid));
	}

	return msg;
}	

/** Callback for msim_postprocess_outgoing() to add a userid to a message, and send it (once receiving userid).
 *
 * @param session
 * @param userinfo The user information reply message, containing the user ID
 * @param data The message to postprocess and send.
 *
 * The data message should contain these fields:
 *
 *  _uid_field_name: string, name of field to add with userid from userinfo message
 *  _uid_before: string, name of field before field to insert, or NULL for end
 *
 *
*/
void 
msim_postprocess_outgoing_cb(MsimSession *session, MsimMessage *userinfo, gpointer data)
{
	gchar *body_str;
	GHashTable *body;
	gchar *uid, *uid_field_name, *uid_before;
	MsimMessage *msg;

	msg = (MsimMessage *)data;

	msim_msg_dump("msim_postprocess_outgoing_cb() got msg=%s\n", msg);

	/* Obtain userid from userinfo message. */
	body_str = msim_msg_get_string(userinfo, "body");
	g_return_if_fail(body_str != NULL);
	body = msim_parse_body(body_str);
	g_free(body_str);

	uid = g_strdup(g_hash_table_lookup(body, "UserID"));
	g_hash_table_destroy(body);

	uid_field_name = msim_msg_get_string(msg, "_uid_field_name");
	uid_before = msim_msg_get_string(msg, "_uid_before");

	msg = msim_do_postprocessing(msg, uid_before, uid_field_name, atol(uid));

	/* Send */
	if (!msim_msg_send(session, msg))
	{
		purple_debug_info("msim", "msim_postprocess_outgoing_cb: sending failed for message: %s\n", msg);
	}


	/* Free field names AFTER sending message, because MsimMessage does NOT copy
	 * field names - instead, treats them as static strings (which they usually are).
	 */
	g_free(uid_field_name);
	g_free(uid_before);

    g_hash_table_destroy(body);

	//msim_msg_free(msg);
}

/** Postprocess and send a message.
 *
 * @param session
 * @param msg Message to postprocess. Will NOT be freed.
 * @param username Username to resolve. Assumed to be a static string (will not be freed or copied).
 * @param uid_field_name Name of new field to add, containing uid of username. Static string.
 * @param uid_before Name of existing field to insert username field before. Static string.
 *
 * @return Postprocessed message.
 */
gboolean 
msim_postprocess_outgoing(MsimSession *session, MsimMessage *msg, 
		const gchar *username, const gchar *uid_field_name, 
		const gchar *uid_before)
{
    PurpleBuddy *buddy;
	guint uid;
	gboolean rc;

	/* Store information for msim_postprocess_outgoing_cb(). */
	purple_debug_info("msim", "msim_postprocess_outgoing(u=%s,ufn=%s,ub=%s)\n",
			username, uid_field_name, uid_before);
	msim_msg_dump("msim_postprocess_outgoing: msg before=%s\n", msg);
	msg = msim_msg_append(msg, "_username", MSIM_TYPE_STRING, g_strdup(username));
	msg = msim_msg_append(msg, "_uid_field_name", MSIM_TYPE_STRING, g_strdup(uid_field_name));
	msg = msim_msg_append(msg, "_uid_before", MSIM_TYPE_STRING, g_strdup(uid_before));

	/* First, try the most obvious. If numeric userid is given, use that directly. */
    if (msim_is_userid(username))
    {
		uid = atol(username);
    } else {
		/* Next, see if on buddy list and know uid. */
		buddy = purple_find_buddy(session->account, username);
		if (buddy)
		{
			uid = purple_blist_node_get_int(&buddy->node, "UserID");
		} else {
			uid = 0;
		}

		if (!buddy || !uid)
		{
			/* Don't have uid offhand - need to ask for it, and wait until hear back before sending. */
			purple_debug_info("msim", ">>> msim_postprocess_outgoing: couldn't find username %s in blist\n",
					username);
			msim_msg_dump("msim_postprocess_outgoing - scheduling lookup, msg=%s\n", msg);
			/* TODO: where is cloned message freed? Should be in _cb. */
			msim_lookup_user(session, username, msim_postprocess_outgoing_cb, msim_msg_clone(msg));
			return TRUE;		/* not sure of status yet - haven't sent! */
		}
	}
	
	/* Already have uid, postprocess and send msg immediately. */
	purple_debug_info("msim", "msim_postprocess_outgoing: found username %s has uid %d\n",
			username, uid);

	msg = msim_do_postprocessing(msg, uid_before, uid_field_name, uid);

	msim_msg_dump("msim_postprocess_outgoing: msg after (uid immediate)=%s\n", msg);
	
	rc = msim_msg_send(session, msg);

	//msim_msg_free(msg);

	return rc;
}

/** Remove a buddy from the user's buddy list. */
void 
msim_remove_buddy(PurpleConnection *gc, PurpleBuddy *buddy, PurpleGroup *group)
{
	MsimSession *session;
	MsimMessage *delbuddy_msg;
	MsimMessage *persist_msg;
	MsimMessage *blocklist_msg;

	session = (MsimSession *)gc->proto_data;

	delbuddy_msg = msim_msg_new(TRUE,
				"delbuddy", MSIM_TYPE_BOOLEAN, TRUE,
				"sesskey", MSIM_TYPE_INTEGER, session->sesskey,
				/* 'delprofileid' with uid will be inserted here. */
				NULL);
	/* TODO: free msg */
	if (!msim_postprocess_outgoing(session, delbuddy_msg, buddy->name, "delprofileid", NULL))
	{
		purple_notify_error(NULL, NULL, _("Failed to remove buddy"), _("'delbuddy' command failed"));
		return;
	}

	persist_msg = msim_msg_new(TRUE, 
			"persist", MSIM_TYPE_INTEGER, 1,
			"sesskey", MSIM_TYPE_INTEGER, session->sesskey,
			"cmd", MSIM_TYPE_INTEGER, MSIM_CMD_BIT_ACTION | MSIM_CMD_DELETE,
			"dsn", MSIM_TYPE_INTEGER, MD_DELETE_BUDDY_DSN,
			"lid", MSIM_TYPE_INTEGER, MD_DELETE_BUDDY_LID,
			"uid", MSIM_TYPE_INTEGER, session->userid,
			"rid", MSIM_TYPE_INTEGER, session->next_rid++,
			/* <uid> will be replaced by postprocessing */
			"body", MSIM_TYPE_STRING, g_strdup("ContactID=<uid>"),
			NULL);

	/* TODO: free msg */
	if (!msim_postprocess_outgoing(session, persist_msg, buddy->name, "body", NULL))
	{
		purple_notify_error(NULL, NULL, _("Failed to remove buddy"), _("persist command failed"));	
		return;
	}

	blocklist_msg = msim_msg_new(TRUE,
			"blocklist", MSIM_TYPE_BOOLEAN, TRUE,
			"sesskey", MSIM_TYPE_INTEGER, session->sesskey,
			/* TODO: MsimMessage lists */
			"idlist", MSIM_TYPE_STRING, g_strdup("a-|<uid>|b-|<uid>"),
			NULL);

	if (!msim_postprocess_outgoing(session, blocklist_msg, buddy->name, "idlist", NULL))
	{
		purple_notify_error(NULL, NULL, _("Failed to remove buddy"), _("blocklist command failed"));
		return;
	}
}

/** Return whether the buddy can be messaged while offline.
 *
 * The protocol supports offline messages in just the same way as online
 * messages.
 */
gboolean 
msim_offline_message(const PurpleBuddy *buddy)
{
	return TRUE;
}

/**
 * Callback when input available.
 *
 * @param gc_uncasted A PurpleConnection pointer.
 * @param source File descriptor.
 * @param cond PURPLE_INPUT_READ
 *
 * Reads the input, and calls msim_preprocess_incoming() to handle it.
 */
void 
msim_input_cb(gpointer gc_uncasted, gint source, PurpleInputCondition cond)
{
    PurpleConnection *gc;
    PurpleAccount *account;
    MsimSession *session;
    gchar *end;
    int n;

    g_return_if_fail(gc_uncasted != NULL);
    g_return_if_fail(source >= 0);  /* Note: 0 is a valid fd */

    gc = (PurpleConnection *)(gc_uncasted);
    account = purple_connection_get_account(gc);
    session = gc->proto_data;

    g_return_if_fail(cond == PURPLE_INPUT_READ);
	g_return_if_fail(MSIM_SESSION_VALID(session));

    /* Mark down that we got data, so don't timeout. */
    session->last_comm = time(NULL);

    /* Only can handle so much data at once... 
     * If this happens, try recompiling with a higher MSIM_READ_BUF_SIZE.
     * Should be large enough to hold the largest protocol message.
     */
    if (session->rxoff == MSIM_READ_BUF_SIZE)
    {
        purple_debug_error("msim", "msim_input_cb: %d-byte read buffer full!\n",
                MSIM_READ_BUF_SIZE);
        purple_connection_error(gc, _("Read buffer full"));
        return;
    }

    purple_debug_info("msim", "buffer at %d (max %d), reading up to %d\n",
            session->rxoff, MSIM_READ_BUF_SIZE, 
			MSIM_READ_BUF_SIZE - session->rxoff);

    /* Read into buffer. On Win32, need recv() not read(). session->fd also holds
     * the file descriptor, but it sometimes differs from the 'source' parameter.
     */
    n = recv(session->fd, session->rxbuf + session->rxoff, MSIM_READ_BUF_SIZE - session->rxoff, 0);

    if (n < 0 && errno == EAGAIN)
    {
        return;
    }
    else if (n < 0)
    {
        purple_debug_error("msim", "msim_input_cb: read error, ret=%d, "
			"error=%s, source=%d, fd=%d (%X))\n", 
			n, strerror(errno), source, session->fd, session->fd);
        purple_connection_error(gc, _("Read error"));
        return;
    } 
    else if (n == 0)
    {
        purple_debug_info("msim", "msim_input_cb: server disconnected\n");
        purple_connection_error(gc, _("Server has disconnected"));
        return;
    }

    /* Null terminate */
    session->rxbuf[session->rxoff + n] = 0;

#ifdef MSIM_CHECK_EMBEDDED_NULLS
    /* Check for embedded NULs. I don't handle them, and they shouldn't occur. */
    if (strlen(session->rxbuf + session->rxoff) != n)
    {
        /* Occurs after login, but it is not a null byte. */
        purple_debug_info("msim", "msim_input_cb: strlen=%d, but read %d bytes"
                "--null byte encountered?\n", 
				strlen(session->rxbuf + session->rxoff), n);
        //purple_connection_error(gc, "Invalid message - null byte on input");
        return;
    }
#endif

    session->rxoff += n;
    purple_debug_info("msim", "msim_input_cb: read=%d\n", n);

#ifdef MSIM_DEBUG_RXBUF
    purple_debug_info("msim", "buf=<%s>\n", session->rxbuf);
#endif

    /* Look for \\final\\ end markers. If found, process message. */
    while((end = strstr(session->rxbuf, MSIM_FINAL_STRING)))
    {
        MsimMessage *msg;

#ifdef MSIM_DEBUG_RXBUF
        purple_debug_info("msim", "in loop: buf=<%s>\n", session->rxbuf);
#endif
        *end = 0;
        msg = msim_parse(g_strdup(session->rxbuf));
        if (!msg)
        {
            purple_debug_info("msim", "msim_input_cb: couldn't parse <%s>\n", 
					session->rxbuf);
            purple_connection_error(gc, _("Unparseable message"));
        }
        else
        {
            /* Process message and then free it (processing function should
			 * clone message if it wants to keep it afterwards.) */
            if (!msim_preprocess_incoming(session, msg))
			{
				msim_msg_dump("msim_input_cb: preprocessing message failed on msg: %s\n", msg);
			}
			msim_msg_free(msg);
        }

        /* Move remaining part of buffer to beginning. */
        session->rxoff -= strlen(session->rxbuf) + strlen(MSIM_FINAL_STRING);
        memmove(session->rxbuf, end + strlen(MSIM_FINAL_STRING), 
                MSIM_READ_BUF_SIZE - (end + strlen(MSIM_FINAL_STRING) - session->rxbuf));

        /* Clear end of buffer */
        //memset(end, 0, MSIM_READ_BUF_SIZE - (end - session->rxbuf));
    }
}

/* Setup a callback, to be called when a reply is received with the returned rid.
 *
 * @param cb The callback, an MSIM_USER_LOOKUP_CB.
 * @param data Arbitrary user data to be passed to callback (probably an MsimMessage *).
 *
 * @return The request/reply ID, used to link replies with requests. Put the rid in your request.
 *
 * TODO: Make more generic and more specific:
 * 1) MSIM_USER_LOOKUP_CB - make it for PERSIST_REPLY, not just user lookup
 * 2) data - make it an MsimMessage?
 */
guint 
msim_new_reply_callback(MsimSession *session, MSIM_USER_LOOKUP_CB cb, 
		gpointer data)
{
	guint rid;

	rid = session->next_rid++;

    g_hash_table_insert(session->user_lookup_cb, GUINT_TO_POINTER(rid), cb);
    g_hash_table_insert(session->user_lookup_cb_data, GUINT_TO_POINTER(rid), data);

	return rid;
}

/**
 * Callback when connected. Sets up input handlers.
 *
 * @param data A PurpleConnection pointer.
 * @param source File descriptor.
 * @param error_message
 */
void 
msim_connect_cb(gpointer data, gint source, const gchar *error_message)
{
    PurpleConnection *gc;
    MsimSession *session;

    g_return_if_fail(data != NULL);

    gc = (PurpleConnection *)data;
    session = (MsimSession *)gc->proto_data;

    if (source < 0)
    {
        purple_connection_error(gc, _("Couldn't connect to host"));
        purple_connection_error(gc, g_strdup_printf(
					_("Couldn't connect to host: %s (%d)"), 
                    error_message ? error_message : "no message given", 
					source));
        return;
    }

    session->fd = source; 

    gc->inpa = purple_input_add(source, PURPLE_INPUT_READ, msim_input_cb, gc);


}

/* Session methods */

/**
 * Create a new MSIM session.
 *
 * @param acct The account to create the session from.
 *
 * @return Pointer to a new session. Free with msim_session_destroy.
 */
MsimSession *
msim_session_new(PurpleAccount *acct)
{
    MsimSession *session;

    g_return_val_if_fail(acct != NULL, NULL);

    session = g_new0(MsimSession, 1);

    session->magic = MSIM_SESSION_STRUCT_MAGIC;
    session->account = acct;
    session->gc = purple_account_get_connection(acct);
	session->sesskey = 0;
	session->userid = 0;
	session->username = NULL;
    session->fd = -1;

	/* TODO: Remove. */
    session->user_lookup_cb = g_hash_table_new_full(g_direct_hash, 
			g_direct_equal, NULL, NULL);  /* do NOT free function pointers! (values) */
    session->user_lookup_cb_data = g_hash_table_new_full(g_direct_hash, 
			g_direct_equal, NULL, NULL);/* TODO: we don't know what the values are,
											 they could be integers inside gpointers
											 or strings, so I don't freed them.
											 Figure this out, once free cache. */
    session->rxoff = 0;
    session->rxbuf = g_new0(gchar, MSIM_READ_BUF_SIZE);
	session->next_rid = 1;
    session->last_comm = time(NULL);
    session->inbox_status = 0;
    
    return session;
}

/**
 * Free a session.
 *
 * @param session The session to destroy.
 */
void 
msim_session_destroy(MsimSession *session)
{
    g_return_if_fail(MSIM_SESSION_VALID(session));
	
    session->magic = -1;

    g_free(session->rxbuf);
	g_free(session->username);

	/* TODO: Remove. */
	g_hash_table_destroy(session->user_lookup_cb);
	g_hash_table_destroy(session->user_lookup_cb_data);
	
    g_free(session);
}
                 
/** 
 * Close the connection.
 * 
 * @param gc The connection.
 */
void 
msim_close(PurpleConnection *gc)
{
	MsimSession *session;

	if (gc == NULL)
		return;

	session = (MsimSession *)gc->proto_data;
	if (session == NULL)
		return;

	gc->proto_data = NULL;

	if (!MSIM_SESSION_VALID(session))
		return;

    if (session->gc->inpa)
		purple_input_remove(session->gc->inpa);

    msim_session_destroy(session);
}


/**
 * Check if a string is a userid (all numeric).
 *
 * @param user The user id, email, or name.
 *
 * @return TRUE if is userid, FALSE if not.
 */
gboolean 
msim_is_userid(const gchar *user)
{
    g_return_val_if_fail(user != NULL, FALSE);

    return strspn(user, "0123456789") == strlen(user);
}

/**
 * Check if a string is an email address (contains an @).
 *
 * @param user The user id, email, or name.
 *
 * @return TRUE if is an email, FALSE if not.
 *
 * This function is not intended to be used as a generic
 * means of validating email addresses, but to distinguish
 * between a user represented by an email address from
 * other forms of identification.
 */ 
gboolean 
msim_is_email(const gchar *user)
{
    g_return_val_if_fail(user != NULL, FALSE);

    return strchr(user, '@') != NULL;
}


/**
 * Asynchronously lookup user information, calling callback when receive result.
 *
 * @param session
 * @param user The user id, email address, or username. Not freed.
 * @param cb Callback, called with user information when available.
 * @param data An arbitray data pointer passed to the callback.
 */
/* TODO: change to not use callbacks */
void 
msim_lookup_user(MsimSession *session, const gchar *user, 
		MSIM_USER_LOOKUP_CB cb, gpointer data)
{
    gchar *field_name;
    guint rid, cmd, dsn, lid;

    g_return_if_fail(MSIM_SESSION_VALID(session));
    g_return_if_fail(user != NULL);
    g_return_if_fail(cb != NULL);

    purple_debug_info("msim", "msim_lookup_userid: "
			"asynchronously looking up <%s>\n", user);

	msim_msg_dump("msim_lookup_user: data=%s\n", (MsimMessage *)data);

    /* Setup callback. Response will be associated with request using 'rid'. */
    rid = msim_new_reply_callback(session, cb, data);

    /* Send request */

    cmd = MSIM_CMD_GET;

    if (msim_is_userid(user))
    {
        field_name = "UserID";
        dsn = MG_MYSPACE_INFO_BY_ID_DSN; 
        lid = MG_MYSPACE_INFO_BY_ID_LID; 
    } else if (msim_is_email(user)) {
        field_name = "Email";
        dsn = MG_MYSPACE_INFO_BY_STRING_DSN;
        lid = MG_MYSPACE_INFO_BY_STRING_LID;
    } else {
        field_name = "UserName";
        dsn = MG_MYSPACE_INFO_BY_STRING_DSN;
        lid = MG_MYSPACE_INFO_BY_STRING_LID;
    }


	g_return_if_fail(msim_send(session,
			"persist", MSIM_TYPE_INTEGER, 1,
			"sesskey", MSIM_TYPE_INTEGER, session->sesskey,
			"cmd", MSIM_TYPE_INTEGER, 1,
			"dsn", MSIM_TYPE_INTEGER, dsn,
			"uid", MSIM_TYPE_INTEGER, session->userid,
			"lid", MSIM_TYPE_INTEGER, lid,
			"rid", MSIM_TYPE_INTEGER, rid,
			/* TODO: dictionary field type */
			"body", MSIM_TYPE_STRING, 
				g_strdup_printf("%s=%s", field_name, user),
			NULL));
} 


/**
 * Obtain the status text for a buddy.
 *
 * @param buddy The buddy to obtain status text for.
 *
 * @return Status text, or NULL if error. Caller g_free()'s.
 *
 */
char *
msim_status_text(PurpleBuddy *buddy)
{
    MsimSession *session;
	const gchar *display_name, *headline;

    g_return_val_if_fail(buddy != NULL, NULL);

    session = (MsimSession *)buddy->account->gc->proto_data;
    g_return_val_if_fail(MSIM_SESSION_VALID(session), NULL);

	display_name = headline = NULL;

	/* Retrieve display name and/or headline, depending on user preference. */
    if (purple_account_get_bool(session->account, "show_display_name", TRUE))
	{
		display_name = purple_blist_node_get_string(&buddy->node, "DisplayName");
	} 

    if (purple_account_get_bool(session->account, "show_headline", FALSE))
	{
		headline = purple_blist_node_get_string(&buddy->node, "Headline");
	}

	/* Return appropriate combination of display name and/or headline, or neither. */

	if (display_name && headline)
		return g_strconcat(display_name, " ", headline, NULL);

	if (display_name)
		return g_strdup(display_name);

	if (headline)
		return g_strdup(headline);

	return NULL;
}

/**
 * Obtain the tooltip text for a buddy.
 *
 * @param buddy Buddy to obtain tooltip text on.
 * @param user_info Variable modified to have the tooltip text.
 * @param full TRUE if should obtain full tooltip text.
 *
 */
void 
msim_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *user_info, 
		gboolean full)
{
	const gchar *str, *str2;
	gint n;

    g_return_if_fail(buddy != NULL);
    g_return_if_fail(user_info != NULL);

    if (PURPLE_BUDDY_IS_ONLINE(buddy))
    {
        MsimSession *session;

        session = (MsimSession *)buddy->account->gc->proto_data;

        g_return_if_fail(MSIM_SESSION_VALID(session));

        /* TODO: if (full), do something different */
		
		/* Useful to identify the account the tooltip refers to. 
		 *  Other prpls show this. */
		str = purple_blist_node_get_string(&buddy->node, "UserName"); 
		if (str)
			purple_notify_user_info_add_pair(user_info, _("User Name"), str);

		/* a/s/l...the vitals */	
		n = purple_blist_node_get_int(&buddy->node, "Age");
		if (n)
			purple_notify_user_info_add_pair(user_info, _("Age"),
					g_strdup_printf("%d", n));

		str = purple_blist_node_get_string(&buddy->node, "Gender");
		if (str)
			purple_notify_user_info_add_pair(user_info, _("Gender"), str);

		str = purple_blist_node_get_string(&buddy->node, "Location");
		if (str)
			purple_notify_user_info_add_pair(user_info, _("Location"), str);

		/* Other information */
 		str = purple_blist_node_get_string(&buddy->node, "Headline");
		if (str)
			purple_notify_user_info_add_pair(user_info, _("Headline"), str);

		str = purple_blist_node_get_string(&buddy->node, "BandName");
		str2 = purple_blist_node_get_string(&buddy->node, "SongName");
		if (str || str2)
			purple_notify_user_info_add_pair(user_info, _("Song"), 
                g_strdup_printf("%s - %s",
					str ? str : _("Unknown Artist"),
					str2 ? str2 : _("Unknown Song")));

		n = purple_blist_node_get_int(&buddy->node, "TotalFriends");
		if (n)
			purple_notify_user_info_add_pair(user_info, _("Total Friends"),
				g_strdup_printf("%d", n));

    }
}

/** Callbacks called by Purple, to access this plugin. */
PurplePluginProtocolInfo prpl_info =
{
	/* options */
      OPT_PROTO_USE_POINTSIZE		/* specify font size in sane point size */
	| OPT_PROTO_MAIL_CHECK,

	/* | OPT_PROTO_IM_IMAGE - TODO: direct images. */	
    NULL,              /* user_splits */
    NULL,              /* protocol_options */
    NO_BUDDY_ICONS,    /* icon_spec - TODO: eventually should add this */
    msim_list_icon,    /* list_icon */
    NULL,              /* list_emblems */
    msim_status_text,  /* status_text */
    msim_tooltip_text, /* tooltip_text */
    msim_status_types, /* status_types */
    NULL,              /* blist_node_menu */
    NULL,              /* chat_info */
    NULL,              /* chat_info_defaults */
    msim_login,        /* login */
    msim_close,        /* close */
    msim_send_im,      /* send_im */
    NULL,              /* set_info */
    msim_send_typing,  /* send_typing */
	msim_get_info, 	   /* get_info */
    msim_set_status,   /* set_status */
    msim_set_idle,     /* set_idle */
    NULL,              /* change_passwd */
    msim_add_buddy,    /* add_buddy */
    NULL,              /* add_buddies */
    msim_remove_buddy, /* remove_buddy */
    NULL,              /* remove_buddies */
    NULL,              /* add_permit */
    NULL,              /* add_deny */
    NULL,              /* rem_permit */
    NULL,              /* rem_deny */
    NULL,              /* set_permit_deny */
    NULL,              /* join_chat */
    NULL,              /* reject chat invite */
    NULL,              /* get_chat_name */
    NULL,              /* chat_invite */
    NULL,              /* chat_leave */
    NULL,              /* chat_whisper */
    NULL,              /* chat_send */
    NULL,              /* keepalive */
    NULL,              /* register_user */
    NULL,              /* get_cb_info */
    NULL,              /* get_cb_away */
    NULL,              /* alias_buddy */
    NULL,              /* group_buddy */
    NULL,              /* rename_group */
    NULL,              /* buddy_free */
    NULL,              /* convo_closed */
    NULL,              /* normalize */
    NULL,              /* set_buddy_icon */
    NULL,              /* remove_group */
    NULL,              /* get_cb_real_name */
    NULL,              /* set_chat_topic */
    NULL,              /* find_blist_chat */
    NULL,              /* roomlist_get_list */
    NULL,              /* roomlist_cancel */
    NULL,              /* roomlist_expand_category */
    NULL,              /* can_receive_file */
    NULL,              /* send_file */
    NULL,              /* new_xfer */
    msim_offline_message, /* offline_message */
    NULL,              /* whiteboard_prpl_ops */
    msim_send_really_raw,     /* send_raw */
    NULL,              /* roomlist_room_serialize */
	NULL,			   /* _purple_reserved1 */
	NULL,			   /* _purple_reserved2 */
	NULL,			   /* _purple_reserved3 */
	NULL 			   /* _purple_reserved4 */
};



/** Based on MSN's plugin info comments. */
PurplePluginInfo info =
{
    PURPLE_PLUGIN_MAGIC,                                
    PURPLE_MAJOR_VERSION,
    PURPLE_MINOR_VERSION,
    PURPLE_PLUGIN_PROTOCOL,                            /**< type           */
    NULL,                                              /**< ui_requirement */
    0,                                                 /**< flags          */
    NULL,                                              /**< dependencies   */
    PURPLE_PRIORITY_DEFAULT,                           /**< priority       */

    "prpl-myspace",                                   /**< id             */
    "MySpaceIM",                                      /**< name           */
    "0.10",                                           /**< version        */
                                                      /**  summary        */
    "MySpaceIM Protocol Plugin",
                                                      /**  description    */
    "MySpaceIM Protocol Plugin",
    "Jeff Connelly <jeff2@soc.pidgin.im>",         /**< author         */
    "http://developer.pidgin.im/wiki/MySpaceIM/",     /**< homepage       */

    msim_load,                                        /**< load           */
    NULL,                                             /**< unload         */
    NULL,                                             /**< destroy        */
    NULL,                                             /**< ui_info        */
    &prpl_info,                                       /**< extra_info     */
    NULL,                                             /**< prefs_info     */

    /* msim_actions */
    NULL,

	NULL,											  /**< reserved1      */
	NULL,											  /**< reserved2      */
	NULL,											  /**< reserved3      */
	NULL 											  /**< reserved4      */
};


#ifdef MSIM_SELF_TEST
/** Test functions.
 * Used to test or try out the internal workings of msimprpl. If you're reading
 * this code for the first time, these functions can be instructive in how
 * msimprpl is architected.
 */
void 
msim_test_all(void) 
{
	guint failures;


	failures = 0;
	failures += msim_test_xml();
	failures += msim_test_msg();
	failures += msim_test_escaping();

	if (failures)
	{
		purple_debug_info("msim", "msim_test_all HAD FAILURES: %d\n", failures);
	}
	else
	{
		purple_debug_info("msim", "msim_test_all - all tests passed!\n");
	}
	exit(0);
}

int 
msim_test_xml(void)
{
	gchar *msg_text;
	xmlnode *root, *n;
	guint failures;
	char *s;
	int len;

	failures = 0;
	
	msg_text = "<p><f n=\"Arial\" h=\"12\">woo!</f>xxx<c v='black'>yyy</c></p>";

	purple_debug_info("msim", "msim_test_xml: msg_text=%s\n", msg_text);

	root = xmlnode_from_str(msg_text, -1);
	if (!root)
	{
		purple_debug_info("msim", "there is no root\n");
		exit(0);
	}

	purple_debug_info("msim", "root name=%s, child name=%s\n", root->name,
			root->child->name);

	purple_debug_info("msim", "last child name=%s\n", root->lastchild->name);
	purple_debug_info("msim", "Root xml=%s\n", 
			xmlnode_to_str(root, &len));
	purple_debug_info("msim", "Child xml=%s\n", 
			xmlnode_to_str(root->child, &len));
	purple_debug_info("msim", "Lastchild xml=%s\n", 
			xmlnode_to_str(root->lastchild, &len));
	purple_debug_info("msim", "Next xml=%s\n", 
			xmlnode_to_str(root->next, &len));
	purple_debug_info("msim", "Next data=%s\n", 
			xmlnode_get_data(root->next));
	purple_debug_info("msim", "Child->next xml=%s\n", 
			xmlnode_to_str(root->child->next, &len));

	for (n = root->child; n; n = n->next)
	{
		if (n->name) 
		{
			purple_debug_info("msim", " ** n=%s\n",n->name);
		} else {
			purple_debug_info("msim", " ** n data=%s\n", n->data);
		}
	}

	purple_debug_info("msim", "root data=%s, child data=%s, child 'h'=%s\n", 
			xmlnode_get_data(root),
			xmlnode_get_data(root->child),
			xmlnode_get_attrib(root->child, "h"));


	for (n = root->child;
			n != NULL;
			n = n->next)
	{
		purple_debug_info("msim", "next name=%s\n", n->name);
	}

	s = xmlnode_to_str(root, &len);
	s[len] = 0;

	purple_debug_info("msim", "str: %s\n", s);
	g_free(s);

	xmlnode_free(root);

	exit(0);

	return failures;
}

/** Test MsimMessage for basic functionality. */
int 
msim_test_msg(void)
{
	MsimMessage *msg, *msg_cloned;
	gchar *packed, *packed_expected, *packed_cloned;
	guint failures;

	failures = 0;

	purple_debug_info("msim", "\n\nTesting MsimMessage\n");
	msg = msim_msg_new(FALSE);		/* Create a new, empty message. */

	/* Append some new elements. */
	msg = msim_msg_append(msg, "bx", MSIM_TYPE_BINARY, g_string_new_len(g_strdup("XXX"), 3));
	msg = msim_msg_append(msg, "k1", MSIM_TYPE_STRING, g_strdup("v1"));
	msg = msim_msg_append(msg, "k1", MSIM_TYPE_INTEGER, GUINT_TO_POINTER(42));
	msg = msim_msg_append(msg, "k1", MSIM_TYPE_STRING, g_strdup("v43"));
	msg = msim_msg_append(msg, "k1", MSIM_TYPE_STRING, g_strdup("v52/xxx\\yyy"));
	msg = msim_msg_append(msg, "k1", MSIM_TYPE_STRING, g_strdup("v7"));
	msim_msg_dump("msg debug str=%s\n", msg);
	packed = msim_msg_pack(msg);

	purple_debug_info("msim", "msg packed=%s\n", packed);

	packed_expected = "\\bx\\WFhY\\k1\\v1\\k1\\42\\k1"
		"\\v43\\k1\\v52/1xxx/2yyy\\k1\\v7\\final\\";

	if (0 != strcmp(packed, packed_expected))
	{
		purple_debug_info("msim", "!!!(%d), msim_msg_pack not what expected: %s != %s\n",
				++failures, packed, packed_expected);
	}


	msg_cloned = msim_msg_clone(msg);
	packed_cloned = msim_msg_pack(msg_cloned);

	purple_debug_info("msim", "msg cloned=%s\n", packed_cloned);
	if (0 != strcmp(packed, packed_cloned))
	{
		purple_debug_info("msim", "!!!(%d), msim_msg_pack on cloned message not equal to original: %s != %s\n",
				++failures, packed_cloned, packed);
	}

	g_free(packed);
	g_free(packed_cloned);
	msim_msg_free(msg_cloned);
	msim_msg_free(msg);

	return failures;
}

/** Test protocol-level escaping/unescaping. */
int 
msim_test_escaping(void)
{
	guint failures;
	gchar *raw, *escaped, *unescaped, *expected;

	failures = 0;

	purple_debug_info("msim", "\n\nTesting escaping\n");

	raw = "hello/world\\hello/world";

	escaped = msim_escape(raw);
	purple_debug_info("msim", "msim_test_escaping: raw=%s, escaped=%s\n", raw, escaped);
	expected = "hello/1world/2hello/1world";
	if (0 != strcmp(escaped, expected))
	{
		purple_debug_info("msim", "!!!(%d), msim_escape failed: %s != %s\n",
				++failures, escaped, expected);
	}


	unescaped = msim_unescape(escaped);
	g_free(escaped);
	purple_debug_info("msim", "msim_test_escaping: unescaped=%s\n", unescaped);
	if (0 != strcmp(raw, unescaped))
	{
		purple_debug_info("msim", "!!!(%d), msim_unescape failed: %s != %s\n",
				++failures, raw, unescaped);
	}	

	return failures;
}
#endif

/** Initialize plugin. */
void 
init_plugin(PurplePlugin *plugin) 
{
	PurpleAccountOption *option;
#ifdef MSIM_SELF_TEST
	msim_test_all();
#endif /* MSIM_SELF_TEST */

	/* TODO: default to automatically try different ports. Make the user be
	 * able to set the first port to try (like LastConnectedPort in Windows client).  */
	option = purple_account_option_string_new(_("Connect server"), "server", MSIM_SERVER);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_int_new(_("Connect port"), "port", MSIM_PORT);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_bool_new(_("Show display name in status text"), "show_display_name", TRUE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = purple_account_option_bool_new(_("Show headline in status text"), "show_headline", TRUE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
}

PURPLE_INIT_PLUGIN(myspace, init_plugin, info);
