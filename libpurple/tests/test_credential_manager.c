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
 * Globals
 *****************************************************************************/

/* Since we're using GTask to test asynchronous functions, we need to use a
 * main loop.
 */
static GMainLoop *loop = NULL;

/******************************************************************************
 * Helpers
 *****************************************************************************/
static gboolean
test_purple_credential_manager_timeout_cb(gpointer data) {
	g_main_loop_quit((GMainLoop *)data);

	g_warning("timed out waiting for the callback function to be called");

	return FALSE;
}

/******************************************************************************
 * TestPurpleCredentialProvider Implementation
 *****************************************************************************/
#define TEST_PURPLE_TYPE_CREDENTIAL_PROVIDER (test_purple_credential_provider_get_type())
G_DECLARE_FINAL_TYPE(TestPurpleCredentialProvider,
                     test_purple_credential_provider,
                     TEST_PURPLE, CREDENTIAL_PROVIDER,
                     PurpleCredentialProvider)

struct _TestPurpleCredentialProvider {
	PurpleCredentialProvider parent;

	PurpleRequestFields *fields;
};

G_DEFINE_TYPE(TestPurpleCredentialProvider,
              test_purple_credential_provider,
              PURPLE_TYPE_CREDENTIAL_PROVIDER)

static void
test_purple_credential_provider_read_password_async(PurpleCredentialProvider *p,
                                                    PurpleAccount *account,
                                                    GCancellable *cancellable,
                                                    GAsyncReadyCallback callback,
                                                    gpointer data)
{
	GTask *task = NULL;

	task = g_task_new(p, cancellable, callback, data);
	g_task_return_pointer(task, g_strdup("password"), g_free);
	g_clear_object(&task);
}

static gchar *
test_purple_credential_provider_read_password_finish(PurpleCredentialProvider *p,
                                                     PurpleAccount *account,
                                                     GAsyncResult *result,
                                                     GError **error)
{
	g_return_val_if_fail(g_task_is_valid(result, p), NULL);

	return g_task_propagate_pointer(G_TASK(result), error);
}

static void
test_purple_credential_provider_write_password_async(PurpleCredentialProvider *p,
                                                     PurpleAccount *account,
                                                     const gchar *password,
                                                     GCancellable *cancellable,
                                                     GAsyncReadyCallback callback,
                                                     gpointer data)
{
	GTask *task = NULL;

	task = g_task_new(p, cancellable, callback, data);
	g_task_return_boolean(task, TRUE);
	g_clear_object(&task);
}

static gboolean
test_purple_credential_provider_write_password_finish(PurpleCredentialProvider *p,
                                                      PurpleAccount *account,
                                                      GAsyncResult *result,
                                                      GError **error)
{
	g_return_val_if_fail(g_task_is_valid(result, p), FALSE);

	return g_task_propagate_boolean(G_TASK(result), error);
}

static void
test_purple_credential_provider_clear_password_async(PurpleCredentialProvider *p,
                                                     PurpleAccount *account,
                                                     GCancellable *cancellable,
                                                     GAsyncReadyCallback callback,
                                                     gpointer data)
{
	GTask *task = NULL;

	task = g_task_new(p, cancellable, callback, data);
	g_task_return_boolean(task, TRUE);
	g_clear_object(&task);
}

static gboolean
test_purple_credential_provider_clear_password_finish(PurpleCredentialProvider *p,
                                                      PurpleAccount *account,
                                                      GAsyncResult *result,
                                                      GError **error)
{
	g_return_val_if_fail(g_task_is_valid(result, p), FALSE);

	return g_task_propagate_boolean(G_TASK(result), error);
}

static PurpleRequestFields *
test_purple_credential_provider_read_settings(PurpleCredentialProvider *p) {
	return purple_request_fields_new();
}

static gboolean
test_purple_credential_provider_write_settings(PurpleCredentialProvider *p,
                                               PurpleRequestFields *fields)
{
	TestPurpleCredentialProvider *tp = TEST_PURPLE_CREDENTIAL_PROVIDER(p);

	tp->fields = fields;

	return TRUE;
}

