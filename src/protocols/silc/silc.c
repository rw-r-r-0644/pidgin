/*

  silcgaim.c

  Author: Pekka Riikonen <priikone@silcnet.org>

  Copyright (C) 2004 Pekka Riikonen

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

*/

#include "silcincludes.h"
#include "silcclient.h"
#include "silcgaim.h"

extern SilcClientOperations ops;
static GaimPlugin *silc_plugin = NULL;

static const char *
silcgaim_list_icon(GaimAccount *a, GaimBuddy *b)
{
	return (const char *)"silc";
}

static void
silcgaim_list_emblems(GaimBuddy *b, char **se, char **sw,
		      char **nw, char **ne)
{
}

static GList *
silcgaim_away_states(GaimConnection *gc)
{
	GList *st = NULL;

	st = g_list_append(st, _("Online"));
	st = g_list_append(st, _("Hyper Active"));
	st = g_list_append(st, _("Away"));
	st = g_list_append(st, _("Busy"));
	st = g_list_append(st, _("Indisposed"));
	st = g_list_append(st, _("Wake Me Up"));

	return st;
}

static void
silcgaim_set_away(GaimConnection *gc, const char *state, const char *msg)
{
	SilcGaim sg = gc->proto_data;
	SilcUInt32 mode;
	SilcBuffer idp;
	unsigned char mb[4];

	if (!state)
		return;
	if (!sg->conn)
		return;

	mode = sg->conn->local_entry->mode;
	mode &= ~(SILC_UMODE_GONE |
		  SILC_UMODE_HYPER |
		  SILC_UMODE_BUSY |
		  SILC_UMODE_INDISPOSED |
		  SILC_UMODE_PAGE);

	if (!strcmp(state, _("Hyper Active")))
		mode |= SILC_UMODE_HYPER;
	else if (!strcmp(state, _("Away")))
		mode |= SILC_UMODE_GONE;
	else if (!strcmp(state, _("Busy")))
		mode |= SILC_UMODE_BUSY;
	else if (!strcmp(state, _("Indisposed")))
		mode |= SILC_UMODE_INDISPOSED;
	else if (!strcmp(state, _("Wake Me Up")))
		mode |= SILC_UMODE_PAGE;

	/* Send UMODE */
	idp = silc_id_payload_encode(sg->conn->local_id, SILC_ID_CLIENT);
	SILC_PUT32_MSB(mode, mb);
	silc_client_command_send(sg->client, sg->conn, SILC_COMMAND_UMODE,
				 ++sg->conn->cmd_ident, 2,
				 1, idp->data, idp->len,
				 2, mb, sizeof(mb));
	silc_buffer_free(idp);
}


/*************************** Connection Routines *****************************/

static void
silcgaim_keepalive(GaimConnection *gc)
{
	SilcGaim sg = gc->proto_data;
	silc_client_send_packet(sg->client, sg->conn, SILC_PACKET_HEARTBEAT,
				NULL, 0);
}

static int
silcgaim_scheduler(gpointer *context)
{
	SilcGaim sg = (SilcGaim)context;
	silc_client_run_one(sg->client);
	return 1;
}

static void
silcgaim_nickname_parse(const char *nickname,
			char **ret_nickname)
{
	silc_parse_userfqdn(nickname, ret_nickname, NULL);
}

static void
silcgaim_login_connected(gpointer data, gint source, GaimInputCondition cond)
{
	GaimConnection *gc = data;
	SilcGaim sg = gc->proto_data;
	SilcClient client = sg->client;
	SilcClientConnection conn;
	GaimAccount *account = sg->account;
	SilcClientConnectionParams params;
	const char *dfile;

	if (source < 0) {
		gaim_connection_error(gc, _("Connection failed"));
		return;
	}
	if (!g_list_find(gaim_connections_get_all(), gc)) {
		close(source);
		g_source_remove(sg->scheduler);
		silc_client_stop(sg->client);
		silc_client_free(sg->client);
		silc_free(sg);
		return;
	}

	/* Get session detachment data, if available */
	memset(&params, 0, sizeof(params));
	dfile = silcgaim_session_file(gaim_account_get_username(sg->account));
	params.detach_data = silc_file_readfile(dfile, &params.detach_data_len);
	if (params.detach_data)
		params.detach_data[params.detach_data_len] = 0;

	/* Add connection to SILC client library */
	conn = silc_client_add_connection(
			  sg->client, &params,
			  (char *)gaim_account_get_string(account, "server",
							  "silc.silcnet.org"),
			  gaim_account_get_int(account, "port", 706), sg);
	if (!conn) {
		gaim_connection_error(gc, _("Cannot initialize SILC Client connection"));
		gc->proto_data = NULL;
		return;
	}
	sg->conn = conn;

	/* Progress */
	if (params.detach_data) {
		gaim_connection_update_progress(gc, _("Resuming session"), 2, 5);
		sg->resuming = TRUE;
	} else {
		gaim_connection_update_progress(gc, _("Performing key exchange"), 2, 5);
	}

	/* Perform SILC Key Exchange.  The "silc_connected" will be called
	   eventually. */
	silc_client_start_key_exchange(sg->client, sg->conn, source);

	/* Set default attributes */
	if (!gaim_account_get_bool(account, "reject-attrs", FALSE)) {
		SilcUInt32 mask;
		const char *tmp;
#ifdef HAVE_SYS_UTSNAME_H
		struct utsname u;
#endif

		mask = SILC_ATTRIBUTE_MOOD_NORMAL;
		silc_client_attribute_add(client, conn,
					  SILC_ATTRIBUTE_STATUS_MOOD,
					  SILC_32_TO_PTR(mask),
					  sizeof(SilcUInt32));
		mask = SILC_ATTRIBUTE_CONTACT_CHAT;
		silc_client_attribute_add(client, conn,
					  SILC_ATTRIBUTE_PREFERRED_CONTACT,
					  SILC_32_TO_PTR(mask),
					  sizeof(SilcUInt32));
#ifdef HAVE_SYS_UTSNAME_H
		if (!uname(&u)) {
			SilcAttributeObjDevice dev;
			memset(&dev, 0, sizeof(dev));
			dev.type = SILC_ATTRIBUTE_DEVICE_COMPUTER;
			dev.version = u.release;
			dev.model = u.sysname;
			silc_client_attribute_add(client, conn,
						  SILC_ATTRIBUTE_DEVICE_INFO,
						  (void *)&dev, sizeof(dev));
		}
#endif
#ifdef _WIN32
		tmp = _tzname[0];
#else
		tmp = tzname[0];
#endif
		silc_client_attribute_add(client, conn,
					  SILC_ATTRIBUTE_TIMEZONE,
					  (void *)tmp, strlen(tmp));
	}

	silc_free(params.detach_data);
}

