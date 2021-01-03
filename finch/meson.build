enable_consoleui = false
if get_option('consoleui')
	libgnt_dep = dependency('gnt3', version : '>= 3.0.0', required : false)
	if libgnt_dep.found()
		libgnt_docpath = libgnt_dep.get_pkgconfig_variable('prefix') / 'share/gtk-doc/html'

		if enable_introspection
			libgnt_gir = ['Gnt-3.0']
		endif
	else
		libgnt_proj = subproject('libgnt',
		    default_options : [
		        'doc=' + get_option('doc').to_string(),
		        'introspection=' + enable_introspection.to_string(),
		    ]
		)
		libgnt_dep = libgnt_proj.get_variable('libgnt_dep')

		libgnt_docpath = meson.build_root() / 'subprojects/libgnt/doc/'
		if enable_introspection
			libgnt_gir = libgnt_proj.get_variable('libgnt_gir')
		endif
	endif

	#######################################################################
	# Check for ncurses and other things used by it
	# FIXME: This should be temporary until libgnt wraps the functionality.
	#######################################################################
	ncurses_available = true
	ncurses_header = 'ncurses.h'
	# Some distros put the headers in ncursesw/, some don't. These are ordered to
	# pick the last available as most-specific version.
	ncursesw_header_paths = ['', 'ncursesw/']

	ncurses = dependency('ncursesw', required : false)
	if ncurses.found()
		foreach location : ncursesw_header_paths
			f = location / 'ncurses.h'
			if compiler.has_header_symbol(f, 'get_wch',
			    prefix : '#define _XOPEN_SOURCE_EXTENDED')
				ncurses_header = f
			endif
		endforeach
	else
		ncurses_available = false
		ncurses_inc = []
		ncurses_libs = compiler.find_library('ncursesw', required : false)
		if ncurses_libs.found()
			foreach location : ncursesw_header_paths
				f = location / 'ncurses.h'
				if compiler.has_header_symbol(f, 'get_wch',
				    prefix : '#define _XOPEN_SOURCE_EXTENDED')
					ncurses_available = true
					ncurses_header = f
				endif
			endforeach

			if ncurses_available
				ncurses = declare_dependency(
				    include_directories : ncurses_inc,
				    dependencies : ncurses_libs
				)
			endif
		endif
	endif

	if not ncurses_available
		# ncursesw was not found. Look for plain old ncurses
		ncurses = dependency('ncurses', required : false)
		if ncurses.found()
			ncurses_available = true
		else
			ncurses_libs = compiler.find_library('ncurses', required : false)
			ncurses_available = ncurses_libs.found()
			ncurses = declare_dependency(dependencies : ncurses_libs)
		endif
	endif

	if not ncurses_available and host_machine.system() == 'windows'
		# Try pdcurses too.
		ncurses_header = 'curses.h'
		ncurses_libs = compiler.find_library('pdcurses', required : false)
		ncurses_available = compiler.has_header(ncurses_header) and ncurses_libs.found()
		ncurses = declare_dependency(dependencies : ncurses_libs)
	endif

	if not ncurses_available
		error('ncurses could not be found!')
	endif

	if libgnt_dep.found() and ncurses_available
		ncurses_header = '-DNCURSES_HEADER="@0@"'.format(ncurses_header)
		enable_consoleui = true
	else
		error('''

Finch will not be built. You need to install libgnt (or its requirements) and its development headers.

''')
	endif
endif


libfinch_SOURCES = [
	'gntaccount.c',
	'gntblist.c',
	'gntconn.c',
	'gntconv.c',
	'gntdebug.c',
	'gntidle.c',
	'gntlog.c',
	'gntmedia.c',
	'gntmenuutil.c',
	'gntnotify.c',
	'gntplugin.c',
	'gntpounce.c',
	'gntprefs.c',
	'gntrequest.c',
	'gntroomlist.c',
	'gntstatus.c',
	'gntui.c',
	'gntxfer.c',
	package_revision,
	'libfinch.c'
]