static void
test_purple_credential_provider_init(TestPurpleCredentialProvider *provider) {
}

static void
test_purple_credential_provider_class_init(TestPurpleCredentialProviderClass *klass)
{
	PurpleCredentialProviderClass *provider_class = PURPLE_CREDENTIAL_PROVIDER_CLASS(klass);

	provider_class->read_password_async = test_purple_credential_provider_read_password_async;
	provider_class->read_password_finish = test_purple_credential_provider_read_password_finish;
	provider_class->write_password_async = test_purple_credential_provider_write_password_async;
	provider_class->write_password_finish = test_purple_credential_provider_write_password_finish;
	provider_class->clear_password_async = test_purple_credential_provider_clear_password_async;
	provider_class->clear_password_finish = test_purple_credential_provider_clear_password_finish;
	provider_class->read_settings = test_purple_credential_provider_read_settings;
	provider_class->write_settings = test_purple_credential_provider_write_settings;
}

static PurpleCredentialProvider *
test_purple_credential_provider_new(void) {
	return g_object_new(
		TEST_PURPLE_TYPE_CREDENTIAL_PROVIDER,
		"id", "test-provider",
		"name", "Test Provider",
		NULL);
}

/******************************************************************************
 * Get Default Tests
 *****************************************************************************/
static void
test_purple_credential_manager_get_default(void) {
	PurpleCredentialManager *manager1 = NULL, *manager2 = NULL;

	manager1 = purple_credential_manager_get_default();
	g_assert_true(PURPLE_IS_CREDENTIAL_MANAGER(manager1));

	manager2 = purple_credential_manager_get_default();
	g_assert_true(PURPLE_IS_CREDENTIAL_MANAGER(manager2));

	g_assert_true(manager1 == manager2);
}

/******************************************************************************
 * Registration Tests
 *****************************************************************************/
static void
test_purple_credential_manager_registration(void) {
	PurpleCredentialManager *manager = NULL;
	PurpleCredentialProvider *provider = NULL;
	GError *error = NULL;
	gboolean r = FALSE;

	manager = purple_credential_manager_get_default();
	g_assert_true(PURPLE_IS_CREDENTIAL_MANAGER(manager));

	provider = test_purple_credential_provider_new();

	/* Register the first time cleanly. */
	r = purple_credential_manager_register_provider(manager, provider, &error);
	g_assert_true(r);
	g_assert_no_error(error);

	/* Register again and verify the error. */
	r = purple_credential_manager_register_provider(manager, provider, &error);
	g_assert_false(r);
	g_assert_error(error, PURPLE_CREDENTIAL_MANAGER_DOMAIN, 0);
	g_clear_error(&error);

	/* Unregister the provider. */
	r = purple_credential_manager_unregister_provider(manager, provider,
	                                                  &error);
	g_assert_true(r);
	g_assert_no_error(error);

	/* Unregister the provider again and verify the error. */
	r = purple_credential_manager_unregister_provider(manager, provider,
	                                                  &error);
	g_assert_false(r);
	g_assert_error(error, PURPLE_CREDENTIAL_MANAGER_DOMAIN, 0);
	g_clear_error(&error);

	/* Final clean ups. */
	g_clear_object(&provider);
}

/******************************************************************************
 * Set Active Tests
 *****************************************************************************/
static void
test_purple_credential_manager_set_active_null(void) {
	PurpleCredentialManager *manager = NULL;
	GError *error = NULL;
	gboolean ret = FALSE;

	manager = purple_credential_manager_get_default();

	ret = purple_credential_manager_set_active_provider(manager, NULL, &error);

	g_assert_true(ret);
	g_assert_no_error(error);

}

static void
test_purple_credential_manager_set_active_non_existent(void) {
	PurpleCredentialManager *manager = NULL;
	GError *error = NULL;
	gboolean ret = FALSE;

	manager = purple_credential_manager_get_default();

	ret = purple_credential_manager_set_active_provider(manager, "foo", &error);

	g_assert_false(ret);
	g_assert_error(error, PURPLE_CREDENTIAL_MANAGER_DOMAIN, 0);
	g_clear_error(&error);
}

