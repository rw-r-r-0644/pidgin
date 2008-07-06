/*
 * Themes for LibPurple
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

#include <stdarg.h>
#include "theme-manager.h"

/******************************************************************************
 * Globals
 *****************************************************************************/

static GHashTable *theme_table = NULL;

/*****************************************************************************
 * GObject Stuff                                                     
 ****************************************************************************/

GType 
purple_theme_manager_get_type (void)
{
  static GType type = 0;
  if (type == 0) {
    static const GTypeInfo info = {
      sizeof (PurpleThemeManagerClass),
      NULL,   /* base_init */
      NULL,   /* base_finalize */
      NULL,   /* class_init */
      NULL,   /* class_finalize */
      NULL,   /* class_data */
      sizeof (PurpleThemeManager),
      0,      /* n_preallocs */
      NULL,    /* instance_init */
      NULL,   /* Value Table */
    };
    type = g_type_register_static (G_TYPE_OBJECT,
                                   "PurpleThemeManager",
                                   &info, 0);
  }
  return type;
}

/******************************************************************************
 * Helpers
 *****************************************************************************/
/* makes a key of <type> + '/' + <name> */
static gchar *
purple_theme_manager_make_key(const gchar *name, const gchar *type)
{
	g_return_val_if_fail(name, NULL);
	g_return_val_if_fail(type, NULL);		
	return g_strconcat(type, "/", name, NULL);
}

/* returns TRUE if theme is of type "user_data" */ 
static gboolean
purple_theme_manager_is_theme_type(gchar *key,
                  gpointer value,
                  gchar *user_data)
{
	return g_str_has_prefix(key, g_strconcat(user_data, "/", NULL));
}

static gboolean
purple_theme_manager_is_theme(gchar *key,
                  gpointer value,
                  gchar *user_data)
{
	return PURPLE_IS_THEME(value);
}

static void
purple_theme_manager_function_wrapper(gchar *key,
                  		      gpointer value,
                		      PTFunc user_data)
{
	if (PURPLE_IS_THEME(value))
		(* user_data) (value);
}

static void
purple_theme_manager_build_dir(const gchar *root)
{

	GDir *rdir;
	gchar *name, *type, *purple_dir, *theme_dir;
	GDir *dir;
	PurpleThemeLoader *loader;

	rdir =  g_dir_open(root, 0, NULL);

	g_return_if_fail(rdir);

	/* Parses directory by root/name/purple/type */
	while ((name = g_strdup(g_dir_read_name (rdir)))){
		
		purple_dir = g_build_filename(root, name, "purple", NULL);
		dir =  g_dir_open(purple_dir, 0, NULL);	
	
		if (dir) {
			while ((type = g_strdup(g_dir_read_name (dir)))) {
				if ((loader = g_hash_table_lookup (theme_table, type))){

					theme_dir = g_build_filename(purple_dir, type, NULL);
					purple_theme_manager_add_theme(purple_theme_loader_build(loader, theme_dir));

				}
				g_free(type);

			}
			g_dir_close(dir);

		}
		g_free(purple_dir);
		g_free(name);	
	
	}
	g_dir_close(rdir);
	
}

/*****************************************************************************
 * Public API functions                                                      *
 *****************************************************************************/

void
purple_theme_manager_init (void)
{
	theme_table = g_hash_table_new_full (g_str_hash,
               	                             g_str_equal,
               	                             g_free,
               	                             g_object_unref);
}

void 
purple_theme_manager_refresh()
{
	g_hash_table_foreach_remove (theme_table,
                	             (GHRFunc) purple_theme_manager_is_theme,
                	             NULL);	
	
	/* TODO: add correct directories to parse */
	purple_theme_manager_build_dir("/usr/share/themes");

}

void 
purple_theme_manager_uninit ()
{
	g_hash_table_destroy(theme_table);
}


void
purple_theme_manager_register_type(PurpleThemeLoader *loader)
{
	gchar *type;

	g_return_if_fail(PURPLE_IS_THEME_LOADER(loader));

	type = g_strdup(purple_theme_loader_get_type_string(loader));
	g_return_if_fail(type);

	/* if something is already there do nothing */
	if (! g_hash_table_lookup (theme_table, type)) 
		g_hash_table_insert(theme_table, type, loader);
}

void
purple_theme_manager_unregister_type(PurpleThemeLoader *loader)
{
	const gchar *type;

	g_return_if_fail(PURPLE_IS_THEME_LOADER(loader));

	type = purple_theme_loader_get_type_string(loader);
	g_return_if_fail(type);

	if (g_hash_table_lookup (theme_table, type) == loader){

		g_hash_table_remove (theme_table, type);

		g_hash_table_foreach_remove (theme_table,
                	                     (GHRFunc) purple_theme_manager_is_theme_type,
                	                     type);		
	}/* only free if given registered loader */
}

PurpleTheme *
purple_theme_manager_find_theme(const gchar *name,
				const gchar *type)
{
	gchar *key;
	PurpleTheme *theme;

	key = purple_theme_manager_make_key(name, type);

	g_return_val_if_fail(key, NULL);

	theme = g_hash_table_lookup(theme_table, key);

	g_free(key);

	return theme;
}


void 
purple_theme_manager_add_theme(PurpleTheme *theme)
{
	gchar *key;
	
	g_return_if_fail(PURPLE_IS_THEME(theme));

	key = purple_theme_manager_make_key(purple_theme_get_name(theme),
			  		    purple_theme_get_type_string(theme));

	g_return_if_fail(key);

	/* if something is already there do nothing */
	if (g_hash_table_lookup(theme_table, key) == NULL) 
		g_hash_table_insert(theme_table, key, theme);
}

void
purple_theme_manager_remove_theme(PurpleTheme *theme)
{
	gchar *key;

	g_return_if_fail(PURPLE_IS_THEME(theme));

	key = purple_theme_manager_make_key(purple_theme_get_name(theme),
				  	    purple_theme_get_type_string(theme));

	g_return_if_fail(key);

	g_hash_table_remove(theme_table, key);	

	g_free(key);	
}

void 
purple_theme_manager_for_each_theme(PTFunc func)
{
	g_return_if_fail(func);

	g_hash_table_foreach(theme_table,
			     (GHFunc) purple_theme_manager_function_wrapper,
			     func);
}

