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

#ifndef PIDGIN_CREDENTIALS_PAGE_H
#define PIDGIN_CREDENTIALS_PAGE_H

/**
 * SECTION:pidgincredentialspage
 * @section_id: pidgin-pidgincredentialspage
 * @short_description: The preferences page for credential management.
 * @title: Credential management widget
 *
 * #PidginCredentialsPage is a widget for the preferences window to let users
 * choose and configure their credential provider.
 */

#include <glib.h>

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PIDGIN_TYPE_CREDENTIALS_PAGE (pidgin_credentials_page_get_type())
G_DECLARE_FINAL_TYPE(PidginCredentialsPage, pidgin_credentials_page,
                     PIDGIN, CREDENTIALS_PAGE, GtkBox)

/**
 * pidgin_credentials_page_new:
 *
 * Creates a new #PidginCredentialsPage instance.
 *
 * Returns: (transfer full): The new #PidginCredentialsPage instance.
 */
GtkWidget *pidgin_credentials_page_new(void);

G_END_DECLS

#endif /* PIDGIN_CREDENTIALS_PAGE_H */
