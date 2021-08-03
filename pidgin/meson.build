libpidgin_SOURCES = [
	'pidginstock.c',
	'gtkaccount.c',
	'gtkblist.c',
	'gtkconn.c',
	'gtkconv.c',
	'gtkdialogs.c',
	'gtkdnd-hints.c',
	'gtkicon-theme.c',
	'gtkicon-theme-loader.c',
	'gtkidle.c',
	'gtkmedia.c',
	'gtknotify.c',
	'gtkpluginpref.c',
	'gtkprivacy.c',
	'gtkrequest.c',
	'gtkroomlist.c',
	'gtksavedstatuses.c',
	'gtksmiley-manager.c',
	'gtksmiley-theme.c',
	'gtkstatus-icon-theme.c',
	'gtkstatusbox.c',
	'gtkutils.c',
	'gtkwhiteboard.c',
	'gtkxfer.c',
	'libpidgin.c',
	'minidialog.c',
	'pidginabout.c',
	'pidginaccountactionsmenu.c',
	'pidginaccountchooser.c',
	'pidginaccountfilterconnected.c',
	'pidginaccountfilterprotocol.c',
	'pidginaccountsmenu.c',
	'pidginaccountstore.c',
	'pidginactiongroup.c',
	'pidginapplication.c',
	'pidginattachment.c',
	'pidginavatar.c',
	'pidgincellrendererexpander.c',
	'pidginclosebutton.c',
	'pidgincolor.c',
	'pidgincommands.c',
	'pidgincontactcompletion.c',
	'pidginconversationwindow.c',
	'pidgincontactlist.c',
	'pidgindebug.c',
	'pidgindialog.c',
	'pidgingdkpixbuf.c',
	'pidgininfopane.c',
	'pidgininvitedialog.c',
	'pidginlog.c',
	'pidginmenutray.c',
	'pidginmessage.c',
	'pidginmooddialog.c',
	'pidginplugininfo.c',
	'pidginpluginsdialog.c',
	'pidginpluginsmenu.c',
	'pidginpresence.c',
	'pidginpresenceicon.c',
	'pidginprotocolchooser.c',
	'pidginprotocolstore.c',
	'pidginscrollbook.c',
	'pidginstylecontext.c',
	'pidgintalkatu.c',
	'prefs/pidginprefs.c',
	'prefs/pidgincredentialproviderrow.c',
	'prefs/pidgincredentialspage.c',
]

libpidgin_headers = [
	'gtkaccount.h',
	'gtkblist.h',
	'gtkconn.h',
	'gtkconv.h',
	'gtkconvwin.h',
	'gtkdialogs.h',
	'gtkdnd-hints.h',
	'gtkicon-theme.h',
	'gtkicon-theme-loader.h',
	'gtkidle.h',
	'gtkmedia.h',
	'gtknotify.h',
	'gtkpluginpref.h',
	'gtkprivacy.h',
	'gtkrequest.h',
	'gtkroomlist.h',
	'gtksavedstatuses.h',
	'gtksmiley-manager.h',
	'gtksmiley-theme.h',
	'gtkstatus-icon-theme.h',
	'gtkstatusbox.h',
	'pidginstock.h',
	'gtkutils.h',
	'gtkwhiteboard.h',
	'gtkxfer.h',
	'minidialog.h',
	'pidginabout.h',
	'pidginaccountactionsmenu.h',
	'pidginaccountchooser.h',
	'pidginaccountfilterconnected.h',
	'pidginaccountfilterprotocol.h',
	'pidginaccountsmenu.h',
	'pidginaccountstore.h',
	'pidginactiongroup.h',
	'pidginapplication.h',
	'pidginattachment.h',
	'pidginavatar.h',
	'pidgincellrendererexpander.h',
	'pidginclosebutton.h',
	'pidgincolor.h',
	'pidgincontactcompletion.h',
	'pidginconversationwindow.h',
	'pidgincontactlist.h',
	'pidgincore.h',
	'pidgindialog.h',
	'pidgindebug.h',
	'pidgingdkpixbuf.h',
	'pidginicon.h',
	'pidgininfopane.h',
	'pidgininvitedialog.h',
	'pidginlog.h',
	'pidginmenutray.h',
	'pidginmessage.h',
	'pidginmooddialog.h',
	'pidginplugininfo.h',
	'pidginpluginsdialog.h',
	'pidginpluginsmenu.h',
	'pidginpresence.h',
	'pidginpresenceicon.h',
	'pidginprotocolchooser.h',
	'pidginprotocolstore.h',
	'pidginscrollbook.h',
	'pidginstylecontext.h',
	'pidgintalkatu.h',
]

