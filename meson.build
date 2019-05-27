project('gupnp', 'c', version : '1.0.4')
gnome = import('gnome')
pkg = import('pkgconfig')

cc = meson.get_compiler('c')

conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())

netlink_test = '''#include <sys/socket.h>
#include <linux/rtnetlink.h>
'''
netlink_available = cc.compiles(netlink_test, name : 'linux/rtnetlink.h availability')
conf.set('HAVE_LINUX_RTNETLINK_H', netlink_available)

ifaddrs_test = '#include <ifaddrs.h>'
ifaddrs_available = cc.compiles(ifaddrs_test, name : 'ifaddrs.h availability')
conf.set('HAVE_IFADDRS_H', ifaddrs_available)

wireless_test ='''#include <sys/socket.h>
#include <linux/wireless.h>
'''
conf.set('HAVE_LINUX_WIRELESS_H',
         cc.compiles(wireless_test, name : 'linux/wireless.h availability'))

config_h = configure_file(output : 'config.h', configuration : conf)
add_global_arguments('-DHAVE_CONFIG_H=1', language : 'c')

guul = subproject('guul', default_options : ['default_library=static'])

dependencies = [
    dependency('glib-2.0', version : '>= 2.44'),
    dependency('gio-2.0', version : '>= 2.44'),
    dependency('gmodule-2.0', version : '>= 2.44'),
    dependency('gobject-2.0', version : '>= 2.44'),
    dependency('gssdp-1.0', version : '>= 1.0'),
    dependency('libsoup-2.4', version : '>= 2.48.0'),
    dependency('libxml-2.0'),
    guul.get_variable('guul')
]

subdir('libgupnp')
subdir('tests')
subdir('tools')

if get_option('vapi') and get_option('introspection')
    subdir('vala')
endif

if get_option('gtk_doc')
    subdir('doc')
endif

if get_option('examples')
    subdir('examples')
endif
