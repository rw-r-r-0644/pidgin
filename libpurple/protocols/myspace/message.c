/** MySpaceIM protocol messages
 *
 * \author Jeff Connelly
 *
 * Copyright (C) 2007, Jeff Connelly <jeff2@homing.pidgin.im>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "myspace.h"
#include "message.h"

static void msim_msg_free_element(gpointer data, gpointer user_data);
static void msim_msg_debug_string_element(gpointer data, gpointer user_data);
static gchar *msim_msg_pack_using(MsimMessage *msg, GFunc gf, gchar *sep, gchar *begin, gchar *end);
static gchar *msim_msg_element_pack(MsimMessageElement *elem);

/** Create a new MsimMessage. */
MsimMessage *msim_msg_new(void)
{
	/* Just an empty list. */
	return NULL;
}

/** Clone an individual element.
 *
 * @param data MsimMessageElement * to clone.
 * @param user_data Pointer to MsimMessage * to add cloned element to.
 */
static void msim_msg_clone_element(gpointer data, gpointer user_data)
{
	MsimMessageElement *elem;
	MsimMessage **new;
	gpointer new_data;

	elem = (MsimMessageElement *)data;
	new = (MsimMessage **)user_data;

	switch (elem->type)
	{
		case MSIM_TYPE_BOOLEAN:
		case MSIM_TYPE_INTEGER:
			new_data = elem->data;
			break;

		case MSIM_TYPE_RAW:
		case MSIM_TYPE_STRING:
			new_data = g_strdup((gchar *)elem->data);
			break;

		case MSIM_TYPE_BINARY:
			{
				GString *gs;

				gs = (GString *)elem->data;

				new_data = g_string_new_len(gs->str, gs->len);
			}
			break;
		/* TODO: other types */
		default:
			purple_debug_info("msim", "msim_msg_clone_element: unknown type %d (%c)\n", elem->type, elem->type);
			g_return_if_fail(NULL);
	}

	/* Append cloned data. Note that the 'name' field is a static string, so it
	 * never needs to be copied nor freed. */
	*new = msim_msg_append(*new, elem->name, elem->type, new_data);
}

/** Clone an existing MsimMessage. 
 *
 * @return Cloned message; caller should free with msim_msg_free().
 */
MsimMessage *msim_msg_clone(MsimMessage *old)
{
	MsimMessage *new;

	if (!old)
		return NULL;

	new = msim_msg_new();

	g_list_foreach(old, msim_msg_clone_element, &new);

	return new;
}

/** Free an individual message element. 
 *
 * @param data MsimMessageElement * to free.
 * @param user_data Not used; required to match g_list_foreach() callback prototype.
 */
static void msim_msg_free_element(gpointer data, gpointer user_data)
{
	MsimMessageElement *elem;

	elem = (MsimMessageElement *)data;

	switch (elem->type)
	{
		case MSIM_TYPE_BOOLEAN:
		case MSIM_TYPE_INTEGER:
			/* Integer value stored in gpointer - no need to free(). */
			break;

		case MSIM_TYPE_RAW:
		case MSIM_TYPE_STRING:
			/* Always free strings - caller should have g_strdup()'d if
			 * string was static or temporary and not to be freed. */
			g_free(elem->data);
			break;

		case MSIM_TYPE_BINARY:
			/* Free the GString itself and the binary data. */
			g_string_free((GString *)elem->data, TRUE);
			break;

		case MSIM_TYPE_DICTIONARY:
			/* TODO: free dictionary */
			break;
			
		case MSIM_TYPE_LIST:
			/* TODO: free list */
			break;

		default:
			purple_debug_info("msim", "msim_msg_free_element: not freeing unknown type %d (%c)\n",
					elem->type, elem->type);
			break;
	}

	g_free(elem);
}

/** Free a complete message. */
void msim_msg_free(MsimMessage *msg)
{
	if (!msg)
	{
		/* already free as can be */
		return;
	}

	g_list_foreach(msg, msim_msg_free_element, NULL);
	g_list_free(msg);
}

