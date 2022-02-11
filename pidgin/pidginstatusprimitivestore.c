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

#include <pidgin/pidginstatusprimitivestore.h>

#include <pidgin/pidginiconname.h>

struct _PidginStatusPrimitiveStore {
	GtkListStore parent;

	PurpleAccount *account;
};

enum {
	PROP_0,
	PROP_ACCOUNT,
	N_PROPERTIES,
};
static GParamSpec *properties[N_PROPERTIES] = { NULL, };

enum {
	COL_ID,
	COL_ICON,
	COL_TEXT,
	N_COLUMNS,
};

G_DEFINE_TYPE(PidginStatusPrimitiveStore, pidgin_status_primitive_store,
              GTK_TYPE_LIST_STORE)

/******************************************************************************
 * Helpers
 *****************************************************************************/
static GList *
pidgin_status_primitive_store_get_primitives_from_account(PidginStatusPrimitiveStore *store)
{
	GList *primitives = NULL;
	GList *list = purple_account_get_status_types(store->account);

	while(list != NULL) {
		PurpleStatusType *type = (PurpleStatusType *)list->data;
		PurpleStatusPrimitive primitive;
		gboolean user_settable = FALSE, independent = FALSE;

		/* iterate to the next item as we already have the data in type. */
		list = list->next;

		user_settable = purple_status_type_is_user_settable(type);
		independent = purple_status_type_is_independent(type);
		if(!user_settable || independent) {
			continue;
		}

		primitive = purple_status_type_get_primitive(type);

		primitives = g_list_append(primitives, GINT_TO_POINTER(primitive));
	}

	return primitives;
}

static GList *
pidgin_status_primitive_store_get_primitives(PidginStatusPrimitiveStore *store)
{
	GList *primitives = NULL;

	primitives = g_list_append(primitives,
	                           GINT_TO_POINTER(PURPLE_STATUS_OFFLINE));
	primitives = g_list_append(primitives,
	                           GINT_TO_POINTER(PURPLE_STATUS_AVAILABLE));
	primitives = g_list_append(primitives,
	                           GINT_TO_POINTER(PURPLE_STATUS_UNAVAILABLE));
	primitives = g_list_append(primitives,
	                           GINT_TO_POINTER(PURPLE_STATUS_INVISIBLE));
	primitives = g_list_append(primitives,
	                           GINT_TO_POINTER(PURPLE_STATUS_AWAY));
	primitives = g_list_append(primitives,
	                           GINT_TO_POINTER(PURPLE_STATUS_EXTENDED_AWAY));

	return primitives;
}

static void
pidgin_status_primitive_store_populate(PidginStatusPrimitiveStore *store) {
	GList *primitives = NULL;

	/* clear out the list */
	gtk_list_store_clear(GTK_LIST_STORE(store));

	/* Get all the primitives and add them. */
	if(PURPLE_IS_ACCOUNT(store->account)) {
		primitives = pidgin_status_primitive_store_get_primitives_from_account(store);
	} else {
		primitives = pidgin_status_primitive_store_get_primitives(store);
	}

	while(primitives != NULL) {
		PurpleStatusPrimitive primitive = GPOINTER_TO_INT(primitives->data);
		GtkTreeIter iter;

		gtk_list_store_append(GTK_LIST_STORE(store), &iter);
		/* clang-format off */
		gtk_list_store_set(
			GTK_LIST_STORE(store), &iter,
			COL_ID, purple_primitive_get_id_from_type(primitive),
			COL_ICON, pidgin_icon_name_from_status_primitive(primitive, NULL),
			COL_TEXT, purple_primitive_get_name_from_type(primitive),
			-1);
		/* clang-format on */

		primitives = g_list_delete_link(primitives, primitives);
	}
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/

static void
pidgin_status_primitive_store_get_property(GObject *obj, guint param_id,
                                           GValue *value, GParamSpec *pspec)
{
	PidginStatusPrimitiveStore *store = PIDGIN_STATUS_PRIMITIVE_STORE(obj);

	switch(param_id) {
		case PROP_ACCOUNT:
			g_value_set_object(value,
			                   pidgin_status_primitive_store_get_account(store));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
pidgin_status_primitive_store_set_property(GObject *obj, guint param_id,
                                           const GValue *value,
                                           GParamSpec *pspec)
{
	PidginStatusPrimitiveStore *store = PIDGIN_STATUS_PRIMITIVE_STORE(obj);

	switch(param_id) {
		case PROP_ACCOUNT:
			pidgin_status_primitive_store_set_account(store,
			                                          g_value_get_object(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
pidgin_status_primitive_store_dispose(GObject *obj) {
	PidginStatusPrimitiveStore *store = PIDGIN_STATUS_PRIMITIVE_STORE(obj);

	g_clear_object(&store->account);

	G_OBJECT_CLASS(pidgin_status_primitive_store_parent_class)->dispose(obj);
}

static void
pidgin_status_primitive_store_constructed(GObject *obj) {
	PidginStatusPrimitiveStore *store = PIDGIN_STATUS_PRIMITIVE_STORE(obj);

	G_OBJECT_CLASS(pidgin_status_primitive_store_parent_class)->constructed(obj);

	/* This exists to populate the store if it was created without a value for
	 * the account property.
	 */
	pidgin_status_primitive_store_populate(store);
}

static void
pidgin_status_primitive_store_init(PidginStatusPrimitiveStore *store) {
	GType types[N_COLUMNS] = { G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING };

	gtk_list_store_set_column_types(GTK_LIST_STORE(store), N_COLUMNS, types);
}

static void
pidgin_status_primitive_store_class_init(PidginStatusPrimitiveStoreClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->get_property = pidgin_status_primitive_store_get_property;
	obj_class->set_property = pidgin_status_primitive_store_set_property;
	obj_class->constructed = pidgin_status_primitive_store_constructed;
	obj_class->dispose = pidgin_status_primitive_store_dispose;

	/**
	 * PidginStatusPrimitiveStore::account:
	 *
	 * The account whose statuses to display.
	 *
	 * Since: 3.0.0
	 */
	/* clang-format off */
	properties[PROP_ACCOUNT] = g_param_spec_object(
		"account", "account",
		"The account whose statuses to display",
		PURPLE_TYPE_ACCOUNT,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS);
	/* clang-format on */

	g_object_class_install_properties(obj_class, N_PROPERTIES, properties);
}

/******************************************************************************
 * Public API
 *****************************************************************************/
GtkListStore *
pidgin_status_primitive_store_new(void) {
	return g_object_new(PIDGIN_TYPE_STATUS_PRIMITIVE_STORE, NULL);
}

void
pidgin_status_primitive_store_set_account(PidginStatusPrimitiveStore *store,
                                          PurpleAccount *account)
{
	g_return_if_fail(PIDGIN_IS_STATUS_PRIMITIVE_STORE(store));

	if(g_set_object(&store->account, account)) {
		pidgin_status_primitive_store_populate(store);

		g_object_notify_by_pspec(G_OBJECT(store), properties[PROP_ACCOUNT]);
	}
}

PurpleAccount *
pidgin_status_primitive_store_get_account(PidginStatusPrimitiveStore *store) {
	g_return_val_if_fail(PIDGIN_IS_STATUS_PRIMITIVE_STORE(store), NULL);

	return store->account;
}
