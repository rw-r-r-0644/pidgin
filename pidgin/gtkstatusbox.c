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

/*
 * The status box is made up of two main pieces:
 *   - The box that displays the current status, which is made
 *     of a GtkListStore ("status_box->store") and GtkCellView
 *     ("status_box->cell_view").  There is always exactly 1 row
 *     in this list store.  Only the TYPE_ICON and TYPE_TEXT
 *     columns are used in this list store.
 *   - The dropdown menu that lets users select a status, which
 *     is made of a GtkComboBox ("status_box") and GtkListStore
 *     ("status_box->dropdown_store").  This dropdown is shown
 *     when the user clicks on the box that displays the current
 *     status.  This list store contains one row for Available,
 *     one row for Away, etc., a few rows for popular statuses,
 *     and the "New..." and "Saved..." options.
 */

#include <glib/gi18n-lib.h>

#include <gtk/gtk.h>

#include <gdk/gdkkeysyms.h>

#include <purple.h>

#include "gtksavedstatuses.h"
#include "gtkstatusbox.h"
#include "gtkutils.h"
#include "pidgincore.h"
#include "pidgingdkpixbuf.h"
#include "pidginstock.h"

/* Timeout for typing notifications in seconds */
#define TYPING_TIMEOUT 4

typedef enum {
	PIDGIN_STATUS_BOX_TYPE_SEPARATOR,
	PIDGIN_STATUS_BOX_TYPE_PRIMITIVE,
	PIDGIN_STATUS_BOX_TYPE_POPULAR,
	PIDGIN_STATUS_BOX_TYPE_SAVED_POPULAR,
	PIDGIN_STATUS_BOX_TYPE_CUSTOM,
	PIDGIN_STATUS_BOX_TYPE_SAVED,
	PIDGIN_STATUS_BOX_NUM_TYPES
} PidginStatusBoxItemType;

struct _PidginStatusBox {
	GtkContainer parent_instance;

	/*< public >*/
	GtkListStore *store;
	GtkListStore *dropdown_store;

	PurpleAccount *account;

	PurpleAccount *token_status_account;

	GtkWidget *vbox;
	gboolean editor_visible;
	GtkWidget *editor;
	GtkWidget *input;
	GtkTextBuffer *buffer;

	GdkCursor *hand_cursor;
	GdkCursor *arrow_cursor;
	int icon_size;
	gboolean icon_opaque;


	GtkWidget *cell_view;
	GtkCellRenderer *icon_rend;
	GtkCellRenderer *text_rend;

	GdkPixbuf *error_pixbuf;

	gboolean network_available;
	gboolean connecting;

	GtkTreeIter iter;
	char *error;

	/*
	 * These widgets are made for renderin'
	 * That's just what they'll do
	 * One of these days these widgets
	 * Are gonna render all over you
	 */
	GtkWidget *hbox;
	GtkWidget *toggle_button;
	GtkWidget *vsep;
	GtkWidget *arrow;

	GtkWidget *popup_window;
	GtkWidget *popup_frame;
	GtkWidget *scrolled_window;
	GtkWidget *cell_view_frame;
	GtkTreeViewColumn *column;
	GtkWidget *tree_view;
	gboolean popup_in_progress;
	GtkTreeRowReference *active_row;
};

static void pidgin_status_box_add(PidginStatusBox *status_box, PidginStatusBoxItemType type, GdkPixbuf *pixbuf, const char *text, const char *sec_text, gpointer data);
static void pidgin_status_box_add_separator(PidginStatusBox *status_box);
static void pidgin_status_box_set_network_available(PidginStatusBox *status_box, gboolean available);
static void activate_currently_selected_status(PidginStatusBox *status_box);

static void update_size (PidginStatusBox *box);
static gint get_statusbox_index(PidginStatusBox *box, PurpleSavedStatus *saved_status);
static PurpleAccount* check_active_accounts_for_identical_statuses(void);

static void pidgin_status_box_refresh(PidginStatusBox *status_box);
static void status_menu_refresh_iter(PidginStatusBox *status_box, gboolean status_changed);
static void pidgin_status_box_regenerate(PidginStatusBox *status_box, gboolean status_changed);
static void pidgin_status_box_changed(PidginStatusBox *box);
static void pidgin_status_box_get_preferred_height (GtkWidget *widget,
	gint *minimum_height, gint *natural_height);
static gboolean pidgin_status_box_draw (GtkWidget *widget, cairo_t *cr);
static void pidgin_status_box_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static void pidgin_status_box_forall (GtkContainer *container, gboolean include_internals, GtkCallback callback, gpointer callback_data);
static void pidgin_status_box_popup(PidginStatusBox *box, GdkEvent *event);
static void pidgin_status_box_popdown(PidginStatusBox *box, GdkEvent *event);

enum {
	/* A PidginStatusBoxItemType */
	TYPE_COLUMN,

	/* This is the stock-id for the icon. */
	ICON_STOCK_COLUMN,

	/*
	 * This is a GdkPixbuf (the other columns are strings).
	 * This column is visible.
	 */
	ICON_COLUMN,

	/* The text displayed on the status box.  This column is visible. */
	TEXT_COLUMN,

	/* The plain-English title of this item */
	TITLE_COLUMN,

	/* A plain-English description of this item */
	DESC_COLUMN,

	/*
	 * This value depends on TYPE_COLUMN.  For POPULAR types,
	 * this is the creation time.  For PRIMITIVE types,
	 * this is the PurpleStatusPrimitive.
	 */
	DATA_COLUMN,

	/*
 	 * This column stores the GdkPixbuf for the status emblem. Currently only 'saved' is stored.
	 * In the GtkTreeModel for the dropdown, this is the stock-id (gchararray), and for the
	 * GtkTreeModel for the cell_view (for the account-specific statusbox), this is the protocol icon
	 * (GdkPixbuf) of the account.
 	 */
	EMBLEM_COLUMN,

	/*
 	* This column stores whether to show the emblem.
 	*/
	EMBLEM_VISIBLE_COLUMN,

	NUM_COLUMNS
};

enum {
	PROP_0,
	PROP_ACCOUNT,
};

static GtkContainerClass *parent_class = NULL;

static void pidgin_status_box_class_init (PidginStatusBoxClass *klass);
static void pidgin_status_box_init (PidginStatusBox *status_box);

static void
pidgin_status_box_network_changed_cb(GNetworkMonitor *m, gboolean available,
                                     gpointer data)
{
	pidgin_status_box_set_network_available(PIDGIN_STATUS_BOX(data), available);
}

GType
pidgin_status_box_get_type (void)
{
	static GType status_box_type = 0;

	if (!status_box_type)
	{
		static const GTypeInfo status_box_info =
		{
			sizeof (PidginStatusBoxClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) pidgin_status_box_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (PidginStatusBox),
			0,
			(GInstanceInitFunc) pidgin_status_box_init,
			NULL  /* value_table */
		};

		status_box_type = g_type_register_static(GTK_TYPE_CONTAINER,
												 "PidginStatusBox",
												 &status_box_info,
												 0);
	}

	return status_box_type;
}

static void
pidgin_status_box_get_property(GObject *object, guint param_id,
                                 GValue *value, GParamSpec *psec)
{
	PidginStatusBox *statusbox = PIDGIN_STATUS_BOX(object);

	switch (param_id) {
	case PROP_ACCOUNT:
		g_value_set_pointer(value, statusbox->account);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, psec);
		break;
	}
}

static void
update_to_reflect_account_status(PidginStatusBox *status_box, PurpleAccount *account, PurpleStatus *newstatus)
{
	GList *l;
	int status_no = -1;
	const PurpleStatusType *statustype = NULL;
	const char *message;

	statustype = purple_status_type_find_with_id((GList *)purple_account_get_status_types(account),
	                                           (char *)purple_status_type_get_id(purple_status_get_status_type(newstatus)));

	for (l = purple_account_get_status_types(account); l != NULL; l = l->next) {
		PurpleStatusType *status_type = (PurpleStatusType *)l->data;

		if (!purple_status_type_is_user_settable(status_type) ||
				purple_status_type_is_independent(status_type))
			continue;
		status_no++;
		if (statustype == status_type)
			break;
	}

	if (status_no != -1) {
		GtkTreePath *path;
		gtk_widget_set_sensitive(GTK_WIDGET(status_box), FALSE);
		path = gtk_tree_path_new_from_indices(status_no, -1);
		if (status_box->active_row)
			gtk_tree_row_reference_free(status_box->active_row);
		status_box->active_row = gtk_tree_row_reference_new(GTK_TREE_MODEL(status_box->dropdown_store), path);
		gtk_tree_path_free(path);

		message = purple_status_get_attr_string(newstatus, "message");

		if (!message || !*message)
		{
			gtk_widget_hide(status_box->vbox);
			status_box->editor_visible = FALSE;
		}
		else
		{
			gtk_widget_show_all(status_box->vbox);
			status_box->editor_visible = TRUE;
			talkatu_markup_set_html(TALKATU_BUFFER(status_box->buffer), message, -1);
		}
		gtk_widget_set_sensitive(GTK_WIDGET(status_box), TRUE);
		pidgin_status_box_refresh(status_box);
	}
}

