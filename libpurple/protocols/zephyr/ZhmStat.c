/* This file is part of the Project Athena Zephyr Notification System.
 * It contains the ZhmStat() function.
 *
 *      Created by:     Marc Horowitz
 *
 *      Copyright (c) 1996 by the Massachusetts Institute of Technology.
 *      For copying and distribution information, see the file
 *      "mit-copyright.h".
 */

#include "internal.h"

#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

Code_t
ZhmStat(ZNotice_t *notice)
{
	struct servent *sp;
	GInetAddress *inet_addr = NULL;
	GSocketAddress *addr = NULL;
	ZNotice_t req;
	Code_t code;
	GError *error = NULL;

	sp = getservbyname(HM_SVCNAME, "udp");

	memset(&req, 0, sizeof(req));
	req.z_kind = STAT;
	req.z_port = 0;
	req.z_class = HM_STAT_CLASS;
	req.z_class_inst = HM_STAT_CLIENT;
	req.z_opcode = HM_GIMMESTATS;
	req.z_sender = "";
	req.z_recipient = "";
	req.z_default_format = "";
	req.z_message_len = 0;

	inet_addr = g_inet_address_new_loopback(G_SOCKET_FAMILY_IPV4);
	addr = g_inet_socket_address_new(inet_addr,
	                                 sp ? sp->s_port : HM_SVC_FALLBACK);
	code = ZSetDestAddr(addr);
	g_object_unref(inet_addr);
	g_object_unref(addr);
	if (code != ZERR_NONE) {
		return code;
	}

	if ((code = ZSendNotice(&req, ZNOAUTH)) != ZERR_NONE) {
		return code;
	}

	/* Wait up to ten seconds for a response. */
	if (!g_socket_condition_timed_wait(ZGetSocket(), G_IO_IN,
	                                   10 * G_USEC_PER_SEC, NULL, &error)) {
		gint ret = ZERR_INTERNAL;
		if (error->code == G_IO_ERROR_TIMED_OUT) {
			ret = ETIMEDOUT;
		}
		g_error_free(error);
		return ret;
	}

	if (ZPending() == 0) {
		return ZERR_HMDEAD;
	}

	return ZReceiveNotice(notice, NULL);
}