/** Send an existing MsimMessage. */
gboolean msim_msg_send(MsimSession *session, MsimMessage *msg)
{
	gchar *raw;
	gboolean success;
	
	raw = msim_msg_pack(msg);
	success = msim_send_raw(session, raw);
	g_free(raw);
	
	return success;
}

/**
 *
 * Send a message to the server, whose contents is specified using 
 * variable arguments.
 *
 * @param session
 * @param ... A sequence of gchar* key/type/value triplets, terminated with NULL. 
 *
 * This function exists for coding convenience: it allows a message to be created
 * and sent in one line of code. Internally it calls msim_msg_send(). 
 *
 * IMPORTANT: See msim_msg_append() documentation for details on element types.
 *
 */
gboolean msim_send(MsimSession *session, ...)
{
	va_list argp;
	gchar *key, *value;
	MsimMessageType type;
	gboolean success;
	MsimMessage *msg;
	GString *gs;
    
	msg = msim_msg_new();

	/* Read key, type, value triplets until NULL. */
	va_start(argp, session);
	do
	{
		key = va_arg(argp, gchar *);
		if (!key)
		{
			break;
		}

		type = va_arg(argp, int);

		/* Interpret variadic arguments. */
		switch (type)
		{
			case MSIM_TYPE_INTEGER: 
				msg = msim_msg_append(msg, key, type, GUINT_TO_POINTER(va_arg(argp, int)));
				break;
				
			case MSIM_TYPE_STRING:
				value = va_arg(argp, char *);

				g_return_val_if_fail(value != NULL, FALSE);

				msg = msim_msg_append(msg, key, type, value);
				break;

			case MSIM_TYPE_BINARY:
				gs = va_arg(argp, GString *);

				g_return_val_if_fail(gs != NULL, FALSE);

				/* msim_msg_free() will free this GString the caller created. */
				msg = msim_msg_append(msg, key, type, gs);
				break;

			default:
				purple_debug_info("msim", "msim_send: unknown type %d (%c)\n", type, type);
				break;
		}
	} while(key);

	/* Actually send the message. */
	success = msim_msg_send(session, msg);

	/* Cleanup. */
	va_end(argp);	
	msim_msg_free(msg);

	return success;
}


/** Append a new element to a message. 
 *
 * @param name Textual name of element (static string, neither copied nor freed).
 * @param type An MSIM_TYPE_* code.
 * @param data Pointer to data, see below.
 *
 * @return The new message - must be assigned to as with GList*. For example:
 *
 * 		msg = msim_msg_append(msg, ...)
 *
 * The data parameter depends on the type given:
 *
 * * MSIM_TYPE_INTEGER: Use GUINT_TO_POINTER(x).
 *
 * * MSIM_TYPE_BINARY: Same as integer, non-zero is TRUE and zero is FALSE.
 *
 * * MSIM_TYPE_STRING: gchar *. The data WILL BE FREED - use g_strdup() if needed.
 *
 * * MSIM_TYPE_RAW: gchar *. The data WILL BE FREED - use g_strdup() if needed.
 *
 * * MSIM_TYPE_BINARY: g_string_new_len(data, length). The data AND GString will be freed.
 *
 * * MSIM_TYPE_DICTIONARY: TODO
 *
 * * MSIM_TYPE_LIST: TODO
 *
 * */
MsimMessage *msim_msg_append(MsimMessage *msg, gchar *name, MsimMessageType type, gpointer data)
{
	MsimMessageElement *elem;

	elem = g_new0(MsimMessageElement, 1);

	elem->name = name;
	elem->type = type;
	elem->data = data;

	return g_list_append(msg, elem);
}

/** Pack a string using the given GFunc and seperator.
 * Used by msim_msg_debug_string() and msim_msg_pack().
 */
