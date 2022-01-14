#include <glib.h>

#include <purple.h>

#ifndef _WIN32
#define HOME g_get_home_dir()
#else
#define HOME g_getenv("APPDATA")
#endif

static void
test_purplepath_home_dir(void) {
	g_assert_cmpstr(HOME, ==, purple_home_dir());
}

static void
test_purplepath_user_dir(void) {
	gchar *user_dir;

	user_dir = g_build_filename(HOME, ".purple", NULL);
	g_assert_cmpstr(user_dir, ==, purple_user_dir());
	g_free(user_dir);
}

static void
test_purplepath_cache_dir(void) {
	gchar *cache_dir;

#ifndef _WIN32
	cache_dir = g_build_filename(g_get_home_dir(), ".cache", "purple", NULL);
#else
	cache_dir = g_build_filename(g_getenv("LOCALAPPDATA"), "Microsoft",
	                             "Windows", "INetCache","purple", NULL);
#endif
	g_assert_cmpstr(cache_dir, ==, purple_cache_dir());
	g_free(cache_dir);
}

static void
test_purplepath_config_dir(void) {
	gchar *config_dir;

#ifndef _WIN32
	config_dir = g_build_filename(g_get_home_dir(), ".config", "purple", NULL);
#else
	config_dir = g_build_filename(g_getenv("LOCALAPPDATA"), "purple", NULL);
#endif
	g_assert_cmpstr(config_dir, ==, purple_config_dir());
	g_free(config_dir);
}

static void
test_purplepath_data_dir(void) {
	gchar *data_dir;

#ifndef _WIN32
	data_dir = g_build_filename(g_get_home_dir(), ".local", "share", "purple",
	                            NULL);
#else
	data_dir = g_build_filename(g_getenv("LOCALAPPDATA"), "purple", NULL);
#endif
	g_assert_cmpstr(data_dir, ==, purple_data_dir());
	g_free(data_dir);
}

gint
main(gint argc, gchar **argv) {
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/purplepath/homedir",
	                test_purplepath_home_dir);
	g_test_add_func("/purplepath/userdir",
	                test_purplepath_user_dir);
	g_test_add_func("/purplepath/cachedir",
	                test_purplepath_cache_dir);
	g_test_add_func("/purplepath/configdir",
	                test_purplepath_config_dir);
	g_test_add_func("/purplepath/datadir",
	                test_purplepath_data_dir);
	return g_test_run();
}