libpidgin_prefs_headers = [
	'prefs/pidginprefs.h',
	'prefs/pidgincredentialproviderrow.h',
	'prefs/pidgincredentialspage.h',
]

libpidgin_enum_headers = [
	'gtkaccount.h',
	'gtkblist.h',
	'gtkconv.h',
	'gtkdnd-hints.h',
	'gtkutils.h',
	'pidginaccountstore.h',
	'pidginprotocolstore.h',
]

pidgin_SOURCES = [
	'pidgin.c'
]

pidgin_resource = gnome.compile_resources('pidginresources',
    'resources/pidgin.gresource.xml',
    source_dir : 'resources',
    c_name : 'pidgin')
libpidgin_SOURCES += pidgin_resource

pidgin_filebase = 'pidgin-3'
pidgin_include_base = pidgin_filebase / 'pidgin'

if IS_WIN32
	libpidgin_SOURCES += [
		'pidgin.c',
		'win32/gtkwin32dep.c',
		'win32/untar.c'
	]

	pidgin_exe_rc = configure_file(
	    input : 'win32/pidgin_exe_rc.rc.in',
	    output : 'pidgin_exe_rc.rc',
	    configuration : version_conf)
	pidgin_SOURCES = [
	    'win32/winpidgin.c',
	    windows.compile_resources(pidgin_exe_rc,
	        include_directories : include_directories('win32')
	    )
	]
endif

