/*

  silcpurple_ft.c

  Author: Pekka Riikonen <priikone@silcnet.org>

  Copyright (C) 2004 - 2007 Pekka Riikonen

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

*/

#include <glib/gi18n-lib.h>
#include <glib/gstdio.h>

#include "silcpurple.h"
#include "ft.h"

/****************************** File Transfer ********************************/

/* This implements the secure file transfer protocol (SFTP) using the SILC
   SFTP library implementation.  The API we use from the SILC Toolkit is the
   SILC Client file transfer API, as it provides a simple file transfer we
   need in this case.  We could use the SILC SFTP API directly, but it would
   be an overkill since we'd effectively re-implement the file transfer what
   the SILC Client's file transfer API already provides.

   From Purple we do NOT use the FT API to do the transfer as it is very limiting.
   In fact it does not suite to file transfers like SFTP at all.  For example,
   it assumes that read operations are synchronous what they are not in SFTP.
   It also assumes that the file transfer socket is to be handled by the Purple
   eventloop, and this naturally is something we don't want to do in case of
   SILC Toolkit.  The FT API suites well to purely stream based file transfers
   like HTTP GET and similar.

   For this reason, we directly access the Purple GKT FT API and hack the FT
   API to merely provide the user interface experience and all the magic
   is done in the SILC Toolkit.  Ie. we update the statistics information in
   the FT API for user interface, and that's it.  A bit dirty but until the
   FT API gets better this is the way to go.  Good thing that FT API allowed
   us to do this. */

struct _SilcPurpleXfer {
	PurpleXfer parent;

	SilcPurple sg;
	SilcClientEntry client_entry;
	SilcUInt32 session_id;
	char *hostname;
	SilcUInt16 port;

	SilcClientFileName completion;
	void *completion_context;
};

G_DEFINE_DYNAMIC_TYPE(SilcPurpleXfer, silcpurple_xfer, PURPLE_TYPE_XFER);

static void
silcpurple_ftp_monitor(SilcClient client,
		     SilcClientConnection conn,
		     SilcClientMonitorStatus status,
		     SilcClientFileError error,
		     SilcUInt64 offset,
		     SilcUInt64 filesize,
		     SilcClientEntry client_entry,
		     SilcUInt32 session_id,
		     const char *filepath,
		     void *context)
{
	PurpleXfer *xfer = context;
	SilcPurpleXfer *spx = SILCPURPLE_XFER(xfer);
	PurpleConnection *gc = spx->sg->gc;
	char tmp[256];

	if (status == SILC_CLIENT_FILE_MONITOR_CLOSED) {
		/* All started sessions terminate here */
		g_object_unref(xfer);
		return;
	}

	if (status == SILC_CLIENT_FILE_MONITOR_DISCONNECT) {
		purple_notify_error(gc, _("Secure File Transfer"), _("Error "
			"during file transfer"), _("Remote disconnected"),
			purple_request_cpar_from_connection(gc));
		purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_CANCEL_REMOTE);
		silc_client_file_close(client, conn, session_id);
		return;
	}

	if (status == SILC_CLIENT_FILE_MONITOR_KEY_AGREEMENT)
		return;

	if (status == SILC_CLIENT_FILE_MONITOR_ERROR) {
		if (error == SILC_CLIENT_FILE_NO_SUCH_FILE) {
			g_snprintf(tmp, sizeof(tmp), "No such file %s",
				   filepath ? filepath : "[N/A]");
			purple_notify_error(gc, _("Secure File Transfer"),
				_("Error during file transfer"), tmp,
				purple_request_cpar_from_connection(gc));
		} else if (error == SILC_CLIENT_FILE_PERMISSION_DENIED) {
			purple_notify_error(gc, _("Secure File Transfer"),
				_("Error during file transfer"),
				_("Permission denied"),
				purple_request_cpar_from_connection(gc));
		} else if (error == SILC_CLIENT_FILE_KEY_AGREEMENT_FAILED) {
			purple_notify_error(gc, _("Secure File Transfer"),
				_("Error during file transfer"),
				_("Key agreement failed"),
				purple_request_cpar_from_connection(gc));
		} else if (error == SILC_CLIENT_FILE_TIMEOUT) {
			purple_notify_error(gc, _("Secure File Transfer"),
				_("Error during file transfer"),
				_("Connection timed out"),
				purple_request_cpar_from_connection(gc));
		} else if (error == SILC_CLIENT_FILE_CONNECT_FAILED) {
			purple_notify_error(gc, _("Secure File Transfer"),
				_("Error during file transfer"),
				_("Creating connection failed"),
				purple_request_cpar_from_connection(gc));
		} else if (error == SILC_CLIENT_FILE_UNKNOWN_SESSION) {
			purple_notify_error(gc, _("Secure File Transfer"),
				_("Error during file transfer"),
				_("File transfer session does not exist"),
				purple_request_cpar_from_connection(gc));
		}
		purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_CANCEL_REMOTE);
		silc_client_file_close(client, conn, session_id);
		return;
	}

	/* Update file transfer UI */
	if (!offset && filesize) {
		purple_xfer_set_size(xfer, filesize);
	}
	if (offset && filesize) {
		purple_xfer_set_bytes_sent(xfer, offset);
	}

	if (status == SILC_CLIENT_FILE_MONITOR_SEND ||
	    status == SILC_CLIENT_FILE_MONITOR_RECEIVE) {
		if (offset == filesize) {
			/* Download finished */
			purple_xfer_set_completed(xfer, TRUE);
			silc_client_file_close(client, conn, session_id);
		}
	}
}

