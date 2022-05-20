/*
 * Purple - Internet Messaging Library
 * Copyright (C) Pidgin Developers <devel@pidgin.im>
 *
 * Purple is the legal property of its developers, whose names are too numerous
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

#if !defined(PURPLE_GLOBAL_HEADER_INSIDE) && !defined(PURPLE_COMPILATION)
# error "only <purple.h> may be included directly"
#endif

#ifndef PURPLE_UI_INFO_H
#define PURPLE_UI_INFO_H

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

/**
 * PURPLE_TYPE_UI_INFO:
 *
 * The standard _get_type macro for #PurpleUiInfo.
 */
#define PURPLE_TYPE_UI_INFO (purple_ui_info_get_type())

/**
 * PurpleUiInfo:
 *
 * #PurpleUiInfo keeps track of basic information about the user interface.
 */
G_DECLARE_FINAL_TYPE(PurpleUiInfo, purple_ui_info, PURPLE, UI_INFO, GObject)

/**
 * purple_ui_info_new:
 * @id: The identifier.
 * @name: The name, which will be displayed.
 * @version: The version.
 * @website: The website.
 * @support_website: The support website.
 * @client_type: The client type.
 *
 * Creates a new #PurpleUiInfo with the given values.  If you only want to set
 * a few of these, you should use g_object_new() directly.
 *
 * Returns: (transfer full): The newly created #PurpleUiInfo instance.
 *
 * Since: 3.0.0
 */
PurpleUiInfo *purple_ui_info_new(const gchar *id,
                                 const gchar *name,
                                 const gchar *version,
                                 const gchar *website,
                                 const gchar *support_website,
                                 const gchar *client_type);

/**
 * purple_ui_info_get_id:
 * @info: The instance.
 *
 * Gets the identifier from @info.
 *
 * Returns: The identifier from @info.
 *
 * Since: 3.0.0
 */
const gchar *purple_ui_info_get_id(PurpleUiInfo *info);

/**
 * purple_ui_info_get_name:
 * @info: The #PurpleUiInfo instance.
 *
 * Gets the name from @info.
 *
 * Returns: The name from @info.
 *
 * Since: 3.0.0
 */
const gchar *purple_ui_info_get_name(PurpleUiInfo *info);

/**
 * purple_ui_info_get_version:
 * @info: The #PurpleUiInfo instance.
 *
 * Gets the version from @info.
 *
 * Returns: The version from @info.
 *
 * Since: 3.0.0
 */
const gchar *purple_ui_info_get_version(PurpleUiInfo *info);

/**
 * purple_ui_info_get_website:
 * @info: The #PurpleUiInfo instance.
 *
 * Gets the website from @info.
 *
 * Returns: The website for @info.
 *
 * Since: 3.0.0
 */
const gchar *purple_ui_info_get_website(PurpleUiInfo *info);

/**
 * purple_ui_info_get_support_website:
 * @info: The #PurpleUiInfo instance.
 *
 * Gets the support website from @info.
 *
 * Returns: The support website from @info.
 *
 * Since: 3.0.0
 */
const gchar *purple_ui_info_get_support_website(PurpleUiInfo *info);

/**
 * purple_ui_info_get_client_type:
 * @info: The #PurpleUiInfo instance.
 *
 * Gets the client type from @info.  For example: 'bot', 'console', 'mobile',
 * 'pc', 'web', etc.
 *
 * Returns: The client type of @info.
 *
 * Since: 3.0.0
 */
const gchar *purple_ui_info_get_client_type(PurpleUiInfo *info);

G_END_DECLS

#endif /* PURPLE_UI_INFO_H */

