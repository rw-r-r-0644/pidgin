/* pidgin
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#if !defined(PIDGIN_GLOBAL_HEADER_INSIDE) && !defined(PIDGIN_COMPILATION)
# error "only <pidgin.h> may be included directly"
#endif

#ifndef PIDGIN_MINI_DIALOG_H
#define PIDGIN_MINI_DIALOG_H

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PIDGIN_TYPE_MINI_DIALOG (pidgin_mini_dialog_get_type())
G_DECLARE_FINAL_TYPE(PidginMiniDialog, pidgin_mini_dialog, PIDGIN, MINI_DIALOG, GtkBox)

/**
 * PidginMiniDialog:
 * @contents: A GtkBox into which extra widgets for the dialog should be packed.
 *
 * A widget resembling a diminutive dialog box, designed to be embedded in the
 * #PidginBuddyList.  Mini-dialogs have titles, optional descriptions, and a row
 * of buttons at the bottom; above the buttons is a #GtkBox into which
 * you can pack any random widgets you want to add to the dialog.  When any of
 * the dialog's buttons is clicked, the dialog will be destroyed.
 *
 * Dialogs have the following GObject properties:
 * <informaltable frame='none'>
 *   <tgroup cols='2'><tbody>
 *   <row><entry><literal>"title"</literal></entry>
 *     <entry>(<type>char *</type>) A string to be displayed as the dialog's
 *       title.</entry>
 *   </row>
 *   <row><entry><literal>"description"</literal></entry>
 *     <entry>(<type>char *</type>) A string to be displayed as the dialog's
 *       description.  If this is %NULL, the description widget will be
 *       hidden.</entry>
 *   </row>
 *   <row><entry><literal>"icon-name"</literal></entry>
 *     <entry>(<type>char *</type>)The #GtkIconTheme name of an icon for the
 *       dialog, or %NULL for no icon.</entry>
 *   </row>
 *   <row><entry><literal>"custom-icon"</literal></entry>
 *     <entry>(<type>GdkPixbuf *</type>) The custom icon to use instead of a
 *       #GtkIconTheme one (overrides the <literal>"icon-name"</literal>
 *       property).</entry>
 *   </row>
 *   </tbody></tgroup>
 * </informaltable>
 */
struct _PidginMiniDialog {
	GtkBox parent;

	/*< public >*/
	GtkBox *contents;
};

/**
 * PidginMiniDialogCallback:
 * @mini_dialog: a dialog, one of whose buttons has been pressed.
 * @button:      the button which was pressed.
 * @user_data:   arbitrary data, supplied to
 *                    pidgin_mini_dialog_add_button() when the button was
 *                    created.
 *
 * The type of a callback triggered by a button in a mini-dialog being pressed.
 */
typedef void (*PidginMiniDialogCallback)(PidginMiniDialog *mini_dialog,
	GtkButton *button, gpointer user_data);

/**
 * pidgin_mini_dialog_new:
 *
 * Creates a new #PidginMiniDialog with a #GtkIconTheme icon. This is a
 * shortcut for creating the dialog with g_object_new() then setting each
 * property yourself.
 *
 * Returns: a new #PidginMiniDialog.
 */
PidginMiniDialog *pidgin_mini_dialog_new(const gchar *title,
	const gchar *description, const gchar *icon_name);

/**
 * pidgin_mini_dialog_new_with_custom_icon:
 *
 * Creates a new #PidginMiniDialog with a custom icon. This is a shortcut for creating the dialog
 * with g_object_new() then setting each property yourself.
 *
 * Returns: a new #PidginMiniDialog.
 */
PidginMiniDialog *pidgin_mini_dialog_new_with_custom_icon(const gchar *title,
	const gchar *description, GdkPixbuf *custom_icon);

/**
 * pidgin_mini_dialog_new_with_buttons:
 * @title: The primary text.
 * @description: The secondary text, or %NULL for no description.
 * @icon_name: The name of an icon to use in the mini dialog.
 * @user_data: Data to pass to the callbacks.
 * @...: A %NULL-terminated list of button labels (<type>char *</type>) and
 *       callbacks (#PidginMiniDialogCallback).  (Callbacks may be %NULL to
 *       take no action when the corresponding button is pressed.) When a
 *       button is pressed, the callback (if any) will be called; when the
 *       callback returns the dialog will be destroyed.
 *
 * Creates a #PidginMiniDialog, suitable for embedding in the buddy list
 * scrollbook with pidgin_blist_add_alert().
 *
 * Returns: (transfer full): A #PidginMiniDialog, suitable for passing to
 *          pidgin_blist_add_alert().
 *
 * Since: 3.0.0
 */
