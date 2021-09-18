project('gupnp', 'c', version : '1.4.0', meson_version : '>= 0.54.0')
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

glib_version = '2.66'
add_project_arguments(cc.get_supported_arguments('-Werror=deprecated-declarations'), language: 'c')
conf.set('GLIB_VERSION_MIN_REQUIRED', 'GLIB_VERSION_' + glib_version.underscorify())
conf.set('GLIB_VERSION_MAX_ALLOWED', 'GLIB_VERSION_' + glib_version.underscorify())

subdir('internal')
guul = subproject('guul', default_options : ['default_library=static'])

gssdp_dep = dependency('gssdp-1.2', version : '>= 1.3.0', default_options: ['sniffer=false'], fallback: 'gssdp-1.2')

gio_unix = dependency('gio-unix-2.0', version: '>= 2.44', required: host_machine.system() != 'windows')

dependencies = [
    dependency('glib-2.0', version : '>= ' + glib_version),
    dependency('gio-2.0', version : '>= ' + glib_version),
    dependency('gmodule-2.0', version : '>= ' + glib_version),
    dependency('gobject-2.0', version : '>= ' + glib_version),
    dependency('libsoup-2.4', version : '>= 2.48.0'),
    gssdp_dep,
    dependency('libxml-2.0'),
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
