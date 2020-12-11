/* This file is part of the Project Athena Zephyr Notification System.
 * It contains the ZCheckIfNotice/select loop used for waiting for
 * a notice, with a timeout.
 *
 *	Copyright (c) 1991 by the Massachusetts Institute of Technology.
 *	For copying and distribution information, see the file
 *	"mit-copyright.h".
 */

#include "internal.h"

#ifdef WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

Code_t
Z_WaitForNotice(ZNotice_t *notice, int (*pred)(ZNotice_t *, void *), void *arg,
                int timeout)
{
	Code_t retval;
	gint64 t0, tdiff, timeout_us;
	GError *error = NULL;

	retval = ZCheckIfNotice(notice, NULL, pred, (char *)arg);
	if (retval == ZERR_NONE) {
		return ZERR_NONE;
	}
	if (retval != ZERR_NONOTICE) {
		return retval;
	}

	timeout_us = timeout * G_USEC_PER_SEC;
	t0 = g_get_monotonic_time() + timeout_us;
	while (TRUE) {
		if (!g_socket_condition_timed_wait(ZGetSocket(), G_IO_IN, timeout_us,
		                                   NULL, &error)) {
			gint ret = ZERR_INTERNAL;
			if (error->code == G_IO_ERROR_TIMED_OUT) {
				ret = ETIMEDOUT;
			}
			g_error_free(error);
			return ret;
		}

		retval = ZCheckIfNotice(notice, NULL, pred, (char *)arg);
		if (retval != ZERR_NONOTICE) {
			/* includes ZERR_NONE */
			return retval;
		}

		tdiff = t0 - g_get_monotonic_time();
		if (tdiff < 0) {
			return ETIMEDOUT;
		}
		/* g_socket_condition_timed_wait requires timeout to be an integral
		 * number of milliseconds. */
		timeout_us = (tdiff / 1000 + 1) * 1000;
	}
	/* NOTREACHED */
}