static void
test_purple_credential_manager_set_active_normal(void) {
	PurpleCredentialManager *manager = NULL;
	PurpleCredentialProvider *provider = NULL;
	GError *error = NULL;
	gboolean r = FALSE;

	manager = purple_credential_manager_get_default();

	/* Create the provider and register it in the manager. */
	provider = test_purple_credential_provider_new();
	r = purple_credential_manager_register_provider(manager, provider, &error);
	g_assert_true(r);
	g_assert_no_error(error);

	/* Set the provider as active and verify it was successful. */
	r = purple_credential_manager_set_active_provider(manager, "test-provider",
	                                                  &error);
	g_assert_true(r);
	g_assert_no_error(error);

	/* Verify that unregistering the provider fails. */
	r = purple_credential_manager_unregister_provider(manager, provider,
	                                                  &error);
	g_assert_false(r);
	g_assert_error(error, PURPLE_CREDENTIAL_MANAGER_DOMAIN, 0);
	g_clear_error(&error);

	/* Now unset the active provider. */
	r = purple_credential_manager_set_active_provider(manager, NULL, &error);
	g_assert_true(r);
	g_assert_no_error(error);

	/* Finally unregister the provider now that it's no longer active. */
	r = purple_credential_manager_unregister_provider(manager, provider,
	                                                  &error);
	g_assert_true(r);
	g_assert_no_error(error);

	/* And our final cleanup. */
	g_clear_object(&provider);
}

/******************************************************************************
 * No Provider Tests
 *****************************************************************************/
static void
test_purple_credential_manager_no_provider_read_password_cb(GObject *obj,
                                                            GAsyncResult *res,
                                                            gpointer d)
{
	PurpleCredentialManager *manager = PURPLE_CREDENTIAL_MANAGER(obj);
	PurpleAccount *account = PURPLE_ACCOUNT(d);
	GError *error = NULL;
	gchar *password = NULL;

	password = purple_credential_manager_read_password_finish(manager, account,
	                                                          res, &error);
	g_assert_error(error, PURPLE_CREDENTIAL_MANAGER_DOMAIN, 0);
	g_assert_null(password);

	g_clear_object(&account);

	g_main_loop_quit(loop);
}

static gboolean
test_purple_credential_manager_no_provider_read_password_idle(gpointer data) {
	PurpleCredentialManager *m = PURPLE_CREDENTIAL_MANAGER(data);
	PurpleAccount *account = NULL;

	account = purple_account_new("test", "test");

	purple_credential_manager_read_password_async(m, account, NULL,
	                                              test_purple_credential_manager_no_provider_read_password_cb,
	                                              account);

	return FALSE;
}

static void
test_purple_credential_manager_no_provider_read_password_async(void) {
	PurpleCredentialManager *m = purple_credential_manager_get_default();

	g_idle_add(test_purple_credential_manager_no_provider_read_password_idle, m);
	g_timeout_add_seconds(10, test_purple_credential_manager_timeout_cb, loop);

	g_main_loop_run(loop);
}

static void
test_purple_credential_manager_no_provider_write_password_cb(GObject *obj,
                                                             GAsyncResult *res,
                                                             gpointer d)
{
	PurpleCredentialManager *manager = PURPLE_CREDENTIAL_MANAGER(obj);
	PurpleAccount *account = PURPLE_ACCOUNT(d);
	GError *error = NULL;
	gboolean r = FALSE;

	r = purple_credential_manager_write_password_finish(manager, account, res,
	                                                    &error);
	g_assert_false(r);
	g_assert_error(error, PURPLE_CREDENTIAL_MANAGER_DOMAIN, 0);

	g_clear_object(&account);

	g_main_loop_quit(loop);
}

static gboolean
test_purple_credential_manager_no_provider_write_password_idle(gpointer data) {
	PurpleCredentialManager *m = PURPLE_CREDENTIAL_MANAGER(data);
	PurpleAccount *account = NULL;

	account = purple_account_new("test", "test");

	purple_credential_manager_write_password_async(m, account, NULL, NULL,
	                                               test_purple_credential_manager_no_provider_write_password_cb,
	                                               account);

	return FALSE;
}

