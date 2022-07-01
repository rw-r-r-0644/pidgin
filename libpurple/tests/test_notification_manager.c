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

#include <glib.h>

#include <purple.h>

#include "test_ui.h"

/******************************************************************************
 * Callbacks
 *****************************************************************************/
static void
test_purple_notification_manager_increment_cb(G_GNUC_UNUSED PurpleNotificationManager *manager,
                                              G_GNUC_UNUSED PurpleNotification *notification,
                                              gpointer data)
{
	gint *called = data;

	*called = *called + 1;
}

static void
test_purple_notification_manager_unread_count_cb(G_GNUC_UNUSED GObject *obj,
                                                 G_GNUC_UNUSED GParamSpec *pspec,
                                                 gpointer data)
{
	gint *called = data;

	*called = *called + 1;
}

/******************************************************************************
 * Tests
 *****************************************************************************/
static void
test_purple_notification_manager_get_default(void) {
	PurpleNotificationManager *manager1 = NULL, *manager2 = NULL;

	manager1 = purple_notification_manager_get_default();
	g_assert_true(PURPLE_IS_NOTIFICATION_MANAGER(manager1));

	manager2 = purple_notification_manager_get_default();
	g_assert_true(PURPLE_IS_NOTIFICATION_MANAGER(manager2));

	g_assert_true(manager1 == manager2);
}

static void
test_purple_notification_manager_add_remove(void) {
	PurpleNotificationManager *manager = NULL;
	PurpleNotification *notification = NULL;
	gint added_called = 0, removed_called = 0;
	guint unread_count = 0;

	manager = g_object_new(PURPLE_TYPE_NOTIFICATION_MANAGER, NULL);

	g_assert_true(PURPLE_IS_NOTIFICATION_MANAGER(manager));

	/* Wire up our signals. */
	g_signal_connect(manager, "added",
	                 G_CALLBACK(test_purple_notification_manager_increment_cb),
	                 &added_called);
	g_signal_connect(manager, "removed",
	                 G_CALLBACK(test_purple_notification_manager_increment_cb),
	                 &removed_called);

	/* Create the notification and store it's id. */
	notification = purple_notification_new(PURPLE_NOTIFICATION_TYPE_GENERIC,
	                                       NULL, NULL, NULL);

	/* Add the notification to the manager. */
	purple_notification_manager_add(manager, notification);

	/* Make sure the added signal was called. */
	g_assert_cmpint(added_called, ==, 1);

	/* Verify that the unread count is 1. */
	unread_count = purple_notification_manager_get_unread_count(manager);
	g_assert_cmpint(unread_count, ==, 1);

	/* Remove the notification. */
	purple_notification_manager_remove(manager, notification);
	g_assert_cmpint(removed_called, ==, 1);

	/* Verify that the unread count is now 0. */
	unread_count = purple_notification_manager_get_unread_count(manager);
	g_assert_cmpint(unread_count, ==, 0);

	/* Clean up the manager. */
	g_clear_object(&manager);
}

static void
test_purple_notification_manager_double_add(void) {
	if(g_test_subprocess()) {
		PurpleNotificationManager *manager = NULL;
		PurpleNotification *notification = NULL;

		manager = g_object_new(PURPLE_TYPE_NOTIFICATION_MANAGER, NULL);

		notification = purple_notification_new(PURPLE_NOTIFICATION_TYPE_GENERIC,
		                                       NULL, NULL, NULL);

		purple_notification_manager_add(manager, notification);
		purple_notification_manager_add(manager, notification);

		/* This will never get called as the double add outputs a g_warning()
		 * that causes the test to fail. This is left to avoid a false postive
		 * in static analysis.
		 */
		g_clear_object(&manager);
	}

	g_test_trap_subprocess(NULL, 0, 0);
	g_test_trap_assert_stderr("*Purple-WARNING*double add detected for notification*");
}