static void
silcpurple_ftp_cancel(PurpleXfer *xfer)
{
	SilcPurpleXfer *spx = SILCPURPLE_XFER(xfer);

	if (!spx) {
		return;
	}

	purple_xfer_set_status(xfer, PURPLE_XFER_STATUS_CANCEL_LOCAL);
	silc_client_file_close(spx->sg->client, spx->sg->conn, spx->session_id);
}

static void
silcpurple_ftp_ask_name_ok(PurpleXfer *xfer)
{
	SilcPurpleXfer *spx = SILCPURPLE_XFER(xfer);
	const char *name;

	if (!spx) {
		return;
	}

	name = purple_xfer_get_local_filename(xfer);
	g_unlink(name);
	spx->completion(name, spx->completion_context);
}

static void
silcpurple_ftp_ask_name(SilcClient client,
		      SilcClientConnection conn,
		      SilcUInt32 session_id,
		      const char *remote_filename,
		      SilcClientFileName completion,
		      void *completion_context,
		      void *context)
{
	PurpleXfer *xfer = context;
	SilcPurpleXfer *spx = SILCPURPLE_XFER(xfer);

	spx->completion = completion;
	spx->completion_context = completion_context;

	/* Request to save the file */
	purple_xfer_set_filename(xfer, remote_filename);
	purple_xfer_request(xfer);
}

static void
silcpurple_ftp_request_result(PurpleXfer *xfer)
{
	SilcPurpleXfer *spx = SILCPURPLE_XFER(xfer);
	SilcClientFileError status;
	PurpleConnection *gc = spx->sg->gc;
	SilcClientConnectionParams params;
	gboolean local = spx->hostname ? FALSE : TRUE;
	char *local_ip = NULL, *remote_ip = NULL;
	SilcSocket sock;

	if (purple_xfer_get_status(xfer) != PURPLE_XFER_STATUS_ACCEPTED) {
		return;
	}

	silc_socket_stream_get_info(silc_packet_stream_get_stream(spx->sg->conn->stream),
				    &sock, NULL, NULL, NULL);

	if (local) {
		/* Do the same magic what we do with key agreement (see silcpurple_buddy.c)
		   to see if we are behind NAT. */
		if (silc_net_check_local_by_sock(sock, NULL, &local_ip)) {
			/* Check if the IP is private */
			if (silcpurple_ip_is_private(local_ip)) {
				local = TRUE;
				/* Local IP is private, resolve the remote server IP to see whether
				   we are talking to Internet or just on LAN. */
				if (silc_net_check_host_by_sock(sock, NULL,
								&remote_ip))
				  if (silcpurple_ip_is_private(remote_ip))
				    /* We assume we are in LAN.  Let's provide the connection point. */
				    local = TRUE;
			}
		}

		if (local && !local_ip)
		  local_ip = silc_net_localip();
	}

	memset(&params, 0, sizeof(params));
	params.timeout_secs = 60;
	if (local)
	  /* Provide connection point */
	  params.local_ip = local_ip;

	/* Start the file transfer */
	status = silc_client_file_receive(spx->sg->client, spx->sg->conn,
	                                  &params, spx->sg->public_key,
	                                  spx->sg->private_key,
	                                  silcpurple_ftp_monitor, xfer,
	                                  NULL, spx->session_id,
	                                  silcpurple_ftp_ask_name, xfer);
	switch (status) {
	case SILC_CLIENT_FILE_OK:
		silc_free(local_ip);
		silc_free(remote_ip);
		return;
		break;

	case SILC_CLIENT_FILE_UNKNOWN_SESSION:
		purple_notify_error(gc, _("Secure File Transfer"),
			_("No file transfer session active"), NULL,
			purple_request_cpar_from_connection(gc));
		break;

	case SILC_CLIENT_FILE_ALREADY_STARTED:
		purple_notify_error(gc, _("Secure File Transfer"),
			_("File transfer already started"), NULL,
			purple_request_cpar_from_connection(gc));
		break;

	case SILC_CLIENT_FILE_KEY_AGREEMENT_FAILED:
		purple_notify_error(gc, _("Secure File Transfer"),
			_("Could not perform key agreement for file transfer"),
			NULL, purple_request_cpar_from_connection(gc));
		break;

	default:
		purple_notify_error(gc, _("Secure File Transfer"),
			_("Could not start the file transfer"), NULL,
			purple_request_cpar_from_connection(gc));
		break;
	}

	/* Error */
	g_object_unref(xfer);
	silc_free(local_ip);
	silc_free(remote_ip);
}

