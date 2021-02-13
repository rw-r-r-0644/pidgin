/* Purple is the legal property of its developers, whose names are too numerous
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include <glib/gi18n-lib.h>

#include "internal.h"
#include "purpleprivate.h"

#include "conversation.h"
#include "core.h"
#include "debug.h"
#include "notify.h"
#include "protocol.h"
#include "prefs.h"
#include "purpleprotocolclient.h"
#include "util.h"

#include <json-glib/json-glib.h>

static char *custom_user_dir = NULL;
static char *user_dir = NULL;
static gchar *cache_dir = NULL;
static gchar *config_dir = NULL;
static gchar *data_dir = NULL;

void
purple_util_init(void)
{
}

void
purple_util_uninit(void)
{
	/* Free these so we don't have leaks at shutdown. */

	g_free(custom_user_dir);
	custom_user_dir = NULL;

	g_free(user_dir);
	user_dir = NULL;

	g_free(cache_dir);
	cache_dir = NULL;

	g_free(config_dir);
	config_dir = NULL;

	g_free(data_dir);
	data_dir = NULL;
}

/**************************************************************************
 * Date/Time Functions
 **************************************************************************/

const char *
purple_utf8_strftime(const char *format, const struct tm *tm)
{
	static char buf[128];
	GDateTime *dt;
	char *utf8;

	g_return_val_if_fail(format != NULL, NULL);

	if (tm == NULL)
	{
		dt = g_date_time_new_now_local();
	} else {
		dt = g_date_time_new_local(tm->tm_year + 1900, tm->tm_mon + 1,
				tm->tm_mday, tm->tm_hour,
				tm->tm_min, tm->tm_sec);
	}

	utf8 = g_date_time_format(dt, format);
	g_date_time_unref(dt);

	if (utf8 == NULL) {
		purple_debug_error("util",
				"purple_utf8_strftime(): Formatting failed\n");
		return "";
	}

	g_strlcpy(buf, utf8, sizeof(buf));
	g_free(utf8);
	return buf;
}

const char *
purple_date_format_full(const struct tm *tm)
{
	return purple_utf8_strftime("%c", tm);
}

/* originally taken from GLib trunk 1-6-11 */
/* originally licensed as LGPL 2+ */
static time_t
mktime_utc(struct tm *tm)
{
	time_t retval;

#ifndef HAVE_TIMEGM
	static const gint days_before[] =
	{
		0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
	};
#endif

#ifndef HAVE_TIMEGM
	if (tm->tm_mon < 0 || tm->tm_mon > 11)
		return (time_t) -1;

	retval = (tm->tm_year - 70) * 365;
	retval += (tm->tm_year - 68) / 4;
	retval += days_before[tm->tm_mon] + tm->tm_mday - 1;

	if (tm->tm_year % 4 == 0 && tm->tm_mon < 2)
		retval -= 1;

	retval = ((((retval * 24) + tm->tm_hour) * 60) + tm->tm_min) * 60 + tm->tm_sec;
#else
	retval = timegm (tm);
#endif /* !HAVE_TIMEGM */

	return retval;
}

time_t
purple_str_to_time(const char *timestamp, gboolean utc,
	struct tm *tm, long *tz_off, const char **rest)
{
	struct tm t;
	const gchar *str;
	gint year = 0;
	long tzoff = PURPLE_NO_TZ_OFF;
	time_t retval;
	gboolean mktime_with_utc = FALSE;

	if (rest != NULL)
		*rest = NULL;

	g_return_val_if_fail(timestamp != NULL, 0);

	memset(&t, 0, sizeof(struct tm));

	str = timestamp;

	/* Strip leading whitespace */
	while (g_ascii_isspace(*str))
		str++;

	if (*str == '\0') {
		if (rest != NULL) {
			*rest = str;
		}

		return 0;
	}

	if (!g_ascii_isdigit(*str) && *str != '-' && *str != '+') {
		if (rest != NULL && *str != '\0')
			*rest = str;

		return 0;
	}

	/* 4 digit year */
	if (sscanf(str, "%04d", &year) && year >= 1900) {
		str += 4;

		if (*str == '-' || *str == '/')
			str++;

		t.tm_year = year - 1900;
	}

	/* 2 digit month */
	if (!sscanf(str, "%02d", &t.tm_mon)) {
		if (rest != NULL && *str != '\0')
			*rest = str;

		return 0;
	}

	str += 2;
	t.tm_mon -= 1;

	if (*str == '-' || *str == '/')
		str++;

	/* 2 digit day */
	if (!sscanf(str, "%02d", &t.tm_mday)) {
		if (rest != NULL && *str != '\0')
			*rest = str;

		return 0;
	}

	str += 2;

	/* Grab the year off the end if there's still stuff */
	if (*str == '/' || *str == '-') {
		/* But make sure we don't read the year twice */
		if (year >= 1900) {
			if (rest != NULL && *str != '\0')
				*rest = str;

			return 0;
		}

		str++;

		if (!sscanf(str, "%04d", &t.tm_year)) {
			if (rest != NULL && *str != '\0')
				*rest = str;

			return 0;
		}

		t.tm_year -= 1900;
	} else if (*str == 'T' || *str == '.') {
		str++;

		/* Continue grabbing the hours/minutes/seconds */
		if ((sscanf(str, "%02d:%02d:%02d", &t.tm_hour, &t.tm_min, &t.tm_sec) == 3 &&
				(str += 8)) ||
		    (sscanf(str, "%02d%02d%02d", &t.tm_hour, &t.tm_min, &t.tm_sec) == 3 &&
				(str += 6)))
		{
			gint sign, tzhrs, tzmins;

			if (*str == '.') {
				/* Cut off those pesky micro-seconds */
				do {
					str++;
				} while (*str >= '0' && *str <= '9');
			}

			sign = (*str == '+') ? 1 : -1;

			/* Process the timezone */
			if (*str == '+' || *str == '-') {
				str++;

				if (((sscanf(str, "%02d:%02d", &tzhrs, &tzmins) == 2 && (str += 5)) ||
					(sscanf(str, "%02d%02d", &tzhrs, &tzmins) == 2 && (str += 4))))
				{
					mktime_with_utc = TRUE;
					tzoff = tzhrs * 60 * 60 + tzmins * 60;
					tzoff *= sign;
				}
			} else if (*str == 'Z') {
				/* 'Z' = Zulu = UTC */
				str++;
				mktime_with_utc = TRUE;
				tzoff = 0;
			}

			if (!mktime_with_utc)
			{
				/* No timezone specified. */

				if (utc) {
					mktime_with_utc = TRUE;
					tzoff = 0;
				} else {
					/* Local Time */
					t.tm_isdst = -1;
				}
			}
		}
	}

