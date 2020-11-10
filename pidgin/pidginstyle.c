/*
 * Pidgin - Internet Messenger
 * Copyright (C) Pidgin Developers <devel@pidgin.im>
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * under the terms of the GNU General Public License as published by
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

#include "pidgin/pidginstyle.h"

/* Assume light mode */
static gboolean dark_mode_have_cache = FALSE;
static gboolean dark_mode_cached_value = FALSE;

gboolean
pidgin_style_is_dark(GtkStyle *style) {
	G_GNUC_BEGIN_IGNORE_DEPRECATIONS

	GdkColor bg;
	gdouble luminance = 0.0;

	if(style == NULL) {
		if(dark_mode_have_cache) {
			return dark_mode_cached_value;
		}

		style = gtk_widget_get_default_style();
	}

	bg = style->base[GTK_STATE_NORMAL];

	/* magic values are taken from https://en.wikipedia.org/wiki/Luma_(video)
	 * Rec._601_luma_versus_Rec._709_luma_coefficients.
	 */
	luminance = (0.299 * bg.red) + (0.587 * bg.green) + (0.114 * bg.blue);

	G_GNUC_END_IGNORE_DEPRECATIONS

	dark_mode_cached_value = (luminance < 0x7FFF);
	dark_mode_have_cache = TRUE;

	return dark_mode_cached_value;
}

void
pidgin_style_adjust_contrast(GtkStyle *style, GdkRGBA *rgba) {
	if (pidgin_style_is_dark(style)) {
		gdouble h, s, v;

		gtk_rgb_to_hsv(rgba->red, rgba->green, rgba->blue, &h, &s, &v);

		v += 0.3;
		v = v > 1.0 ? 1.0 : v;
		s = 0.7;

		gtk_hsv_to_rgb(h, s, v, &rgba->red, &rgba->green, &rgba->blue);
	}
}
