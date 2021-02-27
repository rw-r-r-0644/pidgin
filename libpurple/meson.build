purple_coresources = [
	'account.c',
	'accounts.c',
	'action.c',
	'blistnode.c',
	'buddy.c',
	'buddylist.c',
	'buddyicon.c',
	'chat.c',
	'circularbuffer.c',
	'cmds.c',
	'connection.c',
	'contact.c',
	'conversation.c',
	'conversationtypes.c',
	'conversations.c',
	'core.c',
	'countingnode.c',
	'debug.c',
	'eventloop.c',
	'glibcompat.c',
	'group.c',
	'idle.c',
	'image.c',
	'image-store.c',
	'keyring.c',
	'log.c',
	'media/backend-fs2.c',
	'media/backend-iface.c',
	'media/candidate.c',
	'media/codec.c',
	'media/enum-types.c',
	'media.c',
	'mediamanager.c',
	'memorypool.c',
	'nat-pmp.c',
	'network.c',
	'notify.c',
	'plugins.c',
	'pluginpref.c',
	'prefs.c',
	'proxy.c',
	'protocol.c',
	'protocols.c',
	'purple-gio.c',
	'purpleaccountoption.c',
	'purpleaccountpresence.c',
	'purpleaccountusersplit.c',
	'purpleattachment.c',
	'purpleattentiontype.c',
	'purplebuddypresence.c',
	'purplechatuser.c',
	'purplecredentialmanager.c',
	'purplecredentialprovider.c',
	'purpleimconversation.c',
	'purplekeyvaluepair.c',
	'purplemarkup.c',
	'purplemessage.c',
	'purplenoopcredentialprovider.c',
	'purpleoptions.c',
	'purplepresence.c',
	'purpleprotocolattention.c',
	'purpleprotocolchat.c',
	'purpleprotocolclient.c',
	'purpleprotocolfactory.c',
	'purpleprotocolim.c',
	'purpleprotocolmanager.c',
	'purpleprotocolmedia.c',
	'purpleprotocolprivacy.c',
	'purpleprotocolserver.c',
	'purpleuiinfo.c',
	'purplewhiteboard.c',
	'purplewhiteboarduiops.c',
	'queuedoutputstream.c',
	'request.c',
	'request-datasheet.c',
	'roomlist.c',
	'savedstatuses.c',
	'server.c',
	'signals.c',
	'smiley-custom.c',
	'smiley-list.c',
	'smiley-parser.c',
	'smiley-theme.c',
	'smiley.c',
	'status.c',
	'stun.c',
	'theme.c',
	'theme-loader.c',
	'theme-manager.c',
	'trie.c',
	'upnp.c',
	'util.c',
	'version.c',
	'xfer.c',
	'xmlnode.c'
]

purple_coreheaders = [
	'account.h',
	'accounts.h',
	'action.h',
	'blistnode.h',
	'buddy.h',
	'buddylist.h',
	'buddyicon.h',
	'chat.h',
	'circularbuffer.h',
	'cmds.h',
	'connection.h',
	'contact.h',
	'conversation.h',
	'conversationtypes.h',
	'conversations.h',
	'core.h',
	'countingnode.h',
	'debug.h',
	'eventloop.h',
	'group.h',
	'idle.h',
	'image.h',
	'image-store.h',
	'keyring.h',
	'log.h',
	'media.h',
	'mediamanager.h',
	'memorypool.h',
	'nat-pmp.h',
	'network.h',
	'notify.h',
	'plugins.h',
	'pluginpref.h',
	'prefs.h',
	'proxy.h',
	'protocol.h',
	'protocols.h',
	'purple-gio.h',
	'purpleaccountoption.h',
	'purpleaccountpresence.h',
	'purpleaccountusersplit.h',
	'purpleattentiontype.h',
	'purplebuddypresence.h',
	'purplechatuser.h',
	'purplecredentialmanager.h',
	'purplecredentialprovider.h',
	'purpleimconversation.h',
	'purpleattachment.h',
	'purplekeyvaluepair.h',
	'purplemarkup.h',
	'purplemessage.h',
	'purplenoopcredentialprovider.h',
	'purpleoptions.h',
	'purplepresence.h',
	'purpleprotocolattention.h',
	'purpleprotocolchat.h',
	'purpleprotocolclient.h',
	'purpleprotocolfactory.h',
	'purpleprotocolim.h',
	'purpleprotocolmedia.h',
	'purpleprotocolmanager.h',
	'purpleprotocolprivacy.h',
	'purpleprotocolserver.h',
	'purpleuiinfo.h',
	'purplewhiteboard.h',
	'purplewhiteboardops.h',
	'purplewhiteboarduiops.h',
	'queuedoutputstream.h',
	'request.h',
	'request-datasheet.h',
	'roomlist.h',
	'savedstatuses.h',
	'server.h',
	'signals.h',
	'smiley-custom.h',
	'smiley-list.h',
	'smiley-parser.h',
	'smiley-theme.h',
	'smiley.h',
	'status.h',
	'stun.h',
	'tests.h',
	'theme.h',
	'theme-loader.h',
	'theme-manager.h',
	'trie.h',
	'upnp.h',
	'util.h',
	'xfer.h',
	'xmlnode.h',
]

purple_filebase = 'purple-@0@'.format(purple_major_version)
purple_include_base = purple_filebase / 'libpurple'

if enable_vv
	purple_coreheaders += 'media-gst.h'
endif

