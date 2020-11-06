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

#if !defined(PURPLE_GLOBAL_HEADER_INSIDE) && !defined(PURPLE_COMPILATION)
# error "only <purple.h> may be included directly"
#endif

#ifndef PURPLE_UTIL_H
#define PURPLE_UTIL_H
/**
 * SECTION:util
 * @section_id: libpurple-util
 * @short_description: <filename>util.h</filename>
 * @title: Utility Functions
 *
 * The utility API is a cesspool please don't add more stuff here.  Namespace
 * new functions properly and don't put them here.
 */

#include <stdio.h>

#include "account.h"
#include "signals.h"
#include "xmlnode.h"
#include "notify.h"
#include "protocols.h"


typedef char *(*PurpleInfoFieldFormatCallback)(const char *field, size_t len);

G_BEGIN_DECLS

/**
 * purple_util_set_current_song:
 * @title:     The title of the song, %NULL to unset the value.
 * @artist:    The artist of the song, can be %NULL.
 * @album:     The album of the song, can be %NULL.
 *
 * Set the appropriate presence values for the currently playing song.
 */
void purple_util_set_current_song(const char *title, const char *artist,
		const char *album);

/**
 * purple_util_format_song_info:
 * @title:     The title of the song, %NULL to unset the value.
 * @artist:    The artist of the song, can be %NULL.
 * @album:     The album of the song, can be %NULL.
 * @unused:    Currently unused, must be %NULL.
 *
 * Format song information.
 *
 * Returns:   The formatted string. The caller must g_free the returned string.
 */
char * purple_util_format_song_info(const char *title, const char *artist,
		const char *album, gpointer unused);

/**************************************************************************/
/* Utility Subsystem                                                      */
/**************************************************************************/

/**
 * purple_util_init:
 *
 * Initializes the utility subsystem.
 */
void purple_util_init(void);

/**
 * purple_util_uninit:
 *
 * Uninitializes the util subsystem.
 */
void purple_util_uninit(void);

/**************************************************************************/
/* Date/Time Functions                                                    */
/**************************************************************************/

/**
 * purple_utf8_strftime:
 * @format: The format string, in UTF-8
 * @tm:     The time to format, or %NULL to use the current local time
 *
 * Formats a time into the specified format.
 *
 * This is essentially strftime(), but it has a static buffer
 * and handles the UTF-8 conversion for the caller.
 *
 * This function also provides the GNU \%z formatter if the underlying C
 * library doesn't.  However, the format string parser is very naive, which
 * means that conversions specifiers to \%z cannot be guaranteed.  The GNU
 * strftime(3) man page describes \%z as: 'The time-zone as hour offset from
 * GMT.  Required to emit RFC822-conformant dates
 * (using "\%a, \%d \%b \%Y \%H:\%M:\%S \%z"). (GNU)'
 *
 * On Windows, this function also converts the results for \%Z from a timezone
 * name (as returned by the system strftime() \%Z format string) to a timezone
 * abbreviation (as is the case on Unix).  As with \%z, conversion specifiers
 * should not be used.
 *
 * Note: @format is required to be in UTF-8.  This differs from strftime(),
 *       where the format is provided in the locale charset.
 *
 * Returns: The formatted time, in UTF-8.
 */
const char *purple_utf8_strftime(const char *format, const struct tm *tm);

/**
 * purple_date_format_full:
 * @tm: The time to format, or %NULL to use the current local time
 *
 * Formats a time into the user's preferred full date and time format.
 *
 * The returned string is stored in a static buffer, so the result
 * should be g_strdup()'d if it's going to be kept.
 *
 * Returns: The date and time, formatted as per the user's settings.  In the
 *         USA this is something like "Mon Feb 18 15:26:44 2013"
 */
const char *purple_date_format_full(const struct tm *tm);

