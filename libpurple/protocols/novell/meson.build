NOVELL_SOURCES = [
	'nmfield.h',
	'nmfield.c',
	'nmconn.h',
	'nmconn.c',
	'nmconference.h',
	'nmconference.c',
	'nmcontact.h',
	'nmcontact.c',
	'nmevent.h',
	'nmevent.c',
	'nmmessage.h',
	'nmmessage.c',
	'nmrequest.h',
	'nmrequest.c',
	'nmrtf.h',
	'nmrtf.c',
	'nmuser.h',
	'nmuser.c',
	'nmuserrecord.h',
	'nmuserrecord.c',
	'novell.h',
	'novell.c'
]

if DYNAMIC_NOVELL
	novell_resources = gnome.compile_resources('novellresource',
		'resources/novell.gresource.xml',
		source_dir : 'resources',
		c_name : 'novell')
	NOVELL_SOURCES += novell_resources

	novell_prpl = shared_library('novell', NOVELL_SOURCES,
	    dependencies : [libpurple_dep, glib, ws2_32],
	    install : true, install_dir : PURPLE_PLUGINDIR)

	devenv.append('PURPLE_PLUGIN_PATH', meson.current_build_dir())
endif
