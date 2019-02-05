entities = configuration_data()
entities.set('VERSION', meson.project_version())
configure_file(input: 'version.xml.in',
               output: 'version.xml', configuration:
               entities)

gnome.gtkdoc('gupnp',
             main_xml : 'gupnp-docs.xml',
             src_dir : [join_paths(meson.source_root(), 'libgupnp'),
                        join_paths(meson.build_root(), 'libgupnp')],
             dependencies : libgupnp,
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
                 'gupnp-types-private.h'
             ],
             install : true)