/**
 * PURPLE_NO_TZ_OFF:
 *
 * Used by purple_str_to_time to indicate no timezone offset was
 * specified in the timestamp string.
 */
#define PURPLE_NO_TZ_OFF -500000

/**
 * purple_str_to_time:
 * @timestamp: The timestamp
 * @utc:       Assume UTC if no timezone specified
 * @tm:        If not %NULL, the caller can get a copy of the
 *                  struct tm used to calculate the time_t return value.
 * @tz_off:    If not %NULL, the caller can get a copy of the
 *                  timezone offset (from UTC) used to calculate the time_t
 *                  return value. Note: Zero is a valid offset. As such,
 *                  the value of the macro PURPLE_NO_TZ_OFF indicates no
 *                  offset was specified (which means that the local
 *                  timezone was used in the calculation).
 * @rest:      If not %NULL, the caller can get a pointer to the
 *                  part of @timestamp left over after parsing is
 *                  completed, if it's not the end of @timestamp.
 *
 * Parses a timestamp in jabber, ISO8601, or MM/DD/YYYY format and returns
 * a time_t.
 *
 * Returns: A time_t.
 */
time_t purple_str_to_time(const char *timestamp, gboolean utc,
                        struct tm *tm, long *tz_off, const char **rest);

/**
 * purple_str_to_date_time:
 * @timestamp: The timestamp
 * @utc:       Assume UTC if no timezone specified
 *
 * Parses a timestamp in jabber, ISO8601, or MM/DD/YYYY format and returns
 * a GDateTime.
 *
 * Returns: (transfer full): A GDateTime.
 */
GDateTime *purple_str_to_date_time(const char *timestamp, gboolean utc);

/**
 * purple_time_parse_month:
 * @month_abbr: The 3-letter month abbreviation
 *
 * Get month number suitable for GDateTime. If @month_abbr is unknown,
 * returns 0.
 *
 * Returns: A month number or 0.
 *
 * Since: 3.0.0
 */
gint purple_time_parse_month(const char *month_abbr);

/**************************************************************************/
/* Path/Filename Functions                                                */
/**************************************************************************/

/**
 * purple_home_dir:
 *
 * Returns the user's home directory.
 *
 *  See purple_user_dir()
 *
 * Returns: The user's home directory.
 */
const gchar *purple_home_dir(void);

/**
 * purple_user_dir:
 *
 * Returns the purple settings directory in the user's home directory.
 * This is usually $HOME/.purple
 *
 *  See purple_home_dir()
 *
 * Returns: The purple settings directory.
 * 
 * Deprecated: Use purple_cache_dir(), purple_config_dir() or
 *             purple_data_dir() instead.
 */
G_DEPRECATED_FOR(purple_cache_dir or purple_config_dir or purple_data_dir)
const char *purple_user_dir(void);

/**
 * purple_cache_dir:
 *
 * Returns the purple cache directory according to XDG Base Directory Specification.
 * This is usually $HOME/.cache/purple.
 * If custom user dir was specified then this is cache
 * sub-directory of DIR argument passed to -c option.
 *
 * Returns: The purple cache directory.
 */
const gchar *purple_cache_dir(void);

/**
 * purple_config_dir:
 *
 * Returns the purple configuration directory according to XDG Base Directory Specification.
 * This is usually $HOME/.config/purple.
 * If custom user dir was specified then this is config
 * sub-directory of DIR argument passed to -c option.
 *
 * Returns: The purple configuration directory.
 */
const gchar *purple_config_dir(void);

/**
 * purple_data_dir:
 *
 * Returns the purple data directory according to XDG Base Directory Specification.
 * This is usually $HOME/.local/share/purple.
 * If custom user dir was specified then this is data
 * sub-directory of DIR argument passed to -c option.
 *
 * Returns: The purple data directory.
 */
const gchar *purple_data_dir(void);

