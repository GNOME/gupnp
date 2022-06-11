foreach program : ['context', 'bugs', 'context-filter']
    test(
        program,
        executable(
            'test-' + program,
            'test-@0@.c'.format (program),
            dependencies : gupnp,
            c_args : '-DDATA_PATH="@0@/data"'.format(meson.current_source_dir())
        ),
        is_parallel : false
    )
endforeach
