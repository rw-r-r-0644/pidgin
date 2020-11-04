subdir('keyrings')
subdir('notification-sound')

if PLUGINS
	autoaccept = library('autoaccept', 'autoaccept.c',
	    dependencies : [libpurple_dep, glib],
	    name_prefix : '',
	    install : true, install_dir : PURPLE_PLUGINDIR)

	buddynote = library('buddynote', 'buddynote.c',
	    dependencies : [libpurple_dep],
	    name_prefix : '',
	    install : true, install_dir : PURPLE_PLUGINDIR)

	idle = library('idle', 'idle.c',
	    dependencies : [libpurple_dep],
	    name_prefix : '',
	    install : true, install_dir : PURPLE_PLUGINDIR)

	joinpart = library('joinpart', 'joinpart.c',
	    dependencies : [libpurple_dep],
	    name_prefix : '',
	    install : true, install_dir : PURPLE_PLUGINDIR)

	log_reader = library('log_reader', 'log_reader.c',
	    dependencies : [libpurple_dep],
	    name_prefix : '',
	    install : true, install_dir : PURPLE_PLUGINDIR)

	offlinemsg = library('offlinemsg', 'offlinemsg.c',
	    dependencies : [libpurple_dep],
	    name_prefix : '',
	    install : true, install_dir : PURPLE_PLUGINDIR)

	psychic = library('psychic', 'psychic.c',
	    dependencies : [libpurple_dep],
	    name_prefix : '',
	    install : true, install_dir : PURPLE_PLUGINDIR)

	statenotify = library('statenotify', 'statenotify.c',
	    dependencies : [libpurple_dep],
	    name_prefix : '',
	    install : true, install_dir : PURPLE_PLUGINDIR)

	purple_toast = library('purple-toast', 'purple-toast.c',
		dependencies : [libpurple_dep],
		name_prefix: '',
		install : true, install_dir : PURPLE_PLUGINDIR)

endif  # PLUGINS
