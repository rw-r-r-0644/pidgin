pidgin3_xml = configure_file(
    input : 'pidgin3.xml.in',
    output : 'pidgin3.xml',
    configuration : version_conf,
    install : true,
    install_dir : get_option('datadir') / 'glade/catalogs')

devenv.append('GLADE_CATALOG_SEARCH_PATH', meson.current_build_dir())

