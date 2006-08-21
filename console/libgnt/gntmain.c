#include <ncursesw/panel.h>

#include "gnt.h"
#include "gntbox.h"
#include "gntcolors.h"
#include "gntkeys.h"
#include "gntstyle.h"
#include "gnttree.h"

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

/**
 * Notes: Interesting functions to look at:
 * 	scr_dump, scr_init, scr_restore: for workspaces
 *
 * 	Need to wattrset for colors to use with PDCurses.
 */

static int lock_focus_list;
static GList *focus_list;

static int X_MIN;
static int X_MAX;
static int Y_MIN;
static int Y_MAX;

static gboolean ascii_only;

static GMainLoop *loop;
static struct
{
	GntWidget *window;
	GntWidget *tree;
} window_list;

typedef struct
{
	GntWidget *me;

	PANEL *panel;
} GntNode;

typedef enum
{
	GNT_KP_MODE_NORMAL,
	GNT_KP_MODE_RESIZE,
	GNT_KP_MODE_MOVE,
	GNT_KP_MODE_MENU,
	GNT_KP_MODE_WINDOW_LIST
} GntKeyPressMode;

static GHashTable *nodes;

static void free_node(gpointer data);
static void draw_taskbar(gboolean reposition);
static void bring_on_top(GntWidget *widget);

static gboolean
update_screen(gpointer null)
{
	update_panels();
	doupdate();
	return TRUE;
}

void gnt_screen_take_focus(GntWidget *widget)
{
	GntWidget *w = NULL;

	if (lock_focus_list)
		return;
	if (g_list_find(g_list_first(focus_list), widget))
		return;

	if (focus_list)
		w = focus_list->data;

	/* XXX: ew */
	focus_list = g_list_first(focus_list);
	focus_list = g_list_append(focus_list, widget);
	focus_list = g_list_find(focus_list, w ? w : widget);

	gnt_widget_set_focus(widget, TRUE);
	if (w)
		gnt_widget_set_focus(w, FALSE);
	draw_taskbar(FALSE);
}

void gnt_screen_remove_widget(GntWidget *widget)
{
	int pos = g_list_index(g_list_first(focus_list), widget);
	GList *next;

	if (lock_focus_list)
		return;

	if (pos == -1)
		return;

	focus_list = g_list_first(focus_list);
	focus_list = g_list_remove(focus_list, widget);
	next = g_list_nth(focus_list, pos - 1);
	if (next)
		focus_list = next;

	if (focus_list)
	{
		bring_on_top(focus_list->data);
	}
	draw_taskbar(FALSE);
}

static void
bring_on_top(GntWidget *widget)
{
	GntNode *node = g_hash_table_lookup(nodes, widget);

	g_return_if_fail(focus_list->data == widget);

	if (!node)
		return;

	gnt_widget_set_focus(focus_list->data, TRUE);
	gnt_widget_draw(focus_list->data);

	top_panel(node->panel);

	if (window_list.window)
	{
		GntNode *nd = g_hash_table_lookup(nodes, window_list.window);
		top_panel(nd->panel);
	}
	update_screen(NULL);
	draw_taskbar(FALSE);
}

static void
update_window_in_list(GntWidget *wid)
{
	GntTextFormatFlags flag = 0;

	if (window_list.window == NULL)
		return;

	if (wid == focus_list->data)
		flag |= GNT_TEXT_FLAG_DIM;
	else if (GNT_WIDGET_IS_FLAG_SET(wid, GNT_WIDGET_URGENT))
		flag |= GNT_TEXT_FLAG_BOLD;

	gnt_tree_set_row_flags(GNT_TREE(window_list.tree), wid, flag);
}