static gchar *msim_msg_pack_using(MsimMessage *msg, GFunc gf, gchar *sep, gchar *begin, gchar *end)
{
	gchar **strings;
	gchar **strings_tmp;
	gchar *joined;
	gchar *final;
	int i;

	g_return_val_if_fail(msg != NULL, NULL);

	/* Add one for NULL terminator for g_strjoinv(). */
	strings = (gchar **)g_new0(gchar *, g_list_length(msg) + 1);

	strings_tmp = strings;
	g_list_foreach(msg, gf, &strings_tmp);

	joined = g_strjoinv(sep, strings);
	final = g_strconcat(begin, joined, end, NULL);
	g_free(joined);

	/* Clean up. */
	for (i = 0; i < g_list_length(msg); ++i)
	{
		g_free(strings[i]);
	}

	g_free(strings);

	return final;
}
/** Store a human-readable string describing the element.
 *
 * @param data Pointer to an MsimMessageElement.
 * @param user_data 
 */
static void msim_msg_debug_string_element(gpointer data, gpointer user_data)
{
	MsimMessageElement *elem;
	gchar *string;
	GString *gs;
	gchar *binary;
	gchar ***items;	 	/* wow, a pointer to a pointer to a pointer */

	elem = (MsimMessageElement *)data;
	items = user_data;

	switch (elem->type)
	{
		case MSIM_TYPE_INTEGER:
			string = g_strdup_printf("%s(integer): %d", elem->name, GPOINTER_TO_UINT(elem->data));
			break;

		case MSIM_TYPE_RAW:
			string = g_strdup_printf("%s(raw): %s", elem->name, (gchar *)elem->data);
			break;

		case MSIM_TYPE_STRING:
			string = g_strdup_printf("%s(string): %s", elem->name, (gchar *)elem->data);
			break;

		case MSIM_TYPE_BINARY:
			gs = (GString *)elem->data;
			binary = purple_base64_encode((guchar*)gs->str, gs->len);
			string = g_strdup_printf("%s(binary, %d bytes): %s", elem->name, (int)gs->len, binary);
			g_free(binary);
			break;

		case MSIM_TYPE_BOOLEAN:
			string = g_strdup_printf("%s(boolean): %s", elem->name,
					GPOINTER_TO_UINT(elem->data) ? "TRUE" : "FALSE");
			break;

		case MSIM_TYPE_DICTIONARY:
			/* TODO: provide human-readable output of dictionary. */
			string = g_strdup_printf("%s(dict): TODO", elem->name);
			break;
			
		case MSIM_TYPE_LIST:
			/* TODO: provide human-readable output of list. */
			string = g_strdup_printf("%s(list): TODO", elem->name);
			break;

		default:
			string = g_strdup_printf("%s(unknown type %d (%c)", elem->name, elem->type);
			break;
	}

	**items = string;
	++(*items);
}

/** Return a human-readable string of the message.
 *
 * @return A string. Caller must g_free().
 */
gchar *msim_msg_debug_string(MsimMessage *msg)
{
	if (!msg)
	{
		return g_strdup("<MsimMessage: empty>");
	}

	return msim_msg_pack_using(msg, msim_msg_debug_string_element, "\n", "<MsimMessage: \n", "\n/MsimMessage>");
}

/** Return a message element data as a new string for a raw protocol message, converting from other types (integer, etc.) if necessary.
 *
 * @return gchar * The data as a string, or NULL. Caller must g_free().
 *
 * Returns a string suitable for inclusion in a raw protocol message, not necessarily
 * optimal for human consumption. For example, strings are escaped. Use 
 * msim_msg_get_string() if you want a string, which in some cases is same as this.
 */
