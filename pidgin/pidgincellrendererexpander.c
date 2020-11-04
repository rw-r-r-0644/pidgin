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

#include "pidgin/pidgincellrendererexpander.h"

struct _PidginCellRendererExpander {
	GtkCellRenderer parent;
};

G_DEFINE_TYPE(PidginCellRendererExpander, pidgin_cell_renderer_expander,
              GTK_TYPE_CELL_RENDERER)

/******************************************************************************
 * GtkCellRenderer Implementation
 *****************************************************************************/
static void
pidgin_cell_renderer_expander_get_size(GtkCellRenderer *renderer,
                                       GtkWidget *widget,
                                       const GdkRectangle *cell_area,
                                       gint *x_offset, gint *y_offset,
                                       gint *width, gint *height)
{
	gint calc_width = 0, calc_height = 0;
	gint xpad = 0, ypad = 0;
	gint expander_size = 0;

	gtk_widget_style_get(widget, "expander-size", &expander_size, NULL);

	gtk_cell_renderer_get_padding(renderer, &xpad, &ypad);
	calc_width = (xpad * 2) + expander_size;
	calc_height = (ypad * 2) + expander_size;

	if(width) {
		*width = calc_width;
	}

	if(height) {
		*height = calc_height;
	}

	if(cell_area) {
		gfloat xalign = 0.0f, yalign = 0.0f;

		gtk_cell_renderer_get_alignment(renderer, &xalign, &yalign);

		if(x_offset) {
			*x_offset = (gint)(xalign * (cell_area->width - calc_width));
			*x_offset = MAX (*x_offset, 0);
		}

		if(y_offset) {
			*y_offset = (gint)(yalign * (cell_area->height - calc_height));
			*y_offset = MAX (*y_offset, 0);
		}
	}
}

static void
pidgin_cell_renderer_expander_render(GtkCellRenderer *renderer, cairo_t *cr,
                                     GtkWidget *widget,
                                     const GdkRectangle *background_area,
                                     const GdkRectangle *cell_area,
                                     GtkCellRendererState flags)
{
	GtkStateFlags state;
	GtkStyleContext *context = NULL;
	gboolean is_expanded = FALSE, is_expander = FALSE;
	gint xpad = 0, ypad = 0;
	gint width = cell_area->width, height = cell_area->height;

	/* if the row doesn't have children, bail out. */
	g_object_get(G_OBJECT(renderer), "is-expander", &is_expander, NULL);
	if(!is_expander) {
		return;
	}

	/* Figure out the state of the renderer. */
	if(!gtk_widget_get_sensitive(widget)) {
		state = GTK_STATE_FLAG_INSENSITIVE;
	} else if(flags & GTK_CELL_RENDERER_PRELIT) {
		state = GTK_STATE_FLAG_PRELIGHT;
	} else if(gtk_widget_has_focus(widget) && flags & GTK_CELL_RENDERER_SELECTED) {
		state = GTK_STATE_FLAG_ACTIVE;
	} else {
		state = GTK_STATE_FLAG_NORMAL;
	}

	g_object_get(G_OBJECT(renderer), "is-expanded", &is_expanded, NULL);
	if(is_expanded) {
		state |= GTK_STATE_FLAG_CHECKED;
	} else {
		state &= ~GTK_STATE_FLAG_CHECKED;
	}

	/* Build our style context */
	context = gtk_widget_get_style_context(widget);
	gtk_style_context_add_class(context, GTK_STYLE_CLASS_VIEW);
	gtk_style_context_add_class(context, GTK_STYLE_CLASS_EXPANDER);
	gtk_style_context_set_state(context, state);

	/* Adjust the width and height according to the padding. */
	gtk_cell_renderer_get_padding(renderer, &xpad, &ypad);
	width -= xpad * 2;
	height -= ypad * 2;

	/* Render the arrow. */
	gtk_render_expander(context, cr,
	                    cell_area->x + xpad, cell_area->y + ypad,
	                    width, height);
}

static gboolean
pidgin_cell_renderer_expander_activate(GtkCellRenderer *r, GdkEvent *event,
                                       GtkWidget *widget, const gchar *p,
                                       const GdkRectangle *bg,
                                       const GdkRectangle *cell,
                                       GtkCellRendererState flags)
{
	GtkTreePath *path = gtk_tree_path_new_from_string(p);

	if(gtk_tree_view_row_expanded(GTK_TREE_VIEW(widget), path)) {
		gtk_tree_view_collapse_row(GTK_TREE_VIEW(widget), path);
	} else {
		gtk_tree_view_expand_row(GTK_TREE_VIEW(widget),path,FALSE);
	}

	gtk_tree_path_free(path);

	return FALSE;
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
static void
pidgin_cell_renderer_expander_init (PidginCellRendererExpander *renderer) {
	/* there's no accessor for setting the mode, so we have to set the property
	 * explicitly.
	 */
	g_object_set(G_OBJECT(renderer), "mode",
	             GTK_CELL_RENDERER_MODE_ACTIVATABLE, NULL);

	gtk_cell_renderer_set_padding(GTK_CELL_RENDERER(renderer), 0, 2);
}

static void
pidgin_cell_renderer_expander_class_init(PidginCellRendererExpanderClass *klass)
{
	GtkCellRendererClass *renderer_class = GTK_CELL_RENDERER_CLASS(klass);

	renderer_class->get_size = pidgin_cell_renderer_expander_get_size;
	renderer_class->render = pidgin_cell_renderer_expander_render;
	renderer_class->activate = pidgin_cell_renderer_expander_activate;
}

/******************************************************************************
 * Public API
 *****************************************************************************/
GtkCellRenderer *
pidgin_cell_renderer_expander_new(void) {
	return g_object_new(PIDGIN_TYPE_CELL_RENDERER_EXPANDER, NULL);
}
