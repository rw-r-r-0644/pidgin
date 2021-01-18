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

#include "pidgin/pidginstylecontext.h"

/* Assume light mode */
static gboolean dark_mode_have_cache = FALSE;
static gboolean dark_mode_cached_value = FALSE;

/******************************************************************************
 * Public API
 *****************************************************************************/
void
pidgin_style_context_get_background_color(GdkRGBA *color) {
	/* This value will leak, we could put a shutdown function in but right now
	 * that seems like a bit much for a few bytes.
	 */
	static GdkRGBA *background = NULL;

	if(g_once_init_enter(&background)) {
		GdkRGBA *bg = NULL;
		GtkStyleContext *context = NULL;
		GtkWidget *window = NULL;

		/* We create a window to get its background color from its style
		 * context.  This _is_ doable without creating a window, but you still
		 * need the window class and about four times as much code, so that's
		 * why we do it this way.
		 */
		window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		context = gtk_widget_get_style_context(window);

		gtk_style_context_get(context, GTK_STATE_FLAG_NORMAL,
		                      GTK_STYLE_PROPERTY_BACKGROUND_COLOR, &bg,
		                      NULL);
		g_object_unref(G_OBJECT(window));

		g_once_init_leave(&background, bg);
	}

	*color = *background;
}

gboolean
pidgin_style_context_is_dark(void) {
	GdkRGBA bg;
	gdouble luminance = 0.0;

	if(dark_mode_have_cache) {
		return dark_mode_cached_value;
	}

	pidgin_style_context_get_background_color(&bg);

	/* 709 coefficients from https://en.wikipedia.org/wiki/Luma_(video) */
	luminance = (0.2126 * bg.red) + (0.7152 * bg.green) + (0.0722 * bg.blue);

	dark_mode_cached_value = (luminance < 0.5);
	dark_mode_have_cache = TRUE;

	return dark_mode_cached_value;
}