static void
account_status_changed_cb(PurpleAccount *account, PurpleStatus *oldstatus, PurpleStatus *newstatus, PidginStatusBox *status_box)
{
	if (status_box->account == account)
		update_to_reflect_account_status(status_box, account, newstatus);
	else if (status_box->token_status_account == account)
		status_menu_refresh_iter(status_box, TRUE);
}

static void
pidgin_status_box_set_property(GObject *object, guint param_id,
                               const GValue *value, GParamSpec *pspec)
{
	PidginStatusBox *statusbox = PIDGIN_STATUS_BOX(object);

	switch (param_id) {
	case PROP_ACCOUNT:
		statusbox->account = g_value_get_pointer(value);
		if (statusbox->account)
			statusbox->token_status_account = NULL;
		else
			statusbox->token_status_account = check_active_accounts_for_identical_statuses();

		pidgin_status_box_regenerate(statusbox, TRUE);

		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
		break;
	}
}

static void
pidgin_status_box_finalize(GObject *obj)
{
	PidginStatusBox *statusbox = PIDGIN_STATUS_BOX(obj);

	purple_signals_disconnect_by_handle(statusbox);
	purple_prefs_disconnect_by_handle(statusbox);

	if (statusbox->active_row)
		gtk_tree_row_reference_free(statusbox->active_row);

	g_object_unref(G_OBJECT(statusbox->store));
	g_object_unref(G_OBJECT(statusbox->dropdown_store));
	G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static GType
pidgin_status_box_child_type (GtkContainer *container)
{
	return GTK_TYPE_WIDGET;
}

static void
pidgin_status_box_class_init (PidginStatusBoxClass *klass)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkContainerClass *container_class = (GtkContainerClass*)klass;

	parent_class = g_type_class_peek_parent(klass);

	widget_class = (GtkWidgetClass*)klass;
	widget_class->get_preferred_height = pidgin_status_box_get_preferred_height;
	widget_class->draw = pidgin_status_box_draw;
	widget_class->size_allocate = pidgin_status_box_size_allocate;

	container_class->child_type = pidgin_status_box_child_type;
	container_class->forall = pidgin_status_box_forall;
	container_class->remove = NULL;

	object_class = (GObjectClass *)klass;

	object_class->finalize = pidgin_status_box_finalize;

	object_class->get_property = pidgin_status_box_get_property;
	object_class->set_property = pidgin_status_box_set_property;

	g_object_class_install_property(object_class,
	                                PROP_ACCOUNT,
	                                g_param_spec_pointer("account",
	                                                     "Account",
	                                                     "The account, or NULL for all accounts",
	                                                      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS
	                                                     )
	                               );
}

/*
 * This updates the text displayed on the status box so that it shows
 * the current status.  This is the only function in this file that
 * should modify status_box->store
 */
static void
pidgin_status_box_refresh(PidginStatusBox *status_box)
{
	const char *aa_color;
	PurpleSavedStatus *saved_status;
	char *primary, *secondary, *text;
	const char *stock = NULL;
	GdkPixbuf *emblem = NULL;
	GtkTreePath *path;
	gboolean account_status = FALSE;
	PurpleAccount *acct = (status_box->account) ? status_box->account : status_box->token_status_account;

	saved_status = purple_savedstatus_get_current();

	if (status_box->account || (status_box->token_status_account
			&& purple_savedstatus_is_transient(saved_status)))
		account_status = TRUE;

	/* Primary */
	if (account_status)
		primary = g_strdup(purple_status_get_name(purple_account_get_active_status(acct)));
	else if (purple_savedstatus_is_transient(saved_status))
		primary = g_strdup(purple_primitive_get_name_from_type(purple_savedstatus_get_primitive_type(saved_status)));
	else
		primary = g_markup_escape_text(purple_savedstatus_get_title(saved_status), -1);

	/* Secondary */
	if (status_box->connecting)
		secondary = g_strdup(_("Connecting"));
	else if (!status_box->network_available)
		secondary = g_strdup(_("Waiting for network connection"));
	else if (purple_savedstatus_is_transient(saved_status))
		secondary = NULL;
	else
	{
		const char *message;
		char *tmp;
		message = purple_savedstatus_get_message(saved_status);
		if (message != NULL)
		{
			tmp = purple_markup_strip_html(message);
			purple_util_chrreplace(tmp, '\n', ' ');
			secondary = g_markup_escape_text(tmp, -1);
			g_free(tmp);
		}
		else
			secondary = NULL;
	}

	/* Pixbuf */
	{
	    PurpleStatusType *status_type;
	    PurpleStatusPrimitive prim;
	    if (account_status) {
			status_type = purple_status_get_status_type(purple_account_get_active_status(acct));
	        prim = purple_status_type_get_primitive(status_type);
	    } else {
			prim = purple_savedstatus_get_primitive_type(saved_status);
	    }

		stock = pidgin_stock_id_from_status_primitive(prim);
	}

	aa_color = pidgin_get_dim_grey_string(GTK_WIDGET(status_box));
	if (status_box->account != NULL) {
		text = g_strdup_printf("%s - <span size=\"smaller\" color=\"%s\">%s</span>",
				       purple_account_get_username(status_box->account),
				       aa_color, secondary ? secondary : primary);
		emblem = pidgin_create_protocol_icon(status_box->account, PIDGIN_PROTOCOL_ICON_SMALL);
	} else if (secondary != NULL) {
		text = g_strdup_printf("%s<span size=\"smaller\" color=\"%s\"> - %s</span>",
				       primary, aa_color, secondary);
	} else {
		text = g_strdup(primary);
	}
	g_free(primary);
	g_free(secondary);

	/*
	 * Only two columns are used in this list store (does it
	 * really need to be a list store?)
	 */
	gtk_list_store_set(status_box->store, &(status_box->iter),
			   ICON_STOCK_COLUMN, (gpointer)stock,
			   TEXT_COLUMN, text,
			   EMBLEM_COLUMN, emblem,
			   EMBLEM_VISIBLE_COLUMN, (emblem != NULL),
			   -1);
	g_free(text);
	if (emblem)
		g_object_unref(emblem);

	/* Make sure to activate the only row in the tree view */
	path = gtk_tree_path_new_from_string("0");
	gtk_cell_view_set_displayed_row(GTK_CELL_VIEW(status_box->cell_view), path);
	gtk_tree_path_free(path);

	update_size(status_box);
}

static PurpleStatusType *
find_status_type_by_index(PurpleAccount *account, gint active)
{
	GList *l = purple_account_get_status_types(account);
	gint i;

	for (i = 0; l; l = l->next) {
		PurpleStatusType *status_type = l->data;
		if (!purple_status_type_is_user_settable(status_type) ||
				purple_status_type_is_independent(status_type))
			continue;

		if (active == i)
			return status_type;
		i++;
	}

	return NULL;
}

/*
 * This updates the GtkTreeView so that it correctly shows the state
 * we are currently using.  It is used when the current state is
 * updated from somewhere other than the GtkStatusBox (from a plugin,
 * or when signing on with the "-n" option, for example).  It is
 * also used when the user selects the "New..." option.
 *
 * Maybe we could accomplish this by triggering off the mouse and
 * keyboard signals instead of the changed signal?
 */
