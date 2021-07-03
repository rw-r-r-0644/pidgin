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

#include <gtk/gtk.h>

#include <purple.h>

#include "minidialog.h"

enum {
	PROP_0,
	PROP_TITLE,
	PROP_DESCRIPTION,
	PROP_ICON_NAME,
	PROP_CUSTOM_ICON,
	PROP_ENABLE_DESCRIPTION_MARKUP,
	PROP_LAST
};

static GParamSpec *properties[PROP_LAST] = {NULL};

typedef struct {
	GtkImage *icon;
	GtkBox *title_box;
	GtkLabel *title;
	GtkLabel *desc;
	GtkBox *buttons;
	gboolean enable_description_markup;

	guint idle_destroy_cb_id;
} PidginMiniDialogPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(PidginMiniDialog, pidgin_mini_dialog, GTK_TYPE_BOX)

struct _mini_dialog_button_clicked_cb_data {
	PidginMiniDialog *mini_dialog;
	PidginMiniDialogCallback callback;
	gpointer user_data;
	gboolean close_dialog_after_click;
};

/******************************************************************************
 * Callbacks
 *****************************************************************************/

static gboolean
idle_destroy_cb(GtkWidget *mini_dialog)
{
	gtk_widget_destroy(mini_dialog);
	return FALSE;
}

static void
mini_dialog_button_clicked_cb(GtkButton *button,
                              gpointer user_data)
{
	struct _mini_dialog_button_clicked_cb_data *data = user_data;
	PidginMiniDialogPrivate *priv =
		pidgin_mini_dialog_get_instance_private(data->mini_dialog);

	if (data->close_dialog_after_click) {
		/* Set up the destruction callback before calling the clicked callback,
		 * so that if the mini-dialog gets destroyed during the clicked callback
		 * the idle_destroy_cb is correctly removed by _finalize.
		 */
		priv->idle_destroy_cb_id =
			g_idle_add((GSourceFunc)idle_destroy_cb, data->mini_dialog);
	}

	if (data->callback != NULL) {
		data->callback(data->mini_dialog, button, data->user_data);
	}
}

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
mini_dialog_add_button(PidginMiniDialog *self, const gchar *text,
                       PidginMiniDialogCallback clicked_cb, gpointer user_data,
                       gboolean close_dialog_after_click)
{
	PidginMiniDialogPrivate *priv =
		pidgin_mini_dialog_get_instance_private(self);
	struct _mini_dialog_button_clicked_cb_data *callback_data = NULL;
	GtkWidget *button = NULL;
	GtkWidget *label = NULL;
	char *button_text = NULL;

	label = gtk_label_new(NULL);
	button_text = g_strdup_printf("<span size=\"smaller\">%s</span>", text);
	gtk_label_set_markup_with_mnemonic(GTK_LABEL(label), button_text);
	g_free(button_text);

	callback_data = g_new0(struct _mini_dialog_button_clicked_cb_data, 1);
	callback_data->mini_dialog = self;
	callback_data->callback = clicked_cb;
	callback_data->user_data = user_data;
	callback_data->close_dialog_after_click = close_dialog_after_click;

	button = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(button), label);
	g_signal_connect_data(G_OBJECT(button), "clicked",
	                      G_CALLBACK(mini_dialog_button_clicked_cb),
	                      callback_data, (GClosureNotify)g_free, 0);

	gtk_box_pack_end(GTK_BOX(priv->buttons), button, FALSE, FALSE, 0);
	gtk_widget_show_all(button);
}

static void
mini_dialog_set_title(PidginMiniDialog *self, const gchar *title)
{
	PidginMiniDialogPrivate *priv =
		pidgin_mini_dialog_get_instance_private(self);

	gchar *title_esc = g_markup_escape_text(title, -1);
	gchar *title_markup = g_strdup_printf(
	    "<span weight=\"bold\" size=\"smaller\">%s</span>",
	    title_esc ? title_esc : "");

	gtk_label_set_markup(priv->title, title_markup);

	g_free(title_esc);
	g_free(title_markup);

	g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_TITLE]);
}

