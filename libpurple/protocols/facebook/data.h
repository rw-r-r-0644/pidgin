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

#ifndef PURPLE_FACEBOOK_DATA_H
#define PURPLE_FACEBOOK_DATA_H

#include <glib.h>

#include <purple.h>

#include "api.h"
#include "http.h"
#include "id.h"

#define FB_TYPE_DATA  fb_data_get_type()
#define FB_TYPE_DATA_IMAGE  fb_data_image_get_type()

/**
 * FB_DATA_ICON_MAX:
 *
 * The maximum of number of concurrent icon fetches.
 */
#define FB_DATA_ICON_MAX  4

/**
 * FB_DATA_ICON_SIZE_MAX:
 *
 * The maximum size of a fetched icon.
 */
#define FB_DATA_ICON_SIZE_MAX  0xa00000 /* 10MiB */

G_DECLARE_FINAL_TYPE(FbData, fb_data, FB, DATA, GObject)

G_DECLARE_FINAL_TYPE(FbDataImage, fb_data_image, FB, DATA_IMAGE, GObject)

/**
 * FbDataImageFunc:
 * @img: The #FbDataImage.
 * @error: The #GError or #NULL.
 *
 * The callback for a fetched #FbDataImage.
 */
typedef void (*FbDataImageFunc) (FbDataImage *img, GError *error);

/**
 * fb_data_new:
 * @gc: The #PurpleConnection.
 * @resolver: The proxy resolver to use from the account.
 *
 * Creates a new #FbData. The returned #FbData should be freed with
 * #g_object_unref() when no longer needed.
 *
 * Returns: The new #FbData.
 */
FbData *fb_data_new(PurpleConnection *gc, GProxyResolver *resolver);

/**
 * fb_data_load:
 * @fata: The #FbData.
 *
 * Loads the internal data from the underlying #PurpleAccount.
 *
 * Return: #TRUE if all of the data was loaded, otherwise #FALSE.
 */
gboolean
fb_data_load(FbData *fata);

/**
 * fb_data_save:
 * @fata: The #FbData.
 *
 * Saves the internal data to the underlying #PurpleAccount.
 */
void
fb_data_save(FbData *fata);

/**
 * fb_data_save_timeout:
 * @fata: The #FbData.
 * @name: The name of the timeout.
 * @id: The source id of the timeout.
 *
 * Saves a new callback timer. The callback should be added to the main loop
 * with `g_timeout_add` or similar, and the returned source identifier passed
 * to this function. The timeout should be cleared with
 * #fb_data_clear_timeout() when no longer needed.
 */
void
fb_data_save_timeout(FbData *fata, const gchar *name, guint id);

/**
 * fb_data_clear_timeout:
 * @fata: The #FbData.
 * @name: The name of the timeout.
 * @remove: #TRUE to remove from the event loop, otherwise #FALSE.
 *
 * Clears and removes a callback timer. The only time @remove should be
 * #FALSE, is when being called from a #GSourceFunc, which is returning
 * #FALSE.
 */
void
fb_data_clear_timeout(FbData *fata, const gchar *name, gboolean remove);

/**
 * fb_data_get_api:
 * @fata: The #FbData.
 *
 * Gets the #FbApi from the #FbData.
 *
 * Return: (transfer none): The #FbApi.
 */
FbApi *
fb_data_get_api(FbData *fata);

/**
 * fb_data_get_connection:
 * @fata: The #FbData.
 *
 * Gets the #PurpleConnection from the #FbData.
 *
 * Return: (transfer none): The #PurpleConnection.
 */
PurpleConnection *
fb_data_get_connection(FbData *fata);

/**
 * fb_data_get_roomlist:
 * @fata: The #FbData.
 *
 * Gets the #PurpleRoomlist from the #FbData.
 *
 * Return: (transfer none): The #PurpleRoomlist.
 */
PurpleRoomlist *
fb_data_get_roomlist(FbData *fata);

/**
 * fb_data_get_unread:
 * @fata: The #FbData.
 * @id: The #FbId.
 *
 * Gets the unread state of an #FbId.
 *
 * Return: #TRUE if the #FbId is unread, otherwise #FALSE.
 */
gboolean
fb_data_get_unread(FbData *fata, FbId id);

