/**
 * @file ssl-nss.c SSL Operations for Mozilla NSS
 * @ingroup core
 *
 * gaim
 *
 * Copyright (C) 2003 Christian Hammond <chipx86@gnupdate.org>
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
#include "internal.h"

#ifdef HAVE_NSS

#include "debug.h"
#include "sslconn.h"

#include <nspr.h>
#include <private/pprio.h>
#include <nss.h>
#include <pk11func.h>
#include <prio.h>
#include <secerr.h>
#include <secmod.h>
#include <ssl.h>
#include <sslerr.h>
#include <sslproto.h>

typedef struct
{
	PRFileDesc *fd;
	PRFileDesc *in;

} GaimSslNssData;

#define GAIM_SSL_NSS_DATA(gsc) ((GaimSslNssData *)gsc->private_data)

static const PRIOMethods *_nss_methods = NULL;
static PRDescIdentity _identity;

static SECStatus
ssl_auth_cert(void *arg, PRFileDesc *socket, PRBool checksig,
			  PRBool is_server)
{
	return SECSuccess;

#if 0
	CERTCertificate *cert;
	void *pinArg;
	SECStatus status;

	cert = SSL_PeerCertificate(socket);
	pinArg = SSL_RevealPinArg(socket);

	status = CERT_VerifyCertNow((CERTCertDBHandle *)arg, cert, checksig,
								certUsageSSLClient, pinArg);

	if (status != SECSuccess) {
		gaim_debug_error("nss", "CERT_VerifyCertNow failed\n");
		CERT_DestroyCertificate(cert);
		return status;
	}

	CERT_DestroyCertificate(cert);
	return SECSuccess;
#endif
}

SECStatus
ssl_bad_cert(void *arg, PRFileDesc *socket)
{
	SECStatus status = SECFailure;
	PRErrorCode err;

	if (arg == NULL)
		return status;

	*(PRErrorCode *)arg = err = PORT_GetError();

	switch (err)
	{
		case SEC_ERROR_INVALID_AVA:
		case SEC_ERROR_INVALID_TIME:
		case SEC_ERROR_BAD_SIGNATURE:
		case SEC_ERROR_EXPIRED_CERTIFICATE:
		case SEC_ERROR_UNKNOWN_ISSUER:
		case SEC_ERROR_UNTRUSTED_CERT:
		case SEC_ERROR_CERT_VALID:
		case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:
		case SEC_ERROR_CRL_EXPIRED:
		case SEC_ERROR_CRL_BAD_SIGNATURE:
		case SEC_ERROR_EXTENSION_VALUE_INVALID:
		case SEC_ERROR_CA_CERT_INVALID:
		case SEC_ERROR_CERT_USAGES_INVALID:
		case SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION:
			status = SECSuccess;
			break;

		default:
			status = SECFailure;
			break;
	}

	gaim_debug_error("nss", "Bad certificate: %d\n");

	return status;
}

static gboolean
ssl_nss_init(void)
{
	PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
	NSS_NoDB_Init(NULL);

	/* TODO: Fix this so autoconf does the work trying to find this lib. */
	SECMOD_AddNewModule("Builtins", LIBDIR "/libnssckbi.so", 0, 0);
	NSS_SetDomesticPolicy();

	_identity = PR_GetUniqueIdentity("Gaim");
	_nss_methods = PR_GetDefaultIOMethods();

	return TRUE;
}

static void
ssl_nss_uninit(void)
{
	PR_Cleanup();

	_nss_methods = NULL;
}

static void
ssl_nss_connect_cb(gpointer data, gint source, GaimInputCondition cond)
{
	GaimSslConnection *gsc = (GaimSslConnection *)data;
	GaimSslNssData *nss_data = g_new0(GaimSslNssData, 1);
	PRSocketOptionData socket_opt;

	gsc->private_data = nss_data;

	gsc->fd = source;

	nss_data->fd = PR_ImportTCPSocket(gsc->fd);

	if (nss_data->fd == NULL)
	{
		gaim_debug_error("nss", "nss_data->fd == NULL!\n");

		gaim_ssl_close((GaimSslConnection *)gsc);

		return;
	}

	socket_opt.option = PR_SockOpt_Nonblocking;
	socket_opt.value.non_blocking = PR_FALSE;

	PR_SetSocketOption(nss_data->fd, &socket_opt);

	nss_data->in = SSL_ImportFD(NULL, nss_data->fd);

	if (nss_data->in == NULL)
	{
		gaim_debug_error("nss", "nss_data->in == NUL!\n");

		gaim_ssl_close((GaimSslConnection *)gsc);

		return;
	}

	SSL_OptionSet(nss_data->in, SSL_SECURITY,            PR_TRUE);
	SSL_OptionSet(nss_data->in, SSL_HANDSHAKE_AS_CLIENT, PR_TRUE);

	SSL_AuthCertificateHook(nss_data->in,
							(SSLAuthCertificate)ssl_auth_cert,
							(void *)CERT_GetDefaultCertDB());
	SSL_BadCertHook(nss_data->in, (SSLBadCertHandler)ssl_bad_cert, NULL);

	SSL_SetURL(nss_data->in, gsc->host);

	SSL_ResetHandshake(nss_data->in, PR_FALSE);

	if (SSL_ForceHandshake(nss_data->in))
	{
		gaim_debug_error("nss", "Handshake failed\n");

		gaim_ssl_close(gsc);

		return;
	}

	gsc->connect_cb(gsc->connect_cb_data, gsc, cond);
}

static void
ssl_nss_recv_cb(gpointer data, gint source, GaimInputCondition cond)
{
	GaimSslConnection *gsc = data;
	gsc->recv_cb(gsc->recv_cb_data, gsc, cond);
}

static void
ssl_nss_close(GaimSslConnection *gsc)
{
	GaimSslNssData *nss_data = GAIM_SSL_NSS_DATA(gsc);

	if (nss_data->in) PR_Close(nss_data->in);
	if (nss_data->fd) PR_Close(nss_data->fd);

	g_free(nss_data);
}

static size_t
ssl_nss_read(GaimSslConnection *gsc, void *data, size_t len)
{
	GaimSslNssData *nss_data = GAIM_SSL_NSS_DATA(gsc);

	return PR_Read(nss_data->in, data, len);
}

static size_t
ssl_nss_write(GaimSslConnection *gsc, const void *data, size_t len)
{
	GaimSslNssData *nss_data = GAIM_SSL_NSS_DATA(gsc);

	return PR_Write(nss_data->in, data, len);
}

static GaimSslOps ssl_ops =
{
	ssl_nss_init,
	ssl_nss_uninit,
	ssl_nss_connect_cb,
	ssl_nss_recv_cb,
	ssl_nss_close,
	ssl_nss_read,
	ssl_nss_write
};

GaimSslOps *
gaim_ssl_nss_get_ops()
{
	return &ssl_ops;
}

#endif /* HAVE_NSS */