static void
mini_dialog_set_description(PidginMiniDialog *self, const gchar *description)
{
	PidginMiniDialogPrivate *priv =
		pidgin_mini_dialog_get_instance_private(self);

	if(description) {
		gchar *desc_esc, *desc_markup;

		desc_esc = priv->enable_description_markup
		    ? g_strdup(description)
		    : g_markup_escape_text(description, -1);
		desc_markup = g_strdup_printf("<span size=\"smaller\">%s</span>",
		                              desc_esc);

		gtk_label_set_markup(priv->desc, desc_markup);

		g_free(desc_esc);
		g_free(desc_markup);

		gtk_widget_show(GTK_WIDGET(priv->desc));
		g_object_set(G_OBJECT(priv->desc), "no-show-all", FALSE, NULL);
	} else {
		gtk_label_set_text(priv->desc, NULL);
		gtk_widget_hide(GTK_WIDGET(priv->desc));
		/* make calling show_all() on the minidialog not affect desc
		 * even though it's packed inside it.
	 	 */
		g_object_set(G_OBJECT(priv->desc), "no-show-all", TRUE, NULL);
	}

	g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_DESCRIPTION]);
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
static void
pidgin_mini_dialog_get_property(GObject *obj, guint param_id, GValue *value,
                                GParamSpec *pspec)
{
	PidginMiniDialog *self = PIDGIN_MINI_DIALOG(obj);
	PidginMiniDialogPrivate *priv =
		pidgin_mini_dialog_get_instance_private(self);

	switch (param_id) {
		case PROP_TITLE:
			g_value_set_string(value, gtk_label_get_text(priv->title));
			break;
		case PROP_DESCRIPTION:
			g_value_set_string(value, gtk_label_get_text(priv->desc));
			break;
		case PROP_ICON_NAME:
		{
			const gchar *icon_name = NULL;
			gtk_image_get_icon_name(priv->icon, &icon_name, NULL);
			g_value_set_string(value, icon_name);
			break;
		}
		case PROP_CUSTOM_ICON:
			g_value_set_object(value, gtk_image_get_pixbuf(priv->icon));
			break;
		case PROP_ENABLE_DESCRIPTION_MARKUP:
			g_value_set_boolean(value, priv->enable_description_markup);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
pidgin_mini_dialog_set_property(GObject *obj, guint param_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	PidginMiniDialog *self = PIDGIN_MINI_DIALOG(obj);
	PidginMiniDialogPrivate *priv =
		pidgin_mini_dialog_get_instance_private(self);

	switch (param_id) {
		case PROP_TITLE:
			mini_dialog_set_title(self, g_value_get_string(value));
			break;
		case PROP_DESCRIPTION:
			mini_dialog_set_description(self, g_value_get_string(value));
			break;
		case PROP_ICON_NAME:
			gtk_image_set_from_icon_name(priv->icon,
					g_value_get_string(value),
					GTK_ICON_SIZE_SMALL_TOOLBAR);
			break;
		case PROP_CUSTOM_ICON:
			gtk_image_set_from_pixbuf(priv->icon, g_value_get_object(value));
			break;
		case PROP_ENABLE_DESCRIPTION_MARKUP:
			priv->enable_description_markup = g_value_get_boolean(value);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
pidgin_mini_dialog_finalize(GObject *obj)
{
	PidginMiniDialog *self = PIDGIN_MINI_DIALOG(obj);
	PidginMiniDialogPrivate *priv =
		pidgin_mini_dialog_get_instance_private(self);

	if (priv->idle_destroy_cb_id) {
		g_source_remove(priv->idle_destroy_cb_id);
	}

	purple_prefs_disconnect_by_handle(self);

	G_OBJECT_CLASS(pidgin_mini_dialog_parent_class)->finalize(obj);
}

static void
pidgin_mini_dialog_class_init(PidginMiniDialogClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->get_property = pidgin_mini_dialog_get_property;
	obj_class->set_property = pidgin_mini_dialog_set_property;
	obj_class->finalize = pidgin_mini_dialog_finalize;

	properties[PROP_TITLE] = g_param_spec_string(
		"title", "title",
		"String specifying the mini-dialog's title", NULL,
		G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);

	properties[PROP_DESCRIPTION] = g_param_spec_string(
		"description", "description",
		"Description text for the mini-dialog, if desired", NULL,
		G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);

	properties[PROP_ICON_NAME] = g_param_spec_string(
		"icon-name", "icon-name",
		"String specifying the GtkIconTheme name of the dialog's icon", NULL,
		G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);

	properties[PROP_CUSTOM_ICON] = g_param_spec_object(
		"custom-icon", "custom-icon",
		"Pixbuf to use as the dialog's icon", GDK_TYPE_PIXBUF,
		G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);

	properties[PROP_ENABLE_DESCRIPTION_MARKUP] = g_param_spec_boolean(
		"enable-description-markup", "enable-description-markup",
		"Use GMarkup in the description text", FALSE,
		G_PARAM_STATIC_STRINGS | G_PARAM_READWRITE);

	g_object_class_install_properties(obj_class, PROP_LAST, properties);
}

static void
pidgin_mini_dialog_init(PidginMiniDialog *self)
{
	GtkBox *self_box = GTK_BOX(self);
	PidginMiniDialogPrivate *priv =
		pidgin_mini_dialog_get_instance_private(self);

	gtk_orientable_set_orientation(GTK_ORIENTABLE(self), GTK_ORIENTATION_VERTICAL);

	gtk_container_set_border_width(GTK_CONTAINER(self), 6);

	priv->title_box = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6));

	priv->icon = GTK_IMAGE(gtk_image_new());
	gtk_widget_set_halign(GTK_WIDGET(priv->icon), GTK_ALIGN_START);
	gtk_widget_set_valign(GTK_WIDGET(priv->icon), GTK_ALIGN_START);

	priv->title = GTK_LABEL(gtk_label_new(NULL));
	gtk_label_set_line_wrap(priv->title, TRUE);
	gtk_label_set_selectable(priv->title, TRUE);
	gtk_label_set_xalign(priv->title, 0);
	gtk_label_set_yalign(priv->title, 0);

	gtk_box_pack_start(priv->title_box, GTK_WIDGET(priv->icon), FALSE, FALSE, 0);
	gtk_box_pack_start(priv->title_box, GTK_WIDGET(priv->title), TRUE, TRUE, 0);

	priv->desc = GTK_LABEL(gtk_label_new(NULL));
	gtk_label_set_line_wrap(priv->desc, TRUE);
	gtk_label_set_xalign(priv->desc, 0);
	gtk_label_set_yalign(priv->desc, 0);
	gtk_label_set_selectable(priv->desc, TRUE);
	/* make calling show_all() on the minidialog not affect desc even though
	 * it's packed inside it.
	 */
	g_object_set(G_OBJECT(priv->desc), "no-show-all", TRUE, NULL);

	self->contents = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 0));

	priv->buttons = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0));

	gtk_box_pack_start(self_box, GTK_WIDGET(priv->title_box), FALSE, FALSE, 0);
	gtk_box_pack_start(self_box, GTK_WIDGET(priv->desc), FALSE, FALSE, 0);
	gtk_box_pack_start(self_box, GTK_WIDGET(self->contents), TRUE, TRUE, 0);
	gtk_box_pack_start(self_box, GTK_WIDGET(priv->buttons), FALSE, FALSE, 0);

	gtk_widget_show_all(GTK_WIDGET(self));
}

