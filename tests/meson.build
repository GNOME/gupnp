foreach program : ['browsing', 'proxy', 'server', 'introspection', 'white-list']
    executable(
        'test-' + program,
        sources : 'test-@0@.c'.format(program),
        dependencies : [gupnp, gio_unix]
    )
endforeach

subdir('gtest')
