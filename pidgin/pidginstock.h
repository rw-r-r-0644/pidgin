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

#ifndef _PIDGIN_STOCK_H_
#define _PIDGIN_STOCK_H_
/**
 * SECTION:pidginstock
 * @section_id: pidgin-pidginstock
 * @short_description: <filename>pidginstock.h</filename>
 * @title: Stock Resources
 */

#include <gtk/gtk.h>
#include "gtkstatus-icon-theme.h"

/*
 * For using icons that aren't one of the default GTK_ICON_SIZEs
 */
#define PIDGIN_ICON_SIZE_TANGO_MICROSCOPIC    "pidgin-icon-size-tango-microscopic"
#define PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL    "pidgin-icon-size-tango-extra-small"
#define PIDGIN_ICON_SIZE_TANGO_SMALL          "pidgin-icon-size-tango-small"
#define PIDGIN_ICON_SIZE_TANGO_MEDIUM         "pidgin-icon-size-tango-medium"
#define PIDGIN_ICON_SIZE_TANGO_LARGE          "pidgin-icon-size-tango-large"
#define PIDGIN_ICON_SIZE_TANGO_HUGE           "pidgin-icon-size-tango-huge"

typedef struct _PidginStockIconTheme        PidginStockIconTheme;
typedef struct _PidginStockIconThemeClass   PidginStockIconThemeClass;

#define PIDGIN_TYPE_STOCK_ICON_THEME            (pidgin_stock_icon_theme_get_type ())
#define PIDGIN_STOCK_ICON_THEME(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), PIDGIN_TYPE_STOCK_ICON_THEME, PidginStockIconTheme))
#define PIDGIN_STOCK_ICON_THEME_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), PIDGIN_TYPE_STOCK_ICON_THEME, PidginStockIconThemeClass))
#define PIDGIN_IS_STOCK_ICON_THEME(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), PIDGIN_TYPE_STOCK_ICON_THEME))
#define PIDGIN_IS_STOCK_ICON_THEME_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), PIDGIN_TYPE_STOCK_ICON_THEME))
#define PIDGIN_STOCK_ICON_THEME_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), PIDGIN_TYPE_STOCK_ICON_THEME, PidginStockIconThemeClass))

/**
 * PidginStockIconTheme:
 *
 * extends PidginIconTheme (gtkicon-theme.h)
 * A pidgin stock icon theme.
 * This object represents a Pidgin stock icon theme.
 *
 * PidginStockIconTheme is a PidginIconTheme Object.
 */
struct _PidginStockIconTheme
{
	PidginIconTheme parent;
};

struct _PidginStockIconThemeClass
{
	PidginIconThemeClass parent_class;
};

G_BEGIN_DECLS

/**
 * pidgin_stock_icon_theme_get_type:
 *
 * Returns: The #GType for a stock icon theme.
 */
GType pidgin_stock_icon_theme_get_type(void);

/**
 * pidgin_stock_load_status_icon_theme:
 * @theme:		the theme to load, or null to load all the default icons
 *
 * Loades all of the icons from the status icon theme into Pidgin stock
 */
void pidgin_stock_load_status_icon_theme(PidginStatusIconTheme *theme);


void pidgin_stock_load_stock_icon_theme(PidginStockIconTheme *theme);

/**
 * pidgin_stock_init:
 *
 * Sets up the purple stock repository.
 */
void pidgin_stock_init(void);

G_END_DECLS

#endif /* _PIDGIN_STOCK_H_ */