libfinch_headers = [
	'gntaccount.h',
	'gntblist.h',
	'gntconn.h',
	'gntconv.h',
	'gntdebug.h',
	'gntidle.h',
	'gntlog.h',
	'gntmedia.h',
	'gntmenuutil.h',
	'gntnotify.h',
	'gntplugin.h',
	'gntpounce.h',
	'gntprefs.h',
	'gntrequest.h',
	'gntroomlist.h',
	'gntstatus.h',
	'gntui.h',
	'gntxfer.h',
	'libfinch.h'
]

finch_SOURCES = [
	'finch.c'
]

if IS_WIN32
	winmm = compiler.find_library('winmm')

	finch_winres = configure_file(
	    input : 'finch_winres.rc.in',
	    output : 'finch_winres.rc',
	    configuration : version_conf)
	finch_SOURCES += windows.compile_resources(finch_winres)
	libfinch_winres = configure_file(
	    input : 'libfinch_winres.rc.in',
	    output : 'libfinch_winres.rc',
	    configuration : version_conf)
	libfinch_SOURCES += windows.compile_resources(libfinch_winres)
else
	winmm = []
endif

if enable_consoleui
	FINCH_H_INCLUDES = []
	foreach header : libfinch_headers
		FINCH_H_INCLUDES += '#include <finch/@0@>'.format(header)
	endforeach
	finch_h_conf = configuration_data()
	finch_h_conf.set('FINCH_H_INCLUDES', '\n'.join(FINCH_H_INCLUDES))

	finch_h = configure_file(input : 'finch.h.in',
	                         output : 'finch.h',
	                         configuration : finch_h_conf,
	                         install : true,
				 install_dir : get_option('includedir') / 'finch-3')

	install_headers(libfinch_headers, subdir : 'finch-3')

	libfinch_inc = include_directories('.')
	libfinch = shared_library('finch3',
	    libfinch_SOURCES,
	    c_args : ['-DSTANDALONE', '-DGNTSEAL_ENABLE', '-DFINCH_COMPILATION', ncurses_header],
	    include_directories : [toplevel_inc],
	    version : PURPLE_LIB_VERSION,
	    dependencies : [libpurple_dep, libgnt_dep, ncurses, glib, winmm],
	    install : true)
	libfinch_dep = declare_dependency(
	    include_directories : [toplevel_inc, libfinch_inc],
	    link_with : libfinch,
	    dependencies : [libpurple_dep, libgnt_dep, glib])

	finch = executable('finch3',
	    finch_SOURCES,
	    c_args : ['-DSTANDALONE', '-DGNTSEAL_ENABLE'],
	    dependencies : [libpurple_dep, libgnt_dep, libfinch_dep],
	    install : true)

	pkgconfig.generate(
	    name : 'Finch',
	    description : 'Finch is an instant messenger application that uses libpurple for protocol support and ncurses (libgnt) for the UI.',
	    version : meson.project_version(),
	    filebase : 'finch-3',
	    subdirs : 'finch-3',
	    libraries : [libfinch],
	    requires : ['gnt', 'purple-3'],
	    variables : ['plugindir=${libdir}/finch-@0@'.format(purple_major_version)])

	if enable_introspection
		introspection_sources = libfinch_headers

		Finch_gir_includes = [
			libgnt_gir[0],
			gplugin_gir,
			# subproject needs to be before libpurple in the repo so that
			# meson will use our variables and not try to discover them from
			# libpurple.
			libpurple_gir[0],
		]

		gnome.generate_gir(libfinch,
		    sources : introspection_sources,
		    includes : Finch_gir_includes,
		    namespace : 'Finch',
		    symbol_prefix : 'finch',
		    identifier_prefix : 'Finch',
		    export_packages : 'finch',
		    nsversion : '@0@.@1@'.format(purple_major_version,
		                                 purple_minor_version),
		    include_directories : [
		        gplugin_include_directories,
		    ],
		    install : true,
		    extra_args : ['-DFINCH_COMPILATION', '--quiet'])
	endif

	subdir('plugins')
endif  # enable_consoleui
