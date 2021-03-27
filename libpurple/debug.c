/*
 * Purple - Internet Messaging Library
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

#include "debug.h"
#include "prefs.h"

static PurpleDebugUi *debug_ui = NULL;

/*
 * This determines whether debug info should be written to the
 * console or not.
 *
 * It doesn't make sense to make this a normal Purple preference
 * because it's a command line option.  This will always be FALSE,
 * unless the user explicitly started the UI with the -d flag.
 * It doesn't matter what this value was the last time Purple was
 * started, so it doesn't make sense to save it in prefs.
 */
static gboolean debug_enabled = FALSE;

/*
 * These determine whether verbose or unsafe debugging are desired.  I
 * don't want to make these purple preferences because their values should
 * not be remembered across instances of the UI.
 */
static gboolean debug_verbose = FALSE;
static gboolean debug_unsafe = FALSE;

static gboolean debug_colored = FALSE;

static void
purple_debug_vargs(PurpleDebugLevel level, const gchar *category,
                   const gchar *format, va_list args)
{
	PurpleDebugUi *ui;
	gchar *arg_s = NULL;

	if(!debug_enabled) {
		return;
	}

	g_return_if_fail(level != PURPLE_DEBUG_ALL);
	g_return_if_fail(format != NULL);

	ui = purple_debug_get_ui();
	if(!ui) {
		return;
	}

	if(!purple_debug_ui_is_enabled(ui, level, category)) {
		return;
	}

	arg_s = g_strdup_vprintf(format, args);
	g_strchomp(arg_s); /* strip trailing linefeeds */

	if(debug_enabled) {
		GDateTime *now = NULL;
		gchar *ts_s;
		const gchar *format_pre, *format_post;

		format_pre = "";
		format_post = "";

		if(!debug_colored) {
			format_pre = "";
		} else if(level == PURPLE_DEBUG_MISC) {
			format_pre = "\033[0;37m";
		} else if(level == PURPLE_DEBUG_INFO) {
			format_pre = "";
		} else if(level == PURPLE_DEBUG_WARNING) {
			format_pre = "\033[0;33m";
		} else if(level == PURPLE_DEBUG_ERROR) {
			format_pre = "\033[1;31m";
		} else if(level == PURPLE_DEBUG_FATAL) {
			format_pre = "\033[1;33;41m";
		}

		if(format_pre[0] != '\0') {
			format_post = "\033[0m";
		}

		now = g_date_time_new_now_local();
		ts_s = g_date_time_format(now, "(%H:%M:%S)");
		g_date_time_unref(now);

		if(category == NULL) {
			g_print("%s%s %s%s\n", format_pre, ts_s, arg_s, format_post);
		} else {
			g_print("%s%s %s: %s%s\n", format_pre, ts_s, category, arg_s,
			        format_post);
		}

		g_free(ts_s);
	}

	purple_debug_ui_print(ui, level, category, arg_s);

	g_free(arg_s);
}

void
purple_debug(PurpleDebugLevel level, const gchar *category,
             const gchar *format, ...)
{
	va_list args;

	g_return_if_fail(level != PURPLE_DEBUG_ALL);
	g_return_if_fail(format != NULL);

	va_start(args, format);
	purple_debug_vargs(level, category, format, args);
	va_end(args);
}

void
purple_debug_misc(const gchar *category, const gchar *format, ...) {
	va_list args;

	g_return_if_fail(format != NULL);

	va_start(args, format);
	purple_debug_vargs(PURPLE_DEBUG_MISC, category, format, args);
	va_end(args);
}

void
purple_debug_info(const gchar *category, const gchar *format, ...) {
	va_list args;

	g_return_if_fail(format != NULL);

	va_start(args, format);
	purple_debug_vargs(PURPLE_DEBUG_INFO, category, format, args);
	va_end(args);
}

void
purple_debug_warning(const gchar *category, const gchar *format, ...) {
	va_list args;

	g_return_if_fail(format != NULL);

	va_start(args, format);
	purple_debug_vargs(PURPLE_DEBUG_WARNING, category, format, args);
	va_end(args);
}

void
purple_debug_error(const gchar *category, const gchar *format, ...) {
	va_list args;

	g_return_if_fail(format != NULL);

	va_start(args, format);
	purple_debug_vargs(PURPLE_DEBUG_ERROR, category, format, args);
	va_end(args);
}

void
purple_debug_fatal(const gchar *category, const gchar *format, ...) {
	va_list args;

	g_return_if_fail(format != NULL);

	va_start(args, format);
	purple_debug_vargs(PURPLE_DEBUG_FATAL, category, format, args);
	va_end(args);
}

void
purple_debug_set_enabled(gboolean enabled) {
	debug_enabled = enabled;
}

gboolean
purple_debug_is_enabled() {
	return debug_enabled;
}

void
purple_debug_set_ui(PurpleDebugUi *ui) {
	g_set_object(&debug_ui, ui);
}

gboolean
purple_debug_is_verbose() {
	return debug_verbose;
}

void
purple_debug_set_verbose(gboolean verbose) {
	debug_verbose = verbose;
}

gboolean
purple_debug_is_unsafe() {
	return debug_unsafe;
}

void
purple_debug_set_unsafe(gboolean unsafe) {
	debug_unsafe = unsafe;
}

void
purple_debug_set_colored(gboolean colored) {
	debug_colored = colored;
}

PurpleDebugUi *
purple_debug_get_ui(void) {
	return debug_ui;
}

gboolean
purple_debug_ui_is_enabled(PurpleDebugUi *ui, PurpleDebugLevel level,
                           const gchar *category)
{
	PurpleDebugUiInterface *iface = NULL;

	g_return_val_if_fail(PURPLE_IS_DEBUG_UI(ui), FALSE);

	iface = PURPLE_DEBUG_UI_GET_IFACE(ui);
	if(iface != NULL && iface->is_enabled != NULL) {
		return iface->is_enabled(ui, level, category);
	}

	return FALSE;
}

void
purple_debug_ui_print(PurpleDebugUi *ui, PurpleDebugLevel level,
                      const gchar *category, const gchar *arg_s)
{
	PurpleDebugUiInterface *iface = NULL;

	g_return_if_fail(PURPLE_IS_DEBUG_UI(ui));

	if(!purple_debug_ui_is_enabled(ui, level, category)) {
		return;
	}

	iface = PURPLE_DEBUG_UI_GET_IFACE(ui);
	if(iface != NULL && iface->print != NULL) {
		iface->print(ui, level, category, arg_s);
	}
}

G_DEFINE_INTERFACE(PurpleDebugUi, purple_debug_ui, G_TYPE_OBJECT);

static void
purple_debug_ui_default_init(PurpleDebugUiInterface *iface) {
}

void
purple_debug_init(void) {
	/* Read environment variables once per init */
	if(g_getenv("PURPLE_UNSAFE_DEBUG")) {
		purple_debug_set_unsafe(TRUE);
	}

	if(g_getenv("PURPLE_VERBOSE_DEBUG")) {
		purple_debug_set_verbose(TRUE);
	}

	purple_prefs_add_none("/purple/debug");
}
