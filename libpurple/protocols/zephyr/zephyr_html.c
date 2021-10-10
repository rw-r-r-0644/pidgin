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

#include <ctype.h>

#include <purple.h>

#include "zephyr_html.h"

typedef struct _zframe zframe;

struct _zframe {
	/* common part */
	/* true for everything but @color, since inside the parens of that one is
	 * the color. */
	gboolean has_closer;
	/* </i>, </font>, </b>, etc. */
	const char *closing;
	/* text including the opening html thingie. */
	GString *text;

	/* html_to_zephyr */
	/* @i, @b, etc. */
	const char *env;
	/* }=1, ]=2, )=4, >=8 */
	int closer_mask;
	/* href for links */
	gboolean is_href;
	GString *href;

	/* zephyr_to_html */
	/* }, ], ), > */
	char *closer;
};

static zframe *
zframe_new_with_text(const gchar *text, const gchar *closing, gboolean has_closer)
{
	zframe *frame = g_new(zframe, 1);

	frame->text = g_string_new(text);
	frame->closing = closing;
	frame->has_closer = has_closer;

	return frame;
}

static inline zframe *
zframe_new(const gchar *closing, gboolean has_closer)
{
	return zframe_new_with_text("", closing, has_closer);
}

static gboolean
zframe_href_has_prefix(const zframe *frame, const gchar *prefix)
{
	gsize prefix_len = strlen(prefix);

	return (frame->href->len == (prefix_len + frame->text->len)) &&
	       !strncmp(frame->href->str, prefix, prefix_len) &&
	       purple_strequal(frame->href->str + prefix_len, frame->text->str);
}

static gsize
html_to_zephyr_pop(GQueue *frames)
{
	zframe *popped = (zframe *)g_queue_pop_head(frames);
	zframe *head = (zframe *)g_queue_peek_head(frames);
	gsize result = strlen(popped->closing);

	if (popped->is_href) {
		head->href = popped->text;
	} else {
		g_string_append(head->text, popped->env);
		if (popped->has_closer) {
			g_string_append_c(head->text,
			                  (popped->closer_mask & 1) ? '{' :
			                  (popped->closer_mask & 2) ? '[' :
			                  (popped->closer_mask & 4) ? '(' :
			                  '<');
		}
		g_string_append(head->text, popped->text->str);
		if (popped->href) {
			if (!purple_strequal(popped->href->str, popped->text->str) &&
			    !zframe_href_has_prefix(popped, "http://") &&
			    !zframe_href_has_prefix(popped, "mailto:")) {
				g_string_append(head->text, " <");
				g_string_append(head->text, popped->href->str);
				if (popped->closer_mask & ~8) {
					g_string_append_c(head->text, '>');
					popped->closer_mask &= ~8;
				} else {
					g_string_append(head->text, "@{>}");
				}
			}
			g_string_free(popped->href, TRUE);
		}
		if (popped->has_closer) {
			g_string_append_c(head->text,
			                  (popped->closer_mask & 1) ? '}' :
			                  (popped->closer_mask & 2) ? ']' :
			                  (popped->closer_mask & 4) ? ')' :
			                  '>');
		}
		if (!popped->has_closer) {
			head->closer_mask = popped->closer_mask;
		}
		g_string_free(popped->text, TRUE);
	}
	g_free(popped);
	return result;
}

