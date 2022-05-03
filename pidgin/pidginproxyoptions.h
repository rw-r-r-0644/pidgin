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

#if !defined(PIDGIN_GLOBAL_HEADER_INSIDE) && !defined(PIDGIN_COMPILATION)
# error "only <pidgin.h> may be included directly"
#endif

#ifndef PIDGIN_PROXY_OPTIONS_H
#define PIDGIN_PROXY_OPTIONS_H

#include <gtk/gtk.h>

#include <purple.h>

G_BEGIN_DECLS

/**
 * PidginProxyOptions:
 *
 * A widget for the proxy options in the account editor.
 *
 * Since: 3.0.0
 */

#define PIDGIN_TYPE_PROXY_OPTIONS (pidgin_proxy_options_get_type())
G_DECLARE_FINAL_TYPE(PidginProxyOptions, pidgin_proxy_options, PIDGIN,
                     PROXY_OPTIONS, GtkBox)

/**
 * pidgin_proxy_options_new:
 *
 * Creates a new proxy options widget.
 *
 * Returns: (transfer full): The widget.
 *
 * Since: 3.0.0
 */
GtkWidget *pidgin_proxy_options_new(void);

/**
 * pidgin_proxy_options_set_show_global:
 * @options: The instance.
 * @show_global: Whether or not to show the use global settings proxy item.
 *
 * Sets whether or not to show the "Use Global Proxy Settings" item.
 *
 * Since: 3.0.0
 */
void pidgin_proxy_options_set_show_global(PidginProxyOptions *options, gboolean show_global);

/**
 * pidgin_proxy_options_get_show_global:
 * @options: The instance.
 *
 * Gets whether or not @options is displaying the "Use Global Proxy Settings"
 * item.
 *
 * Returns: %TRUE if displaying it, %FALSE otherwise.
 *
 * Since: 3.0.0
 */
gboolean pidgin_proxy_options_get_show_global(PidginProxyOptions *options);

/**
 * pidgin_proxy_options_get_info:
 * @options: The instance.
 *
 * Gets the [class@Purple.ProxyInfo] that is being configured.
 *
 * Returns: (transfer none): The proxy info.
 *
 * Since: 3.0.0
 */
PurpleProxyInfo *pidgin_proxy_options_get_info(PidginProxyOptions *options);

/**
 * pidgin_proxy_options_set_info:
 * @options: The instance.
 * @info: The [class@Purple.ProxyInfo] to set.
 *
 * The proxy info that will be configured.
 *
 * Since: 3.0.0
 */
void pidgin_proxy_options_set_info(PidginProxyOptions *options, PurpleProxyInfo *info);

G_END_DECLS

#endif /* PIDGIN_PROXY_OPTIONS_H */
