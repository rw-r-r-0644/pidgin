facebook_doc_content_files = []

if get_option('doc')
	facebook_toml = configure_file(
		input : 'facebook.toml.in',
		output : 'facebook.toml',
		configuration : version_conf,
		install : true,
		install_dir : docs_dir / 'facebook',
	)

	facebook_doc = custom_target('facebook-doc',
		input : [ facebook_toml, facebook_gir[0] ],
		output : 'facebook',
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
		depend_files : [ facebook_doc_content_files ],
		build_by_default : true,
		install : true,
		install_dir : docs_dir,
	)
endif