static void
silcgaim_login(GaimAccount *account)
{
	SilcGaim sg;
	SilcClient client;
	SilcClientParams params;
	GaimConnection *gc;

	gc = account->gc;
	if (!gc)
		return;
	gc->proto_data = NULL;

	memset(&params, 0, sizeof(params));
	strcat(params.nickname_format, "%n@%h%a");
	params.nickname_parse = silcgaim_nickname_parse;
	params.ignore_requested_attributes =
		gaim_account_get_bool(account, "reject-attrs", FALSE);

	/* Allocate SILC client */
	client = silc_client_alloc(&ops, &params, gc, NULL);
	if (!client) {
		gaim_connection_error(gc, _("Out of memory"));
		return;
	}

	/* Get username, real name and local hostname for SILC library */
	if (gaim_account_get_username(account)) {
		client->username = strdup(gaim_account_get_username(account));
	} else {
		client->username = silc_get_username();
		gaim_account_set_username(account, client->username);
	}
	if (gaim_account_get_user_info(account)) {
		client->realname = strdup(gaim_account_get_user_info(account));
	} else {
		client->realname = silc_get_real_name();
		gaim_account_set_user_info(account, client->realname);
	}
	client->hostname = silc_net_localhost();

	gaim_connection_set_display_name(gc, client->username);

	/* Init SILC client */
	if (!silc_client_init(client)) {
		gaim_connection_error(gc, ("Cannot initialize SILC protocol"));
		return;
	}

	/* Check the ~/.silc dir and create it, and new key pair if necessary. */
	if (!silcgaim_check_silc_dir(gc)) {
		gaim_connection_error(gc, ("Cannot find/access ~/.silc directory"));
		return;
	}

	/* Progress */
	gaim_connection_update_progress(gc, _("Connecting to SILC Server"), 1, 5);

	/* Load SILC key pair */
	if (!silc_load_key_pair(gaim_prefs_get_string("/plugins/prpl/silc/pubkey"),
				gaim_prefs_get_string("/plugins/prpl/silc/privkey"),
				(account->password == NULL) ? "" : account->password, &client->pkcs,
				&client->public_key, &client->private_key)) {
		gaim_connection_error(gc, ("Could not load SILC key pair"));
		return;
	}

	sg = silc_calloc(1, sizeof(*sg));
	if (!sg)
		return;
	memset(sg, 0, sizeof(*sg));
	sg->client = client;
	sg->gc = gc;
	sg->account = account;
	gc->proto_data = sg;

	/* Connect to the SILC server */
	if (gaim_proxy_connect(account,
			       gaim_account_get_string(account, "server",
						       "silc.silcnet.org"),
			       gaim_account_get_int(account, "port", 706),
			       silcgaim_login_connected, gc)) {
		gaim_connection_error(gc, ("Unable to create connection"));
		return;
	}

	/* Schedule SILC using Glib's event loop */
	sg->scheduler = g_timeout_add(5, (GSourceFunc)silcgaim_scheduler, sg);
}

static int
silcgaim_close_final(gpointer *context)
{
	SilcGaim sg = (SilcGaim)context;
	silc_client_stop(sg->client);
	silc_client_free(sg->client);
	silc_free(sg);
	return 0;
}

static void
silcgaim_close(GaimConnection *gc)
{
	GList *l;
	GaimConversation *conv;
	SilcGaim sg = gc->proto_data;
	if (!sg)
		return;

	/* Close all conversations for this connection */
	for (l = gaim_get_conversations(); l; l = l->next)
	{
		conv = l->data;
		if (gc == conv->account->gc)
			gaim_conversation_destroy(conv);
	}

	/* Send QUIT */
	silc_client_command_call(sg->client, sg->conn, NULL,
				 "QUIT", "Leaving", NULL);

	if (sg->conn)
		silc_client_close_connection(sg->client, sg->conn);

	g_source_remove(sg->scheduler);
	g_timeout_add(1, (GSourceFunc)silcgaim_close_final, sg);
}


/****************************** Protocol Actions *****************************/

static void
silcgaim_attrs_cancel(GaimConnection *gc, GaimRequestFields *fields)
{
	/* Nothing */
}

