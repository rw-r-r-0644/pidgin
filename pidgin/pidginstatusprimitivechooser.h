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

#ifndef PIDGIN_STATUS_PRIMITIVE_CHOOSER_H
#define PIDGIN_STATUS_PRIMITIVE_CHOOSER_H

#include <gtk/gtk.h>

#include <purple.h>

G_BEGIN_DECLS

/**
 * PidginStatusPrimitiveChooser:
 *
 * A [class@Gtk.Combobox] for presenting [class@Purple.StatusPrimitive]'s to a
 * user.
 *
 * Since: 3.0.0
 */

#define PIDGIN_TYPE_STATUS_PRIMITIVE_CHOOSER (pidgin_status_primitive_chooser_get_type())
G_DECLARE_FINAL_TYPE(PidginStatusPrimitiveChooser,
                     pidgin_status_primitive_chooser, PIDGIN,
                     STATUS_PRIMITIVE_CHOOSER, GtkComboBox)

/**
 * pidgin_status_primitive_chooser_new:
 *
 * Creates a new combo box that contains all of the available status
 * primitives in purple.
 *
 * Returns: (transfer full): The new combo box.
 *
 * Since: 3.0.0
 */
GtkWidget *pidgin_status_primitive_chooser_new(void);

G_END_DECLS

#endif /* PIDGIN_STATUS_PRIMITIVE_CHOOSER_H */
