NULL_SOURCES = [
	'nullprpl.h',
	'nullprpl.c'
]

# nullprpl isn't built by default; when it is built, it's dynamically linked
if DYNAMIC_NULL
	null_prpl = shared_library('null', NULL_SOURCES,
	    c_args : ['-DG_LOG_USE_STRUCTURED', '-DG_LOG_DOMAIN="Purple-Null"'],
	    dependencies : [libpurple_dep, glib],
	    install : false, install_dir : PURPLE_PLUGINDIR)

	devenv.append('PURPLE_PLUGIN_PATH', meson.current_build_dir())
endif