static void
draw_taskbar(gboolean reposition)
{
	static WINDOW *taskbar = NULL;
	GList *iter;
	int n, width = 0;
	int i;

	if (taskbar == NULL)
	{
		taskbar = newwin(1, getmaxx(stdscr), getmaxy(stdscr) - 1, 0);
	}
	else if (reposition)
	{
		mvwin(taskbar, Y_MAX, 0);
	}

	wbkgdset(taskbar, '\0' | COLOR_PAIR(GNT_COLOR_NORMAL));
	werase(taskbar);

	n = g_list_length(g_list_first(focus_list));
	if (n)
		width = getmaxx(stdscr) / n;

	for (i = 0, iter = g_list_first(focus_list); iter; iter = iter->next, i++)
	{
		GntWidget *w = iter->data;
		int color;
		const char *title;

		if (w == focus_list->data)
		{
			/* This is the current window in focus */
			color = GNT_COLOR_TITLE;
			GNT_WIDGET_UNSET_FLAGS(w, GNT_WIDGET_URGENT);
		}
		else if (GNT_WIDGET_IS_FLAG_SET(w, GNT_WIDGET_URGENT))
		{
			/* This is a window with the URGENT hint set */
			color = GNT_COLOR_TITLE_D;
		}
		else
		{
			color = GNT_COLOR_NORMAL;
		}
		wbkgdset(taskbar, '\0' | COLOR_PAIR(color));
		mvwhline(taskbar, 0, width * i, ' ' | COLOR_PAIR(color), width);
		title = GNT_BOX(w)->title;
		mvwprintw(taskbar, 0, width * i, "%s", title ? title : "<gnt>");

		update_window_in_list(w);
	}

	wrefresh(taskbar);
}

static void
switch_window(int direction)
{
	GntWidget *w = NULL;
	if (focus_list)
		w = focus_list->data;

	if (direction == 1)
	{
		if (focus_list && focus_list->next)
			focus_list = focus_list->next;
		else
			focus_list = g_list_first(focus_list);
	}
	else if (direction == -1)
	{
		if (focus_list && focus_list->prev)
			focus_list = focus_list->prev;
		else
			focus_list = g_list_last(focus_list);
	}
	
	if (focus_list)
	{
		bring_on_top(focus_list->data);
	}

	if (w && (!focus_list || w != focus_list->data))
	{
		gnt_widget_set_focus(w, FALSE);
	}
}

static void
switch_window_n(int n)
{
	GntWidget *w = NULL;
	GList *l;

	if (focus_list)
		w = focus_list->data;

	if ((l = g_list_nth(g_list_first(focus_list), n)) != NULL)
	{
		focus_list = l;
		bring_on_top(focus_list->data);
	}

	if (w && (!focus_list || w != focus_list->data))
	{
		gnt_widget_set_focus(w, FALSE);
	}
}

static void
window_list_activate(GntTree *tree, gpointer null)
{
	GntWidget *widget = gnt_tree_get_selection_data(GNT_TREE(tree));
	GntWidget *old = NULL;

	if (focus_list)
		old = focus_list->data;

	focus_list = g_list_find(g_list_first(focus_list), widget);
	bring_on_top(widget);

	if (old && (!focus_list || old != focus_list->data))
	{
		gnt_widget_set_focus(old, FALSE);
	}
}

static void
show_window_list()
{
	GntWidget *tree, *win;
	GList *iter;

	if (window_list.window)
		return;

	win = window_list.window = gnt_box_new(FALSE, FALSE);
	gnt_box_set_toplevel(GNT_BOX(win), TRUE);
	gnt_box_set_title(GNT_BOX(win), "Window List");
	gnt_box_set_pad(GNT_BOX(win), 0);

	tree = window_list.tree = gnt_tree_new();

	for (iter = g_list_first(focus_list); iter; iter = iter->next)
	{
		GntBox *box = GNT_BOX(iter->data);

		gnt_tree_add_row_last(GNT_TREE(tree), box,
				gnt_tree_create_row(GNT_TREE(tree), box->title), NULL);
		update_window_in_list(GNT_WIDGET(box));
	}

	gnt_tree_set_selected(GNT_TREE(tree), focus_list->data);
	gnt_box_add_widget(GNT_BOX(win), tree);

	gnt_tree_set_col_width(GNT_TREE(tree), 0, getmaxx(stdscr) / 3);
	gnt_widget_set_size(tree, 0, getmaxy(stdscr) / 2);
	gnt_widget_set_position(win, getmaxx(stdscr) / 3, getmaxy(stdscr) / 4);

	lock_focus_list = 1;
	gnt_widget_show(win);
	lock_focus_list = 0;

	g_signal_connect(G_OBJECT(tree), "activate", G_CALLBACK(window_list_activate), NULL);
}

