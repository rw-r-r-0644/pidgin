subdir('kwallet')

if nettle.found()
	internalkeyring_plugin = library('internalkeyring', 'internalkeyring.c',
	    dependencies : [nettle, libpurple_dep],
	    name_prefix : '',
	    build_by_default : false,
	    install : false, install_dir : PURPLE_PLUGINDIR)
endif

if secretservice.found()
	secretsservice_plugin = library('secretservice', 'secretservice.c',
	    dependencies : [secretservice, libpurple_dep],
	    name_prefix : '',
	    install : true, install_dir : PURPLE_PLUGINDIR)
endif

if IS_WIN32
	wincred_plugin = library('wincred', 'wincred.c',
	    dependencies : [libpurple_dep],
	    name_prefix : '',
	    install : true, install_dir : PURPLE_PLUGINDIR)
endif

if nettle.found() or secretservice.found() or IS_WIN32
	devenv.append('PURPLE_PLUGIN_PATH', meson.current_build_dir())
endif
