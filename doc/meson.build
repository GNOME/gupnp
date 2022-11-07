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
  gidocgen = find_program('gi-docgen', required: true)

  gupnp_toml = configure_file (
      input: 'gupnp.toml.in',
      output: 'gupnp.toml',
      configuration: entities
  )

  docs_dir = join_paths(get_option('prefix'), get_option('datadir'), 'doc')

  if gssdp_dep.type_name() == 'internal'
    gssdp_gir_dirs = subproject('gssdp-1.6').get_variable('gssdp_gir_dir')
  else
    gssdp_gir_dirs = ''
  endif

  custom_target(
      'gupnp-doc',
      input: [ gupnp_toml, gir[0] ],
      output: GUPNP_API_NAME,
      command : [
          gidocgen,
          'generate',
          '--quiet',
          '--add-include-path=@0@'.format(meson.current_build_dir() / '../libgupnp'),
          '--add-include-path=@0@'.format(gssdp_gir_dirs),
          '--config', gupnp_toml,
          '--output-dir=@OUTPUT@',
          '--no-namespace-dir',
          '--content-dir=@0@'.format(meson.current_source_dir()),
          '@INPUT1@',
      ],
      depend_files : [
          gupnp_toml,
          'client-tutorial.md',
          'server-tutorial.md',
          'choosing-a-context-manager.md',
          'glossary.md',
      ],
      build_by_default: true,
      install: true,
      install_dir : docs_dir,
  )
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