/******************************************************************************
 * Public API
 *****************************************************************************/
PidginMiniDialog *
pidgin_mini_dialog_new(const gchar *title,
                       const gchar *description,
                       const gchar *icon_name)
{
	PidginMiniDialog *mini_dialog = g_object_new(PIDGIN_TYPE_MINI_DIALOG,
		"title", title,
		"description", description,
		"icon-name", icon_name,
		NULL);
	return mini_dialog;
}

PidginMiniDialog *
pidgin_mini_dialog_new_with_custom_icon(const gchar *title,
                                        const gchar *description,
                                        GdkPixbuf *custom_icon)
{
	PidginMiniDialog *mini_dialog = g_object_new(PIDGIN_TYPE_MINI_DIALOG,
		"title", title,
		"description", description,
		"custom-icon", custom_icon,
		NULL);
	return mini_dialog;
}

void
pidgin_mini_dialog_set_title(PidginMiniDialog *mini_dialog, const gchar *title)
{
	g_return_if_fail(PIDGIN_IS_MINI_DIALOG(mini_dialog));

	g_object_set(G_OBJECT(mini_dialog), "title", title, NULL);
}

void
pidgin_mini_dialog_set_description(PidginMiniDialog *mini_dialog,
                                   const gchar *description)
{
	g_return_if_fail(PIDGIN_IS_MINI_DIALOG(mini_dialog));

	g_object_set(G_OBJECT(mini_dialog), "description", description, NULL);
}

