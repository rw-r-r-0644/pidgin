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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02111-1301 USA
 */

#if !defined(PIDGIN_GLOBAL_HEADER_INSIDE) && !defined(PIDGIN_COMPILATION)
# error "only <pidgin.h> may be included directly"
#endif

#ifndef PIDGIN_GDK_PIXBUF_H
#define PIDGIN_GDK_PIXBUF_H

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <purple.h>

G_BEGIN_DECLS

/**
 * pidgin_gdk_pixbuf_new_from_image:
 * @image: A #PurpleImage instance.
 * @error: (out) (nullable): A return address for a #GError.
 *
 * Creates a new #GdkPixbuf from the #PurpleImage @image.  If provided Sets
 * @error on error.
 *
 * Returns: (transfer full) (nullable): The new #GdkPixbuf or %NULL or error.
 */
GdkPixbuf *pidgin_gdk_pixbuf_new_from_image(PurpleImage *image, GError **error);

/**
 * pidgin_gdk_pixbuf_make_round:
 * @pixbuf:  The buddy icon to transform
 *
 * Rounds the corners of a GdkPixbuf in place.
 */
void pidgin_gdk_pixbuf_make_round(GdkPixbuf *pixbuf);

/**
 * pidgin_gdk_pixbuf_is_opaque:
 * @pixbuf:  The pixbuf
 *
 * Returns TRUE if the GdkPixbuf is opaque, as determined by no
 * alpha at any of the edge pixels.
 *
 * Returns: TRUE if the pixbuf is opaque around the edges, FALSE otherwise
 */
gboolean pidgin_gdk_pixbuf_is_opaque(GdkPixbuf *pixbuf);

/**
 * pidgin_pixbuf_from_data:
 * @buf: The raw binary image data.
 * @count: The length of buf in bytes.
 *
 * Create a GdkPixbuf from a chunk of image data.
 *
 * Returns: (transfer full): A GdkPixbuf created from the image data, or NULL if
 *         there was an error parsing the data.
 */
GdkPixbuf *pidgin_pixbuf_from_data(const guchar *buf, gsize count);

/**
 * pidgin_pixbuf_anim_from_data:
 * @buf: The raw binary image data.
 * @count: The length of buf in bytes.
 *
 * Create a GdkPixbufAnimation from a chunk of image data.
 *
 * Returns: (transfer full): A GdkPixbufAnimation created from the image data, or NULL if
 *         there was an error parsing the data.
 */
GdkPixbufAnimation *pidgin_pixbuf_anim_from_data(const guchar *buf, gsize count);

/**
 * pidgin_pixbuf_from_image:
 * @image: a PurpleImage.
 *
 * Create a GdkPixbuf from a PurpleImage.
 *
 * Returns: (transfer full): a GdkPixbuf created from the @image.
 */
GdkPixbuf *
pidgin_pixbuf_from_image(PurpleImage *image);

/**
 * pidgin_pixbuf_new_from_file:
 * @filename: Name of file to load, in the GLib file name encoding
 *
 * Helper function that calls gdk_pixbuf_new_from_file() and checks both
 * the return code and the GError and returns NULL if either one failed.
 *
 * The gdk-pixbuf documentation implies that it is sufficient to check
 * the return value of gdk_pixbuf_new_from_file() to determine
 * whether the image was able to be loaded.  However, this is not the case
 * with gdk-pixbuf 2.23.3 and probably many earlier versions.  In some
 * cases a GdkPixbuf object is returned that will cause some operations
 * (like gdk_pixbuf_scale_simple()) to rapidly consume memory in an
 * infinite loop.
 *
 * This function shouldn't be necessary once Pidgin requires a version of
 * gdk-pixbuf where the aforementioned bug is fixed.  However, it might be
 * nice to keep this function around for the debug message that it logs.
 *
 * Returns: (transfer full): The GdkPixbuf if successful.  Otherwise NULL is returned and
 *         a warning is logged.
 */
GdkPixbuf *pidgin_pixbuf_new_from_file(const char *filename);

/**
 * pidgin_pixbuf_new_from_file_at_size:
 * @filename: Name of file to load, in the GLib file name encoding
 * @width: The width the image should have or -1 to not constrain the width
 * @height: The height the image should have or -1 to not constrain the height
 *
 * Helper function that calls gdk_pixbuf_new_from_file_at_size() and checks
 * both the return code and the GError and returns NULL if either one failed.
 *
 * The gdk-pixbuf documentation implies that it is sufficient to check
 * the return value of gdk_pixbuf_new_from_file_at_size() to determine
 * whether the image was able to be loaded.  However, this is not the case
 * with gdk-pixbuf 2.23.3 and probably many earlier versions.  In some
 * cases a GdkPixbuf object is returned that will cause some operations
 * (like gdk_pixbuf_scale_simple()) to rapidly consume memory in an
 * infinite loop.
 *
 * This function shouldn't be necessary once Pidgin requires a version of
 * gdk-pixbuf where the aforementioned bug is fixed.  However, it might be
 * nice to keep this function around for the debug message that it logs.
 *
 * Returns: (transfer full): The GdkPixbuf if successful.  Otherwise NULL is returned and
 *         a warning is logged.
 */
GdkPixbuf *pidgin_pixbuf_new_from_file_at_size(const char *filename, int width, int height);

/**
 * pidgin_pixbuf_new_from_file_at_scale:
 * @filename: Name of file to load, in the GLib file name encoding
 * @width: The width the image should have or -1 to not constrain the width
 * @height: The height the image should have or -1 to not constrain the height
 * @preserve_aspect_ratio: TRUE to preserve the image's aspect ratio
 *
 * Helper function that calls gdk_pixbuf_new_from_file_at_scale() and checks
 * both the return code and the GError and returns NULL if either one failed.
 *
 * The gdk-pixbuf documentation implies that it is sufficient to check
 * the return value of gdk_pixbuf_new_from_file_at_scale() to determine
 * whether the image was able to be loaded.  However, this is not the case
 * with gdk-pixbuf 2.23.3 and probably many earlier versions.  In some
 * cases a GdkPixbuf object is returned that will cause some operations
 * (like gdk_pixbuf_scale_simple()) to rapidly consume memory in an
 * infinite loop.
 *
 * This function shouldn't be necessary once Pidgin requires a version of
 * gdk-pixbuf where the aforementioned bug is fixed.  However, it might be
 * nice to keep this function around for the debug message that it logs.
 *
 * Returns: (transfer full): The GdkPixbuf if successful.  Otherwise NULL is returned and
 *         a warning is logged.
 */
GdkPixbuf *pidgin_pixbuf_new_from_file_at_scale(const char *filename, int width, int height, gboolean preserve_aspect_ratio);

/**
 * pidgin_pixbuf_scale_down:
 * @src: The source image.
 * @max_width: Maximum width in px.
 * @max_height: Maximum height in px.
 * @interp_type: Interpolation method.
 * @preserve_ratio: %TRUE to preserve image's aspect ratio.
 *
 * Scales the image to the desired dimensions. If image is smaller, it will be
 * returned without modifications.
 *
 * If new image is created, @src reference count will be decreased and new image
 * with a ref count of 1 will be returned.
 *
 * Returns: (transfer full): The image with proper sizing. %NULL in case of error.
 */
GdkPixbuf *
pidgin_pixbuf_scale_down(GdkPixbuf *src, guint max_width, guint max_height,
    GdkInterpType interp_type, gboolean preserve_ratio);



G_END_DECLS

#endif /* PIDGIN_GDK_PIXBUF_H */
