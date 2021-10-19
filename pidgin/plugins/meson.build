subdir('disco')
subdir('gestures')
subdir('xmppconsole')

gtkbuddynote = library('gtkbuddynote', 'gtkbuddynote.c',
    c_args : ['-DG_LOG_USE_STRUCTURED', '-DG_LOG_DOMAIN="PidginPlugin-BuddyNote"'],
    dependencies : [libpurple_dep, libpidgin_dep, glib],
    name_prefix : '',
    install : true, install_dir : PIDGIN_PLUGINDIR)

iconaway = library('iconaway', 'iconaway.c',
    c_args : ['-DG_LOG_USE_STRUCTURED', '-DG_LOG_DOMAIN="PidginPlugin-IconAway"'],
    dependencies : [libpurple_dep, libpidgin_dep, glib],
    name_prefix : '',
    install : true, install_dir : PIDGIN_PLUGINDIR)

notify = library('notify', 'notify.c',
    c_args : ['-DG_LOG_USE_STRUCTURED', '-DG_LOG_DOMAIN="PidginPlugin-Notify"'],
    dependencies : [libpurple_dep, libpidgin_dep, glib],
    name_prefix : '',
    build_by_default: false,
    install : false, install_dir : PIDGIN_PLUGINDIR)

relnot = library('relnot', 'relnot.c',
    c_args : ['-DG_LOG_USE_STRUCTURED', '-DG_LOG_DOMAIN="PidginPlugin-RelNot"'],
    dependencies : [libpurple_dep, libpidgin_dep, libsoup, glib],
    name_prefix : '',
    install : true, install_dir : PIDGIN_PLUGINDIR)

spellchk = library('spellchk', 'spellchk.c',
    c_args : ['-DG_LOG_USE_STRUCTURED', '-DG_LOG_DOMAIN="PidginPlugin-SpellCheck"'],
    dependencies : [libpurple_dep, libpidgin_dep, glib],
    name_prefix : '',
    build_by_default: false,
    install : false, install_dir : PIDGIN_PLUGINDIR)

if IS_WIN32
	transparency = library('transparency', 'transparency.c',
		c_args : ['-DG_LOG_USE_STRUCTURED', '-DG_LOG_DOMAIN="PidginPlugin-Transparency"'],
		dependencies : [libpurple_dep, libpidgin_dep, glib],
		name_prefix : '',
		install : true, install_dir : PIDGIN_PLUGINDIR)
endif

if enable_unity
	unity = library('unity', 'unity.c',
	    c_args : ['-DG_LOG_USE_STRUCTURED', '-DG_LOG_DOMAIN="PidginPlugin-Unity"'],
	    dependencies : [UNITY, libpurple_dep, libpidgin_dep, glib],
	    name_prefix : '',
	    install : true, install_dir : PIDGIN_PLUGINDIR)
endif

devenv.append('PIDGIN_PLUGIN_PATH', meson.current_build_dir())
