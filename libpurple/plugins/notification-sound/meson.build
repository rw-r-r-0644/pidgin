if PLUGINS

CANBERRA = dependency('libcanberra', version: '>= 0.30')

library('notification-sound', 'notification-sound.c',
    dependencies : [libpurple_dep, glib, CANBERRA],
    name_prefix : '',
    install : true, install_dir : PURPLE_PLUGINDIR)

endif

