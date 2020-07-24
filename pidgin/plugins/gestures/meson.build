gestures_SOURCES = [
	'gestures.c',
	'gstroke.h',
	'gstroke-internal.h',
	'stroke.c',
	'stroke-draw.c'
]

if PLUGINS
	gestures = library('gestures', gestures_SOURCES,
	    dependencies : [libpurple_dep, libpidgin_dep, glib],
	    name_prefix : '',
	    install : true, install_dir : PIDGIN_PLUGINDIR)
endif
