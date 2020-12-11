/* This file is part of the Project Athena Zephyr Notification System.
 * It contains source for the ZSendPacket function.
 *
 *	Created by:	Robert French
 *
 *	Copyright (c) 1987,1991 by the Massachusetts Institute of Technology.
 *	For copying and distribution information, see the file
 *	"mit-copyright.h".
 */

#include <purple.h>

#include "internal.h"
#ifdef WIN32
#include <winsock.h>
#else
#include <sys/socket.h>
#endif

static int wait_for_hmack(ZNotice_t *notice, void *uid);

Code_t
ZSendPacket(char *packet, int len, int waitforack)
{
	GError *error = NULL;
	Code_t retval;
	ZNotice_t notice, acknotice;

	if (packet == NULL || len < 0) {
		return ZERR_ILLVAL;
	}

	if (len > Z_MAXPKTLEN) {
		return ZERR_PKTLEN;
	}

	if (ZGetSocket() == NULL) {
		retval = ZOpenPort(NULL);
		if (retval != ZERR_NONE) {
			return retval;
		}
	}

	if (g_socket_send_to(ZGetSocket(), ZGetDestAddr(), packet, len, NULL,
	                     &error) < 0) {
		purple_debug_error("zephyr", "Failed to send packet of size %d: %s",
		                   len, error->message);
		g_error_free(error);
		return ZERR_INTERNAL;
	}

	if (!waitforack) {
		return ZERR_NONE;
	}

	if ((retval = ZParseNotice(packet, len, &notice)) != ZERR_NONE) {
		return retval;
	}

	retval = Z_WaitForNotice(&acknotice, wait_for_hmack, &notice.z_uid,
	                         HM_TIMEOUT);
	if (retval == ETIMEDOUT) {
		return ZERR_HMDEAD;
	}
	if (retval == ZERR_NONE) {
		ZFreeNotice(&acknotice);
	}
	return retval;
}

static int wait_for_hmack(ZNotice_t *notice, void *uid)
{
    return (notice->z_kind == HMACK && ZCompareUID(&notice->z_uid, (ZUnique_Id_t *)uid));
}
