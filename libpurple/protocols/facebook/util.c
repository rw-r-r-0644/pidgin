/* purple
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include "internal.h"

#include <string.h>
#include <zlib.h>

#include "util.h"

gchar *
fb_util_locale_str(void)
{
	const gchar * const *langs;
	const gchar *lang;
	gchar *chr;
	guint i;

	static const gchar chrs[] = {'.', '@'};

	langs = g_get_language_names();
	lang = langs[0];

	if (purple_strequal(lang, "C")) {
		return g_strdup("en_US");
	}

	for (i = 0; i < G_N_ELEMENTS(chrs); i++) {
		chr = strchr(lang, chrs[i]);

		if (chr != NULL) {
			return g_strndup(lang, chr - lang);
		}
	}

	return g_strdup(lang);
}

gchar *
fb_util_randstr(gsize size)
{
	gchar *ret;
	GRand *rand;
	guint i;
	guint j;

	static const gchar chars[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789";
	static const gsize charc = G_N_ELEMENTS(chars) - 1;

	if (G_UNLIKELY(size < 1)) {
		return NULL;
	}

	rand = g_rand_new();
	ret = g_new(gchar, size + 1);

	for (i = 0; i < size; i++) {
		j = g_rand_int_range(rand, 0, charc);
		ret[i] = chars[j];
	}

	ret[size] = 0;
	g_rand_free(rand);
	return ret;
}

static void
fb_util_request_buddy_ok(gpointer *mata, PurpleRequestFields *fields)
{
	FbUtilRequestBuddyFunc func = mata[0];
	GList *l;
	GList *select;
	gpointer data = mata[2];
	GSList *ret = NULL;
	PurpleBuddy *bdy;
	PurpleRequestField *field;

	if (func == NULL) {
		g_free(mata);
		return;
	}

	field = purple_request_fields_get_field(fields, "buddy");
	select = purple_request_field_list_get_selected(field);

	for (l = select; l != NULL; l = l->next) {
		bdy = purple_request_field_list_get_data(field, l->data);
		ret = g_slist_prepend(ret, bdy);
	}

	ret = g_slist_reverse(ret);
	func(ret, data);

	g_slist_free(ret);
	g_free(mata);
}

static void
fb_util_request_buddy_cancel(gpointer *mata, PurpleRequestFields *fields)
{
	FbUtilRequestBuddyFunc func = mata[1];
	gpointer data = mata[2];

	if (func != NULL) {
		func(NULL, data);
	}

	g_free(mata);
}

gpointer
fb_util_request_buddy(PurpleConnection *gc, const gchar *title,
                      const gchar *primary, const gchar *secondary,
                      GSList *select, gboolean multi, GCallback ok_cb,
		      GCallback cancel_cb, gpointer data)
{
	const gchar *alias;
	const gchar *name;
	gchar *str;
	GList *items = NULL;
	gpointer *mata;
	GSList *buddies;
	GSList *l;
	PurpleAccount *acct;
	PurpleRequestCommonParameters *cpar;
	PurpleRequestField *field;
	PurpleRequestFieldGroup *group;
	PurpleRequestFields *fields;

	mata = g_new0(gpointer, 3);
	mata[0] = ok_cb;
	mata[1] = cancel_cb;
	mata[2] = data;

	acct = purple_connection_get_account(gc);
	buddies = purple_blist_find_buddies(acct, NULL);
	buddies = g_slist_sort(buddies, (GCompareFunc) g_ascii_strcasecmp);

	fields = purple_request_fields_new();
	group = purple_request_field_group_new(NULL);
	purple_request_fields_add_group(fields, group);

	field = purple_request_field_list_new("buddy", NULL);
	purple_request_field_list_set_multi_select(field, multi);
	purple_request_field_set_required(field, TRUE);
	purple_request_field_group_add_field(group, field);

	for (l = buddies; l != NULL; l = l->next) {
		name = purple_buddy_get_name(l->data);
		alias = purple_buddy_get_alias(l->data);
		str = g_strdup_printf("%s (%s)", alias, name);
		purple_request_field_list_add_icon(field, str, NULL, l->data);
		g_free(str);
	}

	for (l = select; l != NULL; l = l->next) {
		name = purple_buddy_get_name(l->data);
		alias = purple_buddy_get_alias(l->data);
		str = g_strdup_printf("%s (%s)", alias, name);
		items = g_list_append(items, str);
	}

	purple_request_field_list_set_selected(field, items);
	g_slist_free(buddies);
	g_list_free_full(items, g_free);

	cpar = purple_request_cpar_from_connection(gc);
	return purple_request_fields(gc, title, primary, secondary, fields,
	                             _("Ok"),
	                             G_CALLBACK(fb_util_request_buddy_ok),
				     _("Cancel"),
	                             G_CALLBACK(fb_util_request_buddy_cancel),
				     cpar, mata);
}

gboolean
fb_util_str_is(const gchar *str, GAsciiType type)
{
	gsize i;
	gsize size;
	guchar c;

	g_return_val_if_fail(str != NULL, FALSE);
	size = strlen(str);

	for (i = 0; i < size; i++) {
		c = (guchar) str[i];

		if ((g_ascii_table[c] & type) == 0) {
			return FALSE;
		}
	}

	return TRUE;
}

static voidpf
fb_util_zalloc(voidpf opaque, uInt items, uInt size)
{
	return g_malloc(size * items);
}

static void
fb_util_zfree(voidpf opaque, voidpf address)
{
	g_free(address);
}

gboolean
fb_util_zcompressed(const GByteArray *bytes)
{
	guint8 b0;
	guint8 b1;

	g_return_val_if_fail(bytes != NULL, FALSE);

	if (bytes->len < 2) {
		return FALSE;
	}

	b0 = *(bytes->data + 0);
	b1 = *(bytes->data + 1);

	return ((((b0 << 8) | b1) % 31) == 0) && /* Check the header */
	       ((b0 & 0x0F) == Z_DEFLATED);      /* Check the method */
}