static void
shift_window(GntWidget *widget, int dir)
{
	GList *all = g_list_first(focus_list);
	GList *list = g_list_find(all, widget);
	int length, pos;
	if (!list)
		return;

	length = g_list_length(all);
	pos = g_list_position(all, list);

	pos += dir;
	if (dir > 0)
		pos++;

	if (pos < 0)
		pos = length;
	else if (pos > length)
		pos = 0;

	all = g_list_insert(all, widget, pos);
	all = g_list_delete_link(all, list);
	if (focus_list == list)
		focus_list = g_list_find(all, widget);
	draw_taskbar(FALSE);
}

static void
dump_screen()
{
	int x, y;
	chtype old = 0, now = 0;
	FILE *file = fopen("dump.html", "w");

	fprintf(file, "<pre>");
	for (y = 0; y < getmaxy(stdscr); y++)
	{
		for (x = 0; x < getmaxx(stdscr); x++)
		{
			char ch;
			now = mvwinch(newscr, y, x);
			ch = now & A_CHARTEXT;
			now ^= ch;

#define CHECK(attr, start, end) \
			do \
			{  \
				if (now & attr)  \
				{  \
					if (!(old & attr))  \
						fprintf(file, start);  \
				}  \
				else if (old & attr)  \
				{  \
					fprintf(file, end);  \
				}  \
			} while (0) 

			CHECK(A_BOLD, "<b>", "</b>");
			CHECK(A_UNDERLINE, "<u>", "</u>");
			CHECK(A_BLINK, "<blink>", "</blink>");

			if ((now & A_COLOR) != (old & A_COLOR))
			{
				int ret;
				short fgp, bgp, r, g, b;
				struct
				{
					int r, g, b;
				} fg, bg;

				ret = pair_content(PAIR_NUMBER(now & A_COLOR), &fgp, &bgp);
				ret = color_content(fgp, &r, &g, &b);
				fg.r = r; fg.b = b; fg.g = g;
				ret = color_content(bgp, &r, &g, &b);
				bg.r = r; bg.b = b; bg.g = g;
#define ADJUST(x) (x = x * 255 / 1000)
				ADJUST(fg.r);
				ADJUST(fg.g);
				ADJUST(fg.b);
				ADJUST(bg.r);
				ADJUST(bg.b);
				ADJUST(bg.g);
				
				if (x) fprintf(file, "</span>");
				fprintf(file, "<span style=\"background:#%02x%02x%02x;color:#%02x%02x%02x\">",
						bg.r, bg.g, bg.b, fg.r, fg.g, fg.b);
			}
			if (now & A_ALTCHARSET)
			{
				switch (ch)
				{
					case 'q':
						ch = '-'; break;
					case 't':
					case 'u':
					case 'x':
						ch = '|'; break;
					case 'v':
					case 'w':
					case 'l':
					case 'm':
					case 'k':
					case 'j':
					case 'n':
						ch = '+'; break;
					case '-':
						ch = '^'; break;
					case '.':
						ch = 'v'; break;
					case 'a':
						ch = '#'; break;
					default:
						ch = ' '; break;
				}
			}
			if (ch == '&')
				fprintf(file, "&amp;");
			else if (ch == '<')
				fprintf(file, "&lt;");
			else if (ch == '>')
				fprintf(file, "&gt;");
			else
				fprintf(file, "%c", ch);
			old = now;
		}
		fprintf(file, "</span>\n");
		old = 0;
	}
	fprintf(file, "</pre>");
	fclose(file);
}

static void
refresh_node(GntWidget *widget, GntNode *node, gpointer null)
{
	int x, y, w, h;
	int nw, nh;

	gnt_widget_get_position(widget, &x, &y);
	gnt_widget_get_size(widget, &w, &h);

	if (x + w >= X_MAX)
		x = MAX(0, X_MAX - w);
	if (y + h >= Y_MAX)
		y = MAX(0, Y_MAX - h);
	gnt_screen_move_widget(widget, x, y);

	nw = MIN(w, X_MAX);
	nh = MIN(h, Y_MAX);
	if (nw != w || nh != h)
		gnt_screen_resize_widget(widget, nw, nh);
}