GtkWidget *pidgin_mini_dialog_new_with_buttons(const gchar *title,
                                               const gchar *description,
                                               const gchar *icon_name,
                                               gpointer user_data, ...)
                                               G_GNUC_NULL_TERMINATED;

/**
 * pidgin_mini_dialog_set_title:
 * @mini_dialog: a mini-dialog
 * @title:       the new title for @mini_dialog
 *
 * Shortcut for setting a mini-dialog's title via GObject properties.
 */
void pidgin_mini_dialog_set_title(PidginMiniDialog *mini_dialog,
	const gchar *title);

/**
 * pidgin_mini_dialog_set_description:
 * @mini_dialog: a mini-dialog
 * @description: the new description for @mini_dialog, or %NULL to
 *                    hide the description widget.
 *
 * Shortcut for setting a mini-dialog's description via GObject properties.
 */
void pidgin_mini_dialog_set_description(PidginMiniDialog *mini_dialog,
	const gchar *description);

/**
 * pidgin_mini_dialog_enable_description_markup:
 * @mini_dialog: a mini-dialog
 *
 * Enable GMarkup elements in the mini-dialog's description.
 */
void pidgin_mini_dialog_enable_description_markup(PidginMiniDialog *mini_dialog);

/**
 * pidgin_mini_dialog_set_link_callback:
 * @mini_dialog: a mini-dialog
 * @cb: (scope call): the callback to invoke
 * @user_data: the user data to pass to the callback
 *
 * Sets a callback which gets invoked when a hyperlink in the dialog's description is clicked on.
 */
void pidgin_mini_dialog_set_link_callback(PidginMiniDialog *mini_dialog, GCallback cb, gpointer user_data);

/**
 * pidgin_mini_dialog_set_icon_name:
 * @mini_dialog: a mini-dialog
 * @icon_name:   the #GtkIconTheme name of an icon, or %NULL for no icon.
 *
 * Shortcut for setting a mini-dialog's icon via GObject properties.
 */
void pidgin_mini_dialog_set_icon_name(PidginMiniDialog *mini_dialog,
	const gchar *icon_name);

/**
 * pidgin_mini_dialog_set_custom_icon:
 * @mini_dialog: a mini-dialog
 * @custom_icon: the pixbuf to use as a custom icon
 *
 * Shortcut for setting a mini-dialog's custom icon via GObject properties.
 */
void pidgin_mini_dialog_set_custom_icon(PidginMiniDialog *mini_dialog,
	GdkPixbuf *custom_icon);

/**
 * pidgin_mini_dialog_add_button:
 * @mini_dialog: a mini-dialog
 * @text:        the text to display on the new button
 * @clicked_cb: (scope call): the function to call when the button is clicked
 * @user_data:   arbitrary data to pass to @clicked_cb when it is
 *                    called.
 *
 * Adds a new button to a mini-dialog, and attaches the supplied callback to
 * its <literal>clicked</literal> signal.  After a button is clicked, the dialog
 * is destroyed.
 */
void pidgin_mini_dialog_add_button(PidginMiniDialog *mini_dialog,
	const gchar *text, PidginMiniDialogCallback clicked_cb,
	gpointer user_data);

/**
 * pidgin_mini_dialog_add_non_closing_button:
 * @mini_dialog: a mini-dialog
 * @text:        the text to display on the new button
 * @clicked_cb: (scope call): the function to call when the button is clicked
 * @user_data:   arbitrary data to pass to @clicked_cb when it is
 *                    called.
 *
 * Equivalent to pidgin_mini_dialog_add_button(), the only difference
 * is that the mini-dialog won't be closed after the button is clicked.
 */
void pidgin_mini_dialog_add_non_closing_button(PidginMiniDialog *mini_dialog,
	const gchar *text, PidginMiniDialogCallback clicked_cb,
	gpointer user_data);

/**
 * pidgin_mini_dialog_get_num_children:
 * @mini_dialog: a mini-dialog
 *
 * Gets the number of widgets packed into PidginMiniDialog.contents.
 *
 * Returns: the number of widgets in @mini_dialog->contents.
 */
guint pidgin_mini_dialog_get_num_children(PidginMiniDialog *mini_dialog);

G_END_DECLS

#endif /* PIDGIN_MINI_DIALOG_H */
