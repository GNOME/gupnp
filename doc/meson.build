entities = configuration_data()
entities.set('VERSION', meson.project_version())
version_xml = configure_file(input: 'version.xml.in',
               output: 'version.xml', configuration:
               entities)

docbook_man_page = configure_file(
  input: 'gupnp-binding-tool.xml.in',
  output: 'gupnp-binding-tool.xml',
  configuration: api_config,
  install: false
)

if get_option('gtk_doc')
    gnome.gtkdoc('gupnp',
             content_files : files(
                'client-tutorial.xml',
                'fdl-1.1.xml',
                'glossary.xml',
                'gupnp-docs.xml',
                'overview.xml',
                'server-tutorial.xml'
             ),
             main_xml : 'gupnp-docs.xml',
             src_dir : ['libgupnp'],
             dependencies : [libgupnp, version_xml, docbook_man_page],
             scan_args : ['--ignore-decorators', 'G_DEPRECATED|G_GNUC_DEPRECATED,G_DEPRECATED_FOR'],
             ignore_headers : [
                 'gena-protocol.h',
                 'xml-util.h',
                 'gvalue-util.h',
                 'http-headers.h',
                 'gupnp-context-private.h',
                 'gupnp-linux-context-manager.h',
                 'gupnp-network-manager.h',
                 'gupnp-unix-context-manager.h',
                 'gupnp-device-info-private.h',
                 'gupnp-error-private.h',
                 'gupnp-resource-factory-private.h',
                 'gupnp-service-introspection-private.h',
                 'gupnp-service-proxy-action-private.h',
                 'gupnp-types-private.h'
             ],
             install : true)
endif

xsltproc = find_program('xsltproc', required: false)
if xsltproc.found()
    stylesheet = 'http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl'

    xlstproc_flags = [
        '--nonet',
        '--xinclude',
        '--path', '@0@:@1@'.format(meson.current_source_dir(), meson.current_build_dir()),
        '--stringparam', 'man.output.quietly', '1',
        '--stringparam', 'funcsynopsis.style', 'ansi',
        '--stringparam', 'man.th.extra1.suppress', '1',
        '--stringparam', 'man.authors.section.enabled', '1',
        '--stringparam', 'man.copyright.section.enabled', '1',
    ]

    xsltproc_args = [
        xsltproc,
        xlstproc_flags,
        '-o', '@OUTPUT@',
        stylesheet,
        '@INPUT@'
    ]

    stylesheet_check = run_command(
        [
            xsltproc,
            xlstproc_flags,
            '--noout',
            stylesheet,
            docbook_man_page
        ]
    )
    if (stylesheet_check.returncode() == 0)
        message('Stylesheet ' + stylesheet + ' available')
        custom_target(
            'man 1 pages',
            input: docbook_man_page,
            output: 'gupnp-binding-tool-@0@.1'.format(GUPNP_API_VERSION),
            command: xsltproc_args,
            depend_files : version_xml,
            install: true,
            install_dir: join_paths(get_option('mandir'), 'man1')
        )
    else
        message('Stylesheet ' + stylesheet + ' not found, not building man page')
    endif
endif