static gboolean
io_invoke(GIOChannel *source, GIOCondition cond, gpointer null)
{
	char buffer[256];
	gboolean ret = FALSE;
	static GntKeyPressMode mode = GNT_KP_MODE_NORMAL;

	int rd = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
	if (rd < 0)
	{
		endwin();
		printf("ERROR!\n");
		exit(1);
	}
	else if (rd == 0)
	{
		endwin();
		printf("EOF\n");
		exit(1);
	}

	buffer[rd] = 0;

	if (buffer[0] == 27 && buffer[1] == 'd' && buffer[2] == 0)
	{
		/* This dumps the screen contents in an html file */
		dump_screen();
	}

	gnt_keys_refine(buffer);

	if (mode == GNT_KP_MODE_NORMAL)
	{
		if (focus_list)
		{
			ret = gnt_widget_key_pressed(focus_list->data, buffer);
		}

		if (!ret)
		{
			if (buffer[0] == 27)
			{
				/* Some special key has been pressed */
				if (strcmp(buffer+1, GNT_KEY_POPUP) == 0)
				{}
				else if (strcmp(buffer + 1, "c") == 0)
				{
					/* Alt + c was pressed. I am going to use it to close a window. */
					if (focus_list)
					{
						gnt_widget_destroy(focus_list->data);
					}
				}
				else if (strcmp(buffer + 1, "q") == 0)
				{
					/* I am going to use Alt + q to quit. */
					g_main_loop_quit(loop);
				}
				else if (strcmp(buffer + 1, "n") == 0)
				{
					/* Alt + n to go to the next window */
					switch_window(1);
				}
				else if (strcmp(buffer + 1, "p") == 0)
				{
					/* Alt + p to go to the previous window */
					switch_window(-1);
				}
				else if (strcmp(buffer + 1, "m") == 0 && focus_list)
				{
					/* Move a window */
					mode = GNT_KP_MODE_MOVE;
				}
				else if (strcmp(buffer + 1, "w") == 0 && focus_list)
				{
					/* Window list */
					mode = GNT_KP_MODE_WINDOW_LIST;
					show_window_list();
				}
				else if (strcmp(buffer + 1, "r") == 0 && focus_list)
				{
					/* Resize window */
					mode = GNT_KP_MODE_RESIZE;
				}
				else if (strcmp(buffer + 1, ",") == 0 && focus_list)
				{
					/* Re-order the list of windows */
					shift_window(focus_list->data, -1);
				}
				else if (strcmp(buffer + 1, ".") == 0 && focus_list)
				{
					shift_window(focus_list->data, 1);
				}
				else if (strcmp(buffer + 1, "l") == 0)
				{
					update_screen(NULL);
					werase(stdscr);
					wrefresh(stdscr);

					X_MAX = getmaxx(stdscr);
					Y_MAX = getmaxy(stdscr) - 1;

					g_hash_table_foreach(nodes, (GHFunc)refresh_node, NULL);

					update_screen(NULL);
					draw_taskbar(TRUE);
				}
				else if (strlen(buffer) == 2 && isdigit(*(buffer + 1)))
				{
					int n = *(buffer + 1) - '0';

					if (n == 0)
						n = 10;

					switch_window_n(n - 1);
				}
			}
		}
	}
	else if (mode == GNT_KP_MODE_MOVE && focus_list)
	{
		if (buffer[0] == 27)
		{
			gboolean changed = FALSE;
			int x, y, w, h;
			GntWidget *widget = GNT_WIDGET(focus_list->data);

			gnt_widget_get_position(widget, &x, &y);
			gnt_widget_get_size(widget, &w, &h);

			if (strcmp(buffer + 1, GNT_KEY_LEFT) == 0)
			{
				if (x > X_MIN)
				{
					x--;
					changed = TRUE;
				}
			}
			else if (strcmp(buffer + 1, GNT_KEY_RIGHT) == 0)
			{
				if (x + w < X_MAX)
				{
					x++;
					changed = TRUE;
				}
			}
			else if (strcmp(buffer + 1, GNT_KEY_UP) == 0)
			{
				if (y > Y_MIN)
				{
					y--;
					changed = TRUE;
				}						
			}
			else if (strcmp(buffer + 1, GNT_KEY_DOWN) == 0)
			{
				if (y + h < Y_MAX)
				{
					y++;
					changed = TRUE;
				}
			}
			else if (buffer[1] == 0)
			{
				mode = GNT_KP_MODE_NORMAL;
				changed = TRUE;
				gnt_widget_draw(widget);
			}

			if (changed)
			{
				gnt_screen_move_widget(widget, x, y);
			}
		}
		else if (*buffer == '\r')
		{
			mode = GNT_KP_MODE_NORMAL;
		}
	}
	else if (mode == GNT_KP_MODE_WINDOW_LIST && window_list.window)
	{
		gnt_widget_key_pressed(window_list.window, buffer);

		if (buffer[0] == '\r' || (buffer[0] == 27 && buffer[1] == 0))
		{
			mode = GNT_KP_MODE_NORMAL;
			lock_focus_list = 1;
			gnt_widget_destroy(window_list.window);
			window_list.window = NULL;
			window_list.tree = NULL;
			lock_focus_list = 0;
		}
	}
	else if (mode == GNT_KP_MODE_RESIZE)
	{
		if (buffer[0] == '\r' || (buffer[0] == 27 && buffer[1] == 0))
			mode = GNT_KP_MODE_NORMAL;
		else if (buffer[0] == 27)
		{
			GntWidget *widget = focus_list->data;
			gboolean changed = FALSE;
			int width, height;

			gnt_widget_get_size(widget, &width, &height);

			if (strcmp(buffer + 1, GNT_KEY_DOWN) == 0)
			{
				if (widget->priv.y + height < Y_MAX)
				{
					height++;
					changed = TRUE;
				}
			}
			else if (strcmp(buffer + 1, GNT_KEY_UP) == 0)
			{
				height--;
				changed = TRUE;
			}
			else if (strcmp(buffer + 1, GNT_KEY_LEFT) == 0)
			{
				width--;
				changed = TRUE;
			}
			else if (strcmp(buffer + 1, GNT_KEY_RIGHT) == 0)
			{
				if (widget->priv.x + width < X_MAX)
				{
					width++;
					changed = TRUE;
				}
			}

			if (changed)
			{
				gnt_screen_resize_widget(widget, width, height);
			}
		}
	}

	refresh();

	return TRUE;
}

