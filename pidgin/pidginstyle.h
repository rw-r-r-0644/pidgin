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
 * SECTION:pidginstyle
 * @section_id: pidgin-pidginstyle
 * @short_description: <filename>gtkstyle.h</filename>
 * @title: Style API
 *
 * The style API contains some helpers regarding #GtkStyle.
 */

#ifndef PIDGIN_STYLE_H
#define PIDGIN_STYLE_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

/**
 * pidgin_style_is_dark:
 * @style: The #GtkStyle in use, or %NULL to use a cached version.
 *
 * Gets whether or not dark mode is enabled.
 *
 * Returns: %TRUE if dark mode is enabled and foreground colours should be
 *          inverted.
 */

gboolean pidgin_style_is_dark(GtkStyle *style);

/**
 * pidgin_style_adjust_contrast:
 * @style: The #GtkStyle in use.
 * @color: (inout): Color to be lightened.  Transformed color will be written
 *         here.
 *
 * Lighten a color if dark mode is enabled.
 */

void pidgin_style_adjust_contrast(GtkStyle *style, GdkRGBA *color);

G_END_DECLS

#endif /* PIDGIN_STYLE_H */

