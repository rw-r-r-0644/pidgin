xmppdisco_SOURCES = [
	'gtkdisco.c',
	'gtkdisco.h',
	'xmppdisco.c',
	'xmppdisco.h'
]

xmppdisco_resource = gnome.compile_resources('xmppdiscoresources',
    'resources/xmppdisco.gresource.xml',
    source_dir : 'resources',
    c_name : 'xmppdisco')
xmppdisco_SOURCES += xmppdisco_resource

xmppdisco = library('xmppdisco', xmppdisco_SOURCES,
    c_args : ['-DG_LOG_USE_STRUCTURED', '-DG_LOG_DOMAIN="PidginPlugin-XMPPDisco"'],
    dependencies : [libpurple_dep, libpidgin_dep, glib],
    name_prefix : '',
    install : true, install_dir : PIDGIN_PLUGINDIR)

devenv.append('PIDGIN_PLUGIN_PATH', meson.current_build_dir())