static gchar *msim_msg_element_pack(MsimMessageElement *elem)
{
	switch (elem->type)
	{
		case MSIM_TYPE_INTEGER:
			return g_strdup_printf("%d", GPOINTER_TO_UINT(elem->data));

		case MSIM_TYPE_RAW:
			/* Not un-escaped - this is a raw element, already escaped if necessary. */
			return (gchar *)elem->data;

		case MSIM_TYPE_STRING:
			/* Strings get escaped. msim_escape() creates a new string. */
			return msim_escape((gchar *)elem->data);

		case MSIM_TYPE_BINARY:
			{
				GString *gs;

				gs = (GString *)elem->data;
				/* Do not escape! */
				return purple_base64_encode((guchar *)gs->str, gs->len);
			}

		case MSIM_TYPE_BOOLEAN:
			/* These strings are not actually used by the wire protocol
			 * -- see msim_msg_pack_element. */
			return g_strdup(GPOINTER_TO_UINT(elem->data) ? "True" : "False");

		case MSIM_TYPE_DICTIONARY:
			/* TODO: pack using k=v\034k2=v2\034... */
			return NULL;
			
		case MSIM_TYPE_LIST:
			/* TODO: pack using a|b|c|d|... */
			return NULL;

		default:
			purple_debug_info("msim", "field %s, unknown type %d (%c)\n", elem->name, elem->type, elem->type);
			return NULL;
	}
}

/** Pack an element into its protocol representation. 
 *
 * @param data Pointer to an MsimMessageElement.
 * @param user_data Pointer to a gchar ** array of string items.
 *
 * Called by msim_msg_pack(). Will pack the MsimMessageElement into
 * a part of the protocol string and append it to the array. Caller
 * is responsible for creating array to correct dimensions, and
 * freeing each string element of the array added by this function.
 */
static void msim_msg_pack_element(gpointer data, gpointer user_data)
{
	MsimMessageElement *elem;
	gchar *string, *data_string;
	gchar ***items;

	elem = (MsimMessageElement *)data;
	items = user_data;

	data_string = msim_msg_element_pack(elem);

	switch (elem->type)
	{
		/* These types are represented by key name/value pairs (converted above). */
		case MSIM_TYPE_INTEGER:
		case MSIM_TYPE_RAW:
		case MSIM_TYPE_STRING:
		case MSIM_TYPE_BINARY:
		case MSIM_TYPE_DICTIONARY:
		case MSIM_TYPE_LIST:
			string = g_strconcat(elem->name, "\\", data_string, NULL);
			break;

		/* Boolean is represented by absence or presence of name. */
		case MSIM_TYPE_BOOLEAN:
			if (GPOINTER_TO_UINT(elem->data))
			{
				/* True - leave in, with blank value. */
				string = g_strdup_printf("%s\\\\", elem->name);
			} else {
				/* False - leave out. */
				string = g_strdup("");
			}
			break;

		default:
			g_free(data_string);
			g_return_if_fail(FALSE);
			break;
	}

	g_free(data_string);

	**items = string;
	++(*items);
}


/** Return a packed string suitable for sending over the wire.
 *
 * @return A string. Caller must g_free().
 */
gchar *msim_msg_pack(MsimMessage *msg)
{
	g_return_val_if_fail(msg != NULL, NULL);

	return msim_msg_pack_using(msg, msim_msg_pack_element, "\\", "\\", "\\final\\");
}

/** 
 * Parse a raw protocol message string into a MsimMessage *.
 *
 * @param raw The raw message string to parse, will be g_free()'d.
 *
 * @return MsimMessage *. Caller should msim_msg_free() when done.
 */