static void
status_menu_refresh_iter(PidginStatusBox *status_box, gboolean status_changed)
{
	PurpleSavedStatus *saved_status;
	PurpleStatusPrimitive primitive;
	gint index;
	const char *message;
	GtkTreePath *path = NULL;

	/* this function is inappropriate for ones with accounts */
	if (status_box->account)
		return;

	saved_status = purple_savedstatus_get_current();

	/*
	 * Suppress the "changed" signal because the status
	 * was changed programmatically.
	 */
	gtk_widget_set_sensitive(GTK_WIDGET(status_box), FALSE);

	/*
	 * If there is a token-account, then select the primitive from the
	 * dropdown using a loop. Otherwise select from the default list.
	 */
	primitive = purple_savedstatus_get_primitive_type(saved_status);
	if (!status_box->token_status_account && purple_savedstatus_is_transient(saved_status) &&
		((primitive == PURPLE_STATUS_AVAILABLE) || (primitive == PURPLE_STATUS_AWAY) ||
		 (primitive == PURPLE_STATUS_INVISIBLE) || (primitive == PURPLE_STATUS_OFFLINE) ||
		 (primitive == PURPLE_STATUS_UNAVAILABLE)) &&
		(!purple_savedstatus_has_substatuses(saved_status)))
	{
		index = get_statusbox_index(status_box, saved_status);
		path = gtk_tree_path_new_from_indices(index, -1);
	}
	else
	{
		GtkTreeIter iter;
		PidginStatusBoxItemType type;
		gpointer data;

		/* If this saved status is in the list store, then set it as the active item */
		if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(status_box->dropdown_store), &iter))
		{
			do
			{
				gtk_tree_model_get(GTK_TREE_MODEL(status_box->dropdown_store), &iter,
							TYPE_COLUMN, &type,
							DATA_COLUMN, &data,
							-1);

				/* This is a special case because Primitives for the token_status_account are actually
				 * saved statuses with substatuses for the enabled accounts */
				if (status_box->token_status_account && purple_savedstatus_is_transient(saved_status)
					&& type == PIDGIN_STATUS_BOX_TYPE_PRIMITIVE && primitive == (PurpleStatusPrimitive)GPOINTER_TO_INT(data))
				{
					char *name;
					const char *acct_status_name = purple_status_get_name(
						purple_account_get_active_status(status_box->token_status_account));

					gtk_tree_model_get(GTK_TREE_MODEL(status_box->dropdown_store), &iter,
							TEXT_COLUMN, &name, -1);

					if (!purple_savedstatus_has_substatuses(saved_status)
						|| purple_strequal(name, acct_status_name))
					{
						/* Found! */
						path = gtk_tree_model_get_path(GTK_TREE_MODEL(status_box->dropdown_store), &iter);
						g_free(name);
						break;
					}
					g_free(name);

				} else if ((type == PIDGIN_STATUS_BOX_TYPE_POPULAR) &&
						(GPOINTER_TO_INT(data) == purple_savedstatus_get_creation_time(saved_status)))
				{
					/* Found! */
					path = gtk_tree_model_get_path(GTK_TREE_MODEL(status_box->dropdown_store), &iter);
					break;
				}
			} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(status_box->dropdown_store), &iter));
		}
	}

	if (status_box->active_row)
		gtk_tree_row_reference_free(status_box->active_row);
	if (path) {   /* path should never be NULL */
		status_box->active_row = gtk_tree_row_reference_new(GTK_TREE_MODEL(status_box->dropdown_store), path);
		gtk_tree_path_free(path);
	} else
		status_box->active_row = NULL;

	if (status_changed) {
		message = purple_savedstatus_get_message(saved_status);

		/*
		 * If we are going to hide the editor, don't retain the
		 * message because showing the old message later is
		 * confusing. If we are going to set the message to a pre-set,
		 * then we need to do this anyway
		 *
		 * Suppress the "changed" signal because the status
		 * was changed programmatically.
		 */
		gtk_widget_set_sensitive(GTK_WIDGET(status_box->input), FALSE);

		talkatu_buffer_clear(TALKATU_BUFFER(status_box->buffer));

		if (!purple_savedstatus_is_transient(saved_status) || !message || !*message)
		{
			status_box->editor_visible = FALSE;
			gtk_widget_hide(status_box->vbox);
		}
		else
		{
			status_box->editor_visible = TRUE;
			gtk_widget_show_all(status_box->vbox);

			talkatu_markup_set_html(TALKATU_BUFFER(status_box->buffer), message, -1);
		}

		gtk_widget_set_sensitive(GTK_WIDGET(status_box->input), TRUE);
		update_size(status_box);
	}

	/* Stop suppressing the "changed" signal. */
	gtk_widget_set_sensitive(GTK_WIDGET(status_box), TRUE);
}

static void
add_popular_statuses(PidginStatusBox *statusbox)
{
	GList *list, *cur;

	list = purple_savedstatuses_get_popular(6);
	if (list == NULL)
		/* Odd... oh well, nothing we can do about it. */
		return;

	pidgin_status_box_add_separator(statusbox);

	for (cur = list; cur != NULL; cur = cur->next)
	{
		PurpleSavedStatus *saved = cur->data;
		const gchar *message;
		gchar *stripped = NULL;
		PidginStatusBoxItemType type;

		if (purple_savedstatus_is_transient(saved))
		{
			/*
			 * Transient statuses do not have a title, so the savedstatus
			 * API returns the message when purple_savedstatus_get_title() is
			 * called, so we don't need to get the message a second time.
			 */
			type = PIDGIN_STATUS_BOX_TYPE_POPULAR;
		}
		else
		{
			type = PIDGIN_STATUS_BOX_TYPE_SAVED_POPULAR;

			message = purple_savedstatus_get_message(saved);
			if (message != NULL)
			{
				stripped = purple_markup_strip_html(message);
				purple_util_chrreplace(stripped, '\n', ' ');
			}
		}

		pidgin_status_box_add(statusbox, type,
				NULL, purple_savedstatus_get_title(saved), stripped,
				GINT_TO_POINTER(purple_savedstatus_get_creation_time(saved)));
		g_free(stripped);
	}

	g_list_free(list);
}

/* This returns NULL if the active accounts don't have identical
 * statuses and a token account if they do */
static PurpleAccount* check_active_accounts_for_identical_statuses(void)
{
	GList *iter, *active_accts = purple_accounts_get_all_active();
	PurpleAccount *acct1 = NULL;
	const char *proto1 = NULL;

	if (active_accts) {
		acct1 = active_accts->data;
		proto1 = purple_account_get_protocol_id(acct1);
	} else {
		/* there's no enabled account */
		return NULL;
	}

	/* start at the second account */
	for (iter = active_accts->next; iter; iter = iter->next) {
		PurpleAccount *acct2 = iter->data;
		GList *s1, *s2;

		if (!purple_strequal(proto1, purple_account_get_protocol_id(acct2))) {
			acct1 = NULL;
			break;
		}

		for (s1 = purple_account_get_status_types(acct1),
				 s2 = purple_account_get_status_types(acct2); s1 && s2;
			 s1 = s1->next, s2 = s2->next) {
			PurpleStatusType *st1 = s1->data, *st2 = s2->data;
			/* TODO: Are these enough to consider the statuses identical? */
			if (purple_status_type_get_primitive(st1) != purple_status_type_get_primitive(st2)
				|| !purple_strequal(purple_status_type_get_id(st1), purple_status_type_get_id(st2))
				|| !purple_strequal(purple_status_type_get_name(st1), purple_status_type_get_name(st2))) {
				acct1 = NULL;
				break;
			}
		}

		if (s1 != s2) {/* Will both be NULL if matched */
			acct1 = NULL;
			break;
		}
	}

	g_list_free(active_accts);

	return acct1;
}

static void
add_account_statuses(PidginStatusBox *status_box, PurpleAccount *account)
{
	/* Per-account */
	GList *l;

	for (l = purple_account_get_status_types(account); l != NULL; l = l->next)
	{
		PurpleStatusType *status_type = (PurpleStatusType *)l->data;
		PurpleStatusPrimitive prim;

		if (!purple_status_type_is_user_settable(status_type) ||
				purple_status_type_is_independent(status_type))
			continue;

		prim = purple_status_type_get_primitive(status_type);

		pidgin_status_box_add(PIDGIN_STATUS_BOX(status_box),
					PIDGIN_STATUS_BOX_TYPE_PRIMITIVE, NULL,
					purple_status_type_get_name(status_type),
					NULL,
					GINT_TO_POINTER(prim));
	}
}

static void
pidgin_status_box_regenerate(PidginStatusBox *status_box, gboolean status_changed)
{
	/* Unset the model while clearing it */
	gtk_tree_view_set_model(GTK_TREE_VIEW(status_box->tree_view), NULL);
	gtk_list_store_clear(status_box->dropdown_store);
	/* Don't set the model until the new statuses have been added to the box.
	 * What is presumably a bug in Gtk < 2.4 causes things to get all confused
	 * if we do this here. */
	/* gtk_combo_box_set_model(GTK_COMBO_BOX(status_box), GTK_TREE_MODEL(status_box->dropdown_store)); */

	if (status_box->account == NULL)
	{
		/* Do all the currently enabled accounts have the same statuses?
		 * If so, display them instead of our global list.
		 */
		if (status_box->token_status_account) {
			add_account_statuses(status_box, status_box->token_status_account);
		} else {
			/* Global */
			pidgin_status_box_add(PIDGIN_STATUS_BOX(status_box), PIDGIN_STATUS_BOX_TYPE_PRIMITIVE, NULL, _("Available"), NULL, GINT_TO_POINTER(PURPLE_STATUS_AVAILABLE));
			pidgin_status_box_add(PIDGIN_STATUS_BOX(status_box), PIDGIN_STATUS_BOX_TYPE_PRIMITIVE, NULL, _("Away"), NULL, GINT_TO_POINTER(PURPLE_STATUS_AWAY));
			pidgin_status_box_add(PIDGIN_STATUS_BOX(status_box), PIDGIN_STATUS_BOX_TYPE_PRIMITIVE, NULL, _("Do not disturb"), NULL, GINT_TO_POINTER(PURPLE_STATUS_UNAVAILABLE));
			pidgin_status_box_add(PIDGIN_STATUS_BOX(status_box), PIDGIN_STATUS_BOX_TYPE_PRIMITIVE, NULL, _("Invisible"), NULL, GINT_TO_POINTER(PURPLE_STATUS_INVISIBLE));
			pidgin_status_box_add(PIDGIN_STATUS_BOX(status_box), PIDGIN_STATUS_BOX_TYPE_PRIMITIVE, NULL, _("Offline"), NULL, GINT_TO_POINTER(PURPLE_STATUS_OFFLINE));
		}

		add_popular_statuses(status_box);

		pidgin_status_box_add_separator(PIDGIN_STATUS_BOX(status_box));
		pidgin_status_box_add(PIDGIN_STATUS_BOX(status_box), PIDGIN_STATUS_BOX_TYPE_CUSTOM, NULL, _("New status..."), NULL, NULL);
		pidgin_status_box_add(PIDGIN_STATUS_BOX(status_box), PIDGIN_STATUS_BOX_TYPE_SAVED, NULL, _("Saved statuses..."), NULL, NULL);

		status_menu_refresh_iter(status_box, status_changed);
		pidgin_status_box_refresh(status_box);

	} else {
		add_account_statuses(status_box, status_box->account);
		update_to_reflect_account_status(status_box, status_box->account,
			purple_account_get_active_status(status_box->account));
	}
	gtk_tree_view_set_model(GTK_TREE_VIEW(status_box->tree_view), GTK_TREE_MODEL(status_box->dropdown_store));
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(status_box->tree_view), TEXT_COLUMN);
}

