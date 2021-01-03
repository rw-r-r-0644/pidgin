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

#ifndef PURPLE_ZEPHYR_ZEPHYR_HTML_H
#define PURPLE_ZEPHYR_ZEPHYR_HTML_H

/* This parses HTML formatting (put out by one of the gtkimhtml widgets
   And converts it to zephyr formatting.
   It currently deals properly with <b>, <br>, <i>, <font face=...>, <font color=...>,
   It ignores <font back=...>
   It does
   <font size = "1 or 2" -> @small
   3 or 4  @medium()
   5,6, or 7 @large()
   <a href is dealt with by outputting "description <link>" or just "description" as appropriate
*/
char *html_to_zephyr(const char *message);

/* This parses zephyr formatting and converts it to html. For example, if
 * you pass in "@{@color(blue)@i(hello)}" you should get out
 * "<font color=blue><i>hello</i></font>". */
char *zephyr_to_html(const char *message);

#endif /* PURPLE_ZEPHYR_ZEPHYR_HTML_H */
