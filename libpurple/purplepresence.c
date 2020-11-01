/*
 * purple
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

#include <glib/gi18n-lib.h>

#include "internal.h"
#include "debug.h"
#include "purplepresence.h"
#include "purpleprivate.h"

typedef struct {
	gboolean idle;
	time_t idle_time;
	time_t login_time;

	GHashTable *status_table;

	PurpleStatus *active_status;
} PurplePresencePrivate;

enum {
	PROP_0,
	PROP_IDLE,
	PROP_IDLE_TIME,
	PROP_LOGIN_TIME,
	PROP_ACTIVE_STATUS,
	N_PROPERTIES
};
static GParamSpec *properties[N_PROPERTIES];

G_DEFINE_TYPE_WITH_PRIVATE(PurplePresence, purple_presence, G_TYPE_OBJECT)

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
purple_presence_set_active_status(PurplePresence *presence,
                                  PurpleStatus *status)
{
	PurplePresencePrivate *priv = NULL;

	priv = purple_presence_get_instance_private(presence);

	if(g_set_object(&priv->active_status, status)) {
		g_object_notify_by_pspec(G_OBJECT(presence),
		                         properties[PROP_ACTIVE_STATUS]);
	}
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
static void
purple_presence_set_property(GObject *obj, guint param_id, const GValue *value,
                             GParamSpec *pspec)
{
	PurplePresence *presence = PURPLE_PRESENCE(obj);

	switch (param_id) {
		case PROP_IDLE:
			purple_presence_set_idle(presence, g_value_get_boolean(value), 0);
			break;
		case PROP_IDLE_TIME:
#if SIZEOF_TIME_T == 4
			purple_presence_set_idle(presence, TRUE, g_value_get_int(value));
#elif SIZEOF_TIME_T == 8
			purple_presence_set_idle(presence, TRUE, g_value_get_int64(value));
#else
#error Unknown size of time_t
#endif
			break;
		case PROP_LOGIN_TIME:
#if SIZEOF_TIME_T == 4
			purple_presence_set_login_time(presence, g_value_get_int(value));
#elif SIZEOF_TIME_T == 8
			purple_presence_set_login_time(presence, g_value_get_int64(value));
#else
#error Unknown size of time_t
#endif
			break;
		case PROP_ACTIVE_STATUS:
			purple_presence_set_active_status(presence,
			                                  g_value_get_object(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_presence_get_property(GObject *obj, guint param_id, GValue *value,
                             GParamSpec *pspec)
{
	PurplePresence *presence = PURPLE_PRESENCE(obj);

	switch (param_id) {
		case PROP_IDLE:
			g_value_set_boolean(value, purple_presence_is_idle(presence));
			break;
		case PROP_IDLE_TIME:
#if SIZEOF_TIME_T == 4
			g_value_set_int(value, purple_presence_get_idle_time(presence));
#elif SIZEOF_TIME_T == 8
			g_value_set_int64(value, purple_presence_get_idle_time(presence));
#else
#error Unknown size of time_t
#endif
			break;
		case PROP_LOGIN_TIME:
#if SIZEOF_TIME_T == 4
			g_value_set_int(value, purple_presence_get_login_time(presence));
#elif SIZEOF_TIME_T == 8
			g_value_set_int64(value, purple_presence_get_login_time(presence));
#else
#error Unknown size of time_t
#endif
			break;
		case PROP_ACTIVE_STATUS:
			g_value_set_object(value, purple_presence_get_active_status(presence));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_presence_init(PurplePresence *presence) {
	PurplePresencePrivate *priv = NULL;

	priv = purple_presence_get_instance_private(presence);

	priv->status_table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
	                                           NULL);
}

static void
purple_presence_finalize(GObject *obj) {
	PurplePresencePrivate *priv = NULL;

	priv = purple_presence_get_instance_private(PURPLE_PRESENCE(obj));

	g_hash_table_destroy(priv->status_table);
	g_clear_object(&priv->active_status);

	G_OBJECT_CLASS(purple_presence_parent_class)->finalize(obj);
}

static void
purple_presence_class_init(PurplePresenceClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->get_property = purple_presence_get_property;
	obj_class->set_property = purple_presence_set_property;
	obj_class->finalize = purple_presence_finalize;

	/**
	 * PurplePresence:idle:
	 *
	 * Whether or not the presence is in an idle state.
	 */
	properties[PROP_IDLE] = g_param_spec_boolean("idle", "Idle",
				"Whether the presence is in idle state.", FALSE,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	/**
	 * PurplePresence:idle-time:
	 *
	 * The time when the presence went idle.
	 */
	properties[PROP_IDLE_TIME] =
#if SIZEOF_TIME_T == 4
		g_param_spec_int
#elif SIZEOF_TIME_T == 8
		g_param_spec_int64
#else
#error Unknown size of time_t
#endif
				("idle-time", "Idle time",
				"The idle time of the presence",
#if SIZEOF_TIME_T == 4
				G_MININT, G_MAXINT, 0,
#elif SIZEOF_TIME_T == 8
				G_MININT64, G_MAXINT64, 0,
#else
#error Unknown size of time_t
#endif
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	/**
	 * PurplePresence:login-time:
	 *
	 * The login-time of the presence.
	 */
	properties[PROP_LOGIN_TIME] =
#if SIZEOF_TIME_T == 4
		g_param_spec_int
#elif SIZEOF_TIME_T == 8
		g_param_spec_int64
#else
#error Unknown size of time_t
#endif
				("login-time", "Login time",
				"The login time of the presence.",
#if SIZEOF_TIME_T == 4
				G_MININT, G_MAXINT, 0,
#elif SIZEOF_TIME_T == 8
				G_MININT64, G_MAXINT64, 0,
#else
#error Unknown size of time_t
#endif
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	/**
	 * PurplePresence:active-status:
	 *
	 * The currently active status of the presence.
	 */
	properties[PROP_ACTIVE_STATUS] = g_param_spec_object("active-status",
				"Active status",
				"The active status for the presence.", PURPLE_TYPE_STATUS,
				G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, N_PROPERTIES, properties);
}

/******************************************************************************
 * Public API
 *****************************************************************************/
void
purple_presence_set_status_active(PurplePresence *presence,
                                  const gchar *status_id, gboolean active)
{
	PurpleStatus *status = NULL;

	g_return_if_fail(PURPLE_IS_PRESENCE(presence));
	g_return_if_fail(status_id != NULL);

	status = purple_presence_get_status(presence, status_id);

	g_return_if_fail(PURPLE_IS_STATUS(status));
	/* TODO: Should we do the following? */
	/* g_return_if_fail(active == status->active); */

	if(purple_status_is_exclusive(status)) {
		if(!active) {
			purple_debug_warning("presence",
					"Attempted to set a non-independent status "
					"(%s) inactive. Only independent statuses "
					"can be specifically marked inactive.",
					status_id);
			return;
		}
	}

	purple_status_set_active(status, active);
}

void
purple_presence_switch_status(PurplePresence *presence, const gchar *status_id)
{
	purple_presence_set_status_active(presence, status_id, TRUE);
}

void
purple_presence_set_idle(PurplePresence *presence, gboolean idle,
                         time_t idle_time)
{
	PurplePresencePrivate *priv = NULL;
	PurplePresenceClass *klass = NULL;
	gboolean old_idle;
	GObject *obj = NULL;

	g_return_if_fail(PURPLE_IS_PRESENCE(presence));

	priv = purple_presence_get_instance_private(presence);
	klass = PURPLE_PRESENCE_GET_CLASS(presence);

	if (priv->idle == idle && priv->idle_time == idle_time) {
		return;
	}

	old_idle = priv->idle;
	priv->idle = idle;
	priv->idle_time = (idle ? idle_time : 0);

	obj = G_OBJECT(presence);
	g_object_freeze_notify(obj);
	g_object_notify_by_pspec(obj, properties[PROP_IDLE]);
	g_object_notify_by_pspec(obj, properties[PROP_IDLE_TIME]);
	g_object_thaw_notify(obj);

	if(klass->update_idle) {
		klass->update_idle(presence, old_idle);
	}
}

void
purple_presence_set_login_time(PurplePresence *presence, time_t login_time) {
	PurplePresencePrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_PRESENCE(presence));

	priv = purple_presence_get_instance_private(presence);

	if(priv->login_time == login_time) {
		return;
	}

	priv->login_time = login_time;

	g_object_notify_by_pspec(G_OBJECT(presence),
			properties[PROP_LOGIN_TIME]);
}

GList *
purple_presence_get_statuses(PurplePresence *presence) {
	PurplePresenceClass *klass = NULL;

	g_return_val_if_fail(PURPLE_IS_PRESENCE(presence), NULL);

	klass = PURPLE_PRESENCE_GET_CLASS(presence);
	if(klass && klass->get_statuses) {
		return klass->get_statuses(presence);
	}

	return NULL;
}

PurpleStatus *
purple_presence_get_status(PurplePresence *presence, const gchar *status_id) {
	PurplePresencePrivate *priv = NULL;
	PurpleStatus *status = NULL;
	GList *l = NULL;

	g_return_val_if_fail(PURPLE_IS_PRESENCE(presence), NULL);
	g_return_val_if_fail(status_id != NULL, NULL);

	priv = purple_presence_get_instance_private(presence);

	/* What's the purpose of this hash table? */
	status = (PurpleStatus *)g_hash_table_lookup(priv->status_table,
	                                             status_id);

	if(status == NULL) {
		for(l = purple_presence_get_statuses(presence);
			l != NULL && status == NULL; l = l->next)
		{
			PurpleStatus *temp_status = l->data;

			if (purple_strequal(status_id, purple_status_get_id(temp_status))) {
				status = temp_status;
			}
		}

		if(status != NULL) {
			g_hash_table_insert(priv->status_table,
								g_strdup(purple_status_get_id(status)), status);
		}
	}

	return status;
}

PurpleStatus *
purple_presence_get_active_status(PurplePresence *presence) {
	PurplePresencePrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_PRESENCE(presence), NULL);

	priv = purple_presence_get_instance_private(presence);

	return priv->active_status;
}

