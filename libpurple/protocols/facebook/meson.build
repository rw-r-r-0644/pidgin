FACEBOOK_SOURCES = [
	'api.c',
	'api.h',
	'data.c',
	'data.h',
	'facebook.h',
	'facebook.c',
	'http.c',
	'http.h',
	'id.h',
	'json.c',
	'json.h',
	'mqtt.c',
	'mqtt.h',
	'thrift.c',
	'thrift.h',
	'util.c',
	'util.h'
]

if DYNAMIC_FACEBOOK
	facebook_resources = gnome.compile_resources('facebookresource',
		'resources/facebook.gresource.xml',
		source_dir : 'resources',
		c_name : 'facebook')
	FACEBOOK_SOURCES += facebook_resources

	facebook_prpl = shared_library('facebook', FACEBOOK_SOURCES,
	    dependencies : [json, libpurple_dep, libsoup, glib],
	    install : true, install_dir : PURPLE_PLUGINDIR)

	# Used to produce docs.
	facebook_inc = include_directories('.')
	facebook_dep = declare_dependency(
	    link_with : facebook_prpl,
	    dependencies : [json, libpurple_dep, glib])

	devenv.append('PURPLE_PLUGIN_PATH', meson.current_build_dir())
endif