char *
html_to_zephyr(const char *message)
{
	GQueue frames = G_QUEUE_INIT;
	zframe *frame, *new_f;
	char *ret;

	if (*message == '\0')
		return g_strdup("");

	frame = zframe_new(NULL, FALSE);
	frame->href = NULL;
	frame->is_href = FALSE;
	frame->env = "";
	frame->closer_mask = 15;

	g_queue_push_head(&frames, frame);

	purple_debug_info("zephyr", "html received %s\n", message);
	while (*message) {
		frame = (zframe *)g_queue_peek_head(&frames);
		if (frame->closing && purple_str_has_caseprefix(message, frame->closing)) {
			message += html_to_zephyr_pop(&frames);
		} else if (*message == '<') {
			if (!g_ascii_strncasecmp(message + 1, "i>", 2)) {
				new_f = zframe_new("</i>", TRUE);
				new_f->href = NULL;
				new_f->is_href = FALSE;
				new_f->env = "@i";
				new_f->closer_mask = 15;
				g_queue_push_head(&frames, new_f);
				message += 3;
			} else if (!g_ascii_strncasecmp(message + 1, "b>", 2)) {
				new_f = zframe_new("</b>", TRUE);
				new_f->href = NULL;
				new_f->is_href = FALSE;
				new_f->env = "@b";
				new_f->closer_mask = 15;
				g_queue_push_head(&frames, new_f);
				message += 3;
			} else if (!g_ascii_strncasecmp(message + 1, "br>", 3)) {
				g_string_append_c(frame->text, '\n');
				message += 4;
			} else if (!g_ascii_strncasecmp(message + 1, "a href=\"", 8)) {
				message += 9;
				new_f = zframe_new("</a>", FALSE);
				new_f->href = NULL;
				new_f->is_href = FALSE;
				new_f->env = "";
				new_f->closer_mask = frame->closer_mask;
				g_queue_push_head(&frames, new_f);
				new_f = zframe_new("\">", FALSE);
				new_f->href = NULL;
				new_f->is_href = TRUE;
				new_f->closer_mask = frame->closer_mask;
				g_queue_push_head(&frames, new_f);
			} else if (!g_ascii_strncasecmp(message + 1, "font", 4)) {
				new_f = zframe_new("</font>", TRUE);
				new_f->href = NULL;
				new_f->is_href = FALSE;
				new_f->closer_mask = 15;
				g_queue_push_head(&frames, new_f);
				message += 5;
				while (*message == ' ') {
					message++;
				}
				if (!g_ascii_strncasecmp(message, "color=\"", 7)) {
					message += 7;
					new_f->env = "@";
					new_f = zframe_new("\">", TRUE);
					new_f->env = "@color";
					new_f->href = NULL;
					new_f->is_href = FALSE;
					new_f->closer_mask = 15;
					g_queue_push_head(&frames, new_f);
				} else if (!g_ascii_strncasecmp(message, "face=\"", 6)) {
					message += 6;
					new_f->env = "@";
					new_f = zframe_new("\">", TRUE);
					new_f->env = "@font";
					new_f->href = NULL;
					new_f->is_href = FALSE;
					new_f->closer_mask = 15;
					g_queue_push_head(&frames, new_f);
				} else if (!g_ascii_strncasecmp(message, "size=\"", 6)) {
					message += 6;
					if ((*message == '1') || (*message == '2')) {
						new_f->env = "@small";
					} else if ((*message == '3') || (*message == '4')) {
						new_f->env = "@medium";
					} else if ((*message == '5') || (*message == '6')
					           || (*message == '7')) {
						new_f->env = "@large";
					} else {
						new_f->env = "";
						new_f->has_closer = FALSE;
						new_f->closer_mask = frame->closer_mask;
					}
					message += 3;
				} else {
					/* Drop all unrecognized/misparsed font tags */
					new_f->env = "";
					new_f->has_closer = FALSE;
					new_f->closer_mask = frame->closer_mask;
					while (g_ascii_strncasecmp(message, "\">", 2) != 0) {
						message++;
					}
					if (*message != '\0') {
						message += 2;
					}
				}
			} else {
				/* Catch all for all unrecognized/misparsed <foo> tags */
				g_string_append_c(frame->text, *message++);
			}
		} else if (*message == '@') {
			g_string_append(frame->text, "@@");
			message++;
		} else if (*message == '}') {
			if (frame->closer_mask & ~1) {
				frame->closer_mask &= ~1;
				g_string_append_c(frame->text, *message++);
			} else {
				g_string_append(frame->text, "@[}]");
				message++;
			}
		} else if (*message == ']') {
			if (frame->closer_mask & ~2) {
				frame->closer_mask &= ~2;
				g_string_append_c(frame->text, *message++);
			} else {
				g_string_append(frame->text, "@{]}");
				message++;
			}
		} else if (*message == ')') {
			if (frame->closer_mask & ~4) {
				frame->closer_mask &= ~4;
				g_string_append_c(frame->text, *message++);
			} else {
				g_string_append(frame->text, "@{)}");
				message++;
			}
		} else if (!g_ascii_strncasecmp(message, "&gt;", 4)) {
			if (frame->closer_mask & ~8) {
				frame->closer_mask &= ~8;
				g_string_append_c(frame->text, *message++);
			} else {
				g_string_append(frame->text, "@{>}");
				message += 4;
			}
		} else {
			g_string_append_c(frame->text, *message++);
		}
	}
	frame = (zframe *)g_queue_pop_head(&frames);
	ret = g_string_free(frame->text, FALSE);
	g_free(frame);
	purple_debug_info("zephyr", "zephyr outputted  %s\n", ret);
	return ret;
}