static void
silcpurple_ftp_request_init(PurpleXfer *xfer)
{
	SilcPurpleXfer *spx = SILCPURPLE_XFER(xfer);

	if (spx->completion) {
		silcpurple_ftp_ask_name_ok(xfer);
	} else {
		silcpurple_ftp_request_result(xfer);
	}
}

static void
silcpurple_ftp_request_denied(PurpleXfer *xfer)
{
	SilcPurpleXfer *spx = SILCPURPLE_XFER(xfer);

	if (!spx) {
		return;
	}

	/* Cancel the transmission */
	if (spx->completion) {
		spx->completion(NULL, spx->completion_context);
	}
	silc_client_file_close(spx->sg->client, spx->sg->conn, spx->session_id);
}

void silcpurple_ftp_request(SilcClient client, SilcClientConnection conn,
			    SilcClientEntry client_entry, SilcUInt32 session_id,
			    const char *hostname, SilcUInt16 port)
{
	PurpleConnection *gc = client->application;
	SilcPurple sg = purple_connection_get_protocol_data(gc);
	SilcPurpleXfer *spx;

	spx = g_object_new(SILCPURPLE_TYPE_XFER,
	                   "account", sg->account,
	                   "type", PURPLE_XFER_TYPE_RECEIVE,
	                   "remote-user", client_entry->nickname,
	                   NULL);

	spx->sg = sg;
	spx->client_entry = client_entry;
	spx->session_id = session_id;
	spx->hostname = g_strdup(hostname);
	spx->port = port;

	purple_xfer_start(PURPLE_XFER(spx), -1, hostname, port);

	/* File transfer request */
	purple_xfer_request(PURPLE_XFER(spx));
}

static void
silcpurple_ftp_send_cancel(PurpleXfer *xfer)
{
	SilcPurpleXfer *spx = SILCPURPLE_XFER(xfer);

	if (!spx) {
		return;
	}

	/* This call will free all resources */
	silc_client_file_close(spx->sg->client, spx->sg->conn, spx->session_id);
}

static void
silcpurple_ftp_send(PurpleXfer *xfer)
{
	SilcPurpleXfer *spx = SILCPURPLE_XFER(xfer);
	const char *name;
	char *local_ip = NULL, *remote_ip = NULL;
	gboolean local = TRUE;
	SilcClientConnectionParams params;
	SilcSocket sock;

	if (!spx) {
		return;
	}

	name = purple_xfer_get_local_filename(xfer);

	silc_socket_stream_get_info(silc_packet_stream_get_stream(spx->sg->conn->stream),
				    &sock, NULL, NULL, NULL);

	/* Do the same magic what we do with key agreement (see silcpurple_buddy.c)
	   to see if we are behind NAT. */
	if (silc_net_check_local_by_sock(sock, NULL, &local_ip)) {
		/* Check if the IP is private */
		if (silcpurple_ip_is_private(local_ip)) {
			local = FALSE;
			/* Local IP is private, resolve the remote server IP to see whether
			   we are talking to Internet or just on LAN. */
			if (silc_net_check_host_by_sock(sock, NULL,
							&remote_ip))
				if (silcpurple_ip_is_private(remote_ip))
					/* We assume we are in LAN.  Let's provide the connection point. */
					local = TRUE;
		}
	}

	if (local && !local_ip)
		local_ip = silc_net_localip();

	memset(&params, 0, sizeof(params));
	params.timeout_secs = 60;
	if (local)
	  /* Provide connection point */
	  params.local_ip = local_ip;

	/* Send the file */
	silc_client_file_send(spx->sg->client, spx->sg->conn,
	                      spx->client_entry, &params,
	                      spx->sg->public_key, spx->sg->private_key,
	                      silcpurple_ftp_monitor, xfer,
	                      name, &spx->session_id);

	silc_free(local_ip);
	silc_free(remote_ip);
}

