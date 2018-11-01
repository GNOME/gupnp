project('guul', 'c', version : '0.1')

cc = meson.get_compiler('c')
dependencies = [
    dependency('glib-2.0')
]
args = []

if host_machine.system() == 'windows'
    args += '-DGUUL_PLATFORM_WINDOWS'
    dependencies += cc.find_library('rpcrt4')
elif host_machine.system() == 'darwin'
    args += '-DGUUL_PLATFORM_OSX'
else
    if not cc.has_function('uuid_to_string')
        dependencies += dependency('uuid')
        args += '-DGUUL_PLATFORM_GENERIC'
    else
        args += '-DGUUL_PLATFORM_BSD'
    endif
endif

guul_lib = static_library(
    'guul',
    sources : 'guul.c',
    c_args : args,
    dependencies : dependencies
)

guul = declare_dependency (
    link_with : guul_lib,
    include_directories: include_directories('.')
)
