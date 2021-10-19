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
	GLogLevelFlags log_level = G_LOG_LEVEL_DEBUG;
	gchar *msg = NULL;

	g_return_if_fail(format != NULL);

	/* GLib's debug levels are not quite the same as ours, so we need to
	 * re-assign them. */
	switch(level) {
		case PURPLE_DEBUG_MISC:
			log_level = G_LOG_LEVEL_INFO;
			break;
		case PURPLE_DEBUG_INFO:
			log_level = G_LOG_LEVEL_MESSAGE;
			break;
		case PURPLE_DEBUG_WARNING:
			log_level = G_LOG_LEVEL_WARNING;
			break;
		case PURPLE_DEBUG_ERROR:
			log_level = G_LOG_LEVEL_CRITICAL;
			break;
		case PURPLE_DEBUG_FATAL:
			log_level = G_LOG_LEVEL_ERROR;
			break;
		default:
			g_return_if_reached();
	}

	/* strip trailing linefeeds */
	msg = g_strdup(format);
	g_strchomp(msg);

	g_logv(category, log_level, msg, args);
	g_free(msg);
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
