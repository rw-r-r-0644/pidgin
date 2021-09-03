IRC_SOURCES = [
	'cmds.c',
	'dcc_send.c',
	'irc.c',
	'irc.h',
	'msgs.c',
	'parse.c'
]

if DYNAMIC_IRC
	irc_resources = gnome.compile_resources('ircresource',
		'resources/irc.gresource.xml',
		source_dir : 'resources',
		c_name : 'irc')
	IRC_SOURCES += irc_resources

	irc_prpl = shared_library('irc', IRC_SOURCES,
	    dependencies : [sasl, libpurple_dep, glib, gio, ws2_32],
	    install : true, install_dir : PURPLE_PLUGINDIR)
endif