static gboolean
combo_box_scroll_event_cb(GtkWidget *w, GdkEventScroll *event, G_GNUC_UNUSED gpointer data)
{
	pidgin_status_box_popup(PIDGIN_STATUS_BOX(w), (GdkEvent *)event);
	return TRUE;
}

static gboolean
editor_remove_focus(GtkWidget *w, GdkEventKey *event, PidginStatusBox *status_box)
{
	if (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) {
		activate_currently_selected_status(status_box);
		pidgin_status_box_refresh(status_box);

		return TRUE;
	}
	else if (event->keyval == GDK_KEY_Tab || event->keyval == GDK_KEY_KP_Tab || event->keyval == GDK_KEY_ISO_Left_Tab)
	{
		/* If last inserted character is a tab, then remove the focus from here */
		GtkWidget *top = gtk_widget_get_toplevel(w);
		g_signal_emit_by_name(G_OBJECT(top), "move-focus",
				(event->state & GDK_SHIFT_MASK) ?
				                  GTK_DIR_TAB_BACKWARD: GTK_DIR_TAB_FORWARD);
		return TRUE;
	}

	/* Reset the status if Escape was pressed */
	if (event->keyval == GDK_KEY_Escape)
	{
		if (status_box->account != NULL)
			update_to_reflect_account_status(status_box, status_box->account,
							purple_account_get_active_status(status_box->account));
		else {
			status_menu_refresh_iter(status_box, TRUE);
			pidgin_status_box_refresh(status_box);
		}
		return TRUE;
	}

	return FALSE;
}

static gboolean
dropdown_store_row_separator_func(GtkTreeModel *model,
								  GtkTreeIter *iter, gpointer data)
{
	PidginStatusBoxItemType type;

	gtk_tree_model_get(model, iter, TYPE_COLUMN, &type, -1);

	if (type == PIDGIN_STATUS_BOX_TYPE_SEPARATOR)
		return TRUE;

	return FALSE;
}

static void account_enabled_cb(PurpleAccount *acct, PidginStatusBox *status_box)
{
	PurpleAccount *initial_token_acct = status_box->token_status_account;

	if (status_box->account)
		return;

	status_box->token_status_account = check_active_accounts_for_identical_statuses();

	/* Regenerate the list if it has changed */
	if (initial_token_acct != status_box->token_status_account) {
		pidgin_status_box_regenerate(status_box, TRUE);
	}

}

static void
current_savedstatus_changed_cb(PurpleSavedStatus *now, PurpleSavedStatus *old, PidginStatusBox *status_box)
{
	/* Make sure our current status is added to the list of popular statuses */
	pidgin_status_box_regenerate(status_box, TRUE);
}

static void
saved_status_updated_cb(PurpleSavedStatus *status, PidginStatusBox *status_box)
{
	pidgin_status_box_regenerate(status_box,
		purple_savedstatus_get_current() == status);
}

static void
pidgin_status_box_list_position (PidginStatusBox *status_box, int *x, int *y, int *width, int *height)
{
	GdkMonitor *m = NULL;
	GdkRectangle monitor;
	GtkRequisition popup_req;
	GtkPolicyType hpolicy, vpolicy;
	GtkAllocation allocation;

	gtk_widget_get_allocation(GTK_WIDGET(status_box), &allocation);
	gdk_window_get_origin(gtk_widget_get_window(GTK_WIDGET(status_box)), x, y);

	*x += allocation.x;
	*y += allocation.y;

	*width = allocation.width;

	hpolicy = vpolicy = GTK_POLICY_NEVER;
	g_object_set(G_OBJECT(status_box->scrolled_window),
	             "hscrollbar-policy", hpolicy,
	             "vscrollbar-policy", vpolicy,
	             NULL);
	gtk_widget_get_preferred_size(status_box->popup_frame, NULL, &popup_req);

	if (popup_req.width > *width) {
		hpolicy = GTK_POLICY_ALWAYS;
		g_object_set(G_OBJECT(status_box->scrolled_window),
		             "hscrollbar-policy", hpolicy,
		             "vscrollbar-policy", vpolicy,
		             NULL);
		gtk_widget_get_preferred_size(status_box->popup_frame, NULL, &popup_req);
	}

	*height = popup_req.height;

	m = gdk_display_get_monitor_at_window(gdk_display_get_default(),
							gtk_widget_get_window(GTK_WIDGET(status_box)));
	gdk_monitor_get_geometry(m, &monitor);

	if (*x < monitor.x)
		*x = monitor.x;
	else if (*x + *width > monitor.x + monitor.width)
		*x = monitor.x + monitor.width - *width;

	if (*y + allocation.height + *height <= monitor.y + monitor.height)
		*y += allocation.height;
	else if (*y - *height >= monitor.y)
		*y -= *height;
	else if (monitor.y + monitor.height - (*y + allocation.height) > *y - monitor.y)
	{
		*y += allocation.height;
		*height = monitor.y + monitor.height - *y;
	}
	else
	{
		*height = *y - monitor.y;
		*y = monitor.y;
	}

	if (popup_req.height > *height)
	{
		vpolicy = GTK_POLICY_ALWAYS;

		g_object_set(G_OBJECT(status_box->scrolled_window),
		             "hscrollbar-policy", hpolicy,
		             "vscrollbar-policy", vpolicy,
		             NULL);
	}
}

static gboolean
popup_grab_on_window(GdkWindow *window, GdkEvent *event)
{
	GdkSeat *seat = gdk_event_get_seat(event);
	GdkGrabStatus status;

	status = gdk_seat_grab(seat, window, GDK_SEAT_CAPABILITY_ALL, TRUE, NULL,
	                       event, NULL, NULL);
	if (status == GDK_GRAB_SUCCESS) {
		return TRUE;
	}

	return FALSE;
}


static void
pidgin_status_box_popup(PidginStatusBox *box, GdkEvent *event)
{
	int width, height, x, y;
	pidgin_status_box_list_position (box, &x, &y, &width, &height);

	gtk_widget_set_size_request (box->popup_window, width, height);
	gtk_window_move (GTK_WINDOW (box->popup_window), x, y);
	gtk_widget_show(box->popup_window);
	gtk_widget_grab_focus (box->tree_view);
	if (!popup_grab_on_window(gtk_widget_get_window(box->popup_window), event)) {
		gtk_widget_hide (box->popup_window);
		return;
	}
	gtk_grab_add (box->popup_window);
	/*box->popup_in_progress = TRUE;*/
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (box->toggle_button),
				      TRUE);

	if (box->active_row) {
		GtkTreePath *path = gtk_tree_row_reference_get_path(box->active_row);
		gtk_tree_view_set_cursor(GTK_TREE_VIEW(box->tree_view), path, NULL, FALSE);
		gtk_tree_path_free(path);
	}
}

static void
pidgin_status_box_popdown(PidginStatusBox *box, GdkEvent *event)
{
	GdkSeat *seat;
	gtk_widget_hide(box->popup_window);
	box->popup_in_progress = FALSE;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(box->toggle_button), FALSE);
	gtk_grab_remove(box->popup_window);
	seat = gdk_event_get_seat(event);
	gdk_seat_ungrab(seat);
}

static gboolean
toggle_key_press_cb(GtkWidget *widget, GdkEventKey *event, PidginStatusBox *box)
{
	switch (event->keyval) {
		case GDK_KEY_Return:
		case GDK_KEY_KP_Enter:
		case GDK_KEY_KP_Space:
		case GDK_KEY_space:
			if (!box->popup_in_progress) {
				pidgin_status_box_popup(box, (GdkEvent *)event);
				box->popup_in_progress = TRUE;
			} else {
				pidgin_status_box_popdown(box, (GdkEvent *)event);
			}
			return TRUE;
		default:
			return FALSE;
	}
}

