/* gtkxcellrendererexpander.h
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifndef _GAIM_GTKCELLRENDEREREXPANDER_H_
#define _GAIM_GTKCELLRENDEREREXPANDER_H_

#include <gtk/gtkcellrenderer.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GAIM_TYPE_GTK_CELL_RENDERER_EXPANDER         (gaim_gtk_cell_renderer_expander_get_type())
#define GAIM_GTK_CELL_RENDERER_EXPANDER(obj)         (G_TYPE_CHECK_INSTANCE_CAST((obj), GAIM_TYPE_GTK_CELL_RENDERER_EXPANDER, GaimGtkCellRendererExpander))
#define GAIM_GTK_CELL_RENDERER_EXPANDER_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GAIM_TYPE_GTK_CELL_RENDERER_EXPANDER, GaimGtkCellRendererExpanderClass))
#define GAIM_IS_GTK_CELL_RENDERER_EXPANDER(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GAIM_TYPE_GTK_CELL_RENDERER_EXPANDER))
#define GAIM_IS_GTK_CELL_RENDERER_EXPANDER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GAIM_TYPE_GTK_CELL_RENDERER_EXPANDER))
#define GAIM_GTK_CELL_RENDERER_EXPANDER_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), GAIM_TYPE_GTK_CELL_RENDERER_EXPANDER, GaimGtkCellRendererExpanderClass))

typedef struct _GaimGtkCellRendererExpander GaimGtkCellRendererExpander;
typedef struct _GaimGtkCellRendererExpanderClass GaimGtkCellRendererExpanderClass;

struct _GaimGtkCellRendererExpander {
	GtkCellRenderer parent;

	gboolean is_expander;
};

struct _GaimGtkCellRendererExpanderClass {
	GtkCellRendererClass parent_class;
};

GType            gaim_gtk_cell_renderer_expander_get_type     (void);
GtkCellRenderer  *gaim_gtk_cell_renderer_expander_new          (void);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _GAIM_GTKCELLRENDEREREXPANDER_H_ */