static void
silcpurple_ftp_send_file_resolved(SilcClient client,
				  SilcClientConnection conn,
				  SilcStatus status,
				  SilcDList clients,
				  void *context)
{
	PurpleConnection *gc = client->application;
	char tmp[256];

	if (!clients) {
		g_snprintf(tmp, sizeof(tmp),
			   _("User %s is not present in the network"),
			   (const char *)context);
		purple_notify_error(gc, _("Secure File Transfer"),
			_("Cannot send file"), tmp,
			purple_request_cpar_from_connection(gc));
		g_free(context);
		return;
	}

	silcpurple_ftp_send_file(NULL, client->application, (const char *)context, NULL);
	g_free(context);
}

PurpleXfer *silcpurple_ftp_new_xfer(PurpleProtocolXfer *prplxfer, PurpleConnection *gc, const char *name)
{
	SilcPurple sg = purple_connection_get_protocol_data(gc);
	SilcClient client = sg->client;
	SilcClientConnection conn = sg->conn;
	SilcDList clients;
	SilcClientEntry client_entry;
	SilcPurpleXfer *spx;

	g_return_val_if_fail(name != NULL, NULL);

	/* Find client entry */
	clients = silc_client_get_clients_local(client, conn, name, FALSE);
	if (!clients) {
		silc_client_get_clients(client, conn, name, NULL,
					silcpurple_ftp_send_file_resolved,
					g_strdup(name));
		return NULL;
	}
	silc_dlist_start(clients);

	client_entry = silc_dlist_get(clients);
	silc_free(clients);

	spx = g_object_new(SILCPURPLE_TYPE_XFER,
	                   "account", sg->account,
	                   "type", PURPLE_XFER_TYPE_SEND,
	                   "remote-user", client_entry->nickname,
	                   NULL);

	spx->sg = sg;
	spx->client_entry = client_entry;

	return PURPLE_XFER(spx);
}

void silcpurple_ftp_send_file(PurpleProtocolXfer *prplxfer, PurpleConnection *gc, const char *name, const char *file)
{
	PurpleXfer *xfer = silcpurple_ftp_new_xfer(prplxfer, gc, name);

	g_return_if_fail(xfer != NULL);

	/* Choose file to send */
	if (file)
		purple_xfer_request_accepted(xfer, file);
	else
		purple_xfer_request(xfer);
}

static void
silcpurple_ftp_init(PurpleXfer *xfer)
{
	switch(purple_xfer_get_xfer_type(xfer)) {
	case PURPLE_XFER_TYPE_SEND:
		silcpurple_ftp_send(xfer);
		break;
	case PURPLE_XFER_TYPE_RECEIVE:
		silcpurple_ftp_request_init(xfer);
		break;
	case PURPLE_XFER_TYPE_UNKNOWN:
		g_return_if_reached();
		break;
	}
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
static void
silcpurple_xfer_init(SilcPurpleXfer *spx)
{
}

static void
silcpurple_xfer_finalize(GObject *obj) {
	SilcPurpleXfer *spx = SILCPURPLE_XFER(obj);

	g_free(spx->hostname);

	G_OBJECT_CLASS(silcpurple_xfer_parent_class)->finalize(obj);
}

static void
silcpurple_xfer_class_init(SilcPurpleXferClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	PurpleXferClass *xfer_class = PURPLE_XFER_CLASS(klass);

	obj_class->finalize = silcpurple_xfer_finalize;

	xfer_class->init = silcpurple_ftp_init;
	xfer_class->request_denied = silcpurple_ftp_request_denied;
	xfer_class->cancel_send = silcpurple_ftp_send_cancel;
	xfer_class->cancel_recv = silcpurple_ftp_cancel;
}

static void
silcpurple_xfer_class_finalize(SilcPurpleXferClass *klass) {
}

/******************************************************************************
 * Public API
 *****************************************************************************/
void
silcpurple_xfer_register(GTypeModule *module) {
	silcpurple_xfer_register_type(module);
}
