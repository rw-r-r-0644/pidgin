/* This file is part of the Project Athena Zephyr Notification System.
 * It contains source for the ZInitialize function.
 *
 *	Created by:	Robert French
 *
 *	Copyright (c) 1987, 1991 by the Massachusetts Institute of Technology.
 *	For copying and distribution information, see the file
 *	"mit-copyright.h".
 */

#ifdef ZEPHYR_USES_KERBEROS
#ifdef WIN32

#else
#include <krb_err.h>
#endif
#endif

#include "internal.h"

#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif


#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

Code_t
ZInitialize(void)
{
	struct servent *hmserv;
	GResolver *resolver = NULL;
	GList *hosts = NULL;
	guint32 servaddr;
	guint16 port;
	Code_t code;
	ZNotice_t notice;
#ifdef ZEPHYR_USES_KERBEROS
	gchar *krealm = NULL;
	gint krbval;
	gchar d1[ANAME_SZ], d2[INST_SZ];
#endif

	g_clear_object(&__HM_addr);

	hmserv = (struct servent *)getservbyname(HM_SVCNAME, "udp");
	port = hmserv ? hmserv->s_port : HM_SVC_FALLBACK;

	/* Set up local loopback address for HostManager */
	__HM_addr = g_inet_socket_address_new_from_string("127.0.0.1", port);

	/* Initialize the input queue */
	g_queue_init(&Z_input_queue);

	/* If there is no zhm, the code will fall back to something which might
	 * not be "right", but this is is ok, since none of the servers call
	 * krb_rd_req. */

	servaddr = INADDR_NONE;
	if ((code = ZOpenPort(NULL)) != ZERR_NONE) {
		return code;
	}

	if ((code = ZhmStat(&notice)) != ZERR_NONE) {
		return code;
	}

	ZClosePort();

	/* the first field, which is NUL-terminated, is the server name.
	   If this code ever support a multiplexing zhm, this will have to
	   be made smarter, and probably per-message */

#ifdef ZEPHYR_USES_KERBEROS
	krealm = krb_realmofhost(notice.z_message);
#endif
	resolver = g_resolver_get_default();
	hosts = g_resolver_lookup_by_name(resolver, notice.z_message, NULL, NULL);
	while (hosts) {
		GInetAddress *host_addr = hosts->data;
		if (g_inet_address_get_family(host_addr) == G_SOCKET_FAMILY_IPV4) {
			memcpy(&servaddr, g_inet_address_to_bytes(host_addr),
			       sizeof(servaddr));
			g_list_free_full(hosts, g_object_unref);
			hosts = NULL;
			break;
		}
		g_object_unref(host_addr);
		hosts = g_list_delete_link(hosts, hosts);
	}

	ZFreeNotice(&notice);

#ifdef ZEPHYR_USES_KERBEROS
	if (krealm) {
		g_strlcpy(__Zephyr_realm, krealm, REALM_SZ);
	} else if ((krb_get_tf_fullname(TKT_FILE, d1, d2, __Zephyr_realm) !=
	            KSUCCESS) &&
	           ((krbval = krb_get_lrealm(__Zephyr_realm, 1)) != KSUCCESS)) {
		g_object_unref(resolver);
		return krbval;
	}
#else
	g_strlcpy(__Zephyr_realm, "local-realm", REALM_SZ);
#endif

	__My_addr = INADDR_NONE;
	if (servaddr != INADDR_NONE) {
		/* Try to get the local interface address by connecting a UDP
		 * socket to the server address and getting the local address.
		 * Some broken operating systems (e.g. Solaris 2.0-2.5) yield
		 * INADDR_ANY (zero), so we have to check for that. */
		GSocket *s = g_socket_new(G_SOCKET_FAMILY_IPV4, G_SOCKET_TYPE_DATAGRAM,
		                          G_SOCKET_PROTOCOL_DEFAULT, NULL);
		if (s != NULL) {
			GSocketAddress *remote_addr = NULL;
			GInetAddress *inet_addr = NULL;

			inet_addr = g_inet_address_new_from_bytes((const guint8 *)&servaddr,
			                                          G_SOCKET_FAMILY_IPV4);
			remote_addr =
			        g_inet_socket_address_new(inet_addr, HM_SRV_SVC_FALLBACK);
			g_object_unref(inet_addr);

			if (g_socket_connect(s, remote_addr, NULL, NULL)) {
				GSocketAddress *local_addr =
				        g_socket_get_local_address(s, NULL);
				if (local_addr) {
					inet_addr = g_inet_socket_address_get_address(
					        G_INET_SOCKET_ADDRESS(local_addr));
					if (inet_addr) {
						memcpy(&__My_addr, g_inet_address_to_bytes(inet_addr),
						       sizeof(__My_addr));
						g_object_unref(inet_addr);
					}
					g_object_unref(local_addr);
				}
			}
			g_object_unref(remote_addr);
			g_object_unref(s);
		}
	}
	if (__My_addr == INADDR_NONE) {
		/* We couldn't figure out the local interface address by the
		 * above method.  Try by resolving the local hostname.  (This
		 * is a pretty broken thing to do, and unfortunately what we
		 * always do on server machines.) */
		const gchar *hostname = g_get_host_name();
		hosts = g_resolver_lookup_by_name(resolver, hostname, NULL, NULL);
		while (hosts) {
			GInetAddress *host_addr = hosts->data;
			if (g_inet_address_get_family(host_addr) == G_SOCKET_FAMILY_IPV4) {
				memcpy(&servaddr, g_inet_address_to_bytes(host_addr),
				       sizeof(servaddr));
				g_list_free_full(hosts, g_object_unref);
				hosts = NULL;
				break;
			}
			g_object_unref(host_addr);
			hosts = g_list_delete_link(hosts, hosts);
		}
	}
	/* If the above methods failed, zero out __My_addr so things will sort
	 * of kind of work. */
	if (__My_addr == INADDR_NONE) {
		__My_addr = 0;
	}

	g_object_unref(resolver);

	/* Get the sender so we can cache it */
	ZGetSender();

	return ZERR_NONE;
}