void gnt_init()
{
	static GIOChannel *channel = NULL;
	char *filename;
	int result;
	const char *locale;

	if (channel)
		return;
	
	channel = g_io_channel_unix_new(STDIN_FILENO);

	g_io_channel_set_encoding(channel, NULL, NULL);
	g_io_channel_set_buffered(channel, FALSE);
	g_io_channel_set_flags(channel, G_IO_FLAG_NONBLOCK, NULL );

	result = g_io_add_watch_full(channel,  G_PRIORITY_HIGH,
					(G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_PRI | G_IO_NVAL),
					io_invoke, NULL, NULL);
	
	locale = setlocale(LC_ALL, "");

#if 0
	g_io_channel_unref(channel);  /* Apparently this causes crash for some people */
#endif

	if (locale && (strstr(locale, "UTF") || strstr(locale, "utf")))
		ascii_only = FALSE;
	else
		ascii_only = TRUE;

	initscr();
	typeahead(-1);
	noecho();
	curs_set(0);

	gnt_init_colors();
	gnt_init_styles();

	filename = g_build_filename(g_get_home_dir(), ".gntrc", NULL);
	gnt_style_read_configure_file(filename);
	g_free(filename);

	X_MIN = 0;
	Y_MIN = 0;
	X_MAX = getmaxx(stdscr);
	Y_MAX = getmaxy(stdscr) - 1;

	nodes = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, free_node);

	wbkgdset(stdscr, '\0' | COLOR_PAIR(GNT_COLOR_NORMAL));
	refresh();
#if 0
	mousemask(NCURSES_BUTTON_PRESSED | NCURSES_BUTTON_RELEASED | REPORT_MOUSE_POSITION, NULL);
#endif
	wbkgdset(stdscr, '\0' | COLOR_PAIR(GNT_COLOR_NORMAL));
	werase(stdscr);
	wrefresh(stdscr);

	g_type_init();
}

