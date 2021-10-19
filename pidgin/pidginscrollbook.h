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

#if !defined(PIDGIN_GLOBAL_HEADER_INSIDE) && !defined(PIDGIN_COMPILATION)
# error "only <pidgin.h> may be included directly"
#endif

#ifndef PIDGIN_SCROLL_BOOK_H
#define PIDGIN_SCROLL_BOOK_H
/**
 * PidginScrollBook:
 *
 * This widget is used to show error messages in the buddy list window.
 */

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PIDGIN_TYPE_SCROLL_BOOK (pidgin_scroll_book_get_type ())
G_DECLARE_FINAL_TYPE(PidginScrollBook, pidgin_scroll_book, PIDGIN, SCROLL_BOOK,
                     GtkBox)

/**
 * pidgin_scroll_book_new:
 *
 * Creates a new #PidginScrollBook.
 *
 * Returns: (transfer full): The new #PidginScrollBook instance.
 */
GtkWidget *pidgin_scroll_book_new(void);

/**
 * pidgin_scroll_book_get_notebook:
 * @scroll_book: The #PidginScrollBook instance.
 *
 * Gets the notebook widget from @scroll_book.
 *
 * Returns: (transfer none): The notebook inside of @scroll_book.
 */
GtkWidget *pidgin_scroll_book_get_notebook(PidginScrollBook *scroll_book);

G_END_DECLS

#endif  /* PIDGIN_SCROLL_BOOK_H */