if ENABLE_GTK
	if IS_WIN32
		pidgin_dll_rc = configure_file(
		    input : 'win32/pidgin_dll_rc.rc.in',
		    output : 'pidgin_dll_rc.rc',
		    configuration : version_conf)
		libpidgin_SOURCES += windows.compile_resources(pidgin_dll_rc,
		    include_directories : include_directories('win32')
		)
	endif

	libpidgin_enums = gnome.mkenums_simple('pidginenums',
		sources: libpidgin_enum_headers,
		install_header: true,
		install_dir: get_option('includedir') / pidgin_include_base)
	libpidgin_enums_c = libpidgin_enums[0]
	libpidgin_enums_h = libpidgin_enums[1]

	PIDGIN_H_INCLUDES = []
	foreach header : libpidgin_headers + libpidgin_prefs_headers + ['pidginenums.h']
		PIDGIN_H_INCLUDES += '#include <pidgin/@0@>'.format(header)
	endforeach
	pidgin_h_conf = configuration_data()
	pidgin_h_conf.set('PIDGIN_H_INCLUDES', '\n'.join(PIDGIN_H_INCLUDES))
	pidgin_h = configure_file(input : 'pidgin.h.in',
	                          output : 'pidgin.h',
	                          configuration : pidgin_h_conf,
	                          install : true,
	                          install_dir : get_option('includedir') / pidgin_filebase)

	install_headers(libpidgin_headers, subdir : pidgin_include_base)
	install_headers(libpidgin_prefs_headers, subdir : pidgin_include_base / 'prefs')

	_libpidgin_dependencies = [
		glib,
		gplugin_gtk_dep,
		gstreamer_video,
		gtk,
		IOKIT,
		json,
		libhandy,
		math,
		nice,
		libsoup,
		talkatu_dep,
		libpurple_dep,
	]
	if x11.found()
		_libpidgin_dependencies += x11
	endif

	libpidgin_built_sources = [
		libpidgin_enums_c,
	]

	libpidgin_built_headers = [
		pidgin_h,
		libpidgin_enums_h,
	]

	libpidgin_inc = include_directories('.')
	libpidgin = shared_library('pidgin3',
	    libpidgin_SOURCES + libpidgin_built_sources + libpidgin_built_headers + ['pidginprivate.h'],
	    package_revision,
	    c_args : ['-DPIDGIN_COMPILATION', '-DG_LOG_DOMAIN="Pidgin"'],
	    include_directories : [toplevel_inc, include_directories('prefs')],
	    version : PURPLE_LIB_VERSION,
	    dependencies : _libpidgin_dependencies,
	    install : true)
	libpidgin_dep = declare_dependency(
	    include_directories : [toplevel_inc, libpidgin_inc],
	    link_with : libpidgin,
	    sources : libpidgin_built_headers,
	    dependencies : [gtk, glib, libhandy, math, talkatu_dep, gplugin_gtk_dep])

	pidgin = executable('pidgin3',
	    pidgin_SOURCES,
	    c_args : ['-DPIDGIN_COMPILATION', '-DG_LOG_DOMAIN="Pidgin"'],
	    include_directories : toplevel_inc,
	    dependencies : IS_WIN32 ? [] : [libpurple_dep, libpidgin_dep],
	    win_subsystem : 'windows',
	    install : true)

	if meson.version().version_compare('>=0.54.0')
		meson.override_dependency(pidgin_filebase, libpidgin_dep)
	endif

	pkgconfig.generate(
	    name : 'Pidgin',
	    description : 'Pidgin is a GTK3-based instant messenger application.',
	    version : meson.project_version(),
	    filebase : pidgin_filebase,
	    subdirs : pidgin_filebase,
	    libraries : [libpidgin],
	    requires : ['gtk+-3.0', 'libhandy-1', 'purple-3', 'talkatu', 'gplugin-gtk'],
	    variables : ['plugindir=${libdir}/pidgin-@0@'.format(purple_major_version)])

	PIDGIN_DESKTOP_FILE = 'im.pidgin.Pidgin3.desktop'
	if get_option('nls')
		i18n = import('i18n')
		desktop_file_in = configure_file(
		    input : 'data' / (PIDGIN_DESKTOP_FILE + '.in.in'),
		    output : PIDGIN_DESKTOP_FILE + '.in',
		    configuration : conf)
		desktop_file = i18n.merge_file(
		    input : desktop_file_in,
		    output : PIDGIN_DESKTOP_FILE,
		    po_dir : meson.project_source_root() / 'po',
		    type : 'desktop',
		    install : true,
		    install_dir : get_option('datadir') / 'applications')

		appdata = i18n.merge_file(
		    input : 'data/im.pidgin.Pidgin3.appdata.xml.in',
		    output : 'im.pidgin.Pidgin3.appdata.xml',
		    po_dir : meson.project_source_root() / 'po',
		    install : true,
		    install_dir : get_option('datadir') / 'metainfo')
	else
		configure_file(
		    input : 'data' / (PIDGIN_DESKTOP_FILE + '.in.in'),
		    output : PIDGIN_DESKTOP_FILE,
		    configuration : conf,
		    install : true,
		    install_dir : get_option('datadir') / 'applications')

		configure_file(
		    input : 'data/im.pidgin.Pidgin3.appdata.xml.in',
		    output : 'im.pidgin.Pidgin3.appdata.xml',
		    configuration : configuration_data(),
		    install : true,
		    install_dir : get_option('datadir') / 'metainfo')
	endif  # INSTALL_I18N

	if enable_introspection
		introspection_sources = libpidgin_headers + libpidgin_prefs_headers

		Pidgin_gir_includes = [
			'GObject-2.0', 'Gtk-3.0',
			talkatu_gir, gplugin_gir, gplugin_gtk_gir,
			# subproject needs to be before libpurple in the repo so that
			# meson will use our variables and not try to discover them from
			# libpurple.
			libpurple_gir[0],
		]

		gnome.generate_gir(libpidgin,
			sources : introspection_sources,
			includes : Pidgin_gir_includes,
			namespace : 'Pidgin',
			symbol_prefix : 'pidgin',
			identifier_prefix : 'Pidgin',
			export_packages : 'pidgin-@0@'.format(purple_major_version),
			nsversion : '@0@.@1@'.format(purple_major_version,
			                             purple_minor_version),
			include_directories : [
				gplugin_include_directories,
				gplugin_gtk_include_directories,
				talkatu_include_directories,
			],
			install : true,
			extra_args : ['-DPIDGIN_COMPILATION', '--quiet'])
	endif

	subdir('data')
	subdir('glade')
	subdir('pixmaps')
	subdir('plugins')
endif  # ENABLE_GTK