static gboolean
toggled_cb(GtkWidget *widget, GdkEventButton *event, PidginStatusBox *box)
{
	if (!box->popup_in_progress)
		pidgin_status_box_popup(box, (GdkEvent *)event);
	else
		pidgin_status_box_popdown(box, (GdkEvent *)event);
	return TRUE;
}

static void
treeview_activate_current_selection(PidginStatusBox *status_box, GtkTreePath *path, GdkEvent *event)
{
	if (status_box->active_row)
		gtk_tree_row_reference_free(status_box->active_row);

	status_box->active_row = gtk_tree_row_reference_new(GTK_TREE_MODEL(status_box->dropdown_store), path);
	pidgin_status_box_popdown(status_box, event);
	pidgin_status_box_changed(status_box);
}

static void tree_view_delete_current_selection_cb(gpointer data)
{
	PurpleSavedStatus *saved;

	saved = purple_savedstatus_find_by_creation_time(GPOINTER_TO_INT(data));
	g_return_if_fail(saved != NULL);

	if (purple_savedstatus_get_current() != saved)
		purple_savedstatus_delete_by_status(saved);
}

static void
tree_view_delete_current_selection(PidginStatusBox *status_box, GtkTreePath *path, GdkEvent *event)
{
	GtkTreeIter iter;
	gpointer data;
	PurpleSavedStatus *saved;
	gchar *msg;

	if (status_box->active_row) {
		/* don't delete active status */
		if (gtk_tree_path_compare(path, gtk_tree_row_reference_get_path(status_box->active_row)) == 0)
			return;
	}

	if (!gtk_tree_model_get_iter (GTK_TREE_MODEL(status_box->dropdown_store), &iter, path))
		return;

	gtk_tree_model_get(GTK_TREE_MODEL(status_box->dropdown_store), &iter,
			   DATA_COLUMN, &data,
			   -1);

	saved = purple_savedstatus_find_by_creation_time(GPOINTER_TO_INT(data));
	g_return_if_fail(saved != NULL);
	if (saved == purple_savedstatus_get_current())
		return;

	msg = g_strdup_printf(_("Are you sure you want to delete %s?"), purple_savedstatus_get_title(saved));

	purple_request_action(saved, NULL, msg, NULL, 0,
		NULL,
		data, 2,
		_("Delete"), tree_view_delete_current_selection_cb,
		_("Cancel"), NULL);

	g_free(msg);

	pidgin_status_box_popdown(status_box, event);
}

static gboolean
treeview_button_release_cb(GtkWidget *widget, GdkEventButton *event, PidginStatusBox *status_box)
{
	GtkTreePath *path = NULL;
	int ret;
	GtkWidget *ewidget = gtk_get_event_widget ((GdkEvent *)event);

	if (ewidget != status_box->tree_view) {
		if (ewidget == status_box->toggle_button &&
		    status_box->popup_in_progress &&
		    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (status_box->toggle_button))) {
			pidgin_status_box_popdown(status_box, (GdkEvent *)event);
			return TRUE;
		} else if (ewidget == status_box->toggle_button) {
			status_box->popup_in_progress = TRUE;
		}

		/* released outside treeview */
		if (ewidget != status_box->toggle_button) {
				pidgin_status_box_popdown(status_box, (GdkEvent *)event);
				return TRUE;
		}

		return FALSE;
	}

	ret = gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (status_box->tree_view),
					     event->x, event->y,
					     &path,
					     NULL, NULL, NULL);

	if (!ret)
		return TRUE; /* clicked outside window? */

	treeview_activate_current_selection(status_box, path, (GdkEvent *)event);
	gtk_tree_path_free (path);

	return TRUE;
}

static gboolean
treeview_key_press_event(GtkWidget *widget,
			GdkEventKey *event, PidginStatusBox *box)
{
	if (box->popup_in_progress) {
		if (event->keyval == GDK_KEY_Escape) {
			pidgin_status_box_popdown(box, (GdkEvent *)event);
			return TRUE;
		} else {
			GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(box->tree_view));
			GtkTreeIter iter;
			GtkTreePath *path;

			if (gtk_tree_selection_get_selected(sel, NULL, &iter)) {
				gboolean ret = TRUE;
				path = gtk_tree_model_get_path(GTK_TREE_MODEL(box->dropdown_store), &iter);
				if (event->keyval == GDK_KEY_Return) {
					treeview_activate_current_selection(box, path, (GdkEvent *)event);
				} else if (event->keyval == GDK_KEY_Delete) {
					tree_view_delete_current_selection(box, path, (GdkEvent *)event);
				} else
					ret = FALSE;

				gtk_tree_path_free (path);
				return ret;
			}
		}
	}
	return FALSE;
}

static void
treeview_cursor_changed_cb(GtkTreeView *treeview, gpointer data)
{
	GtkTreeSelection *sel = gtk_tree_view_get_selection (treeview);
	GtkTreeModel *model = GTK_TREE_MODEL (data);
	GtkTreeIter iter;
	GtkTreePath *cursor;
	GtkTreePath *selection;
	gint cmp;

	if (gtk_tree_selection_get_selected (sel, NULL, &iter)) {
		if ((selection = gtk_tree_model_get_path (model, &iter)) == NULL) {
			/* Shouldn't happen, but ignore anyway */
			return;
		}
	} else {
		/* I don't think this can happen, but we'll just ignore it */
		return;
	}

	gtk_tree_view_get_cursor (treeview, &cursor, NULL);
	if (cursor == NULL) {
		/* Probably won't happen in a 'cursor-changed' event? */
		gtk_tree_path_free (selection);
		return;
	}

	cmp = gtk_tree_path_compare (cursor, selection);
	if (cmp < 0) {
		/* The cursor moved up without moving the selection, so move it up again */
		gtk_tree_path_prev (cursor);
		gtk_tree_view_set_cursor (treeview, cursor, NULL, FALSE);
	} else if (cmp > 0) {
		/* The cursor moved down without moving the selection, so move it down again */
		gtk_tree_path_next (cursor);
		gtk_tree_view_set_cursor (treeview, cursor, NULL, FALSE);
	}

	gtk_tree_path_free (selection);
	gtk_tree_path_free (cursor);
}

static void
pidgin_status_box_buffer_changed_cb(GtkTextBuffer *buffer, gpointer data) {
	PidginStatusBox *status_box = (PidginStatusBox*)data;

	pidgin_status_box_refresh(status_box);
}

