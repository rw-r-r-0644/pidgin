BONJOUR_SOURCES = [
	'bonjour.c',
	'bonjour.h',
	'buddy.c',
	'buddy.h',
	'xmpp.c',
	'xmpp.h',
	'mdns_common.c',
	'mdns_common.h',
	'mdns_types.h',
	'parser.c',
	'parser.h',
	'bonjour_ft.c',
	'bonjour_ft.h'
]

if IS_WIN32
	BONJOUR_SOURCES += [
		'dns_sd_proxy.c', 'dns_sd_proxy.h', 'mdns_dns_sd.c', 'mdns_dns_sd.h',
		'mdns_win32.c'
	]
	bonjour_link_args = ['-lnetapi32']
else
	BONJOUR_SOURCES += ['mdns_avahi.c']
	bonjour_link_args = []
endif

if DYNAMIC_BONJOUR
	bonjour_prpl = shared_library('bonjour', BONJOUR_SOURCES,
	    link_args : bonjour_link_args,
	    dependencies : [libxml, avahi, libpurple_dep, glib, nice, ws2_32],
	    install : true, install_dir : PURPLE_PLUGINDIR)

	devenv.append('PURPLE_PLUGIN_PATH', meson.current_build_dir())
endif
