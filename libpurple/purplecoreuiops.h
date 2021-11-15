/*
 * Purple - Internet Messaging Library
 * Copyright (C) Pidgin Developers <devel@pidgin.im>
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
# error "only <pidgin.h> may be included directly"
#endif

#ifndef PURPLE_CORE_UIOPS_H
#define PURPLE_CORE_UIOPS_H

#include <glib.h>
#include <glib-object.h>

#include <libpurple/purpleuiinfo.h>

#define PURPLE_TYPE_CORE_UI_OPS (purple_core_ui_ops_get_type())
typedef struct _PurpleCoreUiOps PurpleCoreUiOps;

G_BEGIN_DECLS

/**
 * PurpleCoreUiOps:
 * @ui_prefs_init: Called just after the preferences subsystem is initialized;
 *                 the UI could use this callback to add some preferences it
 *                 needs to be in place when other subsystems are initialized.
 * @ui_init:       Called after all of libpurple has been initialized. The UI
 *                 should use this hook to set all other necessary
 *                 <link linkend="chapter-ui-ops"><literal>UiOps structures</literal></link>.
 * @quit:          Called after most of libpurple has been uninitialized.
 * @get_ui_info:   Called by purple_core_get_ui_info(); should return the
 *                 information documented there.
 *
 * Callbacks that fire at different points of the initialization and teardown
 * of libpurple, along with a hook to return descriptive information about the
 * UI.
 */
struct _PurpleCoreUiOps {
	void (*ui_prefs_init)(void);
	void (*ui_init)(void);

	void (*quit)(void);

	PurpleUiInfo *(*get_ui_info)(void);

	/*< private >*/
	gpointer reserved[4];
};

/**
 * purple_core_ui_ops_get_type:
 *
 * Returns: The #GType for the #PurpleCoreUiOps boxed structure.
 */
GType purple_core_ui_ops_get_type(void);


G_END_DECLS

#endif /* PURPLE_CORE_UIOPS_H */
