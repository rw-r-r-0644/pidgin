DOC_MODULE = 'libpurple'

# Header files or dirs to ignore when scanning. Use base file/dir names
ignore_hfiles = [
    'example',
    'plugins',
    'protocols',
    'tests',
    'tests.h',
    'win32',
    'backend-fs2.h',
    'glibcompat.h',
    'internal.h',
    'purple.h',
    'purpleenums.h',
    'purpleprivate.h',
]

# Extra options to supply to gtkdoc-scan.
scan_args = [
    '--deprecated-guards=PURPLE_DISABLE_DEPRECATED',
    '--rebuild-types',
    '--rebuild-sections',
    '--ignore-headers=' + ' '.join(ignore_hfiles),
]

# Extra options to supply to gtkdoc-mkdb.
mkdb_args = [
    '--ignore-files=' + ' '.join(ignore_hfiles),
]

# Extra options to supply to gtkdoc-fixxref
fixxref_args = [
    '--extra-dir=@0@'.format(gplugin_docpath),
]

libpurple_version_xml = configure_file(
    input : 'version.xml.in',
    output : 'version.xml',
    configuration : version_conf)

content_files = [
    'plugin_i18n.xml',
    'plugin_ids.xml',
    'signals_account.xml',
    'signals_blist.xml',
    'signals_certificate.xml',
    'signals_cmd.xml',
    'signals_connection.xml',
    'signals_conversation.xml',
    'signals_core.xml',
    'signals_jabber.xml',
    'signals_log.xml',
    'signals_notify.xml',
    'signals_plugin.xml',
    'signals_protocol.xml',
    'signals_savedstatus.xml',
    'signals_xfer.xml',
    'tut_c_plugins.xml',
    'tut_signals.xml',
    'ui_ops.xml',
]

libpurple_doc = gnome.gtkdoc(DOC_MODULE,
    main_xml : DOC_MODULE + '-docs.xml',
    src_dir : libpurple_inc,
    dependencies : libpurple_dep,
    install : true,
    scan_args : scan_args,
    mkdb_args : mkdb_args,
    fixxref_args: fixxref_args,
    gobject_typesfile : DOC_MODULE + '.types',
    content_files : content_files)