/**
 * purple_move_to_xdg_base_dir:
 * @purple_xdg_dir: The path to cache, config or data dir.
 *                  Use respective function
 * @path:           File or directory in purple_user_dir
 * 
 * Moves file or directory from legacy user dir to XDG
 * based dir.
 * 
 * Returns: TRUE if moved successfully, FALSE otherwise
 */
gboolean
purple_move_to_xdg_base_dir(const char *purple_xdg_dir, char *path);

/**
 * purple_util_set_user_dir:
 * @dir: The custom settings directory
 *
 * Define a custom purple settings directory, overriding the default (user's home directory/.purple)
 */
void purple_util_set_user_dir(const char *dir);

/**
 * purple_util_write_data_to_file:
 * @filename: The basename of the file to write in the purple_user_dir.
 * @data:     A string of data to write.
 * @size:     The size of the data to save.  If data is
 *                 null-terminated you can pass in -1.
 *
 * Write a string of data to a file of the given name in the Purple
 * user directory ($HOME/.purple by default).  The data is typically
 * a serialized version of one of Purple's config files, such as
 * prefs.xml, accounts.xml, etc.  And the string is typically
 * obtained using purple_xmlnode_to_formatted_str.  However, this function
 * should work fine for saving binary files as well.
 *
 * Returns: TRUE if the file was written successfully.  FALSE otherwise.
 * 
 * Deprecated: Use purple_util_write_data_to_cache_file(),
 *             purple_util_write_data_to_config_file() or
 *             purple_util_write_data_to_data_file() instead.
 */
G_DEPRECATED_FOR(purple_util_write_data_to_cache_file or purple_util_write_data_to_config_file or purple_util_write_data_to_data_file)
gboolean purple_util_write_data_to_file(const char *filename, const char *data,
									  gssize size);

/**
 * purple_util_write_data_to_cache_file:
 * @filename: The basename of the file to write in the purple_cache_dir.
 * @data:     A string of data to write.
 * @size:     The size of the data to save.  If data is
 *                 null-terminated you can pass in -1.
 *
 * Write a string of data to a file of the given name in the Purple
 * cache directory ($HOME/.cache/purple by default).
 * 
 *  See purple_util_write_data_to_file()
 *
 * Returns: TRUE if the file was written successfully.  FALSE otherwise.
 */
gboolean
purple_util_write_data_to_cache_file(const char *filename, const char *data, gssize size);

/**
 * purple_util_write_data_to_config_file:
 * @filename: The basename of the file to write in the purple_config_dir.
 * @data:     A string of data to write.
 * @size:     The size of the data to save.  If data is
 *                 null-terminated you can pass in -1.
 *
 * Write a string of data to a file of the given name in the Purple
 * config directory ($HOME/.config/purple by default).
 *
 *  See purple_util_write_data_to_file()
 *
 * Returns: TRUE if the file was written successfully.  FALSE otherwise.
 */
gboolean
purple_util_write_data_to_config_file(const char *filename, const char *data, gssize size);

/**
 * purple_util_write_data_to_data_file:
 * @filename: The basename of the file to write in the purple_data_dir.
 * @data:     A string of data to write.
 * @size:     The size of the data to save.  If data is
 *                 null-terminated you can pass in -1.
 *
 * Write a string of data to a file of the given name in the Purple
 * data directory ($HOME/.local/share/purple by default).
 *
 *  See purple_util_write_data_to_file()
 *
 * Returns: TRUE if the file was written successfully.  FALSE otherwise.
 */
gboolean
purple_util_write_data_to_data_file(const char *filename, const char *data, gssize size);

/**
 * purple_util_write_data_to_file_absolute:
 * @filename_full: Filename to write to
 * @data:          A string of data to write.
 * @size:          The size of the data to save.  If data is
 *                      null-terminated you can pass in -1.
 *
 * Write data to a file using the absolute path.
 *
 * This exists for Glib backwards compatibility reasons.
 *
 *  See purple_util_write_data_to_file()
 *
 * Returns: TRUE if the file was written successfully.  FALSE otherwise.
 */
