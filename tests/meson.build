foreach program : ['context', 'bugs', 'service', 'acl', 'service-proxy', 'context-filter', 'context-manager']
    test(
        program,
        executable(
            'test-' + program,
            'test-@0@.c'.format (program),
            dependencies : [gupnp, libxml_dep],
            c_args : '-DDATA_PATH="@0@/data"'.format(meson.current_source_dir()),
            include_directories : config_h_inc,
        ),
        is_parallel : false
    )
endforeach