static void
silcgaim_attrs_cb(GaimConnection *gc, GaimRequestFields *fields)
{
	SilcGaim sg = gc->proto_data;
	SilcClient client = sg->client;
	SilcClientConnection conn = sg->conn;
	GaimRequestField *f;
	char *tmp;
	SilcUInt32 tmp_len, mask;
	SilcAttributeObjService service;
	SilcAttributeObjDevice dev;
	SilcVCardStruct vcard;
	const char *val;

	sg = gc->proto_data;
	if (!sg)
		return;

	memset(&service, 0, sizeof(service));
	memset(&dev, 0, sizeof(dev));
	memset(&vcard, 0, sizeof(vcard));

	silc_client_attribute_del(client, conn,
				  SILC_ATTRIBUTE_USER_INFO, NULL);
	silc_client_attribute_del(client, conn,
				  SILC_ATTRIBUTE_SERVICE, NULL);
	silc_client_attribute_del(client, conn,
				  SILC_ATTRIBUTE_STATUS_MOOD, NULL);
	silc_client_attribute_del(client, conn,
				  SILC_ATTRIBUTE_STATUS_FREETEXT, NULL);
	silc_client_attribute_del(client, conn,
				  SILC_ATTRIBUTE_STATUS_MESSAGE, NULL);
	silc_client_attribute_del(client, conn,
				  SILC_ATTRIBUTE_PREFERRED_LANGUAGE, NULL);
	silc_client_attribute_del(client, conn,
				  SILC_ATTRIBUTE_PREFERRED_CONTACT, NULL);
	silc_client_attribute_del(client, conn,
				  SILC_ATTRIBUTE_TIMEZONE, NULL);
	silc_client_attribute_del(client, conn,
				  SILC_ATTRIBUTE_GEOLOCATION, NULL);
	silc_client_attribute_del(client, conn,
				  SILC_ATTRIBUTE_DEVICE_INFO, NULL);

	/* Set mood */
	mask = 0;
	f = gaim_request_fields_get_field(fields, "mood_normal");
	if (f && gaim_request_field_bool_get_value(f))
		mask |= SILC_ATTRIBUTE_MOOD_NORMAL;
	f = gaim_request_fields_get_field(fields, "mood_happy");
	if (f && gaim_request_field_bool_get_value(f))
		mask |= SILC_ATTRIBUTE_MOOD_HAPPY;
	f = gaim_request_fields_get_field(fields, "mood_sad");
	if (f && gaim_request_field_bool_get_value(f))
		mask |= SILC_ATTRIBUTE_MOOD_SAD;
	f = gaim_request_fields_get_field(fields, "mood_angry");
	if (f && gaim_request_field_bool_get_value(f))
		mask |= SILC_ATTRIBUTE_MOOD_ANGRY;
	f = gaim_request_fields_get_field(fields, "mood_jealous");
	if (f && gaim_request_field_bool_get_value(f))
		mask |= SILC_ATTRIBUTE_MOOD_JEALOUS;
	f = gaim_request_fields_get_field(fields, "mood_ashamed");
	if (f && gaim_request_field_bool_get_value(f))
		mask |= SILC_ATTRIBUTE_MOOD_ASHAMED;
	f = gaim_request_fields_get_field(fields, "mood_invincible");
	if (f && gaim_request_field_bool_get_value(f))
		mask |= SILC_ATTRIBUTE_MOOD_INVINCIBLE;
	f = gaim_request_fields_get_field(fields, "mood_inlove");
	if (f && gaim_request_field_bool_get_value(f))
		mask |= SILC_ATTRIBUTE_MOOD_INLOVE;
	f = gaim_request_fields_get_field(fields, "mood_sleepy");
	if (f && gaim_request_field_bool_get_value(f))
		mask |= SILC_ATTRIBUTE_MOOD_SLEEPY;
	f = gaim_request_fields_get_field(fields, "mood_bored");
	if (f && gaim_request_field_bool_get_value(f))
		mask |= SILC_ATTRIBUTE_MOOD_BORED;
	f = gaim_request_fields_get_field(fields, "mood_excited");
	if (f && gaim_request_field_bool_get_value(f))
		mask |= SILC_ATTRIBUTE_MOOD_EXCITED;
	f = gaim_request_fields_get_field(fields, "mood_anxious");
	if (f && gaim_request_field_bool_get_value(f))
		mask |= SILC_ATTRIBUTE_MOOD_ANXIOUS;
	silc_client_attribute_add(client, conn,
				  SILC_ATTRIBUTE_STATUS_MOOD,
				  SILC_32_TO_PTR(mask),
				  sizeof(SilcUInt32));

	/* Set preferred contact */
	mask = 0;
	f = gaim_request_fields_get_field(fields, "contact_chat");
	if (f && gaim_request_field_bool_get_value(f))
		mask |= SILC_ATTRIBUTE_CONTACT_CHAT;
	f = gaim_request_fields_get_field(fields, "contact_email");
	if (f && gaim_request_field_bool_get_value(f))
		mask |= SILC_ATTRIBUTE_CONTACT_EMAIL;
	f = gaim_request_fields_get_field(fields, "contact_call");
	if (f && gaim_request_field_bool_get_value(f))
		mask |= SILC_ATTRIBUTE_CONTACT_CALL;
	f = gaim_request_fields_get_field(fields, "contact_sms");
	if (f && gaim_request_field_bool_get_value(f))
		mask |= SILC_ATTRIBUTE_CONTACT_SMS;
	f = gaim_request_fields_get_field(fields, "contact_mms");
	if (f && gaim_request_field_bool_get_value(f))
		mask |= SILC_ATTRIBUTE_CONTACT_MMS;
	f = gaim_request_fields_get_field(fields, "contact_video");
	if (f && gaim_request_field_bool_get_value(f))
		mask |= SILC_ATTRIBUTE_CONTACT_VIDEO;
	if (mask)
		silc_client_attribute_add(client, conn,
					  SILC_ATTRIBUTE_PREFERRED_CONTACT,
					  SILC_32_TO_PTR(mask),
					  sizeof(SilcUInt32));

	/* Set status text */
	val = NULL;
	f = gaim_request_fields_get_field(fields, "status_text");
	if (f)
		val = gaim_request_field_string_get_value(f);
	if (val && *val)
		silc_client_attribute_add(client, conn,
					  SILC_ATTRIBUTE_STATUS_FREETEXT,
					  (void *)val, strlen(val));

	/* Set vcard */
	val = NULL;
	f = gaim_request_fields_get_field(fields, "vcard");
	if (f)
		val = gaim_request_field_string_get_value(f);
	if (val && *val) {
		gaim_prefs_set_string("/plugins/prpl/silc/vcard", val);
		gaim_prefs_sync();
		tmp = silc_file_readfile(val, &tmp_len);
		if (tmp) {
			tmp[tmp_len] = 0;
			if (silc_vcard_decode(tmp, tmp_len, &vcard))
				silc_client_attribute_add(client, conn,
							  SILC_ATTRIBUTE_USER_INFO,
							  (void *)&vcard,
							  sizeof(vcard));
		}
		silc_vcard_free(&vcard);
		silc_free(tmp);
	}

#ifdef HAVE_SYS_UTSNAME_H
	/* Set device info */
	f = gaim_request_fields_get_field(fields, "device");
	if (f && gaim_request_field_bool_get_value(f)) {
		struct utsname u;
		if (!uname(&u)) {
			dev.type = SILC_ATTRIBUTE_DEVICE_COMPUTER;
			dev.version = u.release;
			dev.model = u.sysname;
			silc_client_attribute_add(client, conn,
						  SILC_ATTRIBUTE_DEVICE_INFO,
						  (void *)&dev, sizeof(dev));
		}
	}
#endif

	/* Set timezone */
	val = NULL;
	f = gaim_request_fields_get_field(fields, "timezone");
	if (f)
		val = gaim_request_field_string_get_value(f);
	if (val && *val)
		silc_client_attribute_add(client, conn,
					  SILC_ATTRIBUTE_TIMEZONE,
					  (void *)val, strlen(val));
}