/* TODO: Remove this function (use g_file_set_contents instead) when 3.0.0
 *       rolls around. */
gboolean
purple_util_write_data_to_file_absolute(const char *filename_full, const char *data, gssize size);

/**
 * purple_util_read_xml_from_file:
 * @filename:    The basename of the file to open in the purple_user_dir.
 * @description: A very short description of the contents of this
 *                    file.  This is used in error messages shown to the
 *                    user when the file can not be opened.  For example,
 *                    "preferences," or "buddy pounces."
 *
 * Read the contents of a given file and parse the results into an
 * PurpleXmlNode tree structure.  This is intended to be used to read
 * Purple's configuration xml files (prefs.xml, pounces.xml, etc.)
 *
 * Returns: An PurpleXmlNode tree of the contents of the given file.  Or NULL, if
 *         the file does not exist or there was an error reading the file.
 * 
 * Deprecated: Use purple_util_read_xml_from_cache_file(),
 *             purple_util_read_xml_from_config_file() or
 *             purple_util_read_xml_from_data_file() instead.
 */
G_DEPRECATED_FOR(purple_util_read_xml_from_cache_file or purple_util_read_xml_from_config_file or purple_util_read_xml_from_data_file)
PurpleXmlNode *purple_util_read_xml_from_file(const char *filename,
									  const char *description);

/**
 * purple_util_read_xml_from_cache_file:
 * @filename:    The basename of the file to open in the purple_cache_dir.
 * @description: A very short description of the contents of this
 *                    file.  This is used in error messages shown to the
 *                    user when the file can not be opened.  For example,
 *                    "preferences," or "buddy pounces."
 *
 * Read the contents of a given file and parse the results into an
 * PurpleXmlNode tree structure.  This is intended to be used to read
 * Purple's cache xml files (xmpp-caps.xml, etc.)
 *
 * Returns: An PurpleXmlNode tree of the contents of the given file.  Or NULL, if
 *         the file does not exist or there was an error reading the file.
 */
PurpleXmlNode *
purple_util_read_xml_from_cache_file(const char *filename, const char *description);

/**
 * purple_util_read_xml_from_config_file:
 * @filename:    The basename of the file to open in the purple_config_dir.
 * @description: A very short description of the contents of this
 *                    file.  This is used in error messages shown to the
 *                    user when the file can not be opened.  For example,
 *                    "preferences," or "buddy pounces."
 *
 * Read the contents of a given file and parse the results into an
 * PurpleXmlNode tree structure.  This is intended to be used to read
 * Purple's config xml files (prefs.xml, pounces.xml, etc.)
 *
 * Returns: An PurpleXmlNode tree of the contents of the given file.  Or NULL, if
 *         the file does not exist or there was an error reading the file.
 */
PurpleXmlNode *
purple_util_read_xml_from_config_file(const char *filename, const char *description);

/**
 * purple_util_read_xml_from_data_file:
 * @filename:    The basename of the file to open in the purple_data_dir.
 * @description: A very short description of the contents of this
 *                    file.  This is used in error messages shown to the
 *                    user when the file can not be opened.  For example,
 *                    "preferences," or "buddy pounces."
 *
 * Read the contents of a given file and parse the results into an
 * PurpleXmlNode tree structure.  This is intended to be used to read
 * Purple's cache xml files (accounts.xml, etc.)
 *
 * Returns: An PurpleXmlNode tree of the contents of the given file.  Or NULL, if
 *         the file does not exist or there was an error reading the file.
 */
PurpleXmlNode *
purple_util_read_xml_from_data_file(const char *filename, const char *description);