static void
pidgin_status_box_init (PidginStatusBox *status_box)
{
	GNetworkMonitor *monitor = NULL;
	GtkCellRenderer *text_rend;
	GtkCellRenderer *icon_rend;
	GtkCellRenderer *emblem_rend;
	GtkWidget *toplevel;
	GtkTreeSelection *sel;
	gboolean network_available = FALSE;

	gtk_widget_set_has_window(GTK_WIDGET(status_box), FALSE);
	status_box->editor_visible = FALSE;
	status_box->network_available = purple_network_is_available();
	status_box->connecting = FALSE;
	status_box->toggle_button = gtk_toggle_button_new();
	status_box->hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	status_box->cell_view = gtk_cell_view_new();
	status_box->vsep = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
	status_box->arrow = gtk_image_new_from_icon_name("pan-down-symbolic", GTK_ICON_SIZE_BUTTON);

	status_box->store = gtk_list_store_new(NUM_COLUMNS, G_TYPE_INT, G_TYPE_STRING, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING,
					       G_TYPE_STRING, G_TYPE_POINTER, GDK_TYPE_PIXBUF, G_TYPE_BOOLEAN);
	status_box->dropdown_store = gtk_list_store_new(NUM_COLUMNS, G_TYPE_INT, G_TYPE_STRING, GDK_TYPE_PIXBUF, G_TYPE_STRING,
							G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_STRING, G_TYPE_BOOLEAN);

	gtk_cell_view_set_model(GTK_CELL_VIEW(status_box->cell_view), GTK_TREE_MODEL(status_box->store));
	gtk_list_store_append(status_box->store, &(status_box->iter));

	atk_object_set_name(gtk_widget_get_accessible(status_box->toggle_button), _("Status Selector"));

	gtk_container_add(GTK_CONTAINER(status_box->toggle_button), status_box->hbox);
	gtk_box_pack_start(GTK_BOX(status_box->hbox), status_box->cell_view, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(status_box->hbox), status_box->vsep, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(status_box->hbox), status_box->arrow, FALSE, FALSE, 0);
	gtk_widget_show_all(status_box->toggle_button);
	gtk_widget_set_focus_on_click(status_box->toggle_button, FALSE);

	text_rend = gtk_cell_renderer_text_new();
	icon_rend = gtk_cell_renderer_pixbuf_new();
	emblem_rend = gtk_cell_renderer_pixbuf_new();
	status_box->popup_window = gtk_window_new (GTK_WINDOW_POPUP);

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (status_box));
	if (GTK_IS_WINDOW (toplevel))  {
		gtk_window_set_transient_for (GTK_WINDOW (status_box->popup_window),
				GTK_WINDOW (toplevel));
	}

	gtk_window_set_resizable (GTK_WINDOW (status_box->popup_window), FALSE);
	gtk_window_set_type_hint (GTK_WINDOW (status_box->popup_window),
			GDK_WINDOW_TYPE_HINT_POPUP_MENU);
	gtk_window_set_screen (GTK_WINDOW (status_box->popup_window),
			gtk_widget_get_screen (GTK_WIDGET (status_box)));
	status_box->popup_frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (status_box->popup_frame),
			GTK_SHADOW_ETCHED_IN);
	gtk_container_add (GTK_CONTAINER (status_box->popup_window),
			status_box->popup_frame);

	gtk_widget_show (status_box->popup_frame);

	status_box->tree_view = gtk_tree_view_new ();
	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (status_box->tree_view));
	gtk_tree_selection_set_mode (sel, GTK_SELECTION_BROWSE);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (status_box->tree_view),
			FALSE);
	gtk_tree_view_set_hover_selection (GTK_TREE_VIEW (status_box->tree_view),
			TRUE);
	gtk_tree_view_set_model (GTK_TREE_VIEW (status_box->tree_view),
			GTK_TREE_MODEL(status_box->dropdown_store));
	status_box->column = gtk_tree_view_column_new ();
	gtk_tree_view_append_column (GTK_TREE_VIEW (status_box->tree_view),
			status_box->column);
	gtk_tree_view_column_pack_start(status_box->column, icon_rend, FALSE);
	gtk_tree_view_column_pack_start(status_box->column, text_rend, TRUE);
	gtk_tree_view_column_pack_start(status_box->column, emblem_rend, FALSE);
	gtk_tree_view_column_set_attributes(status_box->column, icon_rend, "stock-id", ICON_STOCK_COLUMN, NULL);
	gtk_tree_view_column_set_attributes(status_box->column, text_rend, "markup", TEXT_COLUMN, NULL);
	gtk_tree_view_column_set_attributes(status_box->column, emblem_rend, "stock-id", EMBLEM_COLUMN, "visible", EMBLEM_VISIBLE_COLUMN, NULL);

	status_box->scrolled_window = pidgin_make_scrollable(status_box->tree_view, GTK_POLICY_NEVER, GTK_POLICY_NEVER, GTK_SHADOW_NONE, -1, -1);
	gtk_container_add (GTK_CONTAINER (status_box->popup_frame),
			status_box->scrolled_window);

	gtk_widget_show(status_box->tree_view);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(status_box->tree_view), TEXT_COLUMN);
	gtk_tree_view_set_search_equal_func(GTK_TREE_VIEW(status_box->tree_view),
				pidgin_tree_view_search_equal_func, NULL, NULL);

	g_object_set(text_rend, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	status_box->icon_rend = gtk_cell_renderer_pixbuf_new();
	status_box->text_rend = gtk_cell_renderer_text_new();
	emblem_rend = gtk_cell_renderer_pixbuf_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(status_box->cell_view), status_box->icon_rend, FALSE);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(status_box->cell_view), status_box->text_rend, TRUE);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(status_box->cell_view), emblem_rend, FALSE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(status_box->cell_view), status_box->icon_rend, "stock-id", ICON_STOCK_COLUMN, NULL);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(status_box->cell_view), status_box->text_rend, "markup", TEXT_COLUMN, NULL);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(status_box->cell_view), emblem_rend, "pixbuf", EMBLEM_COLUMN, "visible", EMBLEM_VISIBLE_COLUMN, NULL);
	g_object_set(status_box->text_rend, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	status_box->vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, FALSE);
	status_box->editor = talkatu_editor_new();
	status_box->input = talkatu_editor_get_input(TALKATU_EDITOR(status_box->editor));

	status_box->buffer = talkatu_html_buffer_new();
	gtk_text_view_set_buffer(GTK_TEXT_VIEW(status_box->input), status_box->buffer);

	g_signal_connect(G_OBJECT(status_box->buffer), "changed",
	                 G_CALLBACK(pidgin_status_box_buffer_changed_cb),
	                 status_box);

	g_signal_connect(G_OBJECT(status_box->toggle_button), "key-press-event",
	                 G_CALLBACK(toggle_key_press_cb), status_box);
	g_signal_connect(G_OBJECT(status_box->toggle_button), "button-press-event",
	                 G_CALLBACK(toggled_cb), status_box);
	g_signal_connect(G_OBJECT(status_box->input), "key-press-event",
	                 G_CALLBACK(editor_remove_focus), status_box);

	gtk_widget_set_parent(status_box->vbox, GTK_WIDGET(status_box));
	gtk_widget_show_all(status_box->vbox);

	gtk_widget_set_parent(status_box->toggle_button, GTK_WIDGET(status_box));

	gtk_box_pack_start(GTK_BOX(status_box->vbox), status_box->editor, TRUE, TRUE, 0);

	g_signal_connect(G_OBJECT(status_box), "scroll-event", G_CALLBACK(combo_box_scroll_event_cb), NULL);
	g_signal_connect(G_OBJECT(status_box->popup_window), "button_release_event", G_CALLBACK(treeview_button_release_cb), status_box);
	g_signal_connect(G_OBJECT(status_box->popup_window), "key_press_event", G_CALLBACK(treeview_key_press_event), status_box);
	g_signal_connect(G_OBJECT(status_box->tree_view), "cursor-changed",
					 G_CALLBACK(treeview_cursor_changed_cb), status_box->dropdown_store);

	gtk_tree_view_set_row_separator_func(GTK_TREE_VIEW(status_box->tree_view), dropdown_store_row_separator_func, NULL, NULL);

	status_box->token_status_account = check_active_accounts_for_identical_statuses();

	pidgin_status_box_regenerate(status_box, TRUE);

	purple_signal_connect(purple_savedstatuses_get_handle(), "savedstatus-changed",
						status_box,
						PURPLE_CALLBACK(current_savedstatus_changed_cb),
						status_box);
	purple_signal_connect(purple_savedstatuses_get_handle(),
			"savedstatus-added", status_box,
			PURPLE_CALLBACK(saved_status_updated_cb), status_box);
	purple_signal_connect(purple_savedstatuses_get_handle(),
			"savedstatus-deleted", status_box,
			PURPLE_CALLBACK(saved_status_updated_cb), status_box);
	purple_signal_connect(purple_savedstatuses_get_handle(),
			"savedstatus-modified", status_box,
			PURPLE_CALLBACK(saved_status_updated_cb), status_box);
	purple_signal_connect(purple_accounts_get_handle(), "account-enabled", status_box,
						PURPLE_CALLBACK(account_enabled_cb),
						status_box);
	purple_signal_connect(purple_accounts_get_handle(), "account-disabled", status_box,
						PURPLE_CALLBACK(account_enabled_cb),
						status_box);
	purple_signal_connect(purple_accounts_get_handle(), "account-status-changed", status_box,
						PURPLE_CALLBACK(account_status_changed_cb),
						status_box);

	monitor = g_network_monitor_get_default();
	g_signal_connect(G_OBJECT(monitor), "network-changed",
	                 G_CALLBACK(pidgin_status_box_network_changed_cb),
	                 status_box);
	network_available = g_network_monitor_get_network_available(monitor);
	pidgin_status_box_set_network_available(status_box, network_available);
}

static void
pidgin_status_box_get_preferred_height(GtkWidget *widget, gint *minimum_height,
                                       gint *natural_height)
{
	gint box_min_height, box_nat_height;
	gint border_width = gtk_container_get_border_width(GTK_CONTAINER (widget));

	gtk_widget_get_preferred_height(PIDGIN_STATUS_BOX(widget)->toggle_button,
		minimum_height, natural_height);

	*minimum_height = MAX(*minimum_height, 34) + border_width * 2;
	*natural_height = MAX(*natural_height, 34) + border_width * 2;

	/* If the editor is visible, then add some additional padding */
	if (PIDGIN_STATUS_BOX(widget)->editor_visible) {
		gtk_widget_get_preferred_height(PIDGIN_STATUS_BOX(widget)->vbox,
			&box_min_height, &box_nat_height);

		if (box_min_height > 1)
			*minimum_height += box_min_height + border_width * 2;

		if (box_nat_height > 1)
			*natural_height += box_nat_height + border_width * 2;
	}
}