/**
 * fb_data_set_roomlist:
 * @fata: The #FbData.
 * @list: The #PurpleRoomlist.
 *
 * Sets the #PurpleRoomlist to the #FbData.
 */
void
fb_data_set_roomlist(FbData *fata, PurpleRoomlist *list);

/**
 * fb_data_set_unread:
 * @fata: The #FbData.
 * @id: The #FbId.
 * @unread: #TRUE if the #FbId is unread, otherwise #FALSE.
 *
 * Sets the unread state of an #FbId to the #FbData.
 */
void
fb_data_set_unread(FbData *fata, FbId id, gboolean unread);

/**
 * fb_data_add_message:
 * @fata: The #FbData.
 * @msg: The #FbApiMessage.
 *
 * Adds an #FbApiMessage to the #FbData.
 */
void
fb_data_add_message(FbData *fata, FbApiMessage *msg);

/**
 * fb_data_remove_message:
 * @fata: The #FbData.
 * @msg: The #FbApiMessage.
 *
 * Removes an #FbApiMessage from the #FbData.
 */
void
fb_data_remove_message(FbData *fata, FbApiMessage *msg);

/**
 * fb_data_take_messages:
 * @fata: The #FbData.
 * @uid: The user #FbId.
 *
 * Gets a #GSList of messages by the user #FbId from the #FbData. The
 * #FbApiMessage's are removed from the #FbData.
 *
 * Returns: (element-type FbApiMessage) (transfer full): The messages from the
 *          user.
 */
GSList *
fb_data_take_messages(FbData *fata, FbId uid);

/**
 * fb_data_image_add:
 * @fata: The #FbData.
 * @url: The image URL.
 * @func: The #FbDataImageFunc.
 * @data: The user-defined data.
 * @dunc: The #GDestroyNotify for @data or #NULL.
 *
 * Adds a new #FbDataImage to the #FbData. This is used to fetch images
 * from HTTP sources. After calling this, #fb_data_image_queue() should
 * be called to queue the fetching process.
 *
 * Return: (transfer none): The #FbDataImage.
 */
FbDataImage *
fb_data_image_add(FbData *fata, const gchar *url, FbDataImageFunc func,
                  gpointer data, GDestroyNotify dunc);

/**
 * fb_data_image_get_active:
 * @img: The #FbDataImage.
 *
 * Gets the active fetching state from the #FbDataImage.
 *
 * Returns: #TRUE if the image is being fetched, otherwise #FALSE.
 */
gboolean
fb_data_image_get_active(FbDataImage *img);

/**
 * fb_data_image_get_data:
 * @img: The #FbDataImage.
 *
 * Gets the user-defined data from the #FbDataImage.
 *
 * Returns: The user-defined data.
 */
gpointer
fb_data_image_get_data(FbDataImage *img);

/**
 * fb_data_image_get_fata:
 * @img: The #FbDataImage.
 *
 * Gets the #FbData from the #FbDataImage.
 *
 * Returns: (transfer none): The #FbData.
 */
FbData *
fb_data_image_get_fata(FbDataImage *img);

/**
 * fb_data_image_get_image:
 * @img: The #FbDataImage.
 * @size: The return location for the image size or #NULL.
 *
 * Gets the image data from the #FbDataImage.
 *
 * Returns: The image data.
 */
const guint8 *
fb_data_image_get_image(FbDataImage *img, gsize *size);

/**
 * fb_data_image_dup_image:
 * @img: The #FbDataImage.
 * @size: The return location for the image size or #NULL.
 *
 * Gets the duplicated image data from the #FbDataImage. The returned
 * data should be freed with #g_free() when no longer needed.
 *
 * Returns: The duplicated image data.
 */
guint8 *
fb_data_image_dup_image(FbDataImage *img, gsize *size);

/**
 * fb_data_image_get_url:
 * @img: The #FbDataImage.
 *
 * Gets the image URL from the #FbDataImage.
 *
 * Returns: The image URL.
 */
const gchar *
fb_data_image_get_url(FbDataImage *img);

/**
 * fb_data_image_queue:
 * @fata: The #FbData.
 *
 * Queues the next #FbDataImage fetches.
 */
void
fb_data_image_queue(FbData *fata);

#endif /* PURPLE_FACEBOOK_DATA_H */
