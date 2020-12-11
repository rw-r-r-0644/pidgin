/* This file is part of the Project Athena Zephyr Notification System.
 * It contains source for the ZSetDestAddr function.
 *
 *	Created by:	Robert French
 *
 *	Copyright (c) 1987 by the Massachusetts Institute of Technology.
 *	For copying and distribution information, see the file
 *	"mit-copyright.h".
 */

#include "internal.h"

Code_t
ZSetDestAddr(GSocketAddress *addr)
{
	g_set_object(&__HM_addr, addr);

	return ZERR_NONE;
}
