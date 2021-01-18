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
 * along with this library; if not, see <https://www.gnu.org/licenses/>.
 */

#if !defined(PIDGIN_GLOBAL_HEADER_INSIDE) && !defined(PIDGIN_COMPILATION)
# error "only <pidgin.h> may be included directly"
#endif

/**
 * SECTION:pidginstylecontext
 * @section_id: pidgin-pidginstylecontext
 * @short_description: Dark theme helpers.
 * @title: GtkStyleContext Helpers
 *
 * A collection of API to help with handling dark themes.
 */

#ifndef PIDGIN_STYLE_CONTEXT_H
#define PIDGIN_STYLE_CONTEXT_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * pidgin_style_context_get_background_color:
 * @color: (out): A return address of a #GdkRGBA for the background color.
 *
 * Gets the background color for #GtkWindow in the currently selected theme
 * and sets @color to that value.
 *
 * Since: 3.0.0
 */
void pidgin_style_context_get_background_color(GdkRGBA *color);

/**
 * pidgin_style_context_is_dark:
 *
 * Gets whether or not dark mode is enabled.
 *
 * Returns: %TRUE if dark mode is enabled and foreground colours should be
 *          inverted.
 *
 * Since: 3.0.0
 */
gboolean pidgin_style_context_is_dark(void);

G_END_DECLS

#endif /* PIDGIN_STYLE_CONTEXT_H */

