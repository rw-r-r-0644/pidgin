/* This file is part of the Project Athena Zephyr Notification System.
 * It contains source for the ZClosePort function.
 *
 *	Created by:	Robert French
 *
 *	Copyright (c) 1987 by the Massachusetts Institute of Technology.
 *	For copying and distribution information, see the file
 *	"mit-copyright.h".
 */

#include "internal.h"

Code_t
ZClosePort(void)
{
	if (__Zephyr_fd >= 0) {
		(void) close(__Zephyr_fd);
	}

    __Zephyr_fd = -1;

    return (ZERR_NONE);
}
