if nettle.found()
	internalkeyring_plugin = library('internalkeyring', 'internalkeyring.c',
	    c_args : ['-DG_LOG_USE_STRUCTURED', '-DG_LOG_DOMAIN="Purple-InternalKeyring"'],
	    dependencies : [nettle, libpurple_dep],
	    name_prefix : '',
	    build_by_default : false,
	    install : false, install_dir : PURPLE_PLUGINDIR)
endif

if IS_WIN32
	wincred_plugin = library('wincred', 'wincred.c',
	    c_args : ['-DG_LOG_USE_STRUCTURED', '-DG_LOG_DOMAIN="Purple-WinCredKeyring"'],
	    dependencies : [libpurple_dep],
	    name_prefix : '',
	    install : true, install_dir : PURPLE_PLUGINDIR)
endif

if nettle.found() or secretservice.found() or IS_WIN32
	devenv.append('PURPLE_PLUGIN_PATH', meson.current_build_dir())
endif