MsimMessage *msim_parse(gchar *raw)
{
	MsimMessage *msg;
    gchar *token;
    gchar **tokens;
    gchar *key;
    gchar *value;
    int i;

    g_return_val_if_fail(raw != NULL, NULL);

    purple_debug_info("msim", "msim_parse: got <%s>\n", raw);

    key = NULL;

    /* All messages begin with a \. */
    if (raw[0] != '\\' || raw[1] == 0)
    {
        purple_debug_info("msim", "msim_parse: incomplete/bad string, "
                "missing initial backslash: <%s>\n", raw);
        /* XXX: Should we try to recover, and read to first backslash? */

        g_free(raw);
        return NULL;
    }

    msg = msim_msg_new();

    for (tokens = g_strsplit(raw + 1, "\\", 0), i = 0; 
            (token = tokens[i]);
            i++)
    {
#ifdef MSIM_DEBUG_PARSE
        purple_debug_info("msim", "tok=<%s>, i%2=%d\n", token, i % 2);
#endif
        if (i % 2)
        {
			/* Odd-numbered ordinal is a value. */

			value = token;
		
			/* Incoming protocol messages get tagged as MSIM_TYPE_RAW, which
			 * represents an untyped piece of data. msim_msg_get_* will
			 * convert to appropriate types for caller, and handle unescaping if needed. */
			msg = msim_msg_append(msg, g_strdup(key), MSIM_TYPE_RAW, g_strdup(value));
#ifdef MSIM_DEBUG_PARSE
			purple_debug_info("msim", "insert string: |%s|=|%s|\n", key, value);
#endif
        } else {
			/* Even numbered indexes are key names. */
            key = token;
        }
    }
    g_strfreev(tokens);

    /* Can free now since all data was copied to hash key/values */
    g_free(raw);

    return msg;
}

/**
 * Parse a \x1c-separated "dictionary" of key=value pairs into a hash table.
 *
 * @param body_str The text of the dictionary to parse. Often the
 *                  value for the 'body' field.
 *
 * @return Hash table of the keys and values. Must g_hash_table_destroy() when done.
 */
GHashTable *msim_parse_body(const gchar *body_str)
{
    GHashTable *table;
    gchar *item;
    gchar **items;
    gchar **elements;
    guint i;

    g_return_val_if_fail(body_str != NULL, NULL);

    table = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
 
    for (items = g_strsplit(body_str, "\x1c", 0), i = 0; 
        (item = items[i]);
        i++)
    {
        gchar *key, *value;

        elements = g_strsplit(item, "=", 2);

        key = elements[0];
        if (!key)
        {
            purple_debug_info("msim", "msim_parse_body(%s): null key\n", 
					body_str);
            g_strfreev(elements);
            break;
        }

        value = elements[1];
        if (!value)
        {
            purple_debug_info("msim", "msim_parse_body(%s): null value\n", 
					body_str);
            g_strfreev(elements);
            break;
        }

#ifdef MSIM_DEBUG_PARSE
        purple_debug_info("msim", "-- %s: %s\n", key, value);
#endif

        /* XXX: This overwrites duplicates. */
        /* TODO: make the GHashTable values be GList's, and append to the list if 
         * there is already a value of the same key name. This is important for
         * the WebChallenge message. */
        g_hash_table_insert(table, g_strdup(key), g_strdup(value));
        
        g_strfreev(elements);
    }

    g_strfreev(items);

    return table;
}

/** Return the first MsimMessageElement * with given name in the MsimMessage *. 
 *
 * @param name Name to search for.
 *
 * @return MsimMessageElement * matching name, or NULL.
 *
 * Note: useful fields of MsimMessageElement are 'data' and 'type', which
 * you can access directly. But it is often more convenient to use
 * another msim_msg_get_* that converts the data to what type you want.
 */
MsimMessageElement *msim_msg_get(MsimMessage *msg, gchar *name)
{
	GList *i;

	/* Linear search for the given name. O(n) but n is small. */
	for (i = g_list_first(msg); i != NULL; i = g_list_next(i))
	{
		MsimMessageElement *elem;

		elem = i->data;
		g_return_val_if_fail(elem != NULL, NULL);

		if (strcmp(elem->name, name) == 0)
			return elem;
	}
	return NULL;
}

/** Return the data of an element of a given name, as a string.
 *
 * @param name Name of element.
 *
 * @return gchar * The data as a string. Caller must g_free().
 *
 * Note that msim_msg_element_pack() is similar, but returns a string
 * for inclusion into a raw protocol string (escaped and everything).
 * This function unescapes the string for you, if needed.
 */