static void
silcgaim_attrs(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	SilcGaim sg = gc->proto_data;
	SilcClient client = sg->client;
	SilcClientConnection conn = sg->conn;
	GaimRequestFields *fields;
	GaimRequestFieldGroup *g;
	GaimRequestField *f;
	SilcHashTable attrs;
	SilcAttributePayload attr;
	gboolean mnormal = TRUE, mhappy = FALSE, msad = FALSE,
		mangry = FALSE, mjealous = FALSE, mashamed = FALSE,
		minvincible = FALSE, minlove = FALSE, msleepy = FALSE,
		mbored = FALSE, mexcited = FALSE, manxious = FALSE;
	gboolean cemail = FALSE, ccall = FALSE, csms = FALSE,
		cmms = FALSE, cchat = TRUE, cvideo = FALSE;
	gboolean device = TRUE;
	char status[1024];

	sg = gc->proto_data;
	if (!sg)
		return;

	memset(status, 0, sizeof(status));

	attrs = silc_client_attributes_get(client, conn);
	if (attrs) {
		if (silc_hash_table_find(attrs,
					 SILC_32_TO_PTR(SILC_ATTRIBUTE_STATUS_MOOD),
					 NULL, (void *)&attr)) {
			SilcUInt32 mood = 0;
			silc_attribute_get_object(attr, &mood, sizeof(mood));
			mnormal = !mood;
			mhappy = (mood & SILC_ATTRIBUTE_MOOD_HAPPY);
			msad = (mood & SILC_ATTRIBUTE_MOOD_SAD);
			mangry = (mood & SILC_ATTRIBUTE_MOOD_ANGRY);
			mjealous = (mood & SILC_ATTRIBUTE_MOOD_JEALOUS);
			mashamed = (mood & SILC_ATTRIBUTE_MOOD_ASHAMED);
			minvincible = (mood & SILC_ATTRIBUTE_MOOD_INVINCIBLE);
			minlove = (mood & SILC_ATTRIBUTE_MOOD_INLOVE);
			msleepy = (mood & SILC_ATTRIBUTE_MOOD_SLEEPY);
			mbored = (mood & SILC_ATTRIBUTE_MOOD_BORED);
			mexcited = (mood & SILC_ATTRIBUTE_MOOD_EXCITED);
			manxious = (mood & SILC_ATTRIBUTE_MOOD_ANXIOUS);
		}

		if (silc_hash_table_find(attrs,
					 SILC_32_TO_PTR(SILC_ATTRIBUTE_PREFERRED_CONTACT),
					 NULL, (void *)&attr)) {
			SilcUInt32 contact = 0;
			silc_attribute_get_object(attr, &contact, sizeof(contact));
			cemail = (contact & SILC_ATTRIBUTE_CONTACT_EMAIL);
			ccall = (contact & SILC_ATTRIBUTE_CONTACT_CALL);
			csms = (contact & SILC_ATTRIBUTE_CONTACT_SMS);
			cmms = (contact & SILC_ATTRIBUTE_CONTACT_MMS);
			cchat = (contact & SILC_ATTRIBUTE_CONTACT_CHAT);
			cvideo = (contact & SILC_ATTRIBUTE_CONTACT_VIDEO);
		}

		if (silc_hash_table_find(attrs,
					 SILC_32_TO_PTR(SILC_ATTRIBUTE_STATUS_FREETEXT),
					 NULL, (void *)&attr))
			silc_attribute_get_object(attr, &status, sizeof(status));

		if (!silc_hash_table_find(attrs,
					  SILC_32_TO_PTR(SILC_ATTRIBUTE_DEVICE_INFO),
					  NULL, (void *)&attr))
			device = FALSE;
	}

	fields = gaim_request_fields_new();

	g = gaim_request_field_group_new(NULL);
	f = gaim_request_field_label_new("l3", _("Your Current Mood"));
	gaim_request_field_group_add_field(g, f);
	f = gaim_request_field_bool_new("mood_normal", _("Normal"), mnormal);
	gaim_request_field_group_add_field(g, f);
	f = gaim_request_field_bool_new("mood_happy", _("Happy"), mhappy);
	gaim_request_field_group_add_field(g, f);
	f = gaim_request_field_bool_new("mood_sad", _("Sad"), msad);
	gaim_request_field_group_add_field(g, f);
	f = gaim_request_field_bool_new("mood_angry", _("Angry"), mangry);
	gaim_request_field_group_add_field(g, f);
	f = gaim_request_field_bool_new("mood_jealous", _("Jealous"), mjealous);
	gaim_request_field_group_add_field(g, f);
	f = gaim_request_field_bool_new("mood_ashamed", _("Ashamed"), mashamed);
	gaim_request_field_group_add_field(g, f);
	f = gaim_request_field_bool_new("mood_invincible", _("Invincible"), minvincible);
	gaim_request_field_group_add_field(g, f);
	f = gaim_request_field_bool_new("mood_inlove", _("In Love"), minlove);
	gaim_request_field_group_add_field(g, f);
	f = gaim_request_field_bool_new("mood_sleepy", _("Sleepy"), msleepy);
	gaim_request_field_group_add_field(g, f);
	f = gaim_request_field_bool_new("mood_bored", _("Bored"), mbored);
	gaim_request_field_group_add_field(g, f);
	f = gaim_request_field_bool_new("mood_excited", _("Excited"), mexcited);
	gaim_request_field_group_add_field(g, f);
	f = gaim_request_field_bool_new("mood_anxious", _("Anxious"), manxious);
	gaim_request_field_group_add_field(g, f);

	f = gaim_request_field_label_new("l4", _("\nYour Preferred Contact Methods"));
	gaim_request_field_group_add_field(g, f);
	f = gaim_request_field_bool_new("contact_chat", _("Chat"), cchat);
	gaim_request_field_group_add_field(g, f);
	f = gaim_request_field_bool_new("contact_email", _("Email"), cemail);
	gaim_request_field_group_add_field(g, f);
	f = gaim_request_field_bool_new("contact_call", _("Phone"), ccall);
	gaim_request_field_group_add_field(g, f);
	f = gaim_request_field_bool_new("contact_sms", _("SMS"), csms);
	gaim_request_field_group_add_field(g, f);
	f = gaim_request_field_bool_new("contact_mms", _("MMS"), cmms);
	gaim_request_field_group_add_field(g, f);
	f = gaim_request_field_bool_new("contact_video", _("Video Conferencing"), cvideo);
	gaim_request_field_group_add_field(g, f);
	gaim_request_fields_add_group(fields, g);

	g = gaim_request_field_group_new(NULL);
	f = gaim_request_field_string_new("status_text", _("Your Current Status"),
					  status[0] ? status : NULL, TRUE);
	gaim_request_field_group_add_field(g, f);
	gaim_request_fields_add_group(fields, g);

	g = gaim_request_field_group_new(NULL);
#if 0
	f = gaim_request_field_label_new("l2", _("Online Services"));
	gaim_request_field_group_add_field(g, f);
	f = gaim_request_field_bool_new("services",
					_("Let others see what services you are using"),
					TRUE);
	gaim_request_field_group_add_field(g, f);
#endif
#ifdef HAVE_SYS_UTSNAME_H
	f = gaim_request_field_bool_new("device",
					_("Let others see what computer you are using"),
					device);
	gaim_request_field_group_add_field(g, f);
#endif
	gaim_request_fields_add_group(fields, g);

	g = gaim_request_field_group_new(NULL);
	f = gaim_request_field_string_new("vcard", _("Your VCard File"),
					  gaim_prefs_get_string("/plugins/prpl/silc/vcard"),
					  FALSE);
	gaim_request_field_group_add_field(g, f);
#ifdef _WIN32
	f = gaim_request_field_string_new("timezone", _("Timezone"), _tzname[0], FALSE);
#else
	f = gaim_request_field_string_new("timezone", _("Timezone"), tzname[0], FALSE);
#endif
	gaim_request_field_group_add_field(g, f);
	gaim_request_fields_add_group(fields, g);


	gaim_request_fields(NULL, _("User Online Status Attributes"),
			    _("User Online Status Attributes"),
			    _("You can let other users see your online status information "
			      "and your personal information. Please fill the information "
			      "you would like other users to see about yourself."),
			    fields,
			    _("OK"), G_CALLBACK(silcgaim_attrs_cb),
			    _("Cancel"), G_CALLBACK(silcgaim_attrs_cancel), gc);
}