static void
zephyr_to_html_pop(GQueue *frames, gboolean *last_had_closer)
{
	zframe *popped = (zframe *)g_queue_pop_head(frames);
	zframe *head = (zframe *)g_queue_peek_head(frames);

	g_string_append(head->text, popped->text->str);
	g_string_append(head->text, popped->closing);

	if (last_had_closer != NULL) {
		*last_had_closer = popped->has_closer;
	}

	g_string_free(popped->text, TRUE);
	g_free(popped);
}

char *
zephyr_to_html(const char *message)
{
	GQueue frames = G_QUEUE_INIT;
	zframe *frame;
	char *ret;

	frame = zframe_new("", FALSE);
	frame->closer = NULL;

	g_queue_push_head(&frames, frame);

	while (*message) {
		frame = (zframe *)g_queue_peek_head(&frames);
		if (*message == '@' && message[1] == '@') {
			g_string_append(frame->text, "@");
			message += 2;
		} else if (*message == '@') {
			int end = 1;
			while (message[end] && (isalnum(message[end]) || message[end] == '_')) {
				end++;
			}
			if (message[end] &&
			    (message[end] == '{' || message[end] == '[' || message[end] == '(' ||
			     !g_ascii_strncasecmp(message + end, "&lt;", 4))) {
				zframe *new_f;
				char *buf;
				char *closer;
				buf = g_new0(char, end);
				g_snprintf(buf, end, "%s", message + 1);
				message += end;
				closer = (*message == '{' ? "}" :
				          *message == '[' ? "]" :
				          *message == '(' ? ")" :
				          "&gt;");
				message += (*message == '&' ? 4 : 1);
				if (!g_ascii_strcasecmp(buf, "italic") || !g_ascii_strcasecmp(buf, "i")) {
					new_f = zframe_new_with_text("<i>", "</i>", TRUE);
				} else if (!g_ascii_strcasecmp(buf, "small")) {
					new_f = zframe_new_with_text("<font size=\"1\">", "</font>", TRUE);
				} else if (!g_ascii_strcasecmp(buf, "medium")) {
					new_f = zframe_new_with_text("<font size=\"3\">", "</font>", TRUE);
				} else if (!g_ascii_strcasecmp(buf, "large")) {
					new_f = zframe_new_with_text("<font size=\"7\">", "</font>", TRUE);
				} else if (!g_ascii_strcasecmp(buf, "bold")
				           || !g_ascii_strcasecmp(buf, "b")) {
					new_f = zframe_new_with_text("<b>", "</b>", TRUE);
				} else if (!g_ascii_strcasecmp(buf, "font")) {
					zframe *extra_f;
					extra_f = zframe_new("</font>", FALSE);
					extra_f->closer = frame->closer;
					g_queue_push_head(&frames, extra_f);
					new_f = zframe_new_with_text("<font face=\"", "\">", TRUE);
				} else if (!g_ascii_strcasecmp(buf, "color")) {
					zframe *extra_f;
					extra_f = zframe_new("</font>", FALSE);
					extra_f->closer = frame->closer;
					g_queue_push_head(&frames, extra_f);
					new_f = zframe_new_with_text("<font color=\"", "\">", TRUE);
				} else {
					new_f = zframe_new("", TRUE);
				}
				new_f->closer = closer;
				g_queue_push_head(&frames, new_f);
				g_free(buf);
			} else {
				/* Not a formatting tag, add the character as normal. */
				g_string_append_c(frame->text, *message++);
			}
		} else if (frame->closer && purple_str_has_caseprefix(message, frame->closer)) {
			message += strlen(frame->closer);
			if (g_queue_get_length(&frames) > 1) {
				gboolean last_had_closer;

				do {
					zephyr_to_html_pop(&frames, &last_had_closer);
				} while (g_queue_get_length(&frames) > 1 && !last_had_closer);
			} else {
				g_string_append_c(frame->text, *message);
			}
		} else if (*message == '\n') {
			g_string_append(frame->text, "<br>");
			message++;
		} else {
			g_string_append_c(frame->text, *message++);
		}
	}
	/* go through all the stuff that they didn't close */
	while (g_queue_get_length(&frames) > 1) {
		zephyr_to_html_pop(&frames, NULL);
	}
	frame = (zframe *)g_queue_pop_head(&frames);
	ret = g_string_free(frame->text, FALSE);
	g_free(frame);
	return ret;
}
