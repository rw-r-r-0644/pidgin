SILC_SOURCES = [
	'buddy.c',
	'chat.c',
	'ft.c',
	'ft.h',
	'ops.c',
	'pk.c',
	'silc.c',
	'silcpurple.h',
	'util.c',
	'wb.c',
	'wb.h'
]

if DYNAMIC_SILC
	silc_resources = gnome.compile_resources('silcresource',
		'resources/silc.gresource.xml',
		source_dir : 'resources',
		c_name : 'silc')
	SILC_SOURCES += silc_resources

	silc_prpl = shared_library('silcpurple', SILC_SOURCES,
	    dependencies : [silc, libpurple_dep, glib],
	    install : true, install_dir : PURPLE_PLUGINDIR)

	devenv.append('PURPLE_PLUGIN_PATH', meson.current_build_dir())
endif