/**
 * purple_mkstemp:
 * @path:   The returned path to the temp file.
 * @binary: Text or binary, for platforms where it matters.
 *
 * Creates a temporary file and returns a file pointer to it.
 *
 * This is like mkstemp(), but returns a file pointer and uses a
 * pre-set template. It uses the semantics of tempnam() for the
 * directory to use and allocates the space for the file path.
 *
 * The caller is responsible for closing the file and removing it when
 * done, as well as freeing the space pointed to by @path with
 * g_free().
 *
 * Returns: A file pointer to the temporary file, or %NULL on failure.
 */
FILE *purple_mkstemp(char **path, gboolean binary);


/**************************************************************************/
/* Environment Detection Functions                                        */
/**************************************************************************/

/**
 * purple_program_is_valid:
 * @program: The file name of the application.
 *
 * Checks if the given program name is valid and executable.
 *
 * Returns: TRUE if the program is runable.
 */
gboolean purple_program_is_valid(const char *program);

/**
 * purple_running_gnome:
 *
 * Check if running GNOME.
 *
 * Returns: TRUE if running GNOME, FALSE otherwise.
 */
gboolean purple_running_gnome(void);

/**
 * purple_running_kde:
 *
 * Check if running KDE.
 *
 * Returns: TRUE if running KDE, FALSE otherwise.
 */
gboolean purple_running_kde(void);

/**
 * purple_running_osx:
 *
 * Check if running OS X.
 *
 * Returns: TRUE if running OS X, FALSE otherwise.
 */
gboolean purple_running_osx(void);

/**
 * purple_socket_get_family:
 * @fd: The socket file descriptor.
 *
 * Returns the address family of a socket.
 *
 * Returns: The address family of the socket (AF_INET, AF_INET6, etc) or -1
 *         on error.
 */
int purple_socket_get_family(int fd);

/**
 * purple_socket_speaks_ipv4:
 * @fd: The socket file descriptor
 *
 * Returns TRUE if a socket is capable of speaking IPv4.
 *
 * This is the case for IPv4 sockets and, on some systems, IPv6 sockets
 * (due to the IPv4-mapped address functionality).
 *
 * Returns: TRUE if a socket can speak IPv4.
 */
gboolean purple_socket_speaks_ipv4(int fd);


/**************************************************************************/
/* String Functions                                                       */
/**************************************************************************/

/**
 * purple_strequal:
 * @left:	A string
 * @right: A string to compare with left
 *
 * Tests two strings for equality.
 *
 * Unlike strcmp(), this function will not crash if one or both of the
 * strings are %NULL.
 *
 * Returns: %TRUE if the strings are the same, else %FALSE.
 */
static inline gboolean
purple_strequal(const gchar *left, const gchar *right)
{
	return (g_strcmp0(left, right) == 0);
}

/**
 * purple_normalize:
 * @account:  The account the string belongs to, or NULL if you do
 *                 not know the account.  If you use NULL, the string
 *                 will still be normalized, but if the protocol uses a
 *                 custom normalization function then the string may
 *                 not be normalized correctly.
 * @str:      The string to normalize.
 *
 * Normalizes a string, so that it is suitable for comparison.
 *
 * The returned string will point to a static buffer, so if the
 * string is intended to be kept long-term, you <emphasis>must</emphasis>
 * g_strdup() it. Also, calling normalize() twice in the same line
 * will lead to problems.
 *
 * Returns: A pointer to the normalized version stored in a static buffer.
 */
const char *purple_normalize(PurpleAccount *account, const char *str);

/**
 * purple_normalize_nocase:
 * @account:  The account the string belongs to.
 * @str:      The string to normalize.
 *
 * Normalizes a string, so that it is suitable for comparison.
 *
 * This is one possible implementation for the protocol callback
 * function "normalize."  It returns a lowercase and UTF-8
 * normalized version of the string.
 *
 * Returns: A pointer to the normalized version stored in a static buffer.
 */
const char *purple_normalize_nocase(const PurpleAccount *account, const char *str);

