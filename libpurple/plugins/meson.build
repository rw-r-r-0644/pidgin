subdir('keyrings')
subdir('kwallet')
subdir('notification-sound')

autoaccept = library('autoaccept', 'autoaccept.c',
    c_args : ['-DG_LOG_USE_STRUCTURED', '-DG_LOG_DOMAIN="PurplePlugin-AutoAccept"'],
    dependencies : [libpurple_dep, glib],
    name_prefix : '',
    install : true, install_dir : PURPLE_PLUGINDIR)

buddynote = library('buddynote', 'buddynote.c',
    c_args : ['-DG_LOG_USE_STRUCTURED', '-DG_LOG_DOMAIN="PurplePlugin-BuddyNote"'],
    dependencies : [libpurple_dep],
    name_prefix : '',
    install : true, install_dir : PURPLE_PLUGINDIR)

idle = library('idle', 'idle.c',
    c_args : ['-DG_LOG_USE_STRUCTURED', '-DG_LOG_DOMAIN="PurplePlugin-Idle"'],
    dependencies : [libpurple_dep],
    name_prefix : '',
    install : true, install_dir : PURPLE_PLUGINDIR)

joinpart = library('joinpart', 'joinpart.c',
    c_args : ['-DG_LOG_USE_STRUCTURED', '-DG_LOG_DOMAIN="PurplePlugin-JoinPart"'],
    dependencies : [libpurple_dep],
    name_prefix : '',
    install : true, install_dir : PURPLE_PLUGINDIR)

psychic = library('psychic', 'psychic.c',
    c_args : ['-DG_LOG_USE_STRUCTURED', '-DG_LOG_DOMAIN="PurplePlugin-Psychic"'],
    dependencies : [libpurple_dep],
    name_prefix : '',
    install : true, install_dir : PURPLE_PLUGINDIR)

statenotify = library('statenotify', 'statenotify.c',
    c_args : ['-DG_LOG_USE_STRUCTURED', '-DG_LOG_DOMAIN="PurplePlugin-StateNotify"'],
    dependencies : [libpurple_dep],
    name_prefix : '',
    install : true, install_dir : PURPLE_PLUGINDIR)

purple_toast = library('purple-toast', 'purple-toast.c',
	c_args : ['-DG_LOG_USE_STRUCTURED', '-DG_LOG_DOMAIN="PurplePlugin-Toast"'],
	dependencies : [libpurple_dep],
	name_prefix: '',
	install : true, install_dir : PURPLE_PLUGINDIR)

devenv.append('PURPLE_PLUGIN_PATH', meson.current_build_dir())
