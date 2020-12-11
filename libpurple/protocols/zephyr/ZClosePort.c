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
	g_clear_object(&__Zephyr_socket);
	ZSetDestAddr(NULL);

	return ZERR_NONE;
}