static void
test_purple_notification_manager_double_remove(void) {
	PurpleNotificationManager *manager = NULL;
	PurpleNotification *notification = NULL;
	gint removed_called = 0;

	manager = g_object_new(PURPLE_TYPE_NOTIFICATION_MANAGER, NULL);
	g_signal_connect(manager, "removed",
	                 G_CALLBACK(test_purple_notification_manager_increment_cb),
	                 &removed_called);

	notification = purple_notification_new(PURPLE_NOTIFICATION_TYPE_GENERIC,
	                                       NULL, NULL, NULL);
	/* Add an additional reference because the manager takes one and the id
	 * belongs to the notification. So without this, the first remove frees
	 * the id which would cause an invalid read.
	 */
	g_object_ref(notification);

	purple_notification_manager_add(manager, notification);

	purple_notification_manager_remove(manager, notification);
	purple_notification_manager_remove(manager, notification);

	g_assert_cmpint(removed_called, ==, 1);

	g_clear_object(&notification);
	g_clear_object(&manager);
}

static void
test_purple_notification_manager_read_propagation(void) {
	PurpleNotificationManager *manager = NULL;
	PurpleNotification *notification = NULL;
	gint read_called = 0, unread_called = 0, unread_count_called = 0;

	/* Create the manager. */
	manager = g_object_new(PURPLE_TYPE_NOTIFICATION_MANAGER, NULL);

	g_signal_connect(manager, "read",
	                 G_CALLBACK(test_purple_notification_manager_increment_cb),
	                 &read_called);
	g_signal_connect(manager, "unread",
	                 G_CALLBACK(test_purple_notification_manager_increment_cb),
	                 &unread_called);
	g_signal_connect(manager, "notify::unread-count",
	                 G_CALLBACK(test_purple_notification_manager_unread_count_cb),
	                 &unread_count_called);

	/* Create the notification and add a reference to it before we give our
	 * original refernce to the manager.
	 */
	notification = purple_notification_new(PURPLE_NOTIFICATION_TYPE_GENERIC,
	                                       NULL,
	                                       NULL,
	                                       NULL);

	g_object_ref(notification);

	purple_notification_manager_add(manager, notification);

	/* Verify that the read and unread signals were not yet emitted. */
	g_assert_cmpint(read_called, ==, 0);
	g_assert_cmpint(unread_called, ==, 0);

	/* Verify that the unread_count property changed. */
	g_assert_cmpint(unread_count_called, ==, 1);

	/* Now mark the notification as read. */
	purple_notification_set_read(notification, TRUE);

	g_assert_cmpint(read_called, ==, 1);
	g_assert_cmpint(unread_called, ==, 0);

	g_assert_cmpint(unread_count_called, ==, 2);

	/* Now mark the notification as unread. */
	purple_notification_set_read(notification, FALSE);

	g_assert_cmpint(read_called, ==, 1);
	g_assert_cmpint(unread_called, ==, 1);

	g_assert_cmpint(unread_count_called, ==, 3);

	/* Cleanup. */
	g_clear_object(&notification);
	g_clear_object(&manager);
}

/******************************************************************************
 * Main
 *****************************************************************************/
gint
main(gint argc, gchar *argv[]) {
	GMainLoop *loop = NULL;
	gint ret = 0;

	g_test_init(&argc, &argv, NULL);

	test_ui_purple_init();

	loop = g_main_loop_new(NULL, FALSE);

	g_test_add_func("/notification-manager/get-default",
	                test_purple_notification_manager_get_default);
	g_test_add_func("/notification-manager/add-remove",
	                test_purple_notification_manager_add_remove);
	g_test_add_func("/notification-manager/double-add",
	                test_purple_notification_manager_double_add);
	g_test_add_func("/notification-manager/double-remove",
	                test_purple_notification_manager_double_remove);

	g_test_add_func("/notification-manager/read-propagation",
	                test_purple_notification_manager_read_propagation);

	ret = g_test_run();

	g_main_loop_unref(loop);

	return ret;
}
