/**
 * @file gntutils.h Some utility functions
 * @ingroup gnt
 */
/*
 * GNT - The GLib Ncurses Toolkit
 *
 * GNT is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This library is free software; you can redistribute it and/or modify
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

#include <glib.h>

#include "gnt.h"
#include "gnttextview.h"
#include "gntwidget.h"

typedef gpointer (*GDupFunc)(gconstpointer data);

/**
 * Compute the width and height required to view the text on the screen.
 *
 * @text:     The text to be displayed.
 * @width:    The width required is set here, if not %NULL.
 * @height:   The height required is set here, if not %NULL.
 */
void gnt_util_get_text_bound(const char *text, int *width, int *height);

/* excluding *end */
/**
 * Get the onscreen width of a string, or a substring.
 *
 * @start:  The beginning of the string.
 * @end:    The end of the string. The width returned is the width
 *               upto (but not including) end. If end is NULL, then start
 *               is considered as a %NULL-terminated string.
 *
 * Returns:       The on-screen width of the string.
 */
int gnt_util_onscreen_width(const char *start, const char *end);

/**
 * Computes and returns the string after a specific number of onscreen characters.
 *
 * @str:  The string.
 * @len:  The length to consider. If non-positive, the entire screenlength is used.
 * @param w    The actual width of the string upto the returned offset, if not %NULL.
 *
 * Returns:     The string after len offset.
 */
const char *gnt_util_onscreen_width_to_pointer(const char *str, int len, int *w);

/**
 * Inserts newlines in 'string' where necessary so that its onscreen width is
 * no more than 'maxw'.
 *
 * @string:  The string.
 * @maxw:    The width that the string should fit into. If maxw is <= 0,
 *                then the available maximum width is used.
 *
 * Returns:  A newly allocated string that needs to be freed by the caller.
 */
char * gnt_util_onscreen_fit_string(const char *string, int maxw);

/**
 * Duplicate the contents of a hastable.
 *
 * @src:         The source hashtable.
 * @hash:        The hash-function to use.
 * @equal:       The hash-equal function to use.
 * @key_d:       The key-destroy function to use.
 * @value_d:     The value-destroy function to use.
 * @key_dup:     The function to use to duplicate the key.
 * @value_dup:   The function to use to duplicate the value.
 *
 * Returns:    The new hashtable.
 */
GHashTable * g_hash_table_duplicate(GHashTable *src, GHashFunc hash, GEqualFunc equal, GDestroyNotify key_d, GDestroyNotify value_d, GDupFunc key_dup, GDupFunc value_dup);

/**
 * To be used with g_signal_new. Look in the key_pressed signal-definition in
 * gntwidget.c for usage.
 *
 * @ihint:           NA
 * Returns:_accu:     NA
 * @handler_return:  NA
 * @dummy:           NA
 *
 * Returns:  NA
 */
gboolean gnt_boolean_handled_accumulator(GSignalInvocationHint *ihint, GValue *return_accu, const GValue *handler_return, gpointer dummy);

/**
 * Get a helpful display about the bindings of a widget.
 *
 * @widget: The widget to get bindings for.
 *
 * Returns: Returns a GntTree populated with "key" -> "binding" for the widget.
 */
GntWidget * gnt_widget_bindings_view(GntWidget *widget);

/**
 * Parse widgets from an XML description. For example,
 *
 * @code
 * GntWidget *win, *button;
 * gnt_util_parse_widgets("\
 *      <vwindow id='0' fill='0' align='2'>     \
 *          <label>This is a test</label>       \
 *          <button id='1'>OK</button>          \
 *      </vwindow>",
 *   2, &win, &button);
 * @endcode
 *
 * @string:  The XML string.
 * @num:     The number of widgets to return, followed by 'num' GntWidget **
 */
void gnt_util_parse_widgets(const char *string, int num, ...);

/**
 * Parse an XHTML string and add it in a GntTextView with
 * appropriate text flags.
 *
 * @string:   The XHTML string
 * @tv:       The GntTextView
 * Returns:  %TRUE if the string was added to the textview properly, %FALSE otherwise.
 *
 * @since 2.2.0
 */
gboolean gnt_util_parse_xhtml_to_textview(const char *string, GntTextView *tv);

/**
 * Make some keypress activate a button when some key is pressed with 'wid' in focus.
 *
 * @widget:  The widget
 * @key:     The key to trigger the button
 * @button:  The button to trigger
 *
 * @since 2.0.0 (gnt), 2.1.0 (pidgin)
 */
void gnt_util_set_trigger_widget(GntWidget *widget, const char *key, GntWidget *button);

