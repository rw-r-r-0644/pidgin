pidgin_doc_content_files = [
	'signals_gtkaccount.md',
	'signals_gtkblist.md',
	'signals_gtkconv.md',
]

if get_option('doc')
	pidgin_toml = configure_file(
		input : 'pidgin.toml.in',
		output : 'pidgin.toml',
		configuration : version_conf,
		install : true,
		install_dir : docs_dir / 'pidgin',
	)

	pidgin_doc = custom_target('pidgin-doc',
		input : [ pidgin_toml, pidgin_gir[0] ],
		output : 'pidgin',
		command : [
			gidocgen,
			'generate',
			'--quiet',
			'--fatal-warnings',
			'--config=@INPUT0@',
			'--output-dir=@OUTPUT@',
			'--no-namespace-dir',
			'--content-dir=@0@'.format(meson.current_source_dir()),
			'@INPUT1@'
		],
		depend_files : [ pidgin_doc_content_files ],
		build_by_default : true,
		install : true,
		install_dir : docs_dir,
	)
endif

