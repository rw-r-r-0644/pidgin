/*
 * Purple - Internet Messaging Library
 * Copyright (C) Pidgin Developers <devel@pidgin.im>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <https://www.gnu.org/licenses/>.
 */

#include <libpurple/purplepath.h>

#ifdef _WIN32
# include "win32/win32dep.h"
#endif

/******************************************************************************
 * Globals
 *****************************************************************************/
static gchar *custom_user_dir = NULL;
static gchar *user_dir = NULL;
static gchar *cache_dir = NULL;
static gchar *config_dir = NULL;
static gchar *data_dir = NULL;

/******************************************************************************
 * Helpers
 *****************************************************************************/
static const gchar *
purple_xdg_dir(gchar **xdg_dir, const gchar *xdg_base_dir,
               const gchar *xdg_type)
{
	if (!*xdg_dir) {
		if (!custom_user_dir) {
			*xdg_dir = g_build_filename(xdg_base_dir, "purple", NULL);
		} else {
			*xdg_dir = g_build_filename(custom_user_dir, xdg_type, NULL);
		}
	}

	return *xdg_dir;
}

/******************************************************************************
 * Public API
 *****************************************************************************/
const gchar *
purple_home_dir(void) {
#ifndef _WIN32
	return g_get_home_dir();
#else
	return wpurple_home_dir();
#endif
}

/* Returns the argument passed to -c IFF it was present, or ~/.purple. */
const gchar *
purple_user_dir(void) {
	if(custom_user_dir != NULL) {
		return custom_user_dir;
	} else if(!user_dir) {
		user_dir = g_build_filename(purple_home_dir(), ".purple", NULL);
	}

	return user_dir;
}

const gchar *
purple_cache_dir(void) {
	return purple_xdg_dir(&cache_dir, g_get_user_cache_dir(), "cache");
}

const gchar *
purple_config_dir(void) {
	return purple_xdg_dir(&config_dir, g_get_user_config_dir(), "config");
}

const gchar *
purple_data_dir(void) {
	return purple_xdg_dir(&data_dir, g_get_user_data_dir(), "data");
}

void
purple_util_set_user_dir(const gchar *dir) {
	g_free(custom_user_dir);

	if(dir != NULL && *dir) {
		custom_user_dir = g_strdup(dir);
	} else {
		custom_user_dir = NULL;
	}

	g_clear_pointer(&user_dir, g_free);
	g_clear_pointer(&cache_dir, g_free);
	g_clear_pointer(&config_dir, g_free);
	g_clear_pointer(&data_dir, g_free);
}
