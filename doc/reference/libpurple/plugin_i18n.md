Title: Third Party Plugin Translation
Slug: 3rd-party-plugin-i18n

## Third Party Plugin Translation

### Introduction

For the purpose of this document we're going to assume that your plugin:

* Is set up to use autotools. It may be possible to add translation support
without autotools, but we have no idea how. We may not want to know, either ;)
* Has an autogen.sh. You may have also called this bootstrap.sh or similar.
* Resides in a source tree that has `configure.ac` and `Makefile.am` in the
top-level directory as well as a `src` directory in which the plugin's source
is located. A `Makefile.am` should also exist in the `src` directory.

### Steps To Follow

For a plugin to have translation support there are a few steps that need to
followed:

* In your `autogen.sh`, add the following after your other utility checks:

```sh
(intltoolize --version) < /dev/null > /dev/null 2>&1 || {
    echo;
    echo "You must have intltool installed to compile <YOUR PLUGIN NAME>";
    echo;
    exit;
}
```

* Then before your call to aclocal add:

```sh
intltoolize --force --copy
```

* Now edit `configure.ac` and add the following:

```m4
AC_PROG_INTLTOOL

GETTEXT_PACKAGE=<YOUR PLUGIN NAME>
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, ["$GETTEXT_PACKAGE"], [Define the gettext package to be used])

ALL_LINGUAS=""
AM_GLIB_GNU_GETTEXT
```

The position of these macros in the file don't really matter, but if you
have issues either play around with it or feel free to ask one of the Pidgin
developers. Finally add `po/Makefile.in` to you `AC_OUTPUT` command.

* Now create a directory named 'po'.

* `cd` into the `po` directory.

* Create/edit the file `POTFILES.in` in your favorite editor. Each line
should be the name of a file that could or does have strings marked for
translating (we're getting to that step). These file names should be
relative to the top directory of your plugin's source tree.

* `cd` back to the top directory of your plugin's source tree.

* Open `Makefile.am` and add `po` to your `SUBDIRS` variable.

* While still in the top directory of your plugin's source tree, execute
`intltool-prepare`. This will setup anything extra that intltool needs.

* Fire off `autogen.sh` and when it's completed, verify that you have a
`po/POTFILES` (notice the lack of a .in). If you do, everything should be
set on the autotools side.

* Take a break, stretch your legs, smoke a cigarette, whatever, because
we're done with the autotools part.

* When you're ready, `cd` into the directory with the source files for your
plugin.

* Open the file containing the `plugin_query` function.

* If you're not already, please make sure that you are including the
`config.h` file for you plugin.  Note that `config.h` could be whatever
you told autohead to use with AM_CONFIG_HEADER. Also add the following:

```c
#include <glib/gi18n-lib.h>
```

Make sure that this include is after you include of your `config.h`,
otherwise you will break your build. Also note that if you wish to
maintain compatibility with older versions of GLib, you will need to
include additional preprocessor directives, which we won't cover here.

* This is where things get a bit goofy. libpurple is going to try to
translate our strings using the libpurple gettext package.  So we have to
convert them before libpurple attempts to.

* To do this, we're going to change the entries for `name`, `summary`, and
`description` to `NULL`.

* Next, locate your `plugin_load` function.  Your name for this function will be
the first parameter to `GPLUGIN_NATIVE_PLUGIN_DECLARE()` plus `_load`.

* Now add the following within your 'plugin_load' function:

```c
	bindtextdomain(GETTEXT_PACKAGE, PURPLE_LOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");

	info.name        = _("<YOUR PLUGIN NAME>");
	info.summary     = _("<YOUR PLUGIN SUMMARY>");
	info.description = _("<YOUR PLUGIN DESCRIPTION>");
```

> Note that the `_()` is intentional, and that it is telling intltool that
> this string should be translated. There is also `N_()` which says that a
> string should only be marked for translation but should not be translated
> yet.

* Go through the rest of your code and mark all the other strings for
translation with `_()`.

* When that's done, feel free to commit your work, create your po template
(pot file) or whatever.

* To create you po template, `cd` to `po` and execute:

```sh
intltool-update --pot
```

* To add new translations to your plugin, all you have to do is add the
language code to the `ALL_LINGUAS` variable in your `configure.ac`. Take
note that this list of languages should be separated by a space. After
you have added the language code to `ALL_LINGUAS`, drop the `xx.po` file
into `po`, and re-`autogen.sh`. After a full build you should now be
able to use the translation.
