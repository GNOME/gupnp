project('gupnp', 'c', version : '1.2.3', meson_version : '>= 0.48.0')
gnome = import('gnome')
pkg = import('pkgconfig')

cc = meson.get_compiler('c')

netlink_available = cc.has_header('linux/rtnetlink.h')
ifaddrs_available = cc.has_header('ifaddrs.h')

conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())
conf.set('HAVE_NETLINK', netlink_available)
conf.set('HAVE_IFADDRS_H', ifaddrs_available)
conf.set('HAVE_LINUX_WIRELESS_H', cc.has_header('linux/wireless.h'))

config_h = configure_file(output : 'config.h', configuration : conf)
add_global_arguments('-DHAVE_CONFIG_H=1', language : 'c')

guul = subproject('guul', default_options : ['default_library=static'])

dependencies = [
    dependency('glib-2.0', version : '>= 2.58'),
    dependency('gio-2.0', version : '>= 2.58'),
    dependency('gmodule-2.0', version : '>= 2.44'),
    dependency('gobject-2.0', version : '>= 2.44'),
    dependency('gssdp-1.2', version : '>= 1.2.3'),
    dependency('libsoup-2.4', version : '>= 2.48.0'),
    dependency('libxml-2.0')
]

subdir('libgupnp')
subdir('tests')
subdir('tools')
subdir('doc')

if get_option('vapi') and get_option('introspection')
    subdir('vala')
endif

if get_option('examples')
    subdir('examples')
endif
