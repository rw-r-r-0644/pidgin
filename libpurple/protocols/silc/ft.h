/*
 * Purple - Internet Messaging Library
 * Copyright (C) Pidgin Developers <devel@pidgin.im>
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
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
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#ifndef PURPLE_SILC_FT_H
#define PURPLE_SILC_FT_H

#include <purple.h>

G_BEGIN_DECLS

#define SILCPURPLE_TYPE_XFER (silcpurple_xfer_get_type())
G_DECLARE_FINAL_TYPE(SilcPurpleXfer, silcpurple_xfer, SILCPURPLE, XFER, PurpleXfer);

void silcpurple_xfer_register(GTypeModule *module);

G_END_DECLS

#endif /* PURPLE_SILC_FT_H */