void gnt_main()
{
	loop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(loop);
}

/*********************************
 * Stuff for 'window management' *
 *********************************/

static void
free_node(gpointer data)
{
	GntNode *node = data;
	hide_panel(node->panel);
	del_panel(node->panel);
	g_free(node);
}

void gnt_screen_occupy(GntWidget *widget)
{
	GntNode *node;

	while (widget->parent)
		widget = widget->parent;
	
	if (g_hash_table_lookup(nodes, widget))
		return;		/* XXX: perhaps _update instead? */

	node = g_new0(GntNode, 1);
	node->me = widget;

	g_hash_table_replace(nodes, widget, node);

	if (window_list.window)
	{
		if ((GNT_IS_BOX(widget) && GNT_BOX(widget)->title) && window_list.window != widget
				&& GNT_WIDGET_IS_FLAG_SET(widget, GNT_WIDGET_CAN_TAKE_FOCUS))
		{
			gnt_tree_add_row_last(GNT_TREE(window_list.tree), widget,
					gnt_tree_create_row(GNT_TREE(window_list.tree), GNT_BOX(widget)->title),
					NULL);
			update_window_in_list(widget);
		}
	}

	update_screen(NULL);
}

void gnt_screen_release(GntWidget *widget)
{
	GntNode *node;

	gnt_screen_remove_widget(widget);
	node = g_hash_table_lookup(nodes, widget);

	if (node == NULL)	/* Yay! Nothing to do. */
		return;

	g_hash_table_remove(nodes, widget);

	if (window_list.window)
	{
		gnt_tree_remove(GNT_TREE(window_list.tree), widget);
	}

	update_screen(NULL);
}

void gnt_screen_update(GntWidget *widget)
{
	GntNode *node;
	
	while (widget->parent)
		widget = widget->parent;
	
	gnt_box_sync_children(GNT_BOX(widget));
	node = g_hash_table_lookup(nodes, widget);
	if (node && !node->panel)
	{
		node->panel = new_panel(node->me->window);
		if (!GNT_WIDGET_IS_FLAG_SET(node->me, GNT_WIDGET_TRANSIENT))
		{
			bottom_panel(node->panel);     /* New windows should not grab focus */
			gnt_widget_set_urgent(node->me);
		}
	}

	if (window_list.window)
	{
		GntNode *nd = g_hash_table_lookup(nodes, window_list.window);
		top_panel(nd->panel);
	}

	update_screen(NULL);
}

gboolean gnt_widget_has_focus(GntWidget *widget)
{
	GntWidget *w;
	if (!widget)
		return FALSE;

	w = widget;

	while (widget->parent)
		widget = widget->parent;

	if (widget == window_list.window)
		return TRUE;

	if (focus_list && focus_list->data == widget)
	{
		if (GNT_IS_BOX(widget) &&
				(GNT_BOX(widget)->active == w || widget == w))
			return TRUE;
	}

	return FALSE;
}

void gnt_widget_set_urgent(GntWidget *widget)
{
	while (widget->parent)
		widget = widget->parent;

	if (focus_list && focus_list->data == widget)
		return;

	GNT_WIDGET_SET_FLAGS(widget, GNT_WIDGET_URGENT);
	draw_taskbar(FALSE);
}

void gnt_quit()
{
	gnt_uninit_colors();
	gnt_uninit_styles();
	endwin();
}

gboolean gnt_ascii_only()
{
	return ascii_only;
}

void gnt_screen_resize_widget(GntWidget *widget, int width, int height)
{
	if (widget->parent == NULL)
	{
		GntNode *node = g_hash_table_lookup(nodes, widget);
		if (!node)
			return;

		hide_panel(node->panel);
		gnt_widget_set_size(widget, width, height);
		gnt_widget_draw(widget);
		replace_panel(node->panel, widget->window);
		show_panel(node->panel);
		update_screen(NULL);
	}
}

void gnt_screen_move_widget(GntWidget *widget, int x, int y)
{
	GntNode *node = g_hash_table_lookup(nodes, widget);
	gnt_widget_set_position(widget, x, y);
	move_panel(node->panel, y, x);
	update_screen(NULL);
}