static void
silcgaim_detach(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	SilcGaim sg;

	if (!gc)
		return;
	sg = gc->proto_data;
	if (!sg)
		return;

	/* Call DETACH */
	silc_client_command_call(sg->client, sg->conn, "DETACH");
	sg->detaching = TRUE;
}

static void
silcgaim_view_motd(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	SilcGaim sg;

	if (!gc)
		return;
	sg = gc->proto_data;
	if (!sg)
		return;

	if (!sg->motd) {
		gaim_notify_error(
		     gc, _("Message of the Day"), _("No Message of the Day available"),
		     _("There is no Message of the Day associated with this connection"));
		return;
	}

	gaim_notify_formatted(gc, _("Message of the Day"), _("Message of the Day"), NULL,
			      sg->motd, NULL, NULL);
}

static void
silcgaim_change_pass(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	gaim_account_request_change_password(gaim_connection_get_account(gc));
}

static void
silcgaim_change_passwd(GaimConnection *gc, const char *old, const char *new)
{
	silc_change_private_key_passphrase(gaim_prefs_get_string("/plugins/prpl/silc/privkey"), old, new);
}

static void
silcgaim_show_set_info(GaimPluginAction *action)
{
	GaimConnection *gc = (GaimConnection *) action->context;
	gaim_account_request_change_user_info(gaim_connection_get_account(gc));
}

static void
silcgaim_set_info(GaimConnection *gc, const char *text)
{
}

static GList *
silcgaim_actions(GaimPlugin *plugin, gpointer context)
{
	GaimConnection *gc = context;
	GList *list = NULL;
	GaimPluginAction *act;

	if (!gaim_account_get_bool(gc->account, "reject-attrs", FALSE)) {
		act = gaim_plugin_action_new(_("Online Status"),
				silcgaim_attrs);
		list = g_list_append(list, act);
	}

	act = gaim_plugin_action_new(_("Detach From Server"),
			silcgaim_detach);
	list = g_list_append(list, act);

	act = gaim_plugin_action_new(_("View Message of the Day"),
			silcgaim_view_motd);
	list = g_list_append(list, act);

	act = gaim_plugin_action_new(_("Change Password..."),
			silcgaim_change_pass);
	list = g_list_append(list, act);

	act = gaim_plugin_action_new(_("Set User Info..."),
			silcgaim_show_set_info);
	list = g_list_append(list, act);

	return list;
}


/******************************* IM Routines *********************************/

typedef struct {
	char *nick;
	unsigned char *message;
	SilcUInt32 message_len;
	SilcMessageFlags flags;
} *SilcGaimIM;

static void
silcgaim_send_im_resolved(SilcClient client,
			  SilcClientConnection conn,
			  SilcClientEntry *clients,
			  SilcUInt32 clients_count,
			  void *context)
{
	GaimConnection *gc = client->application;
	SilcGaim sg = gc->proto_data;
	SilcGaimIM im = context;
	GaimConversation *convo;
	char tmp[256], *nickname = NULL;
	SilcClientEntry client_entry;

	convo = gaim_find_conversation_with_account(im->nick, sg->account);
	if (!convo)
		return;

	if (!clients)
		goto err;

	if (clients_count > 1) {
		silc_parse_userfqdn(im->nick, &nickname, NULL);

		/* Find the correct one. The im->nick might be a formatted nick
		   so this will find the correct one. */
		clients = silc_client_get_clients_local(client, conn,
							nickname, im->nick,
							&clients_count);
		if (!clients)
			goto err;
		client_entry = clients[0];
		silc_free(clients);
	} else {
		client_entry = clients[0];
	}

	/* Send the message */
	silc_client_send_private_message(client, conn, client_entry, im->flags,
					 im->message, im->message_len, TRUE);
	gaim_conv_im_write(GAIM_CONV_IM(convo), conn->local_entry->nickname,
			   im->message, 0, time(NULL));

	goto out;

 err:
	g_snprintf(tmp, sizeof(tmp),
		   _("User <I>%s</I> is not present in the network"), im->nick);
	gaim_conversation_write(convo, NULL, tmp, GAIM_MESSAGE_SYSTEM, time(NULL));

 out:
	g_free(im->nick);
	g_free(im->message);
	silc_free(im);
	silc_free(nickname);
}