static void
test_purple_credential_manager_no_provider_write_password_async(void) {
	PurpleCredentialManager *m = purple_credential_manager_get_default();

	g_idle_add(test_purple_credential_manager_no_provider_write_password_idle,
	           m);
	g_timeout_add_seconds(10, test_purple_credential_manager_timeout_cb, loop);

	g_main_loop_run(loop);
}

static void
test_purple_credential_manager_no_provider_clear_password_cb(GObject *obj,
                                                             GAsyncResult *res,
                                                             gpointer d)
{
	PurpleCredentialManager *manager = PURPLE_CREDENTIAL_MANAGER(obj);
	PurpleAccount *account = PURPLE_ACCOUNT(d);
	GError *error = NULL;
	gboolean r = FALSE;

	r = purple_credential_manager_clear_password_finish(manager, account, res,
	                                                    &error);
	g_assert_false(r);
	g_assert_error(error, PURPLE_CREDENTIAL_MANAGER_DOMAIN, 0);

	g_clear_object(&account);

	g_main_loop_quit(loop);
}

static gboolean
test_purple_credential_manager_no_provider_clear_password_idle(gpointer data) {
	PurpleCredentialManager *m = PURPLE_CREDENTIAL_MANAGER(data);
	PurpleAccount *account = NULL;

	account = purple_account_new("test", "test");

	purple_credential_manager_clear_password_async(m, account, NULL,
	                                               test_purple_credential_manager_no_provider_clear_password_cb,
	                                               account);

	return FALSE;
}

static void
test_purple_credential_manager_no_provider_clear_password_async(void) {
	PurpleCredentialManager *m = purple_credential_manager_get_default();

	g_idle_add(test_purple_credential_manager_no_provider_clear_password_idle,
	           m);
	g_timeout_add_seconds(10, test_purple_credential_manager_timeout_cb, loop);

	g_main_loop_run(loop);
}

static void
test_purple_credential_manager_no_provider_read_settings(void) {
	PurpleCredentialManager *manager = NULL;
	PurpleRequestFields *fields = NULL;
	GError *error = NULL;

	manager = purple_credential_manager_get_default();

	fields = purple_credential_manager_read_settings(manager, &error);

	g_assert_error(error, PURPLE_CREDENTIAL_MANAGER_DOMAIN, 0);
	g_clear_error(&error);

	g_assert_null(fields);
}

static void
test_purple_credential_manager_no_provider_write_settings(void) {
	PurpleCredentialManager *manager = NULL;
	GError *error = NULL;

	manager = purple_credential_manager_get_default();

	purple_credential_manager_write_settings(manager, NULL, &error);

	g_assert_error(error, PURPLE_CREDENTIAL_MANAGER_DOMAIN, 0);
	g_clear_error(&error);

}

/******************************************************************************
 * Provider Tests
 *****************************************************************************/
static void
test_purple_credential_manager_read_password_cb(GObject *obj, GAsyncResult *res,
                                                gpointer d)
{
	PurpleCredentialManager *manager = PURPLE_CREDENTIAL_MANAGER(obj);
	PurpleAccount *account = PURPLE_ACCOUNT(d);
	GError *error = NULL;
	gchar *password = NULL;

	password = purple_credential_manager_read_password_finish(manager, account,
	                                                          res, &error);
	g_assert_no_error(error);
	g_assert_cmpstr(password, ==, "password");
	g_free(password);

	g_clear_object(&account);

	g_main_loop_quit(loop);
}

static gboolean
test_purple_credential_manager_read_password_idle(gpointer data) {
	PurpleCredentialManager *m = PURPLE_CREDENTIAL_MANAGER(data);
	PurpleAccount *account = NULL;

	account = purple_account_new("test", "test");

	purple_credential_manager_read_password_async(m, account, NULL,
	                                              test_purple_credential_manager_read_password_cb,
	                                              account);

	return FALSE;
}

