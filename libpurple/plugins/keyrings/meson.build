if nettle.found()
	internalkeyring_plugin = library('internalkeyring', 'internalkeyring.c',
	    c_args : ['-DG_LOG_USE_STRUCTURED', '-DG_LOG_DOMAIN="Purple-InternalKeyring"'],
	    dependencies : [nettle, libpurple_dep],
	    name_prefix : '',
	    build_by_default : false,
	    install : false, install_dir : PURPLE_PLUGINDIR)
endif

if nettle.found() or secretservice.found()
	devenv.append('PURPLE_PLUGIN_PATH', meson.current_build_dir())
endif