gboolean
purple_presence_is_available(PurplePresence *presence) {
	PurpleStatus *status = NULL;

	g_return_val_if_fail(PURPLE_IS_PRESENCE(presence), FALSE);

	status = purple_presence_get_active_status(presence);

	return ((status != NULL && purple_status_is_available(status)) &&
			!purple_presence_is_idle(presence));
}

gboolean
purple_presence_is_online(PurplePresence *presence) {
	PurpleStatus *status = NULL;

	g_return_val_if_fail(PURPLE_IS_PRESENCE(presence), FALSE);

	if((status = purple_presence_get_active_status(presence)) == NULL) {
		return FALSE;
	}

	return purple_status_is_online(status);
}

gboolean
purple_presence_is_status_active(PurplePresence *presence,
                                 const gchar *status_id)
{
	PurpleStatus *status = NULL;

	g_return_val_if_fail(PURPLE_IS_PRESENCE(presence), FALSE);
	g_return_val_if_fail(status_id != NULL, FALSE);

	status = purple_presence_get_status(presence, status_id);

	return (status != NULL && purple_status_is_active(status));
}

gboolean
purple_presence_is_status_primitive_active(PurplePresence *presence,
                                           PurpleStatusPrimitive primitive)
{
	GList *l = NULL;

	g_return_val_if_fail(PURPLE_IS_PRESENCE(presence), FALSE);
	g_return_val_if_fail(primitive != PURPLE_STATUS_UNSET, FALSE);

	for(l = purple_presence_get_statuses(presence); l != NULL; l = l->next) {
		PurpleStatus *temp_status = l->data;
		PurpleStatusType *type = purple_status_get_status_type(temp_status);

		if(purple_status_type_get_primitive(type) == primitive &&
		    purple_status_is_active(temp_status))
		{
			return TRUE;
		}
	}

	return FALSE;
}

gboolean
purple_presence_is_idle(PurplePresence *presence) {
	PurplePresencePrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_PRESENCE(presence), FALSE);

	if(!purple_presence_is_online(presence)) {
		return FALSE;
	}

	priv = purple_presence_get_instance_private(presence);

	return priv->idle;
}

time_t
purple_presence_get_idle_time(PurplePresence *presence) {
	PurplePresencePrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_PRESENCE(presence), 0);

	priv = purple_presence_get_instance_private(presence);

	return priv->idle_time;
}

time_t
purple_presence_get_login_time(PurplePresence *presence) {
	PurplePresencePrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_PRESENCE(presence), 0);

	priv = purple_presence_get_instance_private(presence);

	return purple_presence_is_online(presence) ? priv->login_time : 0;
}