	if (rest != NULL && *str != '\0') {
		/* Strip trailing whitespace */
		while (g_ascii_isspace(*str))
			str++;

		if (*str != '\0')
			*rest = str;
	}

	if (mktime_with_utc)
		retval = mktime_utc(&t);
	else
		retval = mktime(&t);

	if (tm != NULL)
		*tm = t;

	if (tzoff != PURPLE_NO_TZ_OFF)
		retval -= tzoff;

	if (tz_off != NULL)
		*tz_off = tzoff;

	return retval;
}

GDateTime *
purple_str_to_date_time(const char *timestamp, gboolean utc)
{
	const gchar *str;
	gint year = 0;
	gint month = 0;
	gint day = 0;
	gint hour = 0;
	gint minute = 0;
	gint seconds = 0;
	gint microseconds = 0;
	int chars = 0;
	GTimeZone *tz = NULL;
	GDateTime *retval;

	g_return_val_if_fail(timestamp != NULL, NULL);

	str = timestamp;

	/* Strip leading whitespace */
	while (g_ascii_isspace(*str))
		str++;

	if (*str == '\0') {
		return NULL;
	}

	if (!g_ascii_isdigit(*str) && *str != '-' && *str != '+') {
		return NULL;
	}

	/* 4 digit year */
	if (sscanf(str, "%04d", &year) && year > 0) {
		str += 4;

		if (*str == '-' || *str == '/')
			str++;
	}

	/* 2 digit month */
	if (!sscanf(str, "%02d", &month)) {
		return NULL;
	}

	str += 2;

	if (*str == '-' || *str == '/')
		str++;

	/* 2 digit day */
	if (!sscanf(str, "%02d", &day)) {
		return NULL;
	}

	str += 2;

	/* Grab the year off the end if there's still stuff */
	if (*str == '/' || *str == '-') {
		/* But make sure we don't read the year twice */
		if (year > 0) {
			return NULL;
		}

		str++;

		if (!sscanf(str, "%04d", &year)) {
			return NULL;
		}
	} else if (*str == 'T' || *str == '.') {
		str++;

		/* Continue grabbing the hours/minutes/seconds */
		if ((sscanf(str, "%02d:%02d:%02d", &hour, &minute, &seconds) == 3 &&
				(str += 8)) ||
		    (sscanf(str, "%02d%02d%02d", &hour, &minute, &seconds) == 3 &&
				(str += 6)))
		{
			if (*str == '.') {
				str++;
				if (sscanf(str, "%d%n", &microseconds, &chars) == 1) {
					str += chars;
				}
			}

			if (*str) {
				const gchar *end = str;
				if (*end == '+' || *end == '-') {
					end++;
				}

				while (isdigit(*end) || *end == ':') {
					end++;
				}

				if (str != end) {
					/* Trim anything trailing a purely numeric time zone. */
					gchar *tzstr = g_strndup(str, end - str);
					tz = g_time_zone_new(tzstr);
					g_free(tzstr);
				} else {
					/* Just try whatever is there. */
					tz = g_time_zone_new(str);
				}
			}
		}
	}

	if (!tz) {
		/* No timezone specified. */
		if (utc) {
			tz = g_time_zone_new_utc();
		} else {
			tz = g_time_zone_new_local();
		}
	}

	retval = g_date_time_new(tz, year, month, day, hour, minute,
	                         seconds + microseconds * pow(10, -chars));
	g_time_zone_unref(tz);

	return retval;
}

gint purple_time_parse_month(const char *month_abbr)
{
	const char *months[] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
		NULL};
	for (gint month = 0; months[month] != NULL; month++) {
		if (purple_strequal(month_abbr, months[month])) {
			return month + 1;
		}
	}
	return 0;
}

/**************************************************************************
 * Path/Filename Functions
 **************************************************************************/
const char *
purple_home_dir(void)
{
#ifndef _WIN32
	return g_get_home_dir();
#else
	return wpurple_home_dir();
#endif
}

/* Returns the argument passed to -c IFF it was present, or ~/.purple. */
const char *
purple_user_dir(void)
{
	if (custom_user_dir != NULL)
		return custom_user_dir;
	else if (!user_dir)
		user_dir = g_build_filename(purple_home_dir(), ".purple", NULL);

	return user_dir;
}

static const gchar *
purple_xdg_dir(gchar **xdg_dir, const gchar *xdg_base_dir, const gchar *xdg_type)
{
	if (!*xdg_dir) {
		if (!custom_user_dir) {
			*xdg_dir = g_build_filename(xdg_base_dir, "purple", NULL);
		} else {
			*xdg_dir = g_build_filename(custom_user_dir, xdg_type, NULL);
		}
	}

	return *xdg_dir;
}

const gchar *
purple_cache_dir(void)
{
	return purple_xdg_dir(&cache_dir, g_get_user_cache_dir(), "cache");
}

const gchar *
purple_config_dir(void)
{
	return purple_xdg_dir(&config_dir, g_get_user_config_dir(), "config");
}

const gchar *
purple_data_dir(void)
{
	return purple_xdg_dir(&data_dir, g_get_user_data_dir(), "data");
}

gboolean
purple_move_to_xdg_base_dir(const char *purple_xdg_dir, char *path)
{
	gint mkdir_res;
	gchar *xdg_path;
	gboolean xdg_path_exists;

	/* Create destination directory */
	mkdir_res = g_mkdir_with_parents(purple_xdg_dir, S_IRWXU);
	if (mkdir_res == -1) {
		purple_debug_error("util", "Error creating xdg directory %s: %s; failed migration\n",
					purple_xdg_dir, g_strerror(errno));
		return FALSE;
	}

	xdg_path = g_build_filename(purple_xdg_dir, path, NULL);
	xdg_path_exists = g_file_test(xdg_path, G_FILE_TEST_EXISTS);
	if (!xdg_path_exists) {
		gchar *old_path;
		gboolean old_path_exists;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
		old_path = g_build_filename(purple_user_dir(), path, NULL);
G_GNUC_END_IGNORE_DEPRECATIONS
		old_path_exists = g_file_test(old_path, G_FILE_TEST_EXISTS);
		if (old_path_exists) {
			int rename_res;

			rename_res = g_rename(old_path, xdg_path);
			if (rename_res == -1) {
				purple_debug_error("util", "Error renaming %s to %s; failed migration\n",
							old_path, xdg_path);
				g_free(old_path);
				g_free(xdg_path);

				return FALSE;
			}
		}

		g_free(old_path);
	}

	g_free(xdg_path);

	return TRUE;
}

