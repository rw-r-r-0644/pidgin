SAMETIME_SOURCES = [
	'sametime.c',
	'sametime.h',
	'im_mime.c',
	'im_mime.h',
]

if DYNAMIC_SAMETIME
	sametime_resources = gnome.compile_resources('sametimeresource',
		'resources/sametime.gresource.xml',
		source_dir : 'resources',
		c_name : 'sametime')
	SAMETIME_SOURCES += sametime_resources

	sametime_prpl = shared_library('sametime', SAMETIME_SOURCES,
	    c_args : ['-DG_LOG_DOMAIN="sametime"'],
	    dependencies : [meanwhile, gmime, libpurple_dep, glib],
	    install : true, install_dir : PURPLE_PLUGINDIR)

	devenv.append('PURPLE_PLUGIN_PATH', meson.current_build_dir())

	subdir('tests')
endif