static void
test_purple_credential_manager_read_password_async(void) {
	PurpleCredentialManager *m = purple_credential_manager_get_default();
	PurpleCredentialProvider *p = test_purple_credential_provider_new();
	GError *e = NULL;
	gboolean r = FALSE;

	r = purple_credential_manager_register_provider(m, p, &e);
	g_assert_true(r);
	g_assert_no_error(e);

	r = purple_credential_manager_set_active_provider(m, "test-provider", &e);
	g_assert_true(r);
	g_assert_no_error(e);

	g_idle_add(test_purple_credential_manager_read_password_idle, m);
	g_timeout_add_seconds(10, test_purple_credential_manager_timeout_cb, loop);

	g_main_loop_run(loop);

	r = purple_credential_manager_set_active_provider(m, NULL, &e);
	g_assert_true(r);
	g_assert_no_error(e);

	r = purple_credential_manager_unregister_provider(m, p, &e);
	g_assert_true(r);
	g_assert_no_error(e);

	g_clear_object(&p);
}

static void
test_purple_credential_manager_write_password_cb(GObject *obj,
                                                 GAsyncResult *res, gpointer d)
{
	PurpleCredentialManager *manager = PURPLE_CREDENTIAL_MANAGER(obj);
	PurpleAccount *account = PURPLE_ACCOUNT(d);
	GError *error = NULL;
	gboolean r = FALSE;

	r = purple_credential_manager_write_password_finish(manager, account, res,
	                                                    &error);
	g_assert_true(r);
	g_assert_no_error(error);

	g_clear_object(&account);

	g_main_loop_quit(loop);
}

static gboolean
test_purple_credential_manager_write_password_idle(gpointer data) {
	PurpleCredentialManager *m = PURPLE_CREDENTIAL_MANAGER(data);
	PurpleAccount *account = NULL;

	account = purple_account_new("test", "test");

	purple_credential_manager_write_password_async(m, account, NULL, NULL,
	                                               test_purple_credential_manager_write_password_cb,
	                                               account);

	return FALSE;
}

static void
test_purple_credential_manager_write_password_async(void) {
	PurpleCredentialManager *m = purple_credential_manager_get_default();
	PurpleCredentialProvider *p = test_purple_credential_provider_new();
	GError *e = NULL;
	gboolean r = FALSE;

	r = purple_credential_manager_register_provider(m, p, &e);
	g_assert_true(r);
	g_assert_no_error(e);

	r = purple_credential_manager_set_active_provider(m, "test-provider", &e);
	g_assert_true(r);
	g_assert_no_error(e);

	g_idle_add(test_purple_credential_manager_write_password_idle, m);
	g_timeout_add_seconds(10, test_purple_credential_manager_timeout_cb, loop);

	g_main_loop_run(loop);

	r = purple_credential_manager_set_active_provider(m, NULL, &e);
	g_assert_true(r);
	g_assert_no_error(e);

	r = purple_credential_manager_unregister_provider(m, p, &e);
	g_assert_true(r);
	g_assert_no_error(e);

	g_clear_object(&p);
}

static void
test_purple_credential_manager_clear_password_cb(GObject *obj,
                                                 GAsyncResult *res, gpointer d)
{
	PurpleCredentialManager *manager = PURPLE_CREDENTIAL_MANAGER(obj);
	PurpleAccount *account = PURPLE_ACCOUNT(d);
	GError *error = NULL;
	gboolean r = FALSE;

	r = purple_credential_manager_clear_password_finish(manager, account, res,
	                                                    &error);
	g_assert_true(r);
	g_assert_no_error(error);

	g_clear_object(&account);

	g_main_loop_quit(loop);
}

static gboolean
test_purple_credential_manager_clear_password_idle(gpointer data) {
	PurpleCredentialManager *m = PURPLE_CREDENTIAL_MANAGER(data);
	PurpleAccount *account = NULL;

	account = purple_account_new("test", "test");

	purple_credential_manager_clear_password_async(m, account, NULL,
	                                               test_purple_credential_manager_clear_password_cb,
	                                               account);

	return FALSE;
}

