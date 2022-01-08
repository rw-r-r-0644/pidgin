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

#if !defined(PURPLE_GLOBAL_HEADER_INSIDE) && !defined(PURPLE_COMPILATION)
# error "only <purple.h> may be included directly"
#endif

#ifndef PURPLE_CORE_H
#define PURPLE_CORE_H

#include <glib.h>
#include <glib-object.h>

#include <libpurple/purplecoreuiops.h>

typedef struct PurpleCore PurpleCore;

G_BEGIN_DECLS

/**
 * purple_core_init:
 * @ui: The ID of the UI using the core. This should be a
 *           unique ID, registered with the purple team.
 *
 * Initializes the core of purple.
 *
 * This will setup preferences for all the core subsystems.
 *
 * Returns: %TRUE if successful, or %FALSE otherwise.
 */
gboolean purple_core_init(const char *ui);

/**
 * purple_core_quit:
 *
 * Quits the core of purple, which, depending on the UI, may quit the
 * application using the purple core.
 */
void purple_core_quit(void);

/**
 * purple_core_quit_cb:
 * @unused: This argument is for consistency with a timeout callback. It is
 *          otherwise unused.
 *
 * Calls purple_core_quit().  This can be used as the function passed to
 * g_timeout_add() when you want to shutdown Purple in a specified amount of
 * time.  When shutting down Purple from a plugin, you must use this instead of
 * purple_core_quit(); for an immediate exit, use a timeout value of 0:
 *
 * <programlisting>
 * g_timeout_add(0, purple_core_quit_cb, NULL)
 * </programlisting>
 *
 * This ensures that code from your plugin is not being executed when
 * purple_core_quit() is called.  If the plugin called purple_core_quit()
 * directly, you would get a core dump after purple_core_quit() executes and
 * control returns to your plugin because purple_core_quit() frees all plugins.
 */
gboolean purple_core_quit_cb(gpointer unused);

/**
 * purple_core_get_version:
 *
 * Returns the version of the core library.
 *
 * Returns: The version of the core library.
 */
const char *purple_core_get_version(void);

/**
 * purple_core_get_ui:
 *
 * Returns the ID of the UI that is using the core, as passed to
 * purple_core_init().
 *
 * Returns: The ID of the UI that is currently using the core.
 */
const char *purple_core_get_ui(void);

/**
 * purple_get_core:
 *
 * This is used to connect to
 * <link linkend="chapter-signals-core">core signals</link>.
 *
 * Returns: (transfer none): A handle to the purple core.
 */
PurpleCore *purple_get_core(void);

/**
 * purple_core_set_ui_ops:
 * @ops: A UI ops structure for the core.
 *
 * Sets the UI ops for the core.
 */
void purple_core_set_ui_ops(PurpleCoreUiOps *ops);

/**
 * purple_core_get_ui_ops:
 *
 * Returns the UI ops for the core.
 *
 * Returns: The core's UI ops structure.
 */
PurpleCoreUiOps *purple_core_get_ui_ops(void);

/**
 * purple_core_get_ui_info:
 *
 * Returns a #PurpleUiInfo that contains information about the user interface.
 *
 * Returns: (transfer full): A #PurpleUiInfo instance.
 */
PurpleUiInfo* purple_core_get_ui_info(void);

G_END_DECLS

#endif /* PURPLE_CORE_H */

/*

                                                  /===-
                                                `//"\\   """"`---.___.-""
             ______-==|                         | |  \\           _-"`
       __--"""  ,-/-==\\                        | |   `\        ,'
    _-"       /'    |  \\            ___         / /      \      /
  .'        /       |   \\         /"   "\    /' /        \   /'
 /  ____  /         |    \`\.__/-""  D O   \_/'  /          \/'
/-'"    """""---__  |     "-/"   O G     R   /'        _--"`
                  \_|      /   R    __--_  t ),   __--""
                    '""--_/  T   _-"_>--<_\ h '-" \
                   {\__--_/}    / \\__>--<__\ e B  \
                   /'   (_/  _-"  | |__>--<__|   U  |
                  |   _/) )-"     | |__>--<__|  R   |
                  / /" ,_/       / /__>---<__/ N    |
                 o-o _//        /-"_>---<__-" I    /
                 (^("          /"_>---<__-  N   _-"
                ,/|           /__>--<__/  A  _-"
             ,//('(          |__>--<__|  T  /                  .----_
            ( ( '))          |__>--<__|    |                 /' _---_"\
         `-)) )) (           |__>--<__|  O |               /'  /     "\`\
        ,/,'//( (             \__>--<__\  R \            /'  //        ||
      ,( ( ((, ))              "-__>--<_"-_  "--____---"' _/'/        /'
    `"/  )` ) ,/|                 "-_">--<_/-__       __-" _/
  ._-"//( )/ )) `                    ""-'_/_/ /"""""""__--"
   ;'( ')/ ,)(                              """"""""""
  ' ') '( (/
    '   '  `

*/