/**
 * purple_validate:
 * @protocol: The protocol the string belongs to.
 * @str:      The string to validate.
 *
 * Checks, if a string is valid.
 *
 * Returns: TRUE, if string is valid, otherwise FALSE.
 */
gboolean purple_validate(const PurpleProtocol *protocol, const char *str);

/**
 * purple_str_has_caseprefix:
 * @s: The string to check.
 * @p: The prefix in question.
 *
 * Compares two strings to see if the first contains the second as
 * a proper case-insensitive prefix.
 *
 * Returns: %TRUE if @p is a prefix of @s, otherwise %FALSE.
 */
gboolean
purple_str_has_caseprefix(const gchar *s, const gchar *p);

/**
 * purple_strdup_withhtml:
 * @src: The source string.
 *
 * Duplicates a string and replaces all newline characters from the
 * source string with HTML linebreaks.
 *
 * Returns: The new string.  Must be g_free'd by the caller.
 */
gchar *purple_strdup_withhtml(const gchar *src);

/**
 * purple_str_add_cr:
 * @str: The source string.
 *
 * Ensures that all linefeeds have a matching carriage return.
 *
 * Returns: The string with carriage returns.
 */
char *purple_str_add_cr(const char *str);

/**
 * purple_str_strip_char:
 * @str:     The string to strip characters from.
 * @thechar: The character to strip from the given string.
 *
 * Strips all instances of the given character from the
 * given string.  The string is modified in place.  This
 * is useful for stripping new line characters, for example.
 *
 * Example usage:
 * purple_str_strip_char(my_dumb_string, '\n');
 */
void purple_str_strip_char(char *str, char thechar);

/**
 * purple_util_chrreplace:
 * @string: The string from which to replace stuff.
 * @delimiter: The character you want replaced.
 * @replacement: The character you want inserted in place
 *        of the delimiting character.
 *
 * Given a string, this replaces all instances of one character
 * with another.  This happens inline (the original string IS
 * modified).
 */
void purple_util_chrreplace(char *string, char delimiter,
						  char replacement);

/**
 * purple_strreplace:
 * @string: The string from which to replace stuff.
 * @delimiter: The substring you want replaced.
 * @replacement: The substring you want inserted in place
 *        of the delimiting substring.
 *
 * Given a string, this replaces one substring with another
 * and returns a newly allocated string.
 *
 * Returns: A new string, after performing the substitution.
 *         free this with g_free().
 */
gchar *purple_strreplace(const char *string, const char *delimiter,
					   const char *replacement);


/**
 * purple_utf8_ncr_encode:
 * @in: The string which might contain utf-8 substrings
 *
 * Given a string, this replaces any utf-8 substrings in that string with
 * the corresponding numerical character reference, and returns a newly
 * allocated string.
 *
 * Returns: A new string, with utf-8 replaced with numerical character
 *         references, free this with g_free()
*/
char *purple_utf8_ncr_encode(const char *in);


/**
 * purple_utf8_ncr_decode:
 * @in: The string which might contain numerical character references.
 *
 * Given a string, this replaces any numerical character references
 * in that string with the corresponding actual utf-8 substrings,
 * and returns a newly allocated string.
 *
 * Returns: A new string, with numerical character references
 *         replaced with actual utf-8, free this with g_free().
 */
char *purple_utf8_ncr_decode(const char *in);


/**
 * purple_strcasereplace:
 * @string: The string from which to replace stuff.
 * @delimiter: The substring you want replaced.
 * @replacement: The substring you want inserted in place
 *        of the delimiting substring.
 *
 * Given a string, this replaces one substring with another
 * ignoring case and returns a newly allocated string.
 *
 * Returns: A new string, after performing the substitution.
 *         free this with g_free().
 */
gchar *purple_strcasereplace(const char *string, const char *delimiter,
						   const char *replacement);