static void
test_purple_credential_manager_clear_password_async(void) {
	PurpleCredentialManager *m = purple_credential_manager_get_default();
	PurpleCredentialProvider *p = test_purple_credential_provider_new();
	GError *e = NULL;
	gboolean r = FALSE;

	r = purple_credential_manager_register_provider(m, p, &e);
	g_assert_true(r);
	g_assert_no_error(e);

	r = purple_credential_manager_set_active_provider(m, "test-provider", &e);
	g_assert_true(r);
	g_assert_no_error(e);

	g_idle_add(test_purple_credential_manager_clear_password_idle, m);
	g_timeout_add_seconds(10, test_purple_credential_manager_timeout_cb, loop);

	g_main_loop_run(loop);

	r = purple_credential_manager_set_active_provider(m, NULL, &e);
	g_assert_true(r);
	g_assert_no_error(e);

	r = purple_credential_manager_unregister_provider(m, p, &e);
	g_assert_true(r);
	g_assert_no_error(e);

	g_clear_object(&p);
}

static void
test_purple_credential_manager_settings(void) {
	PurpleCredentialManager *m = purple_credential_manager_get_default();
	PurpleCredentialProvider *p = test_purple_credential_provider_new();
	TestPurpleCredentialProvider *tp = TEST_PURPLE_CREDENTIAL_PROVIDER(p);
	PurpleRequestFields *fields = NULL;
	GError *e = NULL;
	gboolean r = FALSE;

	r = purple_credential_manager_register_provider(m, p, &e);
	g_assert_true(r);
	g_assert_no_error(e);

	r = purple_credential_manager_set_active_provider(m, "test-provider", &e);
	g_assert_true(r);
	g_assert_no_error(e);

	fields = purple_credential_manager_read_settings(m, &e);
	g_assert_nonnull(fields);
	g_assert_no_error(e);
	purple_request_fields_destroy(fields);

	fields = purple_request_fields_new();
	r = purple_credential_manager_write_settings(m, fields, &e);
	g_assert_true(r);
	g_assert_true(tp->fields == fields);
	g_assert_no_error(e);

	purple_request_fields_destroy(fields);

	g_clear_object(&p);
}

/******************************************************************************
 * Main
 *****************************************************************************/
gint
main(gint argc, gchar *argv[]) {
	gint ret = 0;

	g_test_init(&argc, &argv, NULL);

	test_ui_purple_init();

	loop = g_main_loop_new(NULL, FALSE);

	g_test_add_func("/credential-manager/get-default",
	                test_purple_credential_manager_get_default);
	g_test_add_func("/credential-manager/registration",
	                test_purple_credential_manager_registration);
	g_test_add_func("/credential-manager/set-active/null",
	                test_purple_credential_manager_set_active_null);
	g_test_add_func("/credential-manager/set-active/non-existent",
	                test_purple_credential_manager_set_active_non_existent);
	g_test_add_func("/credential-manager/set-active/normal",
	                test_purple_credential_manager_set_active_normal);

	g_test_add_func("/credential-manager/no-provider/read-password-async",
	                test_purple_credential_manager_no_provider_read_password_async);
	g_test_add_func("/credential-manager/no-provider/write-password-async",
	                test_purple_credential_manager_no_provider_write_password_async);
	g_test_add_func("/credential-manager/no-provider/clear-password-async",
	                test_purple_credential_manager_no_provider_clear_password_async);
	g_test_add_func("/credential-manager/no-provider/read-settings",
	                test_purple_credential_manager_no_provider_read_settings);
	g_test_add_func("/credential-manager/no-provider/write-settings",
	                test_purple_credential_manager_no_provider_write_settings);

	g_test_add_func("/credential-manager/provider/read-password-async",
	                test_purple_credential_manager_read_password_async);
	g_test_add_func("/credential-manager/provider/write-password-async",
	                test_purple_credential_manager_write_password_async);
	g_test_add_func("/credential-manager/provider/clear-password-async",
	                test_purple_credential_manager_clear_password_async);
	g_test_add_func("/credential-manager/provider/settings",
	                test_purple_credential_manager_settings);

	ret = g_test_run();

	g_main_loop_unref(loop);

	return ret;
}
