executable(
    'light-server',
    'light-server.c',
     export_dynamic : true,
     dependencies : gupnp
)

executable(
    'light-client',
    'light-client.c',
    dependencies : gupnp
)

executable(
    'get-volume',
    'get-volume.c',
    dependencies: gupnp
)

foreach program : ['browsing', 'proxy', 'server', 'introspection', 'white-list', 'context-manager']
    executable(
        'test-' + program,
        sources : 'test-@0@.c'.format(program),
        dependencies : [gupnp, gio_unix]
    )
endforeach

