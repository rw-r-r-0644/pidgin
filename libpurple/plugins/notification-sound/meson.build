if target_machine.system() != 'windows'
	CANBERRA = dependency('libcanberra', version: '>= 0.30')

	library('notification-sound', 'notification-sound.c',
		c_args : ['-DG_LOG_USE_STRUCTURED', '-DG_LOG_DOMAIN="PurplePlugin-NotificationSound"'],
		dependencies : [libpurple_dep, glib, CANBERRA],
		name_prefix : '',
		install : true, install_dir : PURPLE_PLUGINDIR)

	devenv.append('PURPLE_PLUGIN_PATH', meson.current_build_dir())
endif