if IS_WIN32
	purple_coresources += [
		'win32/libc_interface.c',
		'win32/win32dep.c'
	]

	purple_coreheaders += [
		'win32/win32dep.h',
	]

	libpurplerc = configure_file(
	    input : 'win32/libpurplerc.rc.in',
	    output : 'libpurplerc.rc',
	    configuration : version_conf)
	purple_coresources += windows.compile_resources(libpurplerc)
endif

purple_mediaheaders = [
	'media/backend-iface.h',
	'media/candidate.h',
	'media/codec.h',
	'media/enum-types.h'
]

purple_enumheaders = [
	'account.h',
	'buddyicon.h',
	'connection.h',
	'conversation.h',
	'debug.h',
	'eventloop.h',
	'notify.h',
	'plugins.h',
	'protocol.h',
	'protocols.h',
	'purplechatuser.h',
	'purpleimconversation.h',
	'purplemessage.h',
	'roomlist.h',
	'status.h',
	'xfer.h',
	'xmlnode.h'
]


enums = gnome.mkenums('enums',
    sources : purple_enumheaders,
    h_template : 'enums.h.in',
    c_template : 'enums.c.in',
    install_header : true,
    install_dir : get_option('includedir') / purple_include_base)
enums_c = enums[0]
enums_h = enums[1]

PURPLE_H_INCLUDES = []
foreach header : purple_coreheaders + purple_mediaheaders + ['version.h', 'enums.h']
	PURPLE_H_INCLUDES += '#include <libpurple/@0@>'.format(header)
endforeach
purple_h_conf = configuration_data()
purple_h_conf.set('PURPLE_H_INCLUDES', '\n'.join(PURPLE_H_INCLUDES))

purple_h = configure_file(input : 'purple.h.in',
                          output : 'purple.h',
                          configuration : purple_h_conf,
                          install : true,
                          install_dir : get_option('includedir') / purple_filebase)
version_h = configure_file(input : 'version.h.in',
                           output : 'version.h',
                           configuration : version_conf,
                           install : true,
                           install_dir : get_option('includedir') / purple_include_base)

purple_builtsources = [
	enums_c,
]

purple_builtheaders = [
	purple_h,
	version_h,
	enums_h,
]

libpurple_inc = include_directories('.')
libpurple = library('purple',
                    purple_coresources + purple_builtsources +
                    purple_builtheaders,
                    'purpleprivate.h',
                    c_args : ['-DPURPLE_COMPILATION', '-DG_LOG_DOMAIN="Purple"'],
                    include_directories : [toplevel_inc, libpurple_inc],
                    install : true,
                    version : PURPLE_LIB_VERSION,
                    dependencies : # static_link_libs
                        [dnsapi, ws2_32, glib, gio, gplugin_dep, libsoup,
                         libxml, farstream, gstreamer, gstreamer_video,
                         gstreamer_app, json, math])

libpurple_dep = declare_dependency(
    # Ensure purple headers built before any dependencies:
    sources : [purple_builtheaders],
    include_directories : [toplevel_inc, libpurple_inc],
    link_with : libpurple,
    dependencies : [gstreamer, gplugin_dep, glib, gio])

install_headers(purple_coreheaders,
                subdir : purple_include_base)

install_headers(purple_mediaheaders,
                subdir : purple_include_base / 'media')

pkgconfig.generate(
    name : 'libpurple',
    description : 'libpurple is a GLib-based instant messenger library.',
    version : meson.project_version(),
    filebase : purple_filebase,
# TODO: Only use purple_filebase once everything is ported to only use purple.h
    subdirs : [purple_filebase, purple_include_base],
    libraries : [libpurple],
    requires : ['glib-2.0', 'gplugin'],
    variables : ['plugindir=${libdir}/@0@'.format(purple_filebase)])

if enable_introspection
	introspection_sources = (purple_coreheaders + purple_builtheaders +
	                         purple_mediaheaders)

	Purple_gir_includes = ['GObject-2.0', 'Gio-2.0', 'Gst-1.0', gplugin_gir]

	libpurple_gir = gnome.generate_gir(libpurple,
	    sources : introspection_sources,
	    includes : Purple_gir_includes,
	    namespace : 'Purple',
	    symbol_prefix : 'purple',
	    identifier_prefix : 'Purple',
	    export_packages : purple_filebase,
	    nsversion : '@0@.@1@'.format(purple_major_version,
	                                 purple_minor_version),
	    include_directories : [
	        gplugin_include_directories,
	    ],
	    install : true,
	    extra_args : ['-DPURPLE_COMPILATION', '--quiet'])
endif

PURPLE3_URL_HANDLER_DESKTOP = 'purple3-url-handler.desktop'
if get_option('nls')
	i18n = import('i18n')
	desktop_file_in = configure_file(
	    input : 'data' / (PURPLE3_URL_HANDLER_DESKTOP + '.in.in'),
	    output : PURPLE3_URL_HANDLER_DESKTOP + '.in',
	    configuration : conf)
	i18n.merge_file(
	    input : desktop_file_in,
	    output : PURPLE3_URL_HANDLER_DESKTOP,
	    po_dir : meson.source_root() / 'po',
	    type : 'desktop',
	    install : true,
	    install_dir : get_option('datadir') / 'applications')
else
	configure_file(
	    input : 'data' / (PURPLE3_URL_HANDLER_DESKTOP + '.in.in'),
	    output : PURPLE3_URL_HANDLER_DESKTOP,
	    configuration : conf,
	    install : true,
	    install_dir : get_option('datadir') / 'applications')
endif

subdir('tests')
subdir('example')
subdir('plugins')
subdir('protocols')