/**
 * purple_strcasestr:
 * @haystack: The string to search in.
 * @needle:   The substring to find.
 *
 * This is like strstr, except that it ignores ASCII case in
 * searching for the substring.
 *
 * Returns: the location of the substring if found, or NULL if not
 */
const char *purple_strcasestr(const char *haystack, const char *needle);

/**
 * purple_str_seconds_to_string:
 * @sec: The seconds.
 *
 * Converts seconds into a human-readable form.
 *
 * Returns: A human-readable form, containing days, hours, minutes, and
 *         seconds.
 */
char *purple_str_seconds_to_string(guint sec);

/**
 * purple_utf16_size:
 * @str: String to check.
 *
 * Calculates UTF-16 string size (in bytes).
 *
 * Returns:    Number of bytes (including NUL character) that string occupies.
 */
size_t purple_utf16_size(const gunichar2 *str);

/**
 * purple_str_wipe:
 * @str: A NUL-terminated string to free, or a NULL-pointer.
 *
 * Fills a NUL-terminated string with zeros and frees it.
 *
 * It should be used to free sensitive data, like passwords.
 */
void purple_str_wipe(gchar *str);

/**
 * purple_utf16_wipe:
 * @str: A NUL-terminated string to free, or a NULL-pointer.
 *
 * Fills a NUL-terminated UTF-16 string with zeros and frees it.
 *
 * It should be used to free sensitive data, like passwords.
 */
void purple_utf16_wipe(gunichar2 *str);

/**************************************************************************/
/* URI/URL Functions                                                      */
/**************************************************************************/

void purple_got_protocol_handler_uri(const char *uri);

/**
 * purple_url_decode:
 * @str: The string to translate.
 *
 * Decodes a URL into a plain string.
 *
 * This will change hex codes and such to their ascii equivalents.
 *
 * Returns: The resulting string.
 */
const char *purple_url_decode(const char *str);

/**
 * purple_url_encode:
 * @str: The string to translate.
 *
 * Encodes a URL into an escaped string.
 *
 * This will change non-alphanumeric characters to hex codes.
 *
 * Returns: The resulting string.
 */
const char *purple_url_encode(const char *str);

/**
 * purple_email_is_valid:
 * @address: The email address to validate.
 *
 * Checks if the given email address is syntactically valid.
 *
 * Returns: True if the email address is syntactically correct.
 */
gboolean purple_email_is_valid(const char *address);

/**
 * purple_uri_list_extract_uris:
 * @uri_list: An uri-list in the standard format.
 *
 * This function extracts a list of URIs from the a "text/uri-list"
 * string.  It was "borrowed" from gnome_uri_list_extract_uris
 *
 * Returns: (element-type utf8) (transfer full): A list of strings that have
 *          been split from uri-list.
 */
GList *purple_uri_list_extract_uris(const gchar *uri_list);

/**
 * purple_uri_list_extract_filenames:
 * @uri_list: A uri-list in the standard format.
 *
 * This function extracts a list of filenames from a
 * "text/uri-list" string.  It was "borrowed" from
 * gnome_uri_list_extract_filenames
 *
 * Returns: (element-type utf8) (transfer full): A list of strings that contain
 *          the filenames in the uri-list. Note that unlike the
 *          purple_uri_list_extract_uris() function, this will discard any
 *          non-file uri from the result value.
 */
GList *purple_uri_list_extract_filenames(const gchar *uri_list);

/**
 * purple_uri_escape_for_open:
 * @unescaped: The unescaped URI.
 *
 * This function escapes any characters that might be interpreted by the shell
 * when executing a program to open a URI on some systems.
 *
 * Returns: A newly allocated string with any shell metacharacters replaced
 *          with their escaped equivalents.
 *
 * Since: 2.13.0
 */
char *purple_uri_escape_for_open(const char *unescaped);

/**************************************************************************
 * UTF8 String Functions
 **************************************************************************/

