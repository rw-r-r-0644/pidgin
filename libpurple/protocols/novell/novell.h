/* purple
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */

#ifndef PURPLE_NOVELL_NOVELL_H
#define PURPLE_NOVELL_NOVELL_H

#include <purple.h>

#define NOVELL_TYPE_PROTOCOL (novell_protocol_get_type())
G_DECLARE_FINAL_TYPE(NovellProtocol, novell_protocol, NOVELL, PROTOCOL,
                     PurpleProtocol)

#endif /* PURPLE_NOVELL_NOVELL_H */