static int
silcgaim_send_im(GaimConnection *gc, const char *who, const char *msg,
		 GaimConvImFlags flags)
{
	SilcGaim sg = gc->proto_data;
	SilcClient client = sg->client;
	SilcClientConnection conn = sg->conn;
	SilcClientEntry *clients;
	SilcUInt32 clients_count, mflags;
	char *nickname;
	int ret;
	gboolean sign = gaim_prefs_get_bool("/plugins/prpl/silc/sign_im");

	if (!who || !msg)
		return 0;

	/* See if command */
	if (strlen(msg) > 1 && msg[0] == '/') {
		if (!silc_client_command_call(client, conn, msg + 1))
			gaim_notify_error(gc, ("Call Command"), _("Cannot call command"),
					  _("Unknown command"));
		return 0;
	}

	if (!silc_parse_userfqdn(who, &nickname, NULL))
		return 0;

	mflags = SILC_MESSAGE_FLAG_UTF8;
	if (sign)
		mflags |= SILC_MESSAGE_FLAG_SIGNED;

	/* Find client entry */
	clients = silc_client_get_clients_local(client, conn, nickname, who,
						&clients_count);
	if (!clients) {
		/* Resolve unknown user */
		SilcGaimIM im = silc_calloc(1, sizeof(*im));
		if (!im)
			return 0;
		im->nick = g_strdup(who);
		im->message = g_strdup(msg);
		im->message_len = strlen(im->message);
		im->flags = mflags;
		silc_client_get_clients(client, conn, nickname, NULL,
					silcgaim_send_im_resolved, im);
		silc_free(nickname);
		return 0;
	}

	/* Send private message directly */
	ret = silc_client_send_private_message(client, conn, clients[0],
					       mflags, (char *)msg,
					       strlen(msg), TRUE);

	silc_free(nickname);
	silc_free(clients);
	return ret;
}


GList *silcgaim_blist_node_menu(GaimBlistNode *node) {
	/* split this single menu building function back into the two
	   original: one for buddies and one for chats */

	if(GAIM_BLIST_NODE_IS_CHAT(node)) {
		return silcgaim_chat_menu((GaimChat *) node);
	} else if(GAIM_BLIST_NODE_IS_BUDDY(node)) {
		return silcgaim_buddy_menu((GaimBuddy *) node);
	} else {
		g_return_val_if_reached(NULL);
	}	
}

/********************************* Commands **********************************/

static GaimCmdRet silcgaim_cmd_chat_part(GaimConversation *conv,
		const char *cmd, char **args, char **error)
{
	GaimConnection *gc;
	int id = 0;

	gc = gaim_conversation_get_gc(conv);
	id = gaim_conv_chat_get_id(GAIM_CONV_CHAT(conv));

	if (gc == NULL || id == 0)
		return GAIM_CMD_RET_FAILED;

	silcgaim_chat_leave(gc, id);

	return GAIM_CMD_RET_OK;

}

static GaimCmdRet silcgaim_cmd_chat_topic(GaimConversation *conv,
		const char *cmd, char **args, char **error)
{
	GaimConnection *gc;
	int id = 0;

	gc = gaim_conversation_get_gc(conv);
	id = gaim_conv_chat_get_id(GAIM_CONV_CHAT(conv));

	if (gc == NULL || id == 0)
		return GAIM_CMD_RET_FAILED;

	silcgaim_chat_set_topic(gc, id, args ? args[0] : NULL);

	return GAIM_CMD_RET_OK;
}

static GaimCmdRet silcgaim_cmd_chat_join(GaimConversation *conv,
        const char *cmd, char **args, char **error)
{
	GHashTable *comp;

	if(!args || !args[0])
		return GAIM_CMD_RET_FAILED;

	comp = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, NULL);

	g_hash_table_replace(comp, "channel", args[0]);
	if(args[1])
		g_hash_table_replace(comp, "passphrase", args[1]);

	silcgaim_chat_join(gaim_conversation_get_gc(conv), comp);

	g_hash_table_destroy(comp);
	return GAIM_CMD_RET_OK;
}

static GaimCmdRet silcgaim_cmd_chat_list(GaimConversation *conv,
        const char *cmd, char **args, char **error)
{
	GaimConnection *gc;
	gc = gaim_conversation_get_gc(conv);
	gaim_roomlist_show_with_account(gaim_connection_get_account(gc));
	return GAIM_CMD_RET_OK;
}

static GaimCmdRet silcgaim_cmd_whois(GaimConversation *conv,
		const char *cmd, char **args, char **error)
{
	GaimConnection *gc;

	gc = gaim_conversation_get_gc(conv);

	if (gc == NULL)
		return GAIM_CMD_RET_FAILED;

	silcgaim_get_info(gc, args[0]);

	return GAIM_CMD_RET_OK;
}

static GaimCmdRet silcgaim_cmd_chat_invite(GaimConversation *conv,
		const char *cmd, char **args, char **error)
{
	int id;
	GaimConnection *gc;

	id = gaim_conv_chat_get_id(GAIM_CONV_CHAT(conv));
	gc = gaim_conversation_get_gc(conv);

	if (gc == NULL)
		return GAIM_CMD_RET_FAILED;

	silcgaim_chat_invite(gc, id, NULL, args[0]);

	return GAIM_CMD_RET_OK;
}

static GaimCmdRet silcgaim_cmd_msg(GaimConversation *conv,
		const char *cmd, char **args, char **error)
{
	int ret;
	GaimConnection *gc;

	gc = gaim_conversation_get_gc(conv);

	if (gc == NULL)
		return GAIM_CMD_RET_FAILED;

	ret = silcgaim_send_im(gc, args[0], args[1], GAIM_MESSAGE_SEND);

	if (ret)
		return GAIM_CMD_RET_OK;
	else
		return GAIM_CMD_RET_FAILED;
}

