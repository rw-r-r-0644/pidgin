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

#ifndef PIDGIN_CONV_THEME_H
#define PIDGIN_CONV_THEME_H
/**
 * SECTION:gtkconv-theme
 * @section_id: pidgin-gtkconv-theme
 * @short_description: <filename>gtkconv-theme.h</filename>
 * @title: Conversation Theme Class
 */

#include <glib.h>
#include <glib-object.h>
#include "conversation.h"
#include "theme.h"

#define PIDGIN_TYPE_CONV_THEME  pidgin_conversation_theme_get_type()

typedef enum {
	PIDGIN_CONVERSATION_THEME_TEMPLATE_MAIN,
	PIDGIN_CONVERSATION_THEME_TEMPLATE_HEADER,
	PIDGIN_CONVERSATION_THEME_TEMPLATE_FOOTER,
	PIDGIN_CONVERSATION_THEME_TEMPLATE_TOPIC,
	PIDGIN_CONVERSATION_THEME_TEMPLATE_STATUS,
	PIDGIN_CONVERSATION_THEME_TEMPLATE_CONTENT,
	PIDGIN_CONVERSATION_THEME_TEMPLATE_INCOMING_CONTENT,
	PIDGIN_CONVERSATION_THEME_TEMPLATE_INCOMING_NEXT_CONTENT,
	PIDGIN_CONVERSATION_THEME_TEMPLATE_INCOMING_CONTEXT,
	PIDGIN_CONVERSATION_THEME_TEMPLATE_INCOMING_NEXT_CONTEXT,
	PIDGIN_CONVERSATION_THEME_TEMPLATE_OUTGOING_CONTENT,
	PIDGIN_CONVERSATION_THEME_TEMPLATE_OUTGOING_NEXT_CONTENT,
	PIDGIN_CONVERSATION_THEME_TEMPLATE_OUTGOING_CONTEXT,
	PIDGIN_CONVERSATION_THEME_TEMPLATE_OUTGOING_NEXT_CONTEXT,
	PIDGIN_CONVERSATION_THEME_TEMPLATE_BASESTYLE_CSS

} PidginConvThemeTemplateType;

/**************************************************************************/
/* Pidgin Conversation Theme API                                          */
/**************************************************************************/
G_BEGIN_DECLS

/**
 * pidgin_conversation_theme_get_type:
 *
 * Returns: The #GType for a conversation theme.
 */
G_DECLARE_FINAL_TYPE(PidginConvTheme, pidgin_conversation_theme, PIDGIN,
		CONV_THEME, PurpleTheme)

/**
 * pidgin_conversation_theme_get_info:
 * @theme: The conversation theme
 *
 * Get the Info.plist hash table from a conversation theme.
 *
 * Returns: The hash table. Keys are strings as outlined for message styles,
 *         values are GValue*s. This is an internal structure. Take a ref if
 *         necessary, but don't destroy it yourself.
 */
const GHashTable *pidgin_conversation_theme_get_info(PidginConvTheme *theme);

/**
 * pidgin_conversation_theme_set_info:
 * @theme: The conversation theme
 * @info:  The new hash table. The theme will take ownership of this hash
 *              table. Do not use it yourself afterwards with holding a ref.
 *              For key and value specifications, see pidgin_conversation_theme_get_info().
 *
 * Set the Info.plist hash table for a conversation theme.
 */
void pidgin_conversation_theme_set_info(PidginConvTheme *theme, GHashTable *info);

/**
 * pidgin_conversation_theme_lookup:
 * @theme:    The conversation theme
 * @key:      The key to find
 * @specific: Whether to search variant-specific keys
 *
 * Lookup a key in a theme
 *
 * Returns: The key information. If @specific is %TRUE, then keys are first
 *         searched by variant, then by general ones. Otherwise, only general
 *         key values are returned.
 */
const GValue *pidgin_conversation_theme_lookup(PidginConvTheme *theme, const char *key, gboolean specific);

/**
 * pidgin_conversation_theme_get_template:
 * @theme: The conversation theme
 * @type:  The type of template data
 *
 * Get the template data from a conversation theme.
 *
 * Returns: The template data requested. Fallback is made as required by styles.
 *         Subsequent calls to this function will return cached values.
 */
const char *pidgin_conversation_theme_get_template(PidginConvTheme *theme, PidginConvThemeTemplateType type);

/**
 * pidgin_conversation_theme_add_variant:
 * @theme:   The conversation theme
 * @variant: The name of the variant
 *
 * Add an available variant name to a conversation theme.
 *
 * Note: The conversation theme will take ownership of the variant name string.
 *       This function should normally only be called by the theme loader.
 */
void pidgin_conversation_theme_add_variant(PidginConvTheme *theme, char *variant);

/**
 * pidgin_conversation_theme_get_variant:
 * @theme: The conversation theme
 *
 * Get the currently set variant name for a conversation theme.
 *
 * Returns: The current variant name.
 */
const char *pidgin_conversation_theme_get_variant(PidginConvTheme *theme);

/**
 * pidgin_conversation_theme_set_variant:
 * @theme:   The conversation theme
 * @variant: The name of the variant
 *
 * Set the variant name for a conversation theme.
 */
void pidgin_conversation_theme_set_variant(PidginConvTheme *theme, const char *variant);

/**
 * pidgin_conversation_theme_get_variants:
 * @theme: The conversation theme
 *
 * Get a list of available variants for a conversation theme.
 *
 * Returns: (element-type utf8): The list of variants. This GList and the string data are owned by
 *         the theme and should not be freed by the caller.
 */
const GList *pidgin_conversation_theme_get_variants(PidginConvTheme *theme);

/**
 * pidgin_conversation_theme_get_template_path:
 * @theme: The conversation theme
 *
 * Get the path to the template HTML file.
 *
 * Returns: The path to the HTML file.
 */
char *pidgin_conversation_theme_get_template_path(PidginConvTheme *theme);

/**
 * pidgin_conversation_theme_get_css_path:
 * @theme: The conversation theme
 *
 * Get the path to the current variant CSS file.
 *
 * Returns: The path to the CSS file.
 */
char *pidgin_conversation_theme_get_css_path(PidginConvTheme *theme);

/**
 * pidgin_conversation_theme_get_nick_colors:
 * @theme: The conversation theme
 *
 * Get (and reference) the array of nick colors
 *
 * Returns: (transfer container) (element-type GdkRGBA): Pointer to GArray of nick colors, or NULL if no colors in theme
 */
GArray *pidgin_conversation_theme_get_nick_colors(PidginConvTheme *theme);

G_END_DECLS

#endif /* PIDGIN_CONV_THEME_H */