gchar *msim_msg_get_string(MsimMessage *msg, gchar *name)
{
	MsimMessageElement *elem;

	elem = msim_msg_get(msg, name);
	if (!elem)
		return NULL;

	switch (elem->type)
	{
		case MSIM_TYPE_INTEGER:
			return g_strdup_printf("%d", GPOINTER_TO_UINT(elem->data));

		case MSIM_TYPE_RAW:
			/* Raw element from incoming message - if its a string, it'll
			 * be escaped. */
			return msim_unescape((gchar *)elem->data);

		case MSIM_TYPE_STRING:
			/* Already unescaped. */
			return (gchar *)elem->data;

		default:
			purple_debug_info("msim", "msim_msg_get_string: type %d unknown, name %s\n",
					elem->type, name);
			return NULL;
	}
}

/** Return the data of an element of a given name, as an integer.
 *
 * @param name Name of element.
 *
 * @return guint Numeric representation of data, or 0 if could not be converted.
 *
 * Useful to obtain an element's data if you know it should be an integer,
 * even if it is not stored as an MSIM_TYPE_INTEGER. MSIM_TYPE_STRING will
 * be converted handled correctly, for example.
 */
guint msim_msg_get_integer(MsimMessage *msg, gchar *name)
{
	MsimMessageElement *elem;

	elem = msim_msg_get(msg, name);

	switch (elem->type)
	{
		case MSIM_TYPE_INTEGER:
			return GPOINTER_TO_UINT(elem->data);

		case MSIM_TYPE_RAW:
		case MSIM_TYPE_STRING:
			/* TODO: find out if we need larger integers */
			return (guint)atoi((gchar *)elem->data);

		default:
			return 0;
	}
}

/** Return the data of an element of a given name, as a binary GString.
 *
 * @param binary_data A pointer to a new pointer, which will be filled in with the binary data. CALLER MUST g_free().
 *
 * @param binary_length A pointer to an integer, which will be set to the binary data length.
 *
 * @return TRUE if successful, FALSE if not.
 */
gboolean msim_msg_get_binary(MsimMessage *msg, gchar *name, gchar **binary_data, gsize *binary_length)
{
	MsimMessageElement *elem;

	elem = msim_msg_get(msg, name);

	switch (elem->type)
	{
		case MSIM_TYPE_RAW:
			 /* Incoming messages are tagged with MSIM_TYPE_RAW, and
			 * converted appropriately. They can still be "strings", just they won't
			 * be tagged as MSIM_TYPE_STRING (as MSIM_TYPE_STRING is intended to be used
			 * by msimprpl code for things like instant messages - stuff that should be
			 * escaped if needed). DWIM.
			 */
	
			/* Previously, incoming messages were stored as MSIM_TYPE_STRING.
			 * This was fine for integers and strings, since they can easily be
			 * converted in msim_get_*, as desirable. However, it does not work
			 * well for binary strings. Consider:
			 *
			 * If incoming base64'd elements were tagged as MSIM_TYPE_STRING.
			 * msim_msg_get_binary() sees MSIM_TYPE_STRING, base64 decodes, returns.
			 * everything is fine.
			 * But then, msim_send() is called on the incoming message, which has
			 * a base64'd MSIM_TYPE_STRING that really is encoded binary. The values
			 * will be escaped since strings are escaped, and / becomes /2; no good.
			 *
			 */
			*binary_data = (gchar *)purple_base64_decode((const gchar *)elem->data, binary_length);
			return TRUE;

		case MSIM_TYPE_BINARY:
			{
				GString *gs;

				gs = (GString *)elem->data;

				/* Duplicate data, so caller can g_free() it. */
				*binary_data = g_new0(char, gs->len);
				memcpy(*binary_data, gs->str, gs->len);

				*binary_length = gs->len;

				return TRUE;
			}


			/* Rejected because if it isn't already a GString, have to g_new0 it and
			 * then caller has to ALSO free the GString! 
			 *
			 * return (GString *)elem->data; */

		default:
			purple_debug_info("msim", "msim_msg_get_binary: unhandled type %d for key %s\n",
					elem->type, name);
			return FALSE;
	}
}