static GaimCmdRet silcgaim_cmd_query(GaimConversation *conv,
		const char *cmd, char **args, char **error)
{
	int ret = 1;
	GaimConversation *convo;
	GaimConnection *gc;
	GaimAccount *account;

	if (!args || !args[0])
		return GAIM_CMD_RET_FAILED;

	gc = gaim_conversation_get_gc(conv);

	if (gc == NULL)
		return GAIM_CMD_RET_FAILED;

	account = gaim_connection_get_account(gc);

	convo = gaim_conversation_new(GAIM_CONV_IM, account, args[0]);

	if (args[1]) {
		ret = silcgaim_send_im(gc, args[0], args[1], GAIM_MESSAGE_SEND);
		gaim_conv_im_write(GAIM_CONV_IM(convo), gaim_connection_get_display_name(gc),
				args[1], GAIM_MESSAGE_SEND, time(NULL));
	}

	if (ret)
		return GAIM_CMD_RET_OK;
	else
		return GAIM_CMD_RET_FAILED;
}

static GaimCmdRet silcgaim_cmd_motd(GaimConversation *conv,
		const char *cmd, char **args, char **error)
{
	GaimConnection *gc;
	SilcGaim sg;

	gc = gaim_conversation_get_gc(conv);

	if (gc == NULL)
		return GAIM_CMD_RET_FAILED;

	sg = gc->proto_data;

	if (sg == NULL)
		return GAIM_CMD_RET_FAILED;

	if (!sg->motd) {
		gaim_notify_error(
				gc, _("Message of the Day"), _("No Message of the Day available"),
				_("There is no Message of the Day associated with this connection"));
		return GAIM_CMD_RET_FAILED;
	}

	gaim_notify_formatted(gc, _("Message of the Day"), _("Message of the Day"), NULL,
			sg->motd, NULL, NULL);

	return GAIM_CMD_RET_OK;
}

static GaimCmdRet silcgaim_cmd_detach(GaimConversation *conv,
		const char *cmd, char **args, char **error)
{
	GaimConnection *gc;
	SilcGaim sg;

	gc = gaim_conversation_get_gc(conv);

	if (gc == NULL)
		return GAIM_CMD_RET_FAILED;

	sg = gc->proto_data;

	if (sg == NULL)
		return GAIM_CMD_RET_FAILED;

	silc_client_command_call(sg->client, sg->conn, "DETACH");
	sg->detaching = TRUE;

	return GAIM_CMD_RET_OK;
}

static GaimCmdRet silcgaim_cmd_umode(GaimConversation *conv,
		const char *cmd, char **args, char **error)
{
	GaimConnection *gc;
	SilcGaim sg;

	gc = gaim_conversation_get_gc(conv);

	if (gc == NULL)
		return GAIM_CMD_RET_FAILED;

	sg = gc->proto_data;

	if (sg == NULL)
		return GAIM_CMD_RET_FAILED;

	silc_client_command_call(sg->client, sg->conn, NULL, "UMODE",
			args[0], NULL);

	return GAIM_CMD_RET_OK;
}


/************************** Plugin Initialization ****************************/

static void
silcgaim_register_commands(void)
{
	gaim_cmd_register("part", "", GAIM_CMD_P_PRPL,
			GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_PRPL_ONLY,
			"prpl-silc", silcgaim_cmd_chat_part, _("part:  Leave the chat"));
	gaim_cmd_register("leave", "", GAIM_CMD_P_PRPL,
			GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_PRPL_ONLY,
			"prpl-silc", silcgaim_cmd_chat_part, _("leave:  Leave the chat"));
	gaim_cmd_register("topic", "s", GAIM_CMD_P_PRPL,
			GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_PRPL_ONLY |
			GAIM_CMD_FLAG_ALLOW_WRONG_ARGS, "prpl-silc",
			silcgaim_cmd_chat_topic, _("topic [&lt;new topic&gt;]:  View or change the topic"));
	gaim_cmd_register("join", "ws", GAIM_CMD_P_PRPL,
			GAIM_CMD_FLAG_IM | GAIM_CMD_FLAG_CHAT |
			GAIM_CMD_FLAG_PRPL_ONLY | GAIM_CMD_FLAG_ALLOW_WRONG_ARGS,
			"prpl-silc", silcgaim_cmd_chat_join,
			_("join &lt;channel&gt; [&lt;password&gt;]:  Join a chat on this network"));
	gaim_cmd_register("list", "", GAIM_CMD_P_PRPL,
			GAIM_CMD_FLAG_IM | GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_PRPL_ONLY |
			GAIM_CMD_FLAG_ALLOW_WRONG_ARGS, "prpl-silc",
			silcgaim_cmd_chat_list, _("list:  List channels on this network"));
	gaim_cmd_register("whois", "w", GAIM_CMD_P_PRPL,
			GAIM_CMD_FLAG_IM | GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_PRPL_ONLY,
			"prpl-silc",
			silcgaim_cmd_whois, _("whois &lt;nick&gt;:  View nick's information"));
	gaim_cmd_register("invite", "w", GAIM_CMD_P_PRPL,
			GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_PRPL_ONLY,
			"prpl-silc", silcgaim_cmd_chat_invite,
			_("invite &lt;nick&gt;:  Invite nick to join this channel"));
	gaim_cmd_register("msg", "ws", GAIM_CMD_P_PRPL,
			GAIM_CMD_FLAG_IM | GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_PRPL_ONLY,
			"prpl-silc", silcgaim_cmd_msg,
			_("msg &lt;nick&gt; &lt;message&gt;:  Send a private message to a user"));
	gaim_cmd_register("query", "ws", GAIM_CMD_P_PRPL,
			GAIM_CMD_FLAG_IM | GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_PRPL_ONLY |
			GAIM_CMD_FLAG_ALLOW_WRONG_ARGS, "prpl-silc", silcgaim_cmd_query,
			_("query &lt;nick&gt; [&lt;message&gt;]:  Send a private message to a user"));
	gaim_cmd_register("motd", "", GAIM_CMD_P_PRPL,
			GAIM_CMD_FLAG_IM | GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_PRPL_ONLY |
			GAIM_CMD_FLAG_ALLOW_WRONG_ARGS, "prpl-silc", silcgaim_cmd_motd,
			_("motd:  View the server's Message Of The Day"));
	gaim_cmd_register("detach", "", GAIM_CMD_P_PRPL,
			GAIM_CMD_FLAG_IM | GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_PRPL_ONLY |
			GAIM_CMD_FLAG_ALLOW_WRONG_ARGS, "prpl-silc", silcgaim_cmd_detach,
			_("detach:  Detach this session"));
	gaim_cmd_register("umode", "w", GAIM_CMD_P_PRPL,
			GAIM_CMD_FLAG_IM | GAIM_CMD_FLAG_CHAT | GAIM_CMD_FLAG_PRPL_ONLY,
			"prpl-silc", silcgaim_cmd_umode,
			_("umode &lt;usermodes&gt;:  Set your user options"));
}