void
pidgin_mini_dialog_enable_description_markup(PidginMiniDialog *mini_dialog)
{
	g_return_if_fail(PIDGIN_IS_MINI_DIALOG(mini_dialog));

	g_object_set(G_OBJECT(mini_dialog), "enable-description-markup", TRUE, NULL);
}

void
pidgin_mini_dialog_set_link_callback(PidginMiniDialog *mini_dialog,
                                     GCallback cb, gpointer user_data)
{
	PidginMiniDialogPrivate *priv = NULL;

	g_return_if_fail(PIDGIN_IS_MINI_DIALOG(mini_dialog));

	priv = pidgin_mini_dialog_get_instance_private(mini_dialog);
	g_signal_connect(priv->desc, "activate-link", cb, user_data);
}

void
pidgin_mini_dialog_set_icon_name(PidginMiniDialog *mini_dialog,
                                 const gchar *icon_name)
{
	g_return_if_fail(PIDGIN_IS_MINI_DIALOG(mini_dialog));

	g_object_set(G_OBJECT(mini_dialog), "icon-name", icon_name, NULL);
}

void
pidgin_mini_dialog_set_custom_icon(PidginMiniDialog *mini_dialog,
                                   GdkPixbuf *custom_icon)
{
	g_return_if_fail(PIDGIN_IS_MINI_DIALOG(mini_dialog));

	g_object_set(G_OBJECT(mini_dialog), "custom-icon", custom_icon, NULL);
}

guint
pidgin_mini_dialog_get_num_children(PidginMiniDialog *mini_dialog)
{
	GList *tmp;
	guint len;

	g_return_val_if_fail(PIDGIN_IS_MINI_DIALOG(mini_dialog), 0);

	tmp = gtk_container_get_children(GTK_CONTAINER(mini_dialog->contents));
	len = g_list_length(tmp);
	g_list_free(tmp);

	return len;
}

void
pidgin_mini_dialog_add_button(PidginMiniDialog *mini_dialog,
                              const gchar *text,
                              PidginMiniDialogCallback clicked_cb,
                              gpointer user_data)
{
	g_return_if_fail(PIDGIN_IS_MINI_DIALOG(mini_dialog));

	mini_dialog_add_button(mini_dialog, text, clicked_cb, user_data, TRUE);
}

void
pidgin_mini_dialog_add_non_closing_button(PidginMiniDialog *mini_dialog,
                                          const gchar *text,
                                          PidginMiniDialogCallback clicked_cb,
                                          gpointer user_data)
{
	g_return_if_fail(PIDGIN_IS_MINI_DIALOG(mini_dialog));

	mini_dialog_add_button(mini_dialog, text, clicked_cb, user_data, FALSE);
}
