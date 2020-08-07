/* pidgin
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#if !defined(PIDGIN_GLOBAL_HEADER_INSIDE) && !defined(PIDGIN_COMPILATION)
# error "only <pidgin.h> may be included directly"
#endif

#ifndef PIDGIN_CORE_H
#define PIDGIN_CORE_H
/**
 * SECTION:core
 * @section_id: pidgin-core
 * @short_description: <filename>pidgincore.h</filename>
 * @title: UI Definitions and Includes
 */

#include <glib.h>

#ifdef _WIN32
#  include "win32/gtkwin32dep.h"
#endif

/**
 * PIDGIN_UI:
 *
 * Our UI's identifier.
 */
/* leave this as gtk-gaim until we have a decent way to migrate UI-prefs */
#define PIDGIN_UI "gtk-gaim"

/* change this only when we have a sane upgrade path for old prefs */
#define PIDGIN_PREFS_ROOT "/pidgin"

/* Translators may want to transliterate the name.
 It is not to be translated. */
#define PIDGIN_NAME _("Pidgin")

#ifndef _WIN32
# define PIDGIN_ALERT_TITLE ""
#else
# define PIDGIN_ALERT_TITLE PIDGIN_NAME
#endif

/**
 * pidgin_start:
 *
 * Start pidgin with the given command line arguments.
 */
int pidgin_start(int argc, char *argv[]);

#endif /* PIDGIN_CORE_H */

