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
  gint64 t0, tdiff;
  struct timeval tv;
  fd_set fdmask;
  int i, fd;

  retval = ZCheckIfNotice (notice, (struct sockaddr_in *) 0, pred,
			   (char *) arg);
  if (retval == ZERR_NONE)
    return ZERR_NONE;
  if (retval != ZERR_NONOTICE)
    return retval;

  fd = ZGetFD ();
  FD_ZERO (&fdmask);
  tv.tv_sec = timeout;
  tv.tv_usec = 0;
  t0 = g_get_monotonic_time() + timeout * G_USEC_PER_SEC;
  while (1) {
    FD_SET (fd, &fdmask);
    i = select (fd + 1, &fdmask, (fd_set *) 0, (fd_set *) 0, &tv);
    if (i == 0)
      return ETIMEDOUT;
    if (i < 0 && errno != EINTR)
      return errno;
    if (i > 0) {
      retval = ZCheckIfNotice (notice, (struct sockaddr_in *) 0, pred,
			       (char *) arg);
      if (retval != ZERR_NONOTICE) /* includes ZERR_NONE */
	return retval;
    }
		tdiff = t0 - g_get_monotonic_time();
		tv.tv_sec = tdiff / G_USEC_PER_SEC;
		tv.tv_usec = tdiff - tv.tv_sec * G_USEC_PER_SEC;
  }
  /*NOTREACHED*/
}
