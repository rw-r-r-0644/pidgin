/*
 * gaim
 *
 * Some code copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
 * libfaim code copyright 1998, 1999 Adam Fritzler <afritz@auk.cx>
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
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
/* this must happen before sys/socket.h or freebsd won't compile */

#ifndef _WIN32
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <signal.h>

#include "gaim.h"
#include "multi.h"
#include "prpl.h"
#include "core.h"
#include "aim.h"
#include "proxy.h"

#ifdef _WIN32
#include "win32dep.h"
#endif

/* constants to identify proto_opts */
#define USEROPT_AUTH      0
#define USEROPT_AUTHPORT  1

#define UC_AOL		0x02
#define UC_ADMIN	0x04
#define UC_UNCONFIRMED	0x08
#define UC_NORMAL	0x10
#define UC_AB		0x20
#define UC_WIRELESS	0x40

#define AIMHASHDATA "http://gaim.sourceforge.net/aim_data.php3"

static struct prpl *my_protocol = NULL;

/* For win32 compatability */
G_MODULE_IMPORT GSList *connections;
G_MODULE_IMPORT int report_idle;

static int caps_aim = AIM_CAPS_CHAT | AIM_CAPS_BUDDYICON | AIM_CAPS_IMIMAGE | AIM_CAPS_SENDFILE | AIM_CAPS_INTEROPERATE;
static int caps_icq = AIM_CAPS_BUDDYICON | AIM_CAPS_IMIMAGE | AIM_CAPS_SENDFILE | AIM_CAPS_ICQUTF8 | AIM_CAPS_INTEROPERATE;

static fu8_t features_aim[] = {0x01, 0x01, 0x01, 0x02};
static fu8_t features_icq[] = {0x01, 0x06};

struct oscar_data {
	aim_session_t *sess;
	aim_conn_t *conn;

	guint cnpa;
	guint paspa;
	guint emlpa;
	guint icopa;

	gboolean iconconnecting;

	GSList *create_rooms;

	gboolean conf;
	gboolean reqemail;
	gboolean setemail;
	char *email;
	gboolean setnick;
	char *newsn;
	gboolean chpass;
	char *oldp;
	char *newp;

	GSList *oscar_chats;
	GSList *direct_ims;
	GSList *file_transfers;
	GHashTable *buddyinfo;
	GSList *requesticon;

	gboolean killme;
	gboolean icq;
	GSList *evilhack;
	guint icontimer;

	struct {
		guint maxwatchers; /* max users who can watch you */
		guint maxbuddies; /* max users you can watch */
		guint maxgroups; /* max groups in server list */
		guint maxpermits; /* max users on permit list */
		guint maxdenies; /* max users on deny list */
		guint maxsiglen; /* max size (bytes) of profile */
		guint maxawaymsglen; /* max size (bytes) of posted away message */
	} rights;
};

struct create_room {
	char *name;
	int exchange;
};

struct chat_connection {
	char *name;
	char *show; /* AOL did something funny to us */
	fu16_t exchange;
	fu16_t instance;
	int fd; /* this is redundant since we have the conn below */
	aim_conn_t *conn;
	int inpa;
	int id;
	struct gaim_connection *gc; /* i hate this. */
	struct gaim_conversation *cnv; /* bah. */
	int maxlen;
	int maxvis;
};

struct direct_im {
	struct gaim_connection *gc;
	char name[80];
	int watcher;
	aim_conn_t *conn;
	gboolean connected;
};

struct ask_direct {
	struct gaim_connection *gc;
	char *sn;
	char ip[64];
	fu8_t cookie[8];
};

/* BBB */
struct oscar_xfer_data {
	fu8_t cookie[8];
	gchar *clientip;
	gchar *clientip2;
	fu32_t modtime;
	fu32_t checksum;
	aim_conn_t *conn;
	struct gaim_xfer *xfer;
	struct gaim_connection *gc;
};

/* Various PRPL-specific buddy info that we want to keep track of */
struct buddyinfo {
	time_t signon;
	int caps;
	gboolean typingnot;

	unsigned long ico_len;
	unsigned long ico_csum;
	time_t ico_time;
	gboolean ico_need;

	unsigned long ico_me_len;
	unsigned long ico_me_csum;
	time_t ico_me_time;
	gboolean ico_informed;

	fu16_t iconstrlen;
	fu8_t iconstr[30];
};

struct name_data {
	struct gaim_connection *gc;
	gchar *name;
	gchar *nick;
};

static void gaim_free_name_data(struct name_data *data) {
	g_free(data->name);
	g_free(data->nick);
	g_free(data);
}

static struct direct_im *find_direct_im(struct oscar_data *od, const char *who) {
	GSList *d = od->direct_ims;
	struct direct_im *m = NULL;

	while (d) {
		m = (struct direct_im *)d->data;
		if (!aim_sncmp(who, m->name))
			return m;
		d = d->next;
	}

	return NULL;
}

static char *extract_name(const char *name) {
	char *tmp, *x;
	int i, j;

	if (!name)
		return NULL;
	
	x = strchr(name, '-');

	if (!x) return NULL;
	x = strchr(++x, '-');
	if (!x) return NULL;
	tmp = g_strdup(++x);

	for (i = 0, j = 0; x[i]; i++) {
		char hex[3];
		if (x[i] != '%') {
			tmp[j++] = x[i];
			continue;
		}
		strncpy(hex, x + ++i, 2); hex[2] = 0;
		i++;
		tmp[j++] = strtol(hex, NULL, 16);
	}

	tmp[j] = 0;
	return tmp;
}

static struct chat_connection *find_oscar_chat(struct gaim_connection *gc, int id) {
	GSList *g = ((struct oscar_data *)gc->proto_data)->oscar_chats;
	struct chat_connection *c = NULL;

	while (g) {
		c = (struct chat_connection *)g->data;
		if (c->id == id)
			break;
		g = g->next;
		c = NULL;
	}

	return c;
}

static struct chat_connection *find_oscar_chat_by_conn(struct gaim_connection *gc,
							aim_conn_t *conn) {
	GSList *g = ((struct oscar_data *)gc->proto_data)->oscar_chats;
	struct chat_connection *c = NULL;

	while (g) {
		c = (struct chat_connection *)g->data;
		if (c->conn == conn)
			break;
		g = g->next;
		c = NULL;
	}

	return c;
}