void purple_util_set_user_dir(const char *dir)
{
	g_free(custom_user_dir);

	if (dir != NULL && *dir)
		custom_user_dir = g_strdup(dir);
	else
		custom_user_dir = NULL;
}

static gboolean
purple_util_write_data_to_file_common(const char *dir, const char *filename, const char *data, gssize size)
{
	gchar *filename_full;
	gboolean ret = FALSE;

	g_return_val_if_fail(dir != NULL, FALSE);

	purple_debug_misc("util", "Writing file %s to directory %s",
			  filename, dir);

	/* Ensure the directory exists */
	if (!g_file_test(dir, G_FILE_TEST_IS_DIR))
	{
		if (g_mkdir(dir, S_IRUSR | S_IWUSR | S_IXUSR) == -1)
		{
			purple_debug_error("util", "Error creating directory %s: %s\n",
					   dir, g_strerror(errno));
			return FALSE;
		}
	}

	filename_full = g_build_filename(dir, filename, NULL);
	ret = g_file_set_contents(filename_full, data, size, NULL);
	g_free(filename_full);

	return ret;
}

gboolean
purple_util_write_data_to_file(const char *filename, const char *data, gssize size)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	const char *user_dir = purple_user_dir();
G_GNUC_END_IGNORE_DEPRECATIONS
	gboolean ret = purple_util_write_data_to_file_common(user_dir, filename, data, size);

	return ret;
}

gboolean
purple_util_write_data_to_cache_file(const char *filename, const char *data, gssize size)
{
	const char *cache_dir = purple_cache_dir();
	gboolean ret = purple_util_write_data_to_file_common(cache_dir, filename, data, size);

	return ret;
}

gboolean
purple_util_write_data_to_config_file(const char *filename, const char *data, gssize size)
{
	const char *config_dir = purple_config_dir();
	gboolean ret = purple_util_write_data_to_file_common(config_dir, filename, data, size);

	return ret;
}

gboolean
purple_util_write_data_to_data_file(const char *filename, const char *data, gssize size)
{
	const char *data_dir = purple_data_dir();
	gboolean ret = purple_util_write_data_to_file_common(data_dir, filename, data, size);

	return ret;
}

gboolean
purple_util_write_data_to_file_absolute(const char *filename_full, const char *data, gssize size)
{
	GFile *file;
	GError *err = NULL;

	g_return_val_if_fail(size >= -1, FALSE);

	if (size == -1) {
		size = strlen(data);
	}

	file = g_file_new_for_path(filename_full);

	if (!g_file_replace_contents(file, data, size, NULL, FALSE,
			G_FILE_CREATE_PRIVATE, NULL, NULL, &err)) {
		purple_debug_error("util", "Error writing file: %s: %s\n",
				   filename_full, err->message);
		g_clear_error(&err);
		g_object_unref(file);
		return FALSE;
	}

	g_object_unref(file);
	return TRUE;
}

PurpleXmlNode *
purple_util_read_xml_from_file(const char *filename, const char *description)
{
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	return purple_xmlnode_from_file(purple_user_dir(), filename, description, "util");
G_GNUC_END_IGNORE_DEPRECATIONS
}

PurpleXmlNode *
purple_util_read_xml_from_cache_file(const char *filename, const char *description)
{
	return purple_xmlnode_from_file(purple_cache_dir(), filename, description, "util");
}

PurpleXmlNode *
purple_util_read_xml_from_config_file(const char *filename, const char *description)
{
	return purple_xmlnode_from_file(purple_config_dir(), filename, description, "util");
}

PurpleXmlNode *
purple_util_read_xml_from_data_file(const char *filename, const char *description)
{
	return purple_xmlnode_from_file(purple_data_dir(), filename, description, "util");
}

/*
 * Like mkstemp() but returns a file pointer, uses a pre-set template,
 * uses the semantics of tempnam() for the directory to use and allocates
 * the space for the filepath.
 *
 * Caller is responsible for closing the file and removing it when done,
 * as well as freeing the space pointed-to by "path" with g_free().
 *
 * Returns NULL on failure and cleans up after itself if so.
 */
static const char *purple_mkstemp_templ = {"purpleXXXXXX"};

