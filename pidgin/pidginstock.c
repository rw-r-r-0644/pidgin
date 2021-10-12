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
 *
 */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <glib/gi18n-lib.h>

#include <purple.h>

#include "pidginstock.h"

#include "gtkicon-theme-loader.h"
#include "pidgincore.h"

#warning GtkStock is deprecated. Port usage of PidginStock to GtkIconTheme \
	and friends.

/**************************************************************************
 * Globals
 **************************************************************************/

static gboolean stock_initted = FALSE;
static GtkIconSize microscopic, extra_small, small, medium, large, huge;

/**************************************************************************
 * Structures
 **************************************************************************/

/*****************************************************************************
 * Public API functions
 *****************************************************************************/

void
pidgin_stock_load_status_icon_theme(PidginStatusIconTheme *theme)
{
	if (theme != NULL) {
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/status/icon-theme",
		                        purple_theme_get_name(PURPLE_THEME(theme)));
		purple_prefs_set_path(PIDGIN_PREFS_ROOT "/status/icon-theme-dir",
		                      purple_theme_get_dir(PURPLE_THEME(theme)));
	}
	else {
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/status/icon-theme", "");
		purple_prefs_set_path(PIDGIN_PREFS_ROOT "/status/icon-theme-dir", "");
	}
}

void
pidgin_stock_load_stock_icon_theme(PidginStockIconTheme *theme)
{
	if (theme != NULL) {
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/stock/icon-theme",
		                        purple_theme_get_name(PURPLE_THEME(theme)));
		purple_prefs_set_path(PIDGIN_PREFS_ROOT "/stock/icon-theme-dir",
		                      purple_theme_get_dir(PURPLE_THEME(theme)));
	}
	else {
		purple_prefs_set_string(PIDGIN_PREFS_ROOT "/stock/icon-theme", "");
		purple_prefs_set_path(PIDGIN_PREFS_ROOT "/stock/icon-theme-dir", "");
	}
}

void
pidgin_stock_init(void)
{
	PidginIconThemeLoader *loader, *stockloader;
	const gchar *path = NULL;

	if (stock_initted)
		return;

	stock_initted = TRUE;

	/* Setup the status icon theme */
	loader = g_object_new(PIDGIN_TYPE_ICON_THEME_LOADER, "type", "status-icon", NULL);
	purple_theme_manager_register_type(PURPLE_THEME_LOADER(loader));
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/status");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/status/icon-theme", "");
	purple_prefs_add_path(PIDGIN_PREFS_ROOT "/status/icon-theme-dir", "");

	stockloader = g_object_new(PIDGIN_TYPE_ICON_THEME_LOADER, "type", "stock-icon", NULL);
	purple_theme_manager_register_type(PURPLE_THEME_LOADER(stockloader));
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/stock");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/stock/icon-theme", "");
	purple_prefs_add_path(PIDGIN_PREFS_ROOT "/stock/icon-theme-dir", "");

	/* register custom icon sizes */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	microscopic =  gtk_icon_size_register(PIDGIN_ICON_SIZE_TANGO_MICROSCOPIC, 11, 11);
	extra_small =  gtk_icon_size_register(PIDGIN_ICON_SIZE_TANGO_EXTRA_SMALL, 16, 16);
	small       =  gtk_icon_size_register(PIDGIN_ICON_SIZE_TANGO_SMALL, 22, 22);
	medium      =  gtk_icon_size_register(PIDGIN_ICON_SIZE_TANGO_MEDIUM, 32, 32);
	large       =  gtk_icon_size_register(PIDGIN_ICON_SIZE_TANGO_LARGE, 48, 48);
	huge        =  gtk_icon_size_register(PIDGIN_ICON_SIZE_TANGO_HUGE, 64, 64);
G_GNUC_END_IGNORE_DEPRECATIONS

	pidgin_stock_load_stock_icon_theme(NULL);

	/* Pre-load Status icon theme - this avoids a bug with displaying the correct icon in the tray, theme is destroyed after*/
	if (purple_prefs_get_string(PIDGIN_PREFS_ROOT "/status/icon-theme") &&
	   (path = purple_prefs_get_path(PIDGIN_PREFS_ROOT "/status/icon-theme-dir"))) {

		PidginStatusIconTheme *theme = PIDGIN_STATUS_ICON_THEME(purple_theme_loader_build(PURPLE_THEME_LOADER(loader), path));
		pidgin_stock_load_status_icon_theme(theme);
		if (theme)
			g_object_unref(G_OBJECT(theme));

	}
	else
		pidgin_stock_load_status_icon_theme(NULL);
}

static void
pidgin_stock_icon_theme_class_init(PidginStockIconThemeClass *klass)
{
}

GType
pidgin_stock_icon_theme_get_type(void)
{
	static GType type = 0;
	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (PidginStockIconThemeClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc)pidgin_stock_icon_theme_class_init, /* class_init */
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (PidginStockIconTheme),
			0, /* n_preallocs */
			NULL,
			NULL, /* value table */
		};
		type = g_type_register_static(PIDGIN_TYPE_ICON_THEME,
				"PidginStockIconTheme", &info, 0);
	}
	return type;
}
