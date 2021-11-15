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

#include "purplecoreuiops.h"

static PurpleCoreUiOps *
purple_core_ui_ops_copy(PurpleCoreUiOps *ops)
{
	PurpleCoreUiOps *ops_new;

	g_return_val_if_fail(ops != NULL, NULL);

	ops_new = g_new(PurpleCoreUiOps, 1);
	*ops_new = *ops;

	return ops_new;
}

GType
purple_core_ui_ops_get_type(void)
{
	static GType type = 0;

	if (type == 0) {
		type = g_boxed_type_register_static("PurpleCoreUiOps",
				(GBoxedCopyFunc)purple_core_ui_ops_copy,
				(GBoxedFreeFunc)g_free);
	}

	return type;
}