FILE *
purple_mkstemp(char **fpath, gboolean binary)
{
	const gchar *tmpdir;
	int fd;
	FILE *fp = NULL;

	g_return_val_if_fail(fpath != NULL, NULL);

	if((tmpdir = (gchar*)g_get_tmp_dir()) != NULL) {
		if((*fpath = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s", tmpdir, purple_mkstemp_templ)) != NULL) {
			fd = g_mkstemp(*fpath);
			if(fd == -1) {
				purple_debug_error("purple_mkstemp", "Couldn't make \"%s\", error: %d",
				                   *fpath, errno);
			} else {
				if((fp = fdopen(fd, "r+")) == NULL) {
					close(fd);
					purple_debug_error("purple_mkstemp", "Couldn't fdopen(), error: %d", errno);
				}
			}

			if(!fp) {
				g_free(*fpath);
				*fpath = NULL;
			}
		}
	} else {
		purple_debug_error("purple_mkstemp", "g_get_tmp_dir() failed!");
	}

	return fp;
}

gboolean
purple_program_is_valid(const char *program)
{
	GError *error = NULL;
	char **argv;
	gchar *progname;
	gboolean is_valid = FALSE;

	g_return_val_if_fail(program != NULL,  FALSE);
	g_return_val_if_fail(*program != '\0', FALSE);

	if (!g_shell_parse_argv(program, NULL, &argv, &error)) {
		purple_debug_error("program_is_valid", "Could not parse program '%s': %s",
		                   program, error->message);
		g_error_free(error);
		return FALSE;
	}

	if (argv == NULL) {
		return FALSE;
	}

	progname = g_find_program_in_path(argv[0]);
	is_valid = (progname != NULL);

	if(purple_debug_is_verbose())
		purple_debug_info("program_is_valid", "Tested program %s.  %s.\n", program,
				is_valid ? "Valid" : "Invalid");

	g_strfreev(argv);
	g_free(progname);

	return is_valid;
}


gboolean
purple_running_gnome(void)
{
#ifndef _WIN32
	gchar *tmp = g_find_program_in_path("gvfs-open");

	if (tmp == NULL) {
		tmp = g_find_program_in_path("gnome-open");

		if (tmp == NULL) {
			return FALSE;
		}
	}

	g_free(tmp);

	tmp = (gchar *)g_getenv("GNOME_DESKTOP_SESSION_ID");

	return ((tmp != NULL) && (*tmp != '\0'));
#else
	return FALSE;
#endif
}

gboolean
purple_running_kde(void)
{
#ifndef _WIN32
	gchar *tmp = g_find_program_in_path("kfmclient");
	const char *session;

	if (tmp == NULL)
		return FALSE;
	g_free(tmp);

	session = g_getenv("KDE_FULL_SESSION");
	if (purple_strequal(session, "true"))
		return TRUE;

	/* If you run Purple from Konsole under !KDE, this will provide a
	 * a false positive.  Since we do the GNOME checks first, this is
	 * only a problem if you're running something !(KDE || GNOME) and
	 * you run Purple from Konsole. This really shouldn't be a problem. */
	return ((g_getenv("KDEDIR") != NULL) || g_getenv("KDEDIRS") != NULL);
#else
	return FALSE;
#endif
}

gboolean
purple_running_osx(void)
{
#if defined(__APPLE__)
	return TRUE;
#else
	return FALSE;
#endif
}

/**************************************************************************
 * String Functions
 **************************************************************************/
const char *
purple_normalize(PurpleAccount *account, const char *str)
{
	const char *ret = NULL;
	static char buf[BUF_LEN];

	/* This should prevent a crash if purple_normalize gets called with NULL str, see #10115 */
	g_return_val_if_fail(str != NULL, "");

	if (account != NULL)
	{
		PurpleProtocol *protocol =
				purple_protocols_find(purple_account_get_protocol_id(account));

		if(PURPLE_IS_PROTOCOL_CLIENT(protocol)) {
			ret = purple_protocol_client_normalize(PURPLE_PROTOCOL_CLIENT(protocol), account, str);
		}
	}

	if (ret == NULL)
	{
		char *tmp;

		tmp = g_utf8_normalize(str, -1, G_NORMALIZE_DEFAULT);
		g_snprintf(buf, sizeof(buf), "%s", tmp);
		g_free(tmp);

		ret = buf;
	}

	return ret;
}

/*
 * You probably don't want to call this directly, it is
 * mainly for use as a protocol callback function.  See the
 * comments in util.h.
 */
const char *
purple_normalize_nocase(const char *str)
{
	static char buf[BUF_LEN];
	char *tmp1, *tmp2;

	g_return_val_if_fail(str != NULL, NULL);

	tmp1 = g_utf8_strdown(str, -1);
	tmp2 = g_utf8_normalize(tmp1, -1, G_NORMALIZE_DEFAULT);
	g_snprintf(buf, sizeof(buf), "%s", tmp2 ? tmp2 : "");
	g_free(tmp2);
	g_free(tmp1);

	return buf;
}

gboolean
purple_validate(PurpleProtocol *protocol, const char *str)
{
	const char *normalized;

	g_return_val_if_fail(protocol != NULL, FALSE);
	g_return_val_if_fail(str != NULL, FALSE);

	if (str[0] == '\0')
		return FALSE;

	if (!PURPLE_PROTOCOL_IMPLEMENTS(protocol, CLIENT, normalize))
		return TRUE;

	normalized = purple_protocol_client_normalize(PURPLE_PROTOCOL_CLIENT(protocol),
	                                              NULL, str);

	return (NULL != normalized);
}

gchar *
purple_strdup_withhtml(const gchar *src)
{
	gulong destsize, i, j;
	gchar *dest;

	g_return_val_if_fail(src != NULL, NULL);

	/* New length is (length of src) + (number of \n's * 3) - (number of \r's) + 1 */
	destsize = 1;
	for (i = 0; src[i] != '\0'; i++)
	{
		if (src[i] == '\n')
			destsize += 4;
		else if (src[i] != '\r')
			destsize++;
	}

	dest = g_malloc(destsize);

	/* Copy stuff, ignoring \r's, because they are dumb */
	for (i = 0, j = 0; src[i] != '\0'; i++) {
		if (src[i] == '\n') {
			strcpy(&dest[j], "<BR>");
			j += 4;
		} else if (src[i] != '\r')
			dest[j++] = src[i];
	}

	dest[destsize-1] = '\0';

	return dest;
}

gboolean
purple_str_has_caseprefix(const gchar *s, const gchar *p)
{
	g_return_val_if_fail(s, FALSE);
	g_return_val_if_fail(p, FALSE);

	return (g_ascii_strncasecmp(s, p, strlen(p)) == 0);
}

char *
purple_str_add_cr(const char *text)
{
	char *ret = NULL;
	int count = 0, j;
	guint i;

	g_return_val_if_fail(text != NULL, NULL);

	if (text[0] == '\n')
		count++;
	for (i = 1; i < strlen(text); i++)
		if (text[i] == '\n' && text[i - 1] != '\r')
			count++;

	if (count == 0)
		return g_strdup(text);

	ret = g_malloc0(strlen(text) + count + 1);

	i = 0; j = 0;
	if (text[i] == '\n')
		ret[j++] = '\r';
	ret[j++] = text[i++];
	for (; i < strlen(text); i++) {
		if (text[i] == '\n' && text[i - 1] != '\r')
			ret[j++] = '\r';
		ret[j++] = text[i];
	}

	return ret;
}

void
purple_str_strip_char(char *text, char thechar)
{
	int i, j;

	g_return_if_fail(text != NULL);

	for (i = 0, j = 0; text[i]; i++)
		if (text[i] != thechar)
			text[j++] = text[i];

	text[j] = '\0';
}

void
purple_util_chrreplace(char *string, char delimiter,
					 char replacement)
{
	int i = 0;

	g_return_if_fail(string != NULL);

	while (string[i] != '\0')
	{
		if (string[i] == delimiter)
			string[i] = replacement;
		i++;
	}
}

gchar *
purple_strreplace(const char *string, const char *delimiter,
				const char *replacement)
{
	gchar **split;
	gchar *ret;

	g_return_val_if_fail(string      != NULL, NULL);
	g_return_val_if_fail(delimiter   != NULL, NULL);
	g_return_val_if_fail(replacement != NULL, NULL);

	split = g_strsplit(string, delimiter, 0);
	ret = g_strjoinv(replacement, split);
	g_strfreev(split);

	return ret;
}

gchar *
purple_strcasereplace(const char *string, const char *delimiter,
					const char *replacement)
{
	gchar *ret;
	int length_del, length_rep, i, j;

	g_return_val_if_fail(string      != NULL, NULL);
	g_return_val_if_fail(delimiter   != NULL, NULL);
	g_return_val_if_fail(replacement != NULL, NULL);

	length_del = strlen(delimiter);
	length_rep = strlen(replacement);

	/* Count how many times the delimiter appears */
	i = 0; /* position in the source string */
	j = 0; /* number of occurrences of "delimiter" */
	while (string[i] != '\0') {
		if (!g_ascii_strncasecmp(&string[i], delimiter, length_del)) {
			i += length_del;
			j += length_rep;
		} else {
			i++;
			j++;
		}
	}

	ret = g_malloc(j+1);

	i = 0; /* position in the source string */
	j = 0; /* position in the destination string */
	while (string[i] != '\0') {
		if (!g_ascii_strncasecmp(&string[i], delimiter, length_del)) {
			strncpy(&ret[j], replacement, length_rep);
			i += length_del;
			j += length_rep;
		} else {
			ret[j] = string[i];
			i++;
			j++;
		}
	}

	ret[j] = '\0';

	return ret;
}

/** TODO: Expose this when we can add API */
static const char *
purple_strcasestr_len(const char *haystack, gssize hlen, const char *needle, gssize nlen)
{
	const char *tmp, *ret;

	g_return_val_if_fail(haystack != NULL, NULL);
	g_return_val_if_fail(needle != NULL, NULL);

	if (hlen == -1)
		hlen = strlen(haystack);
	if (nlen == -1)
		nlen = strlen(needle);
	tmp = haystack,
	ret = NULL;

	g_return_val_if_fail(hlen > 0, NULL);
	g_return_val_if_fail(nlen > 0, NULL);

	while (*tmp && !ret && (hlen - (tmp - haystack)) >= nlen) {
		if (!g_ascii_strncasecmp(needle, tmp, nlen))
			ret = tmp;
		else
			tmp++;
	}

	return ret;
}

const char *
purple_strcasestr(const char *haystack, const char *needle)
{
	return purple_strcasestr_len(haystack, -1, needle, -1);
}

char *
purple_str_seconds_to_string(guint secs)
{
	char *ret = NULL;
	guint days, hrs, mins;

	if (secs < 60)
	{
		return g_strdup_printf(dngettext(PACKAGE, "%d second", "%d seconds", secs), secs);
	}

	days = secs / (60 * 60 * 24);
	secs = secs % (60 * 60 * 24);
	hrs  = secs / (60 * 60);
	secs = secs % (60 * 60);
	mins = secs / 60;
	/* secs = secs % 60; */

	if (days > 0)
	{
		ret = g_strdup_printf(dngettext(PACKAGE, "%d day", "%d days", days), days);
	}

	if (hrs > 0)
	{
		if (ret != NULL)
		{
			char *tmp = g_strdup_printf(
					dngettext(PACKAGE, "%s, %d hour", "%s, %d hours", hrs),
							ret, hrs);
			g_free(ret);
			ret = tmp;
		}
		else
			ret = g_strdup_printf(dngettext(PACKAGE, "%d hour", "%d hours", hrs), hrs);
	}

	if (mins > 0)
	{
		if (ret != NULL)
		{
			char *tmp = g_strdup_printf(
					dngettext(PACKAGE, "%s, %d minute", "%s, %d minutes", mins),
							ret, mins);
			g_free(ret);
			ret = tmp;
		}
		else
			ret = g_strdup_printf(dngettext(PACKAGE, "%d minute", "%d minutes", mins), mins);
	}

	return ret;
}

void
purple_str_wipe(gchar *str)
{
	if (str == NULL)
		return;
	memset(str, 0, strlen(str));
	g_free(str);
}

/**************************************************************************
 * URI/URL Functions
 **************************************************************************/

void purple_got_protocol_handler_uri(const char *uri)
{
	char proto[11];
	char delimiter;
	const char *tmp, *param_string;
	char *cmd;
	GHashTable *params = NULL;
	gsize len;
	if (!(tmp = strchr(uri, ':')) || tmp == uri) {
		purple_debug_error("util", "Malformed protocol handler message - missing protocol.\n");
		return;
	}

	len = MIN(sizeof(proto) - 1, (gsize)(tmp - uri));

	strncpy(proto, uri, len);
	proto[len] = '\0';

	tmp++;

	if (purple_strequal(proto, "xmpp"))
		delimiter = ';';
	else
		delimiter = '&';

	purple_debug_info("util", "Processing message '%s' for protocol '%s' using delimiter '%c'.\n", tmp, proto, delimiter);

	if ((param_string = strchr(tmp, '?'))) {
		const char *keyend = NULL, *pairstart;
		char *key, *value = NULL;

		cmd = g_strndup(tmp, (param_string - tmp));
		param_string++;

		params = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
		pairstart = tmp = param_string;

		while (*tmp || *pairstart) {
			if (*tmp == delimiter || !(*tmp)) {
				/* If there is no explicit value */
				if (keyend == NULL) {
					keyend = tmp;
				}
				/* without these brackets, clang won't
				 * recognize tmp as a non-NULL
				 */

				if (keyend && keyend != pairstart) {
					char *p;
					key = g_strndup(pairstart, (keyend - pairstart));
					/* If there is an explicit value */
					if (keyend != tmp && keyend != (tmp - 1))
						value = g_strndup(keyend + 1, (tmp - keyend - 1));
					for (p = key; *p; ++p)
						*p = g_ascii_tolower(*p);
					g_hash_table_insert(params, key, value);
				}
				keyend = value = NULL;
				pairstart = (*tmp) ? tmp + 1 : tmp;
			} else if (*tmp == '=')
				keyend = tmp;

			if (*tmp)
				tmp++;
		}
	} else
		cmd = g_strdup(tmp);

	purple_signal_emit_return_1(purple_get_core(), "uri-handler", proto, cmd, params);

	g_free(cmd);
	if (params)
		g_hash_table_destroy(params);
}

const char *
purple_url_decode(const char *str)
{
	static char buf[BUF_LEN];
	guint i, j = 0;
	char *bum;
	char hex[3];

	g_return_val_if_fail(str != NULL, NULL);

	/*
	 * XXX - This check could be removed and buf could be made
	 * dynamically allocated, but this is easier.
	 */
	if (strlen(str) >= BUF_LEN)
		return NULL;

	for (i = 0; i < strlen(str); i++) {

		if (str[i] != '%')
			buf[j++] = str[i];
		else {
			strncpy(hex, str + ++i, 2);
			hex[2] = '\0';

			/* i is pointing to the start of the number */
			i++;

			/*
			 * Now it's at the end and at the start of the for loop
			 * will be at the next character.
			 */
			buf[j++] = strtol(hex, NULL, 16);
		}
	}

	buf[j] = '\0';

	if (!g_utf8_validate(buf, -1, (const char **)&bum))
		*bum = '\0';

	return buf;
}

const char *
purple_url_encode(const char *str)
{
	const char *iter;
	static char buf[BUF_LEN];
	char utf_char[6];
	guint i, j = 0;

	g_return_val_if_fail(str != NULL, NULL);
	g_return_val_if_fail(g_utf8_validate(str, -1, NULL), NULL);

	iter = str;
	for (; *iter && j < (BUF_LEN - 1) ; iter = g_utf8_next_char(iter)) {
		gunichar c = g_utf8_get_char(iter);
		/* If the character is an ASCII character and is alphanumeric
		 * no need to escape */
		if (c < 128 && (isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~')) {
			buf[j++] = c;
		} else {
			int bytes = g_unichar_to_utf8(c, utf_char);
			for (i = 0; (int)i < bytes; i++) {
				if (j > (BUF_LEN - 4))
					break;
				if (i >= sizeof(utf_char)) {
					g_warn_if_reached();
					break;
				}
				sprintf(buf + j, "%%%02X", utf_char[i] & 0xff);
				j += 3;
			}
		}
	}

	buf[j] = '\0';

	return buf;
}

/* Originally lifted from
 * http://www.oreillynet.com/pub/a/network/excerpt/spcookbook_chap03/index3.html
 * ... and slightly modified to be a bit more rfc822 compliant
 * ... and modified a bit more to make domain checking rfc1035 compliant
 *     with the exception permitted in rfc1101 for domains to start with digit
 *     but not completely checking to avoid conflicts with IP addresses
 */
gboolean
purple_email_is_valid(const char *address)
{
	const char *c, *domain;
	static char *rfc822_specials = "()<>@,;:\\\"[]";

	g_return_val_if_fail(address != NULL, FALSE);

	if (*address == '.') return FALSE;

	/* first we validate the name portion (name@domain) (rfc822)*/
	for (c = address;  *c;  c++) {
		if (*c == '\"' && (c == address || *(c - 1) == '.' || *(c - 1) == '\"')) {
			while (*++c) {
				if (*c == '\\') {
					if (*c++ && *c < 127 && *c > 0 && *c != '\n' && *c != '\r') continue;
					else return FALSE;
				}
				if (*c == '\"') break;
				if (*c < ' ' || *c >= 127) return FALSE;
			}
			if (!*c++) return FALSE;
			if (*c == '@') break;
			if (*c != '.') return FALSE;
			continue;
		}
		if (*c == '@') break;
		if (*c <= ' ' || *c >= 127) return FALSE;
		if (strchr(rfc822_specials, *c)) return FALSE;
	}

	/* It's obviously not an email address if we didn't find an '@' above */
	if (*c == '\0') return FALSE;

	/* strictly we should return false if (*(c - 1) == '.') too, but I think
	 * we should permit user.@domain type addresses - they do work :) */
	if (c == address) return FALSE;

	/* next we validate the domain portion (name@domain) (rfc1035 & rfc1011) */
	if (!*(domain = ++c)) return FALSE;
	do {
		if (*c == '.' && (c == domain || *(c - 1) == '.' || *(c - 1) == '-'))
			return FALSE;
		if (*c == '-' && (*(c - 1) == '.' || *(c - 1) == '@')) return FALSE;
		if ((*c < '0' && *c != '-' && *c != '.') || (*c > '9' && *c < 'A') ||
			(*c > 'Z' && *c < 'a') || (*c > 'z')) return FALSE;
	} while (*++c);

	if (*(c - 1) == '-') return FALSE;

	return ((c - domain) > 3 ? TRUE : FALSE);
}

/* Stolen from gnome_uri_list_extract_uris */
GList *
purple_uri_list_extract_uris(const gchar *uri_list)
{
	const gchar *p, *q;
	gchar *retval;
	GList *result = NULL;

	g_return_val_if_fail (uri_list != NULL, NULL);

	p = uri_list;

	/* We don't actually try to validate the URI according to RFC
	* 2396, or even check for allowed characters - we just ignore
	* comments and trim whitespace off the ends.  We also
	* allow LF delimination as well as the specified CRLF.
	*/
	while (p) {
		if (*p != '#') {
			while (isspace(*p))
				p++;

			q = p;
			while (*q && (*q != '\n') && (*q != '\r'))
				q++;

			if (q > p) {
				q--;
				while (q > p && isspace(*q))
					q--;

				retval = (gchar*)g_malloc (q - p + 2);
				strncpy (retval, p, q - p + 1);
				retval[q - p + 1] = '\0';

				result = g_list_prepend (result, retval);
			}
		}
		p = strchr (p, '\n');
		if (p)
			p++;
	}

	return g_list_reverse (result);
}


/* Stolen from gnome_uri_list_extract_filenames */
GList *
purple_uri_list_extract_filenames(const gchar *uri_list)
{
	GList *tmp_list, *node, *result;

	g_return_val_if_fail (uri_list != NULL, NULL);

	result = purple_uri_list_extract_uris(uri_list);

	tmp_list = result;
	while (tmp_list) {
		gchar *s = (gchar*)tmp_list->data;

		node = tmp_list;
		tmp_list = tmp_list->next;

		if (!strncmp (s, "file:", 5)) {
			node->data = g_filename_from_uri (s, NULL, NULL);
			/* not sure if this fallback is useful at all */
			if (!node->data) node->data = g_strdup (s+5);
		} else {
			result = g_list_delete_link(result, node);
		}
		g_free (s);
	}
	return result;
}

char *
purple_uri_escape_for_open(const char *unescaped)
{
	/* Replace some special characters like $ with their percent-encoded value.
	 * This shouldn't be necessary because we shell-escape the entire arg before
	 * exec'ing the browser, however, we had a report that a URL containing
	 * $(xterm) was causing xterm to start on his system. This is obviously a
	 * bug on his system, but it's pretty easy for us to protect against it. */
	return g_uri_escape_string(unescaped, "[]:;/%#,+?=&@", FALSE);
}

/**************************************************************************
 * UTF8 String Functions
 **************************************************************************/
gchar *
purple_utf8_try_convert(const char *str)
{
	gsize converted;
	gchar *utf8;

	g_return_val_if_fail(str != NULL, NULL);

	if (g_utf8_validate(str, -1, NULL)) {
		return g_strdup(str);
	}

	utf8 = g_locale_to_utf8(str, -1, &converted, NULL, NULL);
	if (utf8 != NULL)
		return utf8;

	utf8 = g_convert(str, -1, "UTF-8", "ISO-8859-15", &converted, NULL, NULL);
	if ((utf8 != NULL) && (converted == strlen(str)))
		return utf8;

	g_free(utf8);

	return NULL;
}

gchar *
purple_utf8_strip_unprintables(const gchar *str)
{
	gchar *workstr, *iter;
	const gchar *bad;

	if (str == NULL)
		/* Act like g_strdup */
		return NULL;

	if (!g_utf8_validate(str, -1, &bad)) {
		purple_debug_error("util", "purple_utf8_strip_unprintables(%s) failed; "
		                           "first bad character was %02x (%c)\n",
		                   str, *bad, *bad);
		g_return_val_if_reached(NULL);
	}

	workstr = iter = g_new(gchar, strlen(str) + 1);
	while (*str) {
		gunichar ch = g_utf8_get_char(str);
		gchar *next = g_utf8_next_char(str);
		/*
		 * Char ::= #x9 | #xA | #xD | [#x20-#xD7FF] | [#xE000-#xFFFD] |
		 *          [#x10000-#x10FFFF]
		 */
		if ((ch == '\t' || ch == '\n' || ch == '\r') ||
				(ch >= 0x20 && ch <= 0xD7FF) ||
				(ch >= 0xE000 && ch <= 0xFFFD) ||
				(ch >= 0x10000 && ch <= 0x10FFFF)) {
			memcpy(iter, str, next - str);
			iter += (next - str);
		}

		str = next;
	}

	/* nul-terminate the new string */
	*iter = '\0';

	return workstr;
}

/*
 * This function is copied from g_strerror() but changed to use
 * gai_strerror().
 */
const gchar *
purple_gai_strerror(gint errnum)
{
	static GPrivate msg_private = G_PRIVATE_INIT(g_free);
	char *msg;
	int saved_errno = errno;

	const char *msg_locale;

	msg_locale = gai_strerror(errnum);
	if (g_get_charset(NULL))
	{
		/* This string is already UTF-8--great! */
		errno = saved_errno;
		return msg_locale;
	}
	else
	{
		gchar *msg_utf8 = g_locale_to_utf8(msg_locale, -1, NULL, NULL, NULL);
		if (msg_utf8)
		{
			/* Stick in the quark table so that we can return a static result */
			GQuark msg_quark = g_quark_from_string(msg_utf8);
			g_free(msg_utf8);

			msg_utf8 = (gchar *)g_quark_to_string(msg_quark);
			errno = saved_errno;
			return msg_utf8;
		}
	}

	msg = g_private_get(&msg_private);

	if (!msg)
	{
		msg = g_new(gchar, 64);
		g_private_set(&msg_private, msg);
	}

	sprintf(msg, "unknown error (%d)", errnum);

	errno = saved_errno;
	return msg;
}

char *
purple_utf8_ncr_encode(const char *str)
{
	GString *out;

	g_return_val_if_fail(str != NULL, NULL);
	g_return_val_if_fail(g_utf8_validate(str, -1, NULL), NULL);

	out = g_string_new("");

	for(; *str; str = g_utf8_next_char(str)) {
		gunichar wc = g_utf8_get_char(str);

		/* super simple check. hopefully not too wrong. */
		if(wc >= 0x80) {
			g_string_append_printf(out, "&#%u;", (guint32) wc);
		} else {
			g_string_append_unichar(out, wc);
		}
	}

	return g_string_free(out, FALSE);
}


char *
purple_utf8_ncr_decode(const char *str)
{
	GString *out;
	char *buf, *b;

	g_return_val_if_fail(str != NULL, NULL);
	g_return_val_if_fail(g_utf8_validate(str, -1, NULL), NULL);

	buf = (char *) str;
	out = g_string_new("");

	while( (b = strstr(buf, "&#")) ) {
		gunichar wc;
		int base = 0;

		/* append everything leading up to the &# */
		g_string_append_len(out, buf, b-buf);

		b += 2; /* skip past the &# */

		/* strtoul will treat 0x prefix as hex, but not just x */
		if(*b == 'x' || *b == 'X') {
			base = 16;
			b++;
		}

		/* advances buf to the end of the ncr segment */
		wc = (gunichar) strtoul(b, &buf, base);

		/* this mimics the previous impl of ncr_decode */
		if(*buf == ';') {
			g_string_append_unichar(out, wc);
			buf++;
		}
	}

	/* append whatever's left */
	g_string_append(out, buf);

	return g_string_free(out, FALSE);
}


int
purple_utf8_strcasecmp(const char *a, const char *b)
{
	char *a_norm = NULL;
	char *b_norm = NULL;
	int ret = -1;

	if(!a && b)
		return -1;
	else if(!b && a)
		return 1;
	else if(!a && !b)
		return 0;

	if(!g_utf8_validate(a, -1, NULL) || !g_utf8_validate(b, -1, NULL))
	{
		purple_debug_error("purple_utf8_strcasecmp",
						 "One or both parameters are invalid UTF8\n");
		return ret;
	}

	a_norm = g_utf8_casefold(a, -1);
	b_norm = g_utf8_casefold(b, -1);
	ret = g_utf8_collate(a_norm, b_norm);
	g_free(a_norm);
	g_free(b_norm);

	return ret;
}

/* previously conversation::find_nick() */
gboolean
purple_utf8_has_word(const char *haystack, const char *needle)
{
	char *hay, *pin, *p;
	const char *start, *prev_char;
	gunichar before, after;
	int n;
	gboolean ret = FALSE;

	start = hay = g_utf8_strdown(haystack, -1);

	pin = g_utf8_strdown(needle, -1);
	n = strlen(pin);

	while ((p = strstr(start, pin)) != NULL) {
		prev_char = g_utf8_find_prev_char(hay, p);
		before = -2;
		if (prev_char) {
			before = g_utf8_get_char(prev_char);
		}
		after = g_utf8_get_char_validated(p + n, - 1);

		if ((p == hay ||
				/* The character before is a reasonable guess for a word boundary
				   ("!g_unichar_isalnum()" is not a valid way to determine word
				    boundaries, but it is the only reasonable thing to do here),
				   and isn't the '&' from a "&amp;" or some such entity*/
				(before != (gunichar)-2 && !g_unichar_isalnum(before) && *(p - 1) != '&'))
				&& after != (gunichar)-2 && !g_unichar_isalnum(after)) {
			ret = TRUE;
			break;
		}
		start = p + 1;
	}

	g_free(pin);
	g_free(hay);

	return ret;
}

gboolean purple_message_meify(char *message, gssize len)
{
	char *c;
	gboolean inside_html = FALSE;

	g_return_val_if_fail(message != NULL, FALSE);

	if(len == -1)
		len = strlen(message);

	for (c = message; *c; c++, len--) {
		if(inside_html) {
			if(*c == '>')
				inside_html = FALSE;
		} else {
			if(*c == '<')
				inside_html = TRUE;
			else
				break;
		}
	}

	if(*c && !g_ascii_strncasecmp(c, "/me ", 4)) {
		memmove(c, c+4, len-3);
		return TRUE;
	}

	return FALSE;
}

char *purple_text_strip_mnemonic(const char *in)
{
	char *out;
	char *a;
	char *a0;
	const char *b;

	g_return_val_if_fail(in != NULL, NULL);

	out = g_malloc(strlen(in)+1);
	a = out;
	b = in;

	a0 = a; /* The last non-space char seen so far, or the first char */

	while(*b) {
		if(*b == '_') {
			if(a > out && b > in && *(b-1) == '(' && *(b+1) && !(*(b+1) & 0x80) && *(b+2) == ')') {
				/* Detected CJK style shortcut (Bug 875311) */
				a = a0;	/* undo the left parenthesis */
				b += 3;	/* and skip the whole mess */
			} else if(*(b+1) == '_') {
				*(a++) = '_';
				b += 2;
				a0 = a;
			} else {
				b++;
			}
		/* We don't want to corrupt the middle of UTF-8 characters */
		} else if (!(*b & 0x80)) {	/* other 1-byte char */
			if (*b != ' ')
				a0 = a;
			*(a++) = *(b++);
		} else {
			/* Multibyte utf8 char, don't look for _ inside these */
			int n = 0;
			int i;
			if ((*b & 0xe0) == 0xc0) {
				n = 2;
			} else if ((*b & 0xf0) == 0xe0) {
				n = 3;
			} else if ((*b & 0xf8) == 0xf0) {
				n = 4;
			} else if ((*b & 0xfc) == 0xf8) {
				n = 5;
			} else if ((*b & 0xfe) == 0xfc) {
				n = 6;
			} else {		/* Illegal utf8 */
				n = 1;
			}
			a0 = a; /* unless we want to delete CJK spaces too */
			for (i = 0; i < n && *b; i += 1) {
				*(a++) = *(b++);
			}
		}
	}
	*a = '\0';

	return out;
}

const char* purple_unescape_filename(const char *escaped) {
	return purple_url_decode(escaped);
}


/* this is almost identical to purple_url_encode (hence purple_url_decode
 * being used above), but we want to keep certain characters unescaped
 * for compat reasons */
const char *
purple_escape_filename(const char *str)
{
	const char *iter;
	static char buf[BUF_LEN];
	char utf_char[6];
	guint i, j = 0;

	g_return_val_if_fail(str != NULL, NULL);
	g_return_val_if_fail(g_utf8_validate(str, -1, NULL), NULL);

	iter = str;
	for (; *iter && j < (BUF_LEN - 1) ; iter = g_utf8_next_char(iter)) {
		gunichar c = g_utf8_get_char(iter);
		/* If the character is an ASCII character and is alphanumeric,
		 * or one of the specified values, no need to escape */
		if (c < 128 && (g_ascii_isalnum(c) || c == '@' || c == '-' ||
				c == '_' || c == '.' || c == '#')) {
			buf[j++] = c;
		} else {
			int bytes = g_unichar_to_utf8(c, utf_char);
			for (i = 0; (int)i < bytes; i++) {
				if (j > (BUF_LEN - 4))
					break;
				if (i >= sizeof(utf_char)) {
					g_warn_if_reached();
					break;
				}
				sprintf(buf + j, "%%%02x", utf_char[i] & 0xff);
				j += 3;
			}
		}
	}
#ifdef _WIN32
	/* File/Directory names in windows cannot end in periods/spaces.
	 * http://msdn.microsoft.com/en-us/library/aa365247%28VS.85%29.aspx
	 */
	while (j > 0 && (buf[j - 1] == '.' || buf[j - 1] == ' '))
		j--;
#endif
	buf[j] = '\0';

	return buf;
}

static void
set_status_with_attrs(PurpleStatus *status, ...)
{
	va_list args;
	va_start(args, status);
	purple_status_set_active_with_attrs(status, TRUE, args);
	va_end(args);
}

void purple_util_set_current_song(const char *title, const char *artist, const char *album)
{
	GList *list = purple_accounts_get_all();
	for (; list; list = list->next) {
		PurplePresence *presence;
		PurpleStatus *tune;
		PurpleAccount *account = list->data;
		if (!purple_account_get_enabled(account, purple_core_get_ui()))
			continue;

		presence = purple_account_get_presence(account);
		tune = purple_presence_get_status(presence, "tune");
		if (!tune)
			continue;
		if (title) {
			set_status_with_attrs(tune,
					PURPLE_TUNE_TITLE, title,
					PURPLE_TUNE_ARTIST, artist,
					PURPLE_TUNE_ALBUM, album,
					NULL);
		} else {
			purple_status_set_active(tune, FALSE);
		}
	}
}

char * purple_util_format_song_info(const char *title, const char *artist, const char *album, gpointer unused)
{
	GString *string;
	char *esc;

	if (!title || !*title)
		return NULL;

	esc = g_markup_escape_text(title, -1);
	string = g_string_new("");
	g_string_append_printf(string, "%s", esc);
	g_free(esc);

	if (artist && *artist) {
		esc = g_markup_escape_text(artist, -1);
		g_string_append_printf(string, _(" - %s"), esc);
		g_free(esc);
	}

	if (album && *album) {
		esc = g_markup_escape_text(album, -1);
		g_string_append_printf(string, _(" (%s)"), esc);
		g_free(esc);
	}

	return g_string_free(string, FALSE);
}

GValue *
purple_value_new(GType type)
{
	GValue *ret;

	g_return_val_if_fail(type != G_TYPE_NONE, NULL);

	ret = g_new0(GValue, 1);
	g_value_init(ret, type);

	return ret;
}

GValue *
purple_value_dup(GValue *value)
{
	GValue *ret;

	g_return_val_if_fail(value != NULL, NULL);

	ret = g_new0(GValue, 1);
	g_value_init(ret, G_VALUE_TYPE(value));
	g_value_copy(value, ret);

	return ret;
}

void
purple_value_free(GValue *value)
{
	g_return_if_fail(value != NULL);

	g_value_unset(value);
	g_free(value);
}
