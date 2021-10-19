finch_doc_content_files = []

if get_option('doc')
	finch_toml = configure_file(
		input : 'finch.toml.in',
		output : 'finch.toml',
		configuration : version_conf,
		install : true,
		install_dir : docs_dir / 'finch',
	)

	finch_doc = custom_target('finch-doc',
		input : [ finch_toml, libfinch_gir ],
		output : 'finch',
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
		depend_files : [ finch_doc_content_files ],
		build_by_default : true,
		install : true,
		install_dir : docs_dir,
	)
endif

