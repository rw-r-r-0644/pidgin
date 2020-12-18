/*
 * Pidgin - Internet Messenger
 * Copyright (C) Pidgin Developers <devel@pidgin.im>
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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

#if !defined(PIDGIN_GLOBAL_HEADER_INSIDE) && !defined(PIDGIN_COMPILATION)
# error "only <pidgin.h> may be included directly"
#endif

#ifndef PIDGIN_PRESENCE_H
#define PIDGIN_PRESENCE_H

/**
 * SECTION:pidginpresence
 * @section_id: pidgin-presence
 * @short_description: Presence Utilities
 * @title: Presence Utility Functions
 *
 * Displaying #PurplePresence's can be tricky.  These functions make it easier.
 */

#include <gtk/gtk.h>

#include <purple.h>

G_BEGIN_DECLS

/**
 * pidgin_presence_get_icon_name:
 * @presence: The #PurplePresence instance.
 * @fallback: The icon name to fall back to.
 *
 * Gets the icon name that should be used to represent @presence falling back
 * to @fallback if @presence is invalid.
 *
 * Returns: The icon name to represent @presence.
 */
const gchar *pidgin_presence_get_icon_name(PurplePresence *presence, const gchar *fallback);

G_END_DECLS

#endif /* PIDGIN_PRESENCE_H */