/**
 * purple_utf8_try_convert:
 * @str: The source string.
 *
 * Attempts to convert a string to UTF-8 from an unknown encoding.
 *
 * This function checks the locale and tries sane defaults.
 *
 * Returns: The UTF-8 string, or %NULL if it could not be converted.
 */
gchar *purple_utf8_try_convert(const char *str);

/**
 * purple_utf8_strip_unprintables:
 * @str: A valid UTF-8 string.
 *
 * Removes unprintable characters from a UTF-8 string. These characters
 * (in particular low-ASCII characters) are invalid in XML 1.0 and thus
 * are not allowed in XMPP and are rejected by libxml2 by default.
 *
 * The returned string must be freed by the caller.
 *
 * Returns: A newly allocated UTF-8 string without the unprintable characters.
 */
gchar *purple_utf8_strip_unprintables(const gchar *str);

/**
 * purple_gai_strerror:
 * @errnum: The error code.
 *
 * Return the UTF-8 version of #gai_strerror. It calls #gai_strerror
 * then converts the result to UTF-8. This function is analogous to
 * g_strerror().
 *
 * Returns: The UTF-8 error message.
 */
const gchar *purple_gai_strerror(gint errnum);

/**
 * purple_utf8_strcasecmp:
 * @a: The first string.
 * @b: The second string.
 *
 * Compares two UTF-8 strings case-insensitively.  This comparison is
 * more expensive than a simple g_utf8_collate() comparison because
 * it calls g_utf8_casefold() on each string, which allocates new
 * strings.
 *
 * Returns: -1 if @a is less than @b.
 *           0 if @a is equal to @b.
 *           1 if @a is greater than @b.
 */
int purple_utf8_strcasecmp(const char *a, const char *b);

/**
 * purple_utf8_has_word:
 * @haystack: The string to search in.
 * @needle:   The substring to find.
 *
 * Case insensitive search for a word in a string. The needle string
 * must be contained in the haystack string and not be immediately
 * preceded or immediately followed by another alpha-numeric character.
 *
 * Returns: TRUE if haystack has the word, otherwise FALSE
 */
gboolean purple_utf8_has_word(const char *haystack, const char *needle);

/**
 * purple_message_meify:
 * @message: The message to check
 * @len:     The message length, or -1
 *
 * Checks for messages starting (post-HTML) with "/me ", including the space.
 *
 * Returns: TRUE if it starts with "/me ", and it has been removed, otherwise
 *         FALSE
 */
gboolean purple_message_meify(char *message, gssize len);

/**
 * purple_text_strip_mnemonic:
 * @in:  The string to strip
 *
 * Removes the underscore characters from a string used identify the mnemonic
 * character.
 *
 * Returns: The stripped string
 */
char *purple_text_strip_mnemonic(const char *in);

/**
 * purple_unescape_filename:
 * @str: The string to translate.
 *
 * Does the reverse of purple_escape_filename
 *
 * This will change hex codes and such to their ascii equivalents.
 *
 * Returns: The resulting string.
 */
const char *purple_unescape_filename(const char *str);

/**
 * purple_escape_filename:
 * @str: The string to translate.
 *
 * Escapes filesystem-unfriendly characters from a filename
 *
 * Returns: The resulting string.
 */
const char *purple_escape_filename(const char *str);

/**
 * purple_value_new:
 * @type:  The type of data to be held by the GValue
 *
 * Creates a new GValue of the specified type.
 *
 * Returns:  The created GValue
 */
GValue *purple_value_new(GType type);

/**
 * purple_value_dup:
 * @value:  The GValue to duplicate
 *
 * Duplicates a GValue.
 *
 * Returns:  The duplicated GValue
 */
GValue *purple_value_dup(GValue *value);

/**
 * purple_value_free:
 * @value:  The GValue to free.
 *
 * Frees a GValue.
 */
void purple_value_free(GValue *value);

G_END_DECLS

#endif /* PURPLE_UTIL_H */