static void
pidgin_status_box_size_allocate(GtkWidget *widget,
				  GtkAllocation *allocation)
{
	PidginStatusBox *status_box = PIDGIN_STATUS_BOX(widget);
	GtkRequisition req = {0,0};
	GtkAllocation parent_alc, box_alc;
	gint border_width = gtk_container_get_border_width(GTK_CONTAINER (widget));

	gtk_widget_get_preferred_size(status_box->toggle_button, NULL, &req);
	/* Make this icon the same size as other buddy icons in the list; unless it already wants to be bigger */

	req.height = MAX(req.height, 34);
	req.height += border_width * 2;

	box_alc = *allocation;

	box_alc.width -= (border_width * 2);
	box_alc.height = MAX(1, ((allocation->height - req.height) - (border_width*2)));
	box_alc.x += border_width;
	box_alc.y += req.height + border_width;
	gtk_widget_size_allocate(status_box->vbox, &box_alc);

	parent_alc = *allocation;
	parent_alc.height = MAX(1,req.height - (border_width *2));
	parent_alc.width -= (border_width * 2);
	parent_alc.x += border_width;
	parent_alc.y += border_width;

	gtk_widget_size_allocate(status_box->toggle_button, &parent_alc);
	gtk_widget_set_allocation(GTK_WIDGET(status_box), allocation);
}

static gboolean
pidgin_status_box_draw(GtkWidget *widget, cairo_t *cr)
{
	PidginStatusBox *status_box = PIDGIN_STATUS_BOX(widget);
	gtk_widget_draw(status_box->toggle_button, cr);

	gtk_container_propagate_draw(GTK_CONTAINER(widget), status_box->vbox, cr);

	return FALSE;
}

static void
pidgin_status_box_forall(GtkContainer *container,
						   gboolean include_internals,
						   GtkCallback callback,
						   gpointer callback_data)
{
	PidginStatusBox *status_box = PIDGIN_STATUS_BOX (container);

	if (include_internals)
	{
	  	(* callback) (status_box->vbox, callback_data);
		(* callback) (status_box->toggle_button, callback_data);
		(* callback) (status_box->arrow, callback_data);
	}
}

GtkWidget *
pidgin_status_box_new()
{
	return g_object_new(PIDGIN_TYPE_STATUS_BOX, "account", NULL, NULL);
}

GtkWidget *
pidgin_status_box_new_with_account(PurpleAccount *account)
{
	return g_object_new(PIDGIN_TYPE_STATUS_BOX, "account", account, NULL);
}

/*
 * pidgin_status_box_add:
 * @status_box: The status box itself.
 * @type:       A PidginStatusBoxItemType.
 * @pixbuf:     The icon to associate with this row in the menu. The
 *                   function will try to decide a pixbuf if none is given.
 * @title:      The title of this item.  For the primitive entries,
 *                   this is something like "Available" or "Away."  For
 *                   the saved statuses, this is something like
 *                   "My favorite away message!"  This should be
 *                   plaintext (non-markedup) (this function escapes it).
 * @desc:       The secondary text for this item.  This will be
 *                   placed on the row below the title, in a dimmer
 *                   font (generally gray).  This text should be plaintext
 *                   (non-markedup) (this function escapes it).
 * @data:       Data to be associated with this row in the dropdown
 *                   menu.  For primitives this is the value of the
 *                   PurpleStatusPrimitive.  For saved statuses this is the
 *                   creation timestamp.
 *
 * Add a row to the dropdown menu.
 */
void
pidgin_status_box_add(PidginStatusBox *status_box, PidginStatusBoxItemType type, GdkPixbuf *pixbuf,
		const char *title, const char *desc, gpointer data)
{
	GtkTreeIter iter;
	char *text;
	const char *stock = NULL;

	if (desc == NULL)
	{
		text = g_markup_escape_text(title, -1);
	}
	else
	{
		const char *aa_color;
		gchar *escaped_title, *escaped_desc;

		aa_color = pidgin_get_dim_grey_string(GTK_WIDGET(status_box));

		escaped_title = g_markup_escape_text(title, -1);
		escaped_desc = g_markup_escape_text(desc, -1);
		text = g_strdup_printf("%s - <span color=\"%s\" size=\"smaller\">%s</span>",
					escaped_title,
				       aa_color, escaped_desc);
		g_free(escaped_title);
		g_free(escaped_desc);
	}

	if (!pixbuf) {
		PurpleStatusPrimitive prim = PURPLE_STATUS_UNSET;
		if (type == PIDGIN_STATUS_BOX_TYPE_PRIMITIVE) {
			prim = GPOINTER_TO_INT(data);
		} else if (type == PIDGIN_STATUS_BOX_TYPE_SAVED_POPULAR ||
				type == PIDGIN_STATUS_BOX_TYPE_POPULAR) {
			PurpleSavedStatus *saved = purple_savedstatus_find_by_creation_time(GPOINTER_TO_INT(data));
			if (saved) {
				prim = purple_savedstatus_get_primitive_type(saved);
			}
		}

		stock = pidgin_stock_id_from_status_primitive(prim);
	}

	gtk_list_store_append(status_box->dropdown_store, &iter);
	gtk_list_store_set(status_box->dropdown_store, &iter,
			TYPE_COLUMN, type,
			ICON_STOCK_COLUMN, stock,
			TEXT_COLUMN, text,
			TITLE_COLUMN, title,
			DESC_COLUMN, desc,
			DATA_COLUMN, data,
			EMBLEM_VISIBLE_COLUMN, type == PIDGIN_STATUS_BOX_TYPE_SAVED_POPULAR,
			EMBLEM_COLUMN, GTK_STOCK_SAVE,
			-1);
	g_free(text);
}

void
pidgin_status_box_add_separator(PidginStatusBox *status_box)
{
	/* Don't do anything unless GTK actually supports
	 * gtk_combo_box_set_row_separator_func */
	GtkTreeIter iter;

	gtk_list_store_append(status_box->dropdown_store, &iter);
	gtk_list_store_set(status_box->dropdown_store, &iter,
			   TYPE_COLUMN, PIDGIN_STATUS_BOX_TYPE_SEPARATOR,
			   -1);
}

void
pidgin_status_box_set_network_available(PidginStatusBox *status_box, gboolean available)
{
	if (!status_box)
		return;
	status_box->network_available = available;
	pidgin_status_box_refresh(status_box);
}

void
pidgin_status_box_set_connecting(PidginStatusBox *status_box, gboolean connecting)
{
	if (!status_box)
		return;
	status_box->connecting = connecting;
	pidgin_status_box_refresh(status_box);
}