static GaimPluginPrefFrame *
silcgaim_pref_frame(GaimPlugin *plugin)
{
	GaimPluginPrefFrame *frame;
	GaimPluginPref *ppref;

	frame = gaim_plugin_pref_frame_new();

	ppref = gaim_plugin_pref_new_with_label(_("Instant Messages"));
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_name_and_label(
			    "/plugins/prpl/silc/sign_im",
			    _("Digitally sign all IM messages"));
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_name_and_label(
			    "/plugins/prpl/silc/verify_im",
			    _("Verify all IM message signatures"));
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_label(_("Channel Messages"));
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_name_and_label(
			    "/plugins/prpl/silc/sign_chat",
			    _("Digitally sign all channel messages"));
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_name_and_label(
			    "/plugins/prpl/silc/verify_chat",
			    _("Verify all channel message signatures"));
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_label(_("Default SILC Key Pair"));
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_name_and_label(
			    "/plugins/prpl/silc/pubkey",
			    _("SILC Public Key"));
	gaim_plugin_pref_frame_add(frame, ppref);

	ppref = gaim_plugin_pref_new_with_name_and_label(
			    "/plugins/prpl/silc/privkey",
			    _("SILC Private Key"));
	gaim_plugin_pref_frame_add(frame, ppref);

	return frame;
}

static GaimPluginUiInfo prefs_info =
{
	silcgaim_pref_frame,
};

static GaimPluginProtocolInfo prpl_info =
{
	GAIM_PRPL_API_VERSION,
	OPT_PROTO_CHAT_TOPIC | OPT_PROTO_UNIQUE_CHATNAME |
	OPT_PROTO_PASSWORD_OPTIONAL,
	NULL,
	NULL,
	silcgaim_list_icon,
	silcgaim_list_emblems,
	silcgaim_status_text,
	silcgaim_tooltip_text,
	silcgaim_away_states,
	silcgaim_blist_node_menu,
	silcgaim_chat_info,
	silcgaim_login,
	silcgaim_close,
	silcgaim_send_im,
	silcgaim_set_info,
	NULL,
	silcgaim_get_info,
	silcgaim_set_away,
	silcgaim_idle_set,
	silcgaim_change_passwd,
	silcgaim_add_buddy,
	silcgaim_add_buddies,
	silcgaim_remove_buddy,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	silcgaim_chat_join,
	NULL,
	silcgaim_chat_invite,
	silcgaim_chat_leave,
	NULL,
	silcgaim_chat_send,
	silcgaim_keepalive,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	silcgaim_chat_set_topic,
	NULL,
	silcgaim_roomlist_get_list,
	silcgaim_roomlist_cancel,
	NULL
};

static GaimPluginInfo info =
{
	GAIM_PLUGIN_API_VERSION,                          /**< api_version    */
	GAIM_PLUGIN_PROTOCOL,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	"prpl-silc",                                      /**< id             */
	"SILC",                                           /**< name           */
	"1.0",                                            /**< version        */
	/**  summary        */
	N_("SILC Protocol Plugin"),
	/**  description    */
	N_("Secure Internet Live Conferencing (SILC) Protocol"),
	"Pekka Riikonen",                                 /**< author         */
	"http://silcnet.org/",                            /**< homepage       */

	NULL,                                             /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info,                                       /**< extra_info     */
	&prefs_info,                                      /**< prefs_info     */
	silcgaim_actions
};

static void
init_plugin(GaimPlugin *plugin)
{
	GaimAccountOption *option;
	char tmp[256];

	silc_plugin = plugin;

	/* Account options */
	option = gaim_account_option_string_new(_("Connect server"),
						"server",
						"silc.silcnet.org");
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
	option = gaim_account_option_int_new(_("Port"), "port", 706);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	option = gaim_account_option_bool_new(_("Public key authentication"),
					      "pubkey-auth", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
#if 0   /* XXX Public key auth interface with explicit key pair is
	   broken in SILC Toolkit */
	g_snprintf(tmp, sizeof(tmp), "%s/public_key.pub", silcgaim_silcdir());
	option = gaim_account_option_string_new(_("Public Key File"),
						"public-key", tmp);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
	g_snprintf(tmp, sizeof(tmp), "%s/private_key.prv", silcgaim_silcdir());
	option = gaim_account_option_string_new(_("Private Key File"),
						"public-key", tmp);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
#endif

	option = gaim_account_option_bool_new(_("Reject watching by other users"),
					      "reject-watch", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
	option = gaim_account_option_bool_new(_("Block invites"),
					      "block-invites", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
	option = gaim_account_option_bool_new(_("Block IMs without Key Exchange"),
					      "block-ims", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
	option = gaim_account_option_bool_new(_("Reject online status attribute requests"),
					      "reject-attrs", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

	/* Preferences */
	gaim_prefs_add_none("/plugins/prpl/silc");
	gaim_prefs_add_bool("/plugins/prpl/silc/sign_im", FALSE);
	gaim_prefs_add_bool("/plugins/prpl/silc/verify_im", FALSE);
	gaim_prefs_add_bool("/plugins/prpl/silc/sign_chat", FALSE);
	gaim_prefs_add_bool("/plugins/prpl/silc/verify_chat", FALSE);
	g_snprintf(tmp, sizeof(tmp), "%s/public_key.pub", silcgaim_silcdir());
	gaim_prefs_add_string("/plugins/prpl/silc/pubkey", tmp);
	g_snprintf(tmp, sizeof(tmp), "%s/private_key.prv", silcgaim_silcdir());
	gaim_prefs_add_string("/plugins/prpl/silc/privkey", tmp);
	gaim_prefs_add_string("/plugins/prpl/silc/vcard", "");

	silcgaim_register_commands();
}

GAIM_INIT_PLUGIN(silc, init_plugin, info);
