if x11.found()
	gntclipboard = library('gntclipboard', 'gntclipboard.c',
	    c_args : ['-DG_LOG_USE_STRUCTURED', '-DG_LOG_DOMAIN="Gnt-Clipboard"'],
	    dependencies : [x11, libpurple_dep, libfinch_dep, glib],
	    name_prefix : '',
	    install : true, install_dir : FINCH_PLUGINDIR)

	gntgf = library('gntgf', 'gntgf.c',
	    c_args : [
	        ncurses_header,
	        '-DG_LOG_USE_STRUCTURED',
	        '-DG_LOG_DOMAIN="Gnt-GF"',
	    ],
	    dependencies : [x11, libpurple_dep, libfinch_dep, ncurses, glib],
	    name_prefix : '',
	    install : true, install_dir : FINCH_PLUGINDIR)
endif

gntlastlog = library('gntlastlog', 'lastlog.c',
    c_args : ['-DG_LOG_USE_STRUCTURED', '-DG_LOG_DOMAIN="Gnt-LastLog"'],
    dependencies : [libpurple_dep, libfinch_dep, ncurses, glib],
    name_prefix : '',
    install : true, install_dir : FINCH_PLUGINDIR)

gnttinyurl = library('gnttinyurl', 'gnttinyurl.c',
    c_args : ['-DG_LOG_USE_STRUCTURED', '-DG_LOG_DOMAIN="Gnt-TinyURL"'],
    dependencies : [libpurple_dep, libfinch_dep, libsoup, glib],
    name_prefix : '',
    install : true, install_dir : FINCH_PLUGINDIR)

grouping = library('grouping', 'grouping.c',
    c_args : ['-DG_LOG_USE_STRUCTURED', '-DG_LOG_DOMAIN="Gnt-Grouping"'],
    dependencies : [libpurple_dep, libfinch_dep, glib],
    name_prefix : '',
    install : true, install_dir : FINCH_PLUGINDIR)

devenv.append('FINCH_PLUGIN_PATH', meson.current_build_dir())