static void
activate_currently_selected_status(PidginStatusBox *status_box)
{
	PidginStatusBoxItemType type;
	gpointer data;
	gchar *title;
	GtkTreeIter iter;
	GtkTreePath *path;
	char *message;
	PurpleSavedStatus *saved_status = NULL;
	gboolean changed = TRUE;

	path = gtk_tree_row_reference_get_path(status_box->active_row);
	if (!gtk_tree_model_get_iter (GTK_TREE_MODEL(status_box->dropdown_store), &iter, path))
		return;
	gtk_tree_path_free(path);

	gtk_tree_model_get(GTK_TREE_MODEL(status_box->dropdown_store), &iter,
					   TYPE_COLUMN, &type,
					   DATA_COLUMN, &data,
					   -1);

	/*
	 * If the currently selected status is "New..." or
	 * "Saved..." or a popular status then do nothing.
	 * Popular statuses are
	 * activated elsewhere, and we update the status_box
	 * accordingly by connecting to the savedstatus-changed
	 * signal and then calling status_menu_refresh_iter()
	 */
	if (type != PIDGIN_STATUS_BOX_TYPE_PRIMITIVE)
		return;

	gtk_tree_model_get(GTK_TREE_MODEL(status_box->dropdown_store), &iter,
					   TITLE_COLUMN, &title, -1);

	message = pidgin_status_box_get_message(status_box);
	if (!message || !*message)
	{
		gtk_widget_hide(status_box->vbox);
		status_box->editor_visible = FALSE;
		g_free(message);
		message = NULL;
	}

	if (status_box->account == NULL) {
		PurpleStatusType *acct_status_type = NULL;
		const char *id = NULL; /* id of acct_status_type */
		PurpleStatusPrimitive primitive = GPOINTER_TO_INT(data);
		/* Global */
		/* Save the newly selected status to prefs.xml and status.xml */

		/* Has the status really been changed? */
		if (status_box->token_status_account) {
			gint active;
			PurpleStatus *status;
			GtkTreePath *path = gtk_tree_row_reference_get_path(status_box->active_row);
			active = gtk_tree_path_get_indices(path)[0];

			gtk_tree_path_free(path);

			status = purple_account_get_active_status(status_box->token_status_account);

			acct_status_type = find_status_type_by_index(status_box->token_status_account, active);
			id = purple_status_type_get_id(acct_status_type);

			if (purple_strequal(id, purple_status_get_id(status)) &&
				purple_strequal(message, purple_status_get_attr_string(status, "message")))
			{
				/* Selected status and previous status is the same */
				PurpleSavedStatus *ss = purple_savedstatus_get_current();
				/* Make sure that statusbox displays the correct thing.
				 * It can get messed up if the previous selection was a
				 * saved status that wasn't supported by this account */
				if ((purple_savedstatus_get_primitive_type(ss) == primitive)
					&& purple_savedstatus_is_transient(ss)
					&& purple_savedstatus_has_substatuses(ss))
					changed = FALSE;
			}
		} else {
			saved_status = purple_savedstatus_get_current();
			if (purple_savedstatus_get_primitive_type(saved_status) == primitive &&
			    !purple_savedstatus_has_substatuses(saved_status) &&
				purple_strequal(purple_savedstatus_get_message(saved_status), message))
			{
				changed = FALSE;
			}
		}

		if (changed)
		{
			/* Manually find the appropriate transient status */
			if (status_box->token_status_account) {
				GList *iter = purple_savedstatuses_get_all();
				GList *tmp, *active_accts = purple_accounts_get_all_active();

				for (; iter != NULL; iter = iter->next) {
					PurpleSavedStatus *ss = iter->data;
					const char *ss_msg = purple_savedstatus_get_message(ss);
					/* find a known transient status that is the same as the
					 * new selected one */
					if ((purple_savedstatus_get_primitive_type(ss) == primitive) && purple_savedstatus_is_transient(ss) &&
						purple_savedstatus_has_substatuses(ss) && /* Must have substatuses */
						purple_strequal(ss_msg, message))
					{
						gboolean found = FALSE;
						/* this status must have substatuses for all the active accts */
						for(tmp = active_accts; tmp != NULL; tmp = tmp->next) {
							PurpleAccount *acct = tmp->data;
							PurpleSavedStatusSub *sub = purple_savedstatus_get_substatus(ss, acct);
							if (sub) {
								const PurpleStatusType *sub_type =
										purple_savedstatus_substatus_get_status_type(sub);
								const char *subtype_status_id = purple_status_type_get_id(sub_type);
								if (purple_strequal(subtype_status_id, id)) {
									found = TRUE;
									break;
								}
							}
						}

						if (found) {
							saved_status = ss;
							break;
						}
					}
				}

				g_list_free(active_accts);

			} else {
				/* If we've used this type+message before, lookup the transient status */
				saved_status = purple_savedstatus_find_transient_by_type_and_message(primitive, message);
			}

			/* If this type+message is unique then create a new transient saved status */
			if (saved_status == NULL)
			{
				saved_status = purple_savedstatus_new(NULL, primitive);
				purple_savedstatus_set_message(saved_status, message);
				if (status_box->token_status_account) {
					GList *tmp, *active_accts = purple_accounts_get_all_active();
					for (tmp = active_accts; tmp != NULL; tmp = tmp->next) {
						purple_savedstatus_set_substatus(saved_status,
							(PurpleAccount*) tmp->data, acct_status_type, message);
					}
					g_list_free(active_accts);
				}
			}

			/* Set the status for each account */
			purple_savedstatus_activate(saved_status);
		}
	} else {
		/* Per-account */
		gint active;
		PurpleStatusType *status_type;
		PurpleStatus *status;
		const char *id = NULL;

		status = purple_account_get_active_status(status_box->account);

		active = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(status_box), "active"));

		status_type = find_status_type_by_index(status_box->account, active);
		id = purple_status_type_get_id(status_type);

		if (purple_strequal(id, purple_status_get_id(status)) &&
			purple_strequal(message, purple_status_get_attr_string(status, "message")))
		{
			/* Selected status and previous status is the same */
			changed = FALSE;
		}

		if (changed)
		{
			if (message)
				purple_account_set_status(status_box->account, id,
										TRUE, "message", message, NULL);
			else
				purple_account_set_status(status_box->account, id,
										TRUE, NULL);

			saved_status = purple_savedstatus_get_current();
			if (purple_savedstatus_is_transient(saved_status))
				purple_savedstatus_set_substatus(saved_status, status_box->account,
						status_type, message);
		}
	}

	g_free(title);
	g_free(message);
}

static void update_size(PidginStatusBox *status_box)
{
	if (!status_box->editor_visible)
	{
		if (status_box->vbox != NULL)
			gtk_widget_set_size_request(status_box->vbox, -1, -1);
		return;
	}

	gtk_widget_set_size_request(status_box->vbox, -1, -1);
}

static void pidgin_status_box_changed(PidginStatusBox *status_box)
{
	GtkTreePath *path = gtk_tree_row_reference_get_path(status_box->active_row);
	GtkTreeIter iter;
	PidginStatusBoxItemType type;
	gpointer data;
	GList *accounts = NULL, *node;
	int active;
	gboolean wastyping = FALSE;


	if (!gtk_tree_model_get_iter (GTK_TREE_MODEL(status_box->dropdown_store), &iter, path))
		return;
	active = gtk_tree_path_get_indices(path)[0];
	gtk_tree_path_free(path);
	g_object_set_data(G_OBJECT(status_box), "active", GINT_TO_POINTER(active));

	gtk_tree_model_get(GTK_TREE_MODEL(status_box->dropdown_store), &iter,
			   TYPE_COLUMN, &type,
			   DATA_COLUMN, &data,
			   -1);

	if (gtk_widget_get_sensitive(GTK_WIDGET(status_box)))
	{
		if (type == PIDGIN_STATUS_BOX_TYPE_POPULAR || type == PIDGIN_STATUS_BOX_TYPE_SAVED_POPULAR)
		{
			PurpleSavedStatus *saved;
			saved = purple_savedstatus_find_by_creation_time(GPOINTER_TO_INT(data));
			g_return_if_fail(saved != NULL);
			purple_savedstatus_activate(saved);
			return;
		}

		if (type == PIDGIN_STATUS_BOX_TYPE_CUSTOM)
		{
			PurpleSavedStatus *saved_status;
			saved_status = purple_savedstatus_get_current();
			if (purple_savedstatus_get_primitive_type(saved_status) == PURPLE_STATUS_AVAILABLE)
				saved_status = purple_savedstatus_new(NULL, PURPLE_STATUS_AWAY);
			pidgin_status_editor_show(FALSE,
				purple_savedstatus_is_transient(saved_status)
					? saved_status : NULL);
			status_menu_refresh_iter(status_box, wastyping);
			if (wastyping)
				pidgin_status_box_refresh(status_box);
			return;
		}

		if (type == PIDGIN_STATUS_BOX_TYPE_SAVED)
		{
			pidgin_status_window_show();
			status_menu_refresh_iter(status_box, wastyping);
			if (wastyping)
				pidgin_status_box_refresh(status_box);
			return;
		}
	}

	/*
	 * Show the message box whenever the primitive allows for a
	 * message attribute on any protocol that is enabled,
	 * or our protocol, if we have account set
	 */
	if (status_box->account)
		accounts = g_list_prepend(accounts, status_box->account);
	else
		accounts = purple_accounts_get_all_active();
	status_box->editor_visible = FALSE;
	for (node = accounts; node != NULL; node = node->next)
	{
		PurpleAccount *account;
		PurpleStatusType *status_type;

		account = node->data;
		status_type = purple_account_get_status_type_with_primitive(account, GPOINTER_TO_INT(data));
		if ((status_type != NULL) &&
			(purple_status_type_get_attr(status_type, "message") != NULL))
		{
			status_box->editor_visible = TRUE;
			break;
		}
	}
	g_list_free(accounts);

	if (gtk_widget_get_sensitive(GTK_WIDGET(status_box)))
	{
		if (status_box->editor_visible)
		{
			GtkTextIter start, end;

			gtk_widget_show_all(status_box->vbox);
			gtk_widget_grab_focus(status_box->input);

			gtk_text_buffer_get_start_iter(status_box->buffer, &start);
			gtk_text_buffer_get_end_iter(status_box->buffer, &end);
			gtk_text_buffer_select_range(status_box->buffer, &start, &end);
		}
		else
		{
			gtk_widget_hide(status_box->vbox);
			activate_currently_selected_status(status_box); /* This is where we actually set the status */
		}
	}
	pidgin_status_box_refresh(status_box);
}

static gint
get_statusbox_index(PidginStatusBox *box, PurpleSavedStatus *saved_status)
{
	gint index = -1;

	switch (purple_savedstatus_get_primitive_type(saved_status))
	{
		/* In reverse order */
		case PURPLE_STATUS_OFFLINE:
			index++;
			/* fall through */
		case PURPLE_STATUS_INVISIBLE:
			index++;
			/* fall through */
		case PURPLE_STATUS_UNAVAILABLE:
			index++;
			/* fall through */
		case PURPLE_STATUS_AWAY:
			index++;
			/* fall through */
		case PURPLE_STATUS_AVAILABLE:
			index++;
			break;
		default:
			break;
	}

	return index;
}

char *pidgin_status_box_get_message(PidginStatusBox *status_box)
{
	if (status_box->editor_visible)
		return g_strstrip(talkatu_markup_get_html(status_box->buffer, NULL));
	else
		return NULL;
}