GByteArray *
fb_util_zcompress(const GByteArray *bytes)
{
	GByteArray *ret;
	gint res;
	gsize size;
	z_stream zs;

	g_return_val_if_fail(bytes != NULL, NULL);

	memset(&zs, 0, sizeof zs);
	zs.zalloc = fb_util_zalloc;
	zs.zfree = fb_util_zfree;
	zs.next_in = bytes->data;
	zs.avail_in = bytes->len;

	if (deflateInit(&zs, Z_BEST_COMPRESSION) != Z_OK) {
		return NULL;
	}

	size = compressBound(bytes->len);
	ret = g_byte_array_new();

	g_byte_array_set_size(ret, size);

	zs.next_out = ret->data;
	zs.avail_out = size;

	res = deflate(&zs, Z_FINISH);

	if (res != Z_STREAM_END) {
		deflateEnd(&zs);
		g_byte_array_free(ret, TRUE);
		return NULL;
	}

	size -= zs.avail_out;
	g_byte_array_remove_range(ret, size, ret->len - size);

	deflateEnd(&zs);
	return ret;
}

GByteArray *
fb_util_zuncompress(const GByteArray *bytes)
{
	GByteArray *ret;
	gint res;
	guint8 out[1024];
	z_stream zs;

	g_return_val_if_fail(bytes != NULL, NULL);

	memset(&zs, 0, sizeof zs);
	zs.zalloc = fb_util_zalloc;
	zs.zfree = fb_util_zfree;
	zs.next_in = bytes->data;
	zs.avail_in = bytes->len;

	if (inflateInit(&zs) != Z_OK) {
		return NULL;
	}

	ret = g_byte_array_new();

	do {
		zs.next_out = out;
		zs.avail_out = sizeof out;

		res = inflate(&zs, Z_NO_FLUSH);

		if ((res != Z_OK) && (res != Z_STREAM_END)) {
			inflateEnd(&zs);
			g_byte_array_free(ret, TRUE);
			return NULL;
		}

		g_byte_array_append(ret, out, sizeof out - zs.avail_out);
	} while (res != Z_STREAM_END);

	inflateEnd(&zs);
	return ret;
}