/* All the libfaim->gaim callback functions */
static int gaim_parse_auth_resp  (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_login      (aim_session_t *, aim_frame_t *, ...);
static int gaim_handle_redirect  (aim_session_t *, aim_frame_t *, ...);
static int gaim_info_change      (aim_session_t *, aim_frame_t *, ...);
static int gaim_account_confirm  (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_oncoming   (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_offgoing   (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_incoming_im(aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_misses     (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_clientauto (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_user_info  (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_motd       (aim_session_t *, aim_frame_t *, ...);
static int gaim_chatnav_info     (aim_session_t *, aim_frame_t *, ...);
static int gaim_chat_join        (aim_session_t *, aim_frame_t *, ...);
static int gaim_chat_leave       (aim_session_t *, aim_frame_t *, ...);
static int gaim_chat_info_update (aim_session_t *, aim_frame_t *, ...);
static int gaim_chat_incoming_msg(aim_session_t *, aim_frame_t *, ...);
static int gaim_email_parseupdate(aim_session_t *, aim_frame_t *, ...);
static int gaim_icon_error       (aim_session_t *, aim_frame_t *, ...);
static int gaim_icon_parseicon   (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_msgack     (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_ratechange (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_evilnotify (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_searcherror(aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_searchreply(aim_session_t *, aim_frame_t *, ...);
static int gaim_bosrights        (aim_session_t *, aim_frame_t *, ...);
static int gaim_connerr          (aim_session_t *, aim_frame_t *, ...);
static int conninitdone_admin    (aim_session_t *, aim_frame_t *, ...);
static int conninitdone_bos      (aim_session_t *, aim_frame_t *, ...);
static int conninitdone_chatnav  (aim_session_t *, aim_frame_t *, ...);
static int conninitdone_chat     (aim_session_t *, aim_frame_t *, ...);
static int conninitdone_email    (aim_session_t *, aim_frame_t *, ...);
static int conninitdone_icon     (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_msgerr     (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_mtn        (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_locaterights(aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_buddyrights(aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_locerr     (aim_session_t *, aim_frame_t *, ...);
static int gaim_icbm_param_info  (aim_session_t *, aim_frame_t *, ...);
static int gaim_parse_genericerr (aim_session_t *, aim_frame_t *, ...);
static int gaim_memrequest       (aim_session_t *, aim_frame_t *, ...);
static int gaim_selfinfo         (aim_session_t *, aim_frame_t *, ...);
static int gaim_offlinemsg       (aim_session_t *, aim_frame_t *, ...);
static int gaim_offlinemsgdone   (aim_session_t *, aim_frame_t *, ...);
static int gaim_icqalias         (aim_session_t *, aim_frame_t *, ...);
static int gaim_icqinfo          (aim_session_t *, aim_frame_t *, ...);
static int gaim_popup            (aim_session_t *, aim_frame_t *, ...);
#ifndef NOSSI
static int gaim_ssi_parseerr     (aim_session_t *, aim_frame_t *, ...);
static int gaim_ssi_parserights  (aim_session_t *, aim_frame_t *, ...);
static int gaim_ssi_parselist    (aim_session_t *, aim_frame_t *, ...);
static int gaim_ssi_parseack     (aim_session_t *, aim_frame_t *, ...);
static int gaim_ssi_authgiven    (aim_session_t *, aim_frame_t *, ...);
static int gaim_ssi_authrequest  (aim_session_t *, aim_frame_t *, ...);
static int gaim_ssi_authreply    (aim_session_t *, aim_frame_t *, ...);
static int gaim_ssi_gotadded     (aim_session_t *, aim_frame_t *, ...);
#endif

/* for DirectIM/image transfer */
static int gaim_odc_initiate     (aim_session_t *, aim_frame_t *, ...);
static int gaim_odc_incoming     (aim_session_t *, aim_frame_t *, ...);
static int gaim_odc_typing       (aim_session_t *, aim_frame_t *, ...);
static int gaim_update_ui        (aim_session_t *, aim_frame_t *, ...);

/* for file transfer */
static int oscar_sendfile_estblsh(aim_session_t *, aim_frame_t *, ...);
static int oscar_sendfile_prompt (aim_session_t *, aim_frame_t *, ...);
static int oscar_sendfile_ack    (aim_session_t *, aim_frame_t *, ...);
static int oscar_sendfile_done   (aim_session_t *, aim_frame_t *, ...);

/* for icons */
static gboolean gaim_icon_timerfunc(gpointer data);

static fu32_t check_encoding(const char *utf8);
static fu32_t parse_encoding(const char *enc);

static char *msgerrreason[] = {
	N_("Invalid error"),
	N_("Invalid SNAC"),
	N_("Rate to host"),
	N_("Rate to client"),
	N_("Not logged in"),
	N_("Service unavailable"),
	N_("Service not defined"),
	N_("Obsolete SNAC"),
	N_("Not supported by host"),
	N_("Not supported by client"),
	N_("Refused by client"),
	N_("Reply too big"),
	N_("Responses lost"),
	N_("Request denied"),
	N_("Busted SNAC payload"),
	N_("Insufficient rights"),
	N_("In local permit/deny"),
	N_("Too evil (sender)"),
	N_("Too evil (receiver)"),
	N_("User temporarily unavailable"),
	N_("No match"),
	N_("List overflow"),
	N_("Request ambiguous"),
	N_("Queue full"),
	N_("Not while on AOL")
};
static int msgerrreasonlen = 25;

static void gaim_odc_disconnect(aim_session_t *sess, aim_conn_t *conn) {
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	struct gaim_conversation *cnv;
	struct direct_im *dim;
	char *sn;
	char buf[256];

	sn = g_strdup(aim_odc_getsn(conn));

	debug_printf("%s disconnected Direct IM.\n", sn);

	dim = find_direct_im(od, sn);
	od->direct_ims = g_slist_remove(od->direct_ims, dim);
	gaim_input_remove(dim->watcher);

	if (dim->connected)
		g_snprintf(buf, sizeof buf, _("Direct IM with %s closed"), sn);
	else 
		g_snprintf(buf, sizeof buf, _("Direct IM with %s failed"), sn);
		
	if ((cnv = gaim_find_conversation(sn)))
		gaim_conversation_write(cnv, NULL, buf, -1, WFLAG_SYSTEM, time(NULL));

	gaim_conversation_update_progress(cnv, 100);

	g_free(dim); /* I guess? I don't see it anywhere else... -- mid */
	g_free(sn);

	return;
}

static void oscar_callback(gpointer data, gint source, GaimInputCondition condition) {
	aim_conn_t *conn = (aim_conn_t *)data;
	aim_session_t *sess = aim_conn_getsess(conn);
	struct gaim_connection *gc = sess ? sess->aux_data : NULL;
	struct oscar_data *od;

	if (!gc) {
		/* gc is null. we return, else we seg SIGSEG on next line. */
		debug_printf("oscar callback for closed connection (1).\n");
		return;
	}
      
	od = (struct oscar_data *)gc->proto_data;

	if (!g_slist_find(connections, gc)) {
		/* oh boy. this is probably bad. i guess the only thing we 
		 * can really do is return? */
		debug_printf("oscar callback for closed connection (2).\n");
		return;
	}

	if (condition & GAIM_INPUT_READ) {
		if (conn->type == AIM_CONN_TYPE_LISTENER) {
			debug_printf("got information on rendezvous listener\n");
			if (aim_handlerendconnect(od->sess, conn) < 0) {
				debug_printf("connection error (rendezvous listener)\n");
				aim_conn_kill(od->sess, &conn);
			}
		} else {
			if (aim_get_command(od->sess, conn) >= 0) {
				aim_rxdispatch(od->sess);
                                if (od->killme)
                                        signoff(gc);
			} else {
				if ((conn->type == AIM_CONN_TYPE_BOS) ||
					   !(aim_getconn_type(od->sess, AIM_CONN_TYPE_BOS))) {
					debug_printf("major connection error\n");
					hide_login_progress_error(gc, _("Disconnected."));
					signoff(gc);
				} else if (conn->type == AIM_CONN_TYPE_CHAT) {
					struct chat_connection *c = find_oscar_chat_by_conn(gc, conn);
					char buf[BUF_LONG];
					debug_printf("disconnected from chat room %s\n", c->name);
					c->conn = NULL;
					if (c->inpa > 0)
						gaim_input_remove(c->inpa);
					c->inpa = 0;
					c->fd = -1;
					aim_conn_kill(od->sess, &conn);
					snprintf(buf, sizeof(buf), _("You have been disconnected from chat room %s."), c->name);
					do_error_dialog(buf, NULL, GAIM_ERROR);
				} else if (conn->type == AIM_CONN_TYPE_CHATNAV) {
					if (od->cnpa > 0)
						gaim_input_remove(od->cnpa);
					od->cnpa = 0;
					debug_printf("removing chatnav input watcher\n");
					while (od->create_rooms) {
						struct create_room *cr = od->create_rooms->data;
						g_free(cr->name);
						od->create_rooms =
							g_slist_remove(od->create_rooms, cr);
						g_free(cr);
						do_error_dialog(_("Chat is currently unavailable"), NULL, GAIM_ERROR);
					}
					aim_conn_kill(od->sess, &conn);
				} else if (conn->type == AIM_CONN_TYPE_AUTH) {
					if (od->paspa > 0)
						gaim_input_remove(od->paspa);
					od->paspa = 0;
					debug_printf("removing authconn input watcher\n");
					aim_conn_kill(od->sess, &conn);
				} else if (conn->type == AIM_CONN_TYPE_EMAIL) {
					if (od->emlpa > 0)
						gaim_input_remove(od->emlpa);
					od->emlpa = 0;
					debug_printf("removing email input watcher\n");
					aim_conn_kill(od->sess, &conn);
				} else if (conn->type == AIM_CONN_TYPE_ICON) {
					if (od->icopa > 0)
						gaim_input_remove(od->icopa);
					od->icopa = 0;
					debug_printf("removing icon input watcher\n");
					aim_conn_kill(od->sess, &conn);
				} else if (conn->type == AIM_CONN_TYPE_RENDEZVOUS) {
					if (conn->subtype == AIM_CONN_SUBTYPE_OFT_DIRECTIM)
						gaim_odc_disconnect(od->sess, conn);
					aim_conn_kill(od->sess, &conn);
				} else {
					debug_printf("holy crap! generic connection error! %hu\n",
							conn->type);
					aim_conn_kill(od->sess, &conn);
				}
			}
		}
	}
}

static void oscar_debug(aim_session_t *sess, int level, const char *format, va_list va) {
	char *s = g_strdup_vprintf(format, va);
	char buf[256];
	char *t;
	struct gaim_connection *gc = sess->aux_data;

	g_snprintf(buf, sizeof(buf), "%s %d: ", gc->username, level);
	t = g_strconcat(buf, s, NULL);
	debug_printf(t);
	if (t[strlen(t)-1] != '\n')
		debug_printf("\n");
	g_free(t);
	g_free(s);
}

static void oscar_login_connect(gpointer data, gint source, GaimInputCondition cond)
{
	struct gaim_connection *gc = data;
	struct oscar_data *od;
	aim_session_t *sess;
	aim_conn_t *conn;

	if (!g_slist_find(connections, gc)) {
		close(source);
		return;
	}

	od = gc->proto_data;
	sess = od->sess;
	conn = aim_getconn_type_all(sess, AIM_CONN_TYPE_AUTH);
	
	conn->fd = source;

	if (source < 0) {
		hide_login_progress(gc, _("Couldn't connect to host"));
		signoff(gc);
		return;
	}

	aim_conn_completeconnect(sess, conn);
	gc->inpa = gaim_input_add(conn->fd, GAIM_INPUT_READ, oscar_callback, conn);
	debug_printf("Password sent, waiting for response\n");
}

static void oscar_login(struct gaim_account *account) {
	aim_session_t *sess;
	aim_conn_t *conn;
	char buf[256];
	struct gaim_connection *gc = new_gaim_conn(account);
	struct oscar_data *od = gc->proto_data = g_new0(struct oscar_data, 1);

	if (isdigit(*account->username)) {
		od->icq = TRUE;
		gc->password[8] = 0;
	} else {
		gc->flags |= OPT_CONN_HTML;
		gc->flags |= OPT_CONN_AUTO_RESP;
	}
	od->buddyinfo = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	sess = g_new0(aim_session_t, 1);

	aim_session_init(sess, AIM_SESS_FLAGS_NONBLOCKCONNECT, 0);
	aim_setdebuggingcb(sess, oscar_debug);

	/* we need an immediate queue because we don't use a while-loop to
	 * see if things need to be sent. */
	aim_tx_setenqueue(sess, AIM_TX_IMMEDIATE, NULL);
	od->sess = sess;
	sess->aux_data = gc;

	conn = aim_newconn(sess, AIM_CONN_TYPE_AUTH, NULL);
	if (conn == NULL) {
		debug_printf("internal connection error\n");
		hide_login_progress(gc, _("Unable to login to AIM"));
		signoff(gc);
		return;
	}

	g_snprintf(buf, sizeof(buf), _("Signon: %s"), gc->username);
	set_login_progress(gc, 2, buf);

	aim_conn_addhandler(sess, conn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNERR, gaim_connerr, 0);
	aim_conn_addhandler(sess, conn, 0x0017, 0x0007, gaim_parse_login, 0);
	aim_conn_addhandler(sess, conn, 0x0017, 0x0003, gaim_parse_auth_resp, 0);

	conn->status |= AIM_CONN_STATUS_INPROGRESS;
	if (proxy_connect(account, account->proto_opt[USEROPT_AUTH][0] ?
				account->proto_opt[USEROPT_AUTH] : FAIM_LOGIN_SERVER,
				account->proto_opt[USEROPT_AUTHPORT][0] ?
				atoi(account->proto_opt[USEROPT_AUTHPORT]) : FAIM_LOGIN_PORT,
				oscar_login_connect, gc) < 0) {
		hide_login_progress(gc, _("Couldn't connect to host"));
		signoff(gc);
		return;
	}
	aim_request_login(sess, conn, gc->username);
}

static void oscar_close(struct gaim_connection *gc) {
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;

	while (od->oscar_chats) {
		struct chat_connection *n = od->oscar_chats->data;
		if (n->inpa > 0)
			gaim_input_remove(n->inpa);
		g_free(n->name);
		g_free(n->show);
		od->oscar_chats = g_slist_remove(od->oscar_chats, n);
		g_free(n);
	}
	while (od->direct_ims) {
		struct direct_im *n = od->direct_ims->data;
		if (n->watcher > 0)
			gaim_input_remove(n->watcher);
		od->direct_ims = g_slist_remove(od->direct_ims, n);
		g_free(n);
	}
/* BBB */
	while (od->file_transfers) {
		struct gaim_xfer *xfer;
		xfer = (struct gaim_xfer *)od->file_transfers->data;
		gaim_xfer_destroy(xfer);
	}
	while (od->requesticon) {
		char *sn = od->requesticon->data;
		od->requesticon = g_slist_remove(od->requesticon, sn);
		free(sn);
	}
	g_hash_table_destroy(od->buddyinfo);
	while (od->evilhack) {
		g_free(od->evilhack->data);
		od->evilhack = g_slist_remove(od->evilhack, od->evilhack->data);
	}
	while (od->create_rooms) {
		struct create_room *cr = od->create_rooms->data;
		g_free(cr->name);
		od->create_rooms = g_slist_remove(od->create_rooms, cr);
		g_free(cr);
	}
	if (od->email)
		g_free(od->email);
	if (od->newp)
		g_free(od->newp);
	if (od->oldp)
		g_free(od->oldp);
	if (gc->inpa > 0)
		gaim_input_remove(gc->inpa);
	if (od->cnpa > 0)
		gaim_input_remove(od->cnpa);
	if (od->paspa > 0)
		gaim_input_remove(od->paspa);
	if (od->emlpa > 0)
		gaim_input_remove(od->emlpa);
	if (od->icopa > 0)
		gaim_input_remove(od->icopa);
	aim_session_kill(od->sess);
	g_free(od->sess);
	od->sess = NULL;
	g_free(gc->proto_data);
	gc->proto_data = NULL;
	debug_printf("Signed off.\n");
}

static void oscar_bos_connect(gpointer data, gint source, GaimInputCondition cond) {
	struct gaim_connection *gc = data;
	struct oscar_data *od;
	aim_session_t *sess;
	aim_conn_t *bosconn;

	if (!g_slist_find(connections, gc)) {
		close(source);
		return;
	}

	od = gc->proto_data;
	sess = od->sess;
	bosconn = od->conn;	
	bosconn->fd = source;

	if (source < 0) {
		hide_login_progress(gc, _("Could Not Connect"));
		signoff(gc);
		return;
	}

	aim_conn_completeconnect(sess, bosconn);
	gc->inpa = gaim_input_add(bosconn->fd, GAIM_INPUT_READ, oscar_callback, bosconn);
	set_login_progress(gc, 4, _("Connection established, cookie sent"));
}

/* BBB */
/*
 * This little area in oscar.c is the nexus of file transfer code, 
 * so I wrote a little explanation of what happens.  I am such a 
 * ninja.
 *
 * The series of events for a file send is:
 *  -Create xfer and call gaim_xfer_request (this happens in oscar_ask_sendfile)
 *  -User chooses a file and oscar_xfer_init is called.  It establishs a 
 *   listening socket, then asks the remote user to connect to us (and 
 *   gives them the file name, port, IP, etc.)
 *  -They connect to us and we send them an AIM_CB_OFT_PROMPT (this happens 
 *   in oscar_sendfile_estblsh)
 *  -They send us an AIM_CB_OFT_ACK and then we start sending data
 *  -When we finish, they send us an AIM_CB_OFT_DONE and they close the 
 *   connection.
 *  -We get drunk because file transfer kicks ass.
 *
 * The series of events for a file receive is:
 *  -Create xfer and call gaim_xfer request (this happens in incomingim_chan2)
 *  -Gaim user selects file to name and location to save file to and 
 *   oscar_xfer_init is called
 *  -It connects to the remote user using the IP they gave us earlier
 *  -After connecting, they send us an AIM_CB_OFT_PROMPT.  In reply, we send 
 *   them an AIM_CB_OFT_ACK.
 *  -They begin to send us lots of raw data.
 *  -When they finish sending data we send an AIM_CB_OFT_DONE and then close 
 *   the connectionn.
 */
static void oscar_sendfile_connected(gpointer data, gint source, GaimInputCondition condition);

/* XXX - This function is pretty ugly */
static void
oscar_xfer_init(struct gaim_xfer *xfer)
{
	struct gaim_connection *gc;
	struct oscar_data *od;
	struct oscar_xfer_data *xfer_data;

	debug_printf("in oscar_xfer_init\n");
	if (!(xfer_data = xfer->data))
		return;
	if (!(gc = xfer_data->gc))
		return;
	if (!(od = gc->proto_data))
		return;

	if (gaim_xfer_get_type(xfer) == GAIM_XFER_SEND) {
		int i;
		char ip[4];
		gchar **ipsplit;

		if (xfer->local_ip) {
			ipsplit = g_strsplit(xfer->local_ip, ".", 4);
			for (i=0; ipsplit[i]; i++)
				ip[i] = atoi(ipsplit[i]);
			g_strfreev(ipsplit);
		} else {
			memset(ip, 0x00, 4);
		}

		xfer->filename = g_path_get_basename(xfer->local_filename);
		xfer_data->checksum = aim_oft_checksum_file(xfer->local_filename);

		/*
		 * First try the port specified earlier (5190).  If that fails, try a 
		 * few random ports.  Maybe we need a way to tell libfaim to listen 
		 * for multiple connections on one listener socket.
		 */
		xfer_data->conn = aim_sendfile_listen(od->sess, xfer_data->cookie, ip, xfer->local_port);
		for (i=0; (i<5 && !xfer_data->conn); i++) {
			xfer->local_port = (rand() % (65535-1024)) + 1024;
			xfer_data->conn = aim_sendfile_listen(od->sess, xfer_data->cookie, ip, xfer->local_port);
		}
		debug_printf("port is %d, ip is %hhd.%hhd.%hhd.%hhd\n", xfer->local_port, ip[0], ip[1], ip[2], ip[3]);
		if (xfer_data->conn) {
			xfer->watcher = gaim_input_add(xfer_data->conn->fd, GAIM_INPUT_READ, oscar_callback, xfer_data->conn);
			aim_im_sendch2_sendfile_ask(od->sess, xfer_data->cookie, xfer->who, ip, xfer->local_port, xfer->filename, 1, xfer->size);
			aim_conn_addhandler(od->sess, xfer_data->conn, AIM_CB_FAM_OFT, AIM_CB_OFT_ESTABLISHED, oscar_sendfile_estblsh, 0);
		} else {
			do_error_dialog(_("File Transfer Aborted"), _("Unable to establish listener socket."), GAIM_ERROR);
			/* XXX - The below line causes a crash because the transfer is canceled before the "Ok" callback on the file selection thing exists, I think */
			/* gaim_xfer_cancel_remote(xfer); */
		}
	} else if (gaim_xfer_get_type(xfer) == GAIM_XFER_RECEIVE) {
		xfer_data->conn = aim_newconn(od->sess, AIM_CONN_TYPE_RENDEZVOUS, NULL);
		if (xfer_data->conn) {
			xfer_data->conn->subtype = AIM_CONN_SUBTYPE_OFT_SENDFILE;
			aim_conn_addhandler(od->sess, xfer_data->conn, AIM_CB_FAM_OFT, AIM_CB_OFT_PROMPT, oscar_sendfile_prompt, 0);
			xfer_data->conn->fd = xfer->fd = proxy_connect(gc->account, xfer->remote_ip, xfer->remote_port, oscar_sendfile_connected, xfer);
			if (xfer->fd == -1) {
				do_error_dialog(_("File Transfer Aborted"), _("Unable to establish file descriptor."), GAIM_ERROR);
				/* gaim_xfer_cancel_remote(xfer); */
			}
		} else {
			do_error_dialog(_("File Transfer Aborted"), _("Unable to create new connection."), GAIM_ERROR);
			/* gaim_xfer_cancel_remote(xfer); */
			/* Try a different port? Ask them to connect to us? */
		}

	}
}

static void
oscar_xfer_start(struct gaim_xfer *xfer)
{
/*	struct gaim_connection *gc;
	struct oscar_data *od;
	struct oscar_xfer_data *xfer_data;

	if (!(xfer_data = xfer->data))
		return;
	if (!(gc = xfer_data->gc))
		return;
	if (!(od = gc->proto_data))
		return;

	od = xfer_data->od;
*/
	debug_printf("AAA - in oscar_xfer_start\n");

	/* I'm pretty sure we don't need to do jack here.  Nor Jill. */
}

static void
oscar_xfer_end(struct gaim_xfer *xfer)
{
	struct gaim_connection *gc;
	struct oscar_data *od;
	struct oscar_xfer_data *xfer_data;

	debug_printf("AAA - in oscar_xfer_end\n");
	if (!(xfer_data = xfer->data))
		return;

	if (gaim_xfer_get_type(xfer) == GAIM_XFER_RECEIVE)
		aim_oft_sendheader(xfer_data->conn->sessv, xfer_data->conn, AIM_CB_OFT_DONE, xfer_data->cookie, xfer->filename, 1, 1, xfer->size, xfer->size, xfer_data->modtime, xfer_data->checksum, 0x02, xfer->size, xfer_data->checksum);

	g_free(xfer_data->clientip);
	g_free(xfer_data->clientip2);

	if ((gc = xfer_data->gc)) {
		if ((od = gc->proto_data))
			od->file_transfers = g_slist_remove(od->file_transfers, xfer);
	}

	g_free(xfer_data);
	xfer->data = NULL;
}

static void
oscar_xfer_cancel_send(struct gaim_xfer *xfer)
{
	struct gaim_connection *gc;
	struct oscar_data *od;
	struct oscar_xfer_data *xfer_data;
	aim_conn_t *conn;

	debug_printf("AAA - in oscar_xfer_cancel_send\n");
	if (!(xfer_data = xfer->data))
		return;

	if ((conn = xfer_data->conn)) {
		aim_session_t *sess;
		if ((sess = conn->sessv))
			if (xfer_data->cookie && xfer->who)
				aim_im_sendch2_sendfile_cancel(sess, xfer_data->cookie, xfer->who, AIM_CAPS_SENDFILE);
	}

	g_free(xfer_data->clientip);
	g_free(xfer_data->clientip2);

	if ((gc = xfer_data->gc))
		if ((od = gc->proto_data))
			od->file_transfers = g_slist_remove(od->file_transfers, xfer);

	g_free(xfer_data);
	xfer->data = NULL;
}

static void
oscar_xfer_cancel_recv(struct gaim_xfer *xfer)
{
	struct gaim_connection *gc;
	struct oscar_data *od;
	struct oscar_xfer_data *xfer_data;
	aim_conn_t *conn;

	debug_printf("AAA - in oscar_xfer_cancel_recv\n");
	if (!(xfer_data = xfer->data))
		return;

	if ((conn = xfer_data->conn)) {
		aim_session_t *sess;
		if ((sess = conn->sessv))
			if (xfer_data->cookie && xfer->who)
				aim_im_sendch2_sendfile_cancel(sess, xfer_data->cookie, xfer->who, AIM_CAPS_SENDFILE);
	}

	g_free(xfer_data->clientip);
	g_free(xfer_data->clientip2);

	if ((gc = xfer_data->gc))
		if ((od = gc->proto_data))
			od->file_transfers = g_slist_remove(od->file_transfers, xfer);

	g_free(xfer_data);
	xfer->data = NULL;
}

static void
oscar_xfer_ack(struct gaim_xfer *xfer, const char *buffer, size_t size)
{
	struct oscar_xfer_data *xfer_data;

	if (!(xfer_data = xfer->data))
		return;

	if (gaim_xfer_get_type(xfer) == GAIM_XFER_SEND) {
		/*
		 * If we're done sending, intercept the socket from the core ft code
		 * and wait for the other guy to send the "done" OFT packet.
		 */
		if (gaim_xfer_get_bytes_remaining(xfer) <= 0) {
			gaim_input_remove(xfer->watcher);
			xfer->watcher = gaim_input_add(xfer->fd, GAIM_INPUT_READ, oscar_callback, xfer_data->conn);
			xfer->fd = 0;
			gaim_xfer_set_completed(xfer, TRUE);
		}
	} else if (gaim_xfer_get_type(xfer) == GAIM_XFER_RECEIVE) {
		/* Update our rolling checksum */
		/* xfer_data->checksum = aim_oft_checksum_chunk(buffer, size, xfer_data->checksum); */
	}
}

static struct gaim_xfer *
oscar_find_xfer_by_cookie(GSList *fts, const char *ck)
{
	struct gaim_xfer *xfer;
	struct oscar_xfer_data *data;

	while (fts) {
		xfer = fts->data;
		data = xfer->data;

		if (data && !strcmp(data->cookie, ck))
			return xfer;

		fts = g_slist_next(fts);
	}

	return NULL;
}

static struct gaim_xfer *
oscar_find_xfer_by_conn(GSList *fts, aim_conn_t *conn)
{
	struct gaim_xfer *xfer;
	struct oscar_xfer_data *data;

	while (fts) {
		xfer = fts->data;
		data = xfer->data;

		if (data && (conn == data->conn))
			return xfer;

		fts = g_slist_next(fts);
	}

	return NULL;
}

static void oscar_ask_sendfile(struct gaim_connection *gc, char *destsn) {
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;

	/* You want to send a file to someone else, you're so generous */
	struct gaim_xfer *xfer;
	struct oscar_xfer_data *xfer_data;

	/* Create the oscar-specific data */
	xfer_data = g_malloc0(sizeof(struct oscar_xfer_data));
	xfer_data->gc = gc;

	/* Build the file transfer handle */
	xfer = gaim_xfer_new(gc->account, GAIM_XFER_SEND, destsn);
	xfer_data->xfer = xfer;
	xfer->data = xfer_data;

	/* Set the info about the incoming file */
	if (od && od->sess) {
		aim_conn_t *conn;
		if ((conn = aim_conn_findbygroup(od->sess, 0x0004)))
			xfer->local_ip = gaim_getip_from_fd(conn->fd);
	}
	xfer->local_port = 5190;

	 /* Setup our I/O op functions */
	gaim_xfer_set_init_fnc(xfer, oscar_xfer_init);
	gaim_xfer_set_start_fnc(xfer, oscar_xfer_start);
	gaim_xfer_set_end_fnc(xfer, oscar_xfer_end);
	gaim_xfer_set_cancel_send_fnc(xfer, oscar_xfer_cancel_send);
	gaim_xfer_set_cancel_recv_fnc(xfer, oscar_xfer_cancel_recv);
	gaim_xfer_set_ack_fnc(xfer, oscar_xfer_ack);

	/* Keep track of this transfer for later */
	od->file_transfers = g_slist_append(od->file_transfers, xfer);

	/* Now perform the request */
	gaim_xfer_request(xfer);
}

static int gaim_parse_auth_resp(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	struct aim_authresp_info *info;
	int i, rc;
	char *host; int port;
	struct gaim_account *account;
	aim_conn_t *bosconn;

	struct gaim_connection *gc = sess->aux_data;
        struct oscar_data *od = gc->proto_data;
	account = gc->account;
	port = account->proto_opt[USEROPT_AUTHPORT][0] ?
		atoi(account->proto_opt[USEROPT_AUTHPORT]) : FAIM_LOGIN_PORT,

	va_start(ap, fr);
	info = va_arg(ap, struct aim_authresp_info *);
	va_end(ap);

	debug_printf("inside auth_resp (Screen name: %s)\n", info->sn);

	if (info->errorcode || !info->bosip || !info->cookielen || !info->cookie) {
		char buf[256];
		switch (info->errorcode) {
		case 0x05:
			/* Incorrect nick/password */
			hide_login_progress(gc, _("Incorrect nickname or password."));
			break;
		case 0x11:
			/* Suspended account */
			hide_login_progress(gc, _("Your account is currently suspended."));
			break;
		case 0x14:
			/* service temporarily unavailable */
			hide_login_progress(gc, _("The AOL Instant Messenger service is temporarily unavailable."));
			break;
		case 0x18:
			/* connecting too frequently */
			hide_login_progress(gc, _("You have been connecting and disconnecting too frequently. Wait ten minutes and try again. If you continue to try, you will need to wait even longer."));
			break;
		case 0x1c:
			/* client too old */
			g_snprintf(buf, sizeof(buf), _("The client version you are using is too old. Please upgrade at %s"), WEBSITE);
			hide_login_progress(gc, buf);
			break;
		default:
			hide_login_progress(gc, _("Authentication Failed"));
			break;
		}
		debug_printf("Login Error Code 0x%04hx\n", info->errorcode);
		debug_printf("Error URL: %s\n", info->errorurl);
		od->killme = TRUE;
		return 1;
	}


	debug_printf("Reg status: %hu\n", info->regstatus);
	if (info->email) {
		debug_printf("Email: %s\n", info->email);
	} else {
		debug_printf("Email is NULL\n");
	}
	debug_printf("BOSIP: %s\n", info->bosip);
	debug_printf("Closing auth connection...\n");
	aim_conn_kill(sess, &fr->conn);

	bosconn = aim_newconn(sess, AIM_CONN_TYPE_BOS, NULL);
	if (bosconn == NULL) {
		hide_login_progress(gc, _("Internal Error"));
		od->killme = TRUE;
		return 0;
	}

	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNERR, gaim_connerr, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNINITDONE, conninitdone_bos, 0);
	aim_conn_addhandler(sess, bosconn, 0x0009, 0x0003, gaim_bosrights, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_ACK, AIM_CB_ACK_ACK, NULL, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, AIM_CB_GEN_REDIRECT, gaim_handle_redirect, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_LOC, AIM_CB_LOC_RIGHTSINFO, gaim_parse_locaterights, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_BUD, AIM_CB_BUD_RIGHTSINFO, gaim_parse_buddyrights, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_BUD, AIM_CB_BUD_ONCOMING, gaim_parse_oncoming, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_BUD, AIM_CB_BUD_OFFGOING, gaim_parse_offgoing, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, AIM_CB_MSG_INCOMING, gaim_parse_incoming_im, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_LOC, AIM_CB_LOC_ERROR, gaim_parse_locerr, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, AIM_CB_MSG_MISSEDCALL, gaim_parse_misses, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, AIM_CB_MSG_CLIENTAUTORESP, gaim_parse_clientauto, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, AIM_CB_GEN_RATECHANGE, gaim_parse_ratechange, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, AIM_CB_GEN_EVIL, gaim_parse_evilnotify, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_LOK, AIM_CB_LOK_ERROR, gaim_parse_searcherror, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_LOK, 0x0003, gaim_parse_searchreply, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, AIM_CB_MSG_ERROR, gaim_parse_msgerr, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, AIM_CB_MSG_MTN, gaim_parse_mtn, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_LOC, AIM_CB_LOC_USERINFO, gaim_parse_user_info, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_MSG, AIM_CB_MSG_ACK, gaim_parse_msgack, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_GEN, AIM_CB_GEN_MOTD, gaim_parse_motd, 0);
	aim_conn_addhandler(sess, bosconn, 0x0004, 0x0005, gaim_icbm_param_info, 0);
	aim_conn_addhandler(sess, bosconn, 0x0001, 0x0001, gaim_parse_genericerr, 0);
	aim_conn_addhandler(sess, bosconn, 0x0003, 0x0001, gaim_parse_genericerr, 0);
	aim_conn_addhandler(sess, bosconn, 0x0009, 0x0001, gaim_parse_genericerr, 0);
	aim_conn_addhandler(sess, bosconn, 0x0001, 0x001f, gaim_memrequest, 0);
	aim_conn_addhandler(sess, bosconn, 0x0001, 0x000f, gaim_selfinfo, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_ICQ, AIM_CB_ICQ_OFFLINEMSG, gaim_offlinemsg, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_ICQ, AIM_CB_ICQ_OFFLINEMSGCOMPLETE, gaim_offlinemsgdone, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_POP, 0x0002, gaim_popup, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_ICQ, AIM_CB_ICQ_ALIAS, gaim_icqalias, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_ICQ, AIM_CB_ICQ_INFO, gaim_icqinfo, 0);
#ifndef NOSSI
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SSI, AIM_CB_SSI_ERROR, gaim_ssi_parseerr, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SSI, AIM_CB_SSI_RIGHTSINFO, gaim_ssi_parserights, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SSI, AIM_CB_SSI_LIST, gaim_ssi_parselist, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SSI, AIM_CB_SSI_NOLIST, gaim_ssi_parselist, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SSI, AIM_CB_SSI_SRVACK, gaim_ssi_parseack, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SSI, AIM_CB_SSI_RECVAUTH, gaim_ssi_authgiven, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SSI, AIM_CB_SSI_RECVAUTHREQ, gaim_ssi_authrequest, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SSI, AIM_CB_SSI_RECVAUTHREP, gaim_ssi_authreply, 0);
	aim_conn_addhandler(sess, bosconn, AIM_CB_FAM_SSI, AIM_CB_SSI_ADDED, gaim_ssi_gotadded, 0);
#endif

	((struct oscar_data *)gc->proto_data)->conn = bosconn;
	for (i = 0; i < (int)strlen(info->bosip); i++) {
		if (info->bosip[i] == ':') {
			port = atoi(&(info->bosip[i+1]));
			break;
		}
	}
	host = g_strndup(info->bosip, i);
	bosconn->status |= AIM_CONN_STATUS_INPROGRESS;
	rc = proxy_connect(gc->account, host, port, oscar_bos_connect, gc);
	g_free(host);
	if (rc < 0) {
		hide_login_progress(gc, _("Could Not Connect"));
		od->killme = TRUE;
		return 0;
	}
	aim_sendcookie(sess, bosconn, info->cookielen, info->cookie);
	gaim_input_remove(gc->inpa);

	return 1;
}

struct pieceofcrap {
	struct gaim_connection *gc;
	unsigned long offset;
	unsigned long len;
	char *modname;
	int fd;
	aim_conn_t *conn;
	unsigned int inpa;
};

static void damn_you(gpointer data, gint source, GaimInputCondition c)
{
	struct pieceofcrap *pos = data;
	struct oscar_data *od = pos->gc->proto_data;
	char in = '\0';
	int x = 0;
	unsigned char m[17];

	while (read(pos->fd, &in, 1) == 1) {
		if (in == '\n')
			x++;
		else if (in != '\r')
			x = 0;
		if (x == 2)
			break;
		in = '\0';
	}
	if (in != '\n') {
		char buf[256];
		g_snprintf(buf, sizeof(buf), _("You may be disconnected shortly.  You may want to use TOC until "
			"this is fixed.  Check %s for updates."), WEBSITE);
		do_error_dialog(_("Gaim was Unable to get a valid AIM login hash."),
				buf, GAIM_WARNING);
		gaim_input_remove(pos->inpa);
		close(pos->fd);
		g_free(pos);
		return;
	}
	read(pos->fd, m, 16);
	m[16] = '\0';
	debug_printf("Sending hash: ");
	for (x = 0; x < 16; x++)
		debug_printf("%02hhx ", (unsigned char)m[x]);
	debug_printf("\n");
	gaim_input_remove(pos->inpa);
	close(pos->fd);
	aim_sendmemblock(od->sess, pos->conn, 0, 16, m, AIM_SENDMEMBLOCK_FLAG_ISHASH);
	g_free(pos);
}

static void straight_to_hell(gpointer data, gint source, GaimInputCondition cond) {
	struct pieceofcrap *pos = data;
	char buf[BUF_LONG];

	pos->fd = source;

	if (source < 0) {
		char buf[256];
		g_snprintf(buf, sizeof(buf), _("You may be disconnected shortly.  You may want to use TOC until "
			"this is fixed.  Check %s for updates."), WEBSITE);
		do_error_dialog(_("Gaim was Unable to get a valid AIM login hash."),
				buf, GAIM_WARNING);
		if (pos->modname)
			g_free(pos->modname);
		g_free(pos);
		return;
	}

	g_snprintf(buf, sizeof(buf), "GET " AIMHASHDATA
			"?offset=%ld&len=%ld&modname=%s HTTP/1.0\n\n",
			pos->offset, pos->len, pos->modname ? pos->modname : "");
	write(pos->fd, buf, strlen(buf));
	if (pos->modname)
		g_free(pos->modname);
	pos->inpa = gaim_input_add(pos->fd, GAIM_INPUT_READ, damn_you, pos);
	return;
}

/* size of icbmui.ocm, the largest module in AIM 3.5 */
#define AIM_MAX_FILE_SIZE 98304

int gaim_memrequest(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	struct pieceofcrap *pos;
	fu32_t offset, len;
	char *modname;

	va_start(ap, fr);
	offset = va_arg(ap, fu32_t);
	len = va_arg(ap, fu32_t);
	modname = va_arg(ap, char *);
	va_end(ap);

	debug_printf("offset: %lu, len: %lu, file: %s\n", offset, len, (modname ? modname : "aim.exe"));
	if (len == 0) {
		debug_printf("len is 0, hashing NULL\n");
		aim_sendmemblock(sess, fr->conn, offset, len, NULL,
				AIM_SENDMEMBLOCK_FLAG_ISREQUEST);
		return 1;
	}
	/* uncomment this when you're convinced it's right. remember, it's been wrong before.
	if (offset > AIM_MAX_FILE_SIZE || len > AIM_MAX_FILE_SIZE) {
		char *buf;
		int i = 8;
		if (modname)
			i += strlen(modname);
		buf = g_malloc(i);
		i = 0;
		if (modname) {
			memcpy(buf, modname, strlen(modname));
			i += strlen(modname);
		}
		buf[i++] = offset & 0xff;
		buf[i++] = (offset >> 8) & 0xff;
		buf[i++] = (offset >> 16) & 0xff;
		buf[i++] = (offset >> 24) & 0xff;
		buf[i++] = len & 0xff;
		buf[i++] = (len >> 8) & 0xff;
		buf[i++] = (len >> 16) & 0xff;
		buf[i++] = (len >> 24) & 0xff;
		debug_printf("len + offset is invalid, hashing request\n");
		aim_sendmemblock(sess, command->conn, offset, i, buf, AIM_SENDMEMBLOCK_FLAG_ISREQUEST);
		g_free(buf);
		return 1;
	}
	*/

	pos = g_new0(struct pieceofcrap, 1);
	pos->gc = sess->aux_data;
	pos->conn = fr->conn;

	pos->offset = offset;
	pos->len = len;
	pos->modname = modname ? g_strdup(modname) : NULL;

	if (proxy_connect(pos->gc->account, "gaim.sourceforge.net", 80, straight_to_hell, pos) != 0) {
		char buf[256];
		if (pos->modname)
			g_free(pos->modname);
		g_free(pos);
		g_snprintf(buf, sizeof(buf), _("You may be disconnected shortly.  You may want to use TOC until "
			"this is fixed.  Check %s for updates."), WEBSITE);
		do_error_dialog(_("Gaim was Unable to get a valid login hash."),
				 buf, GAIM_WARNING);
	}

	return 1;
}

static int gaim_parse_login(aim_session_t *sess, aim_frame_t *fr, ...) {
	char *key;
	va_list ap;
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = gc->proto_data;

	va_start(ap, fr);
	key = va_arg(ap, char *);
	va_end(ap);

	if (od->icq) {
		struct client_info_s info = CLIENTINFO_ICQ_KNOWNGOOD;
		aim_send_login(sess, fr->conn, gc->username, gc->password, &info, key);
	} else {
#if 0
		struct client_info_s info = {"gaim", 4, 1, 2010, "us", "en", 0x0004, 0x0000, 0x04b};
#endif
		struct client_info_s info = CLIENTINFO_AIM_KNOWNGOOD;
		aim_send_login(sess, fr->conn, gc->username, gc->password, &info, key);
	}

	return 1;
}

static int conninitdone_chat(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct gaim_connection *gc = sess->aux_data;
	struct chat_connection *chatcon;
	static int id = 1;

	aim_conn_addhandler(sess, fr->conn, 0x000e, 0x0001, gaim_parse_genericerr, 0);
	aim_conn_addhandler(sess, fr->conn, AIM_CB_FAM_CHT, AIM_CB_CHT_USERJOIN, gaim_chat_join, 0);
	aim_conn_addhandler(sess, fr->conn, AIM_CB_FAM_CHT, AIM_CB_CHT_USERLEAVE, gaim_chat_leave, 0);
	aim_conn_addhandler(sess, fr->conn, AIM_CB_FAM_CHT, AIM_CB_CHT_ROOMINFOUPDATE, gaim_chat_info_update, 0);
	aim_conn_addhandler(sess, fr->conn, AIM_CB_FAM_CHT, AIM_CB_CHT_INCOMINGMSG, gaim_chat_incoming_msg, 0);

	aim_clientready(sess, fr->conn);

	chatcon = find_oscar_chat_by_conn(gc, fr->conn);
	chatcon->id = id;
	chatcon->cnv = serv_got_joined_chat(gc, id++, chatcon->show);

	return 1;
}

static int conninitdone_chatnav(aim_session_t *sess, aim_frame_t *fr, ...) {

	aim_conn_addhandler(sess, fr->conn, 0x000d, 0x0001, gaim_parse_genericerr, 0);
	aim_conn_addhandler(sess, fr->conn, AIM_CB_FAM_CTN, AIM_CB_CTN_INFO, gaim_chatnav_info, 0);

	aim_clientready(sess, fr->conn);

	aim_chatnav_reqrights(sess, fr->conn);

	return 1;
}

static int conninitdone_email(aim_session_t *sess, aim_frame_t *fr, ...) {

	aim_conn_addhandler(sess, fr->conn, 0x0018, 0x0001, gaim_parse_genericerr, 0);
	aim_conn_addhandler(sess, fr->conn, AIM_CB_FAM_EML, AIM_CB_EML_MAILSTATUS, gaim_email_parseupdate, 0);

	aim_email_sendcookies(sess, fr->conn);
	aim_email_activate(sess, fr->conn);
	aim_clientready(sess, fr->conn);

	return 1;
}

static int conninitdone_icon(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = gc->proto_data;

	aim_conn_addhandler(sess, fr->conn, 0x0018, 0x0001, gaim_parse_genericerr, 0);
	aim_conn_addhandler(sess, fr->conn, AIM_CB_FAM_ICO, AIM_CB_ICO_ERROR, gaim_icon_error, 0);
	aim_conn_addhandler(sess, fr->conn, AIM_CB_FAM_ICO, AIM_CB_ICO_RESPONSE, gaim_icon_parseicon, 0);

	aim_clientready(sess, fr->conn);

	od->iconconnecting = FALSE;

	if (od->icontimer)
		g_source_remove(od->icontimer);
	od->icontimer = g_timeout_add(100, gaim_icon_timerfunc, gc);

	return 1;
}

static void oscar_chatnav_connect(gpointer data, gint source, GaimInputCondition cond) {
	struct gaim_connection *gc = data;
	struct oscar_data *od;
	aim_session_t *sess;
	aim_conn_t *tstconn;

	if (!g_slist_find(connections, gc)) {
		close(source);
		return;
	}

	od = gc->proto_data;
	sess = od->sess;
	tstconn = aim_getconn_type_all(sess, AIM_CONN_TYPE_CHATNAV);
	tstconn->fd = source;

	if (source < 0) {
		aim_conn_kill(sess, &tstconn);
		debug_printf("unable to connect to chatnav server\n");
		return;
	}

	aim_conn_completeconnect(sess, tstconn);
	od->cnpa = gaim_input_add(tstconn->fd, GAIM_INPUT_READ, oscar_callback, tstconn);
	debug_printf("chatnav: connected\n");
}

static void oscar_auth_connect(gpointer data, gint source, GaimInputCondition cond)
{
	struct gaim_connection *gc = data;
	struct oscar_data *od;
	aim_session_t *sess;
	aim_conn_t *tstconn;

	if (!g_slist_find(connections, gc)) {
		close(source);
		return;
	}

	od = gc->proto_data;
	sess = od->sess;
	tstconn = aim_getconn_type_all(sess, AIM_CONN_TYPE_AUTH);
	tstconn->fd = source;

	if (source < 0) {
		aim_conn_kill(sess, &tstconn);
		debug_printf("unable to connect to authorizer\n");
		return;
	}

	aim_conn_completeconnect(sess, tstconn);
	od->paspa = gaim_input_add(tstconn->fd, GAIM_INPUT_READ, oscar_callback, tstconn);
	debug_printf("chatnav: connected\n");
}

static void oscar_chat_connect(gpointer data, gint source, GaimInputCondition cond)
{
	struct chat_connection *ccon = data;
	struct gaim_connection *gc = ccon->gc;
	struct oscar_data *od;
	aim_session_t *sess;
	aim_conn_t *tstconn;

	if (!g_slist_find(connections, gc)) {
		close(source);
		g_free(ccon->show);
		g_free(ccon->name);
		g_free(ccon);
		return;
	}

	od = gc->proto_data;
	sess = od->sess;
	tstconn = ccon->conn;
	tstconn->fd = source;

	if (source < 0) {
		aim_conn_kill(sess, &tstconn);
		g_free(ccon->show);
		g_free(ccon->name);
		g_free(ccon);
		return;
	}

	aim_conn_completeconnect(sess, ccon->conn);
	ccon->inpa = gaim_input_add(tstconn->fd, GAIM_INPUT_READ, oscar_callback, tstconn);
	od->oscar_chats = g_slist_append(od->oscar_chats, ccon);
}

static void oscar_email_connect(gpointer data, gint source, GaimInputCondition cond) {
	struct gaim_connection *gc = data;
	struct oscar_data *od;
	aim_session_t *sess;
	aim_conn_t *tstconn;

	if (!g_slist_find(connections, gc)) {
		close(source);
		return;
	}

	od = gc->proto_data;
	sess = od->sess;
	tstconn = aim_getconn_type_all(sess, AIM_CONN_TYPE_EMAIL);
	tstconn->fd = source;

	if (source < 0) {
		aim_conn_kill(sess, &tstconn);
		debug_printf("unable to connect to email server\n");
		return;
	}

	aim_conn_completeconnect(sess, tstconn);
	od->emlpa = gaim_input_add(tstconn->fd, GAIM_INPUT_READ, oscar_callback, tstconn);
	debug_printf("email: connected\n");
}

static void oscar_icon_connect(gpointer data, gint source, GaimInputCondition cond) {
	struct gaim_connection *gc = data;
	struct oscar_data *od;
	aim_session_t *sess;
	aim_conn_t *tstconn;

	if (!g_slist_find(connections, gc)) {
		close(source);
		return;
	}

	od = gc->proto_data;
	sess = od->sess;
	tstconn = aim_getconn_type_all(sess, AIM_CONN_TYPE_ICON);
	tstconn->fd = source;

	if (source < 0) {
		aim_conn_kill(sess, &tstconn);
		debug_printf("unable to connect to icon server\n");
		return;
	}

	aim_conn_completeconnect(sess, tstconn);
	od->icopa = gaim_input_add(tstconn->fd, GAIM_INPUT_READ, oscar_callback, tstconn);
	debug_printf("icon: connected\n");
}

/* Hrmph. I don't know how to make this look better. --mid */
static int gaim_handle_redirect(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct gaim_connection *gc = sess->aux_data;
	struct gaim_account *account = gc->account;
	aim_conn_t *tstconn;
	int i;
	char *host;
	int port;
	va_list ap;
	struct aim_redirect_data *redir;

	port = account->proto_opt[USEROPT_AUTHPORT][0] ?
		atoi(account->proto_opt[USEROPT_AUTHPORT]) : FAIM_LOGIN_PORT,

	va_start(ap, fr);
	redir = va_arg(ap, struct aim_redirect_data *);
	va_end(ap);

	for (i = 0; i < (int)strlen(redir->ip); i++) {
		if (redir->ip[i] == ':') {
			port = atoi(&(redir->ip[i+1]));
			break;
		}
	}
	host = g_strndup(redir->ip, i);

	switch(redir->group) {
	case 0x7: /* Authorizer */
		debug_printf("Reconnecting with authorizor...\n");
		tstconn = aim_newconn(sess, AIM_CONN_TYPE_AUTH, NULL);
		if (tstconn == NULL) {
			debug_printf("unable to reconnect with authorizer\n");
			g_free(host);
			return 1;
		}
		aim_conn_addhandler(sess, tstconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNERR, gaim_connerr, 0);
		aim_conn_addhandler(sess, tstconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNINITDONE, conninitdone_admin, 0);
		aim_conn_addhandler(sess, tstconn, 0x0007, 0x0003, gaim_info_change, 0);
		aim_conn_addhandler(sess, tstconn, 0x0007, 0x0005, gaim_info_change, 0);
		aim_conn_addhandler(sess, tstconn, 0x0007, 0x0007, gaim_account_confirm, 0);

		tstconn->status |= AIM_CONN_STATUS_INPROGRESS;
		if (proxy_connect(account, host, port, oscar_auth_connect, gc) != 0) {
			aim_conn_kill(sess, &tstconn);
			debug_printf("unable to reconnect with authorizer\n");
			g_free(host);
			return 1;
		}
		aim_sendcookie(sess, tstconn, redir->cookielen, redir->cookie);
	break;

	case 0xd: /* ChatNav */
		tstconn = aim_newconn(sess, AIM_CONN_TYPE_CHATNAV, NULL);
		if (tstconn == NULL) {
			debug_printf("unable to connect to chatnav server\n");
			g_free(host);
			return 1;
		}
		aim_conn_addhandler(sess, tstconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNERR, gaim_connerr, 0);
		aim_conn_addhandler(sess, tstconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNINITDONE, conninitdone_chatnav, 0);

		tstconn->status |= AIM_CONN_STATUS_INPROGRESS;
		if (proxy_connect(account, host, port, oscar_chatnav_connect, gc) != 0) {
			aim_conn_kill(sess, &tstconn);
			debug_printf("unable to connect to chatnav server\n");
			g_free(host);
			return 1;
		}
		aim_sendcookie(sess, tstconn, redir->cookielen, redir->cookie);
	break;

	case 0xe: { /* Chat */
		struct chat_connection *ccon;

		tstconn = aim_newconn(sess, AIM_CONN_TYPE_CHAT, NULL);
		if (tstconn == NULL) {
			debug_printf("unable to connect to chat server\n");
			g_free(host);
			return 1;
		}

		aim_conn_addhandler(sess, tstconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNERR, gaim_connerr, 0);
		aim_conn_addhandler(sess, tstconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNINITDONE, conninitdone_chat, 0);

		ccon = g_new0(struct chat_connection, 1);
		ccon->conn = tstconn;
		ccon->gc = gc;
		ccon->fd = -1;
		ccon->name = g_strdup(redir->chat.room);
		ccon->exchange = redir->chat.exchange;
		ccon->instance = redir->chat.instance;
		ccon->show = extract_name(redir->chat.room);

		ccon->conn->status |= AIM_CONN_STATUS_INPROGRESS;
		if (proxy_connect(account, host, port, oscar_chat_connect, ccon) != 0) {
			aim_conn_kill(sess, &tstconn);
			debug_printf("unable to connect to chat server\n");
			g_free(host);
			g_free(ccon->show);
			g_free(ccon->name);
			g_free(ccon);
			return 1;
		}
		aim_sendcookie(sess, tstconn, redir->cookielen, redir->cookie);
		debug_printf("Connected to chat room %s exchange %hu\n", ccon->name, ccon->exchange);
	} break;

	case 0x0010: { /* icon */
		if (!(tstconn = aim_newconn(sess, AIM_CONN_TYPE_ICON, NULL))) {
			debug_printf("unable to connect to icon server\n");
			g_free(host);
			return 1;
		}
		aim_conn_addhandler(sess, tstconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNERR, gaim_connerr, 0);
		aim_conn_addhandler(sess, tstconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNINITDONE, conninitdone_icon, 0);

		tstconn->status |= AIM_CONN_STATUS_INPROGRESS;
		if (proxy_connect(account, host, port, oscar_icon_connect, gc) != 0) {
			aim_conn_kill(sess, &tstconn);
			debug_printf("unable to connect to icon server\n");
			g_free(host);
			return 1;
		}
		aim_sendcookie(sess, tstconn, redir->cookielen, redir->cookie);
	} break;

	case 0x0018: { /* email */
		if (!(tstconn = aim_newconn(sess, AIM_CONN_TYPE_EMAIL, NULL))) {
			debug_printf("unable to connect to email server\n");
			g_free(host);
			return 1;
		}
		aim_conn_addhandler(sess, tstconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNERR, gaim_connerr, 0);
		aim_conn_addhandler(sess, tstconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNINITDONE, conninitdone_email, 0);

		tstconn->status |= AIM_CONN_STATUS_INPROGRESS;
		if (proxy_connect(account, host, port, oscar_email_connect, gc) != 0) {
			aim_conn_kill(sess, &tstconn);
			debug_printf("unable to connect to email server\n");
			g_free(host);
			return 1;
		}
		aim_sendcookie(sess, tstconn, redir->cookielen, redir->cookie);
	} break;

	default: /* huh? */
		debug_printf("got redirect for unknown service 0x%04hx\n", redir->group);
		break;
	}

	g_free(host);
	return 1;
}

static int gaim_parse_oncoming(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = gc->proto_data;
	struct buddyinfo *bi;
	time_t time_idle = 0, signon = 0;
	int type = 0;
	int caps = 0;
	va_list ap;
	aim_userinfo_t *info;

	va_start(ap, fr);
	info = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	if (info->present & AIM_USERINFO_PRESENT_CAPABILITIES)
		caps = info->capabilities;
	if (info->flags & AIM_FLAG_ACTIVEBUDDY)
		type |= UC_AB;

	if (info->present & AIM_USERINFO_PRESENT_FLAGS) {
		if (info->flags & AIM_FLAG_UNCONFIRMED)
			type |= UC_UNCONFIRMED;
		if (info->flags & AIM_FLAG_ADMINISTRATOR)
			type |= UC_ADMIN;
		if (info->flags & AIM_FLAG_AOL)
			type |= UC_AOL;
		if (info->flags & AIM_FLAG_FREE)
			type |= UC_NORMAL;
		if (info->flags & AIM_FLAG_AWAY)
			type |= UC_UNAVAILABLE;
		if (info->flags & AIM_FLAG_WIRELESS)
			type |= UC_WIRELESS;
	}
	if (info->present & AIM_USERINFO_PRESENT_ICQEXTSTATUS) {
		type = (info->icqinfo.status << 16);
		if (!(info->icqinfo.status & AIM_ICQ_STATE_CHAT) &&
		      (info->icqinfo.status != AIM_ICQ_STATE_NORMAL)) {
			type |= UC_UNAVAILABLE;
		}
	}

	if (caps & AIM_CAPS_ICQ)
		caps ^= AIM_CAPS_ICQ;

	if (info->present & AIM_USERINFO_PRESENT_IDLE) {
		time(&time_idle);
		time_idle -= info->idletime*60;
	}

	if (info->present & AIM_USERINFO_PRESENT_SESSIONLEN)
		signon = time(NULL) - info->sessionlen;

	if (!aim_sncmp(gc->username, info->sn))
		g_snprintf(gc->displayname, sizeof(gc->displayname), "%s", info->sn);

	bi = g_hash_table_lookup(od->buddyinfo, normalize(info->sn));
	if (!bi) {
		bi = g_new0(struct buddyinfo, 1);
		g_hash_table_insert(od->buddyinfo, g_strdup(normalize(info->sn)), bi);
	}
	bi->signon = info->onlinesince ? info->onlinesince : (info->sessionlen + time(NULL));
	bi->caps = caps;
	bi->typingnot = FALSE;
	bi->ico_informed = FALSE;

	/* Server stored icon stuff */
	if (info->iconstrlen) {
		od->requesticon = g_slist_append(od->requesticon, strdup(normalize(info->sn)));
		if (od->icontimer)
			g_source_remove(od->icontimer);
		od->icontimer = g_timeout_add(500, gaim_icon_timerfunc, gc);
		memcpy(bi->iconstr, info->iconstr, info->iconstrlen);
		bi->iconstrlen = info->iconstrlen;
	}

	serv_got_update(gc, info->sn, 1, info->warnlevel/10, signon,
			time_idle, type);

	return 1;
}

static int gaim_parse_offgoing(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct gaim_connection *gc = sess->aux_data;
	va_list ap;
	aim_userinfo_t *info;

	va_start(ap, fr);
	info = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	serv_got_update(gc, info->sn, 0, 0, 0, 0, 0);

	return 1;
}

static void cancel_direct_im(struct ask_direct *d) {
	debug_printf("Freeing DirectIM prompts.\n");

	g_free(d->sn);
	g_free(d);
}

static void oscar_odc_callback(gpointer data, gint source, GaimInputCondition condition) {
	struct direct_im *dim = data;
	struct gaim_connection *gc = dim->gc;
	struct oscar_data *od = gc->proto_data;
	struct gaim_conversation *cnv;
	char buf[256];
	struct sockaddr name;
	socklen_t name_len = 1;
	
	if (!g_slist_find(connections, gc)) {
		g_free(dim);
		return;
	}

	if (source < 0) {
		g_free(dim);
		return;
	}

	dim->conn->fd = source;
	aim_conn_completeconnect(od->sess, dim->conn);
	if (!(cnv = gaim_find_conversation(dim->name)))
		cnv = gaim_conversation_new(GAIM_CONV_IM, dim->gc->account, dim->name);

	/* This is the best way to see if we're connected or not */
	if (getpeername(source, &name, &name_len) == 0) {
		g_snprintf(buf, sizeof buf, _("Direct IM with %s established"), dim->name);
		dim->connected = TRUE;
		gaim_conversation_write(cnv, NULL, buf, -1, WFLAG_SYSTEM, time(NULL));
	}
	od->direct_ims = g_slist_append(od->direct_ims, dim);
	
	dim->watcher = gaim_input_add(dim->conn->fd, GAIM_INPUT_READ, oscar_callback, dim->conn);
}

/* BBB */
/*
 * This is called after a remote AIM user has connected to us.  We 
 * want to do some voodoo with the socket file descriptors, add a 
 * callback or two, and then send the AIM_CB_OFT_PROMPT.
 */
static int oscar_sendfile_estblsh(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	struct gaim_xfer *xfer;
	struct oscar_xfer_data *xfer_data;
	va_list ap;
	aim_conn_t *conn, *listenerconn;

	debug_printf("AAA - in oscar_sendfile_estblsh\n");
	va_start(ap, fr);
	conn = va_arg(ap, aim_conn_t *);
	listenerconn = va_arg(ap, aim_conn_t *);
	va_end(ap);

	if (!(xfer = oscar_find_xfer_by_conn(od->file_transfers, listenerconn)))
		return 1;

	if (!(xfer_data = xfer->data))
		return 1;

	/* Stop watching listener conn; watch transfer conn instead */
	gaim_input_remove(xfer->watcher);
	aim_conn_kill(sess, &listenerconn);

	xfer_data->conn = conn;
	xfer->fd = xfer_data->conn->fd;

	aim_conn_addhandler(sess, xfer_data->conn, AIM_CB_FAM_OFT, AIM_CB_OFT_ACK, oscar_sendfile_ack, 0);
	aim_conn_addhandler(sess, xfer_data->conn, AIM_CB_FAM_OFT, AIM_CB_OFT_DONE, oscar_sendfile_done, 0);
	xfer->watcher = gaim_input_add(xfer_data->conn->fd, GAIM_INPUT_READ, oscar_callback, xfer_data->conn);

	/* Inform the other user that we are connected and ready to transfer */
	aim_oft_sendheader(sess, xfer_data->conn, AIM_CB_OFT_PROMPT, NULL, xfer->filename, 0, 1,  xfer->size, xfer->size, time(NULL), xfer_data->checksum, 0x02, 0, 0);

	return 0;
}

/*
 * This is the gaim callback passed to proxy_connect when connecting to another AIM 
 * user in order to transfer a file.
 */
static void oscar_sendfile_connected(gpointer data, gint source, GaimInputCondition condition) {
	struct gaim_connection *gc;
	struct gaim_xfer *xfer;
	struct oscar_xfer_data *xfer_data;

	debug_printf("AAA - in oscar_sendfile_connected\n");
	if (!(xfer = data))
		return;
	if (!(xfer_data = xfer->data))
		return;
	if (!(gc = xfer_data->gc))
		return;
	if (source < 0)
		return;

	xfer->fd = source;
	xfer_data->conn->fd = source;

	aim_conn_completeconnect(xfer_data->conn->sessv, xfer_data->conn);
	xfer->watcher = gaim_input_add(xfer->fd, GAIM_INPUT_READ, oscar_callback, xfer_data->conn);

	/* Inform the other user that we are connected and ready to transfer */
	aim_im_sendch2_sendfile_accept(xfer_data->conn->sessv, xfer_data->cookie, xfer->who, AIM_CAPS_SENDFILE);

	return;
}

/*
 * This is called when a buddy sends us some file info.  This happens when they 
 * are sending a file to you, and you have just established a connection to them.
 * You should send them the exact same info except use the real cookie.  We also 
 * get like totally ready to like, receive the file, kay?
 */
static int oscar_sendfile_prompt(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = gc->proto_data;
	struct gaim_xfer *xfer;
	struct oscar_xfer_data *xfer_data;
	va_list ap;
	aim_conn_t *conn;
	fu8_t *cookie;
	struct aim_fileheader_t *fh;

	debug_printf("AAA - in oscar_sendfile_prompt\n");
	va_start(ap, fr);
	conn = va_arg(ap, aim_conn_t *);
	cookie = va_arg(ap, fu8_t *);
	fh = va_arg(ap, struct aim_fileheader_t *);
	va_end(ap);

	if (!(xfer = oscar_find_xfer_by_conn(od->file_transfers, conn)))
		return 1;

	if (!(xfer_data = xfer->data))
		return 1;

	/* Jot down some data we'll need later */
	xfer_data->modtime = fh->modtime;
	xfer_data->checksum = fh->checksum;

	/* We want to stop listening with a normal thingy */
	gaim_input_remove(xfer->watcher);
	xfer->watcher = 0;

	/* XXX - convert the name from UTF-8 to UCS-2 if necessary, and pass the encoding to the call below */
	aim_oft_sendheader(xfer_data->conn->sessv, xfer_data->conn, AIM_CB_OFT_ACK, xfer_data->cookie, xfer->filename, 0, 1, xfer->size, xfer->size, fh->modtime, fh->checksum, 0x02, 0, 0);
	gaim_xfer_start(xfer, xfer->fd, NULL, 0);

	return 0;
}

/*
 * We are sending a file to someone else.  They have just acknowledged our 
 * prompt, so we want to start sending data like there's no tomorrow.
 */
static int oscar_sendfile_ack(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = gc->proto_data;
	struct gaim_xfer *xfer;
	va_list ap;
	aim_conn_t *conn;
	fu8_t *cookie;
	struct aim_fileheader_t *fh;

	debug_printf("AAA - in oscar_sendfile_ack\n");
	va_start(ap, fr);
	conn = va_arg(ap, aim_conn_t *);
	cookie = va_arg(ap, fu8_t *);
	fh = va_arg(ap, struct aim_fileheader_t *);
	va_end(ap);

	if (!(xfer = oscar_find_xfer_by_cookie(od->file_transfers, cookie)))
		return 1;

	/* We want to stop listening with a normal thingy */
	gaim_input_remove(xfer->watcher);
	xfer->watcher = 0;

	gaim_xfer_start(xfer, xfer->fd, NULL, 0);

	return 0;
}

/*
 * We just sent a file to someone.  They said they got it and everything, 
 * so we can close our direct connection and what not.
 */
static int oscar_sendfile_done(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = gc->proto_data;
	struct gaim_xfer *xfer;
	va_list ap;
	aim_conn_t *conn;
	fu8_t *cookie;
	struct aim_fileheader_t *fh;

	debug_printf("AAA - in oscar_sendfile_done\n");
	va_start(ap, fr);
	conn = va_arg(ap, aim_conn_t *);
	cookie = va_arg(ap, fu8_t *);
	fh = va_arg(ap, struct aim_fileheader_t *);
	va_end(ap);

	if (!(xfer = oscar_find_xfer_by_conn(od->file_transfers, conn)))
		return 1;

	xfer->fd = conn->fd;
	gaim_xfer_end(xfer);

	return 0;
}

static void accept_direct_im(struct ask_direct *d) {
	struct gaim_connection *gc = d->gc;
	struct oscar_data *od;
	struct direct_im *dim;
	char *host; int port = 4443;
	int i, rc;

	if (!g_slist_find(connections, gc)) {
		cancel_direct_im(d);
		return;
	}

	od = (struct oscar_data *)gc->proto_data;
	debug_printf("Accepted DirectIM.\n");

	dim = find_direct_im(od, d->sn);
	if (dim) {
		cancel_direct_im(d); /* 40 */
		return;
	}
	dim = g_new0(struct direct_im, 1);
	dim->gc = d->gc;
	g_snprintf(dim->name, sizeof dim->name, "%s", d->sn);

	dim->conn = aim_odc_connect(od->sess, d->sn, NULL, d->cookie);
	if (!dim->conn) {
		g_free(dim);
		cancel_direct_im(d);
		return;
	}

	aim_conn_addhandler(od->sess, dim->conn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIMINCOMING,
				gaim_odc_incoming, 0);
	aim_conn_addhandler(od->sess, dim->conn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIMTYPING,
				gaim_odc_typing, 0);
	aim_conn_addhandler(od->sess, dim->conn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_IMAGETRANSFER,
			        gaim_update_ui, 0);
	for (i = 0; i < (int)strlen(d->ip); i++) {
		if (d->ip[i] == ':') {
			port = atoi(&(d->ip[i+1]));
			break;
		}
	}
	host = g_strndup(d->ip, i);
	dim->conn->status |= AIM_CONN_STATUS_INPROGRESS;
	rc = proxy_connect(gc->account, host, port, oscar_odc_callback, dim);
	g_free(host);
	if (rc < 0) {
		aim_conn_kill(od->sess, &dim->conn);
		g_free(dim);
		cancel_direct_im(d);
		return;
	}

	cancel_direct_im(d);

	return;
}

static int incomingim_chan1(aim_session_t *sess, aim_conn_t *conn, aim_userinfo_t *userinfo, struct aim_incomingim_ch1_args *args) {
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = gc->proto_data;
	char *tmp;
	int flags = 0;
	int convlen;
	GError *err = NULL;
	struct buddyinfo *bi;

	bi = g_hash_table_lookup(od->buddyinfo, normalize(userinfo->sn));
	if (!bi) {
		bi = g_new0(struct buddyinfo, 1);
		g_hash_table_insert(od->buddyinfo, g_strdup(normalize(userinfo->sn)), bi);
	}

	if (args->icbmflags & AIM_IMFLAGS_AWAY)
		flags |= IM_FLAG_AWAY;

	if (args->icbmflags & AIM_IMFLAGS_TYPINGNOT)
		bi->typingnot = TRUE;
	else
		bi->typingnot = FALSE;

	if ((args->icbmflags & AIM_IMFLAGS_HASICON) && (args->iconlen) && (args->iconsum) && (args->iconstamp)) {
		debug_printf("%s has an icon\n", userinfo->sn);
		if ((args->iconlen != bi->ico_len) || (args->iconsum != bi->ico_csum) || (args->iconstamp != bi->ico_time)) {
			bi->ico_need = TRUE;
			bi->ico_len = args->iconlen;
			bi->ico_csum = args->iconsum;
			bi->ico_time = args->iconstamp;
		}
	}

	if (gc->account->iconfile[0] && (args->icbmflags & AIM_IMFLAGS_BUDDYREQ)) {
		FILE *file;
		struct stat st;

		if (!stat(gc->account->iconfile, &st)) {
			char *buf = g_malloc(st.st_size);
			file = fopen(gc->account->iconfile, "rb");
			if (file) {
				int len = fread(buf, 1, st.st_size, file);
				debug_printf("Sending buddy icon to %s (%d bytes, %lu reported)\n",
					userinfo->sn, len, st.st_size);
				aim_im_sendch2_icon(sess, userinfo->sn, buf, st.st_size,
					st.st_mtime, aimutil_iconsum(buf, st.st_size));
				fclose(file);
			} else
				debug_printf("Can't open buddy icon file!\n");
			g_free(buf);
		} else
			debug_printf("Can't stat buddy icon file!\n");
	}

	debug_printf("Character set is %hu %hu\n", args->charset, args->charsubset);
	if (args->icbmflags & AIM_IMFLAGS_UNICODE) {
		/* This message is marked as UNICODE, so we have to
		 * convert it to utf-8 before handing it to the gaim core.
		 * This conversion should *never* fail, if it does it
		 * means that either the incoming ICBM is corrupted or
		 * there is something we don't understand about it.
		 * For the record, AIM Unicode is big-endian UCS-2 */

		debug_printf("Received UNICODE IM\n");

		if (!args->msg || !args->msglen)
			return 1;

		tmp = g_convert(args->msg, args->msglen, "UTF-8", "UCS-2BE", NULL, &convlen, &err);
		if (err) {
			debug_printf("Unicode IM conversion: %s\n", err->message);
			tmp = strdup(_("(There was an error receiving this message)"));
			g_error_free(err);
		}
	} else {
		/* This will get executed for both AIM_IMFLAGS_ISO_8859_1 and
		 * unflagged messages, which are ASCII.  That's OK because
		 * ASCII is a strict subset of ISO-8859-1; this should
		 * help with compatibility with old, broken versions of
		 * gaim (everything before 0.60) and other broken clients
		 * that will happily send ISO-8859-1 without marking it as
		 * such */
		if (args->icbmflags & AIM_IMFLAGS_ISO_8859_1)
			debug_printf("Received ISO-8859-1 IM\n");

		if (!args->msg || !args->msglen)
			return 1;

		tmp = g_convert(args->msg, args->msglen, "UTF-8", "ISO-8859-1", NULL, &convlen, &err);
		if (err) {
			debug_printf("ISO-8859-1 IM conversion: %s\n", err->message);
			tmp = strdup(_("(There was an error receiving this message)"));
			g_error_free(err);
		}
	}

	/* strip_linefeed(tmp); */
	serv_got_im(gc, userinfo->sn, tmp, flags, time(NULL), -1);
	g_free(tmp);

	return 1;
}

static int incomingim_chan2(aim_session_t *sess, aim_conn_t *conn, aim_userinfo_t *userinfo, struct aim_incomingim_ch2_args *args) {
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = gc->proto_data;

	if (!args)
		return 0;

	debug_printf("rendezvous with %s, status is %hu\n", userinfo->sn, args->status);

	if (args->reqclass & AIM_CAPS_CHAT) {
		char *name;
		int *exch;
		GList *m = NULL;
		
		if (!args->info.chat.roominfo.name || !args->info.chat.roominfo.exchange || !args->msg)
			return 1;
		name = extract_name(args->info.chat.roominfo.name);
		exch = g_new0(int, 1);
		m = g_list_append(m, g_strdup(name ? name : args->info.chat.roominfo.name));
		*exch = args->info.chat.roominfo.exchange;
		m = g_list_append(m, exch);
		serv_got_chat_invite(gc,
				     name ? name : args->info.chat.roominfo.name,
				     userinfo->sn,
				     (char *)args->msg,
				     m);
		if (name)
			g_free(name);
	} else if (args->reqclass & AIM_CAPS_SENDFILE) {
/* BBB */
		if (args->status == AIM_RENDEZVOUS_PROPOSE) {
			/* Someone wants to send a file (or files) to us */
			struct gaim_xfer *xfer;
			struct oscar_xfer_data *xfer_data;

			if (!args->cookie || !args->verifiedip || !args->port ||
			    !args->info.sendfile.filename || !args->info.sendfile.totsize ||
			    !args->info.sendfile.totfiles || !args->reqclass) {
				debug_printf("%s tried to send you a file with incomplete information.\n", userinfo->sn);
				return 1;
			}

			if (args->info.sendfile.subtype == AIM_OFT_SUBTYPE_SEND_DIR) {
				/* last char of the ft req is a star, they are sending us a
				 * directory -- remove the star and trailing slash so we dont save
				 * directories that look like 'dirname\*'  -- arl */
				char *tmp = strrchr(args->info.sendfile.filename, '\\');
				if (tmp && (tmp[1] == '*')) {
					tmp[0] = '\0';
				}
			}

			/* Setup the oscar-specific transfer xfer_data */
			xfer_data = g_malloc0(sizeof(struct oscar_xfer_data));
			xfer_data->gc = gc;
			memcpy(xfer_data->cookie, args->cookie, 8);

			/* Build the file transfer handle */
			xfer = gaim_xfer_new(gc->account, GAIM_XFER_RECEIVE, userinfo->sn);
			xfer_data->xfer = xfer;
			xfer->data = xfer_data;

			/* Set the info about the incoming file */
			gaim_xfer_set_filename(xfer, args->info.sendfile.filename);
			gaim_xfer_set_size(xfer, args->info.sendfile.totsize);
			xfer->remote_port = args->port;
			xfer->remote_ip = g_strdup(args->verifiedip);
			if (args->clientip)
				xfer_data->clientip = g_strdup(args->clientip);
			if (args->clientip2)
				xfer_data->clientip2 = g_strdup(args->clientip2);

			 /* Setup our I/O op functions */
			gaim_xfer_set_init_fnc(xfer, oscar_xfer_init);
			gaim_xfer_set_start_fnc(xfer, oscar_xfer_start);
			gaim_xfer_set_end_fnc(xfer, oscar_xfer_end);
			gaim_xfer_set_cancel_send_fnc(xfer, oscar_xfer_cancel_send);
			gaim_xfer_set_cancel_recv_fnc(xfer, oscar_xfer_cancel_recv);
			gaim_xfer_set_ack_fnc(xfer, oscar_xfer_ack);

			/*
			 * XXX - Should do something with args->msg, args->encoding, and args->language
			 *       probably make it apply to all ch2 messages.
			 */

			/* Keep track of this transfer for later */
			od->file_transfers = g_slist_append(od->file_transfers, xfer);

			/* Now perform the request */
			gaim_xfer_request(xfer);
		} else if (args->status == AIM_RENDEZVOUS_CANCEL) {
			/* The other user wants to cancel a file transfer */
			struct gaim_xfer *xfer;
			debug_printf("AAA - File transfer canceled by remote user\n");
			if ((xfer = oscar_find_xfer_by_cookie(od->file_transfers, args->cookie)))
				gaim_xfer_cancel_remote(xfer);
		} else if (args->status == AIM_RENDEZVOUS_ACCEPT) {
			/*
			 * This gets sent by the receiver of a file 
			 * as they connect directly to us.  If we don't 
			 * get this, then maybe a third party connected 
			 * to us, and we shouldn't send them anything.
			 */
		} else {
			debug_printf("unknown rendezvous status!\n");
		}
	} else if (args->reqclass & AIM_CAPS_GETFILE) {
	} else if (args->reqclass & AIM_CAPS_VOICE) {
	} else if (args->reqclass & AIM_CAPS_BUDDYICON) {
		set_icon_data(gc, userinfo->sn, args->info.icon.icon,
				args->info.icon.length);
	} else if (args->reqclass & AIM_CAPS_IMIMAGE) {
		struct ask_direct *d = g_new0(struct ask_direct, 1);
		char buf[256];

		if (!args->verifiedip) {
			debug_printf("directim kill blocked (%s)\n", userinfo->sn);
			return 1;
		}

		debug_printf("%s received direct im request from %s (%s)\n",
				gc->username, userinfo->sn, args->verifiedip);

		d->gc = gc;
		d->sn = g_strdup(userinfo->sn);
		strncpy(d->ip, args->verifiedip, sizeof(d->ip));
		memcpy(d->cookie, args->cookie, 8);
		g_snprintf(buf, sizeof buf, _("%s has just asked to directly connect to %s"), userinfo->sn, gc->username);
		do_ask_dialog(buf, _("This requires a direct connection between the two computers and is necessary for IM Images.  Because your IP address will be revealed, this may be considered a privacy risk."), d, _("Connect"), accept_direct_im, _("Cancel"), cancel_direct_im, my_protocol->plug ? my_protocol->plug->handle : NULL, FALSE);
	} else {
		debug_printf("Unknown reqclass %hu\n", args->reqclass);
	}

	return 1;
}

/*
 * Authorization Functions
 * Most of these are callbacks from dialogs.  They're used by both 
 * methods of authorization (SSI and old-school channel 4 ICBM)
 */
/* When you ask other people for authorization */
static void gaim_auth_request(struct name_data *data, char *msg) {
	struct gaim_connection *gc = data->gc;

	if (g_slist_find(connections, gc)) {
		struct oscar_data *od = gc->proto_data;
		struct buddy *buddy = gaim_find_buddy(gc->account, data->name);
		struct group *group = gaim_find_buddys_group(buddy);
		if (buddy && group) {
			debug_printf("ssi: adding buddy %s to group %s\n", buddy->name, group->name);
			aim_ssi_sendauthrequest(od->sess, od->conn, data->name, msg ? msg : _("Please authorize me so I can add you to my buddy list."));
			if (!aim_ssi_itemlist_finditem(od->sess->ssi.local, group->name, buddy->name, AIM_SSI_TYPE_BUDDY))
				aim_ssi_addbuddy(od->sess, od->conn, buddy->name, group->name, gaim_get_buddy_alias_only(buddy), NULL, NULL, 1);
		}
	}
}

static void gaim_auth_request_msgprompt(struct name_data *data) {
	do_prompt_dialog(_("Authorization Request Message:"), _("Please authorize me!"), data, gaim_auth_request, gaim_free_name_data);
}

static void gaim_auth_dontrequest(struct name_data *data) {
	struct gaim_connection *gc = data->gc;

	if (g_slist_find(connections, gc)) {
		/* struct oscar_data *od = gc->proto_data; */
		/* XXX - Take the buddy out of our buddy list */
	}

	gaim_free_name_data(data);
}

static void gaim_auth_sendrequest(struct gaim_connection *gc, char *name) {
	struct name_data *data = g_new(struct name_data, 1);
	struct buddy *buddy;
	gchar *dialog_msg, *nombre;

	buddy = gaim_find_buddy(gc->account, name);
	if (buddy && (gaim_get_buddy_alias_only(buddy)))
		nombre = g_strdup_printf("%s (%s)", name, gaim_get_buddy_alias_only(buddy));
	else
		nombre = g_strdup(name);

	dialog_msg = g_strdup_printf(_("The user %s requires authorization before being added to a buddy list.  Do you want to send an authorization request?"), nombre);
	data->gc = gc;
	data->name = g_strdup(name);
	data->nick = NULL;
	do_ask_dialog(_("Request Authorization"), dialog_msg, data, _("Request Authorization"), gaim_auth_request_msgprompt, _("Cancel"), gaim_auth_dontrequest, my_protocol->plug ? my_protocol->plug->handle : NULL, FALSE);

	g_free(dialog_msg);
	g_free(nombre);
}

/* When other people ask you for authorization */
static void gaim_auth_grant(struct name_data *data) {
	struct gaim_connection *gc = data->gc;

	if (g_slist_find(connections, gc)) {
		struct oscar_data *od = gc->proto_data;
#ifdef NOSSI
		struct buddy *buddy;
		gchar message;
		message = 0;
		buddy = gaim_find_buddy(gc->account, data->name);
		aim_im_sendch4(od->sess, data->name, AIM_ICQMSG_AUTHGRANTED, &message);
		show_got_added(gc, NULL, data->name, (buddy ? gaim_get_buddy_alias_only(buddy) : NULL), NULL);
#else
		aim_ssi_sendauthreply(od->sess, od->conn, data->name, 0x01, NULL);
#endif
	}

	gaim_free_name_data(data);
}

/* When other people ask you for authorization */
static void gaim_auth_dontgrant(struct name_data *data, char *msg) {
	struct gaim_connection *gc = data->gc;

	if (g_slist_find(connections, gc)) {
		struct oscar_data *od = gc->proto_data;
#ifdef NOSSI
		aim_im_sendch4(od->sess, data->name, AIM_ICQMSG_AUTHDENIED, msg ? msg : _("No reason given."));
#else
		aim_ssi_sendauthreply(od->sess, od->conn, data->name, 0x00, msg ? msg : _("No reason given."));
#endif
	}
}

static void gaim_auth_dontgrant_msgprompt(struct name_data *data) {
	do_prompt_dialog(_("Authorization Denied Message:"), _("No reason given."), data, gaim_auth_dontgrant, gaim_free_name_data);
}

/* When someone sends you contacts  */
static void gaim_icq_contactadd(struct name_data *data) {
	struct gaim_connection *gc = data->gc;

	if (g_slist_find(connections, gc)) {
		show_add_buddy(gc, data->name, NULL, data->nick);
	}

	gaim_free_name_data(data);
}

static int incomingim_chan4(aim_session_t *sess, aim_conn_t *conn, aim_userinfo_t *userinfo, struct aim_incomingim_ch4_args *args, time_t t) {
	struct gaim_connection *gc = sess->aux_data;
	gchar **msg1, **msg2;
	GError *err = NULL;
	int i;

	if (!args->type || !args->msg || !args->uin)
		return 1;

	debug_printf("Received a channel 4 message of type 0x%02hhx.\n", args->type);

	/* Split up the message at the delimeter character, then convert each string to UTF-8 */
	msg1 = g_strsplit(args->msg, "\376", 0);
	msg2 = (gchar **)g_malloc(10*sizeof(gchar *)); /* XXX - 10 is a guess */
	for (i=0; msg1[i]; i++) {
		strip_linefeed(msg1[i]);
		msg2[i] = g_convert(msg1[i], strlen(msg1[i]), "UTF-8", "ISO-8859-1", NULL, NULL, &err);
		if (err) {
			debug_printf("Error converting a string from ISO-8859-1 to UTF-8 in oscar ICBM channel 4 parsing\n");
			g_error_free(err);
		}
	}
	msg2[i] = NULL;

	switch (args->type) {
		case 0x01: { /* MacICQ message or basic offline message */
			if (i >= 1) {
				gchar *uin = g_strdup_printf("%lu", args->uin);
				if (t) { /* This is an offline message */
					/* I think this timestamp is in UTC, or something */
					serv_got_im(gc, uin, msg2[0], 0, t, -1);
				} else { /* This is a message from MacICQ/Miranda */
					serv_got_im(gc, uin, msg2[0], 0, time(NULL), -1);
				}
				g_free(uin);
			}
		} break;

		case 0x04: { /* Someone sent you a URL */
			if (i >= 2) {
				gchar *uin = g_strdup_printf("%lu", args->uin);
				gchar *message = g_strdup_printf("<A HREF=\"%s\">%s</A>", msg2[1], msg2[0]);
				serv_got_im(gc, uin, message, 0, time(NULL), -1);
				g_free(uin);
				g_free(message);
			}
		} break;

		case 0x06: { /* Someone requested authorization */
			if (i >= 6) {
				struct name_data *data = g_new(struct name_data, 1);
				gchar *dialog_msg = g_strdup_printf(_("The user %lu wants to add you to their buddy list for the following reason:\n%s"), args->uin, msg2[5] ? msg2[5] : _("No reason given."));
				debug_printf("Received an authorization request from UIN %lu\n", args->uin);
				data->gc = gc;
				data->name = g_strdup_printf("%lu", args->uin);
				data->nick = NULL;
				do_ask_dialog(_("Authorization Request"), dialog_msg, data, _("Authorize"), gaim_auth_grant, _("Deny"), gaim_auth_dontgrant_msgprompt, my_protocol->plug ? my_protocol->plug->handle : NULL, FALSE);
				g_free(dialog_msg);
			}
		} break;

		case 0x07: { /* Someone has denied you authorization */
			if (i >= 1) {
				gchar *dialog_msg = g_strdup_printf(_("The user %lu has denied your request to add them to your contact list for the following reason:\n%s"), args->uin, msg2[0] ? msg2[0] : _("No reason given."));
				do_error_dialog(_("ICQ authorization denied."), dialog_msg, GAIM_INFO);
				g_free(dialog_msg);
			}
		} break;

		case 0x08: { /* Someone has granted you authorization */
			gchar *dialog_msg = g_strdup_printf(_("The user %lu has granted your request to add them to your contact list."), args->uin);
			do_error_dialog("ICQ authorization accepted.", dialog_msg, GAIM_INFO);
			g_free(dialog_msg);
		} break;

		case 0x09: { /* Message from the Godly ICQ server itself, I think */
			if (i >= 5) {
				gchar *dialog_msg = g_strdup_printf(_("You have received a special message\n\nFrom: %s [%s]\n%s"), msg2[0], msg2[3], msg2[5]);
				do_error_dialog("ICQ Server Message", dialog_msg, GAIM_INFO);
				g_free(dialog_msg);
			}
		} break;

		case 0x0d: { /* Someone has sent you a pager message from http://www.icq.com/your_uin */
			if (i >= 6) {
				gchar *dialog_msg = g_strdup_printf(_("You have received an ICQ page\n\nFrom: %s [%s]\n%s"), msg2[0], msg2[3], msg2[5]);
				do_error_dialog("ICQ Page", dialog_msg, GAIM_INFO);
				g_free(dialog_msg);
			}
		} break;

		case 0x0e: { /* Someone has emailed you at your_uin@pager.icq.com */
			if (i >= 6) {
				gchar *dialog_msg = g_strdup_printf(_("You have received an ICQ email from %s [%s]\n\nMessage is:\n%s"), msg2[0], msg2[3], msg2[5]);
				do_error_dialog("ICQ Email", dialog_msg, GAIM_INFO);
				g_free(dialog_msg);
			}
		} break;

		case 0x12: {
			/* Ack for authorizing/denying someone.  Or possibly an ack for sending any system notice */
			/* Someone added you to their contact list? */
		} break;

		case 0x13: { /* Someone has sent you some ICQ contacts */
			int i, num;
			gchar **text;
			text = g_strsplit(args->msg, "\376", 0);
			if (text) {
				num = 0;
				for (i=0; i<strlen(text[0]); i++)
					num = num*10 + text[0][i]-48;
				for (i=0; i<num; i++) {
					struct name_data *data = g_new(struct name_data, 1);
					gchar *message = g_strdup_printf(_("ICQ user %lu has sent you a contact: %s (%s)"), args->uin, text[i*2+2], text[i*2+1]);
					data->gc = gc;
					data->name = g_strdup(text[i*2+1]);
					data->nick = g_strdup(text[i*2+2]);
					do_ask_dialog(message, _("Do you want to add this contact to your Buddy List?"), data, _("Add"), gaim_icq_contactadd, _("Decline"), gaim_free_name_data, my_protocol->plug ? my_protocol->plug->handle : NULL, FALSE);
					g_free(message);
				}
				g_strfreev(text);
			}
		} break;

		case 0x1a: { /* Someone has sent you a greeting card or requested contacts? */
			/* This is boring and silly. */
		} break;

		default: {
			debug_printf("Received a channel 4 message of unknown type (type 0x%02hhx).\n", args->type);
		} break;
	}

	g_strfreev(msg1);
	g_strfreev(msg2);

	return 1;
}

static int gaim_parse_incoming_im(aim_session_t *sess, aim_frame_t *fr, ...) {
	fu16_t channel;
	int ret = 0;
	aim_userinfo_t *userinfo;
	va_list ap;

	va_start(ap, fr);
	channel = (fu16_t)va_arg(ap, unsigned int);
	userinfo = va_arg(ap, aim_userinfo_t *);

	switch (channel) {
		case 1: { /* standard message */
			struct aim_incomingim_ch1_args *args;
			args = va_arg(ap, struct aim_incomingim_ch1_args *);
			ret = incomingim_chan1(sess, fr->conn, userinfo, args);
		} break;

		case 2: { /* rendevous */
			struct aim_incomingim_ch2_args *args;
			args = va_arg(ap, struct aim_incomingim_ch2_args *);
			ret = incomingim_chan2(sess, fr->conn, userinfo, args);
		} break;

		case 4: { /* ICQ */
			struct aim_incomingim_ch4_args *args;
			args = va_arg(ap, struct aim_incomingim_ch4_args *);
			ret = incomingim_chan4(sess, fr->conn, userinfo, args, 0);
		} break;

		default: {
			debug_printf("ICBM received on unsupported channel (channel 0x%04hx).", channel);
		} break;
	}

	va_end(ap);

	return ret;
}

static int gaim_parse_misses(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	fu16_t chan, nummissed, reason;
	aim_userinfo_t *userinfo;
	char buf[1024];

	va_start(ap, fr);
	chan = (fu16_t)va_arg(ap, unsigned int);
	userinfo = va_arg(ap, aim_userinfo_t *);
	nummissed = (fu16_t)va_arg(ap, unsigned int);
	reason = (fu16_t)va_arg(ap, unsigned int);
	va_end(ap);

	switch(reason) {
		case 0:
			/* Invalid (0) */
			g_snprintf(buf,
				   sizeof(buf),
				   ngettext( 
				   "You missed %hu message from %s because it was invalid.",
				   "You missed %hu messages from %s because they were invalid.",
				   nummissed),
				   nummissed,
				   userinfo->sn);
			break;
		case 1:
			/* Message too large */
			g_snprintf(buf,
				   sizeof(buf),
				   ngettext( 
				   "You missed %hu message from %s because it was too large.",
				   "You missed %hu messages from %s because they were too large.",
				   nummissed),
				   nummissed,
				   userinfo->sn);
			break;
		case 2:
			/* Rate exceeded */
			g_snprintf(buf,
				   sizeof(buf),
				   ngettext( 
				   "You missed %hu message from %s because the rate limit has been exceeded.",
				   "You missed %hu messages from %s because the rate limit has been exceeded.",
				   nummissed),
				   nummissed,
				   userinfo->sn);
			break;
		case 3:
			/* Evil Sender */
			g_snprintf(buf,
				   sizeof(buf),
				   ngettext( 
				   "You missed %hu message from %s because he/she was too evil.",
				   "You missed %hu messages from %s because he/she was too evil.",
				   nummissed),
				   nummissed,
				   userinfo->sn);
			break;
		case 4:
			/* Evil Receiver */
			g_snprintf(buf,
				   sizeof(buf),
				   ngettext( 
				   "You missed %hu message from %s because you are too evil.",
				   "You missed %hu messages from %s because you are too evil.",
				   nummissed),
				   nummissed,
				   userinfo->sn);
			break;
		default:
			g_snprintf(buf,
				   sizeof(buf),
				   ngettext( 
				   "You missed %hu message from %s for an unknown reason.",
				   "You missed %hu messages from %s for an unknown reason.",
				   nummissed),
				   nummissed,
				   userinfo->sn);
			break;
	}
	do_error_dialog(buf, NULL, GAIM_ERROR);

	return 1;
}

static char *gaim_icq_status(int state) {
	/* Make a cute little string that shows the status of the dude or dudet */
	if (state & AIM_ICQ_STATE_CHAT)
		return g_strdup_printf(_("Free For Chat"));
	else if (state & AIM_ICQ_STATE_DND)
		return g_strdup_printf(_("Do Not Disturb"));
	else if (state & AIM_ICQ_STATE_OUT)
		return g_strdup_printf(_("Not Available"));
	else if (state & AIM_ICQ_STATE_BUSY)
		return g_strdup_printf(_("Occupied"));
	else if (state & AIM_ICQ_STATE_AWAY)
		return g_strdup_printf(_("Away"));
	else if (state & AIM_ICQ_STATE_WEBAWARE)
		return g_strdup_printf(_("Web Aware"));
	else if (state & AIM_ICQ_STATE_INVISIBLE)
		return g_strdup_printf(_("Invisible"));
	else
		return g_strdup_printf(_("Online"));
}

static int gaim_parse_clientauto_ch2(aim_session_t *sess, const char *who, fu16_t reason, const char *cookie) {
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = gc->proto_data;

/* BBB */
	switch (reason) {
		case 3: { /* Decline sendfile. */
			struct gaim_xfer *xfer;
			debug_printf("AAA - Other user declined file transfer\n");
			if ((xfer = oscar_find_xfer_by_cookie(od->file_transfers, cookie)))
				gaim_xfer_cancel_remote(xfer);
		} break;

		default: {
			debug_printf("Received an unknown rendezvous client auto-response from %s.  Type 0x%04hx\n", who, reason);
		}

	}

	return 0;
}

static int gaim_parse_clientauto_ch4(aim_session_t *sess, char *who, fu16_t reason, fu32_t state, char *msg) {
	struct gaim_connection *gc = sess->aux_data;

	switch(reason) {
		case 0x0003: { /* Reply from an ICQ status message request */
			char *status_msg = gaim_icq_status(state);
			char *dialog_msg, **splitmsg;
			struct oscar_data *od = gc->proto_data;
			GSList *l = od->evilhack;
			gboolean evilhack = FALSE;

			/* Split at (carriage return/newline)'s, then rejoin later with BRs between. */
			splitmsg = g_strsplit(msg, "\r\n", 0);

			/* If who is in od->evilhack, then we're just getting the away message, otherwise this 
			 * will just get appended to the info box (which is already showing). */
			while (l) {
				char *x = l->data;
				if (!strcmp(x, normalize(who))) {
					evilhack = TRUE;
					g_free(x);
					od->evilhack = g_slist_remove(od->evilhack, x);
					break;
				}
				l = l->next;
			}

			if (evilhack)
				dialog_msg = g_strdup_printf(_("<B>UIN:</B> %s<BR><B>Status:</B> %s<HR>%s"), who, status_msg, g_strjoinv("<BR>", splitmsg));
			else
				dialog_msg = g_strdup_printf(_("<B>Status:</B> %s<HR>%s"), status_msg, g_strjoinv("<BR>", splitmsg));
			g_show_info_text(gc, who, 2, dialog_msg, NULL);

			g_free(status_msg);
			g_free(dialog_msg);
			g_strfreev(splitmsg);
		} break;

		default: {
			debug_printf("Received an unknown client auto-response from %s.  Type 0x%04hx\n", who, reason);
		} break;
	} /* end of switch */

	return 0;
}

static int gaim_parse_clientauto(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	fu16_t chan, reason;
	char *who;

	va_start(ap, fr);
	chan = (fu16_t)va_arg(ap, unsigned int);
	who = va_arg(ap, char *);
	reason = (fu16_t)va_arg(ap, unsigned int);

	if (chan == 0x0002) { /* File transfer declined */
		char *cookie = va_arg(ap, char *);
		return gaim_parse_clientauto_ch2(sess, who, reason, cookie);
	} else if (chan == 0x0004) { /* ICQ message */
		fu32_t state = 0;
		char *msg = NULL;
		if (reason == 0x0003) {
			state = va_arg(ap, fu32_t);
			msg = va_arg(ap, char *);
		}
		return gaim_parse_clientauto_ch4(sess, who, reason, state, msg);
	}

	va_end(ap);

	return 1;
}

static int gaim_parse_genericerr(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	fu16_t reason;
	char *m;

	va_start(ap, fr);
	reason = (fu16_t) va_arg(ap, unsigned int);
	va_end(ap);

	debug_printf("snac threw error (reason 0x%04hx: %s)\n", reason,
			(reason < msgerrreasonlen) ? msgerrreason[reason] : "unknown");

	m = g_strdup_printf(_("SNAC threw error: %s\n"),
			reason < msgerrreasonlen ? gettext(msgerrreason[reason]) : _("Unknown error"));
	do_error_dialog(m, NULL, GAIM_ERROR);
	g_free(m);

	return 1;
}

static int gaim_parse_msgerr(aim_session_t *sess, aim_frame_t *fr, ...) {
#if 0
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = gc->proto_data;
	struct gaim_xfer *xfer;
#endif
	va_list ap;
	fu16_t reason;
	char *data, *buf;

	va_start(ap, fr);
	reason = (fu16_t)va_arg(ap, unsigned int);
	data = va_arg(ap, char *);
	va_end(ap);

	debug_printf("Message error with data %s and reason %hu\n", data, reason);

/* BBB */
#if 0
	/* If this was a file transfer request, data is a cookie */
	if ((xfer = oscar_find_xfer_by_cookie(od->file_transfers, data))) {
		gaim_xfer_cancel_remote(xfer);
		return 1;
	}
#endif

	/* Data is assumed to be the destination sn */
	buf = g_strdup_printf(_("Your message to %s did not get sent:"), data);
	do_error_dialog(buf, (reason < msgerrreasonlen) ? gettext(msgerrreason[reason]) : _("No reason given."), GAIM_ERROR);
	g_free(buf);

	return 1;
}

static int gaim_parse_mtn(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct gaim_connection *gc = sess->aux_data;
	va_list ap;
	fu16_t type1, type2;
	char *sn;

	va_start(ap, fr);
	type1 = (fu16_t) va_arg(ap, unsigned int);
	sn = va_arg(ap, char *);
	type2 = (fu16_t) va_arg(ap, unsigned int);
	va_end(ap);

	switch (type2) {
		case 0x0000: { /* Text has been cleared */
			serv_got_typing_stopped(gc, sn);
		} break;

		case 0x0001: { /* Paused typing */
			serv_got_typing(gc, sn, 0, TYPED);
		} break;

		case 0x0002: { /* Typing */
			serv_got_typing(gc, sn, 0, TYPING);
		} break;

		default: {
			printf("Received unknown typing notification message from %s.  Type1 is 0x%04x and type2 is 0x%04hx.\n", sn, type1, type2);
		} break;
	}

	return 1;
}

static int gaim_parse_locerr(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	char *destn;
	fu16_t reason;
	char buf[1024];

	va_start(ap, fr);
	reason = (fu16_t) va_arg(ap, unsigned int);
	destn = va_arg(ap, char *);
	va_end(ap);

	snprintf(buf, sizeof(buf), _("User information for %s unavailable:"), destn);
	do_error_dialog(buf, (reason < msgerrreasonlen) ? gettext(msgerrreason[reason]) : _("No reason given."), GAIM_ERROR);

	return 1;
}

static char *images(int flags) {
	static char buf[1024];
	g_snprintf(buf, sizeof(buf), "%s%s%s%s%s%s%s",
			(flags & AIM_FLAG_ACTIVEBUDDY) ? "<IMG SRC=\"ab_icon.gif\">" : "",
			(flags & AIM_FLAG_UNCONFIRMED) ? "<IMG SRC=\"dt_icon.gif\">" : "",
			(flags & AIM_FLAG_AOL) ? "<IMG SRC=\"aol_icon.gif\">" : "",
			(flags & AIM_FLAG_ICQ) ? "<IMG SRC=\"icq_icon.gif\">" : "",
			(flags & AIM_FLAG_ADMINISTRATOR) ? "<IMG SRC=\"admin_icon.gif\">" : "",
			(flags & AIM_FLAG_FREE) ? "<IMG SRC=\"free_icon.gif\">" : "",
			(flags & AIM_FLAG_WIRELESS) ? "<IMG SRC=\"wireless_icon.gif\">" : "");
	return buf;
}


static char *caps_string(guint caps)
{
	static char buf[512], *tmp;
	int count = 0, i = 0;
	guint bit = 1;

	if (!caps) {
		return NULL;
	} else while (bit <= 0x20000) {
		if (bit & caps) {
			switch (bit) {
			case 0x1:
				tmp = _("Buddy Icon");
				break;
			case 0x2:
				tmp = _("Voice");
				break;
			case 0x4:
				tmp = _("IM Image");
				break;
			case 0x8:
				tmp = _("Chat");
				break;
			case 0x10:
				tmp = _("Get File");
				break;
			case 0x20:
				tmp = _("Send File");
				break;
			case 0x40:
			case 0x200:
				tmp = _("Games");
				break;
			case 0x80:
				tmp = _("Stocks");
				break;
			case 0x100:
				tmp = _("Send Buddy List");
				break;
			case 0x400:
				tmp = _("EveryBuddy Bug");
				break;
			case 0x800:
				tmp = _("AP User");
				break;
			case 0x1000:
				tmp = _("ICQ RTF");
				break;
			case 0x2000:
				tmp = _("Nihilist");
				break;
			case 0x4000:
				tmp = _("ICQ Server Relay");
				break;
			case 0x8000:
				tmp = _("ICQ Unknown");
				break;
			case 0x10000:
				tmp = _("Trillian Encryption");
				break;
			case 0x20000:
				tmp = _("ICQ UTF8");
				break;
			default:
				tmp = NULL;
				break;
			}
			if (tmp)
				i += g_snprintf(buf + i, sizeof(buf) - i, "%s%s", (count ? ", " : ""),
						tmp);
			count++;
		}
		bit <<= 1;
	}
	return buf; 
}

static char *oscar_tooltip_text(struct buddy *b) {
	struct gaim_connection *gc = b->account->gc;
	struct oscar_data *od = gc->proto_data;
	struct buddyinfo *bi = g_hash_table_lookup(od->buddyinfo, normalize(b->name));

	if (bi) {
		gchar *yay;
		char *caps = caps_string(bi->caps);
		char *tstr = sec_to_text(time(NULL) - bi->signon);
		yay = g_strdup_printf(_("<b>Logged In:</b> %s%s%s"), tstr, 
				       caps ? _("\n<b>Capabilities:</b> ") : "", caps ? caps : "");
		free(tstr);
		return yay;
	} else {
		return NULL;
	}
}

static int gaim_parse_user_info(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = gc->proto_data;
	char header[BUF_LONG];
	GSList *l = od->evilhack;
	gboolean evilhack = FALSE;
	gchar *membersince = NULL, *onlinesince = NULL, *idle = NULL;
	fu32_t flags;
	va_list ap;
	aim_userinfo_t *info;
	fu16_t infotype;
	char *text_enc = NULL, *text = NULL, *utf8 = NULL;
	int text_len;

	va_start(ap, fr);
	info = va_arg(ap, aim_userinfo_t *);
	infotype = (fu16_t) va_arg(ap, unsigned int);
	text_enc = va_arg(ap, char *);
	text = va_arg(ap, char *);
	text_len = va_arg(ap, int);
	va_end(ap);

	if (text_len > 0) {
		flags = parse_encoding (text_enc);
		switch (flags) {
		case 0:
			utf8 = g_strndup(text, text_len);
			break;
		case AIM_IMFLAGS_ISO_8859_1:
			utf8 = g_convert(text, text_len, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
			break;
		case AIM_IMFLAGS_UNICODE:
			utf8 = g_convert(text, text_len, "UTF-8", "UCS-2BE", NULL, NULL, NULL);
			break;
		default:
			utf8 = g_strdup(_("<i>Unable to display information because it was sent in an unknown encoding.</i>"));
			debug_printf("Encountered an unknown encoding while parsing userinfo\n");
		}
	}

	if (info->present & AIM_USERINFO_PRESENT_ONLINESINCE) {
		onlinesince = g_strdup_printf("Online Since : <b>%s</b><br>\n",
					asctime(localtime(&info->onlinesince)));
	}

	if (info->present & AIM_USERINFO_PRESENT_MEMBERSINCE) {
		membersince = g_strdup_printf("Member Since : <b>%s</b><br>\n",
					asctime(localtime(&info->membersince)));
	}

	if (info->present & AIM_USERINFO_PRESENT_IDLE) {
		gchar *itime = sec_to_text(info->idletime*60);
		idle = g_strdup_printf("Idle : <b>%s</b>", itime);
		g_free(itime);
	} else
		idle = g_strdup("Idle: <b>Active</b>");

	g_snprintf(header, sizeof header,
			_("Username : <b>%s</b>  %s <br>\n"
			"Warning Level : <b>%d %%</b><br>\n"
			"%s"
			"%s"
			"%s\n"
			"<hr>\n"),
			info->sn, images(info->flags),
			info->warnlevel/10,
			onlinesince ? onlinesince : "",
			membersince ? membersince : "",
			idle ? idle : "");

	g_free(onlinesince);
	g_free(membersince);
	g_free(idle);

	while (l) {
		char *x = l->data;
		if (!strcmp(x, normalize(info->sn))) {
			evilhack = TRUE;
			g_free(x);
			od->evilhack = g_slist_remove(od->evilhack, x);
			break;
		}
		l = l->next;
	}

	if (infotype == AIM_GETINFO_AWAYMESSAGE) {
		if (evilhack) {
			g_show_info_text(gc, info->sn, 2,
					 header,
					 (utf8 && *utf8) ? away_subs(utf8, gc->username) :
					 _("<i>User has no away message</i>"), NULL);
		} else {
			g_show_info_text(gc, info->sn, 0,
					 header,
					 (utf8 && *utf8) ? away_subs(utf8, gc->username) : NULL,
					 (utf8 && *utf8) ? "<hr>" : NULL,
					 NULL);
		}
	} else if (infotype == AIM_GETINFO_CAPABILITIES) {
		g_show_info_text(gc, info->sn, 2,
				header,
				"<i>", _("Client Capabilities: "),
				caps_string(info->capabilities),
				"</i>",
				NULL);
	} else {
		g_show_info_text(gc, info->sn, 1,
				 (utf8 && *utf8) ? away_subs(utf8, gc->username) :
				 _("<i>No Information Provided</i>"),
				 NULL);
	}

	g_free(utf8);

	return 1;
}

static int gaim_parse_motd(aim_session_t *sess, aim_frame_t *fr, ...) {
	char *msg;
	fu16_t id;
	va_list ap;

	va_start(ap, fr);
	id  = (fu16_t) va_arg(ap, unsigned int);
	msg = va_arg(ap, char *);
	va_end(ap);

	debug_printf("MOTD: %s (%hu)\n", msg ? msg : "Unknown", id);
	if (id < 4)
		do_error_dialog(_("Your AIM connection may be lost."), NULL, GAIM_WARNING);

	return 1;
}

static int gaim_chatnav_info(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	fu16_t type;
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;

	va_start(ap, fr);
	type = (fu16_t) va_arg(ap, unsigned int);

	switch(type) {
		case 0x0002: {
			fu8_t maxrooms;
			struct aim_chat_exchangeinfo *exchanges;
			int exchangecount, i;

			maxrooms = (fu8_t) va_arg(ap, unsigned int);
			exchangecount = va_arg(ap, int);
			exchanges = va_arg(ap, struct aim_chat_exchangeinfo *);

			debug_printf("chat info: Chat Rights:\n");
			debug_printf("chat info: \tMax Concurrent Rooms: %hhd\n", maxrooms);
			debug_printf("chat info: \tExchange List: (%d total)\n", exchangecount);
			for (i = 0; i < exchangecount; i++)
				debug_printf("chat info: \t\t%hu    %s\n", exchanges[i].number, exchanges[i].name ? exchanges[i].name : "");
			while (od->create_rooms) {
				struct create_room *cr = od->create_rooms->data;
				debug_printf("creating room %s\n", cr->name);
				aim_chatnav_createroom(sess, fr->conn, cr->name, cr->exchange);
				g_free(cr->name);
				od->create_rooms = g_slist_remove(od->create_rooms, cr);
				g_free(cr);
			}
			}
			break;
		case 0x0008: {
			char *fqcn, *name, *ck;
			fu16_t instance, flags, maxmsglen, maxoccupancy, unknown, exchange;
			fu8_t createperms;
			fu32_t createtime;

			fqcn = va_arg(ap, char *);
			instance = (fu16_t)va_arg(ap, unsigned int);
			exchange = (fu16_t)va_arg(ap, unsigned int);
			flags = (fu16_t)va_arg(ap, unsigned int);
			createtime = va_arg(ap, fu32_t);
			maxmsglen = (fu16_t)va_arg(ap, unsigned int);
			maxoccupancy = (fu16_t)va_arg(ap, unsigned int);
			createperms = (fu8_t)va_arg(ap, unsigned int);
			unknown = (fu16_t)va_arg(ap, unsigned int);
			name = va_arg(ap, char *);
			ck = va_arg(ap, char *);

			debug_printf("created room: %s %hu %hu %hu %lu %hu %hu %hhu %hu %s %s\n",
					fqcn,
					exchange, instance, flags,
					createtime,
					maxmsglen, maxoccupancy, createperms, unknown,
					name, ck);
			aim_chat_join(od->sess, od->conn, exchange, ck, instance);
			}
			break;
		default:
			debug_printf("chatnav info: unknown type (%04hx)\n", type);
			break;
	}

	va_end(ap);

	return 1;
}

static int gaim_chat_join(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	int count, i;
	aim_userinfo_t *info;
	struct gaim_connection *g = sess->aux_data;

	struct chat_connection *c = NULL;

	va_start(ap, fr);
	count = va_arg(ap, int);
	info  = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	c = find_oscar_chat_by_conn(g, fr->conn);
	if (!c)
		return 1;

	for (i = 0; i < count; i++)
		gaim_chat_add_user(GAIM_CHAT(c->cnv), info[i].sn, NULL);

	return 1;
}

static int gaim_chat_leave(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	int count, i;
	aim_userinfo_t *info;
	struct gaim_connection *g = sess->aux_data;

	struct chat_connection *c = NULL;

	va_start(ap, fr);
	count = va_arg(ap, int);
	info  = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	c = find_oscar_chat_by_conn(g, fr->conn);
	if (!c)
		return 1;

	for (i = 0; i < count; i++)
		gaim_chat_remove_user(GAIM_CHAT(c->cnv), info[i].sn, NULL);

	return 1;
}

static int gaim_chat_info_update(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	aim_userinfo_t *userinfo;
	struct aim_chat_roominfo *roominfo;
	char *roomname;
	int usercount;
	char *roomdesc;
	fu16_t unknown_c9, unknown_d2, unknown_d5, maxmsglen, maxvisiblemsglen;
	fu32_t creationtime;
	struct gaim_connection *gc = sess->aux_data;
	struct chat_connection *ccon = find_oscar_chat_by_conn(gc, fr->conn);

	va_start(ap, fr);
	roominfo = va_arg(ap, struct aim_chat_roominfo *);
	roomname = va_arg(ap, char *);
	usercount= va_arg(ap, int);
	userinfo = va_arg(ap, aim_userinfo_t *);
	roomdesc = va_arg(ap, char *);
	unknown_c9 = (fu16_t)va_arg(ap, unsigned int);
	creationtime = va_arg(ap, fu32_t);
	maxmsglen = (fu16_t)va_arg(ap, unsigned int);
	unknown_d2 = (fu16_t)va_arg(ap, unsigned int);
	unknown_d5 = (fu16_t)va_arg(ap, unsigned int);
	maxvisiblemsglen = (fu16_t)va_arg(ap, unsigned int);
	va_end(ap);

	debug_printf("inside chat_info_update (maxmsglen = %hu, maxvislen = %hu)\n",
			maxmsglen, maxvisiblemsglen);

	ccon->maxlen = maxmsglen;
	ccon->maxvis = maxvisiblemsglen;

	return 1;
}

static int gaim_chat_incoming_msg(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct gaim_connection *gc = sess->aux_data;
	va_list ap;
	aim_userinfo_t *info;
	char *msg;
	struct chat_connection *ccon = find_oscar_chat_by_conn(gc, fr->conn);

	va_start(ap, fr);
	info = va_arg(ap, aim_userinfo_t *);
	msg = va_arg(ap, char *);
	va_end(ap);

	serv_got_chat_in(gc, ccon->id, info->sn, 0, msg, time((time_t)NULL));

	return 1;
}

static int gaim_email_parseupdate(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	struct gaim_connection *gc = sess->aux_data;
	struct aim_emailinfo *emailinfo;
	int havenewmail;

	va_start(ap, fr);
	emailinfo = va_arg(ap, struct aim_emailinfo *);
	havenewmail = va_arg(ap, int);
	va_end(ap);

	if (emailinfo) {
		if (emailinfo->unread) {
			if (havenewmail)
				connection_has_mail(gc, emailinfo->nummsgs ? emailinfo->nummsgs : -1, NULL, NULL, emailinfo->url);
		} else
			connection_has_mail(gc, 0, NULL, NULL, emailinfo->url);
	}

	return 1;
}

static int gaim_icon_error(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = gc->proto_data;
	char *sn;

	sn = od->requesticon->data;
	debug_printf("removing %s from hash table\n", sn);
	od->requesticon = g_slist_remove(od->requesticon, sn);
	free(sn);

	if (od->icontimer)
		g_source_remove(od->icontimer);
	od->icontimer = g_timeout_add(500, gaim_icon_timerfunc, gc);

	return 1;
}

static int gaim_icon_parseicon(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = gc->proto_data;
	GSList *cur;
	va_list ap;
	char *sn;
	fu8_t *iconstr, *icon;
	fu16_t iconstrlen, iconlen;

	va_start(ap, fr);
	sn = va_arg(ap, char *);
	iconstr = va_arg(ap, fu8_t *);
	iconstrlen = va_arg(ap, int);
	icon = va_arg(ap, fu8_t *);
	iconlen = va_arg(ap, int);
	va_end(ap);

	if (iconlen > 0)
		set_icon_data(gc, sn, icon, iconlen);

	cur = od->requesticon;
	while (cur) {
		char *cursn = cur->data;
		if (!aim_sncmp(cursn, sn)) {
			od->requesticon = g_slist_remove(od->requesticon, cursn);
			free(cursn);
			cur = od->requesticon;
		} else
			cur = cur->next;
	}

	if (od->icontimer)
		g_source_remove(od->icontimer);
	od->icontimer = g_timeout_add(250, gaim_icon_timerfunc, gc);

	return 1;
}

static gboolean gaim_icon_timerfunc(gpointer data) {
	struct gaim_connection *gc = data;
	struct oscar_data *od = gc->proto_data;
	struct buddy *b;
	struct buddyinfo *bi;
	aim_conn_t *conn;
	char *buddy_icon;

	if (!od->requesticon) {
		debug_printf("no more icons to request\n");
		return FALSE;
	}

	conn = aim_getconn_type(od->sess, AIM_CONN_TYPE_ICON);
	if (!conn && !od->iconconnecting) {
		aim_reqservice(od->sess, od->conn, AIM_CONN_TYPE_ICON);
		od->iconconnecting = TRUE;
		return FALSE;
	}

	bi = g_hash_table_lookup(od->buddyinfo, (char *)od->requesticon->data);
	b = gaim_find_buddy(gc->account, (char *)od->requesticon->data);
	buddy_icon = gaim_buddy_get_setting(b, "buddy_icon");
	if (bi && (bi->iconstrlen > 0) && !buddy_icon) {
		aim_icon_requesticon(od->sess, od->requesticon->data, bi->iconstr, bi->iconstrlen);
		return FALSE;
	} else {
		char *sn = od->requesticon->data;
		od->requesticon = g_slist_remove(od->requesticon, sn);
		free(sn);
	}
	free(buddy_icon);

	return TRUE;
}

/*
 * Recieved in response to an IM sent with the AIM_IMFLAGS_ACK option.
 */
static int gaim_parse_msgack(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	fu16_t type;
	char *sn;

	va_start(ap, fr);
	type = (fu16_t) va_arg(ap, unsigned int);
	sn = va_arg(ap, char *);
	va_end(ap);

	debug_printf("Sent message to %s.\n", sn);

	return 1;
}

static int gaim_parse_ratechange(aim_session_t *sess, aim_frame_t *fr, ...) {
	static const char *codes[5] = {
		"invalid",
		"change",
		"warning",
		"limit",
		"limit cleared",
	};
	va_list ap;
	fu16_t code, rateclass;
	fu32_t windowsize, clear, alert, limit, disconnect, currentavg, maxavg;

	va_start(ap, fr); 
	code = (fu16_t)va_arg(ap, unsigned int);
	rateclass= (fu16_t)va_arg(ap, unsigned int);
	windowsize = va_arg(ap, fu32_t);
	clear = va_arg(ap, fu32_t);
	alert = va_arg(ap, fu32_t);
	limit = va_arg(ap, fu32_t);
	disconnect = va_arg(ap, fu32_t);
	currentavg = va_arg(ap, fu32_t);
	maxavg = va_arg(ap, fu32_t);
	va_end(ap);

	debug_printf("rate %s (param ID 0x%04hx): curavg = %lu, maxavg = %lu, alert at %lu, "
		     "clear warning at %lu, limit at %lu, disconnect at %lu (window size = %lu)\n",
		     (code < 5) ? codes[code] : codes[0],
		     rateclass,
		     currentavg, maxavg,
		     alert, clear,
		     limit, disconnect,
		     windowsize);

	/* XXX fix these values */
	if (code == AIM_RATE_CODE_CHANGE) {
		if (currentavg >= clear)
			aim_conn_setlatency(fr->conn, 0);
	} else if (code == AIM_RATE_CODE_WARNING) {
		aim_conn_setlatency(fr->conn, windowsize/4);
	} else if (code == AIM_RATE_CODE_LIMIT) {
		do_error_dialog(_("Rate limiting error."),
				_("The last message was not sent because you are over the rate limit.  "
				  "Please wait 10 seconds and try again."), GAIM_ERROR);
		aim_conn_setlatency(fr->conn, windowsize/2);
	} else if (code == AIM_RATE_CODE_CLEARLIMIT) {
		aim_conn_setlatency(fr->conn, 0);
	}

	return 1;
}

static int gaim_parse_evilnotify(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	fu16_t newevil;
	aim_userinfo_t *userinfo;
	struct gaim_connection *gc = sess->aux_data;

	va_start(ap, fr);
	newevil = (fu16_t) va_arg(ap, unsigned int);
	userinfo = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	serv_got_eviled(gc, (userinfo && userinfo->sn[0]) ? userinfo->sn : NULL, newevil / 10);

	return 1;
}

static int gaim_selfinfo(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	aim_userinfo_t *info;
	struct gaim_connection *gc = sess->aux_data;

	va_start(ap, fr);
	info = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	gc->evil = info->warnlevel/10;
	/* gc->correction_time = (info->onlinesince - gc->login_time); */

	return 1;
}

static int gaim_connerr(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = gc->proto_data;
	va_list ap;
	fu16_t code;
	char *msg;

	va_start(ap, fr);
	code = (fu16_t)va_arg(ap, int);
	msg = va_arg(ap, char *);
	va_end(ap);

	debug_printf("Disconnected.  Code is 0x%04x and msg is %s\n", code, msg);
	if ((fr) && (fr->conn) && (fr->conn->type == AIM_CONN_TYPE_BOS)) {
		if (code == 0x0001) {
			hide_login_progress_error(gc, _("You have been disconnected because you have signed on with this screen name at another location."));
		} else {
			hide_login_progress_error(gc, _("You have been signed off for an unknown reason."));
		}
		od->killme = TRUE;
	}

	return 1;
}

static int conninitdone_bos(aim_session_t *sess, aim_frame_t *fr, ...) {

	aim_reqpersonalinfo(sess, fr->conn);

#ifndef NOSSI
	debug_printf("ssi: requesting ssi list\n");
	aim_ssi_reqrights(sess, fr->conn);
	aim_ssi_reqdata(sess, fr->conn, sess->ssi.timestamp, sess->ssi.numitems);
#endif

	aim_bos_reqlocaterights(sess, fr->conn);
	aim_bos_reqbuddyrights(sess, fr->conn);
	aim_im_reqparams(sess);
	aim_bos_reqrights(sess, fr->conn);

#ifdef NOSSI
	aim_bos_setgroupperm(sess, fr->conn, AIM_FLAG_ALLUSERS);
	aim_bos_setprivacyflags(sess, fr->conn, AIM_PRIVFLAGS_ALLOWIDLE | AIM_PRIVFLAGS_ALLOWMEMBERSINCE);
#endif

	return 1;
}

static int conninitdone_admin(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = gc->proto_data;

	aim_clientready(sess, fr->conn);
	debug_printf("connected to admin\n");

	if (od->chpass) {
		debug_printf("changing password\n");
		aim_admin_changepasswd(sess, fr->conn, od->newp, od->oldp);
		g_free(od->oldp);
		od->oldp = NULL;
		g_free(od->newp);
		od->newp = NULL;
		od->chpass = FALSE;
	}
	if (od->setnick) {
		debug_printf("formatting screenname\n");
		aim_admin_setnick(sess, fr->conn, od->newsn);
		g_free(od->newsn);
		od->newsn = NULL;
		od->setnick = FALSE;
	}
	if (od->conf) {
		debug_printf("confirming account\n");
		aim_admin_reqconfirm(sess, fr->conn);
		od->conf = FALSE;
	}
	if (od->reqemail) {
		debug_printf("requesting email\n");
		aim_admin_getinfo(sess, fr->conn, 0x0011);
		od->reqemail = FALSE;
	}
	if (od->setemail) {
		debug_printf("setting email\n");
		aim_admin_setemail(sess, fr->conn, od->email);
		g_free(od->email);
		od->setemail = FALSE;
	}

	return 1;
}

static int gaim_icbm_param_info(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct aim_icbmparameters *params;
	va_list ap;

	va_start(ap, fr);
	params = va_arg(ap, struct aim_icbmparameters *);
	va_end(ap);

	/* XXX - evidently this crashes on solaris. i have no clue why
	debug_printf("ICBM Parameters: maxchannel = %hu, default flags = 0x%08lx, max msg len = %hu, "
			"max sender evil = %f, max receiver evil = %f, min msg interval = %lu\n",
			params->maxchan, params->flags, params->maxmsglen,
			((float)params->maxsenderwarn)/10.0, ((float)params->maxrecverwarn)/10.0,
			params->minmsginterval);
	*/

	/* Maybe senderwarn and recverwarn should be user preferences... */
	params->flags = 0x0000000b;
	params->maxmsglen = 8000;
	params->minmsginterval = 0;

	aim_im_setparams(sess, params);

	return 1;
}

static int gaim_parse_locaterights(aim_session_t *sess, aim_frame_t *fr, ...)
{
	va_list ap;
	fu16_t maxsiglen;
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	char *unicode;
	int unicode_len;
	fu32_t flags;

	va_start(ap, fr);
	maxsiglen = (fu16_t) va_arg(ap, int);
	va_end(ap);

	debug_printf("locate rights: max sig len = %d\n", maxsiglen);

	od->rights.maxsiglen = od->rights.maxawaymsglen = (guint)maxsiglen;

	if (od->icq)
		aim_bos_setprofile(sess, fr->conn, NULL, NULL, 0, NULL, NULL, 0, caps_icq);
	else {
		flags = check_encoding (gc->account->user_info);

		if (flags == 0) {
			aim_bos_setprofile(sess, fr->conn, "us-ascii", gc->account->user_info,
					   strlen(gc->account->user_info), NULL, NULL, 0, caps_aim);
		} else {
			unicode = g_convert (gc->account->user_info, strlen(gc->account->user_info),
					     "UCS-2BE", "UTF-8", NULL, &unicode_len, NULL);
			aim_bos_setprofile(sess, fr->conn, "unicode-2-0", unicode, unicode_len,
					   NULL, NULL, 0, caps_aim);
			g_free(unicode);
		}
	}

	return 1;
}

static int gaim_parse_buddyrights(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	fu16_t maxbuddies, maxwatchers;
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;

	va_start(ap, fr);
	maxbuddies = (fu16_t) va_arg(ap, unsigned int);
	maxwatchers = (fu16_t) va_arg(ap, unsigned int);
	va_end(ap);

	debug_printf("buddy list rights: Max buddies = %hu / Max watchers = %hu\n", maxbuddies, maxwatchers);

	od->rights.maxbuddies = (guint)maxbuddies;
	od->rights.maxwatchers = (guint)maxwatchers;

	return 1;
}

static int gaim_bosrights(aim_session_t *sess, aim_frame_t *fr, ...) {
	fu16_t maxpermits, maxdenies;
	va_list ap;
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;

	va_start(ap, fr);
	maxpermits = (fu16_t) va_arg(ap, unsigned int);
	maxdenies = (fu16_t) va_arg(ap, unsigned int);
	va_end(ap);

	debug_printf("BOS rights: Max permit = %hu / Max deny = %hu\n", maxpermits, maxdenies);

	od->rights.maxpermits = (guint)maxpermits;
	od->rights.maxdenies = (guint)maxdenies;

	account_online(gc);
	serv_finish_login(gc);

	debug_printf("buddy list loaded\n");

	aim_clientready(sess, fr->conn);

	/* XXX - Should call aim_bos_setidle with 0x0000 */

	if (od->icq) {
		aim_icq_reqofflinemsgs(sess);
		aim_icq_hideip(sess);
	}

	aim_reqservice(sess, fr->conn, AIM_CONN_TYPE_CHATNAV);
	if (sess->authinfo->email)
		aim_reqservice(sess, fr->conn, AIM_CONN_TYPE_EMAIL);

	return 1;
}

static int gaim_offlinemsg(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	struct aim_icq_offlinemsg *msg;
	struct aim_incomingim_ch4_args args;
	time_t t;

	va_start(ap, fr);
	msg = va_arg(ap, struct aim_icq_offlinemsg *);
	va_end(ap);

	debug_printf("Received offline message.  Converting to channel 4 ICBM...\n");
	args.uin = msg->sender;
	args.type = msg->type;
	args.flags = msg->flags;
	args.msglen = msg->msglen;
	args.msg = msg->msg;
	t = get_time(msg->year, msg->month, msg->day, msg->hour, msg->minute, 0);
	incomingim_chan4(sess, fr->conn, NULL, &args, t);

	return 1;
}

static int gaim_offlinemsgdone(aim_session_t *sess, aim_frame_t *fr, ...)
{
	aim_icq_ackofflinemsgs(sess);
	return 1;
}

static int gaim_icqinfo(aim_session_t *sess, aim_frame_t *fr, ...)
{
	struct gaim_connection *gc = sess->aux_data;
	gchar *buf, *tmp, *utf8;
	gchar who[16];
	va_list ap;
	struct aim_icq_info *info;

	va_start(ap, fr);
	info = va_arg(ap, struct aim_icq_info *);
	va_end(ap);

	if (!info->uin)
		return 0;

	g_snprintf(who, sizeof(who), "%lu", info->uin);
	buf = g_strdup_printf("<b>%s</b> %s", _("UIN:"), who);
	if (info->nick && info->nick[0] && (utf8 = gaim_try_conv_to_utf8(info->nick))) {
		tmp = buf;  buf = g_strconcat(tmp, "\n<br><b>", _("Nick:"), "</b> ", utf8, NULL);  g_free(tmp); g_free(utf8);
	}
	if (info->first && info->first[0] && (utf8 = gaim_try_conv_to_utf8(info->first))) {
		tmp = buf;  buf = g_strconcat(tmp, "\n<br><b>", _("First Name:"), "</b> ", utf8, NULL);  g_free(tmp); g_free(utf8);
	}
	if (info->last && info->last[0] && (utf8 = gaim_try_conv_to_utf8(info->last))) {
		tmp = buf;  buf = g_strconcat(tmp, "\n<br><b>", _("Last Name:"), "</b> ", utf8, NULL);  g_free(tmp); g_free(utf8);
	}
	if (info->email && info->email[0] && (utf8 = gaim_try_conv_to_utf8(info->email))) {
		tmp = buf;  buf = g_strconcat(tmp, "\n<br><b>", _("Email Address:"), "</b> <a href=\"mailto:", utf8, "\">", utf8, "</a>", NULL);  g_free(tmp); g_free(utf8);
	}
	if (info->numaddresses && info->email2) {
		int i;
		for (i = 0; i < info->numaddresses; i++) {
			if (info->email2[i] && info->email2[i][0] && (utf8 = gaim_try_conv_to_utf8(info->email2[i]))) {
			tmp = buf;  buf = g_strconcat(tmp, "\n<br><b>", _("Email Address:"), "</b> <a href=\"mailto:", utf8, "\">", utf8, "</a>", NULL);  g_free(tmp); g_free(utf8);
			}
		}
	}
	if (info->mobile && info->mobile[0] && (utf8 = gaim_try_conv_to_utf8(info->mobile))) {
		tmp = buf;  buf = g_strconcat(tmp, "\n<br><b>", _("Mobile Phone:"), "</b> ", utf8, NULL);  g_free(tmp); g_free(utf8);
	}
	if (info->gender) {
		tmp = buf;  buf = g_strconcat(tmp, "\n<br><b>", _("Gender:"), "</b> ", info->gender==1 ? _("Female") : _("Male"), NULL);  g_free(tmp);
	}
	if (info->birthyear || info->birthmonth || info->birthday) {
		char date[30];
		struct tm tm;
		tm.tm_mday = (int)info->birthday;
		tm.tm_mon = (int)info->birthmonth-1;
		tm.tm_year = (int)info->birthyear-1900;
		strftime(date, sizeof(date), "%x", &tm);
		tmp = buf;  buf = g_strconcat(tmp, "\n<br><b>", _("Birthday:"), "</b> ", date, NULL);  g_free(tmp);
	}
	if (info->age) {
		char age[5];
		snprintf(age, sizeof(age), "%hhd", info->age);
		tmp = buf;  buf = g_strconcat(tmp, "\n<br><b>", _("Age:"), "</b> ", age, NULL);  g_free(tmp);
	}
	if (info->personalwebpage && info->personalwebpage[0] && (utf8 = gaim_try_conv_to_utf8(info->personalwebpage))) {
		tmp = buf;  buf = g_strconcat(tmp, "\n<br><b>", _("Personal Web Page:"), "</b> <a href=\"", utf8, "\">", utf8, "</a>", NULL);  g_free(tmp); g_free(utf8);
	}
	if (info->info && info->info[0] && (utf8 = gaim_try_conv_to_utf8(info->info))) {
		tmp = buf;  buf = g_strconcat(tmp, "<hr><b>", _("Additional Information:"), "</b><br>", utf8, NULL);  g_free(tmp); g_free(utf8);
	}
	tmp = buf;  buf = g_strconcat(tmp, "<hr>\n", NULL);  g_free(tmp);
	if ((info->homeaddr && (info->homeaddr[0])) || (info->homecity && info->homecity[0]) || (info->homestate && info->homestate[0]) || (info->homezip && info->homezip[0])) {
		tmp = buf;  buf = g_strconcat(tmp, "<b>", _("Home Address:"), "</b>", NULL);  g_free(tmp);
		if (info->homeaddr && info->homeaddr[0] && (utf8 = gaim_try_conv_to_utf8(info->homeaddr))) {
			tmp = buf;  buf = g_strconcat(tmp, "\n<br><b>", _("Address:"), "</b> ", utf8, NULL);  g_free(tmp); g_free(utf8);
		}
		if (info->homecity && info->homecity[0] && (utf8 = gaim_try_conv_to_utf8(info->homecity))) {
			tmp = buf;  buf = g_strconcat(tmp, "\n<br><b>", _("City:"), "</b> ", utf8,  NULL);  g_free(tmp); g_free(utf8);
		}
		if (info->homestate && info->homestate[0] && (utf8 = gaim_try_conv_to_utf8(info->homestate))) {
			tmp = buf;  buf = g_strconcat(tmp, "\n<br><b>", _("State:"), "</b> ", utf8, NULL);  g_free(tmp); g_free(utf8);
		}
		if (info->homezip && info->homezip[0] && (utf8 = gaim_try_conv_to_utf8(info->homezip))) {
			tmp = buf;  buf = g_strconcat(tmp, "\n<br><b>", _("Zip Code:"), "</b> ", utf8, NULL);  g_free(tmp); g_free(utf8);
		}
		tmp = buf; buf = g_strconcat(tmp, "\n<hr>\n", NULL); g_free(tmp);
	}
	if ((info->workaddr && info->workaddr[0]) || (info->workcity && info->workcity[0]) || (info->workstate && info->workstate[0]) || (info->workzip && info->workzip[0])) {
		tmp = buf;  buf = g_strconcat(tmp, "<b>", _("Work Address:"), "</b>", NULL);  g_free(tmp);
		if (info->workaddr && info->workaddr[0] && (utf8 = gaim_try_conv_to_utf8(info->workaddr))) {
			tmp = buf;  buf = g_strconcat(tmp, "\n<br><b>", _("Address:"), "</b> ", utf8, NULL);  g_free(tmp); g_free(utf8);
		}
		if (info->workcity && info->workcity[0] && (utf8 = gaim_try_conv_to_utf8(info->workcity))) {
			tmp = buf;  buf = g_strconcat(tmp, "\n<br><b>", _("City:"), "</b> ", utf8, NULL);  g_free(tmp); g_free(utf8);
		}
		if (info->workstate && info->workstate[0] && (utf8 = gaim_try_conv_to_utf8(info->workstate))) {
			tmp = buf;  buf = g_strconcat(tmp, "\n<br><b>", _("State:"), "</b> ", utf8, NULL);  g_free(tmp); g_free(utf8);
		}
		if (info->workzip && info->workzip[0] && (utf8 = gaim_try_conv_to_utf8(info->workzip))) {
			tmp = buf;  buf = g_strconcat(tmp, "\n<br><b>", _("Zip Code:"), "</b> ", utf8, NULL);  g_free(tmp); g_free(utf8);
		}
		tmp = buf; buf = g_strconcat(tmp, "\n<hr>\n", NULL); g_free(tmp);
	}
	if ((info->workcompany && info->workcompany[0]) || (info->workdivision && info->workdivision[0]) || (info->workposition && info->workposition[0]) || (info->workwebpage && info->workwebpage[0])) {
		tmp = buf;  buf = g_strconcat(tmp, "<b>", _("Work Information:"), "</b>", NULL);  g_free(tmp);
		if (info->workcompany && info->workcompany[0] && (utf8 = gaim_try_conv_to_utf8(info->workcompany))) {
			tmp = buf;  buf = g_strconcat(tmp, "\n<br><b>", _("Company:"), "</b> ", utf8, NULL);  g_free(tmp); g_free(utf8);
		}
		if (info->workdivision && info->workdivision[0] && (utf8 = gaim_try_conv_to_utf8(info->workdivision))) {
			tmp = buf;  buf = g_strconcat(tmp, "\n<br><b>", _("Division:"), "</b> ", utf8, NULL);  g_free(tmp); g_free(utf8);
		}
		if (info->workposition && info->workposition[0] && (utf8 = gaim_try_conv_to_utf8(info->workposition))) {
			tmp = buf;  buf = g_strconcat(tmp, "\n<br><b>", _("Position:"), "</b> ", utf8, NULL);  g_free(tmp); g_free(utf8);
		}
		if (info->workwebpage && info->workwebpage[0] && (utf8 = gaim_try_conv_to_utf8(info->workwebpage))) {
			tmp = buf;  buf = g_strconcat(tmp, "\n<br><b>", _("Web Page:"), "</b> <a href=\"", utf8, "\">", utf8, "</a>", NULL);  g_free(tmp); g_free(utf8);
		}
		tmp = buf; buf = g_strconcat(tmp, "\n<hr>\n", NULL); g_free(tmp);
	}

	g_show_info_text(gc, who, 2, buf, NULL);
	g_free(buf);

	return 1;
}

static int gaim_icqalias(aim_session_t *sess, aim_frame_t *fr, ...)
{
	struct gaim_connection *gc = sess->aux_data;
	gchar who[16], *utf8;
	struct buddy *b;
	va_list ap;
	struct aim_icq_info *info;

	va_start(ap, fr);
	info = va_arg(ap, struct aim_icq_info *);
	va_end(ap);

	if (info->uin && info->nick && info->nick[0] && (utf8 = gaim_try_conv_to_utf8(info->nick))) {
		g_snprintf(who, sizeof(who), "%lu", info->uin);
		serv_got_alias(gc, who, utf8);
		if ((b = gaim_find_buddy(gc->account, who))) {
			gaim_buddy_set_setting(b, "servernick", utf8);
			gaim_blist_save();
		}
		g_free(utf8);
	}

	return 1;
}

static int gaim_popup(aim_session_t *sess, aim_frame_t *fr, ...)
{
	char *msg, *url;
	fu16_t wid, hei, delay;
	va_list ap;

	va_start(ap, fr);
	msg = va_arg(ap, char *);
	url = va_arg(ap, char *);
	wid = (fu16_t) va_arg(ap, int);
	hei = (fu16_t) va_arg(ap, int);
	delay = (fu16_t) va_arg(ap, int);
	va_end(ap);

	serv_got_popup(msg, url, wid, hei);

	return 1;
}

static int gaim_parse_searchreply(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	char *address, *SNs;
	int i, num;
	char *buf;
	int at = 0, len;

	va_start(ap, fr);
	address = va_arg(ap, char *);
	num = va_arg(ap, int);
	SNs = va_arg(ap, char *);
	va_end(ap);

	len = num * (MAXSNLEN + 1) + 1024;
	buf = g_malloc(len);
	at += g_snprintf(buf + at, len - at, "<B>%s has the following screen names:</B><BR>", address);
	for (i = 0; i < num; i++)
		at += g_snprintf(buf + at, len - at, "%s<BR>", &SNs[i * (MAXSNLEN + 1)]);
	g_show_info_text(NULL, NULL, 2, buf, NULL);
	g_free(buf);

	return 1;
}

static int gaim_parse_searcherror(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	char *address;
	char buf[BUF_LONG];

	va_start(ap, fr);
	address = va_arg(ap, char *);
	va_end(ap);

	g_snprintf(buf, sizeof(buf), "No results found for email address %s", address);
	do_error_dialog(buf, NULL, GAIM_ERROR);

	return 1;
}

static int gaim_account_confirm(aim_session_t *sess, aim_frame_t *fr, ...) {
	fu16_t status;
	va_list ap;
	char msg[256];
	struct gaim_connection *gc = sess->aux_data;

	va_start(ap, fr);
	status = (fu16_t) va_arg(ap, unsigned int); /* status code of confirmation request */
	va_end(ap);

	debug_printf("account confirmation returned status 0x%04x (%s)\n", status,
			status ? "unknown" : "email sent");
	if (!status) {
		g_snprintf(msg, sizeof(msg), "You should receive an email asking to confirm %s.",
				gc->username);
		do_error_dialog(_("Account Confirmation Requested"), msg, GAIM_INFO);
	}

	return 1;
}

static int gaim_info_change(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct gaim_connection *gc = sess->aux_data;
	va_list ap;
	fu16_t perms, err;
	char *url, *sn, *email;
	int change;

	va_start(ap, fr);
	change = va_arg(ap, int);
	perms = (fu16_t) va_arg(ap, unsigned int);
	err = (fu16_t) va_arg(ap, unsigned int);
	url = va_arg(ap, char *);
	sn = va_arg(ap, char *);
	email = va_arg(ap, char *);
	va_end(ap);

	debug_printf("account info: because of %s, perms=0x%04x, err=0x%04x, url=%s, sn=%s, email=%s\n",
		change ? "change" : "request", perms, err, url, sn, email);

	if (err && url) {
		char *dialog_msg;
		char *dialog_top = g_strdup_printf(_("Error Changing Account Info"));
		switch (err) {
			case 0x0001: {
				dialog_msg = g_strdup_printf(_("Error 0x%04x: Unable to format screen name because the requested screen name differs from the original."), err);
			} break;
			case 0x0006: {
				dialog_msg = g_strdup_printf(_("Error 0x%04x: Unable to format screen name because the requested screen name ends in a space."), err);
			} break;
			case 0x000b: {
				dialog_msg = g_strdup_printf(_("Error 0x%04x: Unable to format screen name because the requested screen name is too long."), err);
			} break;
			case 0x001d: {
				dialog_msg = g_strdup_printf(_("Error 0x%04x: Unable to change email address because there is already a request pending for this screen name."), err);
			} break;
			case 0x0021: {
				dialog_msg = g_strdup_printf(_("Error 0x%04x: Unable to change email address because the given address has too many screen names associated with it."), err);
			} break;
			case 0x0023: {
				dialog_msg = g_strdup_printf(_("Error 0x%04x: Unable to change email address because the given address is invalid."), err);
			} break;
			default: {
				dialog_msg = g_strdup_printf(_("Error 0x%04x: Unknown error."), err);
			} break;
		}
		do_error_dialog(dialog_top, dialog_msg, GAIM_ERROR);
		g_free(dialog_top);
		g_free(dialog_msg);
		return 1;
	}

	if (sn) {
		char *dialog_msg = g_strdup_printf(_("Your screen name is currently formatted as follows:\n%s"), sn);
		do_error_dialog(_("Account Info"), dialog_msg, GAIM_INFO);
		g_free(dialog_msg);
	}

	if (email) {
		char *dialog_msg = g_strdup_printf(_("The email address for %s is %s"), gc->username, email);
		do_error_dialog(_("Account Info"), dialog_msg, GAIM_INFO);
		g_free(dialog_msg);
	}

	return 1;
}

static void oscar_keepalive(struct gaim_connection *gc) {
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	aim_flap_nop(od->sess, od->conn);
}

static int oscar_send_typing(struct gaim_connection *gc, char *name, int typing) {
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	struct direct_im *dim = find_direct_im(od, name);
	if (dim)
		aim_odc_send_typing(od->sess, dim->conn, typing);
	else {
		struct buddyinfo *bi = g_hash_table_lookup(od->buddyinfo, normalize(name));
		if (bi && bi->typingnot) {
			if (typing == TYPING)
				aim_im_sendmtn(od->sess, 0x0001, name, 0x0002);
			else if (typing == TYPED)
				aim_im_sendmtn(od->sess, 0x0001, name, 0x0001);
			else
				aim_im_sendmtn(od->sess, 0x0001, name, 0x0000);
		}
	}
	return 0;
}
static void oscar_ask_direct_im(struct gaim_connection *gc, char *name);

static int oscar_send_im(struct gaim_connection *gc, char *name, char *message, int len, int imflags) {
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	struct direct_im *dim = find_direct_im(od, name);
	int ret = 0;
	GError *err = NULL;

	if (dim) {
		if (dim->connected) {  /* If we're not connected yet, send through server */
			/* XXX - The last parameter below is the encoding.  Let Paco-Paco do something with it. */
			ret =  aim_odc_send_im(od->sess, dim->conn, message, len == -1 ? strlen(message) : len, 0);
			if (ret == 0)
				return 1;
			else return ret;
		}
		debug_printf("Direct IM pending, but not connected; sending through server\n");
	} else if (len != -1) {
		/* Trying to send an IM image outside of a direct connection. */
		oscar_ask_direct_im(gc, name);
		return -ENOTCONN;
	}

	if (imflags & IM_FLAG_AWAY) {
		ret = aim_im_sendch1(od->sess, name, AIM_IMFLAGS_AWAY, message);
	} else {
		struct buddyinfo *bi;
		struct aim_sendimext_args args;
		struct stat st;
		int len;

		bi = g_hash_table_lookup(od->buddyinfo, normalize(name));
		if (!bi) {
			bi = g_new0(struct buddyinfo, 1);
			g_hash_table_insert(od->buddyinfo, g_strdup(normalize(name)), bi);
		}

		args.flags = AIM_IMFLAGS_ACK | AIM_IMFLAGS_CUSTOMFEATURES;
		if (od->icq) {
			args.features = features_icq;
			args.featureslen = sizeof(features_icq);
			args.flags |= AIM_IMFLAGS_OFFLINE;
		} else {
			args.features = features_aim;
			args.featureslen = sizeof(features_aim);
		}

		if (bi->ico_need) {
			debug_printf("Sending buddy icon request with message\n");
			args.flags |= AIM_IMFLAGS_BUDDYREQ;
			bi->ico_need = FALSE;
		}

		if (gc->account->iconfile[0] && !stat(gc->account->iconfile, &st)) {
			FILE *file = fopen(gc->account->iconfile, "r");
			if (file) {
				char *buf = g_malloc(st.st_size);
				fread(buf, 1, st.st_size, file);

				args.iconlen   = st.st_size;
				args.iconsum   = aimutil_iconsum(buf, st.st_size);
				args.iconstamp = st.st_mtime;

				if ((args.iconlen != bi->ico_me_len) || (args.iconsum != bi->ico_me_csum) || (args.iconstamp != bi->ico_me_time))
					bi->ico_informed = FALSE;

				if (!bi->ico_informed) {
					debug_printf("Claiming to have a buddy icon\n");
					args.flags |= AIM_IMFLAGS_HASICON;
					bi->ico_me_len = args.iconlen;
					bi->ico_me_csum = args.iconsum;
					bi->ico_me_time = args.iconstamp;
					bi->ico_informed = TRUE;
				}


				fclose(file);
				g_free(buf);
			}
		}

		args.destsn = name;

		len = strlen(message);
		args.flags |= check_encoding(message);
		if (args.flags & AIM_IMFLAGS_UNICODE) {
			debug_printf("Sending Unicode IM\n");
			args.charset = 0x0002;
			args.charsubset = 0x0000;
			args.msg = g_convert(message, len, "UCS-2BE", "UTF-8", NULL, &len, &err);
			if (err) {
				debug_printf("Error converting a unicode message: %s\n", err->message);
				debug_printf("This really shouldn't happen!\n");
				/* We really shouldn't try to send the
				 * IM now, but I'm not sure what to do */
				g_error_free(err);
			}
		} else if (args.flags & AIM_IMFLAGS_ISO_8859_1) {
			debug_printf("Sending ISO-8859-1 IM\n");
			args.charset = 0x0003;
			args.charsubset = 0x0000;
			args.msg = g_convert(message, len, "ISO-8859-1", "UTF-8", NULL, &len, &err);
			if (err) {
				debug_printf("conversion error: %s\n", err->message);
				debug_printf("Someone tell Ethan his 8859-1 detection is wrong\n");
				args.flags ^= AIM_IMFLAGS_ISO_8859_1 | AIM_IMFLAGS_UNICODE;
				len = strlen(message);
				g_error_free(err);
				args.msg = g_convert(message, len, "UCS-2BE", "UTF8", NULL, &len, &err);
				if (err) {
					debug_printf("Error in unicode fallback: %s\n", err->message);
					g_error_free(err);
				}
			}
		} else {
			args.charset = 0x0000;
			args.charsubset = 0x0000;
			args.msg = message;
		}
		args.msglen = len;

		ret = aim_im_sendch1_ext(od->sess, &args);
	}
	if (ret >= 0)
		return 1;
	return ret;
}

static void oscar_get_info(struct gaim_connection *g, char *name) {
	struct oscar_data *od = (struct oscar_data *)g->proto_data;
	if (od->icq)
		aim_icq_getallinfo(od->sess, name);
	else
		/* people want the away message on the top, so we get the away message
		 * first and then get the regular info, since it's too difficult to
		 * insert in the middle. i hate people. */
		aim_getinfo(od->sess, od->conn, name, AIM_GETINFO_AWAYMESSAGE);
}

static void oscar_get_away(struct gaim_connection *g, char *who) {
	struct oscar_data *od = (struct oscar_data *)g->proto_data;
	if (od->icq) {
		struct buddy *budlight = gaim_find_buddy(g->account, who);
		if (budlight)
			if ((budlight->uc & 0xffff0000) >> 16)
				aim_im_sendch2_geticqaway(od->sess, who, (budlight->uc & 0xffff0000) >> 16);
			else
				debug_printf("Error: The user %s has no status message, therefore not requesting.\n", who);
		else
			debug_printf("Error: Could not find %s in local contact list, therefore unable to request status message.\n", who);
	} else
		aim_getinfo(od->sess, od->conn, who, AIM_GETINFO_GENERALINFO);
}

#if 0
static void oscar_get_caps(struct gaim_connection *g, char *name) {
	struct oscar_data *od = (struct oscar_data *)g->proto_data;
	aim_getinfo(od->sess, od->conn, name, AIM_GETINFO_CAPABILITIES);
}
#endif

static void oscar_set_dir(struct gaim_connection *g, const char *first, const char *middle, const char *last,
			  const char *maiden, const char *city, const char *state, const char *country, int web) {
	/* XXX - some of these things are wrong, but i'm lazy */
	struct oscar_data *od = (struct oscar_data *)g->proto_data;
	aim_setdirectoryinfo(od->sess, od->conn, first, middle, last,
				maiden, NULL, NULL, city, state, NULL, 0, web);
}


static void oscar_set_idle(struct gaim_connection *g, int time) {
	struct oscar_data *od = (struct oscar_data *)g->proto_data;
	aim_bos_setidle(od->sess, od->conn, time);
}

static void oscar_set_info(struct gaim_connection *g, char *info) {
	struct oscar_data *od = (struct oscar_data *)g->proto_data;
	gchar *inforeal, *unicode;
	fu32_t flags;
	int unicode_len;

	if (od->rights.maxsiglen == 0)
		do_error_dialog(_("Unable to set AIM profile."), 
				_("You have probably requested to set your profile before the login procedure completed.  "
				  "Your profile remains unset; try setting it again when you are fully connected."), GAIM_ERROR);

	if (strlen(info) > od->rights.maxsiglen) {
		gchar *errstr;

		errstr = g_strdup_printf(_("The maximum profile length of %d bytes has been exceeded.  "
					   "Gaim has truncated and set it."), od->rights.maxsiglen);
		do_error_dialog("Profile too long.", errstr, GAIM_WARNING);

		g_free(errstr);
	}

	inforeal = g_strndup(info, od->rights.maxsiglen);

	if (od->icq)
		aim_bos_setprofile(od->sess, od->conn, NULL, NULL, 0, NULL, NULL, 0, caps_icq);
	else {
		flags = check_encoding(inforeal);

		if (flags == 0) {
			aim_bos_setprofile(od->sess, od->conn, "us-ascii", inforeal, strlen (inforeal),
					   NULL, NULL, 0, caps_aim);
		} else {
			unicode = g_convert (inforeal, strlen(inforeal), "UCS-2BE", "UTF-8", NULL,
					     &unicode_len, NULL);
			aim_bos_setprofile(od->sess, od->conn, "unicode-2-0", unicode, unicode_len,
					   NULL, NULL, 0, caps_aim);
			g_free(unicode);
		}
	}
	g_free(inforeal);

	return;
}

static void oscar_set_away_aim(struct gaim_connection *gc, struct oscar_data *od, const char *message)
{
	fu32_t flags;
	char *unicode;
	int unicode_len;

	if (od->rights.maxawaymsglen == 0)
		do_error_dialog(_("Unable to set AIM away message."), 
				_("You have probably requested to set your away message before the login procedure completed.  "
				  "You remain in a \"present\" state; try setting it again when you are fully connected."), GAIM_ERROR);
	
	if (gc->away) {
		g_free(gc->away);
		gc->away = NULL;
	}

	if (!message) {
		aim_bos_setprofile(od->sess, od->conn, NULL, NULL, 0, NULL, "", 0, caps_aim);
		return;
	}

	if (strlen(message) > od->rights.maxawaymsglen) {
		gchar *errstr;

		errstr = g_strdup_printf(_("The away message length of %d bytes has been exceeded.  "
					   "Gaim has truncated it and set you away."), od->rights.maxawaymsglen);
		do_error_dialog("Away message too long.", errstr, GAIM_WARNING);
		g_free(errstr);
	}

	gc->away = g_strndup(message, od->rights.maxawaymsglen);
	flags = check_encoding(message);
	
	if (flags == 0) {
		aim_bos_setprofile(od->sess, od->conn, NULL, NULL, 0, "us-ascii", gc->away, strlen(gc->away),
				   caps_aim);
	} else {
		unicode = g_convert(message, strlen(message), "UCS-2BE", "UTF-8", NULL, &unicode_len, NULL);
		aim_bos_setprofile(od->sess, od->conn, NULL, NULL, 0, "unicode-2-0", unicode, unicode_len,
				   caps_aim);
		g_free(unicode);
	}

	return;
}

static void oscar_set_away_icq(struct gaim_connection *gc, struct oscar_data *od, const char *state, const char *message)
{

	if (gc->away) {
		g_free(gc->away);
		gc->away = NULL;
	}

	if (strcmp(state, _("Invisible"))) {
		if (aim_ssi_getpermdeny(od->sess->ssi.local) != gc->account->permdeny)
			aim_ssi_setpermdeny(od->sess, od->conn, gc->account->permdeny,
					0xffffffff);
		gc->account->permdeny = 4;
	} else {
		if (aim_ssi_getpermdeny(od->sess->ssi.local) != 0x03)
			aim_ssi_setpermdeny(od->sess, od->conn, 0x03, 0xffffffff);
		gc->account->permdeny = 3;
	}

	if (!strcmp(state, _("Online")))
		aim_setextstatus(od->sess, od->conn, AIM_ICQ_STATE_NORMAL);
	else if (!strcmp(state, _("Away"))) {
		aim_setextstatus(od->sess, od->conn, AIM_ICQ_STATE_AWAY);
		gc->away = g_strdup("");
	} else if (!strcmp(state, _("Do Not Disturb"))) {
		aim_setextstatus(od->sess, od->conn, AIM_ICQ_STATE_AWAY | AIM_ICQ_STATE_DND | AIM_ICQ_STATE_BUSY);
		gc->away = g_strdup("");
	} else if (!strcmp(state, _("Not Available"))) {
		aim_setextstatus(od->sess, od->conn, AIM_ICQ_STATE_OUT | AIM_ICQ_STATE_AWAY);
		gc->away = g_strdup("");
	} else if (!strcmp(state, _("Occupied"))) {
		aim_setextstatus(od->sess, od->conn, AIM_ICQ_STATE_AWAY | AIM_ICQ_STATE_BUSY);
		gc->away = g_strdup("");
	} else if (!strcmp(state, _("Free For Chat"))) {
		aim_setextstatus(od->sess, od->conn, AIM_ICQ_STATE_CHAT);
		gc->away = g_strdup("");
	} else if (!strcmp(state, _("Invisible"))) {
		aim_setextstatus(od->sess, od->conn, AIM_ICQ_STATE_INVISIBLE);
		gc->away = g_strdup("");
	} else if (!strcmp(state, GAIM_AWAY_CUSTOM)) {
	 	if (message) {
			aim_setextstatus(od->sess, od->conn, AIM_ICQ_STATE_OUT | AIM_ICQ_STATE_AWAY);
			gc->away = g_strdup("");
		} else {
			aim_setextstatus(od->sess, od->conn, AIM_ICQ_STATE_NORMAL);
		}
	}

	return;
}

static void oscar_set_away(struct gaim_connection *gc, char *state, char *message)
{
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;

	if (od->icq)
		oscar_set_away_icq(gc, od, state, message);
	else
		oscar_set_away_aim(gc, od, message);

	return;
}

static void oscar_warn(struct gaim_connection *gc, char *name, int anon) {
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	aim_im_warn(od->sess, od->conn, name, anon ? AIM_WARN_ANON : 0);
}

static void oscar_dir_search(struct gaim_connection *gc, const char *first, const char *middle, const char *last,
			     const char *maiden, const char *city, const char *state, const char *country, const char *email) {
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	if (strlen(email))
		aim_usersearch_address(od->sess, od->conn, email);
}

static void oscar_add_buddy(struct gaim_connection *gc, const char *name) {
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
#ifdef NOSSI
	aim_add_buddy(od->sess, od->conn, name);
#else
	if ((od->sess->ssi.received_data) && !(aim_ssi_itemlist_exists(od->sess->ssi.local, name))) {
		struct buddy *buddy = gaim_find_buddy(gc->account, name);
		struct group *group = gaim_find_buddys_group(buddy);
		if (buddy && group) {
			debug_printf("ssi: adding buddy %s to group %s\n", name, group->name);
			aim_ssi_addbuddy(od->sess, od->conn, buddy->name, group->name, gaim_get_buddy_alias_only(buddy), NULL, NULL, 0);
		}
	}
#endif
	if (od->icq)
		aim_icq_getalias(od->sess, name);
}

static void oscar_add_buddies(struct gaim_connection *gc, GList *buddies) {
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
#ifdef NOSSI
	char buf[MSG_LEN];
	int n=0;
	while (buddies) {
		if (n > MSG_LEN - 18) {
			aim_bos_setbuddylist(od->sess, od->conn, buf);
			n = 0;
		}
		n += g_snprintf(buf + n, sizeof(buf) - n, "%s&", (char *)buddies->data);
		buddies = buddies->next;
	}
	aim_bos_setbuddylist(od->sess, od->conn, buf);
#else
	if (od->sess->ssi.received_data) {
		while (buddies) {
			struct buddy *buddy = gaim_find_buddy(gc->account, (const char *)buddies->data);
			struct group *group = gaim_find_buddys_group(buddy);
			if (buddy && group) {
				debug_printf("ssi: adding buddy %s to group %s\n", (const char *)buddies->data, group->name);
				aim_ssi_addbuddy(od->sess, od->conn, buddy->name, group->name, gaim_get_buddy_alias_only(buddy), NULL, NULL, 0);
			}
			buddies = buddies->next;
		}
	}
#endif
}

static void oscar_remove_buddy(struct gaim_connection *gc, char *name, char *group) {
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
#ifdef NOSSI
	aim_remove_buddy(od->sess, od->conn, name);
#else
	if (od->sess->ssi.received_data) {
		debug_printf("ssi: deleting buddy %s from group %s\n", name, group);
		aim_ssi_delbuddy(od->sess, od->conn, name, group);
	}
#endif
}

static void oscar_remove_buddies(struct gaim_connection *gc, GList *buddies, const char *group) {
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
#ifdef NOSSI
	GList *cur;
	for (cur=buddies; cur; cur=cur->next)
		aim_remove_buddy(od->sess, od->conn, cur->data);
#else
	if (od->sess->ssi.received_data) {
		while (buddies) {
			debug_printf("ssi: deleting buddy %s from group %s\n", (char *)buddies->data, group);
			aim_ssi_delbuddy(od->sess, od->conn, buddies->data, group);
			buddies = buddies->next;
		}
	}
#endif
}

#ifndef NOSSI
static void oscar_move_buddy(struct gaim_connection *gc, const char *name, const char *old_group, const char *new_group) {
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	if (od->sess->ssi.received_data && strcmp(old_group, new_group)) {
		debug_printf("ssi: moving buddy %s from group %s to group %s\n", name, old_group, new_group);
		aim_ssi_movebuddy(od->sess, od->conn, old_group, new_group, name);
	}
}

static void oscar_alias_buddy(struct gaim_connection *gc, const char *name, const char *alias) {
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	if (od->sess->ssi.received_data) {
		char *gname = aim_ssi_itemlist_findparentname(od->sess->ssi.local, name);
		if (gname) {
			debug_printf("ssi: changing the alias for buddy %s to %s\n", name, alias);
			aim_ssi_aliasbuddy(od->sess, od->conn, gname, name, alias);
		}
	}
}

static void oscar_rename_group(struct gaim_connection *g, const char *old_group, const char *new_group, GList *members) {
	struct oscar_data *od = (struct oscar_data *)g->proto_data;

	if (od->sess->ssi.received_data) {
		if (aim_ssi_itemlist_finditem(od->sess->ssi.local, new_group, NULL, AIM_SSI_TYPE_GROUP)) {
			oscar_remove_buddies(g, members, old_group);
			oscar_add_buddies(g, members);
			debug_printf("ssi: moved all buddies from group %s to %s\n", old_group, new_group);
		} else {
			aim_ssi_rename_group(od->sess, od->conn, old_group, new_group);
			debug_printf("ssi: renamed group %s to %s\n", old_group, new_group);
		}
	}
}

static int gaim_ssi_parseerr(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = gc->proto_data;
	va_list ap;
	fu16_t reason;

	va_start(ap, fr);
	reason = (fu16_t)va_arg(ap, unsigned int);
	va_end(ap);

	debug_printf("ssi: SNAC error %hu\n", reason);

	if (reason == 0x0005) {
		do_error_dialog(_("Unable To Retrive Buddy List"), _("Gaim was temporarily unable to retrive your buddy list from the AIM servers.  Your buddy list is not lost, and will probably become available in a few hours."), GAIM_ERROR);
	}

	/* Activate SSI */
	/* Sending the enable causes other people to be able to see you, and you to see them */
	/* Make sure your privacy setting/invisibility is set how you want it before this! */
	debug_printf("ssi: activating server-stored buddy list\n");
	aim_ssi_enable(od->sess);

	return 1;
}

static int gaim_ssi_parserights(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	int numtypes, i;
	fu16_t *maxitems;
	va_list ap;

	va_start(ap, fr);
	numtypes = va_arg(ap, int);
	maxitems = va_arg(ap, fu16_t *);
	va_end(ap);

	debug_printf("ssi rights:");
	for (i=0; i<numtypes; i++)
		debug_printf(" max type 0x%04x=%hd,", i, maxitems[i]);
	debug_printf("\n");

	if (numtypes >= 0)
		od->rights.maxbuddies = maxitems[0];
	if (numtypes >= 1)
		od->rights.maxgroups = maxitems[1];
	if (numtypes >= 2)
		od->rights.maxpermits = maxitems[2];
	if (numtypes >= 3)
		od->rights.maxdenies = maxitems[3];

	return 1;
}

static int gaim_ssi_parselist(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	struct aim_ssi_item *curitem;
	int tmp;
	gboolean export = FALSE;
	/* XXX - use these?
	va_list ap;

	va_start(ap, fr);
	fmtver = (fu16_t)va_arg(ap, int);
	numitems = (fu16_t)va_arg(ap, int);
	items = va_arg(ap, struct aim_ssi_item);
	timestamp = va_arg(ap, fu32_t);
	va_end(ap); */

	debug_printf("ssi: syncing local list and server list\n");

	/* Clean the buddy list */
	aim_ssi_cleanlist(sess, fr->conn);

	/* Add from server list to local list */
	for (curitem=sess->ssi.local; curitem; curitem=curitem->next) {
		switch (curitem->type) {
			case 0x0000: { /* Buddy */
				if (curitem->name) {
					char *gname = aim_ssi_itemlist_findparentname(sess->ssi.local, curitem->name);
					char *gname_utf8 = gaim_try_conv_to_utf8(gname);
					char *alias = aim_ssi_getalias(sess->ssi.local, gname, curitem->name);
					char *alias_utf8 = gaim_try_conv_to_utf8(alias);
					struct buddy *buddy = gaim_find_buddy(gc->account, curitem->name);
					/* Should gname be freed here? -- elb */
					/* Not with the current code, but that might be cleaner -- med */
					free(alias);
					if (buddy) {
						/* Get server stored alias */
						if (alias_utf8) {
							g_free(buddy->alias);
							buddy->alias = g_strdup(alias_utf8);
						}
					} else {
						struct group *g;
						buddy = gaim_buddy_new(gc->account, curitem->name, alias_utf8);
						
						if (!(g = gaim_find_group(gname_utf8 ? gname_utf8 : _("Orphans")))) {
							g = gaim_group_new(gname_utf8 ? gname_utf8 : _("Orphans"));
							gaim_blist_add_group(g, NULL);
						}
						
						debug_printf("ssi: adding buddy %s to group %s to local list\n", curitem->name, gname);
						gaim_blist_add_buddy(buddy, g, NULL);
						export = TRUE;
					}
					free(gname_utf8);
					free(alias_utf8);
				}
			} break;

			case 0x0001: { /* Group */
				/* Shouldn't add empty groups */
			} break;

			case 0x0002: { /* Permit buddy */
				if (curitem->name) {
					/* if (!find_permdeny_by_name(gc->permit, curitem->name)) { AAA */
					GSList *list;
					for (list=gc->account->permit; (list && aim_sncmp(curitem->name, list->data)); list=list->next);
					if (!list) {
						debug_printf("ssi: adding permit buddy %s to local list\n", curitem->name);
						gaim_privacy_permit_add(gc->account, curitem->name);
						build_allow_list();
						export = TRUE;
					}
				}
			} break;

			case 0x0003: { /* Deny buddy */
				if (curitem->name) {
					GSList *list;
					for (list=gc->account->deny; (list && aim_sncmp(curitem->name, list->data)); list=list->next);
					if (!list) {
						debug_printf("ssi: adding deny buddy %s to local list\n", curitem->name);
						gaim_privacy_deny_add(gc->account, curitem->name);
						build_block_list();
						export = TRUE;
					}
				}
			} break;

			case 0x0004: { /* Permit/deny setting */
				if (curitem->data) {
					fu8_t permdeny;
					if ((permdeny = aim_ssi_getpermdeny(sess->ssi.local)) && (permdeny != gc->account->permdeny)) {
						debug_printf("ssi: changing permdeny from %d to %hhu\n", gc->account->permdeny, permdeny);
						gc->account->permdeny = permdeny;
						if (od->icq && gc->account->permdeny == 0x03) {
							serv_set_away(gc, "Invisible", "");
						}
						export = TRUE;
					}
				}
			} break;

			case 0x0005: { /* Presence setting */
				/* We don't want to change Gaim's setting because it applies to all accounts */
			} break;
		} /* End of switch on curitem->type */
	} /* End of for loop */

	/* If changes were made, then flush buddy list to file */
	if (export)
		gaim_blist_save();

	{ /* Add from local list to server list */
		GaimBlistNode *gnode, *bnode;
		struct group *group;
		struct buddy *buddy;
		struct gaim_buddy_list *blist;
		GSList *cur;

		/* Buddies */
		if ((blist = gaim_get_blist()))
			for (gnode = blist->root; gnode; gnode = gnode->next) {
				group = (struct group *)gnode;
				for (bnode = gnode->child; bnode; bnode = bnode->next) {
					buddy = (struct buddy *)bnode;
					if (buddy->account == gc->account) {
						gchar *servernick = gaim_buddy_get_setting(buddy, "servernick");
						if (servernick) {
							serv_got_alias(gc, buddy->name, servernick);
							g_free(servernick);
						}
						if (aim_ssi_itemlist_exists(sess->ssi.local, buddy->name)) {
							/* Store local alias on server */
							char *alias = aim_ssi_getalias(sess->ssi.local, group->name, buddy->name);
							if (!alias && buddy->alias && strlen(buddy->alias))
								aim_ssi_aliasbuddy(sess, od->conn, group->name, buddy->name, buddy->alias);
							free(alias);
						} else {
							debug_printf("ssi: adding buddy %s from local list to server list\n", buddy->name);
							aim_ssi_addbuddy(sess, od->conn, buddy->name, group->name, gaim_get_buddy_alias_only(buddy), NULL, NULL, 0);
						}
					}
				}
			}
		/* Permit list */
		if (gc->account->permit) {
			for (cur=gc->account->permit; cur; cur=cur->next)
				if (!aim_ssi_itemlist_finditem(sess->ssi.local, NULL, cur->data, AIM_SSI_TYPE_PERMIT)) {
					debug_printf("ssi: adding permit %s from local list to server list\n", (char *)cur->data);
					aim_ssi_addpermit(sess, od->conn, cur->data);
				}
		}

		/* Deny list */
		if (gc->account->deny) {
			for (cur=gc->account->deny; cur; cur=cur->next)
				if (!aim_ssi_itemlist_finditem(sess->ssi.local, NULL, cur->data, AIM_SSI_TYPE_DENY)) {
					debug_printf("ssi: adding deny %s from local list to server list\n", (char *)cur->data);
					aim_ssi_adddeny(sess, od->conn, cur->data);
				}
		}
		/* Presence settings (idle time visibility) */
		if ((tmp = aim_ssi_getpresence(sess->ssi.local)) != 0xFFFFFFFF)
			if (report_idle && !(tmp & 0x400))
				aim_ssi_setpresence(sess, fr->conn, tmp | 0x400);
	} /* end adding buddies from local list to server list */

	{ /* Check for maximum number of buddies */
		GaimBlistNode *gnode,*bnode;
		tmp = 0;
		for(gnode = gaim_get_blist()->root; gnode; gnode = gnode->next) {
			if(!GAIM_BLIST_NODE_IS_GROUP(gnode))
				continue;
			for(bnode = gnode->child; bnode; bnode = bnode->next) {
				struct buddy *b = (struct buddy *)bnode;
				if(!GAIM_BLIST_NODE_IS_BUDDY(bnode))
					continue;
				if(b->account == gc->account)
					tmp++;
			}
		}

		if (tmp > od->rights.maxbuddies) {
			char *dialog_msg = g_strdup_printf(_("The maximum number of buddies allowed in your buddy list is %d, and you have %d."
							     "  Until you are below the limit, some buddies will not show up as online."), 
							   od->rights.maxbuddies, tmp);
			do_error_dialog(_("Maximum buddy list length exceeded."), dialog_msg, GAIM_WARNING);
			g_free(dialog_msg);
		}
	}

	/* Activate SSI */
	/* Sending the enable causes other people to be able to see you, and you to see them */
	/* Make sure your privacy setting/invisibility is set how you want it before this! */
	debug_printf("ssi: activating server-stored buddy list\n");
	aim_ssi_enable(sess);

	return 1;
}

static int gaim_ssi_parseack(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct gaim_connection *gc = sess->aux_data;
	va_list ap;
	struct aim_ssi_tmp *retval;

	va_start(ap, fr);
	retval = va_arg(ap, struct aim_ssi_tmp *);
	va_end(ap);

	while (retval) {
		debug_printf("ssi: status is 0x%04hx for a 0x%04hx action with name %s\n", retval->ack,  retval->action, retval->item ? retval->item->name : "no item");

		if (retval->ack != 0xffff)
		switch (retval->ack) {
			case 0x0000: { /* added successfully */
			} break;

			case 0x000e: { /* contact requires authorization */
				if (retval->action == AIM_CB_SSI_ADD)
					gaim_auth_sendrequest(gc, retval->name);
			} break;

			default: { /* La la la */
				debug_printf("ssi: Action 0x%04hx was unsuccessful with error 0x%04hx\n", retval->action, retval->ack);
				/* Should remove buddy from local list and give an error message? */
			} break;
		}

		retval = retval->next;
	}

	return 1;
}

static int gaim_ssi_authgiven(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct gaim_connection *gc = sess->aux_data;
	va_list ap;
	char *sn, *msg;
	gchar *dialog_msg, *nombre;
	struct name_data *data;
	struct buddy *buddy;

	va_start(ap, fr);
	sn = va_arg(ap, char *);
	msg = va_arg(ap, char *);
	va_end(ap);

	debug_printf("ssi: %s has given you permission to add him to your buddy list\n", sn);

	buddy = gaim_find_buddy(gc->account, sn);
	if (buddy && (gaim_get_buddy_alias_only(buddy)))
		nombre = g_strdup_printf("%s (%s)", sn, gaim_get_buddy_alias_only(buddy));
	else
		nombre = g_strdup(sn);

	dialog_msg = g_strdup_printf(_("The user %s has given you permission to add you to their buddy list.  Do you want to add them?"), nombre);
	data = g_new(struct name_data, 1);
	data->gc = gc;
	data->name = g_strdup(sn);
	data->nick = NULL;
	do_ask_dialog(_("Authorization Given"), dialog_msg, data, _("Yes"), gaim_icq_contactadd, _("No"), gaim_free_name_data, my_protocol->plug ? my_protocol->plug->handle : NULL, FALSE);

	g_free(dialog_msg);
	g_free(nombre);

	return 1;
}

static int gaim_ssi_authrequest(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct gaim_connection *gc = sess->aux_data;
	va_list ap;
	char *sn, *msg;
	gchar *dialog_msg, *nombre;
	struct name_data *data;
	struct buddy *buddy;

	va_start(ap, fr);
	sn = va_arg(ap, char *);
	msg = va_arg(ap, char *);
	va_end(ap);

	debug_printf("ssi: received authorization request from %s\n", sn);

	buddy = gaim_find_buddy(gc->account, sn);
	if (buddy && (gaim_get_buddy_alias_only(buddy)))
		nombre = g_strdup_printf("%s (%s)", sn, gaim_get_buddy_alias_only(buddy));
	else
		nombre = g_strdup(sn);

	dialog_msg = g_strdup_printf(_("The user %s wants to add you to their buddy list for the following reason:\n%s"), nombre, msg ? msg : _("No reason given."));
	data = g_new(struct name_data, 1);
	data->gc = gc;
	data->name = g_strdup(sn);
	data->nick = NULL;
	do_ask_dialog(_("Authorization Request"), dialog_msg, data, _("Authorize"), gaim_auth_grant, _("Deny"), gaim_auth_dontgrant_msgprompt, my_protocol->plug ? my_protocol->plug->handle : NULL, FALSE);

	g_free(dialog_msg);
	g_free(nombre);

	return 1;
}

static int gaim_ssi_authreply(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct gaim_connection *gc = sess->aux_data;
	va_list ap;
	char *sn, *msg;
	gchar *dialog_msg, *nombre;
	fu8_t reply;
	struct buddy *buddy;

	va_start(ap, fr);
	sn = va_arg(ap, char *);
	reply = (fu8_t)va_arg(ap, int);
	msg = va_arg(ap, char *);
	va_end(ap);

	debug_printf("ssi: received authorization reply from %s.  Reply is 0x%04hhx\n", sn, reply);

	buddy = gaim_find_buddy(gc->account, sn);
	if (buddy && (gaim_get_buddy_alias_only(buddy)))
		nombre = g_strdup_printf("%s (%s)", sn, gaim_get_buddy_alias_only(buddy));
	else
		nombre = g_strdup(sn);

	if (reply) {
		/* Granted */
		dialog_msg = g_strdup_printf(_("The user %s has granted your request to add them to your contact list."), nombre);
		do_error_dialog(_("Authorization Granted"), dialog_msg, GAIM_INFO);
	} else {
		/* Denied */
		dialog_msg = g_strdup_printf(_("The user %s has denied your request to add them to your contact list for the following reason:\n%s"), nombre, msg ? msg : _("No reason given."));
		do_error_dialog(_("Authorization Denied"), dialog_msg, GAIM_INFO);
	}
	g_free(dialog_msg);
	g_free(nombre);

	return 1;
}

static int gaim_ssi_gotadded(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct gaim_connection *gc = sess->aux_data;
	va_list ap;
	char *sn;
	struct buddy *buddy;

	va_start(ap, fr);
	sn = va_arg(ap, char *);
	va_end(ap);

	buddy = gaim_find_buddy(gc->account, sn);
	debug_printf("ssi: %s added you to their buddy list\n", sn);
	show_got_added(gc, NULL, sn, (buddy ? gaim_get_buddy_alias_only(buddy) : NULL), NULL);

	return 1;
}
#endif

static GList *oscar_chat_info(struct gaim_connection *gc) {
	GList *m = NULL;
	struct proto_chat_entry *pce;

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("Join what group:");
	m = g_list_append(m, pce);

	pce = g_new0(struct proto_chat_entry, 1);
	pce->label = _("Exchange:");
	pce->is_int = TRUE;
	pce->min = 4;
	pce->max = 20;
	m = g_list_append(m, pce);

	return m;
}

static void oscar_join_chat(struct gaim_connection *g, GList *data) {
	struct oscar_data *od = (struct oscar_data *)g->proto_data;
	aim_conn_t *cur;
	char *name;
	int *exchange;

	if (!data || !data->next)
		return;

	name = data->data;
	exchange = data->next->data;

	debug_printf("Attempting to join chat room %s.\n", name);
	if ((cur = aim_getconn_type(od->sess, AIM_CONN_TYPE_CHATNAV))) {
		debug_printf("chatnav exists, creating room\n");
		aim_chatnav_createroom(od->sess, cur, name, *exchange);
	} else {
		/* this gets tricky */
		struct create_room *cr = g_new0(struct create_room, 1);
		debug_printf("chatnav does not exist, opening chatnav\n");
		cr->exchange = *exchange;
		cr->name = g_strdup(name);
		od->create_rooms = g_slist_append(od->create_rooms, cr);
		aim_reqservice(od->sess, od->conn, AIM_CONN_TYPE_CHATNAV);
	}
}

static void oscar_chat_invite(struct gaim_connection *g, int id, const char *message, const char *name) {
	struct oscar_data *od = (struct oscar_data *)g->proto_data;
	struct chat_connection *ccon = find_oscar_chat(g, id);
	
	if (!ccon)
		return;
	
	aim_chat_invite(od->sess, od->conn, name, message ? message : "",
			ccon->exchange, ccon->name, 0x0);
}

static void oscar_chat_leave(struct gaim_connection *g, int id) {
	struct oscar_data *od = g ? (struct oscar_data *)g->proto_data : NULL;
	GSList *bcs = g->buddy_chats;
	struct gaim_conversation *b = NULL;
	struct chat_connection *c = NULL;
	int count = 0;

	while (bcs) {
		count++;
		b = (struct gaim_conversation *)bcs->data;
		if (id == gaim_chat_get_id(GAIM_CHAT(b)))
			break;
		bcs = bcs->next;
		b = NULL;
	}

	if (!b)
		return;

	debug_printf("Attempting to leave room %s (currently in %d rooms)\n", b->name, count);
	
	c = find_oscar_chat(g, gaim_chat_get_id(GAIM_CHAT(b)));
	if (c != NULL) {
		if (od)
			od->oscar_chats = g_slist_remove(od->oscar_chats, c);
		if (c->inpa > 0)
			gaim_input_remove(c->inpa);
		if (g && od->sess)
			aim_conn_kill(od->sess, &c->conn);
		g_free(c->name);
		g_free(c->show);
		g_free(c);
	}
	/* we do this because with Oscar it doesn't tell us we left */
	serv_got_chat_left(g, gaim_chat_get_id(GAIM_CHAT(b)));
}

static int oscar_chat_send(struct gaim_connection *g, int id, char *message) {
	struct oscar_data *od = (struct oscar_data *)g->proto_data;
	GSList *bcs = g->buddy_chats;
	struct gaim_conversation *b = NULL;
	struct chat_connection *c = NULL;
	char *buf, *buf2;
	int i, j;

	while (bcs) {
		b = (struct gaim_conversation *)bcs->data;
		if (id == gaim_chat_get_id(GAIM_CHAT(b)))
			break;
		bcs = bcs->next;
		b = NULL;
	}
	if (!b)
		return -EINVAL;

	bcs = od->oscar_chats;
	while (bcs) {
		c = (struct chat_connection *)bcs->data;
		if (b == c->cnv)
			break;
		bcs = bcs->next;
		c = NULL;
	}
	if (!c)
		return -EINVAL;

	buf = g_malloc(strlen(message) * 4 + 1);
	for (i = 0, j = 0; i < strlen(message); i++) {
		if (message[i] == '\n') {
			buf[j++] = '<';
			buf[j++] = 'B';
			buf[j++] = 'R';
			buf[j++] = '>';
		} else {
			buf[j++] = message[i];
		}
	}
	buf[j] = '\0';

	if (strlen(buf) > c->maxlen)
		return -E2BIG;

	buf2 = strip_html(buf);
	if (strlen(buf2) > c->maxvis) {
		g_free(buf2);
		return -E2BIG;
	}
	g_free(buf2);

	aim_chat_send_im(od->sess, c->conn, 0, buf, strlen(buf));
	g_free(buf);
	return 0;
}

static const char *oscar_list_icon(struct gaim_account *a, struct buddy *b) {
	if (!b || (b && b->name && b->name[0] == '+')) {
		if (isdigit(a->username[0]))
			return "icq";
		else
			return "aim";
	}
				
	if (isdigit(b->name[0]))
		return "icq";
	return "aim";
}

static void oscar_list_emblems(struct buddy *b, char **se, char **sw, char **nw, char **ne)
{
	char *emblems[4] = {NULL,NULL,NULL,NULL};
	int i = 0;

	if (b->name && (b->uc & 0xffff0000) && isdigit(b->name[0])) {
/*		int uc = b->uc >> 16;
		if (uc & AIM_ICQ_STATE_INVISIBLE)
			emblems[i++] = "icq_invisible";
		else if (uc & AIM_ICQ_STATE_CHAT)
			emblems[i++] = "icq_chat";
		else if (uc & AIM_ICQ_STATE_DND)
			emblems[i++] = "icq_dnd";
		else if (uc & AIM_ICQ_STATE_OUT)
			emblems[i++] = "icq_out";
		else if (uc & AIM_ICQ_STATE_BUSY)
			emblems[i++] = "icq_busy";
		else if (uc & AIM_ICQ_STATE_AWAY)
			emblems[i++] = "icq_away";
*/
		if (b->uc & UC_UNAVAILABLE) 
			emblems[i++] = "away";
	} else {
		if (b->uc & UC_UNAVAILABLE) 
			emblems[i++] = "away";
	}
	if (b->uc & UC_WIRELESS)
		emblems[i++] = "wireless";
	if (b->uc & UC_AOL)
		emblems[i++] = "aol";
	if (b->uc & UC_ADMIN)
		emblems[i++] = "admin";
	if (b->uc & UC_AB && i < 4)
		emblems[i++] = "activebuddy";
/*	if (b->uc & UC_UNCONFIRMED && i < 4)
		emblems[i++] = "unconfirmed"; */
	*se = emblems[0];
	*sw = emblems[1];
	*nw = emblems[2];
	*ne = emblems[3];
}

/*
 * We have just established a socket with the other dude, so set up some handlers.
 */
static int gaim_odc_initiate(aim_session_t *sess, aim_frame_t *fr, ...) {
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	struct gaim_conversation *cnv;
	struct direct_im *dim;
	char buf[256];
	char *sn;
	va_list ap;
	aim_conn_t *newconn, *listenerconn;

	va_start(ap, fr);
	newconn = va_arg(ap, aim_conn_t *);
	listenerconn = va_arg(ap, aim_conn_t *);
	va_end(ap);

	aim_conn_close(listenerconn);
	aim_conn_kill(sess, &listenerconn);

	sn = g_strdup(aim_odc_getsn(newconn));

	debug_printf("DirectIM: initiate success to %s\n", sn);
	dim = find_direct_im(od, sn);

	if (!(cnv = gaim_find_conversation(sn)))
		cnv = gaim_conversation_new(GAIM_CONV_IM, dim->gc->account, sn);
	gaim_input_remove(dim->watcher);
	dim->conn = newconn;
	dim->watcher = gaim_input_add(dim->conn->fd, GAIM_INPUT_READ, oscar_callback, dim->conn);
	dim->connected = TRUE;
	g_snprintf(buf, sizeof buf, _("Direct IM with %s established"), sn);
	g_free(sn);
	gaim_conversation_write(cnv, NULL, buf, -1, WFLAG_SYSTEM, time(NULL));

	aim_conn_addhandler(sess, newconn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIMINCOMING, gaim_odc_incoming, 0);
	aim_conn_addhandler(sess, newconn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIMTYPING, gaim_odc_typing, 0);
	aim_conn_addhandler(sess, newconn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_IMAGETRANSFER, gaim_update_ui, 0);

	return 1;
}

static int gaim_update_ui(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	char *sn;
	double percent;
	struct gaim_connection *gc = sess->aux_data;
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	struct gaim_conversation *c;
	struct direct_im *dim;

	va_start(ap, fr);
	sn = va_arg(ap, char *);
	percent = va_arg(ap, double);
	va_end(ap);
	
	if (!(dim = find_direct_im(od, sn)))
		return 1;
	if (dim->watcher) {
		gaim_input_remove(dim->watcher);   /* Otherwise, the callback will callback */
		dim->watcher = 0;
	}
	while (gtk_events_pending())
		gtk_main_iteration();
	
	if ((c = gaim_find_conversation(sn)))
		gaim_conversation_update_progress(c, percent);
	dim->watcher = gaim_input_add(dim->conn->fd, GAIM_INPUT_READ,
				      oscar_callback, dim->conn);

	return 1;
}

static int gaim_odc_incoming(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	char *msg, *sn;
	int len, encoding;
	struct gaim_connection *gc = sess->aux_data;

	va_start(ap, fr);
	sn = va_arg(ap, char *);
	msg = va_arg(ap, char *);
	len = va_arg(ap, int);
	encoding = va_arg(ap, int);
	va_end(ap);

	debug_printf("Got DirectIM message from %s\n", sn);

	/* XXX - I imagine Paco-Paco will want to do some voodoo with the encoding here */
	serv_got_im(gc, sn, msg, 0, time(NULL), len);

	return 1;
}

static int gaim_odc_typing(aim_session_t *sess, aim_frame_t *fr, ...) {
	va_list ap;
	char *sn;
	int typing;
	struct gaim_connection *gc = sess->aux_data;

	va_start(ap, fr);
	sn = va_arg(ap, char *);
	typing = va_arg(ap, int);
	va_end(ap);

	if (typing) {
		/* I had to leave this. It's just too funny. It reminds me of my sister. */
		debug_printf("ohmigod! %s has started typing (DirectIM). He's going to send you a message! *squeal*\n", sn);
		serv_got_typing(gc, sn, 0, TYPING);
	} else
		serv_got_typing_stopped(gc, sn);
	return 1;
}

struct ask_do_dir_im {
	char *who;
	struct gaim_connection *gc;
};

static void oscar_cancel_direct_im(struct ask_do_dir_im *data) {
	g_free(data);
}

static void oscar_direct_im(struct ask_do_dir_im *data) {
	struct gaim_connection *gc = data->gc;
	struct oscar_data *od;
	struct direct_im *dim;

	if (!g_slist_find(connections, gc)) {
		g_free(data);
		return;
	}

	od = (struct oscar_data *)gc->proto_data;

	dim = find_direct_im(od, data->who);
	if (dim) {
		if (!(dim->connected)) {  /* We'll free the old, unconnected dim, and start over */
			od->direct_ims = g_slist_remove(od->direct_ims, dim);
			gaim_input_remove(dim->watcher);
			g_free(dim);
			debug_printf("Gave up on old direct IM, trying again\n");
		} else {
			do_error_dialog("DirectIM already open.", NULL, GAIM_ERROR);
			g_free(data);
			return;
		}
	}
	dim = g_new0(struct direct_im, 1);
	dim->gc = gc;
	g_snprintf(dim->name, sizeof dim->name, "%s", data->who);

	dim->conn = aim_odc_initiate(od->sess, data->who);
	if (dim->conn != NULL) {
		od->direct_ims = g_slist_append(od->direct_ims, dim);
		dim->watcher = gaim_input_add(dim->conn->fd, GAIM_INPUT_READ,
						oscar_callback, dim->conn);
		aim_conn_addhandler(od->sess, dim->conn, AIM_CB_FAM_OFT, AIM_CB_OFT_DIRECTIM_ESTABLISHED,
					gaim_odc_initiate, 0);
	} else {
		do_error_dialog(_("Unable to open Direct IM"), NULL, GAIM_ERROR);
		g_free(dim);
	}

	g_free(data);
}

static void oscar_ask_direct_im(struct gaim_connection *gc, gchar *who) {
	char buf[BUF_LONG];
	struct ask_do_dir_im *data = g_new0(struct ask_do_dir_im, 1);
	data->who = who;
	data->gc = gc;
	g_snprintf(buf, sizeof(buf),  _("You have selected to open a Direct IM connection with %s."), who);
	do_ask_dialog(buf, _("Because this reveals your IP address, it may be considered a privacy risk.  Do you wish to continue?"), data, _("Connect"), oscar_direct_im, _("Cancel"), oscar_cancel_direct_im, my_protocol->plug ? my_protocol->plug->handle : NULL, FALSE);
}

static void oscar_get_away_msg(struct gaim_connection *gc, char *who) {
	struct oscar_data *od = gc->proto_data;
	od->evilhack = g_slist_append(od->evilhack, g_strdup(normalize(who)));
	if (od->icq) {
		struct buddy *budlight = gaim_find_buddy(gc->account, who);
		if (budlight)
			if ((budlight->uc >> 16) & (AIM_ICQ_STATE_AWAY || AIM_ICQ_STATE_DND || AIM_ICQ_STATE_OUT || AIM_ICQ_STATE_BUSY || AIM_ICQ_STATE_CHAT)) {
					char *state_msg = gaim_icq_status((budlight->uc & 0xffff0000) >> 16);
					char *dialog_msg = g_strdup_printf(_("<B>UIN:</B> %s<BR><B>Status:</B> %s<HR><I>Remote client does not support sending status messages.</I><BR>"), who, state_msg);
					g_show_info_text(gc, who, 2, dialog_msg, NULL);
					free(state_msg);
					free(dialog_msg);
				}
			else {
				char *state_msg = gaim_icq_status((budlight->uc & 0xffff0000) >> 16);
				char *dialog_msg = g_strdup_printf(_("<B>UIN:</B> %s<BR><B>Status:</B> %s<HR><I>User has no status message.</I><BR>"), who, state_msg);
				g_show_info_text(gc, who, 2, dialog_msg, NULL);
				free(state_msg);
				free(dialog_msg);
			}
		else
			do_error_dialog("Could not find contact in local list, therefore unable to request status message.\n", NULL, GAIM_ERROR);
	} else
		oscar_get_info(gc, who);
}

static void oscar_set_permit_deny(struct gaim_connection *gc) {
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
#ifdef NOSSI
	GSList *list, *g = gaim_blist_groups(), *g1;
	char buf[MAXMSGLEN];
	int at;

	switch(gc->account->permdeny) {
	case 1: 
		aim_bos_changevisibility(od->sess, od->conn, AIM_VISIBILITYCHANGE_DENYADD, gc->username);
		break;
	case 2:
		aim_bos_changevisibility(od->sess, od->conn, AIM_VISIBILITYCHANGE_PERMITADD, gc->username);
		break;
	case 3:
		list = gc->account->permit;
		at = 0;
		while (list) {
			at += g_snprintf(buf + at, sizeof(buf) - at, "%s&", (char *)list->data);
			list = list->next;
		}
		aim_bos_changevisibility(od->sess, od->conn, AIM_VISIBILITYCHANGE_PERMITADD, buf);
		break;
	case 4:
		list = gc->account->deny;
		at = 0;
		while (list) {
			at += g_snprintf(buf + at, sizeof(buf) - at, "%s&", (char *)list->data);
			list = list->next;
		}
		aim_bos_changevisibility(od->sess, od->conn, AIM_VISIBILITYCHANGE_DENYADD, buf);
		break;
	case 5:
		g1 = g;
		at = 0;
		while (g1) {
			list = gaim_blist_members((struct group *)g->data);
			GSList list1 = list;
			while (list1) {
				struct buddy *b = list1->data;
				if(b->account == gc->account)
					at += g_snprintf(buf + at, sizeof(buf) - at, "%s&", b->name);
				list1 = list1->next;
			}
			g_slist_free(list);
			g1 = g1->next;
		}
		g_slist_free(g);
		aim_bos_changevisibility(od->sess, od->conn, AIM_VISIBILITYCHANGE_PERMITADD, buf);
		break;
	default:
		break;
	}
	signoff_blocked(gc);
#else
	if (od->sess->ssi.received_data)
		aim_ssi_setpermdeny(od->sess, od->conn, gc->account->permdeny, 0xffffffff);
#endif
}

static void oscar_add_permit(struct gaim_connection *gc, const char *who) {
#ifdef NOSSI
	if (gc->account->permdeny == 3)
		oscar_set_permit_deny(gc);
#else
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	debug_printf("ssi: About to add a permit\n");
	if (od->sess->ssi.received_data)
		aim_ssi_addpermit(od->sess, od->conn, who);
#endif
}

static void oscar_add_deny(struct gaim_connection *gc, const char *who) {
#ifdef NOSSI
	if (gc->account->permdeny == 4)
		oscar_set_permit_deny(gc);
#else
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	debug_printf("ssi: About to add a deny\n");
	if (od->sess->ssi.received_data)
		aim_ssi_adddeny(od->sess, od->conn, who);
#endif
}

static void oscar_rem_permit(struct gaim_connection *gc, const char *who) {
#ifdef NOSSI
	if (gc->account->permdeny == 3)
		oscar_set_permit_deny(gc);
#else
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	debug_printf("ssi: About to delete a permit\n");
	if (od->sess->ssi.received_data)
		aim_ssi_delpermit(od->sess, od->conn, who);
#endif
}

static void oscar_rem_deny(struct gaim_connection *gc, const char *who) {
#ifdef NOSSI
	if (gc->account->permdeny == 4)
		oscar_set_permit_deny(gc);
#else
	struct oscar_data *od = (struct oscar_data *)gc->proto_data;
	debug_printf("ssi: About to delete a deny\n");
	if (od->sess->ssi.received_data)
		aim_ssi_deldeny(od->sess, od->conn, who);
#endif
}

static GList *oscar_away_states(struct gaim_connection *gc)
{
	struct oscar_data *od = gc->proto_data;
	GList *m = NULL;

	if (!od->icq)
		return g_list_append(m, GAIM_AWAY_CUSTOM);

	m = g_list_append(m, _("Online"));
	m = g_list_append(m, _("Away"));
	m = g_list_append(m, _("Do Not Disturb"));
	m = g_list_append(m, _("Not Available"));
	m = g_list_append(m, _("Occupied"));
	m = g_list_append(m, _("Free For Chat"));
	m = g_list_append(m, _("Invisible"));

	return m;
}

static GList *oscar_buddy_menu(struct gaim_connection *gc, char *who) {
	struct oscar_data *od = gc->proto_data;
	GList *m = NULL;
	struct proto_buddy_menu *pbm;

	if (od->icq) {
#if 0
		pbm = g_new0(struct proto_buddy_menu, 1);
		pbm->label = _("Get Status Msg");
		pbm->callback = oscar_get_away_msg;
		pbm->gc = gc;
		m = g_list_append(m, pbm);
#endif
	} else {
		struct buddy *b = gaim_find_buddy(gc->account, who);

		if (!b || (b->uc & UC_UNAVAILABLE)) {
			pbm = g_new0(struct proto_buddy_menu, 1);
			pbm->label = _("Get Away Msg");
			pbm->callback = oscar_get_away_msg;
			pbm->gc = gc;
			m = g_list_append(m, pbm);
		}

		if (aim_sncmp(gc->username, who)) {
			pbm = g_new0(struct proto_buddy_menu, 1);
			pbm->label = _("Direct IM");
			pbm->callback = oscar_ask_direct_im;
			pbm->gc = gc;
			m = g_list_append(m, pbm);

			pbm = g_new0(struct proto_buddy_menu, 1);
			pbm->label = _("Send File");
			pbm->callback = oscar_ask_sendfile;
			pbm->gc = gc;
			m = g_list_append(m, pbm);

#if 0
			pbm = g_new0(struct proto_buddy_menu, 1);
			pbm->label = _("Get File");
			pbm->callback = oscar_ask_getfile;
			pbm->gc = gc;
			m = g_list_append(m, pbm);
#endif
		}
	}

#if 0
	pbm = g_new0(struct proto_buddy_menu, 1);
	pbm->label = _("Get Capabilities");
	pbm->callback = oscar_get_caps;
	pbm->gc = gc;
	m = g_list_append(m, pbm);
#endif

	return m;
}

static GList *oscar_edit_buddy_menu(struct gaim_connection *gc, char *who)
{
	struct oscar_data *od = gc->proto_data;
	GList *m = NULL;
	struct proto_buddy_menu *pbm;

	if (od->icq) {
		pbm = g_new0(struct proto_buddy_menu, 1);
		pbm->label = _("Get Info");
		pbm->callback = oscar_get_info;
		pbm->gc = gc;
		m = g_list_append(m, pbm);
	}

	if (od->sess->ssi.received_data) {
		char *gname = aim_ssi_itemlist_findparentname(od->sess->ssi.local, who);
		if (gname && aim_ssi_waitingforauth(od->sess->ssi.local, gname, who)) {
			pbm = g_new0(struct proto_buddy_menu, 1);
			pbm->label = _("Re-request Authorization");
			pbm->callback = gaim_auth_sendrequest;
			pbm->gc = gc;
			m = g_list_append(m, pbm);
		}
	}

	return m;
}

static void oscar_format_screenname(struct gaim_connection *gc, char *nick) {
	struct oscar_data *od = gc->proto_data;
	if (!aim_sncmp(gc->username, nick)) {
		if (!aim_getconn_type(od->sess, AIM_CONN_TYPE_AUTH)) {
			od->setnick = TRUE;
			od->newsn = g_strdup(nick);
			aim_reqservice(od->sess, od->conn, AIM_CONN_TYPE_AUTH);
		} else {
			aim_admin_setnick(od->sess, aim_getconn_type(od->sess, AIM_CONN_TYPE_AUTH), nick);
		}
	} else {
		do_error_dialog(_("The new formatting is invalid."),
				_("Screenname formatting can change only capitalization and whitespace."), GAIM_ERROR);
	}
}

static void oscar_show_format_screenname(struct gaim_connection *gc)
{
	do_prompt_dialog(_("New screenname formatting:"), gc->displayname, gc, oscar_format_screenname, NULL);
}

static void oscar_confirm_account(struct gaim_connection *gc)
{
	struct oscar_data *od = gc->proto_data;
	aim_conn_t *conn = aim_getconn_type(od->sess, AIM_CONN_TYPE_AUTH);

	if (conn) {
		aim_admin_reqconfirm(od->sess, conn);
	} else {
		od->conf = TRUE;
		aim_reqservice(od->sess, od->conn, AIM_CONN_TYPE_AUTH);
	}
}

static void oscar_show_email(struct gaim_connection *gc)
{
	struct oscar_data *od = gc->proto_data;
	aim_conn_t *conn = aim_getconn_type(od->sess, AIM_CONN_TYPE_AUTH);

	if (conn) {
		aim_admin_getinfo(od->sess, conn, 0x11);
	} else {
		od->reqemail = TRUE;
		aim_reqservice(od->sess, od->conn, AIM_CONN_TYPE_AUTH);
	}
}

static void oscar_change_email(struct gaim_connection *gc, char *email)
{
	struct oscar_data *od = gc->proto_data;
	aim_conn_t *conn = aim_getconn_type(od->sess, AIM_CONN_TYPE_AUTH);

	if (conn) {
		aim_admin_setemail(od->sess, conn, email);
	} else {
		od->setemail = TRUE;
		od->email = g_strdup(email);
		aim_reqservice(od->sess, od->conn, AIM_CONN_TYPE_AUTH);
	}
}

static void oscar_show_change_email(struct gaim_connection *gc)
{
	do_prompt_dialog(_("Change Address To: "), NULL, gc, oscar_change_email, NULL);
}

static void oscar_show_awaitingauth(struct gaim_connection *gc)
{
	struct oscar_data *od = gc->proto_data;
	gchar *nombre, *text, *tmp;
	GaimBlistNode *gnode,*bnode;
	int num=0;

	text = g_strdup(_("You are awaiting authorization from the following buddies:<BR>"));

	for (gnode = gaim_get_blist()->root; gnode; gnode = gnode->next) {
		struct group *group = (struct group *)gnode;
		if(!GAIM_BLIST_NODE_IS_GROUP(gnode))
			continue;
		for (bnode = gnode->child; bnode; bnode = bnode->next) {
			struct buddy *buddy = (struct buddy *)bnode;
			if(!GAIM_BLIST_NODE_IS_BUDDY(bnode))
				continue;
			if (buddy->account == gc->account && aim_ssi_waitingforauth(od->sess->ssi.local, group->name, buddy->name)) {
				if (gaim_get_buddy_alias_only(buddy))
					nombre = g_strdup_printf(" %s (%s)", buddy->name, gaim_get_buddy_alias_only(buddy));
				else
					nombre = g_strdup_printf(" %s", buddy->name);
				tmp = g_strdup_printf("%s<BR>%s", text, nombre);
				g_free(text);
				text = tmp;
				g_free(nombre);
				num++;
			}
		}
	}

	if (!num) {
		tmp = g_strdup_printf("%s<BR>%s", text, _("<i>you are not waiting for authorization</i>"));
		g_free(text);
		text = tmp;
	}

	tmp = g_strdup_printf(_("%s<BR><BR>You can re-request authorization from these buddies by right-clicking on them in the \"Edit Buddies\" pane and selecting \"Re-request authorization.\""), text);
	g_free(text);
	text = tmp;
	g_show_info_text(gc, gc->username, 2, text, NULL);
	g_free(text);
}

static void oscar_show_chpassurl(struct gaim_connection *gc)
{
	struct oscar_data *od = gc->proto_data;
	open_url(NULL, od->sess->authinfo->chpassurl);
}

static GList *oscar_actions(struct gaim_connection *gc)
{
	struct oscar_data *od = gc->proto_data;
	struct proto_actions_menu *pam;
	GList *m = NULL;

	pam = g_new0(struct proto_actions_menu, 1);
	pam->label = _("Set User Info");
	pam->callback = show_set_info;
	pam->gc = gc;
	m = g_list_append(m, pam);

	if ((od->sess->authinfo->regstatus == 0x0003) || (od->icq)) {
		pam = g_new0(struct proto_actions_menu, 1);
		pam->label = _("Change Password");
		pam->callback = show_change_passwd;
		pam->gc = gc;
		m = g_list_append(m, pam);
	}

	if (od->sess->authinfo->chpassurl) {
		pam = g_new0(struct proto_actions_menu, 1);
		pam->label = _("Change Password (URL)");
		pam->callback = oscar_show_chpassurl;
		pam->gc = gc;
		m = g_list_append(m, pam);
	}

	if (od->sess->authinfo->regstatus == 0x0003) {
		/* AIM actions */
		m = g_list_append(m, NULL);

		pam = g_new0(struct proto_actions_menu, 1);
		pam->label = _("Format Screenname");
		pam->callback = oscar_show_format_screenname;
		pam->gc = gc;
		m = g_list_append(m, pam);

		pam = g_new0(struct proto_actions_menu, 1);
		pam->label = _("Confirm Account");
		pam->callback = oscar_confirm_account;
		pam->gc = gc;
		m = g_list_append(m, pam);

		pam = g_new0(struct proto_actions_menu, 1);
		pam->label = _("Display Current Registered Address");
		pam->callback = oscar_show_email;
		pam->gc = gc;
		m = g_list_append(m, pam);

		pam = g_new0(struct proto_actions_menu, 1);
		pam->label = _("Change Current Registered Address");
		pam->callback = oscar_show_change_email;
		pam->gc = gc;
		m = g_list_append(m, pam);
	}

	m = g_list_append(m, NULL);

	pam = g_new0(struct proto_actions_menu, 1);
	pam->label = _("Show Buddies Awaiting Authorization");
	pam->callback = oscar_show_awaitingauth;
	pam->gc = gc;
	m = g_list_append(m, pam);

	m = g_list_append(m, NULL);

	pam = g_new0(struct proto_actions_menu, 1);
	pam->label = _("Search for Buddy by Email");
	pam->callback = show_find_email;
	pam->gc = gc;
	m = g_list_append(m, pam);

/*	pam = g_new0(struct proto_actions_menu, 1);
	pam->label = _("Search for Buddy by Information");
	pam->callback = show_find_info;
	pam->gc = gc;
	m = g_list_append(m, pam); */

	return m;
}

static void oscar_change_passwd(struct gaim_connection *gc, const char *old, const char *new)
{
	struct oscar_data *od = gc->proto_data;

	if (od->icq) {
		aim_icq_changepasswd(od->sess, new);
	} else {
		aim_conn_t *conn = aim_getconn_type(od->sess, AIM_CONN_TYPE_AUTH);
		if (conn) {
			aim_admin_changepasswd(od->sess, conn, new, old);
		} else {
			od->chpass = TRUE;
			od->oldp = g_strdup(old);
			od->newp = g_strdup(new);
			aim_reqservice(od->sess, od->conn, AIM_CONN_TYPE_AUTH);
		}
	}
}

static void oscar_convo_closed(struct gaim_connection *gc, char *who)
{
	struct oscar_data *od = gc->proto_data;
	struct direct_im *dim = find_direct_im(od, who);

	if (!dim)
		return;

	od->direct_ims = g_slist_remove(od->direct_ims, dim);
	gaim_input_remove(dim->watcher);
	aim_conn_kill(od->sess, &dim->conn);
	g_free(dim);
}

static fu32_t check_encoding(const char *utf8)
{
	int i = 0;
	fu32_t encodingflag = 0;

	/* Determine how we can send this message.  Per the
	 * warnings elsewhere in this file, these little
	 * checks determine the simplest encoding we can use
	 * for a given message send using it. */
	while (utf8[i]) {
		if ((unsigned char)utf8[i] > 0x7f) {
			/* not ASCII! */
			encodingflag = AIM_IMFLAGS_ISO_8859_1;
			break;
		}
		i++;
	}
	while (utf8[i]) {
		/* ISO-8859-1 is 0x00-0xbf in the first byte
		 * followed by 0xc0-0xc3 in the second */
		if ((unsigned char)utf8[i] < 0x80) {
			i++;
			continue;
		} else if (((unsigned char)utf8[i] & 0xfc) == 0xc0 &&
			   ((unsigned char)utf8[i + 1] & 0xc0) == 0x80) {
			i += 2;
			continue;
		}
		encodingflag = AIM_IMFLAGS_UNICODE;
		break;
	}

	return encodingflag;
}

static fu32_t parse_encoding(const char *enc)
{
	char *charset;

	/* If anything goes wrong, fall back on ASCII and print a message */
	if (enc == NULL) {
		debug_printf("Encoding was null, that's odd\n");
		return 0;
	}
	charset = strstr(enc, "charset=");
	if (charset == NULL) {
		debug_printf("No charset specified for info, assuming ASCII\n");
		return 0;
	}
	charset += 8;
	if (!strcmp(charset, "\"us-ascii\"") || !strcmp(charset, "\"utf-8\"")) {
		/* UTF-8 is our native charset, ASCII is a proper subset */
		return 0;
	} else if (!strcmp(charset, "\"iso-8859-1\"")) {
		return AIM_IMFLAGS_ISO_8859_1;
	} else if (!strcmp(charset, "\"unicode-2-0\"")) {
		return AIM_IMFLAGS_UNICODE;
	} else {
		debug_printf("Unrecognized character set '%s', using ASCII\n", charset);
		return 0;
	}
}

G_MODULE_EXPORT void oscar_init(struct prpl *ret) {
	struct proto_user_opt *puo;
	ret->protocol = PROTO_OSCAR;
	ret->options = OPT_PROTO_MAIL_CHECK | OPT_PROTO_BUDDY_ICON | OPT_PROTO_IM_IMAGE;
	ret->name = g_strdup("AIM/ICQ");
	ret->list_icon = oscar_list_icon;
	ret->list_emblems = oscar_list_emblems;
	ret->away_states = oscar_away_states;
	ret->actions = oscar_actions;
	ret->buddy_menu = oscar_buddy_menu;
	ret->edit_buddy_menu = oscar_edit_buddy_menu;
	ret->login = oscar_login;
	ret->close = oscar_close;
	ret->send_im = oscar_send_im;
	ret->send_typing = oscar_send_typing;
	ret->set_info = oscar_set_info;
	ret->get_info = oscar_get_info;
	ret->set_away = oscar_set_away;
	ret->get_away = oscar_get_away;
	ret->set_dir = oscar_set_dir;
	ret->get_dir = NULL; /* Oscar really doesn't have this */
	ret->dir_search = oscar_dir_search;
	ret->set_idle = oscar_set_idle;
	ret->change_passwd = oscar_change_passwd;
	ret->add_buddy = oscar_add_buddy;
	ret->add_buddies = oscar_add_buddies;
	ret->remove_buddy = oscar_remove_buddy;
	ret->remove_buddies = oscar_remove_buddies;
#ifndef NOSSI
	ret->group_buddy = oscar_move_buddy;
	ret->alias_buddy = oscar_alias_buddy;
	ret->rename_group = oscar_rename_group;
#endif
	ret->add_permit = oscar_add_permit;
	ret->add_deny = oscar_add_deny;
	ret->rem_permit = oscar_rem_permit;
	ret->rem_deny = oscar_rem_deny;
	ret->set_permit_deny = oscar_set_permit_deny;

	ret->warn = oscar_warn;
	ret->chat_info = oscar_chat_info;
	ret->join_chat = oscar_join_chat;
	ret->chat_invite = oscar_chat_invite;
	ret->chat_leave = oscar_chat_leave;
	ret->chat_whisper = NULL;
	ret->chat_send = oscar_chat_send;
	ret->keepalive = oscar_keepalive;
	ret->convo_closed = oscar_convo_closed;
	ret->tooltip_text = oscar_tooltip_text;

	puo = g_new0(struct proto_user_opt, 1);
	puo->label = g_strdup("Auth Host:");
	puo->def = g_strdup("login.oscar.aol.com");
	puo->pos = USEROPT_AUTH;
	ret->user_opts = g_list_append(ret->user_opts, puo);

	puo = g_new0(struct proto_user_opt, 1);
	puo->label = g_strdup("Auth Port:");
	puo->def = g_strdup("5190");
	puo->pos = USEROPT_AUTHPORT;
	ret->user_opts = g_list_append(ret->user_opts, puo);

	my_protocol = ret;
}

#ifndef STATIC

G_MODULE_EXPORT void gaim_prpl_init(struct prpl *prpl)
{
	oscar_init(prpl);
	prpl->plug->desc.api_version = PLUGIN_API_VERSION;
}

#endif
